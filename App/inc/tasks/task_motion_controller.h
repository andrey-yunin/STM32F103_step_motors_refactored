/*
 * task_motion_controller.h
 *
 * Внутренний словарь Dispatcher -> Motion Controller. Не является
 * CAN-протоколом: коды 0x01xx живут в can_protocol.h.
 */
#ifndef TASK_MOTION_CONTROLLER_H_
#define TASK_MOTION_CONTROLLER_H_

#include <stdint.h>

typedef enum {
    CMD_MOVE_ABSOLUTE       = 0x01,
    CMD_MOVE_RELATIVE       = 0x02,
    CMD_SET_SPEED           = 0x03,
    CMD_SET_ACCELERATION    = 0x04,
    CMD_STOP                = 0x05,
    CMD_GET_STATUS          = 0x06,
    CMD_SET_CURRENT         = 0x07,
    CMD_ENABLE_MOTOR        = 0x08,
    CMD_PERFORMER_ID_SET    = 0x09,
} CommandID_t;

// Основная функция-обработчик для задачи TaskMotionController
void app_start_task_motion_controller(void *argument);

#endif /* TASK_MOTION_CONTROLLER_H_ */
