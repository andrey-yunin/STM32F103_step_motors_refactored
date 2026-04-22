/*
 * task_tmc2209_manager.c
 *
 *  Created on: Dec 8, 2025
 *      Author: andrey
 */

#include "task_tmc2209_manager.h"
#include "main.h"
#include "cmsis_os.h"
#include "tmc2209_driver.h" // Для TMC2209_Handle_t
#include "app_config.h"     // Для MOTOR_COUNT
#include "app_queues.h"

// --- Инкапсулированные данные драйверов ---
static TMC2209_Handle_t tmc_drivers[MOTOR_COUNT];

TMC2209_Handle_t* TMCManager_GetHandle(uint8_t motor_id)
{
    if (motor_id >= MOTOR_COUNT) {
        return NULL;
    }

    return &tmc_drivers[motor_id];
}

// Внешние переменные, объявленные в main.c
extern UART_HandleTypeDef huart1; // Хэндл UART1
extern UART_HandleTypeDef huart2; // Хэндл UART2

void app_start_task_tmc2209_manager(void *argument)
{
    // Ждем небольшую паузу, чтобы все остальные части системы успели запуститься
    osDelay(100);

    // --- Инициализация всех 8-ми драйверов ---
    // Моторы 0-3 на USART1 (slave_addr 0-3)
    // Моторы 4-7 на USART2 (slave_addr 0-3)
    for (uint8_t i = 0; i < MOTOR_COUNT; i++) {
        UART_HandleTypeDef* huart_ptr = (i < 4) ? &huart1 : &huart2;
        uint8_t slave_addr = i % 4;

        TMC2209_Init(&tmc_drivers[i], huart_ptr, slave_addr);
        TMC2209_SetMotorCurrent(&tmc_drivers[i], 70, 40); // 70% рабочий ток, 40% ток удержания
        TMC2209_SetMicrosteps(&tmc_drivers[i], 16);       // 16 микрошагов
    }

    // Бесконечный цикл задачи
    for (;;) {
        osDelay(1000);
    }
}
