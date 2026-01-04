#include "switch.h"
#include "usart.h"
#include "string.h"
#include "stdio.h" // 用于printf
#include "bsp_rym.h"
#include "bootloader.h"
#include "GD25Q40E.h"
#include "mcuflash.h"
static uint32_t address = GD25Q40E_OTA_DATA_START_ADDRESS;//SPI FLASH 写入地址 跳过扇区0 从扇区1开始
volatile uint32_t file_size;
static uint32_t pack_size;/*检查过后合规的一包数据的长度*/
static uint16_t rym_data_len = 0;/* 记录接收到的数据长度 */

static uint8_t  is_prepared;

volatile uint8_t ota_update_flag = 0;
volatile static Switch SysState = SYSTEM_IDLE;
static OTA_Information OTA_InfoStruct;
volatile static uint8_t packet_number = 0;
static const uint8_t rym_data_offset = 3; /* 数据在buffer中的偏移量 */
__attribute__((aligned(4))) static uint8_t rym_rx_buffer[RYM_FRAME_LENGTH] = {0};
static uint8_t fatal_error_count = 0;
#define FATAL_ERROR_MAX_COUNT 5

OTA_Information OTA_InfoSet = 
{
	.update_flag = 0,
	.version = "V1.0"
};

void switch_proc(void)
{
	uint16_t cal_crc,rec_crc;
	switch (SysState)
	{
		/*-----------------------------------------------------------------------------*/
		case SYSTEM_IDLE:/*系统空闲状态每隔5s钟读取一次OTA标志位*/
			GD25Q40E_Read(&hgd1, GD25Q40E_OTA_INFO_START_ADDRESS, 
								(uint8_t*)&OTA_InfoStruct, sizeof(OTA_Information));
			if (OTA_InfoStruct.update_flag == 1) SysState = SYSTEM_PREPAR_UPDATE;
			else SysState = SYSTEM_JUMP_APP;
		break;
		/*-----------------------------------------------------------------------------*/
		case SYSTEM_JUMP_APP:
			Jump_To_App();
		break;
		/*-----------------------------------------------------------------------------*/
		case SYSTEM_PREPAR_UPDATE:/*读取到更新标志位为1，做好更新前的准备工作*/
			if (is_prepared == 0) /*还没准备*/
			{
				/*打印一下信息*/
				my_printf(&huart1, "[INFO]:update_flag:%d\r\nversion:%s\r\n",OTA_InfoStruct.update_flag,
															OTA_InfoStruct.version);
				/*更新前准备工作*/
				memset(rym_rx_buffer,0,RYM_FRAME_LENGTH);
				packet_number = 0; 
				if (HAL_UARTEx_ReceiveToIdle_DMA(&RYM_HUSART, rym_rx_buffer, RYM_FRAME_LENGTH) != HAL_OK)
				{
						my_printf(&huart1,"[ERROR] usart open failed\r\n");
						SysState = SYSTEM_JUMP_APP;
				}
				is_prepared = 1;
			}
			if(packet_number == 0)// 发送 C 请求开始 等待用户响应
			{							
				RYM_SendStart(); 
				HAL_Delay(10);
			}
		break;
		/*-----------------------------------------------------------------------------*/
		case SYSTEM_PROC:/*接收到数据系统处理数据状态*/
			
			if (data_base_check(&pack_size) == 0) return ;
			/* 5. CRC 校验 */
			cal_crc = Cal_CRC16(&rym_rx_buffer[rym_data_offset], pack_size);
			rec_crc = (rym_rx_buffer[pack_size + rym_data_offset] << 8) | \
													(rym_rx_buffer[pack_size + rym_data_offset+1]);
			
			if (cal_crc == rec_crc)
			{
					uint8_t seq = rym_rx_buffer[1];

					/* 情况A: 正常的下一包数据 */
					if (seq == packet_number) 
					{							
							// 只有 Packet > 0 才是有效文件数据
							if(packet_number != 0) 
							{
									file_size += pack_size;
							}
							/*数据一切正常可以写入SPI FLASH*/
							SysState = SYSTEM_START_WRITE_FLASH;
							my_printf(&huart1,"[INFO] Recieved Normal\r\n");
							packet_number++;
					}
					/* 情况B: 发送端没收到ACK，重发了上一包 */
					else if (seq == (uint8_t)(packet_number - 1))
					{
						my_printf(&huart1,"[INFO] Recieved Same Packet\r\n");
						HAL_UARTEx_ReceiveToIdle_DMA(&RYM_HUSART, rym_rx_buffer, RYM_FRAME_LENGTH);
						RYM_SendAck();
						/* 准备接收下一包 */
						return ;
					}
					/* 情况C: 乱序 */
					else
					{
							my_printf(&huart1,"[ERROR] Fatal Errored\r\n");
							// 发生严重错误，请求重发或者停止
							HAL_UARTEx_ReceiveToIdle_DMA(&RYM_HUSART, rym_rx_buffer, RYM_FRAME_LENGTH);
							RYM_SendNak(); 
							if (++fatal_error_count == FATAL_ERROR_MAX_COUNT)
							{
								my_printf(&huart1,"[INFO] SystemReset fatal_error_count:%d\r\n",fatal_error_count);
							}
							return; 
					}
			}
			else
			{
					my_printf(&huart1,"[ERROR] CRC Errored\r\n");
					HAL_UARTEx_ReceiveToIdle_DMA(&RYM_HUSART, rym_rx_buffer, RYM_FRAME_LENGTH);
					RYM_SendNak(); // CRC 错误
			}
		break;
		/*-----------------------------------------------------------------------------*/
		case SYSTEM_START_WRITE_FLASH:
			/*第0包不是数据是帧头，数据索引从3开始*/
			if (rym_rx_buffer[1] != 0x00) 
			{
				GD25Q40E_Write_1KB_Block(&hgd1,address,&rym_rx_buffer[rym_data_offset],pack_size);
				address += pack_size;//更新地址
			}
			HAL_UARTEx_ReceiveToIdle_DMA(&RYM_HUSART, rym_rx_buffer, RYM_FRAME_LENGTH);
			RYM_SendAck();
			/* 准备接收下一包 */
			/* 如果是第0包（包含文件名），处理完后可能需要再发一次 'C' 启动数据传输 */
			if (packet_number == 1) RYM_SendStart(); // 刚刚处理完的是 0 号包，序号已加到1
			SysState = SYSTEM_WAITING;
		break;
		case SYSTEM_WAITING:
			
		break;
		/*-----------------------------------------------------------------------------*/
		case SYSTEM_END:/*系统应答状态*/
			RYM_SendNak();HAL_Delay(100); // ACK EOT
			RYM_SendAck();HAL_Delay(100); // 发送 'C' 结束会话 (Ymodem 规范有时需要发送 ACK 后发 C，或者直接结束)
			RYM_SendStart();HAL_Delay(100);RYM_SendAck();
			my_printf(&huart1,"[INFO] file_size:%d\r\n",file_size);
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
				memset(rym_rx_buffer,0,RYM_FRAME_LENGTH);
				// 按实际长度读
				GD25Q40E_Read(&hgd1, i+GD25Q40E_OTA_DATA_START_ADDRESS, rym_rx_buffer, remain_len);
				
				// 按实际长度写
				OTA_APPFlashWrite(APP_START_ADDRESS + i, (uint32_t*)rym_rx_buffer, remain_len);
			}
			/*更新完成重置本地版本和更新标志位*/
			OTA_InfoStruct.update_flag = 0;
			GD25Q40E_Write_1KB_Block(&hgd1,GD25Q40E_OTA_INFO_START_ADDRESS,
														(uint8_t*)&OTA_InfoStruct,sizeof(OTA_Information));
			SysState = SYSTEM_JUMP_APP;/*跳转APP*/
		break;
	};
}

uint8_t data_base_check(uint32_t* p_packet_size)
{
		if (rym_data_len == 1 && rym_rx_buffer[0] == RYM_CODE_EOT)/* 1. 处理 EOT */
			{
					SysState = SYSTEM_END;/*系统最后的通信*/
					return 0;
			}
			/* 2. 检查包头 */
			if (rym_rx_buffer[0] == RYM_CODE_SOH) *p_packet_size = 128;
			else if (rym_rx_buffer[0] == RYM_CODE_STX) *p_packet_size = 1024;
			else 
			{
					RYM_SendNak();
					HAL_UARTEx_ReceiveToIdle_DMA(&RYM_HUSART, rym_rx_buffer, RYM_FRAME_LENGTH);
					return 0;
			}

			/* 3. 长度检查 */
			if (rym_data_len < ((*p_packet_size) + 5))
			{
					 RYM_SendNak();
					 HAL_UARTEx_ReceiveToIdle_DMA(&RYM_HUSART, rym_rx_buffer, RYM_FRAME_LENGTH);
					 return 0;
			}

			/* 4. 序号检查 */
			if (rym_rx_buffer[1] != (uint8_t)(~rym_rx_buffer[2]))
			{
					RYM_SendNak();
					HAL_UARTEx_ReceiveToIdle_DMA(&RYM_HUSART, rym_rx_buffer, RYM_FRAME_LENGTH);
					return 0;
			}
			return 1;
}

/* 串口接收回调 */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if (huart->Instance == RYM_USART)
    {
        if (OTA_InfoStruct.update_flag == 1)
        {
						rym_data_len = Size;
						if (SysState == SYSTEM_WAITING || SysState != SYSTEM_END)
							SysState = SYSTEM_PROC;
        }
    }
}
