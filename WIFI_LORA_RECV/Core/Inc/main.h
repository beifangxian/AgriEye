/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f1xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define LED3_Pin GPIO_PIN_13
#define LED3_GPIO_Port GPIOC
#define DHT22_GPIO_Output_Pin GPIO_PIN_15
#define DHT22_GPIO_Output_GPIO_Port GPIOC
#define LIGHT_ADC1_IN0_Pin GPIO_PIN_0
#define LIGHT_ADC1_IN0_GPIO_Port GPIOA
#define SOIL_ADC1_IN1_Pin GPIO_PIN_1
#define SOIL_ADC1_IN1_GPIO_Port GPIOA
#define LORA_SPI_NSS_Pin GPIO_PIN_4
#define LORA_SPI_NSS_GPIO_Port GPIOA
#define LORA_SPI_SCK_Pin GPIO_PIN_5
#define LORA_SPI_SCK_GPIO_Port GPIOA
#define LORA_SPI_MISO_Pin GPIO_PIN_6
#define LORA_SPI_MISO_GPIO_Port GPIOA
#define LORA_SPI_MOSI_Pin GPIO_PIN_7
#define LORA_SPI_MOSI_GPIO_Port GPIOA
#define LORA_BUSY_Pin GPIO_PIN_0
#define LORA_BUSY_GPIO_Port GPIOB
#define LORA_RESET_Pin GPIO_PIN_1
#define LORA_RESET_GPIO_Port GPIOB
#define LORA_DIO1_Pin GPIO_PIN_10
#define LORA_DIO1_GPIO_Port GPIOB
#define LORA_DIO1_EXTI_IRQn EXTI15_10_IRQn
#define KEY1_Pin GPIO_PIN_12
#define KEY1_GPIO_Port GPIOB
#define KEY1_EXTI_IRQn EXTI15_10_IRQn
#define OLED_I2C1_SCL_Pin GPIO_PIN_6
#define OLED_I2C1_SCL_GPIO_Port GPIOB
#define OLED_I2C1_SDA_Pin GPIO_PIN_7
#define OLED_I2C1_SDA_GPIO_Port GPIOB
/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
