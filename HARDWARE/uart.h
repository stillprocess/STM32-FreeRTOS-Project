#ifndef _UART_H
#define _UART_H

#include "stm32f4xx.h"

extern volatile uint8_t u3_recvbuf[512];
extern volatile uint32_t u3_recvcnt;

void USART1_Config(uint32_t baud);
void USART2_Config(uint32_t baud); // 指纹模块串口，PA2/PA3
void USART3_Config(uint32_t baud);

//利用串口1发送一个字符串
void  USART1_SendString(const char *str);

//利用串口发送一个字符串
void  USART3_SendString(const char *str);
void USART3_ClearRxBuffer(void);
void USART3_DiscardRxBytes(uint32_t length);

#endif
