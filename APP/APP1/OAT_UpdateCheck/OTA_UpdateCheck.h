#ifndef __OTA_UpdateCheck__H
#define __OTA_UpdateCheck__H

#include "main.h"
#include "Key.h"

typedef struct OTA_Information OTA_Information;


extern uint16_t uart4_rx_index;
extern uint8_t uart4_rx_buffer[];
extern OTA_Information OTA_Info;

void OTA_UpdateCheck(OTA_Information* OTA_Info);









#endif
