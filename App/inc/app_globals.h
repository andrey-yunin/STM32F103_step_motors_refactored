/*
 * app_globals.h
 *
 *  Created on: Dec 10, 2025
 *      Author: andrey
 */

#ifndef APP_GLOBALS_H_
#define APP_GLOBALS_H_

#include <stdint.h>
#include "motion_planner.h" // Для MotorMotionState_t
#include "tmc2209_driver.h" // Для TMC2209_Handle_t
#include "app_config.h"     // Для MOTOR_COUNT
#include <stdbool.h>


// Определение и инициализация глобальной переменной
extern uint8_t g_performer_id; // 0xFF означает, что ID еще не установлен

// Массив флагов, указывающих, активен ли каждый мотор.
// true - мотор активен (двигается или включен), false - неактивен.
extern volatile bool g_motor_active[MOTOR_COUNT];


// Объявление глобальных массивов для моторов/драйверов
extern MotorMotionState_t motor_states[MOTOR_COUNT];
extern TMC2209_Handle_t tmc_drivers[MOTOR_COUNT];


#endif /* APP_GLOBALS_H_ */
