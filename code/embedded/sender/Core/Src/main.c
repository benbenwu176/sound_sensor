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

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdbool.h>
#include <stdint.h>

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* ----- MPR121 definitions ----- */
#define MPR121_ADDR_7B							0x5A
#define MPR121_ADDR 								(MPR121_ADDR_7B << 1)

#define MPR121_REG_TOUCH_STATUS_L 	0x00
#define MPR121_REG_ECR 							0x5E
#define MPR121_REG_DEBOUNCE					0x5B
#define MPR121_REG_ELE0_T						0x41 // ELE0_R=0x42, ELE1_T=0x43, ...

#define MPR121_TOUCH_THR_DEFAULT 		0x06 // TODO: fine-tune
#define MPR121_RELEASE_THR_DEFAULT 	0x03 // TODO: fine-tune
#define MPR121_DEBOUNCE							0x00 // TODO: fine-tune
#define MPR121_RUN_MODE							0x8C // TODO: fine-tune

/* ----- nRF24 definitions ----- */
#define NRF_CMD_R_REGISTER 					0x00
#define NRF_CMD_W_REGISTER 					0x20
#define NRF_CMD_W_TX_PAYLOAD				0xA0
#define NRF_CMD_FLUSH_TX						0xE1
#define NRF_CMD_NOP									0xFF

#define NRF_REG_CONFIG							0x00
#define NRF_REG_EN_AA								0x01
#define NRF_REG_EN_RXADDR						0x02
#define NRF_REG_SETUP_AW						0x03
#define NRF_REG_SETUP_RETR					0x04
#define NRF_REG_RF_CH								0x05
#define NRF_REG_RF_SETUP						0x06
#define NRF_REG_STATUS							0x07
#define NRF_REG_RX_ADDR_P0					0x0A
#define NRF_REG_TX_ADDR							0x10
#define NRF_REG_DYNPD								0x1C
#define NRF_REG_FEATURE							0x1D

// Status bits
#define NRF_STATUS_RX_DR						(1U << 6)
#define NRF_STATUS_TX_DS						(1U << 5)
#define NRF_STATUS_MAX_RT						(1U << 4)

#define NRF_CE_GPIO_Port 						GPIOA
#define NRF_CE_Pin									GPIO_PIN_4
#define NRF_CSN_GPIO_Port 					GPIOA
#define NRF_CSN_Pin 								GPIO_PIN_3

// Packet length
#define TOUCH_PKT_LEN								2

// Sensor ID mask
volatile uint8_t g_mpr_irq_pending = 0;
volatile uint16_t g_touched_mask;
static uint16_t s_prev_mask = 0;

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

#ifndef DEVICE_ID
#define DEVICE_ID 0
#endif

// Accommodate for 9 devices total
#if (DEVICE_ID < 0) || (DEVICE_ID > 8)
#error "DEVICE_ID must be in range [0, 8]"
#endif

// Channel split: 0-3 on CH 70, 4... on CH 85
#define RF_CH0 				70
#define RF_CH1 				85
#define RF_CH_VAL 		((DEVICE_ID) < 4 ? (RF_CH0) : (RF_CH1))

// Set pipe index 0-4
#define PIPE_INDEX 		((DEVICE_ID) < 4 ? (DEVICE_ID) : (DEVICE_ID) - 4)

// Set LSB of pipe address
#define ADDR_BASE 		((DEVICE_ID) < 4 ? (0xA0) : (0xB0))
#define ADDR_LAST 		(ADDR_BASE + PIPE_INDEX)

// Set device 5-byte address
#define ADDR_B0 0xE7
#define ADDR_B1 0xE7
#define ADDR_B2 0xE7
#define ADDR_B3 0xE7
#define ADDR_B4 ADDR_LAST

static const uint8_t NRF_ADDR[5] = {ADDR_B0, ADDR_B1, ADDR_B2, ADDR_B3, ADDR_B4};

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;

SPI_HandleTypeDef hspi1;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_SPI1_Init(void);
/* USER CODE BEGIN PFP */

/* ----- MPR121 prototypes ----- */
static HAL_StatusTypeDef MPR121_Write8(uint8_t reg, uint8_t val);
static HAL_StatusTypeDef MPR121_ReadN(uint8_t reg, uint8_t *buf, uint16_t n);
static uint16_t MPR121_ReadTouchedMask(void);
static bool MPR121_Init(void);

/* ----- nRF24 prototypes ----- */
static void NRF_Init_TX(void);
static bool NRF_Send(const uint8_t *payload, uint8_t len);
static void NRF_ClearIRQ(void);
static void SendTouchChanges(uint16_t new_mask);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* ----- MPR121 functions ----- */

// Write a value to a register on the MPR121 board
static HAL_StatusTypeDef MPR121_Write8(uint8_t reg, uint8_t val) {
	return HAL_I2C_Mem_Write(&hi2c1, MPR121_ADDR, reg, I2C_MEMADD_SIZE_8BIT, &val, 1, 50);
}

// Read a register on MPR121 board
static HAL_StatusTypeDef MPR121_ReadN(uint8_t reg, uint8_t *buf, uint16_t n) {
	return HAL_I2C_Mem_Read(&hi2c1, MPR121_ADDR, reg, I2C_MEMADD_SIZE_8BIT, buf, n, 50);
}

// Read 0x00-0x01: also clears MPR121 IRQ line
static uint16_t MPR121_ReadTouchedMask(void) {
	uint8_t s[2] = {0};
	if (MPR121_ReadN(MPR121_REG_TOUCH_STATUS_L, s, 2) != HAL_OK) {return 0;}
	return (uint16_t) s[0] | ((uint16_t) s[1] << 8);
}

// Configure thresholds and start sensing
static bool MPR121_Init(void) {
	// Stop mode while configuring
	if (MPR121_Write8(MPR121_REG_ECR, 0x00) != HAL_OK) {return false;}

	// Touch configurations for ELE0 through ELE11
	for (uint8_t ele = 0; ele < 12; ele++) {
		uint8_t treg = MPR121_REG_ELE0_T + ele * 2;

		// Set activation/release threshold
		if (MPR121_Write8(treg, MPR121_TOUCH_THR_DEFAULT) != HAL_OK) {return false;}
		if (MPR121_Write8(treg + 1, MPR121_RELEASE_THR_DEFAULT) != HAL_OK) {return false;}
	}
	// Set debounce, run mode
	if (MPR121_Write8(MPR121_REG_DEBOUNCE, MPR121_DEBOUNCE) != HAL_OK) {return false;}
	if (MPR121_Write8(MPR121_REG_ECR, MPR121_RUN_MODE) != HAL_OK) {return false;}

	return true;
}


/* ----- nRF24 functions ----- */
// Helpers
static inline void CSN_L(void) { HAL_GPIO_WritePin(NRF_CSN_GPIO_Port, NRF_CSN_Pin, GPIO_PIN_RESET);}
static inline void CSN_H(void) { HAL_GPIO_WritePin(NRF_CSN_GPIO_Port, NRF_CSN_Pin, GPIO_PIN_SET);}
static inline void CE_L(void) {HAL_GPIO_WritePin(NRF_CE_GPIO_Port,  NRF_CE_Pin,  GPIO_PIN_RESET);}
static inline void CE_H(void) {HAL_GPIO_WritePin(NRF_CE_GPIO_Port,  NRF_CE_Pin,  GPIO_PIN_SET);}

static uint8_t spi_txrx(uint8_t b) {
	uint8_t o = 0;
	HAL_SPI_TransmitReceive(&hspi1, &b, &o, 1, 50);
	return o;
}

static void nrf_write_reg(uint8_t reg, uint8_t val) {
	CSN_L();
	spi_txrx(NRF_CMD_W_REGISTER | (reg & 0x1F));
	spi_txrx(val);
	CSN_H();
}

static void nrf_write_buf(uint8_t reg, const uint8_t *buf, uint8_t len) {
	CSN_L();
	spi_txrx(NRF_CMD_W_REGISTER | (reg & 0x1F));
	for (uint8_t i = 0; i < len; i++) {
		spi_txrx(buf[i]);
	}
	CSN_H();
}

static uint8_t nrf_read_status(void) {
	CSN_L();
	uint8_t s = spi_txrx(NRF_CMD_NOP);
	CSN_H();
	return s;
}

static void nrf_flush_tx(void) {
	CSN_L();
	spi_txrx(NRF_CMD_FLUSH_TX);
	CSN_H();
}

static void nrf_write_payload(const uint8_t *buf, uint8_t len) {
	CSN_L();
	spi_txrx(NRF_CMD_W_TX_PAYLOAD);
	for (uint8_t i = 0; i < len; i++) {
		spi_txrx(buf[i]);
	}
	CSN_H();
}

// nRF24 initial TX configuration
static void NRF_Init_TX(void) {
	// Idle levels
	CE_L();
	CSN_H();

	// 5-byte address width
	nrf_write_reg(NRF_REG_SETUP_AW, 0x03);

	// Disable auto-ack, only pipe0 enabled (TX still uses TX_ADDR)
	nrf_write_reg(NRF_REG_EN_AA, 0x00); // TODO: see if retry is needed
	nrf_write_reg(NRF_REG_EN_RXADDR, 0x01);

	// No retries (unused when EN_AA = 0)
	nrf_write_reg(NRF_REG_SETUP_RETR, 0x00);

	// RF channel e.g., 76 = 2.476 GHz) and 1Mbps @ 0 dBm
	nrf_write_reg(NRF_REG_RF_CH, RF_CH_VAL);
	nrf_write_reg(NRF_REG_RF_SETUP, 0x0E); // TODO: fine-tune (2 Mbps: 0x0E, 1Mbps: 0x06)

	// Set TX/RX address based on DEV ID (must match receiver pipe0)
	nrf_write_buf(NRF_REG_TX_ADDR, NRF_ADDR, 5);
	nrf_write_buf(NRF_REG_RX_ADDR_P0, NRF_ADDR, 5);

	// Static payloads, no dynamic, no features
	nrf_write_reg(NRF_REG_DYNPD, 0x00);
	nrf_write_reg(NRF_REG_FEATURE, 0x00);

	// Power-up, TX mode, 2-byte CRC, IRQs enabled
	nrf_write_reg(NRF_REG_CONFIG, 0x0E); // TODO: fine-tune (change to 1-byte CRC? 2 = 0x0E 1 = 0x0C)

	// Clear any stale IRQs and FIFOs
	NRF_ClearIRQ();
	nrf_flush_tx();
}

static void NRF_ClearIRQ(void) {
	// Write 1s to clear MAX_RT, TX_DS, RX_DR
	nrf_write_reg(NRF_REG_STATUS, NRF_STATUS_MAX_RT | NRF_STATUS_TX_DS | NRF_STATUS_RX_DR);
}

// Queue a payload and pulse CE to transmit
static bool NRF_Send(const uint8_t *payload, uint8_t len) {
	// Only accept packet lengths between [1, 32]
	if (len == 0 || len > 32) {return false;}

	// Clear interrupt flags
	NRF_ClearIRQ();
	nrf_flush_tx();

	nrf_write_payload(payload, len);

	// >10 us (microseconds) CE pulse latches the packet out of TX FIFO
	CE_H();
	// Small delay ~15-20 us; use a few NOs or a short delay
	for (volatile uint32_t i = 0; i < 800; i++) {
		; // ~>10us @ 72MHz
	}
	CE_L();

	// Optional brief status poll
//	uint8_t s = nrf_read_status();
//	if (s & NRF_STATUS_MAX_RT) {
//		NRF_ClearIRQ();
//		return false;
//	}
	return true;
}

static void SendTouchChanges(uint16_t new_mask) {
	uint16_t diff = new_mask ^ s_prev_mask;
	if (!diff) return;

	// Transmit event only for changed sensor states
	for (uint8_t ele = 0; ele < 12; ele++) {
		uint16_t pad_mask = diff & (1U << ele);
		if (diff & pad_mask) {
			uint8_t packet[TOUCH_PKT_LEN]; // {sensor_id, state}
			packet[0] = ele;
			packet[1] = (new_mask & pad_mask) ? 1 : 0;
			(void) NRF_Send(packet, TOUCH_PKT_LEN);
			// Opt. tiny gap if receiver tends to be busy
			// HAL_Delay(1);
		}
	}
	s_prev_mask = new_mask;
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
  MX_I2C1_Init();
  MX_SPI1_Init();
  /* USER CODE BEGIN 2 */

  // Initialize MPR121
  if (!MPR121_Init()) {
  	// Rapid blink on MPR121 init failure
  	while (1) {
  		HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
  		HAL_Delay(100);
  	}
  }

  // Initalize nRF24
  NRF_Init_TX();

  // Turn LED off
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  	if (g_mpr_irq_pending) {
  		g_mpr_irq_pending = 0;
  		uint16_t mask = MPR121_ReadTouchedMask();
  		g_touched_mask = mask;

  		// Light the LED while any electrode is touched
  		HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, (mask ? GPIO_PIN_RESET: GPIO_PIN_SET));
  		SendTouchChanges(mask);
  	}
//  	HAL_Delay(5);
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
}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

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

  /*Configure GPIO pin : PC13 */
  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : PA3 PA4 */
  GPIO_InitStruct.Pin = GPIO_PIN_3|GPIO_PIN_4;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : PB0 PB5 */
  GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_5;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI0_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI0_IRQn);

  HAL_NVIC_SetPriority(EXTI9_5_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

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
