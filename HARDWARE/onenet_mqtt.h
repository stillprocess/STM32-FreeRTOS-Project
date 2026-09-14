#ifndef _ONENET_MQTT_H
#define _ONENET_MQTT_H

#include "stm32f4xx.h"

uint8_t OneNET_MQTTConnect(void);
uint8_t OneNET_MQTTPublish(uint8_t temperature, uint8_t humidity);
uint8_t OneNET_MQTTSubscribe(void);
uint8_t OneNET_MQTTReadPropertySet(char *payload, uint16_t payload_size);
uint8_t OneNET_MQTTReplyPropertySet(const char *request_payload);
void OneNET_MQTTPing(void);
uint8_t OneNET_MQTTTakePingResponse(void);

#endif
