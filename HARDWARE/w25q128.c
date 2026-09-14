#include "w25q128.h"
#include "spi1.h"
#include "uart.h"

//片选引脚
#define  W25Q128_CS(n)  (n) ? GPIO_SetBits(GPIOB,GPIO_Pin_14) : GPIO_ResetBits(GPIOB,GPIO_Pin_14) 

void W25Q128_Config()
{
	
     SPI_Config();
	
}
	
uint8_t W25Q128_SendByte(uint8_t byte)
{
  /*!< Loop while DR register in not emplty */
  while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_TXE) == RESET);

  /*!< Send byte through the SPI1 peripheral */
  SPI_I2S_SendData(SPI1, byte);

  /*!< Wait to receive a byte */
  while (SPI_I2S_GetFlagStatus(SPI1,SPI_I2S_FLAG_RXNE) == RESET);

  /*!< Return the byte read from the SPI bus */
  return SPI_I2S_ReceiveData(SPI1);
}

//读取设备ID   0x17
uint8_t W25Q128_ReadDeviceID(void)
{
	 USART1_SendString("start\n");
	//1.把片选引脚拉低
	W25Q128_CS(0);
	
	//2.MCU发送指令  0x90
	W25Q128_SendByte(0x90);
	
	//3.MCU发送地址
	W25Q128_SendByte(0x00);
	W25Q128_SendByte(0x00);
	W25Q128_SendByte(0x00);

	//4.MCU提供脉冲，得到厂商ID 
	W25Q128_SendByte(0xFF);
	
	//5.MCU提供脉冲，得到设备ID
	uint8_t device_id = W25Q128_SendByte(0xFF);
	
	//6.把片选引脚拉高
	W25Q128_CS(1);
	
	return device_id;
}
	
	
	
	
