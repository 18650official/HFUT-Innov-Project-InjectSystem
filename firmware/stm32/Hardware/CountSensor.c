#include "stm32f10x.h"                  // Device header
#include "CountSensor.h"

void refresh_list(uint16_t new_value);

uint8_t counterState = 0;
uint16_t current_timvalue = 0;
uint16_t current_timvalue_prev = 0; //计数器大小的记忆
uint8_t counterState_to_return;
uint16_t sliding_window_array[5] = {0}; //滑动窗口数组
int slidewindow_index = 0;
uint16_t tim2_overflow_ms = 0; //计数器的溢出次数

void Timer2_Init(void) //定时器初始化
{
	/*开启时钟*/
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);			//开启TIM2的时钟
	
	/*配置时钟源*/
	TIM_InternalClockConfig(TIM2);		//选择TIM2为内部时钟，若不调用此函数，TIM默认也为内部时钟
	
	/*时基单元初始化*/
	TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure;				//定义结构体变量
	TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;		//时钟分频，选择不分频，此参数用于配置滤波器时钟，不影响时基单元功能
	TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;	//计数器模式，选择向上计数
	TIM_TimeBaseInitStructure.TIM_Period = 100 - 1;				//计数周期，即ARR的值
	TIM_TimeBaseInitStructure.TIM_Prescaler = 720 - 1;				//预分频器，即PSC的值
	TIM_TimeBaseInitStructure.TIM_RepetitionCounter = 0;			//重复计数器，高级定时器才会用到
	TIM_TimeBaseInit(TIM2, &TIM_TimeBaseInitStructure);				//将结构体变量交给TIM_TimeBaseInit，配置TIM2的时基单元	
	
	/*中断输出配置*/
	TIM_ClearFlag(TIM2, TIM_FLAG_Update);						//清除定时器更新标志位
																//TIM_TimeBaseInit函数末尾，手动产生了更新事件
																//若不清除此标志位，则开启中断后，会立刻进入一次中断
																//如果不介意此问题，则不清除此标志位也可
	
	TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);					//开启TIM2的更新中断
	
	/*NVIC中断分组*/
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);				//配置NVIC为分组2
																//即抢占优先级范围：0~3，响应优先级范围：0~3
																//此分组配置在整个工程中仅需调用一次
																//若有多个中断，可以把此代码放在main函数内，while循环之前
																//若调用多次配置分组的代码，则后执行的配置会覆盖先执行的配置
	
	/*NVIC配置*/
	NVIC_InitTypeDef NVIC_InitStructure;						//定义结构体变量
	NVIC_InitStructure.NVIC_IRQChannel = TIM2_IRQn;				//选择配置NVIC的TIM2线
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;				//指定NVIC线路使能
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 2;	//指定NVIC线路的抢占优先级为2
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 3;			//指定NVIC线路的响应优先级为1
	NVIC_Init(&NVIC_InitStructure);								//将结构体变量交给NVIC_Init，配置NVIC外设
	
	/*TIM使能*/
	TIM_Cmd(TIM2, ENABLE);			//使能TIM2，定时器开始运行
}

/**
  * 函    数：计数传感器初始化
  * 参    数：无
  * 返 回 值：无
  */
void CountSensor_Init(void)
{
	/*开启时钟*/
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);		//开启GPIOB的时钟
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);		//开启AFIO的时钟，外部中断必须开启AFIO的时钟
	
	/*GPIO初始化*/
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &GPIO_InitStructure);						//将PB14引脚初始化为上拉输入
	
	/*AFIO选择中断引脚*/
	GPIO_EXTILineConfig(GPIO_PortSourceGPIOA, GPIO_PinSource11);//将外部中断的14号线映射到GPIOB，即选择PB14为外部中断引脚
	
	/*EXTI初始化*/
	EXTI_InitTypeDef EXTI_InitStructure;						//定义结构体变量
	EXTI_InitStructure.EXTI_Line = EXTI_Line11;					//选择配置外部中断的14号线
	EXTI_InitStructure.EXTI_LineCmd = ENABLE;					//指定外部中断线使能
	EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;			//指定外部中断线为中断模式
	EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Rising;		//指定外部中断线为上升沿触发
	EXTI_Init(&EXTI_InitStructure);								//将结构体变量交给EXTI_Init，配置EXTI外设
	
	/*NVIC中断分组*/
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);				//配置NVIC为分组2
																//即抢占优先级范围：0~3，响应优先级范围：0~3
																//此分组配置在整个工程中仅需调用一次
																//若有多个中断，可以把此代码放在main函数内，while循环之前
																//若调用多次配置分组的代码，则后执行的配置会覆盖先执行的配置
	
	/*NVIC配置*/
	NVIC_InitTypeDef NVIC_InitStructure;						//定义结构体变量
	NVIC_InitStructure.NVIC_IRQChannel = EXTI15_10_IRQn;		//选择配置NVIC的EXTI15_10线
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;				//指定NVIC线路使能
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;	//指定NVIC线路的抢占优先级为1
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;			//指定NVIC线路的响应优先级为1
	NVIC_Init(&NVIC_InitStructure);								//将结构体变量交给NVIC_Init，配置NVIC外设
	
	//Init the timer
	Timer2_Init();
}

/**
  * 函    数：获取计数传感器的两次计数间隔
  * 参    数：无
  * 返 回 值：计数值，范围：0~65535
  */
uint16_t CountSensor_GetTime(void)
{
	return current_timvalue;
}

uint8_t CounterSensor_GetStat() //查询传感器当前的状态
{
	if(counterState_to_return){
		counterState_to_return = 0;
		return 1;
	}
	else return 0;
}

void TIM2_IRQHandler(void) //每毫秒产生一次定时器中断
{
	if (TIM_GetITStatus(TIM2, TIM_IT_Update) == SET)
	{
		tim2_overflow_ms++;
		TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
	} 
}

/**
  * 函    数：EXTI15_10外部中断函数
  * 参    数：无
  * 返 回 值：无
  * 注意事项：此函数为中断函数，无需调用，中断触发后自动执行
  *           函数名为预留的指定名称，可以从启动文件复制
  *           请确保函数名正确，不能有任何差异，否则中断函数将不能进入
  */
void EXTI15_10_IRQHandler(void)
{
	if (EXTI_GetITStatus(EXTI_Line11) == SET)		//判断是否是外部中断14号线触发的中断
	{
		/*如果出现数据乱跳的现象，可再次判断引脚电平，以避免抖动*/
		if (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_11) == 1)
		{
			counterState = 1;					
			counterState_to_return = 1;
			if(counterState == 1){
				TIM_Cmd(TIM2, DISABLE); 
				current_timvalue = tim2_overflow_ms; //获取计时器的寄存器数值
				if(current_timvalue < 180){
					current_timvalue = current_timvalue_prev; //舍弃不稳定的数据
				}
				else{
					current_timvalue_prev = current_timvalue; //刷新数据
					refresh_list(current_timvalue); //填入滑动窗口列表
				}
				counterState = 0;
				TIM_SetCounter(TIM2, 0); //清空计时器
				tim2_overflow_ms = 0; //清空计时器溢出量
				TIM_Cmd(TIM2, ENABLE);
			}
		}
		EXTI_ClearITPendingBit(EXTI_Line11);		//清除外部中断14号线的中断标志位
													//中断标志位必须清除
													//否则中断将连续不断地触发，导致主程序卡死
	}
}

void refresh_list(uint16_t new_value) //更新滑动窗口
{
	int i;
	// 执行逻辑
	if(slidewindow_index >= 4){
		for(i = 0;i < 4;i++){
			sliding_window_array[i] = sliding_window_array[i+1]; //移动前4个数据
		}
	}
	sliding_window_array[slidewindow_index] = new_value; //加入数据的最后一个
	slidewindow_index = (slidewindow_index >= 4 ? 4 : slidewindow_index+1);
}

uint16_t CounterSensor_GetAvgValue(void) //获取平均值
{
	uint16_t res;
	int i;
	for(i=0, res=0;i<slidewindow_index+1;i++){
		res += sliding_window_array[i];
	}
	res /= (slidewindow_index+1); //获取平均数
	return res;
}

uint16_t CountSensor_GetCountPerMinute(void){ //获取每分钟滴数
	int res = (60*1000) / CounterSensor_GetAvgValue();
	return res;
}
