#include "bootloader.h"
#include "usart.h"
#include "switch.h"
// 定义函数指针类型
typedef void (*pFunction)(void);
/*---------- ****** --------*/  //内存结束地址0x20000000+0x30000
/*栈*/

/*堆*/
/*变量*/
/*---------- ****** --------*/ //内存其实地址0x20000000
void Jump_To_App(void)
{
    uint32_t JumpAddress;
    pFunction JumpToApplication;

    // 1. 检查栈顶地址是否合法 (0x20000000 是 SRAM 起始地址)
    // 这一步是防止 APP 区域是空的或者是乱码，导致跳过去死机
		my_printf(&huart1, "[INFO] Check Addr: 0x%08X, Data: 0x%08X\r\n", 
							APP_START_ADDRESS, *(__IO uint32_t*)APP_START_ADDRESS);
		my_printf(&huart1, "File Size: %d Bytes\r\n", file_size);
		HAL_Delay(2000);
    if (((*(__IO uint32_t*)APP_START_ADDRESS) & 0xFFF00000 ) == 0x20000000)
    {
        my_printf(&huart1,"[INFO] Jump to Application...\r\n");
				
        // 2. 获取 APP 复位中断向量地址
        JumpAddress = *(__IO uint32_t*) (APP_START_ADDRESS + 4);
        JumpToApplication = (pFunction) JumpAddress;
				
        // 3. 初始化 APP 的堆栈指针 (MSP)
        __set_MSP(*(__IO uint32_t*) APP_START_ADDRESS);

        // 4. 关闭所有外设中断 (重要！否则跳过去中断会乱飞)
        // 也可以简单粗暴用 __disable_irq(); 但建议逐个 deinit
        HAL_RCC_DeInit(); 
        HAL_DeInit();
        
        // 5. 跳转！
				
        JumpToApplication();
    }
    else
    {
        my_printf(&huart1,"[ERROR] No APP found or Stack Error!\r\n");
    }
}









