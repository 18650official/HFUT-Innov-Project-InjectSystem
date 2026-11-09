#ifndef __ENCODER_H_
#define __ENCODER_H_

#include "stm32f10x.h"
#include "Delay.h"

#endif

void Encoder_Init(void);
int16_t Encoder_Get(void);
int8_t Encoder_Readkey(void);
