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
#include "nrf24l01.h"
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
#define MPR121_REG_DEBOUNCE					0x5B
#define MPR121_REG_AFE_CFG1   			0x5C  // FFI (bits 7:6) | CDC (bits 5:0)
#define MPR121_REG_AFE_CFG2   			0x5D  // CDT (7:5) | SFI (4:3) | ESI (2:0)
#define MPR121_REG_ECR 							0x5E
#define MPR121_REG_ELE0_T						0x41 // ELE0_R=0x42, ELE1_T=0x43, ...

#define MPR121_AFE_CFG1_RESET 			0x10  // FFI=00 (6 samples), CDC=0x10 (16 µA)
#define MPR121_AFE_CFG2_RESET 			0x24  // CDT=001 (0.5 µs), SFI=00 (4), ESI=100 (16 ms)
#define MPR121_AFE_CFG1 						0b00011000 // 24uA
#define MPR121_AFE_CFG2							0b00100001 // CDT 0.5us, ESI 2ms

#define MPR121_TOUCH_THR_DEFAULT 		0x18 // TODO: fine-tune
#define MPR121_RELEASE_THR_DEFAULT 	0x16 // TODO: fine-tune
#define MPR121_DEBOUNCE							0x00 // TODO: fine-tune
#define MPR121_RUN_MODE							0x86 // TODO: fine-tune

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
#if (DEVICE_ID < 0) || (DEVICE_ID > 7)
#error "DEVICE_ID must be in range [0, 7]"
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

	if (MPR121_Write8(MPR121_REG_AFE_CFG1, MPR121_AFE_CFG1) != HAL_OK) {return false;}
	if (MPR121_Write8(MPR121_REG_AFE_CFG2, MPR121_AFE_CFG2) != HAL_OK) {return false;}

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

/* nrf24l01 */
uint8_t TxBuffer[NRF24L01_PAYLOAD_LENGTH] = {0};

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
  while (!MPR121_Init()) {
  	HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
		HAL_Delay(100);
  }

  // Initalize nRF24
  uint8_t TxAddress1[5] = {0xB3, 0xB4, 0xB5, 0xB6, DEVICE_ID};

	NRF24L01_TxInit(70, NRF24L01_DATA_RATE_2MBPS, 2000);

	NRF24L01_SetTxAddress(TxAddress1, 2000);


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
  		uint8_t mask = MPR121_ReadTouchedMask() & 0xFF;
  		g_touched_mask = mask;

  		// Light the LED while any electrode is touched
  		HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, (mask ? GPIO_PIN_RESET: GPIO_PIN_SET));
  		TxBuffer[0] = DEVICE_ID;
  		TxBuffer[1] = mask;
  		NRF24L01_TxTransmit(TxBuffer, 2000);
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
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);

  /*Configure GPIO pin : PC13 */
  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : PA3 */
  GPIO_InitStruct.Pin = GPIO_PIN_3;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : PA4 */
  GPIO_InitStruct.Pin = GPIO_PIN_4;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : PB0 */
  GPIO_InitStruct.Pin = GPIO_PIN_0;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : PB5 */
  GPIO_InitStruct.Pin = GPIO_PIN_5;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI3_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI3_IRQn);

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
