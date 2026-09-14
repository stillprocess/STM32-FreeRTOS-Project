#include "stm32f4xx.h"
#include "FreeRTOS.h"
#include "stdio.h"
#include <stdarg.h>
#include "string.h"
#include "task.h"
#include "led.h"
#include "queue.h"
#include "uart.h"
#include "semphr.h"
#include "event_groups.h"
#include "keyboard.h"
#include "dht11.h"
#include "oled.h"
#include "beep.h"
#include "servo.h"
#include "esp8266.h"
#include "onenet_mqtt.h"
#include "stm32f4xx_iwdg.h"
#include "stm32f4xx_rcc.h"
#include "motor.h"
#include "FPM383F.h"
#include "key.h"



// 蜂鸣器事件位
#define BEEP_EVENT_SHORT  (1U << 0)
#define BEEP_EVENT_DOUBLE (1U << 1)

// 指纹调试打印开关
#define DEBUG_dgb_printf_safe_EN	1

// 指纹验证成功的舵机命令，181不是实际角度
#define SERVO_FINGERPRINT_OPEN 181U


// 事件组句柄
EventGroupHandle_t g_event_group = NULL;
EventGroupHandle_t xBeepEventGroup = NULL;

// OLED显示消息结构体
typedef struct
{
	uint8_t x;
	uint8_t page;
	char text[16];
	uint8_t size;
} OLED_Message_t;


// DHT11采集值与云端下发的报警阈值
uint8_t g_temp = 0;
uint8_t g_humi = 0;
volatile uint8_t g_max_humidity = 100;
volatile uint8_t g_min_humidity = 0;
volatile float g_max_temperature = 80.0f;
volatile float g_min_temperature = -40.0f;
volatile uint8_t g_humidity_alarm = 0;
volatile uint8_t g_temperature_alarm = 0;
volatile uint8_t g_iwdg_feed_enabled = 1;


// 队列句柄
QueueHandle_t xOLEDQueue = NULL;
QueueHandle_t xServoQueue = NULL;
QueueHandle_t xMotorQueue = NULL;

// 串口互斥锁句柄
SemaphoreHandle_t xSemaphore = NULL;



// 检查并清除上次复位原因
static uint8_t IWDG_WasReset(void)
{
	uint8_t was_reset;

	was_reset = (RCC_GetFlagStatus(RCC_FLAG_IWDGRST) == SET) ? 1U : 0U;
	RCC_ClearFlag();
	return was_reset;
}

// 初始化独立看门狗，标称超时时间约8秒
static void IWDG_Config(void)
{
	IWDG_WriteAccessCmd(IWDG_WriteAccess_Enable);
	IWDG_SetPrescaler(IWDG_Prescaler_256);
	IWDG_SetReload(1000);

	while(IWDG_GetFlagStatus(IWDG_FLAG_PVU) == SET)
	{
	}
	while(IWDG_GetFlagStatus(IWDG_FLAG_RVU) == SET)
	{
	}

	IWDG_ReloadCounter();
	IWDG_Enable();
}

// 初始化任务：先创建事件组和队列，其他任务再使用它们
void Task_Config(void *arg)
{
	uint8_t was_iwdg_reset;
    // 初始化LED
	LED_Config();

    // 初始化USART1
	USART1_Config(9600);

    // 输出上一次系统复位原因
	was_iwdg_reset = IWDG_WasReset();
	USART1_SendString(was_iwdg_reset != 0 ?
					  "RESET CAUSE: IWDG\r\n" :
					  "RESET CAUSE: POWER/OTHER\r\n");

    // 初始化矩阵键盘
    key_board_init();

    // 初始化DHT11
    DHT11_Config();

    // 初始化蜂鸣器
	Beep_Config();

    // 初始化舵机PWM，默认角度为90度
	Servo_PWM_Init();

	//初始化电机
    Motor_Init();
    PWM_Motor_Init();

    // 创建USART互斥锁
   xSemaphore = xSemaphoreCreateMutex();
   if( xSemaphore == NULL )
   {
       for( ;; );
   }

    // 创建蜂鸣器事件组
	xBeepEventGroup = xEventGroupCreate();
    if( xBeepEventGroup == NULL )
    {
        for( ;; );
    }

	// S1～S4的中断通知按键任务，按键任务再通知指纹任务
	g_event_group = xEventGroupCreate();
	if(g_event_group == NULL)
	{
		for( ;; );
	}

    // 创建OLED消息队列
	xOLEDQueue = xQueueCreate(5, sizeof(OLED_Message_t));
	if(xOLEDQueue == NULL)
	{
		for( ;; );
	}
    // 创建舵机角度队列
	xServoQueue = xQueueCreate(3, sizeof(uint16_t));
	if(xServoQueue == NULL)
	{
		for( ;; );
	}
	// 创建电机队列
		xMotorQueue = xQueueCreate(4, sizeof(uint16_t));
		if(xMotorQueue == NULL)
		{
			for( ;; );
		}


    // 删除初始化任务
	vTaskDelete(NULL);
}


// 独立看门狗任务
void IWDG_Task(void *arg)
{
	(void)arg;
	IWDG_Config();

	if(xSemaphoreTake(xSemaphore, portMAX_DELAY) == pdTRUE)
	{
		USART1_SendString("IWDG STARTED, KEY D STOPS FEEDING\r\n");
		xSemaphoreGive(xSemaphore);
	}

	for( ;; )
	{
		if(g_iwdg_feed_enabled != 0)
		{
			IWDG_ReloadCounter();
		}
		vTaskDelay(pdMS_TO_TICKS(1000));
	}
}



void dgb_printf_safe(const char *format, ...)
{
#if DEBUG_dgb_printf_safe_EN

	va_list args;
	va_start(args, format);

	/* 获取互斥信号量 */
	xSemaphoreTake(xSemaphore, portMAX_DELAY);

	vprintf(format, args);

	/* 释放互斥信号量 */
	xSemaphoreGive(xSemaphore);

	va_end(args);
#else
	(void)0;
#endif
}

// 四个独立按键共用消抖、等待松开和事件通知流程
static void FPM_HandleKey(IRQn_Type irq, GPIO_TypeDef *port, uint16_t pin,
                          EventBits_t event_bit, const char *key_name)
{
    NVIC_DisableIRQ(irq);

    if(GPIO_ReadInputDataBit(port, pin) == 0)
    {
        dgb_printf_safe("[app_task_key] %s Press\r\n", key_name);
        while(GPIO_ReadInputDataBit(port, pin) == 0)
        {
            vTaskDelay(pdMS_TO_TICKS(20));
        }
        xEventGroupSetBits(g_event_group, event_bit);
    }

    NVIC_EnableIRQ(irq);
}

static void app_task_key(void* pvParameters)
{
    EventBits_t event_value;

    (void)pvParameters;
    key_init();

    for( ;; )
    {
        // 任一按键中断唤醒任务，取出事件后清除按键事件位
        event_value = xEventGroupWaitBits(g_event_group, EVENT_GROUP_KEY_ALL,
                                          pdTRUE, pdFALSE, portMAX_DELAY);
        vTaskDelay(pdMS_TO_TICKS(50));

        if((event_value & EVENT_GROUP_KEY1_DOWN) != 0)
            FPM_HandleKey(EXTI0_IRQn, GPIOA, GPIO_Pin_0, EVENT_GROUP_SFM_USER_REG, "S1");
        if((event_value & EVENT_GROUP_KEY2_DOWN) != 0)
            FPM_HandleKey(EXTI2_IRQn, GPIOE, GPIO_Pin_2, EVENT_GROUP_SFM_USER_COMPARE, "S2");
        if((event_value & EVENT_GROUP_KEY3_DOWN) != 0)
            FPM_HandleKey(EXTI3_IRQn, GPIOE, GPIO_Pin_3, EVENT_GROUP_SFM_USER_TOTAL, "S3");
        if((event_value & EVENT_GROUP_KEY4_DOWN) != 0)
            FPM_HandleKey(EXTI4_IRQn, GPIOE, GPIO_Pin_4, EVENT_GROUP_SFM_USER_DEL_ALL, "S4");
    }
}

// 指纹操作成功后短响一声；保持原有反馈时序
static void FPM_BeepSuccess(void)
{
    beep_ON();
    vTaskDelay(pdMS_TO_TICKS(50));
    beep_OFF();
}

static void app_task_fpm(void* pvParameters)
{
	EventBits_t EventValue=0;
	uint16_t id;
	uint16_t id_total;
	uint8_t fmp_error_code;
	uint16_t servo_command = SERVO_FINGERPRINT_OPEN;

	// 初始化指纹模块串口与传感器
	fpm_init();

	dgb_printf_safe("电容指纹模块FPM383启动\r\n");

	vTaskDelay(pdMS_TO_TICKS(1000));

	for(;;)
	{
		// 等待按键任务发送操作事件，取出后清除事件位
		EventValue = xEventGroupWaitBits(g_event_group, EVENT_GROUP_SFM_ALL,
                                         pdTRUE, pdFALSE, portMAX_DELAY);

		/* 添加指纹 */
		if(EventValue & EVENT_GROUP_SFM_USER_REG)
		{
			fpm_ctrl_led(FPM_LED_BLUE);

			dgb_printf_safe("\r\n\r\n=====================================\r\n\r\n");
			dgb_printf_safe("[app_task_fpm] 执行添加指纹操作,请将手指放到指纹模块触摸感应区\r\n");

			fmp_error_code =fpm_id_total(&id_total);

			if(fmp_error_code == 0)
			{
				dgb_printf_safe("[app_task_fpm] 获取指纹总数：%04d\r\n",id_total);

				/* 添加指纹*/
				fmp_error_code=fpm_enroll_auto(id_total+1);

				if(fmp_error_code == 0)
				{
					fpm_ctrl_led(FPM_LED_GREEN);

					dgb_printf_safe("[app_task_fpm] 自动注册指纹成功\r\n");

					FPM_BeepSuccess();
				}
				else
				{
					fpm_ctrl_led(FPM_LED_RED);
				}

				vTaskDelay(pdMS_TO_TICKS(100));


				vTaskDelay(pdMS_TO_TICKS(1000));
			}

		}

		/* 刷指纹 */
		if(EventValue & EVENT_GROUP_SFM_USER_COMPARE)
		{

			fpm_ctrl_led(FPM_LED_BLUE);

			dgb_printf_safe("\r\n\r\n=====================================\r\n\r\n");
			dgb_printf_safe("[app_task_fpm] 执行刷指纹操作,请将手指放到指纹模块触摸感应区\r\n");

			/* 参数为0xFFFF进行1:N匹配 */
			id = 0xFFFF;

			fmp_error_code=fpm_idenify_auto(&id);

			if(fmp_error_code == 0)
			{
				fpm_ctrl_led(FPM_LED_GREEN);

				dgb_printf_safe("[app_task_fpm] 自动验证指纹%04d成功!\r\n",id);
				// 通知舵机转到180度，5秒后由舵机任务归位
				xQueueSend(xServoQueue, &servo_command, portMAX_DELAY);

				FPM_BeepSuccess();
			}
			else
			{
				fpm_ctrl_led(FPM_LED_RED);
			}


			vTaskDelay(pdMS_TO_TICKS(100));



			vTaskDelay(pdMS_TO_TICKS(1000));

		}

		/* 获取用户总数 */
		if(EventValue & EVENT_GROUP_SFM_USER_TOTAL)
		{
			fpm_ctrl_led(FPM_LED_BLUE);

			dgb_printf_safe("\r\n\r\n=====================================\r\n\r\n");

			fmp_error_code =fpm_id_total(&id_total);

			if(fmp_error_code == 0)
			{
				fpm_ctrl_led(FPM_LED_GREEN);

				dgb_printf_safe("[app_task_fpm] 获取指纹总数：%04d\r\n",id_total);

				FPM_BeepSuccess();
			}
			else
			{
				fpm_ctrl_led(FPM_LED_RED);
			}

			vTaskDelay(pdMS_TO_TICKS(100));


			vTaskDelay(pdMS_TO_TICKS(1000));
		}

		/* 删除所有指纹 */
		if(EventValue & EVENT_GROUP_SFM_USER_DEL_ALL)
		{
			fpm_ctrl_led(FPM_LED_BLUE);

			dgb_printf_safe("\r\n\r\n=====================================\r\n\r\n");

			fmp_error_code=fpm_empty();

			if(fmp_error_code == 0)
			{
				fpm_ctrl_led(FPM_LED_GREEN);

				dgb_printf_safe("[app_task_fpm] 清空指纹成功\r\n");

				FPM_BeepSuccess();
			}
			else
			{
				fpm_ctrl_led(FPM_LED_RED);
			}
			vTaskDelay(pdMS_TO_TICKS(100));


			vTaskDelay(pdMS_TO_TICKS(1000));
		}

	}
}


// OLED显示任务
void OLED_Task(void *arg)
{
	OLED_Message_t message;

	OLED_Init();
    OLED_Clear();
    // 上电默认显示欢迎语，按键1后切换到温湿度页面
    OLED_ShowString(24, 3, (const uint8_t *)"Welcome Home", 8);

	for( ;; )
	{
        // 阻塞等待OLED消息
		if(xQueueReceive(xOLEDQueue, &message, portMAX_DELAY) == pdPASS)
		{
            // 显示队列中的字符串
			OLED_ShowString(
				message.x,
				message.page,
				(const uint8_t *)message.text,
				message.size
			);
		}
	}
}

// DHT11采集任务
void DHT11_Task(void *arg)
{

    uint8_t result = 0;
    uint8_t humidity_alarm_active = 0;
    uint8_t temperature_alarm_active = 0;
    uint8_t alarm_triggered;
    uint8_t temperature_alarm_triggered;
    uint8_t humidity_limit;
    uint8_t min_humidity_limit;
    float temperature_limit;
    float min_temperature_limit;
    char buffer[64];

    (void)arg;

    for(;;)
    {
        vTaskDelay(pdMS_TO_TICKS(5000));
        alarm_triggered = 0;
        temperature_alarm_triggered = 0;

        result = DHT11_Read_Temp_Humi(&g_temp, &g_humi);

        // 校验失败时等待传感器总线恢复，并重读一次
        if(result == DHT11_ERROR_CHECKSUM)
        {
            vTaskDelay(pdMS_TO_TICKS(150));
            result = DHT11_Read_Temp_Humi(&g_temp, &g_humi);
        }

        if(result == DHT11_READ_OK)
        {
            sprintf(
                buffer,
                "Temp: %u C, RH: %u %%\r\n",
                (unsigned int)g_temp,
                (unsigned int)g_humi
            );

            // 湿度首次达到上限时双响，降到上限以下后允许再次报警
            humidity_limit = g_max_humidity;
            min_humidity_limit = g_min_humidity;
            if((g_humi >= humidity_limit) || (g_humi <= min_humidity_limit))
            {
                g_humidity_alarm = 1;
                if(humidity_alarm_active == 0)
                {
                    xEventGroupSetBits(xBeepEventGroup, BEEP_EVENT_DOUBLE);
                    humidity_alarm_active = 1;
                    alarm_triggered = 1;
                }
            }
            else
            {
                g_humidity_alarm = 0;
                humidity_alarm_active = 0;
            }

            // 温度首次达到上限时双响，降到上限以下后允许再次报警
            temperature_limit = g_max_temperature;
            min_temperature_limit = g_min_temperature;
            if(((float)g_temp >= temperature_limit) ||
               ((float)g_temp <= min_temperature_limit))
            {
                g_temperature_alarm = 1;
                if(temperature_alarm_active == 0)
                {
                    xEventGroupSetBits(xBeepEventGroup, BEEP_EVENT_DOUBLE);
                    temperature_alarm_active = 1;
                    temperature_alarm_triggered = 1;
                }
            }
            else
            {
                g_temperature_alarm = 0;
                temperature_alarm_active = 0;
            }

            // 只在有效采样后更新四个报警LED
            LED_SetAlarmState(g_humidity_alarm, g_temperature_alarm);
        }
        else if(result == DHT11_ERROR_ACK)
        {
            sprintf(buffer, "DHT11 ACK ERROR\r\n");
        }
        else if(result == DHT11_ERROR_CHECKSUM)
        {
            sprintf(buffer, "DHT11 CHECKSUM ERROR\r\n");
        }
        else
        {
            sprintf(buffer, "DHT11 UNKNOWN ERROR\r\n");
        }

        if(xSemaphoreTake(xSemaphore, portMAX_DELAY) == pdTRUE)
        {
            USART1_SendString(buffer);
            if(alarm_triggered != 0)
            {
                USART1_SendString("HUMIDITY ALARM\r\n");
            }
            if(temperature_alarm_triggered != 0)
            {
                USART1_SendString("TEMPERATURE ALARM\r\n");
            }
            xSemaphoreGive(xSemaphore);
        }
    }
}

void BEEP_Task(void *arg)
{
   EventBits_t beep_eventValue;
	for( ;; )
	{
        // 阻塞等待蜂鸣器事件
		beep_eventValue = xEventGroupWaitBits(
							xBeepEventGroup,
							BEEP_EVENT_SHORT | BEEP_EVENT_DOUBLE,
                            pdTRUE,         // 退出等待时清除事件位
                            pdFALSE,        // 任意一个事件位满足即可
							portMAX_DELAY
		);

        // 检查蜂鸣器事件位
		if((beep_eventValue & BEEP_EVENT_SHORT) != 0)
		{
                // 响一声
				 beep_ON();
			     vTaskDelay(pdMS_TO_TICKS(500));
			     beep_OFF();
		}
		if((beep_eventValue & BEEP_EVENT_DOUBLE) != 0)
		{
            // 响两声
			beep_ON();
			vTaskDelay(pdMS_TO_TICKS(500));
			beep_OFF();
			vTaskDelay(pdMS_TO_TICKS(500));
			beep_ON();
			vTaskDelay(pdMS_TO_TICKS(500));
			beep_OFF();
		}
	}
}


// 舵机控制任务
void Servo_Task(void *arg)
{
    uint16_t angle;
    TickType_t wait_time = portMAX_DELAY;

    (void)arg;

    for( ;; )
    {
        // 平时阻塞等待角度；指纹开门后最多等待5秒
        if(xQueueReceive(xServoQueue, &angle, wait_time) == pdPASS)
        {
            if(angle == SERVO_FINGERPRINT_OPEN)
            {
                Servo_SetAngle(180);
                wait_time = pdMS_TO_TICKS(5000);
            }
            else
            {
                // 手动角度命令取消自动归位
                Servo_SetAngle(angle);
                wait_time = portMAX_DELAY;
            }
        }
        else
        {
            // 指纹开门满5秒，回到上电时的90度
            Servo_SetAngle(90);
            wait_time = portMAX_DELAY;
        }
    }
}
// ESP8266与OneNET任务
// 将OneNET属性设置写入本地报警阈值，并输出各项结果
static void ApplyCloudAlarmThresholds(char *mqtt_payload)
{
    char setting_message[48];
    char *maxhum_position;
    char *maxtemp_position;
    char *minihum_position;
    char *minitemp_position;
    int maxhum_value;
    int minihum_value;
    float maxtemp_value;
    float minitemp_value;

    // 解析云端下发的湿度上限
    maxhum_position = strstr(mqtt_payload, "\"maxhum_set\":");
    if((maxhum_position != NULL) &&
       (sscanf(maxhum_position, "\"maxhum_set\":%d", &maxhum_value) == 1) &&
       (maxhum_value >= 0) && (maxhum_value <= 100))
    {
        g_max_humidity = (uint8_t)maxhum_value;
        sprintf(setting_message, "MAX HUMIDITY SET: %u %%\r\n",
                (unsigned int)g_max_humidity);
        if(xSemaphoreTake(xSemaphore, portMAX_DELAY) == pdTRUE)
        {
            USART1_SendString(setting_message);
            xSemaphoreGive(xSemaphore);
        }
    }

    // 解析云端下发的温度上限
    maxtemp_position = strstr(mqtt_payload, "\"maxtemp_set\":");
    if((maxtemp_position != NULL) &&
       (sscanf(maxtemp_position, "\"maxtemp_set\":%f", &maxtemp_value) == 1) &&
       (maxtemp_value >= -40.0f) && (maxtemp_value <= 200.0f))
    {
        g_max_temperature = maxtemp_value;
        sprintf(setting_message, "MAX TEMPERATURE SET: %.1f C\r\n",
                (double)g_max_temperature);
        if(xSemaphoreTake(xSemaphore, portMAX_DELAY) == pdTRUE)
        {
            USART1_SendString(setting_message);
            xSemaphoreGive(xSemaphore);
        }
    }

    // 解析云端下发的湿度下限
    minihum_position = strstr(mqtt_payload, "\"minihum_set\":");
    if((minihum_position != NULL) &&
       (sscanf(minihum_position, "\"minihum_set\":%d", &minihum_value) == 1) &&
       (minihum_value >= 0) && (minihum_value <= 100))
    {
        g_min_humidity = (uint8_t)minihum_value;
        sprintf(setting_message, "MIN HUMIDITY SET: %u %%\r\n",
                (unsigned int)g_min_humidity);
        if(xSemaphoreTake(xSemaphore, portMAX_DELAY) == pdTRUE)
        {
            USART1_SendString(setting_message);
            xSemaphoreGive(xSemaphore);
        }
    }

    // 解析云端下发的温度下限
    minitemp_position = strstr(mqtt_payload, "\"minitemp_set\":");
    if((minitemp_position != NULL) &&
       (sscanf(minitemp_position, "\"minitemp_set\":%f", &minitemp_value) == 1) &&
       (minitemp_value >= -40.0f) && (minitemp_value <= 200.0f))
    {
        g_min_temperature = minitemp_value;
        sprintf(setting_message, "MIN TEMPERATURE SET: %.1f C\r\n",
                (double)g_min_temperature);
        if(xSemaphoreTake(xSemaphore, portMAX_DELAY) == pdTRUE)
        {
            USART1_SendString(setting_message);
            xSemaphoreGive(xSemaphore);
        }
    }
}

void ESP8266_Task(void *arg)
{
	uint8_t result;
	uint8_t temperature;
	uint8_t humidity;
	char mqtt_payload[192];
	TickType_t last_publish_time;
	TickType_t last_ping_time;
	TickType_t ping_sent_time;
	TickType_t now;
	uint8_t ping_waiting;

	for( ;; )
	{
        // 每次重连都重新初始化ESP8266和连接链路
		ESP8266_Config();
	vTaskDelay(pdMS_TO_TICKS(2000));

    // 测试AT通信
	result = ESP8266_TestAT();
	if(xSemaphoreTake(xSemaphore, portMAX_DELAY) == pdTRUE)
	{
		USART1_SendString(result != 0 ? "AT OK\r\n" : "AT ERROR\r\n");
		xSemaphoreGive(xSemaphore);
	}

    // 连接WiFi
	if(result != 0)
	{
		result = ESP8266_ConnectWiFi();
		if(xSemaphoreTake(xSemaphore, portMAX_DELAY) == pdTRUE)
		{
			USART1_SendString(result != 0 ? "WIFI CONNECTED\r\n" : "WIFI CONNECT ERROR\r\n");
			xSemaphoreGive(xSemaphore);
		}
	}

    // 建立OneNET TCP连接
	if(result != 0)
	{
		result = ESP8266_ConnectTCP();
		if(xSemaphoreTake(xSemaphore, portMAX_DELAY) == pdTRUE)
		{
			USART1_SendString(result != 0 ? "TCP CONNECTED\r\n" : "TCP CONNECT ERROR\r\n");
			xSemaphoreGive(xSemaphore);
		}
	}

    // 进入TCP透传模式
	if(result != 0)
	{
		result = ESP8266_EnterTransparentMode();
		if(xSemaphoreTake(xSemaphore, portMAX_DELAY) == pdTRUE)
		{
			USART1_SendString(result != 0 ? "TRANSPARENT MODE OK\r\n" : "TRANSPARENT MODE ERROR\r\n");
			xSemaphoreGive(xSemaphore);
		}
	}

    // 发送MQTT CONNECT并等待CONNACK
	if(result != 0)
	{
		result = OneNET_MQTTConnect();
		if(xSemaphoreTake(xSemaphore, portMAX_DELAY) == pdTRUE)
		{
			USART1_SendString(result != 0 ? "MQTT CONNECTED\r\n" : "MQTT CONNECT ERROR\r\n");
			xSemaphoreGive(xSemaphore);
		}
	}

    // MQTT连接成功后订阅属性设置主题
    // 订阅后才能接收OneNET下发命令
	if(result != 0)
	{
		result = OneNET_MQTTSubscribe();
		if(xSemaphoreTake(xSemaphore, portMAX_DELAY) == pdTRUE)
		{
			USART1_SendString(result != 0 ? "MQTT SUBSCRIBED\r\n" : "MQTT SUBSCRIBE ERROR\r\n");
			xSemaphoreGive(xSemaphore);
		}
	}

		if(result == 0)
		{
			if(xSemaphoreTake(xSemaphore, portMAX_DELAY) == pdTRUE)
			{
				USART1_SendString("MQTT CONNECT FAILED, RETRY IN 5S\r\n");
				xSemaphoreGive(xSemaphore);
			}
			vTaskDelay(pdMS_TO_TICKS(5000));
			continue;
		}

    // 连接成功后立即上报一次，使设备页面显示数据
	vTaskDelay(pdMS_TO_TICKS(1000));
	temperature = g_temp;
	humidity = g_humi;
	result = OneNET_MQTTPublish(temperature, humidity);
	if(xSemaphoreTake(xSemaphore, portMAX_DELAY) == pdTRUE)
	{
		USART1_SendString(result != 0 ? "MQTT PUBLISH SENT\r\n" : "MQTT PUBLISH ERROR\r\n");
		xSemaphoreGive(xSemaphore);
	}

    // 记录本次上报时间
		last_publish_time = xTaskGetTickCount();
		last_ping_time = last_publish_time;
		ping_sent_time = 0;
		ping_waiting = 0;

    // 每50ms检查下行数据，每60秒上报一次DHT11数据
		for( ;; )
		{
			if(ESP8266_IsDisconnected() != 0)
			{
				if(xSemaphoreTake(xSemaphore, portMAX_DELAY) == pdTRUE)
				{
					USART1_SendString("MQTT CONNECTION LOST\r\n");
					xSemaphoreGive(xSemaphore);
				}
				break;
			}

			if(OneNET_MQTTReadPropertySet(mqtt_payload, sizeof(mqtt_payload)) != 0)
		{
			if(xSemaphoreTake(xSemaphore, portMAX_DELAY) == pdTRUE)
			{
				USART1_SendString("MQTT RX: ");
				USART1_SendString(mqtt_payload);
				USART1_SendString("\r\n");
				xSemaphoreGive(xSemaphore);
			}

			ApplyCloudAlarmThresholds(mqtt_payload);
			result = OneNET_MQTTReplyPropertySet(mqtt_payload);
			if(xSemaphoreTake(xSemaphore, portMAX_DELAY) == pdTRUE)
			{
				USART1_SendString(result != 0 ? "MQTT SET REPLY SENT\r\n" : "MQTT SET REPLY ERROR\r\n");
				xSemaphoreGive(xSemaphore);
			}
		}


			if(OneNET_MQTTTakePingResponse() != 0)
			{
				ping_waiting = 0;
			}

			now = xTaskGetTickCount();

			if((ping_waiting != 0) &&
			   ((now - ping_sent_time) >= pdMS_TO_TICKS(10000)))
			{
				if(xSemaphoreTake(xSemaphore, portMAX_DELAY) == pdTRUE)
				{
					USART1_SendString("MQTT PING TIMEOUT\r\n");
					xSemaphoreGive(xSemaphore);
				}
				break;
			}

			if((ping_waiting == 0) &&
			   ((now - last_ping_time) >= pdMS_TO_TICKS(30000)))
			{
				OneNET_MQTTPing();
				last_ping_time = now;
				ping_sent_time = now;
				ping_waiting = 1;
			}

			if((now - last_publish_time) >= pdMS_TO_TICKS(60000))
		{
			temperature = g_temp;
			humidity = g_humi;
				OneNET_MQTTPublish(temperature, humidity);
				last_publish_time = now;
		}

			vTaskDelay(pdMS_TO_TICKS(50));
		}

		if(xSemaphoreTake(xSemaphore, portMAX_DELAY) == pdTRUE)
		{
			USART1_SendString("MQTT RECONNECTING IN 5S\r\n");
			xSemaphoreGive(xSemaphore);
		}
		USART3_ClearRxBuffer();
		vTaskDelay(pdMS_TO_TICKS(5000));
	}
}
//创建电机任务
void Motor_Task(void *arg)
{
   uint16_t speed;
   uint8_t motor_running = 0;

	for( ;; )
	{
        // 阻塞等待电机速度
		if(xQueueReceive(xMotorQueue, &speed, portMAX_DELAY) == pdPASS)
		{
			if(speed > 0)
			{
				// 只在停止后启动时设置正转方向，升档时保持当前PWM
				if(motor_running == 0)
				{
					Motor_Control(MOTOR_FORWARD, 0);
					motor_running = 1;
				}
				Motor_SoftControl(speed, 500, 5);
			}
			else
			{
				// 逐步降速后，关闭两个方向输入
				Motor_SoftControl(0, 500, 5);
				Motor_Control(MOTOR_STOP, 0);
				motor_running = 0;
			}
		}
	}
}

// 矩阵键盘扫描任务
void Keyboard_Task(void *arg)
{

    OLED_Message_t message;
    uint16_t servo_angle;
	uint16_t Motor_speed;
	uint8_t key8_pend = 0;
	TickType_t key8_first_tick = 0;
    char key_cur;
    char key_old = 'N';
    uint8_t key_state = 0;
    uint8_t temp;
    uint8_t humi;
    uint8_t max_humi;
    uint8_t min_humi;
    uint8_t humi_alarm;
    uint8_t temp_alarm;
    float max_temp;
    float min_temp;

    uint8_t temp_mode = 0;       // 是否显示温湿度
	TickType_t last_time = 0;

    for( ;; )
    {
        key_cur = get_key_board();

        if(key_cur == 'N')
        {
            key_state = 0;
            key_old = 'N';
        }
        else if(key_state == 0)
        {
            // 首次检测到按键，保存键值用于消抖
            key_old = key_cur;
            key_state = 1;
        }
        else if((key_state == 1) && (key_cur == key_old) && (key_cur == '1') )
        {
            // 连续两次检测到按键1，开启温湿度显示
			 temp_mode = 1;
            // 等待按键释放，防止重复触发
            key_state = 2;
        }
		 else if((key_state == 1) && (key_cur == key_old) && (key_cur == '2') )
        {
            // 连续两次检测到按键2，触发单响
			xEventGroupSetBits(xBeepEventGroup,BEEP_EVENT_SHORT);
            // 等待按键释放，防止重复触发
            key_state = 2;
        }
		else if((key_state == 1) && (key_cur == key_old) && (key_cur == '3') )
        {
            // 连续两次检测到按键3，触发双响
			xEventGroupSetBits(xBeepEventGroup,BEEP_EVENT_DOUBLE);
            // 等待按键释放，防止重复触发
            key_state = 2;
        }
		else if((key_state == 1) && (key_cur == key_old) && (key_cur == '4') )
		{
            // 设置舵机为0度
			servo_angle = 0;
			xQueueSend(xServoQueue, &servo_angle, portMAX_DELAY);
			key_state = 2;
		}
		else if((key_state == 1) && (key_cur == key_old) && (key_cur == '5') )
		{
            // 设置舵机为90度
			servo_angle = 9;
			xQueueSend(xServoQueue, &servo_angle, portMAX_DELAY);
			key_state = 2;
		}
		else if((key_state == 1) && (key_cur == key_old) && (key_cur == '6') )
		{
            // 设置舵机为180度
			servo_angle = 180;
			xQueueSend(xServoQueue, &servo_angle, portMAX_DELAY);
			key_state = 2;
		}
		else if((key_state == 1) && (key_cur == key_old) && (key_cur == '7') )
		{
            // 启动电机
	        Motor_speed = 30;
			xQueueSend(xMotorQueue, &Motor_speed, portMAX_DELAY);
			key_state = 2;
		}
		else if((key_state == 1) && (key_cur == key_old) && (key_cur == '8') )
		{
			//第一次按下的时间
			TickType_t now = xTaskGetTickCount();
			if((key8_pend != 0) && ((now - key8_first_tick) <= pdMS_TO_TICKS(500)))
				{
					if(Motor_speed < 50)       Motor_speed = 50;
					else if(Motor_speed < 70)  Motor_speed = 70;
					else                      Motor_speed = 100;

					xQueueSend(xMotorQueue, &Motor_speed, portMAX_DELAY);
					key8_pend = 0;
				}
				else
				{
					key8_first_tick = now;
					key8_pend = 1;
				}
             key_state = 2;  // 必须松开，才能识别第二次按下
		}
		else if((key_state == 1) && (key_cur == key_old) && (key_cur == '9') )
		{
            // 关闭电机
	        Motor_speed = 0;
			xQueueSend(xMotorQueue, &Motor_speed, portMAX_DELAY);
			key_state = 2;
		}
        else if((key_state == 1) && (key_cur == key_old) && (key_cur == 'D'))
        {
            // 稳定性实验：停止喂狗，等待IWDG复位
            g_iwdg_feed_enabled = 0;
            if(xSemaphoreTake(xSemaphore, portMAX_DELAY) == pdTRUE)
            {
                USART1_SendString("IWDG FEED STOPPED, WAIT FOR RESET\r\n");
                xSemaphoreGive(xSemaphore);
            }
            key_state = 2;
        }
        else if((key_state == 1) && (key_cur != key_old))
        {
            key_state = 0;
            key_old = 'N';
        }

        /* 每5秒刷新一次温湿度 */
		if(temp_mode == 1)
		{
			if(xTaskGetTickCount() - last_time >= pdMS_TO_TICKS(5000))
				{
				last_time = xTaskGetTickCount();

				temp = g_temp;
                humi = g_humi;
                max_humi = g_max_humidity;
                min_humi = g_min_humidity;
                max_temp = g_max_temperature;
                min_temp = g_min_temperature;
                humi_alarm = g_humidity_alarm;
                temp_alarm = g_temperature_alarm;

				OLED_Clear();

				message.x = 1;
				message.page = 2;
				message.size = 8;
				sprintf(message.text, "Temp:%3u C     ", (unsigned int)temp);
				xQueueSend(xOLEDQueue, &message, portMAX_DELAY);

				message.page = 3;
				sprintf(message.text, "Humi:%3u %%     ", (unsigned int)humi);
				xQueueSend(xOLEDQueue, &message, portMAX_DELAY);

				message.page = 4;
				sprintf(message.text, "T:%3.0f-%3.0f C    ", (double)min_temp, (double)max_temp);
				xQueueSend(xOLEDQueue, &message, portMAX_DELAY);

				message.page = 5;
				sprintf(message.text, "H:%3u-%3u %%    ",
						(unsigned int)min_humi, (unsigned int)max_humi);
				xQueueSend(xOLEDQueue, &message, portMAX_DELAY);

				message.page = 6;
                if((temp_alarm != 0) && (humi_alarm != 0))
                {
                    sprintf(message.text, "Alarm:T+H      ");
                }
                else if(temp_alarm != 0)
                {
					xEventGroupSetBits(xBeepEventGroup,BEEP_EVENT_SHORT);
                    sprintf(message.text, "Alarm:TEMP     ");
                }
                else if(humi_alarm != 0)
                {
					xEventGroupSetBits(xBeepEventGroup,BEEP_EVENT_SHORT);
                    sprintf(message.text, "Alarm:HUMI     ");
                }
                else
                {
                    sprintf(message.text, "Alarm:OK       ");
                }
				xQueueSend(xOLEDQueue, &message, portMAX_DELAY);

				}
		}


		 vTaskDelay(pdMS_TO_TICKS(50));
	}

}



int main(void)
{
    // 初始化任务优先级最高，先建立外设、事件组和队列
    if(xTaskCreate(Task_Config, "task of config", 512, NULL, 3, NULL) != pdPASS)
    {
        for( ;; );
    }

    // 应用任务：按键和DHT11优先级为2，其余任务优先级为1
    if(xTaskCreate(Keyboard_Task, "keyboard", 512, NULL, 1, NULL) != pdPASS)
    {
        for( ;; );
    }
    if(xTaskCreate(DHT11_Task, "DHT11", 512, NULL, 2, NULL) != pdPASS)
    {
        for( ;; );
    }
    if(xTaskCreate(OLED_Task, "OLED", 512, NULL, 1, NULL) != pdPASS)
    {
        for( ;; );
    }
    if(xTaskCreate(BEEP_Task, "BEEP", 512, NULL, 1, NULL) != pdPASS)
    {
        for( ;; );
    }
    if(xTaskCreate(Servo_Task, "Servo", 512, NULL, 1, NULL) != pdPASS)
    {
        for( ;; );
    }
    if(xTaskCreate(ESP8266_Task, "WIFI", 512, NULL, 1, NULL) != pdPASS)
    {
        for( ;; );
    }
    if(xTaskCreate(IWDG_Task, "IWDG", 256, NULL, 1, NULL) != pdPASS)
    {
        for( ;; );
    }
    if(xTaskCreate(Motor_Task, "MOTOR", 256, NULL, 1, NULL) != pdPASS)
    {
        for( ;; );
    }

    // 独立按键S1～S4与指纹处理任务通过事件组通信
    if(xTaskCreate(app_task_key, "fpm key", 512, NULL, 2, NULL) != pdPASS)
    {
        for( ;; );
    }
    if(xTaskCreate(app_task_fpm, "fpm", 512, NULL, 2, NULL) != pdPASS)
    {
        for( ;; );
    }

    vTaskStartScheduler();

    // 调度器只有在内存不足等启动失败时才会返回
    for( ;; );
}
