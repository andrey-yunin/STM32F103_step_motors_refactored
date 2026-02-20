/*
 * motion_planner.c
 *
 *  Created on: Dec 8, 2025
 *      Author: andrey
 */


#include "motion_planner.h"
#include <math.h> // Может понадобиться для расчетов ускорения
#include <stdlib.h> // Для abs()

/**
 * @brief Инициализирует начальное состояние мотора.
 * @param state Указатель на структуру состояния.
 * @param initial_pos Начальная позиция, с которой стартует мотор.
 */
void MotionPlanner_InitMotorState(MotorMotionState_t* state, int32_t initial_pos)
{
	if (state == NULL) return;
	state->current_position = initial_pos;
    state->target_position = initial_pos;
    state->current_speed_steps_per_sec = 0;
    state->max_speed_steps_per_sec = 2000; // Скорость по умолчанию
    state->acceleration_steps_per_sec2 = 500; // Ускорение по умолчанию
    state->direction = 0;
    state->steps_to_go = 0;
    state->step_pulse_period_us = 0;
    }

/**
 * @brief Рассчитывает новое задание на движение.
 * @param state Указатель на структуру состояния.
 * @param target Новая целевая позиция.
 * @return Количество шагов, которые нужно сделать.
 */
int32_t MotionPlanner_CalculateNewTarget(MotorMotionState_t* state, int32_t target)
{
	if (state == NULL) return 0;

	state->target_position = target;
    state->steps_to_go = abs(state->target_position - state->current_position);

    if (state->target_position > state->current_position) {
    state->direction = 1; // Условно, вперед
    } else {
    state->direction = 0; // Условно, назад
    }

    return state->steps_to_go;
    }

/**
 * @brief Рассчитывает следующий период для таймера. (Пока заглушка)
 * @param state Указатель на структуру состояния.
 * @return Период в микросекундах.
 */
uint32_t MotionPlanner_GetNextPulsePeriod(MotorMotionState_t* state)
{
	if (state == NULL || state->steps_to_go == 0) {
		return 0; // Движение не требуется
    }

//
// --- ЗДЕСЬ БУДЕТ СЛОЖНАЯ ЛОГИКА ---
//
// На данном этапе мы просто возвращаем период для максимальной скорости.
// В будущем здесь будет алгоритм, который плавно изменяет этот период
// для реализации трапецеидального профиля движения (ускорение/замедление).
//

    if (state->steps_to_go > 0) {
// Уменьшаем количество шагов
    state->steps_to_go--;
// Обновляем текущую позицию
    if (state->direction) {
    	state->current_position++;
     } else {
     state->current_position--;
            }
     }

// Период = 1 000 000 микросекунд / скорость (шагов/сек)
return 1000000 / state->max_speed_steps_per_sec;
}

/**
 * @brief Проверяет, завершено ли движение.
 * @param state Указатель на структуру состояния.
 * @return 1 если завершено, 0 если нет.
 */
uint8_t MotionPlanner_IsMovementComplete(MotorMotionState_t* state)
{
	if (state == NULL) return 1;

    return (state->steps_to_go == 0);
}



