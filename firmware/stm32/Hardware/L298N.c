#include "L298N.h"

#define OFF 0
#define ZHENG 1
#define FAN 2

uint16_t motor1_ccr;
uint16_t motor2_ccr;
uint16_t timvalue = 0;

unsigned char gpio_config_list[4] = {0}; //A1,A2,A3,A4

void L298N_ConfigRotateDirect(int a,int b) //配置L298N两路电机的输出状态
{
    switch(a){
        case OFF:
        {
            gpio_config_list[0] = 0;
            gpio_config_list[1] = 0;
            break;
        }
        case ZHENG:
        {
            gpio_config_list[0] = 1;
            gpio_config_list[1] = 0;
            break;
        }
        case FAN:
        {
            gpio_config_list[0] = 0;
            gpio_config_list[1] = 1;
            break;
        }
        default : break;
    }
    switch(b){
        case OFF:
        {
            gpio_config_list[2] = 0;
            gpio_config_list[3] = 0;
            break;
        }
        case ZHENG:
        {
            gpio_config_list[2] = 1;
            gpio_config_list[3] = 0;
            break;
        }
        case FAN:
        {
            gpio_config_list[2] = 0;
            gpio_config_list[3] = 1;
            break;
        }
        default : break;
    }
}

void L298N_GPIO_Init()
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    /*GPIO初始化*/
		GPIO_InitTypeDef GPIO_InitStructure;
		GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
		GPIO_InitStructure.GPIO_Pin = GPIO_Pin_15;
		GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
		GPIO_Init(GPIOB, &GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_14;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
}

void Tim3_Init(void) //定时器初始化函数
{
	/*开启时钟*/
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);			//开启TIM2的时钟

	/*配置时钟源*/
	TIM_InternalClockConfig(TIM3);	
	
	/*初始化定时器自增计数单元*/
	TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure;				//定义结构体变量
	TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;     //时钟分频，选择不分频，此参数用于配置滤波器时钟，不影响时基单元功能
	TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up; //计数器模式，选择向上计数
	TIM_TimeBaseInitStructure.TIM_Period = 10000 - 1;					//计数周期，即ARR的值
	TIM_TimeBaseInitStructure.TIM_Prescaler = 7200 - 1;				//预分频器，即PSC的值
	TIM_TimeBaseInitStructure.TIM_RepetitionCounter = 0;            //重复计数器，高级定时器才会用到
	TIM_TimeBaseInit(TIM3, &TIM_TimeBaseInitStructure);             //将结构体变量交给TIM_TimeBaseInit，配置TIM2的时基单元
	
	/*中断输出配置*/
	TIM_ClearFlag(TIM3, TIM_FLAG_Update);						//清除定时器更新标志位
																//TIM_TimeBaseInit函数末尾，手动产生了更新事件
																//若不清除此标志位，则开启中断后，会立刻进入一次中断
																//如果不介意此问题，则不清除此标志位也可
	
	TIM_ITConfig(TIM3, TIM_IT_Update, ENABLE);					//开启TIM3的更新中断
	
	/*NVIC中断分组*/
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);				//配置NVIC为分组2
																//即抢占优先级范围：0~3，响应优先级范围：0~3
																//此分组配置在整个工程中仅需调用一次
																//若有多个中断，可以把此代码放在main函数内，while循环之前
																//若调用多次配置分组的代码，则后执行的配置会覆盖先执行的配置
	
	/*NVIC配置*/
	NVIC_InitTypeDef NVIC_InitStructure;						//定义结构体变量
	NVIC_InitStructure.NVIC_IRQChannel = TIM3_IRQn;				//选择配置NVIC的TIM2线
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;				//指定NVIC线路使能
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 3;	//指定NVIC线路的抢占优先级为3
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;			//指定NVIC线路的响应优先级为1
	NVIC_Init(&NVIC_InitStructure);								//将结构体变量交给NVIC_Init，配置NVIC外设
	
	/*TIM使能*/
	TIM_Cmd(TIM3, ENABLE);			//使能TIM2，定时器开始运行
}

/**
  * 函    数：PWM设置CCR
  * 参    数：Compare 要写入的CCR的值，范围：0~100
  * 返 回 值：无
  * 注意事项：CCR和ARR共同决定占空比，此函数仅设置CCR的值，并不直接是占空比
  *           占空比Duty = CCR / (ARR + 1)
  */
void L298N_SetCompare(uint16_t Compare1, uint16_t Compare2)
{
		motor1_ccr = Compare1;
    motor2_ccr = Compare2;
}

void TIM3_IRQHandler(void) //中断函数
{
    if (TIM_GetITStatus(TIM3, TIM_IT_Update) == SET) //清除TIM3的中断标志位
	{
        timvalue = (timvalue >= 100 ? 0 : timvalue+1);
        //判断并执行操作
        //设置第1路直流电机
        if(timvalue >= motor1_ccr)
        {
            GPIO_ResetBits(GPIOB, GPIO_Pin_15);
            GPIO_ResetBits(GPIOB, GPIO_Pin_14);
        }
        else
        {
            if(gpio_config_list[0] == 0 && gpio_config_list[1] == 0){
                GPIO_ResetBits(GPIOB, GPIO_Pin_15);
                GPIO_ResetBits(GPIOB, GPIO_Pin_14);
            }
            else if(gpio_config_list[0] == 1 && gpio_config_list[1] == 0){
                GPIO_SetBits(GPIOB, GPIO_Pin_15);
                GPIO_ResetBits(GPIOB, GPIO_Pin_14);
            }
            else if(gpio_config_list[0] == 0 && gpio_config_list[1] == 1){
                GPIO_ResetBits(GPIOB, GPIO_Pin_15);
                GPIO_SetBits(GPIOB, GPIO_Pin_14);
            }
        }
        //设置第2路直流电机
        if(timvalue >= motor2_ccr)
        {
            GPIO_ResetBits(GPIOB, GPIO_Pin_13);
            GPIO_ResetBits(GPIOB, GPIO_Pin_12);
        }
        else
        {
            if(gpio_config_list[2] == 0 && gpio_config_list[3] == 0){
                GPIO_ResetBits(GPIOB, GPIO_Pin_13);
                GPIO_ResetBits(GPIOB, GPIO_Pin_12);
            }
            else if(gpio_config_list[2] == 1 && gpio_config_list[3] == 0){
                GPIO_SetBits(GPIOB, GPIO_Pin_13);
                GPIO_ResetBits(GPIOB, GPIO_Pin_12);
            }
            else if(gpio_config_list[2] == 0 && gpio_config_list[3] == 1){
                GPIO_ResetBits(GPIOB, GPIO_Pin_13);
                GPIO_SetBits(GPIOB, GPIO_Pin_12);
            }
        }
        TIM_ClearITPendingBit(TIM3, TIM_IT_Update);
    }
}

void L298N_Init()
{
    L298N_GPIO_Init();
    Tim3_Init();
    L298N_ConfigRotateDirect(OFF, OFF);
    L298N_SetCompare(0, 0);
}
