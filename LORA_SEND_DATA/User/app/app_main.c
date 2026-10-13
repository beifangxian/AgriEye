#include "app_main.h"
#include "main.h"
#include "tim.h" // 包含STM32CubeMX生成的TIM头文件
#include "debug.h"
#include "oled_demo.h"
#include "oled.h"
#include "co2.h"
#include "light.h"
#include "dht22.h"
#include "delay.h"
#include "llcc68_p2p.h"

/* ==================== */
/*                      */
/*    发        送      */
/*                      */
/* ==================== */

/*
	本次发送存在8发送到接收为4的情况
	其原因肯是build和Rebuild的区别
*/

// 替代main函数中的while循环
/************
!!!!!!!!
如果开发板没有对应的硬件一定，不要进行初始化

*************/

uint16_t adc1_values[2] = {0};

void app_main(void)
{
	//定义co2,温湿度，光值
	uint16_t co2_val;
	float temp, hum;
	uint16_t light_val;
	uint8_t res;
	DHT22_Init();
	//OLED初始化
	//OLED_Init();
	CO2_UART_Receive_Start();//启动co2

	llcc68_p2p_demo();
    while (1)      
    {

			
			
    }
}