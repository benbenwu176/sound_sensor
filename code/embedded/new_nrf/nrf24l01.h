/*
------------------------------------------------------------------------------
~ File   : nrf24l01.h (multi-instance refactor)
~ Notes  : Refactored to support multiple radios, each with its own SPI bus
------------------------------------------------------------------------------
*/

#ifndef __NRF24L01_H_
#define __NRF24L01_H_

/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ Include ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
#include <stdint.h>
#include "nrf24l01_conf.h"

#ifdef __cplusplus
extern "C" {
#endif

/* --------------------------------------------------------------------------------------------------------------
 * Instance handle (one per radio). Each instance can use a different SPI bus and CE/CS pins.
 * -------------------------------------------------------------------------------------------------------------- */
#if defined(USE_HAL_DRIVER)
#include "stm32f1xx_hal.h"
typedef struct
{
    SPI_HandleTypeDef *hspi;        /* SPI peripheral for this radio */

    GPIO_TypeDef *CS_GPIO_PORT;     /* CS (CSN) pin port */
    uint16_t      CS_GPIO_PIN;      /* CS (CSN) pin */

    GPIO_TypeDef *CE_GPIO_PORT;     /* CE pin port */
    uint16_t      CE_GPIO_PIN;      /* CE pin */

    GPIO_TypeDef *IRQ_GPIO_PORT;    /* Optional IRQ pin port */
    uint16_t      IRQ_GPIO_PIN;     /* Optional IRQ pin */
} NRF24L01_HandleTypeDef;
#else
/* If you're not using STM32 HAL, adapt this struct to your platform's SPI/GPIO types. */
typedef struct
{
    void *hspi;                     /* Platform-specific SPI handle */
    void *CS_GPIO_PORT;
    uint16_t CS_GPIO_PIN;
    void *CE_GPIO_PORT;
    uint16_t CE_GPIO_PIN;
    void *IRQ_GPIO_PORT;
    uint16_t IRQ_GPIO_PIN;
} NRF24L01_HandleTypeDef;
#endif

/* --------------------------------------------------------------------------------------------------------------
 * Public API (same names as the original library, but now take a handle as the first parameter)
 * -------------------------------------------------------------------------------------------------------------- */

/* Initialize */
void NRF24L01_RxInit(NRF24L01_HandleTypeDef *h, uint8_t Channel, uint8_t DataRate, uint16_t Timeout);
void NRF24L01_TxInit(NRF24L01_HandleTypeDef *h, uint8_t Channel, uint8_t DataRate, uint16_t Timeout);

/* Control */
void NRF24L01_Reset(NRF24L01_HandleTypeDef *h, uint16_t Timeout);

/* Primary Mode */
void NRF24L01_PRxMode(NRF24L01_HandleTypeDef *h, uint16_t Timeout);
void NRF24L01_PTxMode(NRF24L01_HandleTypeDef *h, uint16_t Timeout);

/* Transmit/Receive */
void NRF24L01_RxReceive(NRF24L01_HandleTypeDef *h, uint8_t *pRxPayload, uint8_t *RxPayloadNumber, uint16_t Timeout);
void NRF24L01_TxTransmit(NRF24L01_HandleTypeDef *h, uint8_t* pTxPayload, uint16_t Timeout);

/* Rx/Tx Status Control */
void NRF24L01_ClearRxDR(NRF24L01_HandleTypeDef *h, uint16_t Timeout);
void NRF24L01_ClearTxDS(NRF24L01_HandleTypeDef *h, uint16_t Timeout);
void NRF24L01_ClearMaxRT(NRF24L01_HandleTypeDef *h, uint16_t Timeout);

/* FIFO Control */
uint8_t NRF24L01_ReadRxFIFO(NRF24L01_HandleTypeDef *h, uint8_t *pRxPayload, uint16_t Timeout);
uint8_t NRF24L01_WriteTxFIFO(NRF24L01_HandleTypeDef *h, uint8_t *pTxPayload, uint16_t Timeout);

void NRF24L01_FlushRxFIFO(NRF24L01_HandleTypeDef *h, uint16_t Timeout);
void NRF24L01_FlushTxFIFO(NRF24L01_HandleTypeDef *h, uint16_t Timeout);

uint8_t NRF24L01_GetFIFOStatus(NRF24L01_HandleTypeDef *h, uint16_t Timeout);

/* Power */
void NRF24L01_PowerUp(NRF24L01_HandleTypeDef *h, uint16_t Timeout);
void NRF24L01_PowerDown(NRF24L01_HandleTypeDef *h, uint16_t Timeout);

/* RF Address */
typedef enum {
    NRF24L01_RX_ADDRESS_P0 = 0x01,
    NRF24L01_RX_ADDRESS_P1 = 0x02,
    NRF24L01_RX_ADDRESS_P2 = 0x04,
    NRF24L01_RX_ADDRESS_P3 = 0x08,
    NRF24L01_RX_ADDRESS_P4 = 0x10,
    NRF24L01_RX_ADDRESS_P5 = 0x20,
    NRF24L01_RX_ADDRESS_ALL = 0x3F
} NRF24L01_RxAddTypeDef;

void NRF24L01_EnableRxAddress(NRF24L01_HandleTypeDef *h, NRF24L01_RxAddTypeDef RxAddress, uint16_t Timeout);
void NRF24L01_DisableRxAddress(NRF24L01_HandleTypeDef *h, NRF24L01_RxAddTypeDef RxAddress, uint16_t Timeout);

void NRF24L01_SetAddressWidths(NRF24L01_HandleTypeDef *h, uint8_t Width, uint16_t Timeout);
void NRF24L01_SetRxAddress(NRF24L01_HandleTypeDef *h, NRF24L01_RxAddTypeDef RxAddress, uint8_t *Address, uint16_t Timeout);
void NRF24L01_SetTxAddress(NRF24L01_HandleTypeDef *h, uint8_t *Address, uint16_t Timeout);

/* RF Control */
typedef enum {
    NRF24L01_PWR_18DBM = 0,
    NRF24L01_PWR_12DBM,
    NRF24L01_PWR_6DBM,
    NRF24L01_PWR_0DBM
} NRF24L01_PWRTypeDef;

typedef enum {
    NRF24L01_DATA_RATE_1MBPS = 0,
    NRF24L01_DATA_RATE_2MBPS,
    NRF24L01_DATA_RATE_250KBPS
} NRF24L01_DataRateTypeDef;

typedef enum {
    NRF24L01_DISABLE = 0,
    NRF24L01_ENABLE  = 1
} NRF24L01_StateTypeDef;

typedef enum {
    NRF24L01_AUTO_ACK_NONE = 0x00,
    NRF24L01_AUTO_ACK_P0   = 0x01,
    NRF24L01_AUTO_ACK_P1   = 0x02,
    NRF24L01_AUTO_ACK_P2   = 0x04,
    NRF24L01_AUTO_ACK_P3   = 0x08,
    NRF24L01_AUTO_ACK_P4   = 0x10,
    NRF24L01_AUTO_ACK_P5   = 0x20,
    NRF24L01_AUTO_ACK_ALL  = 0x3F
} NRF24L01_AutoACKTypeDef;

void NRF24L01_SetRxPayloadWidths(NRF24L01_HandleTypeDef *h, uint8_t Width, uint16_t Timeout);
void NRF24L01_SetCRCLength(NRF24L01_HandleTypeDef *h, uint8_t Length, uint16_t Timeout);
void NRF24L01_SetAutoACK(NRF24L01_HandleTypeDef *h, NRF24L01_AutoACKTypeDef AACK, NRF24L01_StateTypeDef State, uint16_t Timeout);
void NRF24L01_AutoRetransmitCount(NRF24L01_HandleTypeDef *h, uint8_t Count, uint16_t Timeout);
void NRF24L01_AutoRetransmitDelay(NRF24L01_HandleTypeDef *h, uint16_t DelayTimeUS, uint16_t Timeout);
void NRF24L01_SetRFChannel(NRF24L01_HandleTypeDef *h, uint8_t Channel, uint16_t Timeout);
void NRF24L01_SetRFTxOutputPower(NRF24L01_HandleTypeDef *h, NRF24L01_PWRTypeDef dBm, uint16_t Timeout);
void NRF24L01_SetRFAirDataRate(NRF24L01_HandleTypeDef *h, NRF24L01_DataRateTypeDef bps, uint16_t Timeout);

/* IRQ */
void NRF24L01_TxIRQHandle(NRF24L01_HandleTypeDef *h, uint16_t Timeout);

/* Status */
uint8_t NRF24L01_GetStatus(NRF24L01_HandleTypeDef *h, uint16_t Timeout);

/* -------------------------- Device constants from the original library -------------------------- */
#define NRF24L01_ADDRESS_WIDTH           5
#define NRF24L01_PAYLOAD_LENGTH          32

/* Register addresses */
#define NRF24L01_REG_CONFIG              0x00
#define NRF24L01_REG_EN_AA               0x01
#define NRF24L01_REG_EN_RXADDR           0x02
#define NRF24L01_REG_SETUP_AW            0x03
#define NRF24L01_REG_SETUP_RETR          0x04
#define NRF24L01_REG_RF_CH               0x05
#define NRF24L01_REG_RF_SETUP            0x06
#define NRF24L01_REG_STATUS              0x07
#define NRF24L01_REG_RX_ADDR_P0          0x0A
#define NRF24L01_REG_RX_ADDR_P1          0x0B
#define NRF24L01_REG_RX_ADDR_P2          0x0C
#define NRF24L01_REG_RX_ADDR_P3          0x0D
#define NRF24L01_REG_RX_ADDR_P4          0x0E
#define NRF24L01_REG_RX_ADDR_P5          0x0F
#define NRF24L01_REG_TX_ADDR             0x10
#define NRF24L01_REG_RX_PW_P0            0x11
#define NRF24L01_REG_RX_PW_P1            0x12
#define NRF24L01_REG_RX_PW_P2            0x13
#define NRF24L01_REG_RX_PW_P3            0x14
#define NRF24L01_REG_RX_PW_P4            0x15
#define NRF24L01_REG_RX_PW_P5            0x16
#define NRF24L01_REG_FIFO_STATUS         0x17
#define NRF24L01_REG_DYNPD               0x1C
#define NRF24L01_REG_FEATURE             0x1D

/* Commands */
#define NRF24L01_CMD_R_REGISTER          0x00
#define NRF24L01_CMD_W_REGISTER          0x20
#define NRF24L01_CMD_R_RX_PAYLOAD        0x61
#define NRF24L01_CMD_W_TX_PAYLOAD        0xA0
#define NRF24L01_CMD_FLUSH_TX            0xE1
#define NRF24L01_CMD_FLUSH_RX            0xE2
#define NRF24L01_CMD_NOP                 0xFF

/* Bit positions & masks (keep consistent with original) */
#define NRF24L01_EN_CRC                  3
#define NRF24L01_CRCO                    2
#define NRF24L01_PWR_UP                  1
#define NRF24L01_PRIM_RX                 0
#define NRF24L01_RX_DR                   6
#define NRF24L01_TX_DS                   5
#define NRF24L01_MAX_RT                  4

#define NRF24L01_RF_PWR                  1
#define NRF24L01_RF_DR_BIT_MASK_RST      0xD7
#define NRF24L01_RF_PWR_BIT_MASK_RST     0xF9
#define NRF24L01_ARC_MASK_RST            0xF0
#define NRF24L01_ARD_BIT_MASK_RST        0x0F
#define NRF24L01_CRCO_MASK_RST           0xFB
#define NRF24L01_TX_DS_MASK              0x20

/* Default register values used by Reset() (match original defaults) */
#define NRF24L01_REG_DEF_CONFIG          0x08
#define NRF24L01_REG_DEF_EN_AA           0x3F
#define NRF24L01_REG_DEF_EN_RXADDR       0x03
#define NRF24L01_REG_DEF_SETUP_AW        0x03
#define NRF24L01_REG_DEF_SETUP_RETR      0x03
#define NRF24L01_REG_DEF_RF_CH           0x02
#define NRF24L01_REG_DEF_RF_SETUP        0x0E
#define NRF24L01_REG_DEF_STATUS          0x70
#define NRF24L01_REG_DEF_RX_ADDR_P0      0xE7
#define NRF24L01_REG_DEF_RX_ADDR_P1      0xC2
#define NRF24L01_REG_DEF_RX_ADDR_P2      0xC3
#define NRF24L01_REG_DEF_RX_ADDR_P3      0xC4
#define NRF24L01_REG_DEF_RX_ADDR_P4      0xC5
#define NRF24L01_REG_DEF_RX_ADDR_P5      0xC6
#define NRF24L01_REG_DEF_RX_PW_P0_P5     0x00
#define NRF24L01_REG_DEF_FIFO_STATUS     0x11
#define NRF24L01_REG_DEF_DYNPD           0x00
#define NRF24L01_REG_DEF_FEATURE         0x00

/* Simple helpers (mirroring original style) */
#define __NRF24L01_SET_BIT(REG, BIT)     ((REG) |=  (1U << (BIT)))
#define __NRF24L01_RESET_BIT(REG, BIT)   ((REG) &= ~(1U << (BIT)))

/* GPIO pin state aliases (for compatibility with original macros) */
#define NRF24L01_GPIO_PIN_SET            1
#define NRF24L01_GPIO_PIN_RESET          0

#ifdef __cplusplus
}
#endif

#endif /* __NRF24L01_H_ */
