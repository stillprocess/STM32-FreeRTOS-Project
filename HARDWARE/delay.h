#ifndef  _DELAY_H
#define  _DELAY_H

#include "stm32f4xx.h"  //必须包含

//延时微秒 注意：Systick是24bit的递减计数器  约等于798915us,所以参数不可以超过这个值
void delay_us(uint32_t nus);
//延时毫秒 注意：Systick是24bit的递减计数器  约等于798ms,所以参数不可以超过这个值
void delay_ms(uint32_t nms);




#endif
