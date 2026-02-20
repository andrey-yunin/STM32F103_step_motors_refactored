/*
 * app_queues.h
 *
 *  Created on: Dec 9, 2025
 *      Author: andrey
 */

#ifndef APP_QUEUES_H_
#define APP_QUEUES_H_

#include "cmsis_os.h" // Для osMessageQueueId_t

// Глобальные хэндлы для всех очередей FreeRTOS
extern osMessageQueueId_t can_rx_queueHandle;      // Для приема сырых CAN-фреймов (ISR -> CAN Handler)
extern osMessageQueueId_t can_tx_queueHandle;      // Для отправки CAN-сообщений (любая задача -> CAN Handler)
extern osMessageQueueId_t parser_queueHandle;      // Для передачи CAN-фреймов (CAN Handler -> Command Parser)
extern osMessageQueueId_t motion_queueHandle;      // Для передачи заданий движения (Command Parser -> Motion Controller)
extern osMessageQueueId_t tmc_manager_queueHandle; // Для передачи команд TMC (Command Parser -> TMC Manager)

#endif /* APP_QUEUES_H_ */
