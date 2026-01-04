#include "GD25Q40E.h"
#include "math.h"
GD25Q40E_HandleTypeDef hgd1;
uint8_t test_data[1024] = {0};
uint8_t read_buffer[1024];

static inline void GD25Q40E_CSLow(GD25Q40E_HandleTypeDef * hgd)
{
	HAL_GPIO_WritePin(hgd->spi_port,hgd->spi_pin,GPIO_PIN_RESET);
}

static inline void GD25Q40E_CSHigh(GD25Q40E_HandleTypeDef * hgd)
{
	HAL_GPIO_WritePin(hgd->spi_port,hgd->spi_pin,GPIO_PIN_SET);
}

static void GD25Q40E_ReadID(GD25Q40E_HandleTypeDef * hgd,uint8_t* manufacturer_id, uint8_t* device_id)
{
	uint8_t read_id_command = 0x90;
	uint8_t data_size = 6;
	uint8_t tx_buffer[10] = {0};
	uint8_t rx_buffer[10] = {0};
	tx_buffer[0] = read_id_command;
	GD25Q40E_CSLow(hgd);	//拉低CCS
	HAL_SPI_TransmitReceive(&hgd->spi_handler,&tx_buffer[0],//发送读取ID命令同时接收ID
													&rx_buffer[0],
													data_size,HAL_MAX_DELAY);
	GD25Q40E_CSHigh(hgd);
	*manufacturer_id = rx_buffer[4];
	*device_id = rx_buffer[5];
}

static void GD25Q40E_WriteEnable(GD25Q40E_HandleTypeDef * hgd,uint8_t enable)
{
	uint8_t write_enable_code = enable ? 0x06 : 0x04;
	GD25Q40E_CSLow(hgd);
	HAL_SPI_Transmit(&hgd->spi_handler,&write_enable_code,1,HAL_MAX_DELAY);
	GD25Q40E_CSHigh(hgd);
}


static uint8_t GD25Q40E_ReadStatusRegister(GD25Q40E_HandleTypeDef * hgd)
{
	uint8_t ready[2] = {0x00,0x01};
	uint8_t read_psr_code = 0x05;
	GD25Q40E_CSLow(hgd);
	HAL_SPI_TransmitReceive(&hgd->spi_handler,&read_psr_code,&ready[0],2,HAL_MAX_DELAY);
	GD25Q40E_CSHigh(hgd);
	return ready[1];
}
/* 等待写入/擦除 */
static void GD25Q40E_WaitBusy(GD25Q40E_HandleTypeDef * hgd)
{
	/* 等待扇区擦除完成 */
	/* 读取状态寄存器 */
	uint32_t tick = uwTick;
	uint8_t wait_ready = 0x01;
	/*检查ready的最后一位是否为0*/
	while (((wait_ready = GD25Q40E_ReadStatusRegister(hgd)) & 0x01))
	{
		/* 超时 */
		if (uwTick - tick > 3000)
		{
//			char tx_buffer[30] = {"erase time out\r\n"};
			/* 可选 串口调试信息 */
//			HAL_UART_Transmit(&huart1,(uint8_t*)tx_buffer,sizeof(tx_buffer) - 1,HAL_MAX_DELAY);
			GD25Q40E_CSHigh(hgd);
			return ;
		}
	}
}

static void GD25Q40E_SectorErase(GD25Q40E_HandleTypeDef * hgd,uint32_t addr)
{
	uint8_t command_size = 4;
	uint8_t sector_erase_code = 0x20;
	uint8_t erase_buffer[20] = {0};
	erase_buffer[0] = sector_erase_code;
	/*地址是24位的从高到低*/
	erase_buffer[1] = (uint8_t)(addr >> 8 * 2);
	erase_buffer[2] = (uint8_t)(addr >> 8 * 1);
	erase_buffer[3] = (uint8_t)(addr >> 8 * 0);
	/*发送扇区擦除指令*/
	GD25Q40E_CSLow(hgd);
	HAL_SPI_Transmit(&hgd->spi_handler,&erase_buffer[0],
										command_size,HAL_MAX_DELAY);
	GD25Q40E_CSHigh(hgd);
}
/*
	擦除整个芯片
*/

void GD25Q40E_ChipErase(GD25Q40E_HandleTypeDef * hgd)
{
	/*写入和擦除前都要写使能*/
	GD25Q40E_WriteEnable(hgd,WRITE_ENABLE);
	/* 发送擦除指令 */
	GD25Q40E_CSLow(hgd);
	uint8_t chip_erase_command = 0x60;
	HAL_SPI_Transmit(&hgd->spi_handler,&chip_erase_command,1,HAL_MAX_DELAY);
	GD25Q40E_CSHigh(hgd);
}

/*
	@param hgd flash 句柄
	@param spi_port 使用的SPI GPIOx
  	@param spi_pin GPIO_PIN_X
  	@param hspi  spi句柄
*/

GD25Q40E_StatusTypeDef GD25Q40E_Init(GD25Q40E_HandleTypeDef * hgd,GPIO_TypeDef* spi_port,
																			uint16_t spi_pin, SPI_HandleTypeDef* hspi)
{
	hgd->spi_port = spi_port;
	hgd->spi_pin  = spi_pin;
	hgd->spi_handler = *hspi;
	GD25Q40E_ReadID(hgd,&hgd->manufacturer_id,
									&hgd->device_id);
	if (hgd->manufacturer_id != 0xC8)
		return GD25Q40E_ERROR;
	
	return GD25Q40E_OK;
}
/* 先擦除再写入 */

/*
	@param hgd flash 句柄
	@param addr flash扇区地址0xm0000  m:0~7 每个扇区4kb 
							一个扇区16页 页的地址为：扇区地址+0xm00 m:(0~F)
							超出页面字节的数据会自动丢弃
  	@param data 要写入数据的起始地址
  	@param len  写入数据的长度
*/

// 这一层只管写，不管擦除。自动处理 256 字节页边界。
void GD25Q40E_Write_NoErase(GD25Q40E_HandleTypeDef * hgd, uint32_t addr, uint8_t* data, uint16_t len)
{
    uint16_t pager;
    while (len > 0)
    {
        pager = 256 - (addr % 256); // 计算当前页剩余空间
        if (len < pager) pager = len;
				
        GD25Q40E_WriteEnable(hgd, WRITE_ENABLE);
        
        uint8_t cmd[4];
        cmd[0] = 0x02; // Page Program
        cmd[1] = (uint8_t)(addr >> 16);
        cmd[2] = (uint8_t)(addr >> 8);
        cmd[3] = (uint8_t)(addr);
				
        GD25Q40E_CSLow(hgd);
        HAL_SPI_Transmit(&hgd->spi_handler, cmd, 4, HAL_MAX_DELAY);
        HAL_SPI_Transmit(&hgd->spi_handler, data, pager, HAL_MAX_DELAY);
        GD25Q40E_CSHigh(hgd);

        GD25Q40E_WaitBusy(hgd); // 等待写入结束

        data += pager;
        addr += pager;
        len  -= pager;
    }
}
/*
 * @brief  写入 1KB 数据。如果是扇区首地址，会自动擦除该扇区。
 * @param  addr: 写入地址，建议 1024 对齐 (0, 1024, 2048...)
 * @param  data: 1024 字节的数据指针
 */
void GD25Q40E_Write_1KB_Block(GD25Q40E_HandleTypeDef * hgd, uint32_t addr, uint8_t* data,uint16_t len)
{
    // 1. 检查是否是扇区 (4KB) 的起始地址
    // 0x0000, 0x1000(4096), 0x2000(8192) ...
    if ((addr % 4096) == 0)
    {
        // 是扇区开头：执行“擦除一次”
        GD25Q40E_WriteEnable(hgd, WRITE_ENABLE);
        GD25Q40E_SectorErase(hgd, addr); // 擦除当前 4KB 扇区
        GD25Q40E_WaitBusy(hgd);          // 必须等待擦除完成
    }
    
    // 2. 无论是否擦除，都执行“写入”
    // 因为 len=1024，Write_NoErase 内部会自动循环 4 次 (每次 256 字节)
    GD25Q40E_Write_NoErase(hgd, addr, data, len);
}

/*
 * @brief  读取任意长度数据
 * @param  len: 这里改为 uint16_t，否则无法读取 1024 字节
 */
void GD25Q40E_Read(GD25Q40E_HandleTypeDef * hgd, uint32_t addr, uint8_t* data, uint16_t len)
{
    uint8_t cmd[4];
    cmd[0] = 0x03; // Read Data 指令
    cmd[1] = (uint8_t)(addr >> 16);
    cmd[2] = (uint8_t)(addr >> 8);
    cmd[3] = (uint8_t)(addr);

    GD25Q40E_CSLow(hgd);

    // 1. 先发送 4 字节指令
    if (HAL_SPI_Transmit(&hgd->spi_handler, cmd, 4, HAL_MAX_DELAY) == HAL_OK)
    {
        // 2. 直接接收 len 长度的数据到用户 buffer
        // 这里不需要中间数组，也不需要 memcpy，效率更高且支持大长度读取
        HAL_SPI_Receive(&hgd->spi_handler, data, len, HAL_MAX_DELAY);
    }

    GD25Q40E_CSHigh(hgd);
}

//void GD25Q40E_test(GD25Q40E_HandleTypeDef * hgd)
//{
//	/*起始地址*/
//	uint32_t address = 0x00000000;
//	for (int i = 0; i < 1024; i++)
//	{
//		test_data[i] = i;
//	}
//	/*先擦除扇区*/
//	for (int i = 0; i < 5; i++)
//	{
//		GD25Q40E_Write_1KB_Block(hgd,address,test_data);
//		GD25Q40E_Read(hgd,address,read_buffer,1024);
//		memset(read_buffer,0,1024);
//		address += 1024;
//	}
//	
//}

