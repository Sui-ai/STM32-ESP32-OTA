#ifndef __GD25Q40E__H
#define __GD25Q40E__H

#include "main.h"
#include "string.h"
#include "spi.h"

typedef enum GD25Q40E_StatusTypeDef
{
	GD25Q40E_OK = 0,
	GD25Q40E_ERROR
}GD25Q40E_StatusTypeDef;

struct GD25Q40E_InitStruct{
	GPIO_TypeDef* 			spi_port;
	uint16_t			 			spi_pin;
	SPI_HandleTypeDef		spi_handler;
	uint8_t manufacturer_id;
	uint8_t device_id;
};
typedef struct GD25Q40E_InitStruct GD25Q40E_HandleTypeDef ;


#define WRITE_ENABLE   1
#define WRITE_DISABLE  0

#define GD25Q40E_OTA_DATA_START_ADDRESS 0x1000
#define GD25Q40E_OTA_INFO_START_ADDRESS 0x0000


void GD25Q40E_Read(GD25Q40E_HandleTypeDef * hgd, uint32_t addr, uint8_t* data, uint16_t len);
void GD25Q40E_Write_1KB_Block(GD25Q40E_HandleTypeDef * hgd, uint32_t addr, uint8_t* data,uint16_t len);
extern GD25Q40E_StatusTypeDef GD25Q40E_Init(GD25Q40E_HandleTypeDef * hgd,GPIO_TypeDef* spi_port,
																						uint16_t spi_pin, SPI_HandleTypeDef* hspi);
extern GD25Q40E_HandleTypeDef hgd1;
void GD25Q40E_test(GD25Q40E_HandleTypeDef * hgd);
void GD25Q40E_ChipErase(GD25Q40E_HandleTypeDef * hgd);


#endif
