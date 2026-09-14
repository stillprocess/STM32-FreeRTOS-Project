# STM32F407 + FreeRTOS + OneNET 智能家居学习工程

这是一个从外设驱动、FreeRTOS 任务到 OneNET 数据交互逐步复刻的 STM32F407 工程。固件使用 STM32 标准外设库和 Keil MDK；`vue-frontend/` 是 Vue 3 + Vite 仪表盘。

## 已实现功能

- DHT11 采集温湿度，通过 USART1、OLED 和 MQTT 上报展示。
- OneNET 下发温湿度上下限；越界时蜂鸣器报警。湿度报警点亮 LED1/LED2，温度报警点亮 LED3/LED4，上电时四灯熄灭。
- 独立按键 S1～S4 对应指纹录入、识别、查询数量、清空。识别成功后舵机转到 180°，5 秒后回到 90°。
- 矩阵键盘向电机和显示任务发送操作；独立看门狗监测任务运行。
- ESP8266 通过 AT 指令连接 Wi-Fi 与 OneNET，使用 MQTT 上报、接收属性设置并在连接丢失后重连。
- Vue 仪表盘显示温湿度、阈值与历史曲线，并支持阈值设置；详见 [前端说明](vue-frontend/README.md)。

## 工程结构

| 路径 | 内容 |
| --- | --- |
| `USER/main.c` | 初始化、FreeRTOS 任务、队列和事件组 |
| `HARDWARE/` | LED、DHT11、OLED、按键、指纹、舵机、电机、ESP8266 和 MQTT 驱动 |
| `FreeRTOS_SOURCE/`、`FreeRTOS_Portable/`、`FreeRTOS_INCLUDE/` | FreeRTOS 内核与移植代码 |
| `CMSIS/`、`DEVICE_LIB/`、`SYSTEM/` | STM32 启动、标准外设库和系统代码 |
| `USER/project.uvprojx` | Keil 工程文件 |
| `vue-frontend/` | Vue 仪表盘与本地 OneNET 代理 |

数据流：`DHT11_Task → 温湿度与报警状态 → LED/蜂鸣器、OLED、ESP8266_Task → OneNET`。指纹按键经 EXTI 和事件组通知指纹任务，验证成功后通过队列通知舵机任务。

## 主要连接

| 模块 | STM32 引脚或外设 |
| --- | --- |
| DHT11 数据 | PG9 |
| LED1/LED2（湿度） | PF9/PF10，低电平点亮 |
| LED3/LED4（温度） | PE13/PE14，低电平点亮 |
| 舵机 PWM | PC9，TIM3_CH4 |
| 电机驱动 | PD15/PE7 方向，PE6 TIM9_CH2 PWM，PE12 STBY |
| 独立指纹按键 S1～S4 | PA0、PE2、PE3、PE4 |

接线与电平应以自己的板卡和 `HARDWARE/` 中的初始化代码为准。

## 构建与运行

1. 复制 `HARDWARE/secrets.example.h` 为 `HARDWARE/secrets.h`，填入 Wi-Fi SSID、密码和 OneNET 设备 Token。`secrets.h` 被 Git 忽略。
2. 用 Keil MDK 打开 `USER/project.uvprojx`，选择 `project` 目标并编译、下载到 STM32F407ZETx。
3. 按实际接线连接 DHT11、ESP8266、指纹模块、OLED、舵机、电机和按键。USART1 输出调试信息。
4. 前端进入 `vue-frontend/`，将 `.env.example` 复制为 `.env`，填写 OneNET 用户 ID、Access Key、产品 ID 和设备名称，然后执行 `npm install`、`npm run dev`。

OneNET 物模型标识符及前端构建方式见 [前端说明](vue-frontend/README.md)。

