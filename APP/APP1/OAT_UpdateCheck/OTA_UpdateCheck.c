#include "OTA_UpdateCheck.h"
#include "GD25Q40E.h"
#include "usart.h"
#define UART4_RX_BUFFER_SIZE 64
uint16_t uart4_rx_index;
uint32_t uart4_rx_tick;
uint8_t uart4_rx_buffer[UART4_RX_BUFFER_SIZE];


typedef struct OTA_Information{
	uint8_t update_flag;
	uint8_t version[11];
}OTA_Information;

OTA_Information OTA_Info = 
{
	.update_flag = 0,
	.version = "V1.0"
};
/*模拟从云上获取的版本信息*/
OTA_Information OTA_SetInfo = 
{
	.update_flag = 1,
	.version = "V2.0"
};


HAL_StatusTypeDef OTA_Send_Local_Version(OTA_Information* OTA_Info) {
    uint8_t send_buf[50]; // 发送缓冲区
    char* version_str = (char*)OTA_Info->version; 
    uint8_t ver_len = strlen(version_str); // 获取版本号长度，例如 4

    // 1. 填入帧头 (AA 55)
    send_buf[0] = 0xAA;
    send_buf[1] = 0x55;

    // 2. 填入指令 (01: 发送本地版本)
    send_buf[2] = 0x01;

    // 3. 填入长度 (仅版本号字符串的长度)
    send_buf[3] = ver_len;

    // 4. 填入版本号数据 (V0.9)
    // memcpy(目标, 源, 长度)
    memcpy(&send_buf[4], version_str, ver_len);

    // 5. 填入帧尾 (0D 0A)
    send_buf[4 + ver_len] = 0x0D;
    send_buf[4 + ver_len + 1] = 0x0A;

    // 6. 计算总包长并发送
    // 总长 = 头(2) + 指令(1) + 长度(1) + 数据(ver_len) + 尾(2)
		
    uint8_t total_len = 2 + 1 + 1 + ver_len + 2;
    HAL_StatusTypeDef status = HAL_UART_Transmit(&huart4, send_buf, total_len, 100);
		return status;
}

HAL_StatusTypeDef OTA_Receive_Cloud_Version(OTA_Information* OTA_Info) {
    uint8_t rx_byte;
    uint8_t header_step = 0;
    uint32_t start_tick = HAL_GetTick();
    if (start_tick - uart4_rx_tick < 500) HAL_Delay(start_tick - uart4_rx_tick);
    // 1. 寻找帧头 (AA 55) 和 指令 (02)
    // 设置一个总超时时间，比如 5s，避免死等
	//协议：帧头(AA 55) + 指令(02) + 长度 + [Flag + VerStr] + 帧尾(0D 0A)
		while (HAL_GetTick() - start_tick < 5000)
		{
			/*超时解析+防止死循环*/
			for (int i = 0; i < uart4_rx_index - 4; i++)
			{
				if (uart4_rx_buffer[i] == 0xAA && uart4_rx_buffer[i+1] == 0x55 && uart4_rx_buffer[i+2] == 0x02)
				{
					uint16_t data_len = uart4_rx_buffer[i+3];
					/*防止溢出*/
					if (i+5+data_len > UART4_RX_BUFFER_SIZE) break;
					if (uart4_rx_buffer[i+4+data_len] == 0x0D && uart4_rx_buffer[i+5+data_len] == 0x0A)
					{
						OTA_Info->update_flag = uart4_rx_buffer[i+4];
						memcpy(&OTA_Info->version,&uart4_rx_buffer[i+5],data_len-1);
						OTA_Info->version[data_len] = '\0';
						//也需要打扫战场
						memset(uart4_rx_buffer, 0, UART4_RX_BUFFER_SIZE);
						uart4_rx_index = 0;
						uart4_rx_tick = 0; // 这个 tick 也可以重置一下
						HAL_UART_AbortReceive(&huart4);
						HAL_UART_Receive_IT(&huart4, &uart4_rx_buffer[0], 1);
						return HAL_OK;
					}
				}
			}
		}
		memset(uart4_rx_buffer,0,sizeof(uart4_rx_buffer));
		uart4_rx_index = uart4_rx_tick = 0;
		HAL_UART_Receive_IT(&huart4,&uart4_rx_buffer[0],1);
    
    return HAL_TIMEOUT;
}



void OTA_UpdateCheck(OTA_Information* OTA_Info)
{
	HAL_StatusTypeDef status;
	if (check_update_flag == 1)
	{
		/*APP跟ESP通信需要换串口不能再用串口1*/
		/*串口1用于接收用户的Yes/No*/
		/*向ESP发送当前版本信息 让ESP用python比较版本大小 然后返回update_flag 还有云上版本*/
		GD25Q40E_Read(&hgd1,GD25Q40E_OTA_INFO_START_ADDRESS,(uint8_t*)OTA_Info,sizeof(OTA_Information));
		my_printf(&huart1, "Local OTA_Info:\r\nupdate_flag:%d\r\nversion:%s\r\n",OTA_Info->update_flag,
														OTA_Info->version);
		//给ESP发送当前版本信息 
		/* 帧格式: 0xAA 0x55(帧头) + 01(发送本地版本信息指令) + len(数据字节数) + 数据 + \r\n(帧尾)  */
		HAL_UART_Receive_IT(&huart4,&uart4_rx_buffer[0],1);
		OTA_Send_Local_Version(OTA_Info);
		//串口打印当前版本信息调试

		status = OTA_Receive_Cloud_Version(OTA_Info);
		my_printf(&huart1,"Cloud Version Receive status:%d\r\n",status);
		if (status == HAL_OK)
		{
			my_printf(&huart1, "Cloud OTA_Info:\r\nupdate_flag:%d\r\nversion:%s\r\n",OTA_Info->update_flag,
														OTA_Info->version);
		}
		/*询问用户要不要更新固件*/
		if (OTA_Info->update_flag == 1)
		{
			uint8_t ans[5] = {0};
			my_printf(&huart1, "Ready to update your flash?\r\nyes/no\r\n");
			while(HAL_UART_Receive(&huart1, ans, 1, HAL_MAX_DELAY) != HAL_OK);
			if (ans[0] == 'y' || ans[0] == 'Y') 
			{
				/*软件重启前先写入标志位*/
				GD25Q40E_Write_1KB_Block(&hgd1,GD25Q40E_OTA_INFO_START_ADDRESS,
																(uint8_t*)OTA_Info,sizeof(OTA_Information));
				HAL_NVIC_SystemReset();/*软件复位重启运行bootloader*/
			}
			else /*用户拒绝更新固件*/
			{
				my_printf(&huart1, "return to app\r\n");
			}
		}
		check_update_flag = 0;
	}
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
	if (huart->Instance == UART4)
	{
		uart4_rx_tick = HAL_GetTick();
		// 2. 索引增加，但必须防止越界！
		if (uart4_rx_index < UART4_RX_BUFFER_SIZE - 1) {
				uart4_rx_index++;
		}
		HAL_UART_Receive_IT(&huart4,&uart4_rx_buffer[uart4_rx_index],1);
	}
}

