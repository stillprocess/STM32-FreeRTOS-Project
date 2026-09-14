#ifndef _OLED_H
#define _OLED_H

#include "stm32f4xx.h"

//OLED命令和数据标志
#define OLED_CMD   0
#define OLED_DATA  1

//初始化OLED
void OLED_Init(void);

//清空OLED屏幕
void OLED_Clear(void);

//设置OLED显示位置
void OLED_SetPosition(uint8_t x, uint8_t page);

//显示一个ASCII字符
void OLED_ShowChar(uint8_t x, uint8_t page, uint8_t chr, uint8_t size);

//显示ASCII字符串
void OLED_ShowString(uint8_t x, uint8_t page, const uint8_t *str, uint8_t size);

#endif
