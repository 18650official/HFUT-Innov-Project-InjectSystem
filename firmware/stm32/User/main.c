#include "stm32f10x.h"                  // Device header
#include "Delay.h"
#include "OLED.h"
#include "Serial.h"
#include "L298N.h"
#include "CountSensor.h" 
#include "Buzzer.h"
#include "Timer.h"
#include "Encoder.h"
#include "cJSON.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 引脚定义
#define COUNTER PA_11
#define TXD PA_2
#define RXD PA_3
#define LED_PIN PB_1
#define BUZ_PIN PA_1
#define IN1 PB_15
#define IN2 PB_14
// #define IN3 PB_13
// #define IN4 PB_12
#define ENC_SW PA_7
#define ENC_A PA_6 
#define ENC_B PA_5
#define ESP_TX PB_10
#define ESP_RX PB_11

// Motor direct definition
#define DIR_OFF 0
#define DIR_ZHENG 1
#define DIR_FAN 2
#define JSON_SIZE 256

void infusion_loop(void);
void sendJSONData(char* buf, size_t size, uint8_t serial);

uint16_t machineNumber = 1; //设备编号
uint8_t infusion_state = 0; //输液状态机
uint8_t alarm_state = 0; //警报状态机
uint16_t miss_count = 0; //发现流量不足的时间

uint8_t pump_direct = DIR_OFF; //水泵方向
uint8_t valve_direct = DIR_OFF; //阀门状态
int16_t valve_compare = 100; //阀门的闭合度
int16_t pump_compare = 50; //水泵运行速度
char JSONData_buffer[JSON_SIZE]; //JSON数据缓冲区

int speed_required = 80; //预设速度，默认速度为80

//从传感器读取的实时数据变量
uint16_t cur_time = 0;
uint16_t count_per_min = 0;

// Main loop function
int main(void)
{
	/*模块初始化*/
	OLED_Init();		//OLED初始化
	Serial_Init(); //初始化串口
	ESP_UART_Init(); //初始化ESP32 UART
	Buzzer_init();  //初始化蜂鸣器
	CountSensor_Init(); //初始化流速传感器
	L298N_Init(); //初始化电机驱动器
	Tim4_Init(); //初始化定时器4，实现心跳包的发送和电机状态的更新
	Encoder_Init(); //初始化旋转编码器
	
	GPIO_ResetBits(GPIOB, GPIO_Pin_1);
	beep(100);
	GPIO_SetBits(GPIOB, GPIO_Pin_1); 
	Serial_SendString("Start!\nPowered by Snowmiku\n");
	ESP_Serial_SendByte('O');
	
	valve_set(DIR_OFF, 0);
	
	/*显示静态字符串*/
	OLED_ShowString(1, 1, "Avg:");
	OLED_ShowNum(1, 6, 0, 4);
	OLED_ShowString(2, 1, "Spd:");
	OLED_ShowNum(2, 6, 0, 3);
	OLED_ShowString(3, 1, (infusion_state==1 ? "ON " : "OFF"));
	OLED_ShowNum(3, 5, speed_required, 3);
	
	while (1)
	{
		//读取数据
		cur_time = CounterSensor_GetAvgValue();
		count_per_min = CountSensor_GetCountPerMinute();

		/*显示静态字符串*/
		OLED_ShowString(1, 1, "Avg:");
		OLED_ShowString(2, 1, "Spd:");

		//展示数据
		OLED_ShowNum(1, 6, cur_time, 4);
		OLED_ShowNum(2, 6, count_per_min, 3); //显示流速
		OLED_ShowString(3, 1, (infusion_state==1 ? "ON " : "OFF"));
		OLED_ShowNum(3, 5, speed_required, 3);


		//进入马达控制逻辑
		infusion_loop(); //执行输液逻辑
	}
}

// Main loop of processing

void infusion_loop(void)
{
	int receive_integer_temp; 
	int encoder_value;
	char encoder_keystate;
	static int interrupt_time_overflow = 0;
	char interrupt_okay = 0;

	//处理电脑端串口输入
	if(Serial_RxFlag == 1) 
	{
		Serial_RxFlag = 0;
		beep(50);
		Delay_ms(10);
		if(strcmp(Serial_RxPacket, "switch") == 0)
		{
			beep(100);
			infusion_state = (infusion_state == 0 ? 1 : 0); //切换输液状态
			if(infusion_state == 0) //关断
			{
				valve_set(DIR_ZHENG, 100);
				miss_count = 0;
				interrupt_time_overflow = 0;
			}
			else //打开阀门
			{
				valve_set(DIR_OFF, 0);
			}
			Serial_Printf("State:%d", infusion_state); //回传状态
		}
		else if(strcmp(Serial_RxPacket, "set_speed") == 0) //接收液体滴速的输入
		{
			beep(100);
			Serial_Printf("Please input the speed(10 - 120):");
			while(1)
			{
				if(Serial_RxFlag == 1) //处理串口输入
				{
					Serial_RxFlag = 0;
					receive_integer_temp = atoi(Serial_RxPacket); //读取代表输液速度的整数值
					if(receive_integer_temp < 10 || receive_integer_temp > 120){
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
		else if(strcmp(Serial_RxPacket, "acquire") == 0)
		{
			//以JSON格式发送数据
			sendJSONData(JSONData_buffer, JSON_SIZE, 0); //打包发送数据
		}
	}

	//处理ESP串口输入
	#define DEBUG
	if(ESP_Serial_RxFlag == 1) 
	{
		ESP_Serial_RxFlag = 0;
		Delay_ms(1);
		#ifdef DEBUG
		Serial_Printf("[ESP->]%s\n", ESP_Serial_RxPacket);
		#endif
		// 启停指令
		if(strcmp(ESP_Serial_RxPacket, "switch") == 0)
		{
			beep(100);
			infusion_state = (infusion_state == 0 ? 1 : 0); //切换输液状态
			if(infusion_state == 0) //关断
			{
				valve_set(DIR_ZHENG, 100);
				miss_count = 0;
				interrupt_time_overflow = 0;
			}
			else //打开阀门
			{
				valve_set(DIR_OFF, 0);
			}
			sendJSONData(JSONData_buffer, JSON_SIZE, 1);
			#ifdef DEBUG
			Serial_Printf("[->ESP]%s", JSONData_buffer);
			#endif
		}
		// 
		else if(strcmp(ESP_Serial_RxPacket, "set_speed") == 0) //接收液体滴速的输入
		{
			beep(40);
			Serial_Printf("ESP set speed command!\n");
			ESP_Serial_Printf("OK\n");
			while(1)
			{
				//等待esp发送数字信息
				if(ESP_Serial_RxFlag == 1) //处理串口输入
				{
					ESP_Serial_RxFlag = 0;
					receive_integer_temp = atoi(ESP_Serial_RxPacket); //读取代表输液速度的整数值
					#ifdef DEBUG
					Serial_Printf("[ESP->]%s, transfer to %d\n", ESP_Serial_RxPacket, receive_integer_temp);
					#endif
					if(receive_integer_temp < 10 || receive_integer_temp > 120){
						ESP_Serial_Printf("ERR\n");
						return;
					}
					else{
						speed_required = receive_integer_temp;
						sendJSONData(JSONData_buffer, JSON_SIZE, 1);
						break;
					}
				}
			}
		}
		else if(strcmp(ESP_Serial_RxPacket, "acquire") == 0)
		{
			//以JSON格式发送数据
			Delay_ms(1);
			Serial_Printf("ESP request!\n");
			sendJSONData(JSONData_buffer, JSON_SIZE, 1); //打包发送数据
		}
		else if(strcmp(ESP_Serial_RxPacket, "ESP") == 0)
		{
			Serial_Printf("ESP Connected!\n");
			ESP_Serial_Printf("STM\n");
		}
	}
	
	//当检测到液体滴落，发出声音提醒
	if(CounterSensor_GetStat() == 1){
			beep(30);
			interrupt_time_overflow = 0;
			interrupt_okay = 1;
	}

	//读取编码器的输入
	encoder_keystate = Encoder_Readkey();
	encoder_value = Encoder_Get();

	if(encoder_keystate || encoder_value){ //有输入时，发出警报
		beep(50);
	}

	if(encoder_keystate){
		//用按键切换输液的开关状态
		infusion_state = (infusion_state == 0 ? 1 : 0); //切换输液状态
			if(infusion_state == 0) //关断
			{
				valve_set(DIR_ZHENG, 100);
			}
			else //打开阀门
			{
				valve_set(DIR_OFF, 0);
			}
			miss_count = 0;
			interrupt_time_overflow = 0;
			Serial_Printf("State:%d", infusion_state); //回传状态
	}

	if(encoder_value != 0)
	{
		//更改输液的速度需求
		speed_required += encoder_value;
		if(speed_required >= 120) speed_required=120;
		if(speed_required <= 20) speed_required=20;
	}

	//根据输液是否打开的状态，执行逻辑
	if(infusion_state == 1)
	{
		//每10个心跳节拍，对电机状态进行更新 - 500ms
		if(tim4_heartbeats >= 10){
			tim4_heartbeats = 0;
			
			if(interrupt_okay) 
			{
				interrupt_okay = 0;
				interrupt_time_overflow = 0;
			}
			else{
				interrupt_time_overflow++;
			}
			if(speed_required > count_per_min)
			{
				valve_compare -= 2; //放松阀门
				if(valve_compare <= 0) valve_compare = 0;  
				valve_set(DIR_ZHENG, valve_compare);
			}
			else if(speed_required < count_per_min)
			{
				valve_compare += 2; //拧紧阀门
				if(valve_compare >= 100) valve_compare = 100;
				valve_set(DIR_ZHENG, valve_compare);
			}
			//对液体不足情况的处理
			if((speed_required > count_per_min && valve_compare == 0) || interrupt_time_overflow >= 18)
			{
				miss_count ++;
				if(miss_count >= 20 || interrupt_time_overflow >= 18){
					miss_count = 0;
					interrupt_time_overflow = 0;

					alarm_state = 1;
					valve_set(DIR_ZHENG, 100);
				}
				if(alarm_state == 1)
				{
					OLED_Clear();
					OLED_ShowString(1,1,"Empty!");
					OLED_ShowString(2,1,"Press R to reset");
					Serial_Printf("Empty, press R key to reset!\n");
					//警报界面
					while(1){
						beep(300);
						Delay_ms(100);
						ESP_Serial_Printf("EMPTY\n");
						encoder_keystate = Encoder_Readkey();
						if(encoder_keystate == 1){
							encoder_keystate = 0;
							alarm_state = 0;
							valve_compare = 0;
							infusion_state = 0;
							valve_set(DIR_OFF,valve_compare);
							Delay_ms(50);
							beep(200);
							Serial_Printf("Reset!\n");
							ESP_Serial_Printf("RESET\n");
							OLED_Clear();
							break;
						}
					}
				}
			}
			else miss_count = 0;
			//将当前的数据发送到电脑
			// Serial_Printf("----------Info------------\n");
			// Serial_Printf("Count:%d, Valve:%d\n", count_per_min, valve_compare);
			// Serial_Printf("Miss:%d, Overflow:%d\n", miss_count, interrupt_time_overflow);
			// Serial_Printf("\n");
		}
	}
}

void sendJSONData(char* buf, size_t size, uint8_t serial)
{
	cJSON *root = cJSON_CreateObject();
	// 加入数据
	cJSON_AddNumberToObject(root, "state", (double)infusion_state);
	cJSON_AddNumberToObject(root, "speed", (double)count_per_min);
	cJSON_AddNumberToObject(root, "requiredspeed", (double)speed_required);
	//输出数据
	char* json_cache = cJSON_PrintUnformatted(root);
	if(json_cache == NULL)
	{
		Serial_Printf("Can't allocate memory!\n");
	}
	else
	{
		snprintf(buf, size, "%s\n", json_cache);
		cJSON_Delete(root);
		free(json_cache);

		//发送数据
		if(serial == 0)
		{
			Serial_SendString(buf);
		}
		else if(serial == 1)
		{
			ESP_Serial_SendString(buf);
		}
	}
}
