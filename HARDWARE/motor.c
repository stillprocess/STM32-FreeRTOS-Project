#include "motor.h"
#include "uart.h"
#include "delay.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>
/*
 * 直流电机驱动模块（TB6612FNG 驱动芯片）
 *
 * 硬件连接：
 *   PD15 → TB6612 AIN1（方向控制位1）
 *   PE7  → TB6612 AIN2（方向控制位2）
 *   PE6  → TB6612 PWMA（速度控制，TIM9_CH2，1kHz PWM）
 *
 * TB6612 AIN1/AIN2 真值表：
 *   AIN1=1, AIN2=0 → 正转（MOTOR_FORWARD）
 *   AIN1=0, AIN2=1 → 反转（MOTOR_REVERSE）
 *   AIN1=0, AIN2=0 → 自由停止（MOTOR_STOP）
 *   AIN1=1, AIN2=1 → 短路制动（MOTOR_BRAKE）
 */

/* ==================== 电机引脚定义 ==================== */
// 根据您的最新接线：AIN1→PD15，AIN2→PE7，PWM→PE6
#define MOTOR_PWM_PORT    GPIOE         // PWM信号端口 (PE6) TIM9 CH2
#define MOTOR_DIR1_PORT   GPIOD         // AIN1控制端口 (PD15)
#define MOTOR_DIR2_PORT   GPIOE         // AIN2控制端口 (PE7)

#define MOTOR_DIR1_PIN    GPIO_Pin_15   // PD15 -> TB6612 AIN1 (方向控制)
#define MOTOR_DIR2_PIN    GPIO_Pin_7    // PE7 -> TB6612 AIN2 (方向控制)
#define MOTOR_PWM_PIN     GPIO_Pin_6    // PE6 -> TB6612 PWMA

/* 引脚操作宏（安全版本）
*/
#define MOTOR_DIR1(x)     do{x ? (GPIO_SetBits(MOTOR_DIR1_PORT,MOTOR_DIR1_PIN )) : (GPIO_ResetBits(MOTOR_DIR1_PORT,MOTOR_DIR1_PIN ));}while(0)
#define MOTOR_DIR2(x)     do{x ? (GPIO_SetBits(MOTOR_DIR2_PORT,MOTOR_DIR2_PIN )) : (GPIO_ResetBits(MOTOR_DIR2_PORT,MOTOR_DIR2_PIN ));}while(0)


void Motor_Init(void)
{
	//1.定义GPIO外设的结构体变量
	GPIO_InitTypeDef  GPIO_InitStructure;

	//2.打开GPIOD端口、GPIOE端口的时钟
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOE | RCC_AHB1Periph_GPIOD, ENABLE);


	//3.配置PE7、PE12(STBY)、PD15为输出模式

	GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_OUT;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_NOPULL;

	//4.对AIN1->GPIOD,AIN2->GPIOE端口进行初始化
	GPIO_InitStructure.GPIO_Pin   = MOTOR_DIR1_PIN ;
	GPIO_Init(MOTOR_DIR1_PORT, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin   = MOTOR_DIR2_PIN;
	GPIO_Init(MOTOR_DIR2_PORT, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin   =  GPIO_Pin_12;
	GPIO_Init(GPIOE, &GPIO_InitStructure);
	GPIO_SetBits(GPIOE,GPIO_Pin_12);

	// AIN1、AIN2均置低，电机上电保持停止
    MOTOR_DIR1(0);  // AIN1 = 0 (PD15 = 0)
    MOTOR_DIR2(0);  // AIN2 = 0 (PE7 = 0)

}

 /* ---------------------------------------------------------------
 * PWM_Motor_Init: 直流电机 PWM 初始化
 * TIM9_CH2输出到PE6，当前配置的PWM频率约2kHz
 * TIM9时钟168MHz，预分频84后计数频率2MHz，周期为1001个计数
 * 初始占空比0%；通过PWM_SetCompareMotor更新TIM9_CH2比较值
 * --------------------------------------------------------------- */


void PWM_Motor_Init(void)
{

	//定义结构体变量
	GPIO_InitTypeDef  GPIO_InitStructure;
    TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;
    TIM_OCInitTypeDef  TIM_OCInitStructure;

	//开启相关时钟
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOE, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM9, ENABLE);

	//配置PE6引脚为复用模式
	GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_UP;

	// 将PE6配置为TIM9复用功能
	GPIO_PinAFConfig(GPIOE,GPIO_PinSource6, GPIO_AF_TIM9);

	//对PWMA->GPIOE端口进行初始化
	GPIO_InitStructure.GPIO_Pin   = MOTOR_PWM_PIN  ;
	GPIO_Init(MOTOR_PWM_PORT, &GPIO_InitStructure);

  /* Time base configuration */
  TIM_TimeBaseStructure.TIM_Period = 1000;          // 1001个计数，PWM约2kHz
  TIM_TimeBaseStructure.TIM_Prescaler = 84-1;       // 168MHz/84=2MHz，0.5us计数一次
  TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
  TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;

  TIM_TimeBaseInit(TIM9, &TIM_TimeBaseStructure);

  /*配置输出比较单元 - TIM9_CH2*/
  TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;                //CNT < CCR 时有效
  TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;    //使能通道输出
  TIM_OCInitStructure.TIM_Pulse = 0;                               //初始占空比为0
  TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;        //输出极性为高 CNT < CCR 输出为高电平
  TIM_OC2Init(TIM9, &TIM_OCInitStructure);                         // 把配置写入TIM9_CH2
  TIM_OC2PreloadConfig(TIM9, TIM_OCPreload_Disable);               // 禁用TIM9_CH2预装载

  TIM_ARRPreloadConfig(TIM9, ENABLE);								// 使能自动重装载预装载
  TIM_Cmd(TIM9, ENABLE);                                            //开启时钟

  //设置PWM为0（电机停止）
    PWM_SetCompareMotor(0);

    printf("Motor driver initialized safely!\r\n");
    printf("AIN1: PD15, AIN2: PE7, PWM: PE6\r\n");
    printf("Initial state: Direction=STOP, Speed=0%%\r\n");
    printf("Motor will NOT run on power-up now!\r\n");
}

//TIM9_CH2 的 CCR 值
void PWM_SetCompareMotor(uint16_t CCR)
{
	 TIM_SetCompare2(TIM9, CCR);
}

/* ---------------------------------------------------------------
 * Motor_SetSpeed: 设置电机转速
 * @param speed: 速度百分比，范围 0~100（超出自动限幅为 100）
 * 百分比乘10后写入TIM9_CH2的CCR，0～100对应0～1000
 * --------------------------------------------------------------- */
void Motor_SetSpeed( uint16_t speed)
{
    if(speed > 100) speed = 100;

    PWM_SetCompareMotor((uint16_t)speed * 10U);
}


/* ---------------------------------------------------------------
 * Motor_Control: 电机方向与转速综合控制
 * @param direction: 运行方向
 *   MOTOR_FORWARD(0) → 正转：AIN1=1（PD15=1），AIN2=0（PE7=0）
 *   MOTOR_REVERSE(1) → 反转：AIN1=0（PD15=0），AIN2=1（PE7=1）
 *   MOTOR_STOP(2)    → 自由停止：AIN1=0，AIN2=0（电机惰行）
 *   MOTOR_BRAKE(3)   → 短路制动：AIN1=1，AIN2=1（快速停止）
 * @param speed: 速度百分比，0~100
 * 先设置方向，再调用 Motor_SetSpeed 写入 PWM 占空比
 * --------------------------------------------------------------- */
void Motor_Control(uint8_t direction, uint8_t speed)
{
    switch(direction)
    {
        case MOTOR_FORWARD:  // 正转: AIN1=1, AIN2=0
            MOTOR_DIR1(1);  // PD15 = 1
            MOTOR_DIR2(0);  // PE7 = 0
            break;

        case MOTOR_REVERSE:  // 反转: AIN1=0, AIN2=1
            MOTOR_DIR1(0);  // PD15 = 0
            MOTOR_DIR2(1);  // PE7 = 1
            break;

        case MOTOR_STOP:     // 停止: AIN1=0, AIN2=0
            MOTOR_DIR1(0);  // PD15 = 0
            MOTOR_DIR2(0);  // PE7 = 0
            break;

        case MOTOR_BRAKE:    // 制动: AIN1=1, AIN2=1
            MOTOR_DIR1(1);  // PD15 = 1
            MOTOR_DIR2(1);  // PE7 = 1
            break;

        default:  /* 未知指令：安全停止 */
            MOTOR_DIR1(0);
            MOTOR_DIR2(0);
            break;
    }

    Motor_SetSpeed(speed);
}

/* ---------------------------------------------------------------
 * Motor_EmergencyStop: 电机急停（两阶段）
 * 第一阶段：MOTOR_BRAKE + 满速 PWM，产生短路制动力矩，快速减速
 * 第二阶段：等待 10ms 后切换为 MOTOR_STOP，释放制动，完全断开驱动
 * --------------------------------------------------------------- */
void Motor_EmergencyStop(void)
{
    Motor_Control(MOTOR_BRAKE, 100);  // 短路制动
    delay_ms(10);
    Motor_Control(MOTOR_STOP, 0);     // 完全停止
}

/* ---------------------------------------------------------------
 * Motor_SoftControl: 电机软启动 / 软停止（斜坡加减速）
 * @param target_speed:  目标速度百分比（0~100）
 * @param step_time_ms:  每步之间的延时时间（ms），值越大加速越慢
 * @param step_size:     每步改变的速度值，值越大加速越快
 * 使用静态变量 current_speed 记录当前速度，实现平滑过渡，
 * 避免直接跳变导致电机和驱动芯片过流冲击。
 * --------------------------------------------------------------- */
void Motor_SoftControl(uint8_t target_speed, uint16_t step_time_ms, uint8_t step_size)
{
    static uint8_t current_speed = 0;
    int16_t speed_diff = target_speed - current_speed;

    if(speed_diff > 0) {
        // 渐加速
        while(current_speed < target_speed) {
            current_speed += step_size;
            if(current_speed > target_speed) current_speed = target_speed;
            Motor_SetSpeed(current_speed);
            vTaskDelay(pdMS_TO_TICKS(step_time_ms));
        }
    } else if(speed_diff < 0) {
        // 渐减速
        while(current_speed > target_speed) {
            if(current_speed >= step_size) {
                current_speed -= step_size;
            } else {
                current_speed = 0;  /* 防止无符号数下溢 */
            }
            if(current_speed < target_speed) current_speed = target_speed;
            Motor_SetSpeed(current_speed);
            vTaskDelay(pdMS_TO_TICKS(step_time_ms));
        }
    }
}



/* ---------------------------------------------------------------
 * Motor_FwdRevTest: 电机正反转循环测试（调试/出厂验证用）
 * @param speed: 测试速度百分比（0~100）
 * 循环执行：正转 2s → 停止 0.5s → 反转 2s → 停止 0.5s
 * 注意：此函数包含 while(1) 死循环，调试完成后不应在正式代码中调用
 * --------------------------------------------------------------- */
void Motor_FwdRevTest(uint8_t speed)
{
    printf("Forward and reverse test start...\r\n");

    while(1)
    {
        printf("Forward...\r\n");
        Motor_Control(MOTOR_FORWARD, speed);
        delay_ms(2000);

        printf("Stop...\r\n");
        Motor_Control(MOTOR_STOP, 0);
        delay_ms(500);

        printf("Reverse...\r\n");
        Motor_Control(MOTOR_REVERSE, speed);
        delay_ms(2000);

        printf("Stop...\r\n");
        Motor_Control(MOTOR_STOP, 0);
        delay_ms(500);
    }
}


/* ---------------------------------------------------------------
 * Motor_SpeedTest: 电机速度阶梯测试（调试/出厂验证用）
 * 循环以 20%、40%、60%、80%、100% 五个档位正转各 2s，
 * 然后调用 Motor_SoftControl 软减速到 0，停止 1s 后重复
 * 注意：此函数包含 while(1) 死循环，调试完成后不应在正式代码中调用
 * --------------------------------------------------------------- */
void Motor_SpeedTest(void)
{
    uint8_t speed;
    printf("Speed step test start...\r\n");

    while(1)
    {
        for(speed = 20; speed <= 100; speed += 20)
        {
            printf("Speed %d%%\r\n", speed);
            Motor_Control(MOTOR_FORWARD, speed);
            delay_ms(2000);
        }

        printf("Decelerate to stop...\r\n");
        Motor_SoftControl(0, 500, 5);  /* 每500ms减速5% */
        delay_ms(1000);
    }
}

/* ---------------------------------------------------------------
 * Motor_ComprehensiveTest: 电机综合测试（调试/出厂验证用）
 * 依次执行：正转软加速 → 正转全速 → 正转软减速 →
 *           反转软加速 → 反转全速 → 反转软减速 → 急停
 * 覆盖加速、全速、减速、反转、制动等所有工况
 * 注意：此函数包含 while(1) 死循环，调试完成后不应在正式代码中调用
 * --------------------------------------------------------------- */
void Motor_ComprehensiveTest(void)
{
    uint8_t i;
    printf("Comprehensive motor test start...\r\n");

    while(1)
    {
        // 正转加速测试
        printf("Forward acceleration test...\r\n");
        Motor_Control(MOTOR_FORWARD, 0);
        for(i = 0; i <= 100; i += 20) {
            Motor_SoftControl(i, 300, 10);
            delay_ms(100);
        }

        delay_ms(1000);

        printf("Forward full speed...\r\n");
        Motor_Control(MOTOR_FORWARD, 100);
        delay_ms(2000);

        printf("Forward deceleration...\r\n");
        Motor_SoftControl(0, 300, 10);
        delay_ms(1000);

        // 反转测试
        printf("Reverse acceleration test...\r\n");
        Motor_Control(MOTOR_REVERSE, 0);
        for(i = 0; i <= 100; i += 20) {
            Motor_SoftControl(i, 300, 10);
            delay_ms(100);
        }

        delay_ms(1000);

        printf("Reverse full speed...\r\n");
        Motor_Control(MOTOR_REVERSE, 100);
        delay_ms(2000);

        printf("Reverse deceleration...\r\n");
        Motor_SoftControl(0, 300, 10);
        delay_ms(1000);

        // 制动测试
        printf("Brake test...\r\n");
        Motor_EmergencyStop();
        delay_ms(1000);
    }
}
