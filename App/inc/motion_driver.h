#ifndef __MOTION_DRIVER_H
#define __MOTION_DRIVER_H

#include "stm32f1xx_hal.h"
#include <stdbool.h>

void MotionDriver_Init(void);
void MotionDriver_SetDirection(uint8_t motor_id, bool forward);
void MotionDriver_StartMotor(uint8_t motor_id, uint32_t frequency);
void MotionDriver_StopMotor(uint8_t motor_id);
void MotionDriver_PulseGenerationCompleted_Callback(TIM_HandleTypeDef *htim);

#endif // __MOTION_DRIVER_H
