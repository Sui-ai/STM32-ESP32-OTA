#include "mcuflash.h"
#include "bootloader.h"
#include "usart.h"
/*前4个扇区为16KB，第五个扇区为64KB，之后的扇区为128KB*/
#define FISRT_SECTOR_SIZE  (16*1024)		//1
#define SECOND_SECTOR_SIZE (16*1024)	//2
#define THIRD_SECTOR_SIZE (16*1024)		//3
#define FORTH_SECTOR_SIZE (16*1024)		//4
#define FIFTH_SECTOR_SIZE (64*1024)		//5
#define SIXTH_SECTOR_SIZE (128*1024)	//6

static uint32_t GetSector(uint32_t Address);



void OTA_APPFlashErase(uint32_t app_start_addr, uint32_t app_size)
{
	HAL_FLASH_Unlock();
	uint32_t first_sector_id = GetSector(app_start_addr);
	uint32_t end_sector_id	 = GetSector(app_start_addr+app_size-1);
	uint32_t erase_sector_num = end_sector_id - first_sector_id + 1;
	uint32_t sector_error = 0x00000000;
	FLASH_EraseInitTypeDef EraseStruct;
	EraseStruct.TypeErase = FLASH_TYPEERASE_SECTORS;//擦除方式：扇区擦除
	EraseStruct.Banks		  = FLASH_BANK_1;//扇区所在的Bank
	EraseStruct.Sector		= first_sector_id;//起始扇区编号
	EraseStruct.NbSectors = erase_sector_num;//要擦除扇区的个数
	EraseStruct.VoltageRange = VOLTAGE_RANGE_3;//电压范围
	
	HAL_Delay(1000);
	
	__HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR | 
                           FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);
	my_printf(&huart1, "Start Sector: %d, End Sector: %d, Num: %d\r\n", 
          first_sector_id, end_sector_id, erase_sector_num);
	__disable_irq();
	HAL_StatusTypeDef status = HAL_FLASHEx_Erase(&EraseStruct,&sector_error);
	__enable_irq();
	
	if (status != HAL_OK)
	{
		my_printf(&huart1,"Erase Error at Sector Code: %d\r\n", sector_error);
	}
	else
	{
		my_printf(&huart1, "Start Sector: %d, End Sector: %d, Num: %d\r\n", 
          first_sector_id, end_sector_id, erase_sector_num);
		my_printf(&huart1,"Erase Done!\r\n");
	}
	
	HAL_FLASH_Lock();
}

void OTA_APPFlashWrite(uint32_t app_start_addr, uint32_t* app_data, uint32_t app_data_len)
{
    // 1. 安全检查：如果长度不是4的倍数，就不写，或者向下取整
    if (app_data_len % 4 != 0)
    {
        // 这里的处理看你需求，要么返回错误，要么自己处理剩余字节
        // 为了安全，这里防止死循环，我们把余数去掉
        app_data_len = app_data_len - (app_data_len % 4);
    }

    uint32_t write_addr = app_start_addr;
    uint32_t end_addr   = app_start_addr + app_data_len;

    // 2. 移到循环外面：只解锁一次，只关一次中断
    HAL_FLASH_Unlock();

		__HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR | 
                           FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);
    // 3. 使用地址比较作为循环条件，比减法更安全
    while (write_addr < end_addr)
    {
        // 写入数据
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, write_addr, *app_data) == HAL_OK)
        {
            write_addr += 4; // 地址后移4字节
            app_data++;      // 指针后移1个uint32 (也就是4字节)
        }
        else
        {
						// 如果写入出错，必须跳出，否则会死在这里
            my_printf(&huart1,"[ERROR] Flash Program Error\r\n");
            // 可以加个打印或者错误标志
            break; 
        }
    }
    // 4. 移到循环外面：恢复中断，上锁

    HAL_FLASH_Lock();
}

//void Flash_test(void)
//{
//OTA_APPFlashWrite(APP_START_ADDRESS,&test_flag,sizeof(test_flag));
//my_printf(&huart1,"before erase is %x\r\n",*(__IO uint32_t*)(APP_START_ADDRESS));
//OTA_APPFlashErase(APP_START_ADDRESS,1024);
//my_printf(&huart1,"after erase is %x\r\n",*(__IO uint32_t*)APP_START_ADDRESS);
//}

// 获取某个地址所在的扇区编号
static uint32_t GetSector(uint32_t Address)
{
    uint32_t sector = 0;

    if((Address < 0x08004000) && (Address >= 0x08000000))
    {
        sector = FLASH_SECTOR_0;
    }
    else if((Address < 0x08008000) && (Address >= 0x08004000))
    {
        sector = FLASH_SECTOR_1;
    }
    else if((Address < 0x0800C000) && (Address >= 0x08008000))
    {
        sector = FLASH_SECTOR_2;
    }
    else if((Address < 0x08010000) && (Address >= 0x0800C000))
    {
        sector = FLASH_SECTOR_3;
    }
    else if((Address < 0x08020000) && (Address >= 0x08010000))
    {
        sector = FLASH_SECTOR_4; // 64KB
    }
    else if((Address < 0x08040000) && (Address >= 0x08020000))
    {
        sector = FLASH_SECTOR_5; // 128KB 
    }
    // ... 如果是 1MB FLASH，后面还有 Sector 6, 7...
    // 根据你的 512KB Flash 大小，继续补充：
    else if((Address < 0x08060000) && (Address >= 0x08040000))
    {
        sector = FLASH_SECTOR_6;
    }
    else if((Address < 0x08080000) && (Address >= 0x08060000))
    {
        sector = FLASH_SECTOR_7;
    }
    else
    {
        // 这里的处理根据实际情况，通常返回一个错误值或者最大的扇区
        sector = FLASH_SECTOR_7; 
    }
    return sector;
}

