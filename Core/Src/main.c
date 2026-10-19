/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
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
#include <stdio.h>
#include <string.h>
#include "VL53L0X.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;

UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */
typedef enum {
    TOF_STATE_IDLE,
    TOF_STATE_WAIT_DATA
} ToF_State_t;

ToF_State_t tof_state = TOF_STATE_IDLE;
uint32_t tof_cycle_timer = 0;
uint32_t tof_timeout_timer = 0;
uint16_t tof_distance = 0;
char uart_buf[80];
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_USART1_UART_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

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
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
  // 1. Базова ініціалізація внутрішніх адрес
  if (initVL53L0X(1, &hi2c1) == 1) { // Передаємо 1 для активації 2V8 режиму I/O живлення
	  sprintf(uart_buf, "Залізо VL53L0X ініціалізовано!\r\n");
  } else {
	  sprintf(uart_buf, "Помилка ToF ініціалізації!\r\n");
  }
  HAL_UART_Transmit(&huart1, (uint8_t*)uart_buf, strlen(uart_buf), 100);

  //КОНФІГУРАЦІЯ РОЗГОНУ (High Speed & Long Range)
  // 1. Знижуємо поріг повернення сигналу (Signal Rate Limit) з 0.25 MCPS до 0.1 MCPS.
  // Це підвищує чутливість датчика до слабких відблисків від дальніх стін (до 2 метрів).
  setSignalRateLimit(100);

  // 2. Збільшуємо періоди імпульсів лазера (VCSEL Pulse Period).
  // Довші імпульси необхідні для стабільної роботи на великих відстанях (Long Range профиль ST).
  setVcselPulsePeriod(VcselPeriodPreRange, 18);  // було 10
  setVcselPulsePeriod(VcselPeriodFinalRange, 14); // було 14

  // 3. Зменшуємо таймінг-бюджет вимірювання з 300 мс до 30 мс!
  // Тепер чип буде робити одиночний замір всього за ~30 мілісекунд.
  setMeasurementTimingBudget(30000UL);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    // === АСИНХРОННИЙ СКІНЧЕННИЙ АВТОМАТ ДАЛЕКОМІРА ===
    switch (tof_state)
    {
        case TOF_STATE_IDLE:
            // Запускаємо новий замір кожні 50 мс
            if (HAL_GetTick() - tof_cycle_timer >= 50) {
                tof_cycle_timer = HAL_GetTick();

                VL53L0X_AsynchStartSingleMeasurement(); // Миттєвий старт
                tof_timeout_timer = HAL_GetTick();            // Фіксуємо час старту для таймауту
                tof_state = TOF_STATE_WAIT_DATA;
            }
            break;

        case TOF_STATE_WAIT_DATA:
            // 1. Опитуємо датчик без блокування ядра
            if (VL53L0X_AsynchCheckDataReady())
            {
                // Дані готові! Зчитуємо результат
                tof_distance = VL53L0X_AsynchReadDistanceResult();

                if (tof_distance < 2000 && tof_distance > 10) {
                    sprintf(uart_buf, "ToF (Асинхронний): %d мм\r\n", tof_distance);
                } else {
                    sprintf(uart_buf, "ToF (Асинхронний): Вперед вільно (>2м) %d мм\r\n", tof_distance);
                }
                HAL_UART_Transmit(&huart1, (uint8_t*)uart_buf, strlen(uart_buf), 10);

                tof_state = TOF_STATE_IDLE; // Повертаємо в очікування наступного такту
            }
            // 2. ЗАХИСТ: Перевірка на апаратний таймаут (якщо датчик завис або відключився)
            else if (HAL_GetTick() - tof_timeout_timer >= 100)
            {
            	sprintf(uart_buf, "ToF -> Помилка: Таймаут датчика!\r\n");
            	HAL_UART_Transmit(&huart1, (uint8_t*)uart_buf, strlen(uart_buf), 100);
                // Примусово очищаємо прапорці переривань, щоб спробувати оживити датчик
                HAL_I2C_Mem_Write(&hi2c1, (0x29<<1), 0x0B, I2C_MEMADD_SIZE_8BIT, (uint8_t[]){0x01}, 1, 10);

                tof_state = TOF_STATE_IDLE; // Скидаємо автомат, щоб не заклинило прошивку
            }
            break;
    }

    // === ТУТ ПАРАЛЕЛЬНО МОЖНА КРУТИТИ БУДЬ-ЯКІ ІНШІ ЗАВДАННЯ ===
    // Основний цикл тепер абсолютно вільний від затримок!

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
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
  hi2c1.Init.ClockSpeed = 400000;
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
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

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
