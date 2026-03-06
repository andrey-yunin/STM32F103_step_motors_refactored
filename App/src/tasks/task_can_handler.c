/*
 * task_can_handler.c
 *
 *  Created on: Dec 8, 2025
 *      Author: andrey
 *
 *   Refactored  on: Mar 5, 2026
 *      Author: andrey
 *   Транспортный уровень CAN. Отвечает за:
 *   - Приём CAN-фреймов, базовую валидацию, упаковку в ParsedCanCommand_t → parser_queue
 *   - Отправку CAN-фреймов из can_tx_queue в CAN-периферию
 *   - Event-driven обработка через osThreadFlags (FLAG_CAN_RX, FLAG_CAN_TX)
 */


 #include "task_can_handler.h"
 #include "main.h"
 #include "cmsis_os.h"
 #include "app_queues.h"
 #include "app_config.h"
 #include "can_protocol.h"

 // --- Внешние хэндлы HAL ---
 extern CAN_HandleTypeDef hcan;

// ============================================================
// Вспомогательные функции
// ============================================================

 /**
  * @brief Отправляет NACK-ответ дирижеру.
  * @param cmd_code Код команды, на которую отвечаем
  * @param error_code Код ошибки
  */

 static void CAN_SendNack(uint16_t cmd_code, uint16_t error_code)
 {
	 CanTxFrame_t tx;
     tx.header.ExtId = CAN_BUILD_ID(CAN_PRIORITY_NORMAL,
    		 CAN_MSG_TYPE_NACK,
			 CAN_ADDR_CONDUCTOR,
			 CAN_ADDR_MOTOR_BOARD);
     tx.header.IDE = CAN_ID_EXT;
     tx.header.RTR = CAN_RTR_DATA;
     tx.header.DLC = 4;
     tx.data[0] = (uint8_t)(cmd_code & 0xFF);
     tx.data[1] = (uint8_t)((cmd_code >> 8) & 0xFF);
     tx.data[2] = (uint8_t)(error_code & 0xFF);
     tx.data[3] = (uint8_t)((error_code >> 8) & 0xFF);

     osMessageQueuePut(can_tx_queueHandle, &tx, 0, 0);
     osThreadFlagsSet(task_can_handleHandle, FLAG_CAN_TX);
}

 /**
  * @brief Отправляет DONE-ответ дирижеру (задача выполнена).
  * @param cmd_code Код команды, которая завершена
  * @param device_id Логический ID устройства
  */
void CAN_SendDone(uint16_t cmd_code, uint8_t device_id)
{
	CanTxFrame_t tx;
    tx.header.ExtId = CAN_BUILD_ID(CAN_PRIORITY_NORMAL,
    		CAN_MSG_TYPE_DATA_DONE_LOG,
			CAN_ADDR_CONDUCTOR,
			CAN_ADDR_MOTOR_BOARD);
    tx.header.IDE = CAN_ID_EXT;
    tx.header.RTR = CAN_RTR_DATA;
    tx.header.DLC = 4;
    tx.data[0] = CAN_SUB_TYPE_DONE;
    tx.data[1] = (uint8_t)(cmd_code & 0xFF);
    tx.data[2] = (uint8_t)((cmd_code >> 8) & 0xFF);
    tx.data[3] = device_id;

    osMessageQueuePut(can_tx_queueHandle, &tx, 0, 0);
    osThreadFlagsSet(task_can_handleHandle, FLAG_CAN_TX);
    }

 /**
  * @brief Отправляет ACK-ответ дирижеру.
  * @param cmd_code Код команды, на которую отвечаем
  */
void CAN_SendAck(uint16_t cmd_code)
{
	CanTxFrame_t tx;
	tx.header.ExtId = CAN_BUILD_ID(CAN_PRIORITY_NORMAL,
			CAN_MSG_TYPE_ACK,
			CAN_ADDR_CONDUCTOR,
			CAN_ADDR_MOTOR_BOARD);
	tx.header.IDE = CAN_ID_EXT;
	tx.header.RTR = CAN_RTR_DATA;
	tx.header.DLC = 2;
	tx.data[0] = (uint8_t)(cmd_code & 0xFF);
	tx.data[1] = (uint8_t)((cmd_code >> 8) & 0xFF);

	osMessageQueuePut(can_tx_queueHandle, &tx, 0, 0);
    osThreadFlagsSet(task_can_handleHandle, FLAG_CAN_TX);
}

 /**
  * @brief Отправляет NACK-ответ дирижеру (публичная обёртка, вызывается из других задач).
  * @param cmd_code Код команды
  * @param error_code Код ошибки
  */
void CAN_SendNackPublic(uint16_t cmd_code, uint16_t error_code)
{
	CAN_SendNack(cmd_code, error_code);
	}

// ============================================================
// Основная задача
// ============================================================

void app_start_task_can_handler(void *argument)
{
	CanRxFrame_t rx_frame;
    CanTxFrame_t tx_frame;
    uint32_t txMailbox;

    // --- Настройка CAN-фильтра ---
    // Фильтруем по DstAddr = CAN_ADDR_MOTOR_BOARD (0x20) в 29-bit Extended ID
    // DstAddr находится в битах [23:16] CAN ID
    // FilterId и FilterMask в регистрах STM32 для 32-bit scale:
    //   Регистр = (ExtId << 3) | CAN_ID_EXT

    CAN_FilterTypeDef sFilterConfig;

    uint32_t filter_id   = ((uint32_t)CAN_ADDR_MOTOR_BOARD << 16) << 3 | CAN_ID_EXT;
    uint32_t filter_mask = ((uint32_t)0xFF << 16) << 3 | CAN_ID_EXT;

    sFilterConfig.FilterBank = 0;
    sFilterConfig.FilterMode = CAN_FILTERMODE_IDMASK;
    sFilterConfig.FilterScale = CAN_FILTERSCALE_32BIT;
    sFilterConfig.FilterIdHigh = (uint16_t)(filter_id >> 16);
    sFilterConfig.FilterIdLow = (uint16_t)(filter_id & 0xFFFF);
    sFilterConfig.FilterMaskIdHigh = (uint16_t)(filter_mask >> 16);
    sFilterConfig.FilterMaskIdLow = (uint16_t)(filter_mask & 0xFFFF);
    sFilterConfig.FilterFIFOAssignment = CAN_RX_FIFO0;
    sFilterConfig.FilterActivation = ENABLE;
    sFilterConfig.SlaveStartFilterBank = 14;

    if (HAL_CAN_ConfigFilter(&hcan, &sFilterConfig) != HAL_OK) {
    	Error_Handler();
    	}

    // --- Запуск CAN ---
    if (HAL_CAN_Start(&hcan) != HAL_OK) {
    	Error_Handler();
    	}

    // --- Активация прерываний RX FIFO0 ---
    if (HAL_CAN_ActivateNotification(&hcan, CAN_IT_RX_FIFO0_MSG_PENDING) != HAL_OK) {
    	Error_Handler();
    	}

    // --- Основной цикл (event-driven) ---
    for (;;)
    	{
    	// Ожидаем любой из флагов: RX или TX
    	uint32_t flags = osThreadFlagsWait(
    			FLAG_CAN_RX | FLAG_CAN_TX,
				osFlagsWaitAny,
				osWaitForever
				);

    	// === Обработка входящих сообщений (RX) ===
    	if (flags & FLAG_CAN_RX)
    		{
    		while (osMessageQueueGet(can_rx_queueHandle, &rx_frame, NULL, 0) == osOK)
    			{
    			// Проверяем Extended ID
    			if (rx_frame.header.IDE != CAN_ID_EXT) {
    				continue;
    				}

    			// Извлекаем поля из 29-bit CAN ID
    			uint32_t can_id = rx_frame.header.ExtId;
                uint8_t msg_type = CAN_GET_MSG_TYPE(can_id);
                uint8_t dst_addr = CAN_GET_DST_ADDR(can_id);

                // Проверяем адресацию
                if (dst_addr != CAN_ADDR_MOTOR_BOARD && dst_addr != CAN_ADDR_BROADCAST) {
                	continue;
                	}

                // Обрабатываем только команды
                if (msg_type != CAN_MSG_TYPE_COMMAND) {
                	continue;
                	}

                // Минимальная валидация: DLC >= 3 (cmd_code:2 + device_id:1)
                if (rx_frame.header.DLC < 3) {
                	continue;
                	}

                // Упаковываем в ParsedCanCommand_t
                ParsedCanCommand_t parsed;
                parsed.cmd_code = (uint16_t)(rx_frame.data[0] |
                		((uint16_t)rx_frame.data[1] << 8));
                parsed.device_id = rx_frame.data[2];
                parsed.data_len = (rx_frame.header.DLC > 3) ? (rx_frame.header.DLC - 3) : 0;

                for (uint8_t i = 0; i < parsed.data_len && i < 5; i++) {
                	parsed.data[i] = rx_frame.data[3 + i];
                	}

                // Отправляем в parser_queue
                osMessageQueuePut(parser_queueHandle, &parsed, 0, 0);
                }
    		}

    	// === Обработка исходящих сообщений (TX) ===
    	if (flags & FLAG_CAN_TX)
    		{
    		while (osMessageQueueGet(can_tx_queueHandle, &tx_frame, NULL, 0) == osOK)
    			{
    			HAL_CAN_AddTxMessage(&hcan, &tx_frame.header, tx_frame.data, &txMailbox);
    			}
    		}
    	}
}





