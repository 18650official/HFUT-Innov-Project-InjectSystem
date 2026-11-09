#ifndef __L298N_H__
#define __L298N_H__

#include "stm32f10x.h"

#endif

void L298N_Init(void);
void L298N_SetCompare(uint16_t Compare1, uint16_t Compare2);
void L298N_ConfigRotateDirect(int a,int b); //配置L298N两路电机的输出状态
