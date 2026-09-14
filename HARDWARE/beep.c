#include "beep.h"

void Beep_Config(void)
{
	


	//1.定义GPIO外设的结构体变量
	GPIO_InitTypeDef  GPIO_InitStructure;
	
	//2.打开GPIOF端口的时钟
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOF, ENABLE);

	
	//3.配置PF引脚为输出模式
	
	GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_OUT;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_NOPULL;
	
	//4.对GPIOF端口进行初始化
	GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_8;
	GPIO_Init(GPIOF, &GPIO_InitStructure);
	

	//5.设置默认为高电平
	GPIO_ResetBits(GPIOF,GPIO_Pin_8);

}

void beep_ON(void)
{
	
	GPIO_SetBits(GPIOF,GPIO_Pin_8);
	
}

void beep_OFF(void)
{
	
	GPIO_ResetBits(GPIOF,GPIO_Pin_8);
	
}

