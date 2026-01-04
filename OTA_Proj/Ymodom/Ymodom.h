#ifndef __YMODOM__H
#define __YMODOM__H

#include "main.h"

typedef enum {
    RYM_CODE_SOH = 0x01,
    RYM_CODE_STX = 0x02,
    RYM_CODE_EOT = 0x04,
    RYM_CODE_ACK = 0x06,
    RYM_CODE_NAK = 0x15,
    RYM_CODE_CAN = 0x18,
    RYM_CODE_C   = 0x43,
} RYM_CommandCode;

typedef struct OTA_Information{
	uint8_t update_flag;
	uint8_t version[11];
}OTA_Information;

typedef struct OTA_Information OTA_Information;
extern OTA_Information OTA_InfoSet;

void RYM_SendStart(void);
void RYM_SendAck(void);
void RYM_SendNak(void);

void rym_usart_proc(void);
void rym_gd25_write_app_data_to_spi_flah(void);
#define RYM_FRAME_LENGTH 1105 // 增加到最大可能长度
#define RYM_USART UART4
#define RYM_HUSART huart4
#define RYM_LOGHAND huart1
#define RYM_LOG(...)  my_printf(&RYM_LOGHAND,...,##__VA_ARGS__)
extern volatile uint64_t file_size;
void OTA_SetUpdateFlag(OTA_Information* OTA_Info);
void OTA_GetUpdateFlag(OTA_Information* OTA_Info);
uint16_t Cal_CRC16(const uint8_t* data, uint32_t size);
#endif
