#include "light.h"
#include "adc.h" 
#include "app_main.h"
// 定义阈值
#define EPSILON    (1e-4f) // 极小阈值，可根据场景调整

extern uint16_t adc1_values[2];

// 变量声明
typedef struct
{
   uint32_t ohm; // 光敏电阻值
   uint16_t lux; // 勒克斯
} PhotoRes_TypeDef;

// GL5528光敏电阻的阻值与流明对应的关系
const PhotoRes_TypeDef GL5528[] =
    {
        {100000, 0}, // 接近全黑
        {70000, 1},  // 极暗环境
        {50000, 1},  // 深夜无灯
        {40000, 1},  // 昏暗角落
        {30000, 2},  // 弱光环境
        {20000, 4},  // 夜间灯光旁
        {15000, 5},  // 傍晚室内
        {10000, 10}, // 昏暗室内
        {7000, 17},  // 普通室内
        {5000, 29},  // 室内台灯旁
        {4000, 45},  // 明亮室内
        {3000, 68},  // 晴天窗边
        {2000, 124}, // 晴天户外阴影
        {1000, 350}, // 烈日 / 强光直射
};

// 获取光敏电压
static float get_light_voltage(void)
{
    float voltage = 0;
#if DMA_CH
	uint16_t adc_value = adc1_values[0];
	
#else
    HAL_ADC_Start(&hadc1); // 启动ADC
    // 等待转换完成，阻塞等待
    while(HAL_ADC_PollForConversion(&hadc1, HAL_MAX_DELAY) != HAL_OK);
    uint16_t adc_value = HAL_ADC_GetValue(&hadc1); // 获取ADC值
    voltage = (float)adc_value * 3.3 / 4096;       // 将ADC值转换为电压值
	HAL_ADC_Stop(&hadc1);
#endif
	
	voltage = (float)adc_value * 3.3f / 4095.0f;
    return voltage;
}

// 遍历数组，获取光照度单位lux
uint16_t GetLux(void)
{
    int i;
    uint16_t lux = 0;
    float voltage = get_light_voltage();

#if 0  // 防止除零错误                 
    if (((3.3 - voltage) >= 0.0f && (3.3 - voltage) < EPSILON) ||
         ((3.3 - voltage) < 0.0f && -(3.3 - voltage) < EPSILON)) 
        return 0;  
#endif
	//光敏电阻值
    uint32_t resistance = (uint32_t)(10.0f * 1000 * voltage ) / (3.3 - voltage);
    //根据电阻值得出光照度
    for (i = 0; i < sizeof(GL5528) / sizeof(GL5528[0]); i++)
    {
        lux = GL5528[i].lux;
        if (resistance >= GL5528[i].ohm)
        {
            break;
        }
    }
    return lux;
}