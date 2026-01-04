#ifndef __TIMCAPTURE__H
#define __TIMCAPTURE__H

#include "main.h"

void tim1_init(void);
int add(int a, int b);

struct Key_Event{
	uint8_t click;
	uint8_t double_click;
	uint8_t long_press;
};

extern volatile uint32_t dif_time;
extern struct Key_Event Key_Event;











#endif
