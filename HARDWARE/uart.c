#include "uart.h"
#include <stdio.h>

/* USART3接收缓冲区和接收字节计数，由USART3中断写入 */

volatile uint8_t  u3_recvbuf[512] = {0};
volatile uint32_t u3_recvcnt = 0;

int fputc(int ch, FILE *f)
{
	USART_SendData(USART1,(uint8_t)ch);
	while(USART_GetFlagStatus(USART1,USART_FLAG_TXE) == RESET);
	return ch;
}


void USART1_Config(uint32_t baud)
{
	// USART1_TX  PA9  USART1_RX PA10
	//定义GPIO外设的结构体变量
	GPIO_InitTypeDef  GPIO_InitStructure;

	//打开GPIOA端口端口的时钟 选择GPIO引脚的复用功能
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);

	GPIO_PinAFConfig(GPIOA,GPIO_PinSource9,GPIO_AF_USART1);
	GPIO_PinAFConfig(GPIOA,GPIO_PinSource10,GPIO_AF_USART1);

	//配置PA9  PA10 注意复用模式
	GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_UP;

	//对GPIOA端口进行初始化
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9 | GPIO_Pin_10;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	//使能串口1时钟
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);

	//定义串口的结构体变量
	USART_InitTypeDef USART_InitStructure;

	//配置串口参数，初始化串口
	USART_InitStructure.USART_BaudRate = baud;                                      //波特率
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;                     //数据位
	USART_InitStructure.USART_StopBits = USART_StopBits_1;                          //停止位
	USART_InitStructure.USART_Parity = USART_Parity_No;                             //无校验
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;                //收发模式
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;  //无硬件流控
	USART_Init(USART1,&USART_InitStructure);

	//定义NVIC结构体变量
	NVIC_InitTypeDef NVIC_InitStructure;

	NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn ;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);

   //选择USART1的中断源，接收到数据则触发中断
	USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);

   //打开串口
    USART_Cmd(USART1, ENABLE);
    USART_ClearITPendingBit(USART1, USART_IT_RXNE); // 清除初始化期间遗留的接收标志
}

	//利用串口发送一个字符串
void  USART1_SendString(const char *str)
{
	while(*str)
	{
		USART_SendData(USART1,*str++);
		while(USART_GetFlagStatus(USART1,USART_FLAG_TXE) == RESET);
	}

}
	//前台程序就是中断服务程序，该程序是不需要手动调用的，当中断触发之后CPU会自动跳转过来执行该函数
void USART1_IRQHandler(void)
{
    uint8_t data;
    if (USART_GetITStatus(USART1, USART_IT_RXNE) == SET)
    {
        data = USART_ReceiveData(USART1);
        // 发送前等待 TXE（无需超时，直接循环）
        while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);
        USART_SendData(USART1, data);
    }
}

void USART2_Config(uint32_t baud)
{
	// USART2_TX  PA2  USART2_RX PA3
	//定义GPIO外设的结构体变量
	GPIO_InitTypeDef  GPIO_InitStructure;

	//打开GPIOA端口端口的时钟 选择GPIO引脚的复用功能
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);

	GPIO_PinAFConfig(GPIOA,GPIO_PinSource2,GPIO_AF_USART2);
	GPIO_PinAFConfig(GPIOA,GPIO_PinSource3,GPIO_AF_USART2);

	//配置PA2  PA3 注意复用模式
	GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_UP;

	//对GPIOA端口进行初始化
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2 | GPIO_Pin_3;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	//使能串口2时钟
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);

	//定义串口的结构体变量
	USART_InitTypeDef USART_InitStructure;

	//配置串口参数，初始化串口
	USART_InitStructure.USART_BaudRate = baud;                                      //波特率
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;                     //数据位
	USART_InitStructure.USART_StopBits = USART_StopBits_1;                          //停止位
	USART_InitStructure.USART_Parity = USART_Parity_No;                             //无校验
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;                //收发模式
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;  //无硬件流控
	USART_Init(USART2,&USART_InitStructure);

	//定义NVIC结构体变量
	NVIC_InitTypeDef NVIC_InitStructure;

	NVIC_InitStructure.NVIC_IRQChannel = USART2_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);

   //选择USART2的中断源，接收到数据则触发中断
	USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);

   //打开串口
    USART_Cmd(USART2, ENABLE);
    USART_ClearITPendingBit(USART2, USART_IT_RXNE); // 加这一行！
}


void USART3_Config(uint32_t baud)
{
	// USART3_TX  PB10  USART3_RX PB11
	//定义GPIO外设的结构体变量
	GPIO_InitTypeDef  GPIO_InitStructure;

	//打开GPIOA端口端口的时钟 选择GPIO引脚的复用功能
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);

	GPIO_PinAFConfig(GPIOB,GPIO_PinSource10,GPIO_AF_USART3);
	GPIO_PinAFConfig(GPIOB,GPIO_PinSource11,GPIO_AF_USART3);

	//配置PB11  PB10 注意复用模式
	GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_UP;

	//对GPIOB端口进行初始化
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11 | GPIO_Pin_10;
	GPIO_Init(GPIOB, &GPIO_InitStructure);

	//使能串口3时钟
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART3, ENABLE);

	//定义串口的结构体变量
	USART_InitTypeDef USART_InitStructure;

	//配置串口参数，初始化串口
	USART_InitStructure.USART_BaudRate = baud;                                      //波特率
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;                     //数据位
	USART_InitStructure.USART_StopBits = USART_StopBits_1;                          //停止位
	USART_InitStructure.USART_Parity = USART_Parity_No;                             //无校验
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;                //收发模式
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;  //无硬件流控
	USART_Init(USART3,&USART_InitStructure);

	//定义NVIC结构体变量
	NVIC_InitTypeDef NVIC_InitStructure;

	NVIC_InitStructure.NVIC_IRQChannel = USART3_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);

   //选择USART3的中断源，接收到数据则触发中断
	USART_ITConfig(USART3, USART_IT_RXNE, ENABLE);

   //打开串口
    USART_Cmd(USART3, ENABLE);
    USART_ClearITPendingBit(USART3, USART_IT_RXNE); // 加这一行！
}

	//利用串口3发送一个字符串
void  USART3_SendString(const char *str)
{
	while(*str)
	{
		USART_SendData(USART3,*str++);
		while(USART_GetFlagStatus(USART3,USART_FLAG_TXE) == RESET);
	}

}
//USART3接收中断服务函数
void USART3_ClearRxBuffer(void)
{
	//清空接收缓冲区时暂时关闭接收中断
	USART_ITConfig(USART3, USART_IT_RXNE, DISABLE);
	u3_recvcnt = 0;
	u3_recvbuf[0] = '\0';
	USART_ITConfig(USART3, USART_IT_RXNE, ENABLE);
}



//从USART3接收缓冲区头部移除已处理的数据，保留后续MQTT报文
void USART3_DiscardRxBytes(uint32_t length)
{
	uint32_t i;
	uint32_t remain;

	USART_ITConfig(USART3, USART_IT_RXNE, DISABLE);
	if(length >= u3_recvcnt)
	{
		u3_recvcnt = 0;
		u3_recvbuf[0] = '\0';
	}
	else
	{
		remain = u3_recvcnt - length;
		for(i = 0; i < remain; i++)
		{
			u3_recvbuf[i] = u3_recvbuf[length + i];
		}
		u3_recvcnt = remain;
		u3_recvbuf[remain] = '\0';
	}
	USART_ITConfig(USART3, USART_IT_RXNE, ENABLE);
}

//前台程序就是中断服务程序，该程序是不需要手动调用的，当中断触发之后CPU会自动跳转过来执行该函数
void USART3_IRQHandler(void)
{
	uint8_t data;

	if(USART_GetITStatus(USART3, USART_IT_RXNE) == SET)
	{
		data = (uint8_t)USART_ReceiveData(USART3);

		//保留一个位置存放字符串结束符
		if(u3_recvcnt < sizeof(u3_recvbuf) - 1)
		{
			u3_recvbuf[u3_recvcnt++] = data;
			u3_recvbuf[u3_recvcnt] = '\0';
		}
	}
}
