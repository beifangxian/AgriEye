#include "co2.h"
#include "delay.h"
#include <string.h>
#include <stdio.h>

uint8_t co2_buffer[CO2_RX_BUF_SIZE]; // CO2接收缓冲
volatile uint8_t co2_rx_len;         // CO2接收长度



//------------------------------------------------------------------
#define KEY_PRESSED     1   // 定义按键按下状态为 1 (通常表示检测到高电平)
#define KEY_UNPRESSED   0   // 定义按键松开状态为 0 (通常表示检测到低电平)

#define KEY_TIME_DOUBLE 500 // 双击按键的时间窗口阈值 (500ms)
#define KEY_TIME_LONG   1000// 长按按键的时间阈值 (1000ms，即1秒)
#define KEY_TIME_REPEAT 100 // 长按连发(重复触发)的时间间隔 (100ms)

#define KEY_HOLD				0x01
#define KEY_DOWN				0x02
#define KEY_UP					0x04
#define KEY_SINGLE				0x08
#define KEY_DOUBLE				0x10
#define KEY_LONG				0x20
#define KEY_REPEAT				0x40


uint8_t key_flag;
uint16_t CO2_high = CO2_high_alarm_threshold;
uint16_t CO2_low = CO2_low_alarm_threshold;

uint8_t alarm_flag = 0;//0:调整低阈值，1:调整高阈值

//按键状态机检查
uint8_t Key_Check(uint8_t n, uint8_t Flag)
{
	if (key_flag & Flag)
	{
		if (Flag != KEY_HOLD)
		{
			key_flag &= ~Flag;
		}
		return 1;
	}
	return 0;
}
//获取摁键状态
uint8_t Key_GetState(void)
{
	if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_12) == GPIO_PIN_RESET)
	{
		return KEY_PRESSED;
	}
	return KEY_UNPRESSED;
}
//单击双击长摁
void Key_Tick(void)
{
	static uint32_t last_scan_ms;
	static uint8_t curr_state = KEY_UNPRESSED;
	static uint8_t prev_state = KEY_UNPRESSED;
	static uint8_t state;
	static uint32_t deadline_ms;
	uint32_t now = HAL_GetTick();

	if ((now - last_scan_ms) < 20)
	{
		return;
	}
	last_scan_ms = now;

	prev_state = curr_state;
	curr_state = Key_GetState();

	if (curr_state == KEY_PRESSED)
	{
		key_flag |= KEY_HOLD;
	}
	else
	{
		key_flag &= ~KEY_HOLD;
	}

	if (curr_state == KEY_PRESSED && prev_state == KEY_UNPRESSED)
	{
		key_flag |= KEY_DOWN;
	}
	if (curr_state == KEY_UNPRESSED && prev_state == KEY_PRESSED)
	{
		key_flag |= KEY_UP;
	}

	if (state == 0)
	{
		if (curr_state == KEY_PRESSED)
		{
			deadline_ms = now + KEY_TIME_LONG;
			state = 1;
		}
	}
	else if (state == 1)
	{
		if (curr_state == KEY_UNPRESSED)
		{
			deadline_ms = now + KEY_TIME_DOUBLE;
			state = 2;
		}
		else if ((int32_t)(now - deadline_ms) >= 0)
		{
			deadline_ms = now + KEY_TIME_REPEAT;
			key_flag |= KEY_LONG;
			state = 4;
		}
	}
	else if (state == 2)
	{
		if ((int32_t)(now - deadline_ms) >= 0)
		{
			key_flag |= KEY_SINGLE;
			if (curr_state == KEY_PRESSED)
			{
				deadline_ms = now + KEY_TIME_LONG;
				state = 1;
			}
			else
			{
				state = 0;
			}
		}
		else if (curr_state == KEY_PRESSED)
		{
			key_flag |= KEY_DOUBLE;
			state = 3;
		}
	}
	else if (state == 3)
	{
		if (curr_state == KEY_UNPRESSED)
		{
			state = 0;
		}
	}
	else if (state == 4)
	{
		if (curr_state == KEY_UNPRESSED)
		{
			state = 0;
		}
		else if ((int32_t)(now - deadline_ms) >= 0)
		{
			deadline_ms = now + KEY_TIME_REPEAT;
			key_flag |= KEY_REPEAT;
		}
	}
}

void Key_ClearFlag(uint8_t flag)
{
    key_flag &= ~flag;
}

//无效函数
int alarm_check(void)
{
	if(CO2_low >= CO2_high)
	{
		printf("低阈值线超越高阈值线\n");
		return 0;
	}
	return 1;
}


//摁键控制高低阈值
void key_ctrl_low_high(void)
{
	if (key_flag & KEY_DOWN)
	{
		Key_ClearFlag(KEY_DOWN);
	}
	if (key_flag & KEY_UP)
	{
		Key_ClearFlag(KEY_UP);
	}

	if (key_flag & KEY_SINGLE)
	{
		Key_ClearFlag(KEY_SINGLE);
		if (alarm_flag == 0)
		{
			if ((CO2_low + 100) < CO2_high)
			{
				CO2_low += 100;
			}
			else
			{
				printf("低阈值不能高于或等于高阈值\n");
			}
		}
		else
		{
			if (CO2_high < 2000)
			{
				CO2_high += 100;
			}
			else
			{
				CO2_high = CO2_high_alarm_threshold;
			}
		}
	}

	if (key_flag & KEY_DOUBLE)
	{
		Key_ClearFlag(KEY_DOUBLE);
		if (alarm_flag == 0)
		{
			if (CO2_low > 100)
			{
				CO2_low -= 100;
			}
			else
			{
				CO2_low = CO2_low_alarm_threshold;
			}
		}
		else
		{
			if (CO2_high > (CO2_low + 100))
			{
				CO2_high -= 100;
			}
			else
			{
				printf("高阈值不能低于或等于低阈值\n");
			}
		}
	}

	if (key_flag & KEY_LONG)
	{
		Key_ClearFlag(KEY_LONG);
	}
	if (key_flag & KEY_REPEAT)
	{
		Key_ClearFlag(KEY_REPEAT);
	}
}

//------------------------------------------------------------------

void alarm_change(void)
{
	if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_13) == GPIO_PIN_RESET)
	{
		printf("等待摁键释放\n");
		delay_ms(15);
		alarm_flag = !alarm_flag;
		printf("Adjusting %s threshold\r\n", alarm_flag ? "HIGH" : "LOW");
		while (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_13) == GPIO_PIN_RESET);
	}
}

// 接收中断回调函数,在HAL_UARTEx_RxEventCallback中调用
void CO2_UART_Callback(uint16_t Size)
{
    co2_rx_len = Size;                                                    // 串口接收中断
    HAL_UARTEx_ReceiveToIdle_IT(&huart2, co2_buffer, sizeof(co2_buffer)); // 开启接收数据
    // printf("co2_rx_len = %d, uart1_buf = %s\n", co2_rx_len, co2_buffer);
}
// 启动接收中断
void CO2_UART_Receive_Start(void)
{
    co2_rx_len = 0;
    memset(co2_buffer, 0, sizeof(co2_buffer));
    HAL_UARTEx_ReceiveToIdle_IT(&huart2, co2_buffer, sizeof(co2_buffer));
}

/**
 * @brief 获取CO2浓度值
 *
 * @param co2_value CO2浓度值
 * @param timeout 超时时间(ms)
 * @return uint8_t 0: 成功;  1:超时;  2:模块地址错误;  3:校验和错误
 */
uint8_t CO2_get_data(uint16_t *co2_value, uint32_t timeout)
{
    uint32_t start_time = HAL_GetTick();
    while (1)
    {
        if (co2_rx_len == CO2_RX_BUF_SIZE)
        {
            break;
        }
        if (HAL_GetTick() - start_time > timeout)
        {
            return 1;
        }
    }
    

    co2_rx_len = 0; // 重新接收

    // 1. 校验模块地址（第1字节为0x2C）
    if (co2_buffer[0] != 0x2C)
    {
        return 2;
    }

    // 2. 计算和校验
    uint8_t check_sum = 0;
    for (uint8_t i = 0; i < CO2_RX_BUF_SIZE - 1; i++)
    {
        check_sum += co2_buffer[i];
    }
 
    if (check_sum != co2_buffer[CO2_RX_BUF_SIZE - 1]) // 校验和不匹配
    {
        return 3;
    }

    // 3. 解析CO2浓度值
    *co2_value = (co2_buffer[1] << 8) | co2_buffer[2];

    return 0;
}



void CO2_alarm_threshold(uint16_t co2_value)
{
	if((co2_value > CO2_low) && (co2_value < CO2_high))
	{
		//关闭蜂鸣器
		printf("co2范围在%d到%d之间\n",CO2_low,CO2_high);
		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, GPIO_PIN_RESET);

	}else{
		//启动蜂鸣器
		printf("co2范围不在%d到%d之间\n",CO2_low,CO2_high);
		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, GPIO_PIN_SET);
	}
}
