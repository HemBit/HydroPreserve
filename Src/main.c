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
#include "Ultra.h"
#include <stdio.h>
#include <string.h>

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define TRANSMIT_DELAY 500     
#define UART_BUFFER_SIZE 128
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
extern double percentage; // Sensor percentage data from Ultra.c
extern uint8_t relayState; // Relay state from Ultra.c
extern uint8_t overrideActive; // Override mode from Ultra.c
char uart_tx_buffer[64]; // Buffer for UART transmission

// UART reception buffer
volatile char uart_rx_buffer[UART_BUFFER_SIZE];
volatile uint16_t rx_write_pos = 0;
volatile uint16_t rx_read_pos = 0;
volatile uint8_t rx_buffer_overflow = 0;
volatile uint8_t command_received = 0;

// Single byte buffer for interrupt reception
uint8_t rx_byte;
char command_buffer[UART_BUFFER_SIZE];
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
int measureDistance(void); // Function from Ultra.c
void initUltrasonicSensor(void); // Function from Ultra.c
void my_TIM2_us_Delay(uint32_t delay); // Function from Ultra.c
void my_SetRelayOff(void); // Function from Ultra.c
void my_ResetOverride(void); // Function from Ultra.c
void ProcessCommand(void); // Process received UART command
void CheckAndProcessCommands(void); // Check for complete commands
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void HAL_Delay(uint32_t delay_ms) {
  for(uint32_t i = 0; i < delay_ms; i++) {
    my_TIM2_us_Delay(1000); 
  }
}

// UART Reception Complete Callback
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
  if (huart->Instance == USART2) {
    uart_rx_buffer[rx_write_pos] = rx_byte;
    rx_write_pos = (rx_write_pos + 1) % UART_BUFFER_SIZE;
    
    if (rx_write_pos == rx_read_pos) {
      rx_buffer_overflow = 1;
    }
    
    if (rx_byte == '\n' || rx_byte == '\r') {
      command_received = 1;
    }
    
    HAL_UART_Receive_IT(&huart2, &rx_byte, 1);
  }
}

// Process circular buffer to extract complete commands
void CheckAndProcessCommands(void) {
  if (command_received) {
    command_received = 0;
    
    uint16_t i = 0;
    uint8_t found_start = 0;
    
    memset(command_buffer, 0, UART_BUFFER_SIZE);
    
    while (rx_read_pos != rx_write_pos) {
      char c = uart_rx_buffer[rx_read_pos];
      rx_read_pos = (rx_read_pos + 1) % UART_BUFFER_SIZE;
      
      if ((c >= 32 && c <= 126) || c == '\n' || c == '\r') {
        if (c == '{') {
          found_start = 1;
        }
        
        if (found_start) {
          if (i < UART_BUFFER_SIZE - 1) {
            command_buffer[i++] = c;
          }
          
          if (c == '}') {
            command_buffer[i] = '\0';
            ProcessCommand();
            break;
          }
        }
        
        if ((c == '\n' || c == '\r') && i > 0) {
          command_buffer[i] = '\0';
          ProcessCommand();
          break;
        }
      }
    }
    
    if (rx_buffer_overflow) {
      rx_buffer_overflow = 0;
    }
  }
}

// Process received command
void ProcessCommand(void) {
  if (strstr(command_buffer, "{\"control\":") != NULL) {
    sprintf(uart_tx_buffer, "Received command: %s\r\n", command_buffer);
    HAL_UART_Transmit(&huart2, (uint8_t*)uart_tx_buffer, strlen(uart_tx_buffer), 1000);
    
    char *control_start = strstr(command_buffer, "\"control\":\"") + 11;
    char *control_end = strchr(control_start, '\"');
    
    if (control_start != NULL && control_end != NULL) {
      *control_end = '\0';
      
      if (strcmp(control_start, "OFF") == 0) {
        my_SetRelayOff();
        
        sprintf(uart_tx_buffer, "{\"status\":\"OK\",\"action\":\"relay_off\"}\r\n");
      }
      else if (strcmp(control_start, "AUTO") == 0) {
        my_ResetOverride();
        
        sprintf(uart_tx_buffer, "{\"status\":\"OK\",\"action\":\"auto_mode\"}\r\n");
      }
      else {
        sprintf(uart_tx_buffer, "{\"status\":\"ERROR\",\"message\":\"Unknown command\"}\r\n");
      }
      
      HAL_UART_Transmit(&huart2, (uint8_t*)uart_tx_buffer, strlen(uart_tx_buffer), 1000);
    }
  }
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void) {
  /* USER CODE BEGIN 1 */
  int measurement_status;
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
  MX_USART2_UART_Init();
  
  /* USER CODE BEGIN 2 */
  // Initialize the ultrasonic sensor
  initUltrasonicSensor();
  
  // Small delay after initialization
  HAL_Delay(1000);
  
  // Send startup message
  sprintf(uart_tx_buffer, "Ultrasonic Sensor System Starting\r\n");
  HAL_UART_Transmit(&huart2, (uint8_t*)uart_tx_buffer, strlen(uart_tx_buffer), 1000);
  
  // Send a test debug message for UART
  sprintf(uart_tx_buffer, "UART reception started with interrupts enabled\r\n");
  HAL_UART_Transmit(&huart2, (uint8_t*)uart_tx_buffer, strlen(uart_tx_buffer), 1000);
  
  // Start UART reception in interrupt mode
  HAL_UART_Receive_IT(&huart2, &rx_byte, 1);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1) {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    // Check for and process any received commands
    CheckAndProcessCommands();
    
    // Measure distance with ultrasonic sensor
    measurement_status = measureDistance();
    
    // Format the data to send over UART(Create a JSON-like string with percentage and relay state)
    if (measurement_status) {
      sprintf(uart_tx_buffer, "{\"percentage\":%.2f,\"relay\":%d}\r\n", 
              percentage, 
              relayState);
    } else {
      sprintf(uart_tx_buffer, "{\"error\":\"sensor\",\"percentage\":0.00,\"relay\":%d}\r\n", 
              relayState); 
    }
    
    // Send the data via UART
    HAL_UART_Transmit(&huart2, (uint8_t*)uart_tx_buffer, strlen(uart_tx_buffer), 1000);
    
    // Delay before next measurement cycle
    HAL_Delay(TRANSMIT_DELAY);
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

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{
  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */
  
  (void)USART2->SR;
  (void)USART2->DR;
  
  USART2->CR1 |= USART_CR1_RXNEIE;  // Enable RXNE interrupt
  /* USER CODE END USART2_Init 2 */
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
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();

  /* Configure LED pins for debug */
  GPIO_InitStruct.Pin = GPIO_PIN_12|GPIO_PIN_13|GPIO_PIN_14|GPIO_PIN_15;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */
  /* Note: The ultrasonic sensor pins (PA5, PA6, PB6) are initialized in my_GPIO_Init() */
  /* USART2 pins are initialized in HAL_UART_MspInit() */
  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
/**
  * @brief UART error callback
  * @param huart: UART handle
  * @retval None
  */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart) {
  if (huart->Instance == USART2) {
    // In case of error, restart the reception
    HAL_UART_Receive_IT(&huart2, &rx_byte, 1);
  }
}
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

#ifdef  USE_FULL_ASSERT
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