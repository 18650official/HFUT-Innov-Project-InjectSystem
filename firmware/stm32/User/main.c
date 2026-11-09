#include "stm32f10x.h"                  // Device header
#include "Delay.h"
#include "OLED.h"
#include "Serial.h"
#include "L298N.h"
#include "CountSensor.h" //红外对射传感器
#include "Timer.h"
#include <ctype.h>

// 引脚定义
#define COUNTER PA_11
#define TXD PA_9
#define RXD PA_10
#define LED_PIN PB_1
#define BUZ_PIN PB_11
#define IN1 PB_15
#define IN2 PB_14
#define IN3 PB_13
#define IN4 PB_12

#define DIR_OFF 0
#define DIR_ZHENG 1
#define DIR_FAN 2

void buzzer_init(void);
void beep(int t);
void pump_set(uint8_t direct, uint16_t speed);
void valve_set(uint8_t direct, uint16_t speed);
void infusion_loop(void);
int string_to_integer(const char *str);


uint16_t machineNumber = 0x01; //设备编号
uint8_t liquidState = 0; //管内的液体状态
uint16_t liquidRate; //每分钟液体的流量
uint8_t infusion_state = 0; //输液状态机
char isStrEqual(char* s1, char* s2); //判断字符串是否相等


uint8_t pump_direct = DIR_OFF; //水泵方向
uint16_t pump_speed = 50; //水泵运行速度
uint8_t valve_direct = DIR_OFF; //阀门状态
uint16_t valve_power = 100; //阀门的闭合度
int speed_required = 80; //预设速度，默认速度为80

//从传感器读取的实时数据变量
uint16_t cur_time = 0;
uint16_t count_per_min = 0;

int main(void)
{
	/*模块初始化*/
	OLED_Init();		//OLED初始化
	buzzer_init();  //初始化蜂鸣器
	CountSensor_Init(); //初始化流速传感器
	L298N_Init(); //初始化电机驱动器
	Serial_Init(); //初始化串口
	Tim4_Init(); //初始化定时器4，实现心跳包的发送和电机状态的更新
	
	GPIO_ResetBits(GPIOB, GPIO_Pin_1);
	beep(100);
	GPIO_SetBits(GPIOB, GPIO_Pin_1); 
	Serial_SendString("Start!\nPowered by Snowmiku\n");
	
	/*显示静态字符串*/
	OLED_ShowString(1, 1, "Device:");
	OLED_ShowNum(1, 9, machineNumber, 2); // 显示设备编号
	OLED_ShowString(2, 1, "Avg:");
	OLED_ShowNum(2, 6, 0, 4);
	OLED_ShowString(3, 1, "Spd:");
	OLED_ShowNum(2, 6, 0, 3);
	
	while (1)
	{
		//读取数据
		cur_time = CounterSensor_GetAvgValue();
		count_per_min = CountSensor_GetCountPerMinute();

		//展示数据
		OLED_ShowNum(2, 6, cur_time, 4);
		OLED_ShowNum(3, 6, count_per_min, 3); //显示流速

		//进入马达控制逻辑
		infusion_loop(); //执行输液逻辑
	}
}

void buzzer_init(void)
{
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
	//Enable the buzzer
	GPIO_InitTypeDef GPIO_InitStructure;
 	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11;
 	GPIO_Init(GPIOB, &GPIO_InitStructure);
	//Enable the LED Pin
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
 	GPIO_Init(GPIOB, &GPIO_InitStructure);

	GPIO_SetBits(GPIOB, GPIO_Pin_11); 
	GPIO_SetBits(GPIOB, GPIO_Pin_1); //关闭LED
}

void beep(int t)
{
	GPIO_ResetBits(GPIOB, GPIO_Pin_11);
	Delay_ms(t);
	GPIO_SetBits(GPIOB, GPIO_Pin_11); 
}

void pump_set(uint8_t direct, uint16_t speed)
{
	pump_direct = direct;
	pump_speed = speed;
	//设置阀门和水泵的方向和速度
	L298N_ConfigRotateDirect(pump_direct, valve_direct);
	L298N_SetCompare(pump_speed, valve_power);
}

void valve_set(uint8_t direct, uint16_t speed)
{
	valve_direct = direct;
	valve_power = speed;
	//设置阀门和水泵的方向和速度
	L298N_ConfigRotateDirect(pump_direct, valve_direct);
	L298N_SetCompare(pump_speed, valve_power);
}

char isStrEqual(char* s1, char* s2) //判断字符串是否相等
{
	char result = 0;
	int index = 1;
	while(s1[index])
	{
		if(s1[index] == s2[index])
		{
			index++;
		}
		else return result;
	}
	result = 1;
	return result;
}

/**
 * 将字符串解析为整数
 * @param str - 包含数字字符的字符串
 * @return - 解析后的整数值
 */

int string_to_integer(const char *str) 
{
    if (str == NULL || *str == '\0') {
        return 0; // 空指针或空字符串处理
    }

    int sign = 1;
    int result = 0;
    size_t i = 0;

    // 检查可选的正负号
    if (str[i] == '-') {
        sign = -1;
        i++;
    } else if (str[i] == '+') {
        i++;
    }

    // 遍历字符串中的每个字符
    while (str[i] != '\0') {
        // 检查是否为有效数字
        if (!isdigit((unsigned char)str[i])) {
            return 0; // 如果遇到非数字字符，则返回错误
        }
        
        // 计算整数值
        result = result * 10 + (str[i] - '0');
        i++;
    }

    return result * sign;
}

void infusion_loop(void)
{
	int receive_integer_temp; 

	if(Serial_RxFlag == 1) //处理串口输入
	{
		Serial_RxFlag = 0;
		beep(50);
		Delay_ms(10);
		if(isStrEqual(Serial_RxPacket, "switch"))
		{
			beep(100);
			infusion_state = (infusion_state == 0 ? 1 : 0); //切换输液状态
			if(infusion_state == 0) //关断
			{
				pump_set(DIR_OFF, 0);
				valve_set(DIR_ZHENG, 80);
			}
			else //打开阀门并开启电机，设置初始转速
			{
				pump_set(DIR_ZHENG, 30);
				valve_set(DIR_OFF, 0);
			}
			Serial_Printf("State:%d", infusion_state); //回传状态
		}
		if(isStrEqual(Serial_RxPacket, "set_speed")) //接收液体滴速的输入
		{
			beep(100);
			Serial_Printf("Please input the speed(20 - 400):");
			while(1)
			{
				if(Serial_RxFlag == 1) //处理串口输入
				{
					Serial_RxFlag = 0;
					receive_integer_temp = string_to_integer(Serial_RxPacket); //读取代表输液速度的整数值
					if(receive_integer_temp < 20 || receive_integer_temp > 400){
						Serial_Printf("Input error!"); 
						return;
					}
					else{
						speed_required = receive_integer_temp;
						Serial_Printf("The speed is adjusted successfully:%d\n", speed_required); 
						beep(200);
						break;
					}
				}
			}
		}
	}
	
	//当检测到液体滴落，发出声音提醒
	if(CounterSensor_GetStat() == 1){
			beep(100);
	}

	//根据输液是否打开的状态，执行逻辑
	if(infusion_state == 0)
	{
		//停止输液
		pump_set(DIR_OFF, 0);
		valve_set(DIR_ZHENG, 80);
	}
	else if(infusion_state == 1)
	{
		//每5个心跳节拍，对电机状态进行更新 - 500ms
		if(tim1_heartbeats >= 10){
			tim1_heartbeats = 0;
			if(speed_required > count_per_min)
			{
				pump_speed += 2;
				if(pump_speed >= 100) pump_speed = 100; //如果实际速度小于预设速度，则给水泵加速
				pump_set(pump_direct, pump_speed);
			}
			else if(speed_required < count_per_min)
			{
				pump_speed -= 2;
				if(pump_speed <= 20) pump_speed = 20;
				pump_set(pump_direct, pump_speed);
			}
			//将当前的数据发送到电脑
			Serial_Printf("----------Info------------\n");
			Serial_Printf("Count:%d, Pump:%d, Valve:%d\n", count_per_min, pump_speed, valve_power);
			Serial_Printf("---------------------\n");
		}
	}
}
