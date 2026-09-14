#ifndef _SERVO_H
#define _SERVO_H

#include "stm32f4xx.h"

//初始化PC9对应的TIM3通道4 PWM
void Servo_PWM_Init(void);

//设置舵机角度，范围为0到180度
void Servo_SetAngle(uint16_t angle);

#endif
