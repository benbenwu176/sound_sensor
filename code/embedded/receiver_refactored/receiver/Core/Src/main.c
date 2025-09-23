/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "usb_device.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "nrf24_hal.h"
#include "usbd_cdc_if.h"
#include "string.h"
#include <stdint.h>
#include <stdbool.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#define NRF_CMD_R_REGISTER 			0x00
#define NRF_CMD_W_REGISTER			0x20
#define NRF_CMD_R_RX_PL_WID			0x60
#define NRF_CMD_R_RX_PAYLOAD		0X61
#define NRF_CMD_W_TX_PAYLOAD		0xA0
#define NRF_CMD_FLUSH_RX				0xE2
#define NRF_CMD_FLUSH_TX				0xE1
#define NRF_CMD_REUSE_TX_PL			0xE3
#define NRF_CMD_ACTIVATE				0x50
#define NRF_CMD_NOP							0xFF

#define NRF_REG_CONFIG            0x00
#define NRF_REG_EN_AA             0x01
#define NRF_REG_EN_RXADDR         0x02
#define NRF_REG_SETUP_AW          0x03
#define NRF_REG_SETUP_RETR        0x04
#define NRF_REG_RF_CH             0x05
#define NRF_REG_RF_SETUP          0x06
#define NRF_REG_STATUS            0x07
#define NRF_REG_RX_ADDR_P0        0x0A
#define NRF_REG_RX_ADDR_P1        0x0B
#define NRF_REG_RX_ADDR_P2        0x0C
#define NRF_REG_RX_ADDR_P3        0x0D
#define NRF_REG_RX_ADDR_P4        0x0E
#define NRF_REG_RX_ADDR_P5        0x0F
#define NRF_REG_RX_PW_P0          0x11
#define NRF_REG_DYNPD             0x1C
#define NRF_REG_FEATURE           0x1D
#define NRF_REG_FIFO_STATUS       0x17

// STATUS bits
#define NRF_STATUS_RX_DR          (1U<<6)
#define NRF_STATUS_TX_DS          (1U<<5)
#define NRF_STATUS_MAX_RT         (1U<<4)
#define NRF_STATUS_RX_P_NO_MASK   (7U<<1)

// GPIO Pins
#define NRF1_CE_GPIO_Port GPIOA
#define NRF1_CE_Pin       GPIO_PIN_4
#define NRF1_CSN_GPIO_Port GPIOA
#define NRF1_CSN_Pin       GPIO_PIN_3

#define NRF2_CE_GPIO_Port GPIOB
#define NRF2_CE_Pin       GPIO_PIN_11
#define NRF2_CSN_GPIO_Port GPIOB
#define NRF2_CSN_Pin       GPIO_PIN_12

// RF base address
static const uint8_t PREFIX[4] = {0xE7,0xE7,0xE7,0xE7};


#define NRF1_IRQ_GPIO_Port GPIOB
#define NRF1_IRQ_Pin       GPIO_PIN_1
#define NRF2_IRQ_GPIO_Port GPIOB
#define NRF2_IRQ_Pin       GPIO_PIN_10
// Radio A (SPI1) on CH=70 listens to A0..A3
static const uint8_t A_LSB[4] = {0xA0,0xA1,0xA2,0xA3};
#define RF_CH_A 70

// Radio B (SPI2) on CH=85 listens to B0..B3
static const uint8_t B_LSB[4] = {0xB0,0xB1,0xB2,0xB3};
#define RF_CH_B 85

// USB packet is always 3 bytes: {addrLSB, sensorId(0..11), state(0/1)}
static inline void usb_send3(uint8_t b0, uint8_t b1, uint8_t b2)
{
  uint8_t pkt[3] = { b0, b1, b2 };
  while (CDC_Transmit_FS(pkt, 3) == USBD_BUSY) {;}
}

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
SPI_HandleTypeDef hspi1;
SPI_HandleTypeDef hspi2;

/* USER CODE BEGIN PV */
extern USBD_HandleTypeDef hUsbDeviceFS;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_SPI1_Init(void);
static void MX_SPI2_Init(void);
/* USER CODE BEGIN PFP */
static void usb_wait_configured(void);

// nRF types & functions
typedef struct {
  SPI_HandleTypeDef *spi;
  GPIO_TypeDef *ce_port, *csn_port;
  uint16_t ce_pin, csn_pin;
  uint8_t pipe_lsb[6];  // map pipe index -> LSB for USB byte0
} NrfRx;

static void nrf_prx_init(NrfRx *nrf, SPI_HandleTypeDef *spi,
                         GPIO_TypeDef *ce_port, uint16_t ce_pin,
                         GPIO_TypeDef *csn_port, uint16_t csn_pin,
                         uint8_t rf_ch, const uint8_t prefix[4],
                         const uint8_t lsbs[], uint8_t n_lsbs);

static bool nrf_poll_drain(NrfRx *nrf); // returns true if any packet handled

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* --- Radio driver contexts --- */
static nrf24_t radioA = { .hspi=&hspi1, .ce_port=NRF1_CE_GPIO_Port, .ce_pin=NRF1_CE_Pin, .csn_port=NRF1_CSN_GPIO_Port, .csn_pin=NRF1_CSN_Pin, .irq_port=NRF1_IRQ_GPIO_Port, .irq_pin=NRF1_IRQ_Pin, .fixed_pld_len=2 };
static nrf24_t radioB = { .hspi=&hspi2, .ce_port=NRF2_CE_GPIO_Port, .ce_pin=NRF2_CE_Pin, .csn_port=NRF2_CSN_GPIO_Port, .csn_pin=NRF2_CSN_Pin, .irq_port=NRF2_IRQ_GPIO_Port, .irq_pin=NRF2_IRQ_Pin, .fixed_pld_len=2 };

static volatile uint8_t irqA = 0, irqB = 0;

static void radios_init(void) {
    /* Addresses: E7E7E7E7 A0..A3 on CH70; E7E7E7E7 B0..B3 on CH85 */
    const uint8_t base_addr[5] = {0xE7,0xE7,0xE7,0xE7, 0xA0};
    uint8_t lsbsA[4] = {0xA0,0xA1,0xA2,0xA3};
    uint8_t lsbsB[4] = {0xB0,0xB1,0xB2,0xB3};

    nrf24_prx_config(&radioA, 70, NRF_DR_2M, 5, base_addr, lsbsA, 4, true/*AA*/, 2/*ARC*/, 250/*us*/);
    nrf24_prx_config(&radioB, 85, NRF_DR_2M, 5, base_addr, lsbsB, 4, true/*AA*/, 2/*ARC*/, 250/*us*/);
}

/* Drain one radio and emit USB triplets {addrLSB, id, state} for every packet */
static bool radio_drain(nrf24_t *r, const uint8_t *pipe_lsbs){
    bool any=false;
    for(;;){
        uint8_t pipe, pkt[2];
        if(!nrf24_prx_read(r, &pipe, pkt, 2)) break;
        uint8_t addr_lsb = (pipe < 6) ? pipe_lsbs[pipe] : 0xFF;
        uint8_t id = pkt[0];
        uint8_t st = pkt[1];
        usb_send3(addr_lsb, id, st);
        any = true;
    }
    return any;
}


static void usb_wait_configured(void) {
	while (hUsbDeviceFS.dev_state != USBD_STATE_CONFIGURED) {
		// Rapid LED Flashing - USB connecting state
		HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
		HAL_Delay(100);
	}
}

static void send_bytes(const uint8_t *buf, uint16_t len) {
	while (CDC_Transmit_FS((uint8_t *) buf, len) == USBD_BUSY);
}

static void test_send(void) {
	uint8_t bytes[] = {0x10, 60, 127};
	CDC_Transmit_FS(bytes, 3);
}

/* ----- nRF functions ----- */
static inline void CSN_L(NrfRx *n){ HAL_GPIO_WritePin(n->csn_port, n->csn_pin, GPIO_PIN_RESET); }
static inline void CSN_H(NrfRx *n){ HAL_GPIO_WritePin(n->csn_port, n->csn_pin, GPIO_PIN_SET); }
static inline void CE_L (NrfRx *n){ HAL_GPIO_WritePin(n->ce_port,  n->ce_pin,  GPIO_PIN_RESET); }
static inline void CE_H (NrfRx *n){ HAL_GPIO_WritePin(n->ce_port,  n->ce_pin,  GPIO_PIN_SET);  }

static uint8_t spi_xfer(NrfRx *n, uint8_t b) {
  uint8_t o=0;
  HAL_SPI_TransmitReceive(n->spi, &b, &o, 1, 50);
  return o;
}

static void reg_write(NrfRx *n, uint8_t reg, uint8_t val) {
  CSN_L(n);
  spi_xfer(n, NRF_CMD_W_REGISTER | (reg & 0x1F));
  spi_xfer(n, val);
  CSN_H(n);
}

static void reg_write_buf(NrfRx *n, uint8_t reg, const uint8_t *buf, uint8_t len) {
  CSN_L(n);
  spi_xfer(n, NRF_CMD_W_REGISTER | (reg & 0x1F));
  for(uint8_t i=0;i<len;i++) spi_xfer(n, buf[i]);
  CSN_H(n);
}

static uint8_t reg_status(NrfRx *n) {
  CSN_L(n);
  uint8_t s = spi_xfer(n, NRF_CMD_NOP);
  CSN_H(n);
  return s;
}

static void flush_rx(NrfRx *n) {
  CSN_L(n); spi_xfer(n, NRF_CMD_FLUSH_RX); CSN_H(n);
}

static void clear_irqs(NrfRx *n, uint8_t mask) {
  reg_write(n, NRF_REG_STATUS, mask); // write-1-to-clear
}

static void dpl_enable(NrfRx *n, uint8_t pipes_mask) {
  // Some clones require ACTIVATE 0x73 before FEATURE access
  CSN_L(n); spi_xfer(n, NRF_CMD_ACTIVATE); spi_xfer(n, 0x73); CSN_H(n);
  reg_write(n, NRF_REG_FEATURE, 0x04);     // EN_DPL
  reg_write(n, NRF_REG_DYNPD,   pipes_mask); // enable DPL on selected pipes
}

static void nrf_prx_init(NrfRx *nrf, SPI_HandleTypeDef *spi,
                         GPIO_TypeDef *ce_port, uint16_t ce_pin,
                         GPIO_TypeDef *csn_port, uint16_t csn_pin,
                         uint8_t rf_ch, const uint8_t prefix[4],
                         const uint8_t lsbs[], uint8_t n_lsbs)
{
  nrf->spi = spi;
  nrf->ce_port = ce_port;  nrf->ce_pin = ce_pin;
  nrf->csn_port= csn_port; nrf->csn_pin= csn_pin;
  for (int i=0;i<6;i++) nrf->pipe_lsb[i]=0xFF;
  for (int i=0;i<n_lsbs && i<4;i++) nrf->pipe_lsb[i] = lsbs[i];

  CE_L(nrf); CSN_H(nrf);

  // 5-byte addresses, 2 Mbps @ 0 dBm
  reg_write(nrf, NRF_REG_SETUP_AW, 0x03);
  reg_write(nrf, NRF_REG_RF_SETUP, 0x0E);
  reg_write(nrf, NRF_REG_RF_CH,    rf_ch);

  // No Auto-ACK (low latency); enable pipes 0..(n_lsbs-1)
  uint8_t pipes_mask = (1U << n_lsbs) - 1U; // e.g., 4 -> 0b1111
  reg_write(nrf, NRF_REG_EN_AA,     0x00);
  reg_write(nrf, NRF_REG_EN_RXADDR, pipes_mask);

  // Address programming:
  // P0 has its own full address; P1 full; P2..P5 only LSB (share P1's top bytes)
  uint8_t a0[5] = { prefix[0],prefix[1],prefix[2],prefix[3], lsbs[0] };
  reg_write_buf(nrf, NRF_REG_RX_ADDR_P0, a0, 5);

  if (n_lsbs > 1) {
    uint8_t a1[5] = { prefix[0],prefix[1],prefix[2],prefix[3], lsbs[1] };
    reg_write_buf(nrf, NRF_REG_RX_ADDR_P1, a1, 5);
  }
  if (n_lsbs > 2) reg_write(nrf, NRF_REG_RX_ADDR_P2, lsbs[2]);
  if (n_lsbs > 3) reg_write(nrf, NRF_REG_RX_ADDR_P3, lsbs[3]);

  // Use Dynamic Payload Length so we can accept 2 or 3 bytes transparently
  dpl_enable(nrf, pipes_mask);

  // Power up, PRX, 2-byte CRC
  reg_write(nrf, NRF_REG_CONFIG, 0x0F); // PWR_UP=1, PRIM_RX=1, EN_CRC=1, CRCO=1

  // Clean state
  clear_irqs(nrf, NRF_STATUS_RX_DR | NRF_STATUS_TX_DS | NRF_STATUS_MAX_RT);
  flush_rx(nrf);

  // Enter receive
  HAL_Delay(3);
  CE_H(nrf); // PRX listens while CE high
}

// returns true if any packet processed
static bool nrf_poll_drain(NrfRx *nrf)
{
  bool any=false;
  for (;;) {
    uint8_t s = reg_status(nrf);
    uint8_t pno = (s >> 1) & 0x07;
    if (!(s & NRF_STATUS_RX_DR) || pno > 5) break;

    // Find payload width (DPL)
    uint8_t pw;
    CSN_L(nrf); spi_xfer(nrf, NRF_CMD_R_RX_PL_WID); pw = spi_xfer(nrf, 0); CSN_H(nrf);
    if (pw == 0xFF || pw > 32) { flush_rx(nrf); clear_irqs(nrf, NRF_STATUS_RX_DR); continue; }

    uint8_t buf[32];
    CSN_L(nrf); spi_xfer(nrf, NRF_CMD_R_RX_PAYLOAD); for (uint8_t i=0;i<pw;i++) buf[i]=spi_xfer(nrf,0); CSN_H(nrf);
    clear_irqs(nrf, NRF_STATUS_RX_DR);

    uint8_t addr_lsb = (pno < 6) ? nrf->pipe_lsb[pno] : 0xFF;

    // Expected TX payloads: 2 bytes {id,state}  OR  3 bytes {dev,id,state}
    uint8_t id=0, st=0;
    if (pw >= 2) { id = buf[(pw==2)?0:1]; st = buf[(pw==2)?1:2]; }

    // USB: {addrLSB, id, state}
    usb_send3(addr_lsb, id, st);
//    test_send();
    any = true;
  }
  return any;
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USB_DEVICE_Init();
  MX_SPI1_Init();
  MX_SPI2_Init();
  /* USER CODE BEGIN 2 */

  usb_wait_configured();
  // Turn LED off
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);

  // Radios
  NrfRx nrfA, nrfB;
  radios_init(); /* replaced legacy init */
// old init removed: nrf_prx_init(
                        NRF1_CSN_GPIO_Port, NRF1_CSN_Pin,
                        RF_CH_A, PREFIX, A_LSB, 4);

  // nrf_prx_init(&nrfB, ... ) removed
                        NRF2_CSN_GPIO_Port, NRF2_CSN_Pin,
                        RF_CH_B, PREFIX, B_LSB, 4);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1) {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  	bool got = false;
  	if(irqA){irqA=0; got |= radio_drain(&radioA, (uint8_t[]){0xA0,0xA1,0xA2,0xA3,0,0});}
  	if(irqB){irqB=0; got |= radio_drain(&radioB, (uint8_t[]){0xB0,0xB1,0xB2,0xB3,0,0});}
  	if (got) {
			HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);
			HAL_Delay(1000);
			HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
		}
  }

  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USB;
  PeriphClkInit.UsbClockSelection = RCC_USBCLKSOURCE_PLL_DIV1_5;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

}

/**
  * @brief SPI2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI2_Init(void)
{

  /* USER CODE BEGIN SPI2_Init 0 */

  /* USER CODE END SPI2_Init 0 */

  /* USER CODE BEGIN SPI2_Init 1 */

  /* USER CODE END SPI2_Init 1 */
  /* SPI2 parameter configuration*/
  hspi2.Instance = SPI2;
  hspi2.Init.Mode = SPI_MODE_MASTER;
  hspi2.Init.Direction = SPI_DIRECTION_2LINES;
  hspi2.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi2.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi2.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi2.Init.NSS = SPI_NSS_SOFT;
  hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_4;
  hspi2.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi2.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi2.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi2.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI2_Init 2 */

  /* USER CODE END SPI2_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3|GPIO_PIN_4, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_11|GPIO_PIN_12, GPIO_PIN_RESET);

  /*Configure GPIO pin : PC13 */
  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : PA0 */
  GPIO_InitStruct.Pin = GPIO_PIN_0;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : PA3 PA4 */
  GPIO_InitStruct.Pin = GPIO_PIN_3|GPIO_PIN_4;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : PB11 PB12 */
  GPIO_InitStruct.Pin = GPIO_PIN_11|GPIO_PIN_12;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */
  /* NRF CE pins */
  HAL_GPIO_WritePin(NRF1_CE_GPIO_Port, NRF1_CE_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(NRF2_CE_GPIO_Port, NRF2_CE_Pin, GPIO_PIN_RESET);
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  GPIO_InitStruct.Pin = NRF1_CE_Pin; GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP; GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH; HAL_GPIO_Init(NRF1_CE_GPIO_Port, &GPIO_InitStruct);
  GPIO_InitStruct.Pin = NRF2_CE_Pin; HAL_GPIO_Init(NRF2_CE_GPIO_Port, &GPIO_InitStruct);
  /* IRQ pins */
  GPIO_InitStruct.Pin = NRF1_IRQ_Pin; GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING; GPIO_InitStruct.Pull = GPIO_PULLUP; HAL_GPIO_Init(NRF1_IRQ_GPIO_Port, &GPIO_InitStruct);
  GPIO_InitStruct.Pin = NRF2_IRQ_Pin; HAL_GPIO_Init(NRF2_IRQ_GPIO_Port, &GPIO_InitStruct);
  HAL_NVIC_SetPriority(EXTI1_IRQn, 1, 0); HAL_NVIC_EnableIRQ(EXTI1_IRQn);
  HAL_NVIC_SetPriority(EXTI15_10_IRQn, 1, 0); HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);


  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
