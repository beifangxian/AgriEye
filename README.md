AgriEye 农业之眼
基于 STM32F103C8T6 的环境数据采集与无线传输示例。系统由 LoRa 采集发送端和 LoRa/Wi-Fi 接收端组成：发送端读取光照、温湿度、土壤湿度和 CO₂ 数据，通过 LLCC68 LoRa 点对点发送；接收端显示数据，并通过 ESP-12F 将数据发布到巴法云 MQTT。

工程组成
工程目录	作用
LORA_SEND_DATA	传感器采集、数据打包和 LLCC68 LoRa 发送
WIFI_LORA_RECV	LLCC68 LoRa 接收、OLED 显示、ESP-12F Wi-Fi/MQTT 通信
两个工程均使用 STM32Cube HAL，芯片配置为 STM32F103C8T6，系统时钟配置为 72 MHz。工程文件位于各目录的 MDK-ARM/Template.uvprojx，CubeMX 配置文件为 Template.ioc。

功能与数据流
发送端通过 ADC、DHT22 和 CO₂ 传感器获取数据。
每约 3 秒将 5 项数据编码为一个 10 字节 LoRa 数据包。
接收端校验数据包长度，解码数据并显示在 OLED 上。
接收端通过 ESP-12F 连接 Wi-Fi 和 MQTT 服务器，发布环境数据，并订阅 LED 控制主题。
当前代码仍包含联调行为：接收端启动时会先发布一组固定测试值；运行过程中每秒发布一次接收缓存中的数据。发送数据尚未到达时，缓存可能仍为初始值。

硬件接口
两端共用的 LLCC68 接口
信号	STM32 引脚
SPI1 NSS / CS	PA4
SPI1 SCK	PA5
SPI1 MISO	PA6
SPI1 MOSI	PA7
BUSY	PB0
RESET	PB1
DIO1 中断	PB10
发送端接口
模块	STM32 引脚或接口	说明
光照传感器	PA0 / ADC1_IN0	GL5528 光敏电阻查表换算
土壤湿度传感器	PA1 / ADC1_IN1	电压换算为湿度等级
CO₂ 传感器	USART2：PA2 TX、PA3 RX	9600、8N1；接收 6 字节数据帧
DHT22	PC15	单总线温湿度传感器
调试串口	USART1：PA9 TX、PA10 RX	115200、8N1
OLED	I2C1：PB6 SCL、PB7 SDA	OLED 驱动默认为 I2C1
接收端接口
模块	STM32 引脚或接口	说明
ESP-12F	USART2：PA2 TX、PA3 RX	115200、8N1
调试串口	USART1：PA9 TX、PA10 RX	115200、8N1
OLED	I2C1：PB6 SCL、PB7 SDA	显示 LoRa 接收数据
LED	PA8	MQTT on/off 控制
发送端 USART2 接 CO₂ 传感器；接收端 USART2 接 ESP-12F。两者用途不同，接线时不要混用。请根据实际开发板和传感器模块确认电压、电平、供电与引脚定义。

LoRa 无线协议
工程使用 LLCC68 驱动进行 LoRa 点对点通信。发送端与接收端的射频参数必须保持一致。

参数	当前配置
频率	470.5 MHz
扩频因子	SF7
带宽	500 kHz
编码率	4/5
前导码	8 个符号
包头	显式包头
CRC	开启
IQ	不反转
发射功率	配置为 +20 dBm
应用负载	10 字节
负载为 5 个 16 位无符号整数，按小端序排列：

字节偏移	字段	说明
0–1	光照	Lux 查表结果
2–3	湿度	DHT22 湿度转为整数
4–5	温度	DHT22 温度转为整数
6–7	土壤湿度	查表得到的等级
8–9	CO₂	浓度值
该负载格式是应用层约定，不包含额外的设备地址或应用层校验和；无线链路使用 LoRa 包 CRC。温度和湿度在发送前转换为整数，因此当前数据包不保留 DHT22 的小数精度。

传感器数据格式
CO₂ 驱动通过 USART2 接收 6 字节数据帧：首字节应为 0x2C，第 2、3 字节组成 CO₂ 浓度值（高字节在前），末字节为前 5 字节之和的校验值。数据帧格式应与实际 CO₂ 模块手册核对。

光照值由 GL5528 电压换算出的电阻查表得到；土壤湿度输出为 1–4 的等级值。两者都依赖实际传感器、电路分压和标定，不能直接视为通用的校准测量值。

MQTT 协议与主题
接收端通过 ESP-12F 建立到巴法云的 TCP 连接，再由固件实现 MQTT 报文交互。

项目	当前配置
MQTT 版本	MQTT 3.1.1
服务器	bemfa.com:9501
传输安全	普通 TCP，未启用 TLS
客户端 ID	使用巴法云 UID
用户名 / 密码	当前为空
发布 QoS	QoS 0
心跳	每 60 秒发送 PINGREQ
MQTT 负载格式如下：

数据	负载示例格式
温度、湿度	#温度#湿度
CO₂	#浓度值
土壤湿度	#等级
光照	#lux
LED 控制	on 或 off
LED 控制通过订阅主题接收。巴法云 UID 和主题配置分别位于 WIFI_LORA_RECV/User/mqtt/esp_mqtt.h 与 WIFI_LORA_RECV/User/lora/llcc68_p2p.c；使用自己的巴法云设备时，请同步修改相关主题配置，并在平台侧创建对应主题。

编译与烧录
使用 Keil MDK-ARM / µVision 打开对应工程的 MDK-ARM/Template.uvprojx。
检查芯片型号、调试器和 Flash 下载配置。
分别编译并烧录 LORA_SEND_DATA 和 WIFI_LORA_RECV。
确保两端 LoRa 射频参数一致，再检查串口输出、OLED 显示和 MQTT 平台数据。
项目提供 Template.ioc，可用 STM32CubeMX 查看或调整外设配置。编译生成的 .axf、.hex 等文件属于本地构建产物，不纳入版本控制。

配置与注意事项
将 WIFI_SSID、WIFI_PASSWORD 设置为实际网络配置；将 BAFA_UID 设置为自己的巴法云 UID。不要将真实凭据提交到公开仓库。
当前 MQTT 使用非加密 TCP，凭据和消息没有 TLS 保护，不适合直接用于敏感或生产环境。
soil.c 当前 DMA 读取使用 adc1_values[0]，而 CubeMX 将土壤传感器配置在 ADC1_IN1；接入实物前请检查并确认 DMA 数组索引。
温度和湿度在 LoRa 发送前被转换为整数，若需要保留 0.1 精度，应调整数据包编码和接收端解码格式。
发射功率和工作频段须符合所在地无线电法规及所用模块的硬件能力。
本 README 描述的是当前代码配置；具体传感器型号、接线、电阻标定和云端主题需按实际设备确认。
