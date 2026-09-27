#include "onenet_mqtt.h"
#include "esp8266.h"
#include "uart.h"
#include "FreeRTOS.h"
#include "task.h"
#include "string.h"
#include "stdio.h"
#include "secrets.h"


static volatile uint8_t g_mqtt_ping_response = 0;

//向MQTT缓冲区写入带两字节长度的字符串
static uint16_t MQTT_WriteString(uint8_t *buffer, uint16_t position, const char *text)
{
	uint16_t length;

	length = (uint16_t)strlen(text);
	buffer[position++] = (uint8_t)(length >> 8);
	buffer[position++] = (uint8_t)length;
	memcpy(&buffer[position], text, length);

	return position + length;
}

//等待OneNET返回MQTT CONNACK
static uint8_t MQTT_WaitConnAck(uint32_t timeout_ms)
{
	TickType_t start_time;
	uint32_t count;
	uint32_t i;

	start_time = xTaskGetTickCount();

	while((xTaskGetTickCount() - start_time) < pdMS_TO_TICKS(timeout_ms))
	{
		count = u3_recvcnt;
		for(i = 0; i + 3 < count; i++)
		{
			if((u3_recvbuf[i] == 0x20) &&
			   (u3_recvbuf[i + 1] == 0x02) &&
			   (u3_recvbuf[i + 2] == 0x00))
			{
				return u3_recvbuf[i + 3] == 0x00 ? 1 : 0;
			}
		}

		vTaskDelay(pdMS_TO_TICKS(10));
	}

	return 0;
}

//构造并发送MQTT 3.1.1 CONNECT报文
uint8_t OneNET_MQTTConnect(void)
{
	static uint8_t body[384];
	static uint8_t packet[400];
	uint16_t body_length;
	uint16_t packet_length;
	uint32_t remaining_length;
	uint8_t encoded_byte;

	if(strlen(ONENET_DEVICE_NAME) + strlen(ONENET_PRODUCT_ID) +
	   strlen(ONENET_DEVICE_TOKEN) + 16U > sizeof(body))
	{
		return 0;
	}

	body_length = 0;

	//MQTT协议名称和版本
	body[body_length++] = 0x00;
	body[body_length++] = 0x04;
	body[body_length++] = 'M';
	body[body_length++] = 'Q';
	body[body_length++] = 'T';
	body[body_length++] = 'T';
	body[body_length++] = 0x04;

	//用户名、密码、清理会话标志和120秒保活时间
	body[body_length++] = 0xC2;
	body[body_length++] = 0x00;
	body[body_length++] = 0x78;

	//CONNECT载荷顺序：Client ID、Username、Password
	body_length = MQTT_WriteString(body, body_length, ONENET_DEVICE_NAME);
	body_length = MQTT_WriteString(body, body_length, ONENET_PRODUCT_ID);
	body_length = MQTT_WriteString(body, body_length, ONENET_DEVICE_TOKEN);

	packet_length = 0;
	packet[packet_length++] = 0x10;
	remaining_length = body_length;

	//MQTT剩余长度使用变长编码
	do
	{
		encoded_byte = (uint8_t)(remaining_length % 128);
		remaining_length /= 128;
		if(remaining_length > 0)
		{
			encoded_byte |= 0x80;
		}
		packet[packet_length++] = encoded_byte;
	}
	while(remaining_length > 0);

	if(packet_length + body_length > sizeof(packet))
	{
		return 0;
	}

	memcpy(&packet[packet_length], body, body_length);
	packet_length += body_length;

	USART3_ClearRxBuffer();
	ESP8266_SendBytes(packet, packet_length);

	return MQTT_WaitConnAck(5000);
}



//等待服务器返回SUBACK，消息标识符固定为1
static uint8_t MQTT_WaitSubAck(uint32_t timeout_ms)
{
	TickType_t start_time;
	uint32_t count;
	uint32_t i;

	start_time = xTaskGetTickCount();
	while((xTaskGetTickCount() - start_time) < pdMS_TO_TICKS(timeout_ms))
	{
		count = u3_recvcnt;
		for(i = 0; i + 4 < count; i++)
		{
			if((u3_recvbuf[i] == 0x90) &&
			   (u3_recvbuf[i + 1] == 0x03) &&
			   (u3_recvbuf[i + 2] == 0x00) &&
			   (u3_recvbuf[i + 3] == 0x01))
			{
				if(u3_recvbuf[i + 4] != 0x80)
				{
					USART3_ClearRxBuffer();
					return 1;
				}
				USART3_ClearRxBuffer();
				return 0;
			}
		}
		vTaskDelay(pdMS_TO_TICKS(10));
	}
	return 0;
}

//订阅OneNET属性设置主题
uint8_t OneNET_MQTTSubscribe(void)
{
	uint8_t packet[128];
	char topic[96];
	uint16_t topic_length;
	uint16_t packet_length;
	uint32_t remaining_length;
	uint32_t value;
	uint8_t encoded_byte;

	if(snprintf(topic, sizeof(topic), "$sys/%s/%s/thing/property/set", ONENET_PRODUCT_ID, ONENET_DEVICE_NAME) >= sizeof(topic))
	{
		return 0;
	}
	topic_length = (uint16_t)strlen(topic);
	remaining_length = 2U + 2U + topic_length + 1U;

	if(remaining_length + 5U > sizeof(packet))
	{
		return 0;
	}

	packet_length = 0;
	packet[packet_length++] = 0x82;
	value = remaining_length;
	do
	{
		encoded_byte = (uint8_t)(value % 128U);
		value /= 128U;
		if(value > 0U)
		{
			encoded_byte |= 0x80U;
		}
		packet[packet_length++] = encoded_byte;
	}
	while(value > 0U);

	packet[packet_length++] = 0x00;
	packet[packet_length++] = 0x01;
	packet[packet_length++] = (uint8_t)(topic_length >> 8);
	packet[packet_length++] = (uint8_t)topic_length;
	memcpy(&packet[packet_length], topic, topic_length);
	packet_length += topic_length;
	packet[packet_length++] = 0x00;

	USART3_ClearRxBuffer();
	ESP8266_SendBytes(packet, packet_length);
	return MQTT_WaitSubAck(5000);
}

//解析OneNET下发的属性设置PUBLISH，只返回JSON负载
uint8_t OneNET_MQTTReadPropertySet(char *payload, uint16_t payload_size)
{
	uint32_t count;
	uint32_t i;
	uint32_t pos;
	uint32_t multiplier;
	uint32_t remaining_length;
	uint32_t frame_end;
	uint32_t payload_length;
	uint16_t topic_length;
	uint16_t copy_length;
	uint8_t encoded_byte;
	char topic[96];

	if((payload == NULL) || (payload_size == 0U))
	{
		return 0;
	}

	count = u3_recvcnt;
	for(i = 0; i < count; i++)
	{
		if((u3_recvbuf[i] & 0xF0U) == 0x30U)
		{
			pos = i + 1U;
			multiplier = 1U;
			remaining_length = 0U;
			do
			{
				if(pos >= count || multiplier > 2097152U)
				{
					return 0;
				}
				encoded_byte = u3_recvbuf[pos++];
				remaining_length += (encoded_byte & 0x7FU) * multiplier;
				multiplier *= 128U;
			}
			while((encoded_byte & 0x80U) != 0U);

			if(remaining_length > count - pos)
			{
				return 0;
			}
			frame_end = pos + remaining_length;
			if(pos + 2U > frame_end)
			{
				return 0;
			}

			topic_length = (uint16_t)(((uint16_t)u3_recvbuf[pos] << 8) |
									 u3_recvbuf[pos + 1U]);
			pos += 2U;
			if((pos + topic_length > frame_end) || (topic_length >= sizeof(topic)))
			{
				USART3_ClearRxBuffer();
				return 0;
			}

			for(copy_length = 0; copy_length < topic_length; copy_length++)
			{
				topic[copy_length] = (char)u3_recvbuf[pos + copy_length];
			}
			topic[topic_length] = '\0';
			pos += topic_length;

			if((u3_recvbuf[i] & 0x06U) != 0U)
			{
				if(pos + 2U > frame_end)
				{
					USART3_ClearRxBuffer();
					return 0;
				}
				pos += 2U;
			}

			payload_length = frame_end - pos;
			if(strstr(topic, "/property/set") != NULL)
			{
				if(payload_length >= payload_size)
				{
					USART3_DiscardRxBytes(frame_end);
					return 0;
				}
				copy_length = (uint16_t)payload_length;
				for(i = 0; i < copy_length; i++)
				{
					payload[i] = (char)u3_recvbuf[pos + i];
				}
				payload[copy_length] = '\0';
				USART3_DiscardRxBytes(frame_end);
				return 1;
			}

			USART3_DiscardRxBytes(frame_end);
			return 0;
		}
		else if((u3_recvbuf[i] == 0xD0U) && (i + 1U < count) &&
				(u3_recvbuf[i + 1U] == 0x00U))
		{
			g_mqtt_ping_response = 1;
			USART3_DiscardRxBytes(i + 2U);
			return 0;
		}
	}

	if(count > 480U)
	{
		USART3_ClearRxBuffer();
	}
	return 0;
}

//构造并发送OneNET属性上报报文，使用QoS 0
static uint8_t MQTT_SendPublishPacket(const char *topic, const char *payload)
{
	static uint8_t packet[256];
	uint16_t topic_length;
	uint16_t payload_length;
	uint16_t packet_length;
	uint32_t remaining_length;
	uint32_t value;
	uint8_t encoded_byte;

	topic_length = (uint16_t)strlen(topic);
	payload_length = (uint16_t)strlen(payload);
	remaining_length = 2U + topic_length + payload_length;
	if((remaining_length + 5U) > sizeof(packet))
	{
		return 0;
	}

	packet_length = 0;
	packet[packet_length++] = 0x30;
	value = remaining_length;
	do
	{
		encoded_byte = (uint8_t)(value % 128U);
		value /= 128U;
		if(value > 0U)
		{
			encoded_byte |= 0x80U;
		}
		packet[packet_length++] = encoded_byte;
	}
	while(value > 0U);

	packet[packet_length++] = (uint8_t)(topic_length >> 8);
	packet[packet_length++] = (uint8_t)topic_length;
	memcpy(&packet[packet_length], topic, topic_length);
	packet_length += topic_length;
	memcpy(&packet[packet_length], payload, payload_length);
	packet_length += payload_length;

	ESP8266_SendBytes(packet, packet_length);
	return 1;
}

//构造并发送OneNET属性上报报文，使用QoS 0
uint8_t OneNET_MQTTPublish(uint8_t temperature, uint8_t humidity)
{
	char topic[96];
	char payload[160];

	if(snprintf(topic, sizeof(topic), "$sys/%s/%s/thing/property/post", ONENET_PRODUCT_ID, ONENET_DEVICE_NAME) >= sizeof(topic))
	{
		return 0;
	}
	if(snprintf(payload, sizeof(payload),
			"{\"id\":\"%lu\",\"version\":\"1.0\","
			"\"params\":{\"temp_value\":{\"value\":%u},"
			"\"humidity_value\":{\"value\":%u}},"
			"\"method\":\"thing.event.property.post\"}",
			(unsigned long)xTaskGetTickCount(),
			(unsigned int)temperature,
			(unsigned int)humidity) >= sizeof(payload))
	{
		return 0;
	}

	return MQTT_SendPublishPacket(topic, payload);
}

//回复OneNET属性设置请求，回复中的id必须与下发请求一致
uint8_t OneNET_MQTTReplyPropertySet(const char *request_payload)
{
	const char *id_start;
	const char *id_end;
	char request_id[48];
	char topic[96];
	char reply[112];
	uint16_t id_length;

	id_start = strstr(request_payload, "\"id\":\"");
	if(id_start == NULL)
	{
		return 0;
	}
	id_start += 6;
	id_end = strchr(id_start, '"');
	if(id_end == NULL)
	{
		return 0;
	}

	id_length = (uint16_t)(id_end - id_start);
	if((id_length == 0U) || (id_length >= sizeof(request_id)))
	{
		return 0;
	}
	memcpy(request_id, id_start, id_length);
	request_id[id_length] = '\0';

	if(snprintf(topic, sizeof(topic), "$sys/%s/%s/thing/property/set_reply", ONENET_PRODUCT_ID, ONENET_DEVICE_NAME) >= sizeof(topic))
	{
		return 0;
	}
	if(snprintf(reply, sizeof(reply), "{\"id\":\"%s\",\"code\":200,\"msg\":\"success\"}", request_id) >= sizeof(reply))
	{
		return 0;
	}

	return MQTT_SendPublishPacket(topic, reply);
}

//发送MQTT心跳报文
void OneNET_MQTTPing(void)
{
	uint8_t packet[2];

	packet[0] = 0xC0;
	packet[1] = 0x00;
	g_mqtt_ping_response = 0;
	ESP8266_SendBytes(packet, 2);
}

uint8_t OneNET_MQTTTakePingResponse(void)
{
	uint8_t received;

	received = g_mqtt_ping_response;
	g_mqtt_ping_response = 0;
	return received;
}
