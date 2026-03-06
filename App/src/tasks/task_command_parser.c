/*
 * task_command_parser.c
 *
 *  Created on: Dec 8, 2025
 *      Author: andrey
 *
 *  Refactored on: Mar 6, 2026
 *      Author: andrey
 *
 *  Прикладной уровень. Отвечает за:
 *  - Приём ParsedCanCommand_t из parser_queue (от CAN Handler)
 *  - Трансляцию device_id → physical_motor_id
 *  - Парсинг параметров команд (steps, speed, direction)
 *  - Формирование MotionCommand_t → motion_queue
 *  - NACK при невалидном device_id
 */

#include "task_command_parser.h"
#include "main.h"
#include "cmsis_os.h"
#include "app_queues.h"
#include "app_config.h"
#include <string.h>
#include "can_protocol.h"
#include "command_protocol.h"
#include "device_mapping.h"

void app_start_task_command_parser(void *argument)
{
	ParsedCanCommand_t parsed;    // Буфер для принятой команды от CAN Handler
    MotionCommand_t motion_cmd;   // Буфер для команды движения

    for (;;)
    	{
    	// Ожидаем команду из parser_queue
    	if (osMessageQueueGet(parser_queueHandle, &parsed, NULL, osWaitForever) != osOK) {
    		continue;
    		}

    	// Трансляция логического device_id в физический motor_id
    	uint8_t physical_motor_id = DeviceMapping_ToPhysicalId(parsed.device_id);
    	if (physical_motor_id == MOTOR_ID_INVALID) {
    		CAN_SendNackPublic(parsed.cmd_code, CAN_ERR_INVALID_MOTOR_ID);
    		continue;
    		}

    	// Инициализация структуры команды движения
    	memset(&motion_cmd, 0, sizeof(motion_cmd));
    	motion_cmd.motor_id = physical_motor_id;
    	motion_cmd.cmd_code = parsed.cmd_code;
    	motion_cmd.device_id = parsed.device_id;

    	// Парсинг параметров в зависимости от cmd_code
    	switch (parsed.cmd_code)
    		{
    		case CAN_CMD_MOTOR_ROTATE:
    			{
    				// data[0..3] = steps (int32_t LE), data[4] = speed
                    int32_t steps = (int32_t)(parsed.data[0] |
                    ((uint32_t)parsed.data[1] << 8) |
					((uint32_t)parsed.data[2] << 16) |
					((uint32_t)parsed.data[3] << 24));

                    motion_cmd.command_id = CMD_MOVE_RELATIVE;
                    motion_cmd.steps = (uint32_t)((steps >= 0) ? steps : -steps);
                    motion_cmd.direction = (steps >= 0) ? 1 : 0;
                    motion_cmd.speed_steps_per_sec = (uint32_t)parsed.data[4] * 100;

                    osMessageQueuePut(motion_queueHandle, &motion_cmd, 0, 0);
                    break;
                    }

    		case CAN_CMD_MOTOR_HOME:
    			{
    				// data[0..1] = speed (uint16_t LE)
    				uint16_t speed = (uint16_t)(parsed.data[0] |
    						((uint16_t)parsed.data[1] << 8));

    				motion_cmd.command_id = CMD_MOVE_ABSOLUTE;
    				motion_cmd.steps = 0; // Целевая позиция = 0 (домой)
    				motion_cmd.speed_steps_per_sec = (uint32_t)speed;

    				osMessageQueuePut(motion_queueHandle, &motion_cmd, 0, 0);
    				break;
    				}

    		case CAN_CMD_MOTOR_START_CONTINUOUS:
    			{
    				// data[0] = speed (uint8_t)
    				motion_cmd.command_id = CMD_SET_SPEED;
    				motion_cmd.speed_steps_per_sec = (uint32_t)parsed.data[0] * 100;

    				osMessageQueuePut(motion_queueHandle, &motion_cmd, 0, 0);
    				break;
    				}

    		case CAN_CMD_MOTOR_STOP:
    			{
    				motion_cmd.command_id = CMD_STOP;
    				osMessageQueuePut(motion_queueHandle, &motion_cmd, 0, 0);
    				break;
    				}

    		default:
    			CAN_SendNackPublic(parsed.cmd_code, CAN_ERR_UNKNOWN_CMD);
    			break;
    			}
    	}
}


