#include "esp8266.h"
#include "uart.h"
#include "FreeRTOS.h"
#include "task.h"
#include "stdio.h"
#include "secrets.h"
#include <string.h>

#define MQTTCOM         "Kb6mt1xhD7.mqtts.acc.cmcconenet.cn"
#define MQTTPORT         1883

//在USART3接收计数范围内查找AT回复或断线提示

static uint8_t ESP8266_BufferContains(const char *text)
{
    uint32_t count;
    uint32_t text_length;
    uint32_t i;
    uint32_t j;

    count = u3_recvcnt;
    text_length = (uint32_t)strlen(text);
    if((text_length == 0U) || (count < text_length))
    {
        return 0;
    }

    for(i = 0; i + text_length <= count; i++)
    {
        for(j = 0; j < text_length; j++)
        {
            if(u3_recvbuf[i + j] != (uint8_t)text[j])
            {
                break;
            }
        }
        if(j == text_length)
        {
            return 1;
        }
    }

    return 0;
}

static uint8_t ESP8266_ResponseContains(const char *text)
{
    return ESP8266_BufferContains(text);
}

uint8_t ESP8266_IsDisconnected(void)
{
    if((ESP8266_BufferContains("CLOSED") != 0) ||
       (ESP8266_BufferContains("WIFI DISCONNECT") != 0) ||
       (ESP8266_BufferContains("SEND FAIL") != 0) ||
       (ESP8266_BufferContains("link is not valid") != 0))
    {
        return 1;
    }

    return 0;
}

//发送AT指令并等待指定回复
static uint8_t ESP8266_SendCommand(const char *command, const char *ack, uint32_t timeout_ms)
{
	TickType_t start_time;

	USART3_ClearRxBuffer();
	USART3_SendString(command);
	start_time = xTaskGetTickCount();

	while((xTaskGetTickCount() - start_time) < pdMS_TO_TICKS(timeout_ms))
	{
		if(ESP8266_ResponseContains(ack) != 0)
		{
			return 1;
		}

		if((ESP8266_ResponseContains("ERROR") != 0) ||
		   (ESP8266_ResponseContains("FAIL") != 0))
		{
			return 0;
		}

		vTaskDelay(pdMS_TO_TICKS(10));
	}

	return 0;
}

//初始化ESP8266使能引脚和通信串口
void ESP8266_Config(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	//开启GPIOB时钟
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);

	//配置PB6为ESP8266使能输出
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
	GPIO_Init(GPIOB, &GPIO_InitStructure);
	//拉低使能引脚复位ESP8266
	GPIO_ResetBits(GPIOB, GPIO_Pin_6);
	vTaskDelay(pdMS_TO_TICKS(200));

	//重新使能ESP8266并等待启动
	GPIO_SetBits(GPIOB, GPIO_Pin_6);
	vTaskDelay(pdMS_TO_TICKS(2000));

	//初始化ESP8266通信串口
	USART3_Config(115200);
}

//测试ESP8266是否响应AT指令
uint8_t ESP8266_TestAT(void)
{
	return ESP8266_SendCommand("AT\r\n", "OK", 1000);
}

//设置STA模式并连接WiFi
uint8_t ESP8266_ConnectWiFi(void)
{
	char command[96];

	if(ESP8266_SendCommand("ATE0\r\n", "OK", 1000) == 0)
	{
		return 0;
	}

	if(ESP8266_SendCommand("AT+CWMODE=1\r\n", "OK", 2000) == 0)
	{
		return 0;
	}

	sprintf(command, "AT+CWJAP=\"%s\",\"%s\"\r\n", WIFI_SSID, WIFI_PASSWORD);
	if(ESP8266_SendCommand(command, "OK", 20000) == 0)
	{
		return 0;
	}

	return 1;
}


//WiFi模式下连接服务器
uint8_t ESP8266_ConnectTCP(void)
{
	char command[96];
	TickType_t start_time;

	//透传模式只支持单连接
	if(ESP8266_SendCommand("AT+CIPMUX=0\r\n", "OK", 2000) == 0)
	{
		return 0;
	}

	//端口参数是数字，不能添加双引号
	sprintf(command, "AT+CIPSTART=\"TCP\",\"%s\",%d\r\n", MQTTCOM, MQTTPORT);

	//一条连接指令只发送一次
	USART3_ClearRxBuffer();
	USART3_SendString(command);
	start_time = xTaskGetTickCount();

	while((xTaskGetTickCount() - start_time) < pdMS_TO_TICKS(20000))
	{
		//先判断失败信息，避免把CONNECT FAIL判断为连接成功
		if((ESP8266_ResponseContains("ERROR") != 0) ||
		   (ESP8266_ResponseContains("FAIL") != 0) ||
		   (ESP8266_ResponseContains("CLOSED") != 0))
		{
			return 0;
		}

		//CONNECT和ALREADY CONNECTED都包含CONNECT
		if(ESP8266_ResponseContains("CONNECT") != 0)
		{
			return 1;
		}

		vTaskDelay(pdMS_TO_TICKS(10));
	}

	return 0;
}

//发送指定长度的二进制数据
void ESP8266_SendBytes(const uint8_t *data, uint16_t length)
{
	uint16_t i;

	for(i = 0; i < length; i++)
	{
		USART_SendData(USART3, data[i]);
		while(USART_GetFlagStatus(USART3, USART_FLAG_TXE) == RESET);
	}
}

//进入ESP8266 TCP透传模式
uint8_t ESP8266_EnterTransparentMode(void)
{
	if(ESP8266_SendCommand("AT+CIPMODE=1\r\n", "OK", 2000) == 0)
	{
		return 0;
	}

	if(ESP8266_SendCommand("AT+CIPSEND\r\n", ">", 3000) == 0)
	{
		return 0;
	}

	return 1;
}
