#include "Ymodom.h"
#include "usart.h"
#include "string.h"
#include "stdio.h" // 用于printf
#include "bootloader.h"
#include "GD25Q40E.h"
#include "mcuflash.h"
/* 
 * YMODEM 帧定义:
 * SOH(1) + BLK(1) + ~BLK(1) + DATA(128) + CRCH(1) + CRCL(1) = 133 bytes
 * STX(1) + BLK(1) + ~BLK(1) + DATA(1024)+ CRCH(1) + CRCL(1) = 1029 bytes
 */

volatile uint64_t file_size;
__attribute__((aligned(4))) uint8_t app_data[1029]; 
/*程序状态变量*/
volatile uint8_t update_flash_flag = 0;
volatile uint8_t rym_usart_dma_data_recevied_flag = 0;
volatile uint8_t rym_gd25_write_start_flag = 0;
volatile uint8_t rym_transfer_end_flag = 0; // 新增：传输结束标志
uint8_t rym_gd25_write_end_flag = 0;        // 初始设为1，允许第一次进入
/*数据缓存区*/
__attribute__((aligned(4))) uint8_t rym_rx_buffer[RYM_FRAME_LENGTH] = {0};

/* 记录当前包序号，用于防错 */
static uint8_t packet_number = 0;
/* 记录接收到的有效数据长度 */
uint16_t rym_data_len = 0;
/* 数据在buffer中的偏移量 */
uint8_t rym_data_offset = 3; 

/*函数声明*/
static void reset_flags(void);
static uint16_t Cal_CRC16(const uint8_t* data, uint32_t size);
static void RYM_SendStart(void);
static void RYM_SendAck(void);
static void RYM_SendNak(void);



OTA_Information OTA_Info;
OTA_Information OTA_InfoSet = 
{
	.update_flag = 0,
	.version     = "V1.0"
};
void OTA_GetUpdateFlag(OTA_Information* OTA_Info)
{
	GD25Q40E_Read(&hgd1, GD25Q40E_OTA_INFO_START_ADDRESS, 
								(uint8_t*)OTA_Info, sizeof(OTA_Information));
}
void OTA_SetUpdateFlag(OTA_Information* OTA_Info)
{
	GD25Q40E_Write_1KB_Block(&hgd1,GD25Q40E_OTA_INFO_START_ADDRESS,(uint8_t*)OTA_Info,sizeof(OTA_Information));
}
/* 串口接收回调 */
__weak void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if (huart->Instance == RYM_USART)
    {
        if (update_flash_flag)
        {
            /* 1. 处理 EOT */
            if (Size == 1 && rym_rx_buffer[0] == RYM_CODE_EOT)
            {
                rym_transfer_end_flag = 1;
                rym_usart_dma_data_recevied_flag = 1;
                return;
            }

            /* 2. 检查包头 */
            uint16_t packet_size = 0;
            if (rym_rx_buffer[0] == RYM_CODE_SOH) packet_size = 128;
            else if (rym_rx_buffer[0] == RYM_CODE_STX) packet_size = 1024;
            else 
            {
                RYM_SendNak();
                HAL_UARTEx_ReceiveToIdle_DMA(&RYM_HUSART, rym_rx_buffer, RYM_FRAME_LENGTH);
                return;
            }

            /* 3. 长度检查 */
            if (Size < (packet_size + 5))
            {
                 RYM_SendNak();
                 HAL_UARTEx_ReceiveToIdle_DMA(&RYM_HUSART, rym_rx_buffer, RYM_FRAME_LENGTH);
                 return;
            }

            /* 4. 序号检查 */
            if (rym_rx_buffer[1] != (uint8_t)(~rym_rx_buffer[2]))
            {
                RYM_SendNak();
                HAL_UARTEx_ReceiveToIdle_DMA(&RYM_HUSART, rym_rx_buffer, RYM_FRAME_LENGTH);
                return;
            }
            
            /* 5. CRC 校验 */
            uint16_t cal_crc = Cal_CRC16(&rym_rx_buffer[3], packet_size);
            uint16_t rec_crc = (rym_rx_buffer[packet_size + 3] << 8) | rym_rx_buffer[packet_size + 4];
						
            if (cal_crc == rec_crc)
            {
                uint8_t seq = rym_rx_buffer[1];

                /* 情况A: 正常的下一包数据 */
                if (seq == packet_number) 
                {
                    rym_data_len = packet_size;
                    
                    // 只有 Packet > 0 才是有效文件数据
                    if(packet_number != 0) 
                    {
                        file_size += rym_data_len;
                    }

                    rym_gd25_write_start_flag = 1;      // 允许写入Flash
                    rym_usart_dma_data_recevied_flag = 1; // 允许主循环处理
                    
                    packet_number++; // 序号递增
                }
                /* 情况B: 发送端没收到ACK，重发了上一包 */
                else if (seq == (uint8_t)(packet_number - 1))
                {
                    // 数据是重复的，丢弃数据，但需要触发主循环发ACK
                    rym_gd25_write_start_flag = 0; // 禁止写入Flash！
                    rym_usart_dma_data_recevied_flag = 1; // 允许主循环处理
                }
                /* 情况C: 乱序 */
                else
                {
                    // 发生严重错误，请求重发或者停止
                    RYM_SendNak(); 
                    // 此时不应该清零 file_size，而是尝试恢复，或者直接由上位机决定超时
                    HAL_UARTEx_ReceiveToIdle_DMA(&RYM_HUSART, rym_rx_buffer, RYM_FRAME_LENGTH);
                    return; 
                }
            }
            else
            {
                RYM_SendNak(); // CRC 错误
                HAL_UARTEx_ReceiveToIdle_DMA(&RYM_HUSART, rym_rx_buffer, RYM_FRAME_LENGTH);
            }
        }
    }
}
/*接收用户响应APP bin文件和跳转APP*/
void rym_usart_proc(void)
{
    /* 初始状态检查 */
    if (!update_flash_flag)
    {
        uint8_t ans[5] = {0};
				OTA_GetUpdateFlag(&OTA_Info);
        my_printf(&huart1, "OTA_Info:\r\nupdate_flag:%d\r\nversion:%s\r\n",OTA_Info.update_flag,
														OTA_Info.version);
				update_flash_flag = OTA_Info.update_flag;
        if (update_flash_flag != 1)
				{
					Jump_To_App();
				}
				else
				{
					OTA_Info.update_flag = 0;
					GD25Q40E_Write_1KB_Block(&hgd1,GD25Q40E_OTA_INFO_START_ADDRESS,
																(uint8_t*)&OTA_Info,sizeof(OTA_Information));
            // 协议开始，序号归零（0号包是文件名）
            rym_transfer_end_flag = 0;
						reset_flags();
						update_flash_flag = 1;
            memset(rym_rx_buffer, 0, RYM_FRAME_LENGTH);
            
						packet_number = 0; 
						// 第一次启动接收
						if (HAL_UARTEx_ReceiveToIdle_DMA(&RYM_HUSART, rym_rx_buffer, RYM_FRAME_LENGTH) != HAL_OK)
						{
								// 如果启动失败，尝试复位一下串口（可选）
								HAL_UART_AbortReceive(&RYM_HUSART);
								HAL_UARTEx_ReceiveToIdle_DMA(&RYM_HUSART, rym_rx_buffer, RYM_FRAME_LENGTH);
						}
						while (packet_number == 0)// 发送 C 请求开始 等待用户响应
						{							
							
							 // 双重保险：检查串口接收状态，如果状态不是 BUSY (比如变成了 READY 或 ERROR)，说明接收断了
							if (RYM_HUSART.RxState != HAL_UART_STATE_BUSY_RX)
							{
									// 接收断了，重启它！
									HAL_UARTEx_ReceiveToIdle_DMA(&RYM_HUSART, rym_rx_buffer, RYM_FRAME_LENGTH);
							}
							RYM_SendStart(); 
							HAL_Delay(1000);
						}
				}

    }

    /* 协议运行状态 */
    if (update_flash_flag)
    {
			/* 检测传输是否收到 EOT 结束 */
			if (rym_transfer_end_flag)
			{
					RYM_SendNak();HAL_Delay(100); // ACK EOT
					RYM_SendAck();HAL_Delay(100); // 发送 'C' 结束会话 (Ymodem 规范有时需要发送 ACK 后发 C，或者直接结束)
					RYM_SendStart();HAL_Delay(100);RYM_SendAck();
					reset_flags();
					
					/*擦除FLASH*/
					OTA_APPFlashErase(APP_START_ADDRESS,file_size);
					/*从SPI FLASH中读取数据 并写入 单片机FLASH*/
					for (uint32_t i = 0; i < file_size; i += 1024)
					{
						uint32_t remain_len;
						// 计算剩余长度：如果剩余大于1024，就取1024，否则取剩余值
						if ((file_size - i) >= 1024)
						{
								remain_len = 1024;
						}
						else
						{
								remain_len = file_size - i;
						}
						
						// 按实际长度读
						GD25Q40E_Read(&hgd1, i+GD25Q40E_OTA_DATA_START_ADDRESS, app_data, remain_len);
						
						// 按实际长度写
						OTA_APPFlashWrite(APP_START_ADDRESS + i, (uint32_t*)app_data, remain_len);
					}
					/*更新完成重置本地版本和更新标志位*/
					OTA_Info.update_flag = 0;
					GD25Q40E_Write_1KB_Block(&hgd1,GD25Q40E_OTA_INFO_START_ADDRESS,
																(uint8_t*)&OTA_Info,sizeof(OTA_Information));
					Jump_To_App();/*跳转APP*/
					return;
			}

        /* 正常数据包处理逻辑：接收成功且上一包写入完成 */
        if (rym_usart_dma_data_recevied_flag)
        {
					my_printf(&huart1,"rym_usart_dma_data_recevied_flag :%d",rym_usart_dma_data_recevied_flag);
					if (rym_gd25_write_end_flag == 1)
					{
							rym_gd25_write_end_flag = 0; // 清除写入结束标志，等待W25Q写入函数置位
							rym_usart_dma_data_recevied_flag = 0;  // 清除接收标志
							
							/* 
							 * 1. 先发送 ACK 告诉电脑“这包我收到了，发下一包吧”
							 * 2. 在第0包（文件名）之后，需要再次发'C'
							 */
							RYM_SendAck();
							/* 准备接收下一包 */
							memset(rym_rx_buffer, 0, RYM_FRAME_LENGTH);
							HAL_UARTEx_ReceiveToIdle_DMA(&RYM_HUSART, rym_rx_buffer, RYM_FRAME_LENGTH);
							/* 如果是第0包（包含文件名），处理完后可能需要再发一次 'C' 启动数据传输 */
							if (packet_number == 1) RYM_SendStart(); // 刚刚处理完的是 0 号包，序号已加到1
					}
					else if (rym_gd25_write_start_flag == 1)
					{
						/*等待写入完成*/
					}
					else
					{
						// 如果是重发包 (write_start_flag == 0)，直接ACK，不需要等待Flash写入
						rym_usart_dma_data_recevied_flag = 0;
						RYM_SendAck();
						/* 准备接收下一包 */
						memset(rym_rx_buffer, 0, RYM_FRAME_LENGTH);
						HAL_UARTEx_ReceiveToIdle_DMA(&RYM_HUSART, rym_rx_buffer, RYM_FRAME_LENGTH);
						/* 如果是第0包（包含文件名），处理完后可能需要再发一次 'C' 启动数据传输 */
						if (packet_number == 1) RYM_SendStart(); // 刚刚处理完的是 0 号包，序号已加到1
					}
        }
    }
}
/*将接收到的数据包写入SPI FLASH*/
void rym_gd25_write_app_data_to_spi_flah(void) 
{
	static uint32_t address = GD25Q40E_OTA_DATA_START_ADDRESS;//SPI FLASH 写入地址 跳过扇区0 从扇区1开始
	if (rym_gd25_write_start_flag)
	{
			/* 
			 * 解析数据:
			 * 如果是 Packet 0 (rym_rx_buffer[1] == 0x00): 包含文件名和文件大小，不需要写入Flash数据区。
			 * 如果是 Packet > 0: 才是真正的固件数据。
			 */
			
			if (rym_rx_buffer[1] == 0x00) 
			{
				/* 可以在这里解析文件大小，擦除对应的Flash扇区 */
				
				address = GD25Q40E_OTA_DATA_START_ADDRESS;
				my_printf(&huart1,"pack zero\r\n");
//				packet_number = 1;
			}
			else
			{
					/* 
					 * 实际写入 Flash 操作 
					 * 数据起始位置: &rym_rx_buffer[3]
					 * 数据长度: rym_data_len (128 或 1024)
					*/
				GD25Q40E_Write_1KB_Block(&hgd1,address,&rym_rx_buffer[3],rym_data_len);
				address += rym_data_len;//更新地址
			}
			rym_gd25_write_start_flag = 0;
			rym_gd25_write_end_flag = 1; // 通知主循环写入完成，可以ACK并接收下一包
			my_printf(&huart1,"pack num:%d write end\r\n",packet_number);
	}
}

/*-------------------------------------------------辅助函数实现 ---------------------------------------------------*/
static void reset_flags(void)/*清除标志位*/
{
	update_flash_flag = 0;
	rym_transfer_end_flag = 0;
	rym_usart_dma_data_recevied_flag = 0;
	rym_gd25_write_start_flag = 0;
	rym_gd25_write_end_flag = 0;
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
/*Ymodem通信开始函数*/
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

/* 放在 main.c 或 usart.c 中 */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    // 只处理 Ymodem 用的串口
    if (huart->Instance == RYM_USART)
    {
        /* 
         * 发生错误（如过载、噪声、帧错误）时，HAL库会关闭接收。
         * 我们需要在这里清除错误标志并重新启动接收！
         */
         
        // 读取 SR 和 DR 寄存器可以清除部分错误标志（适用于F1/F4等，通用做法）
        volatile uint32_t tmp_sr = huart->Instance->SR; // 对于 L4/G0 系列可能是 ISR
        volatile uint32_t tmp_dr = huart->Instance->DR; // 对于 L4/G0 系列可能是 RDR
        (void)tmp_sr;
        (void)tmp_dr;

        // 重新启动 DMA 接收
        HAL_UARTEx_ReceiveToIdle_DMA(&RYM_HUSART, rym_rx_buffer, RYM_FRAME_LENGTH);
    }
}