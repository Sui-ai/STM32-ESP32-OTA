#ifndef __SWITCH__H
#define __SWITCH__H

#include "main.h"



typedef enum Switch
{
	SYSTEM_IDLE = 0,			/*系统空闲状态*/
	SYSTEM_JUMP_APP,			/*系统跳转APP状态*/
	SYSTEM_PREPAR_UPDATE,	/*系统准备更新状态*/
	SYSTEM_PROC,					/*系统处理数据状态*/
	SYSTEM_START_WRITE_FLASH,/*系统开始写入外部FLASH*/
	SYSTEM_WAITING,				/*系统写完FLASH后不能重复写，等待中断改变系统状态*/
	SYSTEM_END,						/*系统最后的通信状态*/
}Switch;

typedef struct OTA_Information{
	uint8_t update_flag;
	uint8_t version[11];
}OTA_Information;

typedef struct OTA_Information OTA_Information;
extern OTA_Information OTA_InfoSet;
extern volatile uint32_t file_size;


uint8_t data_base_check(uint32_t* p_packet_size);
void switch_proc(void);


#endif
