#ifndef __SERIAL_H
#define __SERIAL_H

#include <stdio.h>

extern char Serial_RxPacket[];
extern uint8_t Serial_RxFlag;
extern char ESP_Serial_RxPacket[];
extern uint8_t ESP_Serial_RxFlag;

void Serial_Init(void);
void Serial_SendByte(uint8_t Byte);
void Serial_SendArray(uint8_t *Array, uint16_t Length);
void Serial_SendString(char *String);
void Serial_SendNumber(uint32_t Number, uint8_t Length);
void Serial_Printf(char *format, ...);

void ESP_UART_Init(void);
void ESP_Serial_SendByte(uint8_t Byte);
void ESP_Serial_SendArray(uint8_t *Array, uint16_t Length);
void ESP_Serial_SendString(char *String);
void ESP_Serial_SendNumber(uint32_t Number, uint8_t Length);
void ESP_Serial_Printf(char *format, ...);


#endif
