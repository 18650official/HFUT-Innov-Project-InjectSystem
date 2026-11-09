#ifndef __COUNT_SENSOR_H
#define __COUNT_SENSOR_H

void CountSensor_Init(void);
uint16_t CountSensor_GetTime(void);
uint8_t CounterSensor_GetStat(void);
uint16_t CountSensor_GetCountPerMinute(void);
uint16_t CounterSensor_GetAvgValue(void);

#endif
