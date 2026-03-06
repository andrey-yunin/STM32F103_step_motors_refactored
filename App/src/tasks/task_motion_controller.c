/*
 * task_motion_controller.c
 *
 *  Created on: Dec 8, 2025
 *      Author: andrey
 *
 *  Refactored on: Mar 6, 2026
 *      Author: andrey
 *
 *  Контроллер движения. Отвечает за:
 *  - Приём MotionCommand_t из motion_queue (от Command Parser)
 *  - Проверку занятости мотора (ACK / NACK MOTOR_BUSY)
 *  - Управление моторами через MotionDriver API
 *  - Отправку DONE после завершения движения
 */

#include "task_motion_controller.h"
#include "main.h"
#include "cmsis_os.h"
#include "motion_planner.h"
#include "app_config.h"
#include "app_globals.h"
#include "app_queues.h"
#include "motion_driver.h"
#include "command_protocol.h"
#include "can_protocol.h"

void app_start_task_motion_controller(void *argument)
{
	MotionCommand_t received_motion_cmd;
	// --- Инициализация состояний моторов и драйвера ---

	for (uint8_t i = 0; i < MOTOR_COUNT; i++) {
		MotionPlanner_InitMotorState(&motor_states[i], 0);
        }

	MotionDriver_Init();

	// --- Основной цикл ---
	for (;;)
		{
		if (osMessageQueueGet(motion_queueHandle, &received_motion_cmd, NULL, osWaitForever) == osOK)
			{
			uint8_t motor_id = received_motion_cmd.motor_id;
			uint16_t cmd_code = received_motion_cmd.cmd_code;
            uint8_t device_id = received_motion_cmd.device_id;

            // --- Валидация motor_id ---
            if (motor_id >= MOTOR_COUNT) {
            	CAN_SendNackPublic(cmd_code, CAN_ERR_INVALID_MOTOR_ID);
            	continue;
            	}

            // --- Проверка занятости мотора ---
            // CMD_STOP разрешён даже если мотор занят
            if (received_motion_cmd.command_id != CMD_STOP && g_motor_active[motor_id]) {
                                CAN_SendNackPublic(cmd_code, CAN_ERR_MOTOR_BUSY);
                                continue;
                        }

                        // --- ACK: команда принята к исполнению ---
                        CAN_SendAck(cmd_code);

                        // --- Обработка команды ---
                        switch (received_motion_cmd.command_id)
                        {
                                case CMD_MOVE_ABSOLUTE:
                                case CMD_MOVE_RELATIVE:
                                {
                                        int32_t target_pos;
                                        if (received_motion_cmd.command_id == CMD_MOVE_ABSOLUTE) {
                                                target_pos = (int32_t)received_motion_cmd.steps;
                                        } else {
                                                target_pos = motor_states[motor_id].current_position +
                                                                (received_motion_cmd.direction ? (int32_t)received_motion_cmd.steps
                                                                                                       : -(int32_t)received_motion_cmd.steps);
                                        }

                                        int32_t steps_to_go = MotionPlanner_CalculateNewTarget(&motor_states[motor_id], target_pos);
                                        if (steps_to_go == 0) {
                                                // Движение не требуется — сразу DONE
                                                CAN_SendDone(cmd_code, device_id);
                                                continue;
                                        }

                                        MotionDriver_SetDirection(motor_id, (motor_states[motor_id].direction == 1));
                                        g_motor_active[motor_id] = true;
                                        MotionDriver_StartMotor(motor_id, motor_states[motor_id].max_speed_steps_per_sec);

                                        // TODO: Механизм подсчёта шагов и автоматической остановки + DONE
                                        // будет реализован при интеграции с прерыванием таймера.
                                        // Пока DONE не отправляется автоматически по завершении движения.
                                        break;
                                }

                                case CMD_STOP:
                                {
                                        MotionDriver_StopMotor(motor_id);
                                        g_motor_active[motor_id] = false;
                                        motor_states[motor_id].steps_to_go = 0;
                                        motor_states[motor_id].current_speed_steps_per_sec = 0;

                                        CAN_SendDone(cmd_code, device_id);
                                        break;
                                }

                                case CMD_SET_SPEED:
                                {
                                        motor_states[motor_id].max_speed_steps_per_sec = received_motion_cmd.speed_steps_per_sec;
                                        if (g_motor_active[motor_id]) {
                                                MotionDriver_StartMotor(motor_id, motor_states[motor_id].max_speed_steps_per_sec);
                                        }

                                        CAN_SendDone(cmd_code, device_id);
                                        break;
                                }

                                case CMD_SET_ACCELERATION:
                                {
                                        motor_states[motor_id].acceleration_steps_per_sec2 = received_motion_cmd.acceleration_steps_per_sec2;

                                        CAN_SendDone(cmd_code, device_id);
                                        break;
                                }

                                default:
                                        break;
                        }
                }
        }
  }
