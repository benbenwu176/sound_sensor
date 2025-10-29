
#include "nrf24_hal.h"

#define NRF_CMD_R_REGISTER      0x00
#define NRF_CMD_W_REGISTER      0x20
#define NRF_CMD_R_RX_PAYLOAD    0x61
#define NRF_CMD_W_TX_PAYLOAD    0xA0
#define NRF_CMD_FLUSH_TX        0xE1
#define NRF_CMD_FLUSH_RX        0xE2
#define NRF_CMD_REUSE_TX_PL     0xE3
#define NRF_CMD_R_RX_PL_WID     0x60
#define NRF_CMD_ACTIVATE        0x50
#define NRF_CMD_NOP             0xFF

#define NRF_REG_CONFIG          0x00
#define NRF_REG_EN_AA           0x01
#define NRF_REG_EN_RXADDR       0x02
#define NRF_REG_SETUP_AW        0x03
#define NRF_REG_SETUP_RETR      0x04
#define NRF_REG_RF_CH           0x05
#define NRF_REG_RF_SETUP        0x06
#define NRF_REG_STATUS          0x07
#define NRF_REG_RX_ADDR_P0      0x0A
#define NRF_REG_RX_ADDR_P1      0x0B
#define NRF_REG_RX_ADDR_P2      0x0C
#define NRF_REG_RX_ADDR_P3      0x0D
#define NRF_REG_RX_ADDR_P4      0x0E
#define NRF_REG_RX_ADDR_P5      0x0F
#define NRF_REG_TX_ADDR         0x10
#define NRF_REG_RX_PW_P0        0x11
#define NRF_REG_RX_PW_P1        0x12
#define NRF_REG_RX_PW_P2        0x13
#define NRF_REG_RX_PW_P3        0x14
#define NRF_REG_RX_PW_P4        0x15
#define NRF_REG_RX_PW_P5        0x16
#define NRF_REG_FIFO_STATUS     0x17
#define NRF_REG_DYNPD           0x1C
#define NRF_REG_FEATURE         0x1D

/* STATUS bits */
#define NRF_STATUS_RX_DR        (1u<<6)
#define NRF_STATUS_TX_DS        (1u<<5)
#define NRF_STATUS_MAX_RT       (1u<<4)

/* CONFIG bits */
#define CFG_PRIM_RX             (1u<<0)
#define CFG_PWR_UP              (1u<<1)
#define CFG_CRCO                (1u<<2)
#define CFG_EN_CRC              (1u<<3)
#define CFG_MASK_MAX_RT         (1u<<4)
#define CFG_MASK_TX_DS          (1u<<5)
#define CFG_MASK_RX_DR          (1u<<6)

/* FEATURE bits */
#define FEATURE_EN_DPL          (1u<<2)
#define FEATURE_EN_ACK_PAY      (1u<<1)
#define FEATURE_EN_DYN_ACK      (1u<<0)

static inline void CE_H(nrf24_t *n){ HAL_GPIO_WritePin(n->ce_port, n->ce_pin, GPIO_PIN_SET); }
static inline void CE_L(nrf24_t *n){ HAL_GPIO_WritePin(n->ce_port, n->ce_pin, GPIO_PIN_RESET); }
static inline void CSN_H(nrf24_t *n){ HAL_GPIO_WritePin(n->csn_port, n->csn_pin, GPIO_PIN_SET); }
static inline void CSN_L(nrf24_t *n){ HAL_GPIO_WritePin(n->csn_port, n->csn_pin, GPIO_PIN_RESET); }

static uint8_t xfer(nrf24_t *n, uint8_t v){
    uint8_t rx=0;
    HAL_SPI_TransmitReceive(n->hspi, &v, &rx, 1, 5);
    return rx;
}

static void write_reg(nrf24_t *n, uint8_t reg, uint8_t val){
    CSN_L(n); xfer(n, NRF_CMD_W_REGISTER | (reg & 0x1F)); xfer(n, val); CSN_H(n);
}
static uint8_t read_reg(nrf24_t *n, uint8_t reg){
    CSN_L(n); xfer(n, NRF_CMD_R_REGISTER | (reg & 0x1F)); uint8_t v = xfer(n, 0); CSN_H(n); return v;
}
static void write_buf(nrf24_t *n, uint8_t reg, const uint8_t *buf, uint8_t len){
    CSN_L(n); xfer(n, NRF_CMD_W_REGISTER | (reg & 0x1F)); for(uint8_t i=0;i<len;i++) xfer(n, buf[i]); CSN_H(n);
}

uint8_t nrf24_status(nrf24_t *n){ CSN_L(n); uint8_t s=xfer(n, NRF_CMD_NOP); CSN_H(n); return s; }

void nrf24_clear_irqs(nrf24_t *n){ write_reg(n, NRF_REG_STATUS, NRF_STATUS_RX_DR|NRF_STATUS_TX_DS|NRF_STATUS_MAX_RT); }
void nrf24_flush_rx(nrf24_t *n){ CSN_L(n); xfer(n, NRF_CMD_FLUSH_RX); CSN_H(n); }
void nrf24_flush_tx(nrf24_t *n){ CSN_L(n); xfer(n, NRF_CMD_FLUSH_TX); CSN_H(n); }

void nrf24_init(nrf24_t *n){
    /* CSN high, CE low */
    CE_L(n); CSN_H(n);
    HAL_Delay(5);
    /* Disable all special features */
    write_reg(n, NRF_REG_FEATURE, 0);
    write_reg(n, NRF_REG_DYNPD, 0);
    /* Clear interrupts; FIFOs */
    nrf24_clear_irqs(n); nrf24_flush_rx(n); nrf24_flush_tx(n);
}

static void set_addr_width(nrf24_t *n, uint8_t bytes){
    if(bytes < 3) bytes = 3; else if(bytes>5) bytes = 5;
    write_reg(n, NRF_REG_SETUP_AW, bytes-2);
}

static void set_rf(nrf24_t *n, uint8_t rf_ch, nrf_datarate_t dr, nrf_txpower_t pwr){
    write_reg(n, NRF_REG_RF_CH, rf_ch & 0x7F);
    /* RF_SETUP: bit5=RF_DR_LOW, bit3=RF_DR_HIGH */
    uint8_t rf_setup = 0;
    if(dr == NRF_DR_2M) rf_setup |= (1<<3);
    if(dr == NRF_DR_250K) rf_setup |= (1<<5);
    rf_setup |= (pwr & 0x03) << 1; /* RF_PWR bits 2:1 */
    write_reg(n, NRF_REG_RF_SETUP, rf_setup);
}

static void set_auto_retr(nrf24_t *n, uint8_t arc, uint16_t ard_us){
    if(arc>15) arc=15;
    /* ARD step = 250us, min 250us, max 4000us */
    uint8_t ard_steps = (ard_us + 249) / 250;
    if(ard_steps<1) ard_steps=1; if(ard_steps>15) ard_steps=15;
    write_reg(n, NRF_REG_SETUP_RETR, (ard_steps<<4) | (arc & 0x0F));
}

void nrf24_prx_config(nrf24_t *n, uint8_t rf_ch, nrf_datarate_t dr, uint8_t addr_width_bytes,
                      const uint8_t addr_p0[5], const uint8_t *pipe_lsbs, uint8_t n_pipes,
                      bool auto_ack, uint8_t arc, uint8_t ard_us){
    nrf24_init(n);
    set_addr_width(n, addr_width_bytes);
    set_rf(n, rf_ch, dr, NRF_PWR_0);

    /* Enable selected RX pipes */
    uint8_t en_rx = 0;
    for(uint8_t i=0;i<n_pipes && i<6;i++) en_rx |= (1u<<i);
    write_reg(n, NRF_REG_EN_RXADDR, en_rx);
    /* Auto-ACK mask */
    uint8_t en_aa = auto_ack ? en_rx : 0;
    write_reg(n, NRF_REG_EN_AA, en_aa);
    set_auto_retr(n, auto_ack ? arc : 0, auto_ack ? ard_us : 250);

    /* Program addresses:
     * P0 full address = addr_p0
     * P1 full address shares top bytes with P0 if not provided here (we provide only LSBs for P2..P5)
     */
    write_buf(n, NRF_REG_RX_ADDR_P0, addr_p0, 5);
    uint8_t base4[4] = { addr_p0[0], addr_p0[1], addr_p0[2], addr_p0[3] };
    if(n_pipes > 1){
        uint8_t a1[5] = { base4[0],base4[1],base4[2],base4[3], pipe_lsbs[1] };
        write_buf(n, NRF_REG_RX_ADDR_P1, a1, 5);
    }
    if(n_pipes > 2) write_reg(n, NRF_REG_RX_ADDR_P2, pipe_lsbs[2]);
    if(n_pipes > 3) write_reg(n, NRF_REG_RX_ADDR_P3, pipe_lsbs[3]);
    if(n_pipes > 4) write_reg(n, NRF_REG_RX_ADDR_P4, pipe_lsbs[4]);
    if(n_pipes > 5) write_reg(n, NRF_REG_RX_ADDR_P5, pipe_lsbs[5]);

    /* Set static payload length for used pipes */
    for(uint8_t i=0;i<n_pipes && i<6;i++){
        uint8_t reg = NRF_REG_RX_PW_P0 + i;
        write_reg(n, reg, n->fixed_pld_len);
    }
    /* Disable DPL & features for determinism */
    write_reg(n, NRF_REG_FEATURE, 0);
    write_reg(n, NRF_REG_DYNPD, 0);

    /* PRX: PWR_UP, PRIM_RX, EN_CRC, CRCO(2B). Unmask IRQs. */
    write_reg(n, NRF_REG_CONFIG, CFG_PWR_UP | CFG_PRIM_RX | CFG_EN_CRC | CFG_CRCO);
    HAL_Delay(3);
    CE_H(n);
}

void nrf24_ptx_config(nrf24_t *n, uint8_t rf_ch, nrf_datarate_t dr, uint8_t addr_width_bytes,
                      const uint8_t addr_tx[5], bool auto_ack, uint8_t arc, uint16_t ard_us){
    nrf24_init(n);
    set_addr_width(n, addr_width_bytes);
    set_rf(n, rf_ch, dr, NRF_PWR_0);
    /* TX/RX address both = addr_tx for auto-ack */
    write_buf(n, NRF_REG_TX_ADDR, addr_tx, 5);
    write_buf(n, NRF_REG_RX_ADDR_P0, addr_tx, 5);
    write_reg(n, NRF_REG_EN_RXADDR, 0x01); /* only pipe0 needed for auto-ack */
    write_reg(n, NRF_REG_EN_AA, auto_ack ? 0x01 : 0x00);
    set_auto_retr(n, auto_ack ? arc : 0, auto_ack ? ard_us : 250);

    /* Static payload length for TX ack matching (not strictly required) */
    write_reg(n, NRF_REG_RX_PW_P0, n->fixed_pld_len);
    write_reg(n, NRF_REG_FEATURE, 0);
    write_reg(n, NRF_REG_DYNPD, 0);

    /* PTX: PWR_UP, EN_CRC, CRCO(2B) */
    write_reg(n, NRF_REG_CONFIG, CFG_PWR_UP | CFG_EN_CRC | CFG_CRCO);
    HAL_Delay(3);
}

bool nrf24_ptx_send(nrf24_t *n, const uint8_t *pld, uint8_t len, bool *acked_out){
    if(len != n->fixed_pld_len) return false;
    /* Write payload */
    CSN_L(n); xfer(n, NRF_CMD_W_TX_PAYLOAD); for(uint8_t i=0;i<len;i++) xfer(n, pld[i]); CSN_H(n);
    /* Pulse CE >=10us */
    CE_H(n); for(volatile int i=0;i<800;i++) __NOP(); CE_L(n); // ~20us @72MHz
    /* Wait for TX_DS or MAX_RT; Polling is fine for Blue Pill */
    uint32_t t0 = HAL_GetTick();
    while(1){
        uint8_t s = nrf24_status(n);
        if(s & NRF_STATUS_TX_DS){ nrf24_clear_irqs(n); if(acked_out) *acked_out = true; return true; }
        if(s & NRF_STATUS_MAX_RT){ nrf24_clear_irqs(n); nrf24_flush_tx(n); if(acked_out) *acked_out = false; return false; }
        if(HAL_GetTick() - t0 > 5){ /* 5ms guard */ nrf24_clear_irqs(n); return false; }
    }
}

bool nrf24_prx_read(nrf24_t *n, uint8_t *pipe_no, uint8_t *buf, uint8_t len){
    /* Check status */
    uint8_t s = nrf24_status(n);
    uint8_t pno = (s >> 1) & 0x07;
    if(!(s & NRF_STATUS_RX_DR) || pno > 5) return false;

    /* Fixed-length read */
    CSN_L(n); xfer(n, NRF_CMD_R_RX_PAYLOAD); for(uint8_t i=0;i<len;i++) buf[i]=xfer(n,0); CSN_H(n);
    nrf24_clear_irqs(n);
    if(pipe_no) *pipe_no = pno;
    return true;
}
