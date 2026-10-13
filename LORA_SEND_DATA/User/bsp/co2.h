#ifndef CO2_H
#define CO2_H

#include "main.h"
#include "usart.h"

#define CO2_RX_BUF_SIZE 6 // 调试接收缓冲区大小
#define CO2_high_alarm_threshold 	1000	//初始高阈值
#define CO2_low_alarm_threshold		300		//初始低阈值

extern uint8_t co2_buffer[CO2_RX_BUF_SIZE]; // CO2接收缓冲
extern volatile uint8_t co2_rx_len;         // CO2接收长度

void CO2_UART_Callback(uint16_t Size);
void CO2_UART_Receive_Start(void);
uint8_t CO2_get_data(uint16_t *co2_value, uint32_t timeout);
int alarm_check(void);
void alarm_change(void);
void CO2_alarm_threshold(uint16_t co2_value);
void Key_Tick(void);
void key_ctrl_low_high(void);
#endif
