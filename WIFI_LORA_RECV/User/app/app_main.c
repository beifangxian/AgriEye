#include "app_main.h"
#include "main.h"
#include "delay.h"
#include "tim.h"
#include "debug.h"
#include "stdlib.h"
#include "string.h"
#include "usart.h"
#include "debug.h"
#include "adc.h"
#include "oled.h"
#include "oled_demo.h"
#include "llcc68_p2p.h"
#include "esp_mqtt.h"
#include "esp.h"

/* 宏：计算数组元素个数，x 为数组名 */
#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

/* 串口1（调试串口）接收相关变量 */
uint8_t g_uart1_rx_buf[32] = {0}; /* 串口1接收缓冲区，容量 32 字节，初始化为 0 */
uint32_t g_uart1_rx_cnt = 0;	  /* 串口1接收计数器，记录已接收的字节数 */
uint32_t g_uart1_rx_end = 0;	  /* 串口1接收完成标志，0 表示未完成 */


// 替代main函数中的while循环
void app_main(void)
{
    uint32_t tick_1S = HAL_GetTick();
	OLED_Init();
	/* 启动串口1空闲中断接收（调试用，接收上位机数据） */
	HAL_UARTEx_ReceiveToIdle_IT(&huart1, g_uart1_rx_buf, sizeof(g_uart1_rx_buf));
	llcc68_p2p_demo();
	//esp_mqtt_test1();
    while (1) // 你可以在这里处理其它事情的代码
    {
    }
}

