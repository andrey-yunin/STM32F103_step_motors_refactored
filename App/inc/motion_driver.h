#ifndef __MOTION_DRIVER_H
#define __MOTION_DRIVER_H

#include "stm32f1xx_hal.h"
#include <stdbool.h>

void MotionDriver_Init(void);
void MotionDriver_AllSafe(void);
void MotionDriver_SetDirection(uint8_t motor_id, bool forward);
void MotionDriver_StartMotor(uint8_t motor_id, uint32_t frequency);
bool MotionDriver_StartFinite(uint8_t motor_id, uint32_t frequency, uint32_t steps);
void MotionDriver_StopMotor(uint8_t motor_id);
bool MotionDriver_ConsumeCompletedMotor(uint8_t *motor_id);

#endif // __MOTION_DRIVER_H
