/*
 * app_globals.c
 *
 *  Created on: Dec 10, 2025
 *      Author: andrey
 */


#include "app_globals.h"
#include <stdbool.h>

// Определения глобальных переменных

uint8_t g_performer_id = 0xFF;
volatile bool g_motor_active[MOTOR_COUNT] = {false}; // All motors inactive by default
MotorMotionState_t motor_states[MOTOR_COUNT];
TMC2209_Handle_t tmc_drivers[MOTOR_COUNT];


