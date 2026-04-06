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

#include "main.h"
#include "cmsis_os.h"
#include "app_queues.h"
#include "app_config.h"
#include "app_flash.h"
#include <string.h>
#include "task_dispatcher.h"
#include "can_protocol.h"
#include "command_protocol.h"

void app_start_task_dispatcher(void *argument)
{
	ParsedCanCommand_t parsed;    // Буфер для принятой команды от CAN Handler
    MotionCommand_t motion_cmd;   // Буфер для команды движения

    for (;;)
    	{
    	// 1. Ожидаем команду из dispatcher_queue
    	if (osMessageQueueGet(dispatcher_queueHandle, &parsed, NULL, osWaitForever) != osOK) {
    		continue;
    		}

        // 2. --- ЗОЛОТОЙ ЭТАЛОН: Немедленное подтверждение приема (ACK) ---
        CAN_SendAck(parsed.cmd_code);

    	// 3. Распределение команд по группам
    	switch (parsed.cmd_code)
    		{
            // ============================================================
            // ГРУППА 0x01xx: УПРАВЛЕНИЕ МОТОРАМИ
            // ============================================================
    		case CAN_CMD_MOTOR_ROTATE:
    		case CAN_CMD_MOTOR_HOME:
    		case CAN_CMD_MOTOR_START_CONTINUOUS:
    		case CAN_CMD_MOTOR_STOP:
            {
                // --- Трансляция через Таблицу Маппинга во Flash ---
                uint8_t physical_motor_id = MOTOR_ID_INVALID;
                for (uint8_t i = 0; i < MOTOR_COUNT; i++) {
                    if (AppConfig_GetMotorLogicalID(i) == parsed.device_id) {
                        physical_motor_id = i;
                        break;
                    }
                }

                if (physical_motor_id == MOTOR_ID_INVALID) {
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
                    // Согласно аудиту Дирижера: speed упакован как speed >> 2
                    motion_cmd.speed_steps_per_sec = (uint32_t)parsed.data[4] << 2;
                } 
                else if (parsed.cmd_code == CAN_CMD_MOTOR_HOME) {
                    uint16_t speed = (uint16_t)(parsed.data[0] | ((uint16_t)parsed.data[1] << 8));
                    motion_cmd.command_id = CMD_MOVE_ABSOLUTE;
                    motion_cmd.steps = 0; 
                    motion_cmd.speed_steps_per_sec = (uint32_t)speed;
                }
                else if (parsed.cmd_code == CAN_CMD_MOTOR_START_CONTINUOUS) {
                    motion_cmd.command_id = CMD_SET_SPEED;
                    motion_cmd.speed_steps_per_sec = (uint32_t)parsed.data[0] * 100;
                }
                else if (parsed.cmd_code == CAN_CMD_MOTOR_STOP) {
                    motion_cmd.command_id = CMD_STOP;
                }

                osMessageQueuePut(motion_queueHandle, &motion_cmd, 0, 0);
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
                data[4] = 0; data[5] = 0;
                CAN_SendData(parsed.cmd_code, data, 6);

                CAN_SendDone(parsed.cmd_code, 0);
                break;
            }

            case CAN_CMD_SRV_REBOOT: {
                uint16_t key = (uint16_t)(parsed.data[0] | (parsed.data[1] << 8));
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

            case 0xF006: { // CAN_CMD_SRV_FACTORY_RESET (Золотой стандарт)
                uint16_t key = (uint16_t)(parsed.data[0] | (parsed.data[1] << 8));
                if (key == 0xDEAD) { // SRV_MAGIC_FACTORY_RESET
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
                    CAN_SendNackPublic(parsed.cmd_code, CAN_ERR_UNKNOWN_CMD); // Исправлено: замена 0x0001
                }
                break;
            }

            case CAN_CMD_SRV_GET_UID: { // Добавлена явная поддержка UID, если она была пропущена
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

    		default:
    			CAN_SendNackPublic(parsed.cmd_code, CAN_ERR_UNKNOWN_CMD);
    			break;
    			}
    	}
}
