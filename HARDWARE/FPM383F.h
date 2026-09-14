#ifndef _FPM383F_H_
#define _FPM383F_H_

// 指纹模块 LED 颜色定义
#define FPM_LED_RED      1
#define FPM_LED_GREEN    2
#define FPM_LED_BLUE     3



#include "stm32f4xx.h"
#include "uart.h"

// USART1 调试输出，使用工程已有的串口互斥锁
extern void dgb_printf_safe(const char *format, ...);
#include "delay.h"
#include "servo.h"



// PE15 触摸事件标志；触摸中断置 1，现有 S1～S4 操作不依赖此标志
extern volatile uint32_t g_fpm_touch_event;

// 初始化 FPM383F 指纹模块
// 初始化 USART2、TIM5 和 PE15 触摸输入；PD15 保留给电机
extern void fpm_init(void);


// 通过串口向 FPM383F 发送一帧命令数据
// length：发送数据长度
// FPM383C_Databuffer：待发送的数据数组
extern void fpm_send_data(int length, uint8_t FPM383C_Databuffer[]);

// 让 FPM383F 进入休眠状态
extern void fpm_sleep(void);

// 清空指纹模块中保存的所有指纹
// 返回 0：成功
// 返回 -1：失败
extern int32_t fpm_empty(void);

// 获取当前已经录入的指纹总数
// total：用于保存返回的指纹数量
// 返回 0：成功
// 返回 -1：失败
extern int32_t fpm_id_total(uint16_t *total);

// 控制指纹模块 LED 灯颜色
// color：FPM_LED_RED / GREEN / BLUE
extern uint8_t fpm_ctrl_led(uint8_t color);

// 根据指纹录入错误码，返回对应的错误说明字符串
// error_code：指纹模块返回的错误码
extern const char *fpm_error_code_auto_enroll(uint8_t error_code);

// 自动验证 / 识别指纹
// id：传入待匹配 ID，也用于保存匹配成功后的指纹 ID
// 传入 0xFFFF 时通常表示全库搜索
// 返回 0：识别成功
// 返回 -1：识别失败
extern int32_t fpm_idenify_auto(uint16_t *id);

// 自动录入一个新指纹
// id：要保存的指纹 ID
// 返回 0：录入成功
// 返回 -1：录入失败
extern int32_t fpm_enroll_auto(uint16_t id);

#endif
