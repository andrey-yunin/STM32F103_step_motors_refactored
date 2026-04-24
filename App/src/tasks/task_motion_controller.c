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
#include "app_queues.h"
#include "motion_driver.h"
#include "command_protocol.h"
#include "can_protocol.h"
#include "watchdog.h"
#include <string.h>


#define MOTION_GROUP_COUNT 2U
#define MOTION_GROUP_NONE  0xFFU

typedef struct {
    uint8_t active_motor_id;       // Какой мотор сейчас владеет TIM-группой.
    uint16_t active_cmd_code;      // Low-level CAN command, для которого надо отправить DONE.
    uint8_t active_device_id;      // Device/channel id, возвращается в DONE.
    uint32_t target_speed_sps;     // Скорость из атомарной команды Дирижера.
    uint32_t current_speed_sps;    // Позже сюда ляжет acceleration/deceleration profile.
    uint32_t target_steps;         // Количество STEP, которое должен выдать driver.
} MotionGroupState_t;

static MotionGroupState_t g_motion_groups[MOTION_GROUP_COUNT];

static uint8_t MotionController_GetGroupByMotor(uint8_t motor_id)
{
    // Моторы 0..3 физически сидят на TIM1, моторы 4..7 на TIM2.
    return (motor_id < 4U) ? 0U : 1U;
}

static void MotionController_ResetGroup(MotionGroupState_t *group)
{
    if (group == NULL) {
        return;
    }

    group->active_motor_id = MOTION_GROUP_NONE;
    group->active_cmd_code = 0U;
    group->active_device_id = 0U;
    group->target_speed_sps = 0U;
    group->current_speed_sps = 0U;
    group->target_steps = 0U;
}

static void MotionController_InitGroups(void)
{
    for (uint8_t i = 0; i < MOTION_GROUP_COUNT; i++) {
        MotionController_ResetGroup(&g_motion_groups[i]);
    }
}

static bool MotionController_IsGroupBusy(uint8_t group_id)
{
    if (group_id >= MOTION_GROUP_COUNT) {
        return true;
    }

    return g_motion_groups[group_id].active_motor_id != MOTION_GROUP_NONE;
}

static bool MotionController_IsSpeedValid(uint32_t speed_sps)
{
    // speed=0 для ненулевого ROTATE недопустим: движение никогда не завершится.
    if (speed_sps == 0U) {
        return false;
    }

    // Верхний предел - локальный safe-limit Motion Executor.
    // Дирижер выбирает скорость, но исполнитель обязан защитить свою плату.
    if (speed_sps > MOTION_MAX_SAFE_SPEED_STEPS_PER_SEC) {
        return false;
    }

    return true;
}


// --- Инкапсулированные данные (скрыты от других модулей) ---
static MotorMotionState_t motor_states[MOTOR_COUNT];
static volatile bool g_motor_active[MOTOR_COUNT] = {false};

#if APP_WATCHDOG_TEST_STALL_MOTION_AFTER_ROTATE
static void MotionController_TestStallForWatchdog(uint8_t motor_id)
{
    if (motor_id != APP_WATCHDOG_TEST_STALL_MOTION_MOTOR_ID) {
        return;
    }

    for (;;) {
        osDelay(APP_WATCHDOG_TASK_IDLE_TIMEOUT_MS);
    }
}
#endif

// --- Публичный API доступа к состоянию (Thread-safe) ---

bool MotionController_IsMotorActive(uint8_t motor_id)
{
    if (motor_id >= MOTOR_COUNT) {
        return false;
    }

    return g_motor_active[motor_id];
}

void MotionController_SetMotorActive(uint8_t motor_id, bool active)
{
    if (motor_id >= MOTOR_COUNT) {
        return;
    }

    g_motor_active[motor_id] = active;
}

void MotionController_GetMotorState(uint8_t motor_id, MotorMotionState_t *out_state)
{
    if (motor_id >= MOTOR_COUNT || out_state == NULL) {
        return;
    }

    memcpy(out_state, &motor_states[motor_id], sizeof(MotorMotionState_t));
}

static void MotionController_ProcessCompletedMove(uint8_t motor_id)
{
    if (motor_id >= MOTOR_COUNT) {
        return;
    }

    uint8_t group_id = MotionController_GetGroupByMotor(motor_id);
    MotionGroupState_t *group = &g_motion_groups[group_id];

    if (group->active_motor_id != motor_id) {
        // Stale completion: например, движение было отменено STOP до обработки флага.
        return;
    }

    // Обновляем модель оси только после фактического completion.
    motor_states[motor_id].current_position = motor_states[motor_id].target_position;
    motor_states[motor_id].steps_to_go = 0;
    motor_states[motor_id].current_speed_steps_per_sec = 0;
    g_motor_active[motor_id] = false;

    CAN_SendDone(group->active_cmd_code, group->active_device_id);

    // Освобождаем общий TIM resource.
    MotionController_ResetGroup(group);
}

static void MotionController_ProcessDriverCompletions(void)
{
    uint8_t completed_motor_id;

    // ISR только ставит флаг completion; все CAN-ответы уходят отсюда, из task context.
    while (MotionDriver_ConsumeCompletedMotor(&completed_motor_id)) {
        MotionController_ProcessCompletedMove(completed_motor_id);
    }
}

void app_start_task_motion_controller(void *argument)
{
    MotionCommand_t received_motion_cmd;

    // --- Инициализация состояний моторов и драйвера ---
    for (uint8_t i = 0; i < MOTOR_COUNT; i++) {
        MotionPlanner_InitMotorState(&motor_states[i], 0);
    }
    MotionController_InitGroups();

    MotionDriver_Init();
    AppWatchdog_Heartbeat(APP_WDG_CLIENT_MOTION);

    // --- Основной цикл ---
    for (;;) {
        AppWatchdog_Heartbeat(APP_WDG_CLIENT_MOTION);
        MotionController_ProcessDriverCompletions();

        if (osMessageQueueGet(motion_queueHandle, &received_motion_cmd, NULL,
                              MOTION_COMPLETION_POLL_TIMEOUT_MS) == osOK) {
            AppWatchdog_Heartbeat(APP_WDG_CLIENT_MOTION);
            MotionController_ProcessDriverCompletions();

            uint8_t motor_id = received_motion_cmd.motor_id;
            uint16_t cmd_code = received_motion_cmd.cmd_code;
            uint8_t device_id = received_motion_cmd.device_id;

            // --- Валидация motor_id ---
            if (motor_id >= MOTOR_COUNT) {
                CAN_SendNackPublic(cmd_code, CAN_ERR_INVALID_MOTOR_ID);
                continue;
            }

            uint8_t group_id = MotionController_GetGroupByMotor(motor_id);

            // --- Проверка занятости мотора ---
            // CMD_STOP разрешён даже если мотор занят
            if (received_motion_cmd.command_id != CMD_STOP && g_motor_active[motor_id]) {
                CAN_SendNackPublic(cmd_code, CAN_ERR_MOTOR_BUSY);
                continue;
            }

            if (received_motion_cmd.command_id != CMD_STOP && MotionController_IsGroupBusy(group_id)) {
                // TIM1 обслуживает моторы 0..3, TIM2 - 4..7; частота общая для всей группы.
                CAN_SendNackPublic(cmd_code, CAN_ERR_MOTOR_BUSY);
                continue;
            }

            // --- Обработка команды ---
            switch (received_motion_cmd.command_id) {
            case CMD_MOVE_ABSOLUTE:
            case CMD_MOVE_RELATIVE: {
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
                    // Движение не требуется: контракт finite-команды выполнен без запуска STEP.
                    CAN_SendDone(cmd_code, device_id);
                    continue;
                }

                if (!MotionController_IsSpeedValid(received_motion_cmd.speed_steps_per_sec)) {
                    CAN_SendNackPublic(cmd_code, CAN_ERR_INVALID_PARAM);
                    continue;
                }

                motor_states[motor_id].max_speed_steps_per_sec = received_motion_cmd.speed_steps_per_sec;
                motor_states[motor_id].current_speed_steps_per_sec = received_motion_cmd.speed_steps_per_sec;

                g_motion_groups[group_id].active_motor_id = motor_id;
                g_motion_groups[group_id].active_cmd_code = cmd_code;
                g_motion_groups[group_id].active_device_id = device_id;
                g_motion_groups[group_id].target_speed_sps = received_motion_cmd.speed_steps_per_sec;
                g_motion_groups[group_id].current_speed_sps = received_motion_cmd.speed_steps_per_sec;
                g_motion_groups[group_id].target_steps = (uint32_t)steps_to_go;

                MotionDriver_SetDirection(motor_id, (motor_states[motor_id].direction == 1));
                g_motor_active[motor_id] = true;

                if (!MotionDriver_StartFinite(motor_id,
                                              received_motion_cmd.speed_steps_per_sec,
                                              (uint32_t)steps_to_go)) {
                    g_motor_active[motor_id] = false;
                    motor_states[motor_id].steps_to_go = 0;
                    motor_states[motor_id].current_speed_steps_per_sec = 0;
                    MotionController_ResetGroup(&g_motion_groups[group_id]);
                    CAN_SendNackPublic(cmd_code, CAN_ERR_INVALID_PARAM);
                    continue;
                }

#if APP_WATCHDOG_TEST_STALL_MOTION_AFTER_ROTATE
                MotionController_TestStallForWatchdog(motor_id);
#endif

                // DONE не отправляем здесь: finite-команда завершится только после N STEP.
                // TIM ISR остановит PWM и выставит completion flag, task отправит DONE.
                break;
            }

            case CMD_STOP: {
                MotionDriver_StopMotor(motor_id);
                g_motor_active[motor_id] = false;
                motor_states[motor_id].steps_to_go = 0;
                motor_states[motor_id].current_speed_steps_per_sec = 0;

                if (g_motion_groups[group_id].active_motor_id == motor_id) {
                    MotionController_ResetGroup(&g_motion_groups[group_id]);
                }

                CAN_SendDone(cmd_code, device_id);
                break;
            }

            case CMD_SET_SPEED: {
                if (received_motion_cmd.speed_steps_per_sec > MOTION_MAX_SAFE_SPEED_STEPS_PER_SEC) {
                    CAN_SendNackPublic(cmd_code, CAN_ERR_INVALID_PARAM);
                    break;
                }

                motor_states[motor_id].max_speed_steps_per_sec = received_motion_cmd.speed_steps_per_sec;

                if (cmd_code == CAN_CMD_MOTOR_START_CONTINUOUS &&
                    received_motion_cmd.speed_steps_per_sec > 0U) {
                    // START_CONTINUOUS - это state-команда: DONE означает, что режим включен.
                    // Направление не приходит в payload 0x0103, поэтому используем текущее состояние оси.
                    MotionDriver_SetDirection(motor_id, (motor_states[motor_id].direction == 1));

                    g_motion_groups[group_id].active_motor_id = motor_id;
                    g_motion_groups[group_id].active_cmd_code = cmd_code;
                    g_motion_groups[group_id].active_device_id = device_id;
                    g_motion_groups[group_id].target_speed_sps = received_motion_cmd.speed_steps_per_sec;
                    g_motion_groups[group_id].current_speed_sps = received_motion_cmd.speed_steps_per_sec;
                    g_motion_groups[group_id].target_steps = 0U;

                    motor_states[motor_id].current_speed_steps_per_sec = received_motion_cmd.speed_steps_per_sec;
                    g_motor_active[motor_id] = true;
                    MotionDriver_StartMotor(motor_id, received_motion_cmd.speed_steps_per_sec);
                }

                CAN_SendDone(cmd_code, device_id);
                break;
            }

            case CMD_SET_ACCELERATION: {
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
