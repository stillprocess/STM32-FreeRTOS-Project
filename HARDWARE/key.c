#include "key.h"
#include "task.h"

void key_init(void)
{
	GPIO_InitTypeDef	GPIO_InitStructure;
	EXTI_InitTypeDef	EXTI_InitStructure;	
	NVIC_InitTypeDef	NVIC_InitStructure;
	
	// FreeRTOS 中断优先级使用 4 位抢占优先级
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);

	//使能端口A的硬件时钟，端口A才能工作，说白了就是对端口A上电
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
	
	//使能端口E的硬件时钟，端口E才能工作，说白了就是对端口E上电
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOE, ENABLE);
		
	
	//使能系统配置硬件时钟，说白了就是对系统配置的硬件供电
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_SYSCFG, ENABLE);

	//配置硬件，配置GPIO，端口A，第0个引脚
	GPIO_InitStructure.GPIO_Pin=GPIO_Pin_0;							//第0 个引脚
	GPIO_InitStructure.GPIO_Mode=GPIO_Mode_IN;						//输入模式
	GPIO_InitStructure.GPIO_Speed=GPIO_High_Speed;					//引脚高速工作，收到指令立即工作；缺点：功耗高
	GPIO_InitStructure.GPIO_PuPd=GPIO_PuPd_NOPULL;					//由于外部已有上拉电阻，内部不需要上下拉电阻
	GPIO_Init(GPIOA,&GPIO_InitStructure);		

	//配置硬件，配置GPIO，端口E，第2 3 4个引脚
	GPIO_InitStructure.GPIO_Pin=GPIO_Pin_2|GPIO_Pin_3|GPIO_Pin_4;	//第2 3 4个引脚
	GPIO_InitStructure.GPIO_Mode=GPIO_Mode_IN;						//输入模式
	GPIO_InitStructure.GPIO_Speed=GPIO_High_Speed;					//引脚高速工作
	GPIO_InitStructure.GPIO_PuPd=GPIO_PuPd_NOPULL;					//由于外部已有上拉电阻，内部不需要上下拉电阻。矩阵键盘要使用内部上拉电阻
	GPIO_Init(GPIOE,&GPIO_InitStructure);			
	
	
	//将PA0引脚连接到EXTI0
	SYSCFG_EXTILineConfig(EXTI_PortSourceGPIOA,EXTI_PinSource0);
	
	//将PE2引脚连接到EXTI2
	SYSCFG_EXTILineConfig(EXTI_PortSourceGPIOE,EXTI_PinSource2);
	
	//将PE3引脚连接到EXTI3
	SYSCFG_EXTILineConfig(EXTI_PortSourceGPIOE,EXTI_PinSource3);	

	//将PE4引脚连接到EXTI4
	SYSCFG_EXTILineConfig(EXTI_PortSourceGPIOE,EXTI_PinSource4);	
	
	EXTI_InitStructure.EXTI_Line = EXTI_Line0|EXTI_Line2|EXTI_Line3|EXTI_Line4;	//EXTI0/2/3/4
	EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;							//中断触发
	EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling; 					//下降沿，按键按下，就立即触发中断请求，通知CPU立即处理 
	EXTI_InitStructure.EXTI_LineCmd = ENABLE;									//使能
	EXTI_Init(&EXTI_InitStructure);
	
	NVIC_InitStructure.NVIC_IRQChannel = EXTI0_IRQn;							//外部中断0的中断号
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY;				//抢占优先级 0x05
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;						//响应优先级 0x05
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;								//NVIC使能
	NVIC_Init(&NVIC_InitStructure);	
	
	NVIC_InitStructure.NVIC_IRQChannel = EXTI2_IRQn;							//外部中断2的中断号
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY;				//抢占优先级 0x05
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;						//响应优先级 0x05
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;								//NVIC使能
	NVIC_Init(&NVIC_InitStructure);		
	
	NVIC_InitStructure.NVIC_IRQChannel = EXTI3_IRQn;							//外部中断3的中断号
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY;				//抢占优先级 0x05
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;						//响应优先级 0x05
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;								//NVIC使能
	NVIC_Init(&NVIC_InitStructure);		
	
	
	NVIC_InitStructure.NVIC_IRQChannel = EXTI4_IRQn;							//外部中断3的中断号
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY;				//抢占优先级 0x05
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;						//响应优先级 0x05
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;								//NVIC使能
	NVIC_Init(&NVIC_InitStructure);	
}

// S1～S4 中断只发送事件；消抖和指纹操作由任务完成
void EXTI0_IRQHandler(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if(EXTI_GetITStatus(EXTI_Line0) == SET)
    {
        xEventGroupSetBitsFromISR(g_event_group, EVENT_GROUP_KEY1_DOWN, &xHigherPriorityTaskWoken);
        EXTI_ClearITPendingBit(EXTI_Line0);
    }
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void EXTI2_IRQHandler(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if(EXTI_GetITStatus(EXTI_Line2) == SET)
    {
        xEventGroupSetBitsFromISR(g_event_group, EVENT_GROUP_KEY2_DOWN, &xHigherPriorityTaskWoken);
        EXTI_ClearITPendingBit(EXTI_Line2);
    }
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void EXTI3_IRQHandler(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if(EXTI_GetITStatus(EXTI_Line3) == SET)
    {
        xEventGroupSetBitsFromISR(g_event_group, EVENT_GROUP_KEY3_DOWN, &xHigherPriorityTaskWoken);
        EXTI_ClearITPendingBit(EXTI_Line3);
    }
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void EXTI4_IRQHandler(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if(EXTI_GetITStatus(EXTI_Line4) == SET)
    {
        xEventGroupSetBitsFromISR(g_event_group, EVENT_GROUP_KEY4_DOWN, &xHigherPriorityTaskWoken);
        EXTI_ClearITPendingBit(EXTI_Line4);
    }
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
