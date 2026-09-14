#include "led.h"

void LED_Config(void)
{
	//1.定义GPIO外设的结构体变量
	GPIO_InitTypeDef  GPIO_InitStructure;
	
	//2.打开GPIOF端口、GPIOE端口的时钟
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOE | RCC_AHB1Periph_GPIOF, ENABLE);

	
	//3.配置PF,PE引脚为输出模式
	
	GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_OUT;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_NOPULL;
	
	//4.对GPIOF,GPIOE端口进行初始化
	GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_9 | GPIO_Pin_10;
	GPIO_Init(GPIOF, &GPIO_InitStructure);
	
	GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_13 | GPIO_Pin_14;
	GPIO_Init(GPIOE, &GPIO_InitStructure);

	// 低电平点亮；上电先让四个LED全部熄灭
	GPIO_SetBits(GPIOF,GPIO_Pin_9  | GPIO_Pin_10);
	GPIO_SetBits(GPIOE,GPIO_Pin_13 | GPIO_Pin_14);
	
}

// 根据DHT11报警状态控制LED；报警消失后对应LED熄灭
void LED_SetAlarmState(uint8_t humidity_alarm, uint8_t temperature_alarm)
{
    // 湿度报警：LED1(PF9)、LED2(PF10)
    if(humidity_alarm != 0)
        GPIO_ResetBits(GPIOF, GPIO_Pin_9 | GPIO_Pin_10);
    else
        GPIO_SetBits(GPIOF, GPIO_Pin_9 | GPIO_Pin_10);

    // 温度报警：LED3(PE13)、LED4(PE14)
    if(temperature_alarm != 0)
        GPIO_ResetBits(GPIOE, GPIO_Pin_13 | GPIO_Pin_14);
    else
        GPIO_SetBits(GPIOE, GPIO_Pin_13 | GPIO_Pin_14);
}
