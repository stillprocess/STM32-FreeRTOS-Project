/****************************************************************
*名    称:基于stm32f4的电容指纹模块FPM383
*作    者:温子祺
*创建日期:2023/06/13
*知 识 点:
	1.电容指纹模块手册的阅读、数据交换
	2.串口编程
	3.按键检测
	4.FreeRTOS
*说  明:	
	1.按键功能
		1)按键1按下，则执行添加指纹工作
		2)按键2按下，则执行刷指纹工作
		3)按键3按下，则执行获取所有用户的总数
		4)按键4按下，则执行删除所有用户
		
	2.电容指纹模块光圈
		1)操作状态时，蓝色
		2)操作确认
			.成功，绿色
			.失败，红色
*****************************************************************/

#ifndef __KEY_H__
#define __KEY_H__

#include "stm32f4xx.h"
#include "FreeRTOS.h"
#include "event_groups.h"

// S1～S4 按下事件位
#define EVENT_GROUP_KEY1_DOWN       (1U << 0)
#define EVENT_GROUP_KEY2_DOWN       (1U << 1)
#define EVENT_GROUP_KEY3_DOWN       (1U << 2)
#define EVENT_GROUP_KEY4_DOWN       (1U << 3)
#define EVENT_GROUP_KEY_ALL         (0x0FU)

// 指纹录入、识别、查询数量、清空事件位
#define EVENT_GROUP_SFM_USER_REG     (1U << 4)
#define EVENT_GROUP_SFM_USER_COMPARE (1U << 5)
#define EVENT_GROUP_SFM_USER_TOTAL   (1U << 6)
#define EVENT_GROUP_SFM_USER_DEL_ALL (1U << 7)
#define EVENT_GROUP_SFM_ALL          (0xF0U)

extern EventGroupHandle_t g_event_group;
extern void key_init(void);




#endif


