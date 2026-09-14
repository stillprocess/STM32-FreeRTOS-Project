#ifndef _DHT11_H
#define _DHT11_H

#include "stm32f4xx.h"
#include "stdbool.h"

#define DHT11_READ_OK              1
#define DHT11_ERROR_ACK            2
#define DHT11_ERROR_CHECKSUM       3

void DHT11_Config(void);

uint8_t DHT11_Read_Temp_Humi(uint8_t *temp, uint8_t *humi);

#endif
