#include "scheduler.h"
#include "switch.h"


typedef struct TaskStruct
{
	void (*func)(void);
	uint32_t last_time;
	uint32_t period_time;
}TaskStruct;

TaskStruct tasks[] = 
{
	{switch_proc,0,10}
};

uint8_t task_num = sizeof(tasks)/sizeof(TaskStruct);

void scheduler(void)
{
	uint32_t now_time = HAL_GetTick();
	for (int i = 0; i < task_num; i++)
	{
		if (now_time - tasks[i].last_time >= tasks[i].period_time)
		{
			tasks[i].last_time = now_time;
			tasks[i].func();
		}
	}
}

