
/* nrf24_hal.h - Minimal nRF24L01+ driver for STM32 HAL (multi-instance)
 * Public domain / Unlicense. Inspired by elmot/nrf24l01-lib (hardware-independent).
 * Only the features needed for this project are implemented.
 */
#ifndef NRF24_HAL_H
#define NRF24_HAL_H

#include "stm32f1xx_hal.h"
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    SPI_HandleTypeDef *hspi;
    GPIO_TypeDef *ce_port;  uint16_t ce_pin;
    GPIO_TypeDef *csn_port; uint16_t csn_pin;
    GPIO_TypeDef *irq_port; uint16_t irq_pin; // optional; may be 0
    uint8_t fixed_pld_len; // 1..32
} nrf24_t;

/* Radio config enums */
typedef enum { NRF_DR_250K = 2, NRF_DR_1M = 0, NRF_DR_2M = 1 } nrf_datarate_t;
typedef enum { NRF_PWR_NEG18=0, NRF_PWR_NEG12=1, NRF_PWR_NEG6=2, NRF_PWR_0=3 } nrf_txpower_t;

void nrf24_init(nrf24_t *n);
void nrf24_prx_config(nrf24_t *n, uint8_t rf_ch, nrf_datarate_t dr, uint8_t addr_width_bytes,
                      const uint8_t addr_p0[5], const uint8_t *pipe_lsbs, uint8_t n_pipes,
                      bool auto_ack, uint8_t arc, uint8_t ard_us);
void nrf24_ptx_config(nrf24_t *n, uint8_t rf_ch, nrf_datarate_t dr, uint8_t addr_width_bytes,
                      const uint8_t addr_tx[5], bool auto_ack, uint8_t arc, uint16_t ard_us);
bool nrf24_ptx_send(nrf24_t *n, const uint8_t *pld, uint8_t len, bool *acked_out);
bool nrf24_prx_read(nrf24_t *n, uint8_t *pipe_no, uint8_t *buf, uint8_t len);
void nrf24_clear_irqs(nrf24_t *n);
void nrf24_flush_rx(nrf24_t *n);
void nrf24_flush_tx(nrf24_t *n);

/* Low-level helpers (exposed for ISR) */
uint8_t nrf24_status(nrf24_t *n);

#endif /* NRF24_HAL_H */
