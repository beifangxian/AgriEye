/**
 * @file llcc68_p2p.c 
 * @author Jaychen (719095404@qq.com)
 * @brief lora点对点通信测试示例
 * @version 0.1
 * @date 2026-02-01
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#include "llcc68_p2p.h"
#include "llcc68.h"
#include "llcc68_regs.h"
#include "stm32f1xx_hal.h"
#include "main.h"
#include "spi.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "oled.h"
#include "delay.h"
#include "usart.h"
#include "esp.h"
#include "esp_mqtt.h"


llcc68_hal_context_t llcc68_ctx = {
	.hspi = LLCC68_SPI_HANDLE,
	.nss_port = LLCC68_NSS_PORT,
	.nss_pin = LLCC68_NSS_PIN,
	.rst_port = LLCC68_RST_PORT,
	.rst_pin = LLCC68_RST_PIN,
	.busy_port = LLCC68_BUSY_PORT,
	.busy_pin = LLCC68_BUSY_PIN,
	.dio1_port = LLCC68_DIO1_PORT,
	.dio1_pin = LLCC68_DIO1_PIN};

volatile static bool tx_done = false;
volatile static bool rx_done = false;
volatile static bool rx_timeout = false;
/* 修改：接收接口返回的是字节长度，因此接收缓冲区也按字节定义。 */
volatile uint8_t rx_data[LORA_PAYLOAD_LEN] = {0};
/* 旧代码：volatile int16_t rx_data[LORA_PAYLOAD_LEN/2] = {0}; */

extern char g_mqtt_msg[526];

/**
 * @brief DIO1中断回调函数,需要在外部中断中被调用
 *
 */
void DIO1_EXTI_Callback(void)
{
	llcc68_irq_mask_t irq_status = 0;
	// 获取并清除中断状态
	llcc68_get_and_clear_irq_status(&llcc68_ctx, &irq_status);
	printf("IRQ tatus: %#x\r\n", irq_status);

	// 处理TX_DONE中断
	if (irq_status & LLCC68_IRQ_TX_DONE)
	{
		tx_done = 1;
		printf("IRQ Tx done\r\n");
	}

	// 处理RX_DONE中断
	if (irq_status & LLCC68_IRQ_RX_DONE)
	{
		rx_done = true;
		printf("IRQ Rx done\r\n");
	}
	if (irq_status & LLCC68_IRQ_TIMEOUT)
	{
		printf("IRQ Rx timeout\r\n");
		rx_timeout = true;
	}
	if (irq_status & LLCC68_IRQ_CRC_ERROR)
	{
		printf("IRQ CRC error\r\n");
	}
}

/**
 * @brief 初始化llcc68
 *
 * @param context 上下文
 * @return llcc68_status_t
 */
llcc68_status_t llcc68_init(const void *context)
{
	llcc68_status_t status;

	// 硬件复位
	status = llcc68_hal_reset(context);
	if (status != LLCC68_STATUS_OK)
	{
		printf("LLCC68 reset failed!\r\n");
		return status;
	}
	status = llcc68_set_standby(context, LLCC68_STANDBY_CFG_RC);
	if (status != LLCC68_STATUS_OK)
	{
		printf("LLCC68 set STDBY_RC failed!\r\n");
		return status;
	}
	HAL_Delay(50);

	// 进入待机模式（XOSC）
	status = llcc68_set_standby(context, LLCC68_STANDBY_CFG_XOSC);
	if (status != LLCC68_STATUS_OK)
	{
		printf("LLCC68 set standby failed!\r\n");
		return status;
	}
	HAL_Delay(1000); // XOSC启动需等待稳定

	// 清除复位后残留错误（避免影响后续校准）
	status = llcc68_clear_device_errors(context);
	if (status != LLCC68_STATUS_OK)
	{
		printf("LLCC68 clear errors failed!\r\n");
		return status;
	}

	llcc68_cal_mask_t cal_mask = LLCC68_CAL_PLL | LLCC68_CAL_ADC_PULSE | LLCC68_CAL_ADC_BULK_N | LLCC68_CAL_ADC_BULK_P | LLCC68_CAL_IMAGE;
	status = llcc68_cal(context, cal_mask);
	if (status != LLCC68_STATUS_OK)
	{
		printf("LLCC68 cal failed!\r\n");
		return status;
	}

	// 中国频段（470-510MHz）Image校准（手册9.2.1）
	status = llcc68_cal_img_in_mhz(context, 470, 510);
	if (status != LLCC68_STATUS_OK)
	{
		printf("LLCC68 image cal failed!\r\n");
		return status;
	}

	// 配置PA参数（优化功率与可靠性）
	llcc68_pa_cfg_params_t pa_cfg = {
		.pa_duty_cycle = 0x03, // 手册13.1.14.1：+20dBm最优占空比
		.hp_max = 0x05,		   // 手册4.4.1：+20dBm对应最大增益等级
		.device_sel = 0x00,	   // 内置PA（外部PA需设为0x01）
		.pa_lut = 0x01		   // 启用PA校准表
	};
	status = llcc68_set_pa_cfg(context, &pa_cfg);
	if (status != LLCC68_STATUS_OK)
	{
		printf("LLCC68 set PA cfg failed!\r\n");
		return status;
	}

	// 配置OCP过流保护（手册5.1，避免PA损坏）
	uint8_t ocp_value = LLCC68_OCP_PARAM_VALUE_140_MA; // 内置PA默认140mA
	status = llcc68_set_ocp_value(context, ocp_value);
	if (status != LLCC68_STATUS_OK)
	{
		printf("LLCC68 set OCP failed!\r\n");
		return status;
	}

	// 设置工作模式为DCDC
	status = llcc68_set_reg_mode(context, LLCC68_REG_MODE_LDO);
	if (status != LLCC68_STATUS_OK)
		return status;

	// 设置包类型为LoRa,核心射频参数配置
	status = llcc68_set_pkt_type(context, LLCC68_PKT_TYPE_LORA);
	if (status != LLCC68_STATUS_OK)
		return status;

	// 设置射频频率, 只有选择包类型后，才能设置
	status = llcc68_set_rf_freq(context, LORA_FREQ);
	if (status != LLCC68_STATUS_OK)
		return status;

	llcc68_mod_params_lora_t lora_mod_params = {
		.sf = LORA_SF,
		.bw = LORA_BW,
		.cr = LORA_CR,
		.ldro = 0 // 关闭低数据率优化
	};
	// 设置LoRa调制参数,只有选择包类型后，才能设置
	status = llcc68_set_lora_mod_params(context, &lora_mod_params);
	if (status != LLCC68_STATUS_OK)
		return status;

	// 设置发射功率
	status = llcc68_set_tx_params(context, LORA_TX_POWER_DBM, LLCC68_RAMP_200_US);
	if (status != LLCC68_STATUS_OK)
		return status;

	// 优化500kHz带宽调制质量（即使当前用125kHz，提前兼容）
	status = llcc68_tx_modulation_workaround(context, LLCC68_PKT_TYPE_LORA, LORA_BW);
	if (status != LLCC68_STATUS_OK)
	{
		printf("LLCC68 tx modulation workaround failed!\r\n");
		return status;
	}

	// 增强PA抗天线失配能力
	status = llcc68_cfg_tx_clamp(context);
	if (status != LLCC68_STATUS_OK)
	{
		printf("LLCC68 cfg tx clamp failed!\r\n");
		return status;
	}

	// 配置DIO中断映射：DIO1映射TX_DONE和RX_DONE
	status = llcc68_set_dio_irq_params(context,
									   LLCC68_IRQ_TX_DONE | LLCC68_IRQ_RX_DONE | LLCC68_IRQ_TIMEOUT, // 使能系统中断
									   LLCC68_IRQ_TX_DONE | LLCC68_IRQ_RX_DONE | LLCC68_IRQ_TIMEOUT, // DIO1映射TX/RX完成
									   LLCC68_IRQ_NONE,												 // DIO2无映射
									   LLCC68_IRQ_NONE);											 // DIO3无映射
	if (status != LLCC68_STATUS_OK)
		return status;

	// 配置缓冲区基地址（TX:0, RX:0）
	status = llcc68_set_buffer_base_address(context, 0, 0);
	if (status != LLCC68_STATUS_OK)
		return status;

	// 清除所有中断标志
	llcc68_clear_irq_status(context, LLCC68_IRQ_ALL);
	printf("LLCC68 init success! Chip mode: STDBY_XOSC, RF freq: %lu Hz\r\n", LORA_FREQ);
	return LLCC68_STATUS_OK;
}

/**
 * @brief LoRa发送函数
 *
 * @param context 上下文
 * @param data 数据
 * @param len 数据长度
 * @param timeout_in_ms 超时时间,单位ms
 * @return llcc68_status_t
 */
llcc68_status_t llcc68_lora_send(const void *context, const uint8_t *data, uint8_t len, uint32_t timeout_in_ms)
{
	llcc68_status_t status;
	llcc68_pkt_params_lora_t lora_pkt_params = {
		.preamble_len_in_symb = LORA_PREAMBLE_LEN, // 前导码长度8个符号
		.header_type = LLCC68_LORA_PKT_EXPLICIT,   // 显式头
		.pld_len_in_bytes = len,				   // 有效载荷长度（发送时动态设置）
		.crc_is_on = true,						   // 启用CRC
		.invert_iq_is_on = false				   // 不反转IQ
	};
	tx_done = false;

	// 进入频率合成模式
	status = llcc68_set_fs(context);
	if (status != LLCC68_STATUS_OK)
		return status;

	// 设置数据包长度,必须设置包类型，再设置LoRa数据包参数
	status = llcc68_set_pkt_type(context, LLCC68_PKT_TYPE_LORA);
	if (status != LLCC68_STATUS_OK)
		return status;

	// 设置LoRa数据包参数
	status = llcc68_set_lora_pkt_params(context, &lora_pkt_params);
	if (status != LLCC68_STATUS_OK)
		return status;

	// 配置LoRa调制参数（与接收端、初始化参数一致）
	llcc68_mod_params_lora_t lora_mod_params = {
		.sf = LORA_SF, // 扩频因子
		.bw = LORA_BW, // 带宽
		.cr = LORA_CR, // 编码率
		.ldro = 0	   // 低数据率优化（SF9+125kHz无需启用）
	};
	status = llcc68_set_lora_mod_params(context, &lora_mod_params);
	if (status != LLCC68_STATUS_OK)
		return status;

	if (len > LORA_PAYLOAD_LEN)
	{
		printf("Data length is too long!\r\n");
		return LLCC68_STATUS_ERROR;
	}
	// 写入发送数据到TX缓冲区
	status = llcc68_write_buffer(context, 0, data, len);
	if (status != LLCC68_STATUS_OK)
		return status;

	// 设置发送模式
	status = llcc68_set_tx(context, timeout_in_ms);
	if (status != LLCC68_STATUS_OK)
		return status;
	printf("sending...\r\n");

	// 等待发送完成或超时
	uint32_t start = HAL_GetTick();
	while (!tx_done)
	{
		if (HAL_GetTick() - start > timeout_in_ms)
		{
			tx_done = false;
			printf("Send timeout...!\r\n");
			return LLCC68_STATUS_ERROR;
		}
		HAL_Delay(1);
	}
	printf("Sent completed!\r\n");
	return LLCC68_STATUS_OK;
}

llcc68_status_t llcc68_lora_receive_mode(const void *context, uint32_t timeout_in_ms)
{
	llcc68_status_t status;
	llcc68_pkt_params_lora_t lora_pkt_params = {
		.preamble_len_in_symb = LORA_PREAMBLE_LEN, // 与发送端一致
		.header_type = LLCC68_LORA_PKT_EXPLICIT,   // 与发送端一致
		.pld_len_in_bytes = LORA_PAYLOAD_LEN,	   // 最大接收长度
		.crc_is_on = true,						   // 与发送端一致
		.invert_iq_is_on = false				   // 与发送端一致
	};

	// 设置接收模式为Boosted
	status = llcc68_cfg_rx_boosted(context, true);
	if (status != LLCC68_STATUS_OK)
	{
		printf("Failed to set RX boosted mode.\r\n");
		// 可以选择返回错误或继续
	}
	// 先设包类型和参数，再进入 FS 模式
	status = llcc68_set_pkt_type(context, LLCC68_PKT_TYPE_LORA);
	if (status != LLCC68_STATUS_OK)
		return status;
	status = llcc68_set_lora_pkt_params(context, &lora_pkt_params);
	if (status != LLCC68_STATUS_OK)
		return status;
	// 最后进入FS模式
	status = llcc68_set_fs(context);
	if (status != LLCC68_STATUS_OK)
		return status;

	// 设置为接收模式
//	status = llcc68_set_rx(context, timeout_in_ms);
//	if (status != LLCC68_STATUS_OK)
//	{
//		printf("Set RX mode failed!\r\n");
//	}
	if(timeout_in_ms==0){
		status = llcc68_set_rx_with_timeout_in_rtc_step(context,LLCC68_RX_CONTINUOUS);
	}
	else{
		status = llcc68_set_rx(context, timeout_in_ms);
	}
	if (status != LLCC68_STATUS_OK)
	{
		printf("Set RX mode failed!\r\n");
	}
	
	
	return status;
}

llcc68_status_t llcc68_lora_receive_data(const void *context, uint8_t *data, uint16_t *len,
										 llcc68_pkt_status_lora_t *pkt_status, uint32_t timeout_in_ms)
{
	llcc68_status_t status = LLCC68_STATUS_ERROR;
	printf("Waiting for packet...\r\n");
	// 等待接收完成或超时
	uint32_t start = HAL_GetTick();
	while (!rx_done && !rx_timeout)
	{
		if (HAL_GetTick() - start > timeout_in_ms + 10)
		{
			tx_done = false;
			printf("Receive timeout...!\r\n");
			return status;
		}
		HAL_Delay(1);
	}

	if (rx_done)
	{
		// 获取接收状态
		llcc68_rx_buffer_status_t rx_buffer_status;
		status = llcc68_get_rx_buffer_status(context, &rx_buffer_status);
		if (status != LLCC68_STATUS_OK)
		{
			printf("Get RX buffer status failed!\r\n");
			goto rx;
		}
		// 获取包状态
		status = llcc68_get_lora_pkt_status(context, pkt_status);
		if (status != LLCC68_STATUS_OK)
		{
			printf("Get packet status failed!\r\n");
			goto rx;
		}

		// 从FIFO读取数据
		*len = rx_buffer_status.pld_len_in_bytes;
		status = llcc68_read_buffer(context, rx_buffer_status.buffer_start_pointer, data, *len);
		if (status != LLCC68_STATUS_OK)
		{
			printf("Read buffer failed!\r\n");
			goto rx;
		}
		printf("Reception completed\r\n");
	}
	else
	{
		printf("Receive timeout...\r\n");
		goto rx;
	}
	// 处理接收完成后的清理工作（重要！）
	llcc68_handle_rx_done(context);
rx:
	//  清理接收完成后的标志位
	rx_done = false;
	rx_timeout = false;
	return status;
}



// LoRa测试函数
void llcc68_p2p_demo(void)
{
	uint32_t start = HAL_GetTick();
	llcc68_status_t status;

	// 初始化LLCC68
	while (1)
	{
		status = llcc68_init(&llcc68_ctx);

		if (status != LLCC68_STATUS_OK)
		{
			printf("LLCC68初始化失败！\r\n");
		}
		else
		{
			printf("LLCC68初始化成功！\r\n");
			break;
		}
		HAL_Delay(1000);
	}
	// 测试数据
		// 测试数据
	uint16_t rx_data_len = 0;
	llcc68_pkt_status_lora_t pkt_status;
	
	
	    uint32_t last_heart_tick = 0;     /* 上次心跳时间（HAL_GetTick） */
    uint32_t last_publish_tick = 0;   /* 上次发布时间（HAL_GetTick） */
    uint16_t payload_offset;          /* 消息内容起始位置 */
    char topic[32] = {0};             /* 主题缓冲区 */

    printf("\r\n");
    printf("========================================\r\n");
    printf("      ESP12-F 巴法云MQTT测试\r\n");
    printf("========================================\r\n");
    printf("\r\n");

    /* [1] 初始化并连接巴法云MQTT服务器 */
    printf("[1] 初始化ESP模块并连接巴法云MQTT服务器...\r\n");
    while (esp_mqtt_init())
    {
        printf("esp_mqtt_init ...重试\r\n");
        /*
         * 初始化失败时先退出可能残留的透传状态，再清空 UART 接收区；
         * 如果先清缓冲，模块仍处于透传状态时可能继续产生无效残留数据。
         *
         * 原:
         * printf("esp_mqtt_init ...重试\r\n");
         * delay_ms(1000);
         */
        esp_exit_transparent_transmission();
        esp_clear_rx_buf();
        delay_ms(1000);
    }
    printf("    [OK] 连接巴法云成功\r\n");
    printf("\r\n");

    /* [2] 发布测试数据到温湿度主题 */
    printf("[2] 发布测试数据到主题 %s...\r\n", MQTT_PUBLISH_TOPIC);
    sprintf(g_mqtt_msg, "#25.5#60.0");
    mqtt_publish_data(MQTT_PUBLISH_TOPIC, g_mqtt_msg, 0);
    printf("    发布数据: %s\r\n", g_mqtt_msg);
    printf("\r\n");

    /* [3] 发布LED状态到LED主题 */
    printf("[3] 发布LED状态到主题 %s...\r\n", BAFA_TOPIC_LED);
    sprintf(g_mqtt_msg, "on");
    mqtt_publish_data(BAFA_TOPIC_LED, g_mqtt_msg, 0);
    printf("    发布数据: %s\r\n", g_mqtt_msg);
    printf("\r\n");
	
	/* [4] 发布测试数据到CO2浓度主题 */
    printf("[4] 发布测试数据到主题 %s...\r\n", "XXgfyUYJo004");
    sprintf(g_mqtt_msg, "#350");
    mqtt_publish_data("XXgfyUYJo004", g_mqtt_msg, 0);
    printf("    发布数据: %s\r\n", g_mqtt_msg);
    printf("\r\n");
	
	/* [5] 发布测试数据到土壤湿度等级主题 */
    printf("[5] 发布测试数据到主题 %s...\r\n", "uer5PHwpr004");
    sprintf(g_mqtt_msg, "#4");
    mqtt_publish_data("uer5PHwpr004", g_mqtt_msg, 0);
    printf("    发布数据: %s\r\n", g_mqtt_msg);
    printf("\r\n");
	
	/* [5] 发布测试数据到光照强度主题 */
    printf("[5] 发布测试数据到主题 %s...\r\n", "3QrntTY7C004");
    sprintf(g_mqtt_msg, "#4");
    mqtt_publish_data("3QrntTY7C004", g_mqtt_msg, 0);
    printf("    发布数据: %s\r\n", g_mqtt_msg);
    printf("\r\n");

    printf("========================================\r\n");
    printf("      进入消息接收循环（发送end退出）\r\n");
    printf("========================================\r\n");
	

	while (1)
	{
		if (g_esp_rx_end && g_esp_transparent_transmission_sta)
        {
            /*
             * 一次接收可能包含多个MQTT数据包（粘包），需要逐包解析。
             * 常见类型：
             *   0x90 = SUBACK（订阅响应）
             *   0x30 = PUBLISH（发布消息）
             *   0xD0 = PINGRESP（心跳响应）
             */
            uint16_t offset = 0;

            while (offset + 1 < g_esp_rx_cnt)
            {
                uint8_t pkt_type = g_esp_rx_buf[offset];
                uint8_t remain_len = g_esp_rx_buf[offset + 1];

                /* 检查剩余长度是否合法 */
                if (remain_len == 0 || offset + 2 + remain_len > g_esp_rx_cnt)
                {
                    break;  /* 数据不完整，等待下一帧 */
                }

                if (pkt_type == 0x30)
                {
                    /* ===== PUBLISH 消息解析 ===== */
                    uint16_t topic_len = (g_esp_rx_buf[offset + 2] << 8) | g_esp_rx_buf[offset + 3];
                    uint16_t pkt_end = offset + 2 + remain_len;

                    printf("\r\n========== MQTT PUBLISH 消息 ==========\r\n");
                    printf("消息类型: 0x%02X (PUBLISH)\r\n", pkt_type);
                    printf("剩余长度: %u\r\n", remain_len);
                    printf("主题长度: %u\r\n", topic_len);

                    /* 提取并打印主题名 */
                    uint16_t copy_len = topic_len > 31 ? 31 : topic_len;
                    memset(topic, 0, sizeof(topic));
                    memcpy(topic, (void *)&g_esp_rx_buf[offset + 4], copy_len);
                    topic[copy_len] = '\0';
                    printf("主题名: \"%s\"\r\n", topic);

                    /* 计算消息内容起始位置 */
                    payload_offset = offset + 2 + 2 + topic_len;

                    /* 提取并打印消息内容 */
                    if (pkt_end > payload_offset)
                    {
                        char *payload = (char *)&g_esp_rx_buf[payload_offset];
                        uint16_t payload_len = pkt_end - payload_offset;

                        printf("消息内容: \"");
                        for (uint16_t i = 0; i < payload_len && i < 32; i++)
                        {
                            printf("%c", payload[i]);
                        }
                        printf("\" (长度: %u)\r\n", payload_len);

                        /* 检查是否收到退出指令 */
                        if (strstr(payload, "end") != NULL)
                        {
                            printf("    [收到退出指令，退出MQTT测试]\r\n");
                            goto mqtt_exit;
                        }

                        /* 检查LED控制指令 */
                        if (strstr(payload, "on"))
                        {
							HAL_GPIO_WritePin(GPIOA,GPIO_PIN_8,GPIO_PIN_RESET);
                            printf("    [LED ON 指令]\r\n");
                        }
                        else if (strstr(payload, "off"))
                        {
							HAL_GPIO_WritePin(GPIOA,GPIO_PIN_8,GPIO_PIN_SET);
                            printf("    [LED OFF 指令]\r\n");
                        }
                    }
                    printf("========================================\r\n\r\n");
                }
                else if (pkt_type == 0x90)
                {
                    /* SUBACK，跳过 */
                    printf("[SUBACK] 订阅响应 (offset=%u)\r\n", offset);
                }
                else if (pkt_type == 0xD0)
                {
                    /* PINGRESP，跳过 */
                    printf("[PINGRESP] 心跳响应\r\n");
                }
                else
                {
                    /* 未知类型，打印并跳过 */
                    printf("[未知] 类型: 0x%02X (offset=%u)\r\n", pkt_type, offset);
                }

                /* 移动到下一个数据包 */
                offset += 2 + remain_len;
            }

            /* 清空接收缓冲区并重启UART接收，准备接收下一帧数据 */
            esp_clear_rx_buf();
        }


        /* 每60秒发送一次心跳保活 */
        if ((HAL_GetTick() - last_heart_tick) >= 60000)
        {
            mqtt_send_heart();
            printf("[心跳] 已发送PINGREQ\r\n");
            last_heart_tick = HAL_GetTick();
        }

        /* 每1秒发布一次CO2,温湿度,土壤湿度，光照强度测试数据 */
        if ((HAL_GetTick() - last_publish_tick) >= 1000)
        {
			//温湿度
            sprintf(g_mqtt_msg, "#%.1f#%.1f", (float)(rx_data[4] | rx_data[5] << 8),(float)(rx_data[2] | (rx_data[3] << 8)));
            mqtt_publish_data(MQTT_PUBLISH_TOPIC, g_mqtt_msg, 0);
            printf("[发布] %s -> %s\r\n", g_mqtt_msg, MQTT_PUBLISH_TOPIC);
			//CO2
			sprintf(g_mqtt_msg, "#%d", rx_data[8] | (rx_data[9] << 8));
            mqtt_publish_data("XXgfyUYJo004", g_mqtt_msg, 0);
            printf("[发布] %s -> %s\r\n", g_mqtt_msg, "XXgfyUYJo004");
			//土壤湿度等级
			sprintf(g_mqtt_msg, "#%d", rx_data[6] | (rx_data[7] << 8));
            mqtt_publish_data("uer5PHwpr004", g_mqtt_msg, 0);
            printf("[发布] %s -> %s\r\n", g_mqtt_msg, "uer5PHwpr004");
			//光照强度
			sprintf(g_mqtt_msg, "#%d", rx_data[0] | (rx_data[1] << 8));
            mqtt_publish_data("3QrntTY7C004", g_mqtt_msg, 0);
            printf("[发布] %s -> %s\r\n", g_mqtt_msg, "3QrntTY7C004");
            last_publish_tick = HAL_GetTick();
        }
		// 每轮先确保芯片处于接收状态。
		//启动接受
		llcc68_lora_receive_mode(&llcc68_ctx, 0);
		if (llcc68_lora_receive_data(&llcc68_ctx, (uint8_t *)rx_data, &rx_data_len, &pkt_status, 1000) == LLCC68_STATUS_OK)
		{
			if (rx_data_len != 10)
			{
				printf("RX payload length=%u bytes, expected 10 bytes\r\n",
					   (unsigned)rx_data_len);
				continue;
			}
			//OLED显示部分
			char nums[20] = {0};
			
			sprintf(nums, "lux:%d", (rx_data[0] | rx_data[1] << 8));
			OLED_ShowString(0, 0, nums, 16);
			memset(nums, 0, sizeof(nums));
			
			sprintf(nums, "humi:%.1f", (float)(rx_data[2] | rx_data[3] << 8));
			OLED_ShowString(0, 16, nums, 16);
			memset(nums, 0, sizeof(nums));
			
			sprintf(nums, "temp:%.1f", (float)(rx_data[4] | (rx_data[5] << 8)));
			OLED_ShowString(0, 32, nums, 16);
			memset(nums, 0, sizeof(nums));
			
			sprintf(nums, "soil:%d", (rx_data[6] | rx_data[7] << 8));
			OLED_ShowString(60, 0, nums, 16);
			memset(nums, 0, sizeof(nums));
			
			sprintf(nums, "co2:%d", (rx_data[8] | rx_data[9] << 8));
			OLED_ShowString(0, 48, nums, 16);
			memset(nums, 0, sizeof(nums));
			
			//printf("接收信号强度: %d dBm\r\n", pkt_status.rssi_pkt_in_dbm); // 数值越大信号越强
			//printf("接收信噪比: %d dB\r\n", pkt_status.snr_pkt_in_db);		// 数值越大信噪比越好
		}

	}
	mqtt_exit:
    /* [5] 退出处理 */
    printf("\r\n[5] 退出MQTT测试...\r\n");

    /* 退出透传模式 */
    esp_exit_transparent_transmission();
    printf("    已退出透传模式\r\n");

    /* 断开服务器连接 */
    esp_disconnect_server();
    printf("    已断开服务器连接\r\n");

    printf("\r\n========================================\r\n");
    printf("      巴法云MQTT测试完成\r\n");
    printf("========================================\r\n");
}
