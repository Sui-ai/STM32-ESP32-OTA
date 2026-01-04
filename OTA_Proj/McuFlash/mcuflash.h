#ifndef __MCUFLASH__H
#define __MCUFLASH__H

#include "main.h"

void Flash_test(void);
void OTA_APPFlashErase(uint32_t app_start_addr, uint32_t app_size);
void OTA_APPFlashWrite(uint32_t app_start_addr, uint32_t* app_data, uint32_t app_data_len);

#endif

