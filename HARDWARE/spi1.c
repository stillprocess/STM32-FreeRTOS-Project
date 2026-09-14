#include "spi1.h"
#include <string.h>


/*   SCK  PB3
     MISO PB4
	 MOSI PB5
*/



void SPI_Config(void)
{

  SPI_InitTypeDef  SPI_InitStructure;

  GPIO_InitTypeDef GPIO_InitStructure;
	
  //  必须清零结构体，防止垃圾值
  memset(&SPI_InitStructure, 0, sizeof(SPI_InitStructure));

  /*!< Enable the SPI clock */
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI1, ENABLE);

  /*!< Enable GPIO clocks */
  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB,ENABLE);
  
  /*!< SPI pins configuration *************************************************/

  /*!< Connect SPI pins to AF5 */  
  GPIO_PinAFConfig(GPIOB, GPIO_PinSource3, GPIO_AF_SPI1);
  GPIO_PinAFConfig(GPIOB, GPIO_PinSource4, GPIO_AF_SPI1);
  GPIO_PinAFConfig(GPIOB, GPIO_PinSource5, GPIO_AF_SPI1);

  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
  GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_NOPULL;
        
  /*!< SPI SCK pin configuration */
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;
  GPIO_Init(GPIOB, &GPIO_InitStructure);

  /*!< SPI MOSI pin configuration */
  GPIO_InitStructure.GPIO_Pin =  GPIO_Pin_5;
  GPIO_Init(GPIOB, &GPIO_InitStructure);

  /*!< SPI MISO pin configuration */
  GPIO_InitStructure.GPIO_Pin =  GPIO_Pin_4;
  GPIO_Init(GPIOB, &GPIO_InitStructure);

   /*!< Configure CS pin in output pushpull mode ********************/
  GPIO_InitStructure.GPIO_Pin =  GPIO_Pin_14;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
  GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
  GPIO_Init(GPIOB, &GPIO_InitStructure);
  
  /*!< Deselect the FLASH: Chip Select high */
  GPIO_SetBits(GPIOB,GPIO_Pin_14);

  /*!< SPI configuration */
  SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex;  //全双工
  SPI_InitStructure.SPI_Mode = SPI_Mode_Master;                       //主模式
  SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;                   //8位数据
  SPI_InitStructure.SPI_CPOL = SPI_CPOL_High;                         //时钟极性 1
  SPI_InitStructure.SPI_CPHA = SPI_CPHA_2Edge;                        //时钟相位 1
  SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;                           //软件控制CS片选
  SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_4;  
  
  SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;					//高位先出
  SPI_InitStructure.SPI_CRCPolynomial = 7;
  SPI_Init(SPI1, &SPI_InitStructure);

  /*!< Enable the sFLASH_SPI  */
  SPI_Cmd(SPI1, ENABLE);

}
