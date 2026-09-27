#include "FPM383F.h"
#include "FreeRTOS.h"
#include "task.h"

/*
 * FPM383F 电容指纹模块驱动
 *
 * 硬件连接：
 *   PA2/PA3 接 FPM383F 的 USART2 TX/RX（波特率 57600）
 *   PE15 接指纹模块 TOUCHOUT；PD15 仍用于电机方向控制
 *   TIM5 用于 USART2 接收完成检测（1ms 空闲判定）
 *
 * FPM383F 通信帧格式（16 进制）：
 *   [EF 01] [FF FF FF FF] [包标识] [长度高] [长度低] [指令码] [数据...] [校验和高] [校验和低]
 *   校验和 = 包标识 + 长度 + 指令码 + 数据 各字节之和
 */

/* USART2 接收状态标志（当前未直接用于帧长度，由 TIM5 空闲检测触发事件） */
volatile uint8_t USART2_STA  = 0;




/* USART2 接收缓冲区及接收字节计数 */
static volatile uint8_t  g_usart2_buf[64];
static volatile uint32_t g_usart2_cnt=0;
/* USART2 接收完成事件标志（TIM5 检测到串口空闲 1ms 后置 1） */
static volatile uint32_t g_usart2_event=0;

// PE15 触摸中断置位；S1～S4 指纹操作仍由独立按键发起
volatile uint32_t g_fpm_touch_event = 0;

/* 外设初始化结构体（复用，节省栈空间） */
static NVIC_InitTypeDef   NVIC_InitStructure;
static TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;

/* ---------------------------------------------------------------
 * FPM383F LED 控制协议帧（16字节固定格式）
 * 指令码 0x3C，参数字段控制颜色及闪烁方式
 * --------------------------------------------------------------- */
static const uint8_t fpm_led_blue[16]  = {0xEF,0x01,0xFF,0xFF,0xFF,0xFF,0x01,0x00,0x07,0x3C,0x03,0x01,0x01,0x00,0x00,0x49};
static const uint8_t fpm_led_red[16]   = {0xEF,0x01,0xFF,0xFF,0xFF,0xFF,0x01,0x00,0x07,0x3C,0x02,0x04,0x04,0x02,0x00,0x50};
static const uint8_t fpm_led_green[16] = {0xEF,0x01,0xFF,0xFF,0xFF,0xFF,0x01,0x00,0x07,0x3C,0x02,0x02,0x02,0x02,0x00,0x4C};

// PE15 接指纹模块 TOUCHOUT；上升沿产生触摸事件，不在中断中执行指纹操作
static void fpm_touch_init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    EXTI_InitTypeDef EXTI_InitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    // 配置 PE15 为下拉输入，避免 TOUCHOUT 悬空时误触发
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOE, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_SYSCFG, ENABLE);
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_15;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_DOWN;
    GPIO_Init(GPIOE, &GPIO_InitStructure);

    // PE15 映射到 EXTI15，上升沿触发
    SYSCFG_EXTILineConfig(EXTI_PortSourceGPIOE, EXTI_PinSource15);
    EXTI_InitStructure.EXTI_Line = EXTI_Line15;
    EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
    EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Rising;
    EXTI_InitStructure.EXTI_LineCmd = ENABLE;
    EXTI_Init(&EXTI_InitStructure);
    EXTI_ClearITPendingBit(EXTI_Line15);

    // 中断只设置标志，不调用 FreeRTOS API
    NVIC_InitStructure.NVIC_IRQChannel = EXTI15_10_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 6;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
}

void EXTI15_10_IRQHandler(void)
{
    if(EXTI_GetITStatus(EXTI_Line15) == SET)
    {
        if(GPIO_ReadInputDataBit(GPIOE, GPIO_Pin_15) == Bit_SET)
        {
            g_fpm_touch_event = 1;
        }
        EXTI_ClearITPendingBit(EXTI_Line15);
    }
}
/* ---------------------------------------------------------------
 * TIM5_Init: 初始化 TIM5，用于 USART2 接收帧完成检测
 * TIM5 时钟 = APB1 × 2 = 84MHz
 * 预分频 8400 → 计数时钟 10000Hz（每计数一次 = 0.1ms）
 * 计数值 10000/1000-1 = 9 → 每 1ms 产生一次更新中断
 * TIM5_IRQHandler 利用该中断检测 USART2 是否空闲（接收完成）
 * --------------------------------------------------------------- */
static void TIM5_Init(void)
{
	//使能TIM5的硬件时钟
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM5, ENABLE);
	
	//配置TIM5的分频值、计数值
	//TIM5硬件时钟=84MHz/8400=10000Hz，就是进行10000次计数，就是1秒时间的到达

	TIM_TimeBaseStructure.TIM_Period = 10000/1000-1; //计数值0 -> 999就是1毫秒时间的到达
	TIM_TimeBaseStructure.TIM_Prescaler = 8400-1;	//预分频值8400
	TIM_TimeBaseStructure.TIM_ClockDivision = 0;	//时钟分频，当前是没有的，不需要进行配置
	TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
	
	TIM_TimeBaseInit(TIM5, &TIM_TimeBaseStructure);
	
	//配置TIM5的中断
	TIM_ITConfig(TIM5,TIM_IT_Update,ENABLE);
	
	/* NVIC：抢占优先级 0，子优先级 1 */
	NVIC_InitStructure.NVIC_IRQChannel = TIM5_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);
	
	TIM_Cmd(TIM5,ENABLE);
}


/* ---------------------------------------------------------------
 * check_sum: 计算 FPM383F 帧校验和
 * 校验范围：从包标识（buf[6]）开始，共 len 个字节之和
 * 返回 16 位无符号和，高字节先发
 * --------------------------------------------------------------- */
static uint16_t check_sum(uint8_t *buf,uint32_t len)
{
	uint16_t sum=0;
	
	uint8_t *p=buf;
	
	while(len--)
		sum+=*p++;
	
	return sum;
}	

/* ---------------------------------------------------------------
 * usart2_printf_recv_buf: 调试用，将 USART2 接收缓冲区内容以十六进制打印到串口1
 * 仅在调试阶段使用，用于观察指纹模块返回的原始帧数据
 * --------------------------------------------------------------- */
static void usart2_printf_recv_buf(void)
{
	uint32_t i;
	
	for(i=0;i<USART2_STA;i++)
	{
		dgb_printf_safe("%02X ",g_usart2_buf[i]);
	
	}
	
	dgb_printf_safe("\r\n");
}

/* ---------------------------------------------------------------
 * fpm_send_data: 通过 USART2 向 FPM383F 指纹模块发送命令帧
 * @param length: 帧字节总数
 * @param FPM383C_Databuffer: 待发送帧数据
 * 发送完成后清空接收缓冲区计数和事件标志，准备接收应答帧
 * --------------------------------------------------------------- */
void fpm_send_data(int32_t length,uint8_t FPM383C_Databuffer[])
{
	int32_t i;
	
	// 发命令前清除上一帧状态，避免清掉本次命令的快速应答
	g_usart2_event = 0;
	g_usart2_cnt = 0;

	for(i = 0;i<length;i++)
	{
		USART_SendData(USART2,FPM383C_Databuffer[i]);
		
		/* 等待发送寄存器为空，确保每字节发送完成 */
		while(!USART_GetFlagStatus(USART2,USART_FLAG_TXE));
	}
	
}

/* ---------------------------------------------------------------
 * fpm_sleep: 发送睡眠命令并等待 USART2 应答
 * 当前 S1～S4 操作保持模块唤醒，不调用此函数。
 * TOUCHOUT 接 PE15；当前 S1～S4 操作不调用睡眠函数。
 * --------------------------------------------------------------- */
void fpm_sleep(void)
{
    uint8_t buf[12] = {0xEF,0x01,0xFF,0xFF,0xFF,0xFF,0x01,0x00,0x03,0x33,0x00,0x37};
    uint32_t timeout = 800;

    fpm_send_data(12, buf);
    while (!g_usart2_event && (--timeout))
        vTaskDelay(pdMS_TO_TICKS(1));
    g_usart2_event = 0;
    g_usart2_cnt = 0;

    dgb_printf_safe(timeout ? "[INFO] fpm_sleep: reply received\r\n" :
                              "[WARN] fpm_sleep: reply timeout\r\n");
}
/* ---------------------------------------------------------------
 * fpm_ctrl_led: 控制 FPM383F 模块 LED 颜色
 * @param color: FPM_LED_RED / FPM_LED_GREEN / FPM_LED_BLUE
 * 发送对应的 16 字节 LED 控制帧，无返回值判断
 * --------------------------------------------------------------- */
uint8_t fpm_ctrl_led(uint8_t color)
{

	if(color == FPM_LED_RED)
		fpm_send_data(16,(uint8_t *)fpm_led_red);
	
	if(color == FPM_LED_GREEN)
		fpm_send_data(16,(uint8_t *)fpm_led_green);	
	
	if(color == FPM_LED_BLUE)
		fpm_send_data(16,(uint8_t *)fpm_led_blue);	
	
	// LED 指令也有应答；等应答结束后再发指纹命令，避免串帧
	{
		uint32_t timeout = 200;
		while(!g_usart2_event && (--timeout))
			 vTaskDelay(pdMS_TO_TICKS(1));
	}
	return 0;
}

/* ---------------------------------------------------------------
 * fpm_empty: 清空 FPM383F 指纹库中所有已录入的指纹
 * 发送指令码 0x0D，等待模块应答（最多 4000ms）
 * 应答帧 buf[9] = 0x00 表示清空成功，否则返回 -1
 * --------------------------------------------------------------- */
int32_t fpm_empty(void)
{
    uint32_t timeout=4000;	
	

	uint8_t buf[12] = {0xEF,0x01,0xFF,0xFF,0xFF,0xFF,0x01,0x00,0x03,0x0D,0x00,0x11};	
	
	fpm_send_data(12,buf);
	
	/* 等待 TIM5 检测到 USART2 接收完成 */
	while(!g_usart2_event && (--timeout))
	{
		 vTaskDelay(pdMS_TO_TICKS(1)); ;
	}
	
	usart2_printf_recv_buf();	/* 调试打印原始响应帧 */
	
	/* 校验帧头 EF 01，确认是 FPM383F 的合法回应 */
	if(g_usart2_cnt < 10 || g_usart2_buf[0]!=0xEF || g_usart2_buf[1]!=0x01)
	{
		dgb_printf_safe("fpm_empty检查硬件连接\r\n");

		return -1;
	}	
	
	if(!timeout)
	{
		dgb_printf_safe("清空指纹:超时\r\n");
		
		return -1;
	}
	
	/*  打印出指纹模块工作出错原因 */
	if(g_usart2_buf[9] != 0x00)
	{

		dgb_printf_safe("清空指纹：失败 %02X\r\n",g_usart2_buf[9] );
		dgb_printf_safe("请不要按压指纹模块\r\n");
		return -1;
		
	}	

	return 0;
	
}

/* ---------------------------------------------------------------
 * fpm_enroll_auto: 自动注册指纹（指令码 0x31）
 * @param id: 要录入的指纹 ID（0x0000 ~ 0x00FF）
 * 录入次数固定为 1 次，安全等级参数 0x003F
 * 等待应答最多 4000ms，buf[9] = 0x00 表示录入成功
 * --------------------------------------------------------------- */
int32_t fpm_enroll_auto(uint16_t id)
{
    uint8_t buf[17];
	uint16_t cs=0;
    uint32_t timeout=4000;
	
    /* 包头：2字节 */
    buf[0]=0xEF;buf[1]=0x01;
    
    /* 设备地址：4字节 */    
    buf[2]=0xFF;buf[3]=0xFF;buf[4]=0xFF;buf[5]=0xFF;
	
    /* 包标识：1字节 */ 
    buf[6]=0x01;
    
    /* 包长度：2字节 */
    buf[7]=0x00;buf[8]=0x08;
    
    /* 指令码：1字节 - 自动注册 0x31 */
    buf[9]=0x31;
    
    /* 注册目标 ID：2字节，高字节在前 */
    buf[10]=(id>>8)&0xFF;buf[11]=id&0xFF;
    
    /* 录入次数：1字节，固定为 1 */
    buf[12]=1;
    
    /* 安全等级参数：2字节 */
    buf[13]=0x00;
    
    buf[14]=0x3F;
    
    /* 校验和覆盖范围：从 buf[6]（包标识）开始共 9 字节 */
    cs=check_sum(&buf[6],9);
    
    buf[15]=(cs>>8)&0xFF;
    buf[16]=(cs)&0xFF;   
	
	
	fpm_send_data(17,buf);
	
	/* 等待 TIM5 检测到 USART2 接收完成 */
	while(!g_usart2_event && (--timeout))
	{
		 vTaskDelay(pdMS_TO_TICKS(1)); ;
	}
	
	usart2_printf_recv_buf();
	
	if(g_usart2_cnt < 10 || g_usart2_buf[0]!=0xEF || g_usart2_buf[1]!=0x01)
	{
		dgb_printf_safe("自动注册指纹:数据包异常！\r\n");
		dgb_printf_safe("1.检查硬件连接\r\n");
		dgb_printf_safe("2.检查是否有按压指纹模块\r\n");		
		return -1;
	}	
	
	if(!timeout)
	{
		dgb_printf_safe("自动注册指纹:超时\r\n");
		
		return -1;
	}
	
	/*  打印出指纹模块工作出错原因 */
	if(g_usart2_buf[9] != 0x00)
	{

		dgb_printf_safe("自动注册指纹：失败 %02X\r\n",g_usart2_buf[9] );
		
		return -1;
		
	}	
	

	
	/* 正确返回 */ 	
	return 0;
}

/* ---------------------------------------------------------------
 * fpm_idenify_auto: 自动认证指纹（指令码 0x32，按指定 ID 比对）
 * @param id: 输入要比对的 ID，输出成功匹配的 ID（buf[11]~buf[12]）
 * 最低分数门槛 80 分，参数 0x0007
 * buf[9] = 0x00 且分数值不为 0xFFFF 表示比对成功
 * --------------------------------------------------------------- */
int32_t fpm_idenify_auto(uint16_t *id)
{
    uint8_t buf[17];
	uint16_t cs=0;
    uint32_t timeout=4000;
    
    /* 包头：2字节 */
    buf[0]=0xEF;buf[1]=0x01;
    
    /* 设备地址：4字节 */    
    buf[2]=0xFF;buf[3]=0xFF;buf[4]=0xFF;buf[5]=0xFF;
	
    /* 包标识：1字节 */ 
    buf[6]=0x01;
    
    /* 包长度：2字节 */
    buf[7]=0x00;buf[8]=0x08;
    
    /* 指令码：1字节 - 自动认证 0x32 */
    buf[9]=0x32;
    
    /* 匹配分数门槛：1字节，设为 80 */
    buf[10]=80;
    
    /* 目标 ID：2字节，高字节在前 */
    buf[11]=(*id>>8)&0xFF;buf[12]=*id&0xFF;
    
    /* 参数：2字节 */
    buf[13]=0x00;
    
    buf[14]=0x07;
    
    cs=check_sum(&buf[6],9);
    
    buf[15]=(cs>>8)&0xFF;
    buf[16]=(cs)&0xFF;    
    

	fpm_send_data(17,buf);
	
	/* 等待 TIM5 检测到 USART2 接收完成 */
	while(!g_usart2_event && (--timeout))
	{
		 vTaskDelay(pdMS_TO_TICKS(1)); 
	}
	
	usart2_printf_recv_buf();
	
	if(g_usart2_cnt < 10 || g_usart2_buf[0]!=0xEF || g_usart2_buf[1]!=0x01)
	{
		dgb_printf_safe("自动验证指纹:数据包异常！\r\n");
		dgb_printf_safe("1.检查硬件连接\r\n");
		dgb_printf_safe("2.检查是否有按压指纹模块\r\n");		
		return -1;
	}	
	
	if(!timeout)
	{
		dgb_printf_safe("自动验证指纹:超时\r\n");
		return -1;
	}
	
	/*  打印出指纹模块工作出错原因 */
	if(g_usart2_buf[9] != 0x00)
	{
		dgb_printf_safe("指纹验证失败--%02X\r\n",g_usart2_buf[9] );
		
		return -1;
	}
	
	/* 分数值 0xFFFF 表示未找到匹配指纹 */
	if(g_usart2_cnt < 15 || ((g_usart2_buf[13]<<8)|g_usart2_buf[14])==0xFFFF)
	{
		dgb_printf_safe("自动验证指纹：分数值异常，可能没有存在该指纹\r\n");
		
		return -1;		
	}		
	
	/* 成功匹配：从 buf[11]~buf[12] 读出匹配到的指纹 ID */
	*id = (g_usart2_buf[11]<<8)|g_usart2_buf[12];
	
	/* 正确返回 */ 	
	return 0;
}

/* ---------------------------------------------------------------
 * fpm_identify_all: 全库搜索指纹识别（指令码 0x32，ID 设为 0xFFFF）
 * @param found_id: 输出匹配成功的指纹 ID
 * 与 fpm_idenify_auto 的区别：目标 ID 固定为 0xFFFF，让模块在整个
 * 指纹库中搜索，而非只比对指定 ID。
 * 由独立按键发起识别，不要求 PE15 触摸事件。
 * --------------------------------------------------------------- */
int32_t fpm_identify_all(uint16_t *found_id)
{
    uint8_t buf[17];
    uint16_t cs=0;
    uint32_t timeout=4000;
    
    /* 包头：2字节 */
    buf[0]=0xEF;buf[1]=0x01;
    
    /* 设备地址：4字节 */    
    buf[2]=0xFF;buf[3]=0xFF;buf[4]=0xFF;buf[5]=0xFF;
    
    /* 包标识：1字节 */ 
    buf[6]=0x01;
    
    /* 包长度：2字节 */
    buf[7]=0x00;buf[8]=0x08;
    
    /* 指令码：1字节 - 自动识别 0x32 */
    buf[9]=0x32;
    
    /* 分数等级：1字节 */
    buf[10]=80;
    
    /* ID：2字节 - 0xFFFF 表示全库搜索 */
    buf[11]=0xFF; buf[12]=0xFF;
    
    /* 参数：2字节 */
    buf[13]=0x00;
    buf[14]=0x07;
    
    cs=check_sum(&buf[6],9);
    
    buf[15]=(cs>>8)&0xFF;
    buf[16]=(cs)&0xFF;    

    fpm_send_data(17,buf);
    
    /* 等待 TIM5 检测到 USART2 接收完成 */
    while(!g_usart2_event && (--timeout))
    {
         vTaskDelay(pdMS_TO_TICKS(1)); ;
    }
    
    usart2_printf_recv_buf();
    
	if(!timeout)
	{
		dgb_printf_safe("指纹识别:超时\r\n");
		return -1;
	}
	if(g_usart2_cnt < 10 || g_usart2_buf[0]!=0xEF || g_usart2_buf[1]!=0x01)
	{
		dgb_printf_safe("指纹识别:数据包异常！\r\n");
		return -1;
	}
		
    if(g_usart2_buf[9] != 0x00)
    {
        dgb_printf_safe("指纹识别失败--%02X\r\n",g_usart2_buf[9] );
        return -1;
    }
    
    /* 分数值 0xFFFF 表示全库无匹配 */
    if(g_usart2_cnt < 15 || ((g_usart2_buf[13]<<8)|g_usart2_buf[14])==0xFFFF)
    {
        dgb_printf_safe("指纹识别：未找到匹配指纹\r\n");
        return -1;        
    }        
    
    /* 成功匹配：从 buf[11]~buf[12] 读出指纹 ID */
    *found_id = (g_usart2_buf[11]<<8)|g_usart2_buf[12];
    
    return 0;
}

/* ---------------------------------------------------------------
 * fpm_id_total: 获取 FPM383F 指纹库中已录入的有效指纹总数
 * 发送指令码 0x1D，应答帧 buf[10]~buf[11] 为总数（大端）
 * --------------------------------------------------------------- */
int32_t fpm_id_total(uint16_t *total)
{
    uint32_t timeout=4000;	
	
	uint8_t buf[12] = {0xEF,0x01,0xFF,0xFF,0xFF,0xFF,0x01,0x00,0x03,0x1D,0x00,0x21};	
	
	fpm_send_data(12,buf);
	
	while(!g_usart2_event && (--timeout))
	{
		 vTaskDelay(pdMS_TO_TICKS(1)); ;
	}
	
	usart2_printf_recv_buf();	
	
	if(g_usart2_cnt < 10 || g_usart2_buf[0]!=0xEF || g_usart2_buf[1]!=0x01)
	{
		dgb_printf_safe("fpm_id_total请检查硬件连接\r\n");

		return -1;
	}	
	
	if(!timeout)
	{
		dgb_printf_safe("获取有效指纹总数:超时\r\n");
		
		return -1;
	}
	
	/*  打印出指纹模块工作出错原因 */
	if(g_usart2_buf[9] != 0x00)
	{
		dgb_printf_safe("获取有效指纹总数：失败 %02X\r\n",g_usart2_buf[9] );
		dgb_printf_safe("请不要按压指纹模块\r\n");
		return -1;
	}	
	
	/* buf[10]~buf[11]：指纹总数，高字节在前 */
	if(g_usart2_cnt < 12)
	{
		return -1;
	}

	*total=(g_usart2_buf[10]<<8)|g_usart2_buf[11];

	return 0;
}

/* ---------------------------------------------------------------
 * fpm_init: FPM383F 指纹模块总初始化入口
 * 初始化顺序：TIM5（接收完成检测）→ USART2（57600bps）→ PE15 触摸输入
 * S1～S4 直接发起指纹命令；PE15 触摸事件留给后续功能使用
 * --------------------------------------------------------------- */
void fpm_init(void)
{
	TIM5_Init();
	USART2_Config(57600);
	fpm_touch_init(); // TOUCHOUT 使用 PE15，不占用电机的 PD15
}

/* ---------------------------------------------------------------
 * fpm_error_code_auto_enroll: 将自动注册指令的错误码转换为可读字符串
 * @param error_code: 应答帧 buf[9] 的值
 * @return: 对应的中文描述字符串
 * --------------------------------------------------------------- */
const char *fpm_error_code_auto_enroll(uint8_t error_code)
{
	const char *p;
	
	switch(error_code)
	{
		case 0x00:p="自动注册成功";
		break;
		
		case 0x01:p="自动执行失败";
		break;	

		case 0x07:p="生成特征失败";
		break;		

		case 0x0A:p="合并模板";
		break;		

		case 0x0B:p="ID号超出范围";
		break;	
		
		case 0x1F:p="指纹库已满";
		break;		
	
		case 0x22:p="指纹模板非空";
		break;	
		
		case 0x25:p="录入次数设置错误";
		break;	

		case 0x26:p="超时";
		break;	

		case 0x27:p="指纹已存在";
		break;	
		
		case 0x31:p="功能与加密等级不匹配";
		break;

		default :
			p="模块返回确认码有误";break;
	}

	return p;
}

/* ---------------------------------------------------------------
 * USART2_IRQHandler: USART2 接收中断服务函数
 * 每收到一个字节就存入 g_usart2_buf，计数器 g_usart2_cnt 加 1。
 * 缓冲区满时停止接收（防止溢出）。
 * 接收完成由 TIM5_IRQHandler 检测空闲来判定（非 IDLE 中断）。
 * --------------------------------------------------------------- */
void USART2_IRQHandler(void)
{
	static uint8_t d=0;
	
	//检测是否接收到数据
	if (USART_GetITStatus(USART2, USART_IT_RXNE) == SET)
	{
		d=USART_ReceiveData(USART2);
		
		/* 缓冲区未满时才存入，防止越界 */
		if(g_usart2_cnt< sizeof(g_usart2_buf))
			g_usart2_buf[g_usart2_cnt++]=d;
		
		//清空标志位，可以响应新的中断请求
		USART_ClearITPendingBit(USART2, USART_IT_RXNE);
	}
}

/* ---------------------------------------------------------------
 * TIM5_IRQHandler: TIM5 更新中断（1ms 周期）
 * 用于检测 USART2 接收完成（空闲检测）：
 *   - 若当前字节计数 cnt != g_usart2_cnt，说明还在接收，更新快照
 *   - 若 cnt != 0 且 cnt == g_usart2_cnt，说明连续 1ms 无新数据，
 *     认为一帧接收完毕，置 g_usart2_event = 1 通知上层处理
 * 该机制代替硬件 IDLE 中断，兼容各种长度的应答帧。
 * --------------------------------------------------------------- */
void TIM5_IRQHandler(void)
{
	static uint32_t cnt=0;

	//检测标志位
	if(TIM_GetITStatus(TIM5,TIM_IT_Update) == SET)
	{
		/* 若接收计数发生变化，更新快照，继续等待 */
		if(cnt!=g_usart2_cnt)
		{
			cnt=g_usart2_cnt;
		}
		/* 若快照非 0 且与当前计数相同，说明 1ms 内无新字节，帧接收完成 */
		else if(cnt && (cnt == g_usart2_cnt))
		{
			g_usart2_event=1;
			cnt=0;
		}
		
		//清空标志位
		TIM_ClearITPendingBit(TIM5,TIM_IT_Update);
	}
}
