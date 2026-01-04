#ifndef __BSP_RYM__H
#define __BSP_RYM__H

#include "main.h"

#define RYM_FRAME_LENGTH 1105 // 增加到最大可能长度
#define RYM_USART UART4
#define RYM_HUSART huart4
#define RYM_LOGHAND huart1
#define RYM_LOG(...)  my_printf(&RYM_LOGHAND,...,##__VA_ARGS__)

typedef enum {
    RYM_CODE_SOH = 0x01,
    RYM_CODE_STX = 0x02,
    RYM_CODE_EOT = 0x04,
    RYM_CODE_ACK = 0x06,
    RYM_CODE_NAK = 0x15,
    RYM_CODE_CAN = 0x18,
    RYM_CODE_C   = 0x43,
} RYM_CommandCode;

void RYM_SendStart(void);
void RYM_SendAck(void);
void RYM_SendNak(void);
uint16_t Cal_CRC16(const uint8_t* data, uint32_t size);








#endif
