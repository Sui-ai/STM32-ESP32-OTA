#include "Key.h"
#include "tim.h"

volatile uint8_t check_update_flag = 0;

/**
  * @brief  GPIO外部中断回调函数
  * @param  GPIO_Pin: 触发中断的引脚号
  */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    // 判断是否是 PE8 引脚触发的中断
    if (GPIO_Pin == GPIO_PIN_8)
    {
        /* 定时器消抖 */
			__HAL_TIM_SetCounter(&htim6,0);
			HAL_TIM_Base_Start_IT(&htim6);
    }
}




