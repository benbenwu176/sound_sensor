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
#include "usbd_cdc_if.h"
#include "string.h"
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "nrf24l01.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#define PAYLOAD_LENGTH 2

// GPIO Pins
#define NRFA_CE_GPIO_PORT 	GPIOB
#define NRFA_CE_GPIO_PIN    GPIO_PIN_0
#define NRFA_CS_GPIO_PORT 	GPIOA
#define NRFA_CS_GPIO_PIN    GPIO_PIN_4
#define NRFA_IRQ_GPIO_PORT 	GPIOA
#define NRFA_IRQ_GPIO_PIN 	GPIO_PIN_3

#define NRFB_CE_GPIO_PORT 	GPIOB
#define NRFB_CE_GPIO_PIN    GPIO_PIN_11
#define NRFB_CS_GPIO_PORT 	GPIOB
#define NRFB_CS_GPIO_PIN    GPIO_PIN_12
#define NRFB_IRQ_GPIO_PORT 	GPIOB
#define NRFB_IRQ_GPIO_PIN 	GPIO_PIN_10

#define NRF_FIFO_RX_EMPTY_BIT  (1U << 0)

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
SPI_HandleTypeDef hspi1;
SPI_HandleTypeDef hspi2;

/* USER CODE BEGIN PV */
extern USBD_HandleTypeDef hUsbDeviceFS;

NRF24L01_HandleTypeDef nrf_rx_A;
NRF24L01_HandleTypeDef nrf_rx_B;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_SPI1_Init(void);
static void MX_SPI2_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

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

volatile uint8_t nrfA_pending = 0;
volatile uint8_t nrfB_pending = 0;

uint8_t usb_buffer_A[2] = {0};
uint8_t usb_buffer_B[2] = {0};

uint8_t rx_pipe_A = 0;
uint8_t rx_pipe_B = 0;

uint8_t rx_buffer_A[PAYLOAD_LENGTH] = {0, };
uint8_t rx_buffer_B[PAYLOAD_LENGTH] = {0, };

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
	if (GPIO_Pin == nrf_rx_A.IRQ_GPIO_PIN) {
		nrfA_pending = 1;
	}

	if (GPIO_Pin == nrf_rx_B.IRQ_GPIO_PIN) {
		nrfB_pending = 1;
	}
}

void NRF_Configure_Handles(void) {
	nrf_rx_A.hspi          = &hspi1;
	nrf_rx_A.CS_GPIO_PORT  = NRFA_CS_GPIO_PORT;
	nrf_rx_A.CS_GPIO_PIN   = NRFA_CS_GPIO_PIN;
	nrf_rx_A.CE_GPIO_PORT  = NRFA_CE_GPIO_PORT;
	nrf_rx_A.CE_GPIO_PIN   = NRFA_CE_GPIO_PIN;
	nrf_rx_A.IRQ_GPIO_PORT = NRFA_IRQ_GPIO_PORT;
	nrf_rx_A.IRQ_GPIO_PIN  = NRFA_IRQ_GPIO_PIN;

	nrf_rx_B.hspi          = &hspi2;
	nrf_rx_B.CS_GPIO_PORT  = NRFB_CS_GPIO_PORT;
	nrf_rx_B.CS_GPIO_PIN   = NRFB_CS_GPIO_PIN;
	nrf_rx_B.CE_GPIO_PORT  = NRFB_CE_GPIO_PORT;
	nrf_rx_B.CE_GPIO_PIN   = NRFB_CE_GPIO_PIN;
	nrf_rx_B.IRQ_GPIO_PORT = NRFB_IRQ_GPIO_PORT;
	nrf_rx_B.IRQ_GPIO_PIN  = NRFB_IRQ_GPIO_PIN;
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

  NRF_Configure_Handles();

  NRF24L01_SetRxPayloadWidths(&nrf_rx_A, PAYLOAD_LENGTH, 2000);
  NRF24L01_SetRxPayloadWidths(&nrf_rx_B, PAYLOAD_LENGTH, 2000);

  // Radios
  uint8_t RxAddress0_A[5] = {0xB3, 0xB4, 0xB5, 0xB6, 0x00};
  uint8_t RxAddress1_A[5] = {0xB3, 0xB4, 0xB5, 0xB6, 0x01};
  uint8_t RxAddress2_A = 0x02;
  uint8_t RxAddress3_A = 0x03;

  uint8_t RxAddress0_B[5] = {0xB3, 0xB4, 0xB5, 0xB6, 0x04};
	uint8_t RxAddress1_B[5] = {0xB3, 0xB4, 0xB5, 0xB6, 0x05};
	uint8_t RxAddress2_B = 0x06;
	uint8_t RxAddress3_B = 0x07;


	NRF24L01_RxInit(&nrf_rx_A, 70, NRF24L01_DATA_RATE_2MBPS, 2000);
	NRF24L01_RxInit(&nrf_rx_B, 85, NRF24L01_DATA_RATE_2MBPS, 2000);

	NRF24L01_SetRxAddress(&nrf_rx_A, NRF24L01_RX_ADDRESS_P0, RxAddress0_A, 2000);
	NRF24L01_SetRxAddress(&nrf_rx_A, NRF24L01_RX_ADDRESS_P1, RxAddress1_A, 2000);
	NRF24L01_SetRxAddress(&nrf_rx_A, NRF24L01_RX_ADDRESS_P2, &RxAddress2_A, 2000);
	NRF24L01_SetRxAddress(&nrf_rx_A, NRF24L01_RX_ADDRESS_P3, &RxAddress3_A, 2000);

	NRF24L01_SetRxAddress(&nrf_rx_B, NRF24L01_RX_ADDRESS_P0, RxAddress0_B, 2000);
	NRF24L01_SetRxAddress(&nrf_rx_B, NRF24L01_RX_ADDRESS_P1, RxAddress1_B, 2000);
	NRF24L01_SetRxAddress(&nrf_rx_B, NRF24L01_RX_ADDRESS_P2, &RxAddress2_B, 2000);
	NRF24L01_SetRxAddress(&nrf_rx_B, NRF24L01_RX_ADDRESS_P3, &RxAddress3_B, 2000);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1) {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  	if (nrfA_pending) {
			nrfA_pending = 0;
			while ((NRF24L01_GetFIFOStatus(&nrf_rx_A, 200) & NRF_FIFO_RX_EMPTY_BIT) == 0) {
				NRF24L01_RxReceive(&nrf_rx_A, rx_buffer_A, &rx_pipe_A, 200);
				memcpy(usb_buffer_A, rx_buffer_A, PAYLOAD_LENGTH);
				send_bytes(usb_buffer_A, PAYLOAD_LENGTH);
			}
//			uint8_t status = NRF24L01_GetStatus(&nrf_rx_A, 200);
//
//			if (status & NRF24L01_TX_DS_MASK) {
//				NRF24L01_ClearTxDS(&nrf_fx_A, 200);
//			}
//
//			if (status & NRF24L01_MAX_RT_MASK) {
//				NRF24L01_ClearMaxRT(&nrf_rx_A, 200);
//				NRF24L01_FlushTxFIFO(&nrf_rx_A, 200);
//			}
//
//			if (status & NRF24L01_RX_DR_MASK) {
//				/* read until RX FIFO is empty */
//				while ((NRF24L01_GetFIFOStatus(&nrf_rx_A, 200) & NRF_FIFO_RX_EMPTY_BIT) == 0) {
//					NRF24L01_RxReceive(&nrf_rx_A, rx_buffer_A, &rx_pipe_A, 200);
//					memcpy(usb_buffer_A, rx_buffer_A, PAYLOAD_LENGTH);
//					send_bytes(usb_buffer_A, PAYLOAD_LENGTH);
//				}
//			}
		}

		if (nrfB_pending) {
			nrfB_pending = 0;

			while ((NRF24L01_GetFIFOStatus(&nrf_rx_B, 200) & NRF_FIFO_RX_EMPTY_BIT) == 0) {
				NRF24L01_RxReceive(&nrf_rx_B, rx_buffer_B, &rx_pipe_B, 200);
				memcpy(usb_buffer_B, rx_buffer_B, PAYLOAD_LENGTH);
				send_bytes(usb_buffer_B, PAYLOAD_LENGTH);
			}
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
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0|GPIO_PIN_11|GPIO_PIN_12, GPIO_PIN_RESET);

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

  /*Configure GPIO pins : PB0 PB11 PB12 */
  GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_11|GPIO_PIN_12;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : PB10 */
  GPIO_InitStruct.Pin = GPIO_PIN_10;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI3_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI3_IRQn);

  HAL_NVIC_SetPriority(EXTI15_10_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

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
