/*
 * task_dispatcher.c
 *
 *  Created on: Dec 8, 2025
 *      Author: andrey
 *
 *  Refactored on: Mar 6, 2026
 *      Author: andrey
 *
 *  Прикладной уровень. Отвечает за:
 *  - Приём ParsedCanCommand_t из dispatcher_queue (от CAN Handler)
 *  - Жизненный цикл команды (ACK -> [DATA] -> DONE/NACK)
 *  - Универсальные сервисные команды (0xF0xx)
 *  - Трансляцию Глобальный ID -> Физический индекс через Flash Mapping
 *  - Формирование MotionCommand_t → motion_queue
 */

#include <string.h>

#include "main.h"
#include "cmsis_os.h"
#include "app_queues.h"
#include "app_config.h"
#include "app_flash.h"
#include "task_dispatcher.h"
#include "task_can_handler.h"
#include "can_protocol.h"
#include "command_protocol.h"
#include "watchdog.h"

// Отправляет одну метрику GET_STATUS.
//
// Формат DATA после служебных байтов CAN_SendData():
// byte 2..3: metric_id, uint16 little-endian
// byte 4..7: value, uint32 little-endian
static void SendStatusMetric(uint16_t cmd_code, uint16_t metric_id, uint32_t value)
{
    uint8_t data[6];

    data[0] = (uint8_t)(metric_id & 0xFF);
    data[1] = (uint8_t)((metric_id >> 8) & 0xFF);
    data[2] = (uint8_t)(value & 0xFF);
    data[3] = (uint8_t)((value >> 8) & 0xFF);
    data[4] = (uint8_t)((value >> 16) & 0xFF);
    data[5] = (uint8_t)((value >> 24) & 0xFF);

    CAN_SendData(cmd_code, data, sizeof(data));
}

void app_start_task_dispatcher(void *argument)
{
    ParsedCanCommand_t parsed;  // Буфер для принятой команды от CAN Handler
    MotionCommand_t motion_cmd; // Буфер для команды движения

    for (;;) {
        // 1. Ожидаем команду из dispatcher_queue
        AppWatchdog_Heartbeat(APP_WDG_CLIENT_DISPATCHER);
        if (osMessageQueueGet(dispatcher_queueHandle, &parsed, NULL,
                              APP_WATCHDOG_TASK_IDLE_TIMEOUT_MS) != osOK) {
            continue;
        }
        AppWatchdog_Heartbeat(APP_WDG_CLIENT_DISPATCHER);

        // 2. --- ЗОЛОТОЙ ЭТАЛОН: Немедленное подтверждение приема (ACK) ---
        CAN_SendAck(parsed.cmd_code);

        // 3. Распределение команд по группам
        switch (parsed.cmd_code) {
        // ============================================================
        // ГРУППА 0x01xx: УПРАВЛЕНИЕ МОТОРАМИ
        // ============================================================
        case CAN_CMD_MOTOR_ROTATE:
        case CAN_CMD_MOTOR_HOME:
        case CAN_CMD_MOTOR_START_CONTINUOUS:
        case CAN_CMD_MOTOR_STOP: {
            // Директива 2.0: Используем 0-based индекс напрямую (device_id == physical_id)
            uint8_t physical_motor_id = parsed.device_id;

            if (physical_motor_id >= MOTOR_COUNT) {
                CAN_SendNackPublic(parsed.cmd_code, CAN_ERR_INVALID_MOTOR_ID);
                continue;
            }

            // Инициализация структуры команды движения
            memset(&motion_cmd, 0, sizeof(motion_cmd));
            motion_cmd.motor_id = physical_motor_id;
            motion_cmd.cmd_code = parsed.cmd_code;
            motion_cmd.device_id = parsed.device_id;

            // Парсинг параметров в зависимости от кода
            if (parsed.cmd_code == CAN_CMD_MOTOR_ROTATE) {
                int32_t steps = (int32_t)(parsed.data[0] |
                                          ((uint32_t)parsed.data[1] << 8) |
                                          ((uint32_t)parsed.data[2] << 16) |
                                          ((uint32_t)parsed.data[3] << 24));

                motion_cmd.command_id = CMD_MOVE_RELATIVE;
                motion_cmd.steps = (uint32_t)((steps >= 0) ? steps : -steps);
                motion_cmd.direction = (steps >= 0) ? 1 : 0;
                // Согласно Директиве 2.0: speed упакован как speed >> 2 (в байте data[4])
                motion_cmd.speed_steps_per_sec = (uint32_t)parsed.data[4] << 2;
            } else if (parsed.cmd_code == CAN_CMD_MOTOR_HOME) {
                uint16_t speed = (uint16_t)(parsed.data[0] | ((uint16_t)parsed.data[1] << 8));
                motion_cmd.command_id = CMD_MOVE_ABSOLUTE;
                motion_cmd.steps = 0;
                motion_cmd.speed_steps_per_sec = (uint32_t)speed;
            } else if (parsed.cmd_code == CAN_CMD_MOTOR_START_CONTINUOUS) {
                motion_cmd.command_id = CMD_SET_SPEED;
                // Согласно Директиве 2.0: speed упакован как speed / 100
                motion_cmd.speed_steps_per_sec = (uint32_t)parsed.data[0] * 100;
            } else if (parsed.cmd_code == CAN_CMD_MOTOR_STOP) {
                motion_cmd.command_id = CMD_STOP;
            }

            if (osMessageQueuePut(motion_queueHandle, &motion_cmd, 0, 0) != osOK) {
                CAN_Diagnostics_RecordAppQueueOverflow();
                CAN_SendNackPublic(parsed.cmd_code, CAN_ERR_MOTOR_BUSY);
            }
            break;
        }

        // ============================================================
        // ГРУППА 0xF0xx: УНИВЕРСАЛЬНЫЕ СЕРВИСНЫЕ КОМАНДЫ
        // ============================================================
        case CAN_CMD_SRV_GET_DEVICE_INFO: {
            uint8_t uid[12];
            uint8_t data[6];
            AppConfig_GetMCU_UID(uid);

            // Пакет 1: Метаданные (Тип, Версия, Каналы) + начало UID
            data[0] = CAN_DEVICE_TYPE_MOTION;
            data[1] = FW_REV_MAJOR;
            data[2] = FW_REV_MINOR;
            data[3] = MOTOR_COUNT;
            data[4] = uid[0];
            data[5] = uid[1];
            CAN_SendData(parsed.cmd_code, data, 6);

            // Пакет 2: Середина UID
            memcpy(data, &uid[2], 6);
            CAN_SendData(parsed.cmd_code, data, 6);

            // Пакет 3: Конец UID
            memcpy(data, &uid[8], 4);
            data[4] = 0;
            data[5] = 0;
            CAN_SendData(parsed.cmd_code, data, 6);

            CAN_SendDone(parsed.cmd_code, 0);
            break;
        }

        case CAN_CMD_SRV_REBOOT: {
            uint16_t key = (uint16_t)(parsed.data[0] | ((uint16_t)parsed.data[1] << 8));
            if (key == SRV_MAGIC_REBOOT) {
                CAN_SendDone(parsed.cmd_code, 0);
                osDelay(100);
                NVIC_SystemReset();
            } else {
                CAN_SendNackPublic(parsed.cmd_code, CAN_ERR_INVALID_KEY);
            }
            break;
        }

        case CAN_CMD_SRV_FLASH_COMMIT: {
            if (AppConfig_Commit()) {
                CAN_SendDone(parsed.cmd_code, 0);
            } else {
                CAN_SendNackPublic(parsed.cmd_code, CAN_ERR_FLASH_WRITE);
            }
            break;
        }

        case CAN_CMD_SRV_FACTORY_RESET: {
            uint16_t key = (uint16_t)(parsed.data[0] | ((uint16_t)parsed.data[1] << 8));
            if (key == SRV_MAGIC_FACTORY_RESET) {
                AppConfig_FactoryReset();
                CAN_SendDone(parsed.cmd_code, 0);
                osDelay(100);
                NVIC_SystemReset();
            } else {
                CAN_SendNackPublic(parsed.cmd_code, CAN_ERR_INVALID_KEY);
            }
            break;
        }

        case CAN_CMD_SRV_SET_NODE_ID: {
            if (parsed.device_id >= 0x02 && parsed.device_id <= 0x7F && parsed.device_id != 0x10) {
                AppConfig_SetPerformerID(parsed.device_id);
                CAN_SendDone(parsed.cmd_code, parsed.device_id);
            } else {
                CAN_SendNackPublic(parsed.cmd_code, CAN_ERR_UNKNOWN_CMD);
            }
            break;
        }

        case CAN_CMD_SRV_GET_UID: {
            uint8_t uid[12];
            uint8_t data[6];
            AppConfig_GetMCU_UID(uid);

            // Пакет 1: UID[0..5]
            memcpy(data, &uid[0], 6);
            CAN_SendData(parsed.cmd_code, data, 6);

            // Пакет 2: UID[6..11]
            memcpy(data, &uid[6], 6);
            CAN_SendData(parsed.cmd_code, data, 6);

            CAN_SendDone(parsed.cmd_code, 0);
            break;
        }

        case CAN_CMD_SRV_GET_STATUS: {
            CanDiagnostics_t diag;
            CAN_Diagnostics_GetSnapshot(&diag);

            SendStatusMetric(parsed.cmd_code, CAN_STATUS_RX_TOTAL, diag.rx_total);
            SendStatusMetric(parsed.cmd_code, CAN_STATUS_TX_TOTAL, diag.tx_total);
            SendStatusMetric(parsed.cmd_code, CAN_STATUS_RX_QUEUE_OVERFLOW, diag.rx_queue_overflow);
            SendStatusMetric(parsed.cmd_code, CAN_STATUS_TX_QUEUE_OVERFLOW, diag.tx_queue_overflow);
            SendStatusMetric(parsed.cmd_code, CAN_STATUS_DISPATCHER_OVERFLOW, diag.dispatcher_queue_overflow);
            SendStatusMetric(parsed.cmd_code, CAN_STATUS_DROP_NOT_EXT, diag.dropped_not_ext);
            SendStatusMetric(parsed.cmd_code, CAN_STATUS_DROP_WRONG_DST, diag.dropped_wrong_dst);
            SendStatusMetric(parsed.cmd_code, CAN_STATUS_DROP_WRONG_TYPE, diag.dropped_wrong_type);
            SendStatusMetric(parsed.cmd_code, CAN_STATUS_DROP_WRONG_DLC, diag.dropped_wrong_dlc);
            SendStatusMetric(parsed.cmd_code, CAN_STATUS_TX_MAILBOX_TIMEOUT, diag.tx_mailbox_timeout);
            SendStatusMetric(parsed.cmd_code, CAN_STATUS_TX_HAL_ERROR, diag.tx_hal_error);
            SendStatusMetric(parsed.cmd_code, CAN_STATUS_ERROR_CALLBACK, diag.can_error_callback_count);
            SendStatusMetric(parsed.cmd_code, CAN_STATUS_ERROR_WARNING, diag.error_warning_count);
            SendStatusMetric(parsed.cmd_code, CAN_STATUS_ERROR_PASSIVE, diag.error_passive_count);
            SendStatusMetric(parsed.cmd_code, CAN_STATUS_BUS_OFF, diag.bus_off_count);
            SendStatusMetric(parsed.cmd_code, CAN_STATUS_LAST_HAL_ERROR, diag.last_hal_error);
            SendStatusMetric(parsed.cmd_code, CAN_STATUS_LAST_ESR, diag.last_esr);
            SendStatusMetric(parsed.cmd_code, CAN_STATUS_APP_QUEUE_OVERFLOW, diag.app_queue_overflow);

            CAN_SendDone(parsed.cmd_code, 0);
            break;
        }

        default:
            CAN_SendNackPublic(parsed.cmd_code, CAN_ERR_UNKNOWN_CMD);
            break;
        }
    }
}
