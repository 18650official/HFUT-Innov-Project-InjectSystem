#include "Buzzer.h"

void Buzzer_init(void)
{
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
	//Enable the buzzer
	GPIO_InitTypeDef GPIO_InitStructure;
 	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1;
 	GPIO_Init(GPIOA, &GPIO_InitStructure);
	//Enable the LED Pin
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
 	GPIO_Init(GPIOB, &GPIO_InitStructure);

	GPIO_SetBits(GPIOA, GPIO_Pin_1); 
	GPIO_SetBits(GPIOB, GPIO_Pin_1); //关闭LED
}

void beep(uint32_t t)
{
	GPIO_ResetBits(GPIOA, GPIO_Pin_1);
	Delay_ms(t);
	GPIO_SetBits(GPIOA, GPIO_Pin_1); 
}
