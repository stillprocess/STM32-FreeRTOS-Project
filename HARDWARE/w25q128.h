#ifndef _W25Q128_H
#define _W25Q128_H

#include "stm32f4xx.h"
#include "misc.h"

void W25Q128_Config(void);
uint8_t W25Q128_SendByte(uint8_t byte);
//∂¡»°…Ë±∏ID   0x17
uint8_t W25Q128_ReadDeviceID(void);
#endif
