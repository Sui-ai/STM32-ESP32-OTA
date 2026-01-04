#include "tim.h"
#include "stdio.h"

#define TICK_TIME 	 		(1) //1us
#define PERIOD_TIME 		(1000)//1ms
#define DOUBLE_TIME_OUT 	(500000)//500ms
#define LONG_PRESS_TIME_OUT (3000000)//3s
volatile uint32_t double_click_delay;//双击间隔时间
volatile uint32_t dif_time = 0;//记录按键按下时间
volatile uint32_t long_pressed_time = 0;//长按时间
volatile int16_t pressed_time = -1;//0~ARR,-1为初始化值不初始化为0是因为防止按下的时候恰好按到0了
volatile uint16_t flow_cnt1 = 0;//溢出次数
volatile uint8_t	first_run_flag =  0;
extern volatile uint8_t check_update_flag;

struct Key_Event{
	uint8_t click;
	uint8_t double_click;
	uint8_t long_press;
};
struct Key_Event Key_Event = {0,0,0};

enum Key_State{
	IDLE,
	PRESSED,
	DOUBLE_CHECK,
	DOUBLE_CONFIRM,
	LONG_PRESSED,
}Key_State = IDLE;


void tim1_init(void)
{
	HAL_TIM_IC_Start_IT(&htim1,TIM_CHANNEL_2);
	HAL_TIM_IC_Start_IT(&htim1,TIM_CHANNEL_1);
	HAL_Delay(10);
	HAL_TIM_Base_Start_IT(&htim1);
}

//__attribute__((section("ramfunc"))) 
void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim)
{
	if (htim->Instance == TIM1)
	{
		if (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_1)//捕捉按键按下
		{
			/*记录按下时间*/
			dif_time = 0;
			long_pressed_time = 0;
			pressed_time = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_1);
			long_pressed_time += pressed_time*TICK_TIME; 
			if (Key_State == IDLE){
				Key_State = PRESSED;
			}
			else if (Key_State == DOUBLE_CHECK && \
				(double_click_delay + pressed_time*TICK_TIME) < DOUBLE_TIME_OUT)//双击
			{
				Key_State = DOUBLE_CONFIRM;
				double_click_delay = 0;
			}
		}
		else if (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_2)
		{
			uint32_t cap_value = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_2);
			//有溢出
			if (flow_cnt1) dif_time = (uint32_t)(flow_cnt1 - 1) * PERIOD_TIME + \
				(uint32_t)(htim->Instance->ARR - pressed_time) * TICK_TIME  + \
			 HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_2) * TICK_TIME;
			//无溢出
			else dif_time = \
				(HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_2) - pressed_time) * TICK_TIME;
			
			if (Key_State == PRESSED && dif_time > LONG_PRESS_TIME_OUT )
			{
				Key_State = IDLE;
				Key_Event.long_press += 1;
			}
			else if (Key_State == PRESSED && dif_time < LONG_PRESS_TIME_OUT)
			{
				Key_State = DOUBLE_CHECK;
				double_click_delay = (htim->Instance->ARR - cap_value)*TICK_TIME;//按键单击抬起时准备计时
			}
			else if (Key_State == DOUBLE_CONFIRM && dif_time < LONG_PRESS_TIME_OUT)
			{
				Key_Event.double_click += 1;
				Key_State = IDLE;
			}
			else if (Key_State == LONG_PRESSED)
			{
				Key_State = IDLE;
			}
			else if (Key_State == DOUBLE_CONFIRM && dif_time > LONG_PRESS_TIME_OUT)
			{
				Key_State = IDLE;
			}
			else Key_State = IDLE;
			flow_cnt1 = 0;
			pressed_time = -1;
		}
	}
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
	if (htim->Instance == TIM1)
	{
		if (Key_State == DOUBLE_CHECK) 
		{
			double_click_delay += (uint32_t) PERIOD_TIME;
			if (double_click_delay > DOUBLE_TIME_OUT)
			{
				Key_State = IDLE;
				Key_Event.click += 1;
				double_click_delay = 0;
			}
		}
		else if (Key_State == PRESSED)
		{
			if (!first_run_flag)
			{
				first_run_flag = 1;
				Key_State = IDLE;
			}
			else{
				long_pressed_time += (uint32_t) PERIOD_TIME;
				if (long_pressed_time > LONG_PRESS_TIME_OUT)
				{
					long_pressed_time = 0;
					Key_Event.long_press += 1;
					Key_State = LONG_PRESSED;
				}
			}
		}
		if (!(pressed_time & 0x7000)) flow_cnt1++;//pressed_time != -1
	}
	else if (htim->Instance == TIM6)
	{
		if (HAL_GPIO_ReadPin(GPIOE,GPIO_PIN_8) == GPIO_PIN_RESET)
		{
			check_update_flag = 1;
		}
		HAL_TIM_Base_Stop_IT(&htim6);
	}
}

