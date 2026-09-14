#include "dht11.h"
#include "delay.h"


//DHT11数据引脚输出模式配置
void DHT11_PinOutputModeConfig(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	//使能GPIOG时钟
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOG, ENABLE);

	//配置PG9为开漏输出
	GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_OUT;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_OD;
	GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_UP;
	GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_9;
	GPIO_Init(GPIOG, &GPIO_InitStructure);

	//释放DHT11数据总线
	GPIO_SetBits(GPIOG, GPIO_Pin_9);
}

//DHT11引脚输入配置
void DHT11_PinInputModeConfig(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOG, ENABLE);

	GPIO_InitStructure.GPIO_Mode 	= GPIO_Mode_IN;
	GPIO_InitStructure.GPIO_Speed 	= GPIO_Speed_100MHz;
	GPIO_InitStructure.GPIO_PuPd 	= GPIO_PuPd_UP;
	GPIO_InitStructure.GPIO_Pin 	= GPIO_Pin_9;
	GPIO_Init(GPIOG, &GPIO_InitStructure);
}

//向DHT11发送开始信号
void DHT11_SendStartSig(void)
{
	//配置PG9为输出模式
	DHT11_PinOutputModeConfig();

	//拉低总线至少18毫秒
	GPIO_ResetBits(GPIOG, GPIO_Pin_9);
	delay_ms(20);

	//释放总线
	GPIO_SetBits(GPIOG, GPIO_Pin_9);

	//等待DHT11准备应答
	delay_us(30);

	//切换为输入模式，接收DHT11应答
	DHT11_PinInputModeConfig();
}


//判断DHT11是否应答
bool DHT11_IsACK(void)
{
	uint8_t cnt = 0;

	//等待DHT11拉低数据总线
	while(GPIO_ReadInputDataBit(GPIOG, GPIO_Pin_9) == 1)
	{
		//等待低电平超时
		if(++cnt > 100)
		{
			return false;
		}

		delay_us(1);
	}

	cnt = 0;

	//等待DHT11结束应答低电平
	while(GPIO_ReadInputDataBit(GPIOG, GPIO_Pin_9) == 0)
	{
		//低电平持续时间异常
		if(++cnt > 100)
		{
			return false;
		}

		delay_us(1);
	}

	//已经检测到DHT11应答
	return true;
}

//初始化DHT11
void DHT11_Config(void)
{
	DHT11_PinInputModeConfig();   // 默认状态：上拉输入，空闲高电平
}


//读取一个字节（数据位：高位在前 MSB）
uint8_t DHT11_ReadByte(void)
{
	uint8_t i = 0;
	uint8_t dat = 0;
	uint8_t cnt = 0;

	for(i = 0; i < 8 ; i++)
	 {
		 cnt = 0;

		//1.等待上一个位结束（等待50us的低电平结束）,判断是否超时
		 while( GPIO_ReadInputDataBit(GPIOG, GPIO_Pin_9) == 1 )
			 {
			if(++cnt > 100)
				return false;
				delay_us(1);
			 }

	     cnt = 0;

		 while( GPIO_ReadInputDataBit(GPIOG, GPIO_Pin_9) == 0 )
		 {
		if(++cnt > 100)
			return false;
		    delay_us(1);
	     }

	      cnt = 0;

	 	 //2.现在引脚为高电平，开始计算高电平持续的时间
		 while( GPIO_ReadInputDataBit(GPIOG, GPIO_Pin_9) == 1 )
		 {
			if(++cnt > 100)
			   return false;  //超时保护 (正常高电平是26us~70us)
			delay_us(1);
	      }

		  // 判断时间：数据“0”高电平约26us~28us；数据“1”高电平约70us
          // 设置40us为分界线，大于40为1，小于40为0
		 if( cnt > 40 )
          {
            dat |= (uint8_t)(1 << (7 - i)); // 高位在前
          }

	 }

		 return dat;
}

//发送起始信号、接收5字节并校验；失败时不更新输出参数
uint8_t DHT11_Read_Temp_Humi(uint8_t *temp, uint8_t *humi)
{

	uint8_t buf[5];
    uint8_t i;
    uint16_t cnt = 0;

	//1.主机发送开始信号
      DHT11_SendStartSig();

	//2.判断DHT11是否响应
       if(DHT11_IsACK() == 0)
	   {
		   return DHT11_ERROR_ACK;
	   }

	//3.循环读取5个字节
	for(i=0; i<5; i++)
		{
			buf[i] = DHT11_ReadByte();
		}

	//4.等待DHT11释放总线（标准在最后会有50us低电平，然后释放）
    while( GPIO_ReadInputDataBit(GPIOG, GPIO_Pin_9) == 0 )
    {
        if(++cnt > 100) break;
        delay_us(1);
    }
    DHT11_PinInputModeConfig(); // 最后确保总线处于释放状态

	//5.校验数据 (前4字节之和等于第5字节)
    if( (uint8_t)(buf[0] + buf[1] + buf[2] + buf[3]) != buf[4] )
    {
        return DHT11_ERROR_CHECKSUM;
    }

        *humi = buf[0]; // 湿度整数
        *temp = buf[2]; // 温度整数
       return DHT11_READ_OK;

}


