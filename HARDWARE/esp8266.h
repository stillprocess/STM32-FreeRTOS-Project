#ifndef _ESP8266_H
#define _ESP8266_H

#include "stm32f4xx.h"

void ESP8266_Config(void);
uint8_t ESP8266_TestAT(void);
uint8_t ESP8266_ConnectWiFi(void);
uint8_t ESP8266_ConnectTCP(void);
uint8_t ESP8266_EnterTransparentMode(void);
uint8_t ESP8266_IsDisconnected(void);
void ESP8266_SendBytes(const uint8_t *data, uint16_t length);

#endif
