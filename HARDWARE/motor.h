#ifndef _MOTOR_H
#define _MOTOR_H

#include "stm32f4xx.h"

/* 电机方向定义 */
#define MOTOR_FORWARD     0
#define MOTOR_REVERSE     1
#define MOTOR_STOP        2
#define MOTOR_BRAKE       3

void Motor_Init(void);
void PWM_Motor_Init(void);
void PWM_SetCompareMotor(uint16_t CCR);
void Motor_Control(uint8_t direction, uint8_t speed);
void Motor_EmergencyStop(void);
void Motor_SoftControl(uint8_t target_speed, uint16_t step_time_ms, uint8_t step_size);
void Motor_FwdRevTest(uint8_t speed);
void Motor_SpeedTest(void);
void Motor_ComprehensiveTest(void);

#endif
