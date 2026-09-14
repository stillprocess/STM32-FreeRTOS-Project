#include "delay.h"


/**
   *********************************************************************************
   * @file    delay.c 
   * @author  
   * @version 
   * @date    2024/07/22
   * @brief   Systick定时器属于Cortex-M4内核中的一款定时器，具有2个时钟源，内部时钟
	            (168MHZ)和参考时钟(168MHZ/8=21MHZ), 但是FreeRTOS使用Systick定时器进行
							时基生成是使用内核时钟(168MHZ)
							
							由于时钟源是168MHZ，所以Systick计数周期是1/168us，换句话说，1us可以计
							数168次
							
							之前裸机开发时是直接对Systick定时器的寄存器进行控制，Systick定时器的
							寄存器的数量有4个，其中3个必须使用(CTRL、LOAD、VAL)
							
							注意：MCU搭载了FreeRTOS之后，Systick定时器需要提供给FreeRTOS内核来生成
							时基，所以用户就不应该再对Systick定时器的寄存器进行修改，但是用户可以
							读取Systick寄存器的值，完成接口的优化
							
							定时的原理：通过计数的方式达到定时的目的，由于Syctick是倒计时的定时器，
							所以Systick会从一个初值不断递减，当值为0时则表示时间到达，用户可以利用
							计数器的差值来换算时间
							
							注意：这两个延时函数的是非阻塞型的延时函数，所以在调用时是不会导致任务
							阻塞的。
						
   *********************************************************************************
**/

//延时微秒 
void delay_us(uint32_t nus)
{
	  int  sum  = 0;             //作为计数器，对递减次数进行累加
	  int  load = SysTick->LOAD; //把Systick的重载寄存器的值备份 
	  int  told = 0;						 //用于存储读取的Systick的VAL寄存器的第1次的值
	  int  tnew = 0;             //用于存储读取的Systick的VAL寄存器的第2次的值
	  
	  told = SysTick->VAL;       //读取第1次
	
	  while(1)
		{
				tnew = SysTick->VAL ;  //读取第2次
				
				if(told != tnew)
				{
					//此时分为2种情况：told > tnew (一轮之内)  or   told < tnew (一轮之外)
					if(told > tnew)
					{
						sum += told - tnew;
					}						
					else
					{
						sum += load - tnew + told;
					}
					
					told = tnew;
					
					//判断递减次数之和是否达到延时时间对应的计数次数
					if(sum >= nus*168)
					{
						break;
					}
				}
		}
}

//延时毫秒 
void delay_ms(uint32_t nms)
{
	while(nms--)
	{
		delay_us(1000);
	}
}
