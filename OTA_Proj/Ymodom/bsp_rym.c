#include "bsp_rym.h"
#include "usart.h"
void RYM_SendStart(void)
{
    uint8_t command = 'C';
    HAL_UART_Transmit(&RYM_HUSART, &command, 1, HAL_MAX_DELAY);
}
/*Ymodem通信正确响应函数*/
void RYM_SendAck(void)
{
    uint8_t command = RYM_CODE_ACK;
    HAL_UART_Transmit(&RYM_HUSART, &command, 1, HAL_MAX_DELAY);
}
/*Ymodem通信错误响应函数*/
void RYM_SendNak(void)
{
    uint8_t command = RYM_CODE_NAK;
    HAL_UART_Transmit(&RYM_HUSART, &command, 1, HAL_MAX_DELAY);
}

/* CRC16 计算函数 (XMODEM) */
uint16_t Cal_CRC16(const uint8_t* data, uint32_t size)
{
    uint16_t crc = 0;
    const uint16_t polynomial = 0x1021;
    for (uint32_t i = 0; i < size; ++i)
    {
        crc ^= ((uint16_t)data[i] << 8);
        for (uint8_t j = 0; j < 8; ++j)
        {
            if (crc & 0x8000)
                crc = (crc << 1) ^ polynomial;
            else
                crc <<= 1;
        }
    }
    return crc;
}






