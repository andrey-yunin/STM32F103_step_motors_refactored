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

#include <string.h>

#include "task_can_handler.h"
#include "main.h"
#include "app_queues.h"
#include "app_flash.h"
#include "app_config.h"
#include "can_protocol.h"
#include "watchdog.h"

// --- Внешние хэндлы HAL ---
extern CAN_HandleTypeDef hcan;

// Счетчики CAN diagnostics живут в transport layer: здесь видны drops,
// переполнения очередей, TX mailbox/HAL errors и CAN fault states.
static volatile CanDiagnostics_t g_can_diag;

// Единая точка постановки CAN-фрейма в TX-очередь.
//
// Зачем:
// - все ACK/NACK/DONE/DATA проходят через один путь;
// - флаг CAN-задаче ставится только если фрейм реально попал в очередь;
// - если очередь переполнена, фиксируем это в GET_STATUS diagnostics.
static void CAN_QueueTxFrame(CanTxFrame_t *tx)
{
    if (osMessageQueuePut(can_tx_queueHandle, tx, 0, 0) == osOK) {
        osThreadFlagsSet(task_can_handleHandle, FLAG_CAN_TX);
    } else {
        g_can_diag.tx_queue_overflow++;
    }
}

void CAN_Diagnostics_GetSnapshot(CanDiagnostics_t *out)
{
    if (out == NULL) {
        return;
    }

    __disable_irq();
    memcpy(out, (const void *)&g_can_diag, sizeof(CanDiagnostics_t));
    __enable_irq();
}

void CAN_Diagnostics_RecordRxQueueOverflow(void)
{
    g_can_diag.rx_queue_overflow++;
}

void CAN_Diagnostics_RecordAppQueueOverflow(void)
{
    g_can_diag.app_queue_overflow++;
}

void CAN_Diagnostics_RecordCanError(uint32_t hal_error, uint32_t esr)
{
    g_can_diag.can_error_callback_count++;
    g_can_diag.last_hal_error = hal_error;
    g_can_diag.last_esr = esr;

    if ((esr & CAN_ESR_EWGF) != 0U) {
        g_can_diag.error_warning_count++;
    }

    if ((esr & CAN_ESR_EPVF) != 0U) {
        g_can_diag.error_passive_count++;
    }

    if ((esr & CAN_ESR_BOFF) != 0U) {
        g_can_diag.bus_off_count++;
    }
}

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
    memset(&tx, 0, sizeof(tx));

    tx.header.ExtId = CAN_BUILD_ID(CAN_PRIORITY_NORMAL,
                                   CAN_MSG_TYPE_NACK,
                                   CAN_ADDR_CONDUCTOR,
                                   AppConfig_GetPerformerID());

    tx.header.IDE = CAN_ID_EXT;
    tx.header.RTR = CAN_RTR_DATA;
    tx.header.DLC = 8;

    tx.data[0] = (uint8_t)(cmd_code & 0xFF);
    tx.data[1] = (uint8_t)((cmd_code >> 8) & 0xFF);
    tx.data[2] = (uint8_t)(error_code & 0xFF);
    tx.data[3] = (uint8_t)((error_code >> 8) & 0xFF);

    // Ставим фрейм в TX-очередь через общий helper.
    // Helper сам решает, надо ли будить CAN-задачу.
    CAN_QueueTxFrame(&tx);
}

void CAN_SendDone(uint16_t cmd_code, uint8_t device_id)
{
    CanTxFrame_t tx;
    memset(&tx, 0, sizeof(tx));

    tx.header.ExtId = CAN_BUILD_ID(CAN_PRIORITY_NORMAL,
                                   CAN_MSG_TYPE_DATA_DONE_LOG,
                                   CAN_ADDR_CONDUCTOR,
                                   AppConfig_GetPerformerID());

    tx.header.IDE = CAN_ID_EXT;
    tx.header.RTR = CAN_RTR_DATA;
    tx.header.DLC = 8;

    tx.data[0] = CAN_SUB_TYPE_DONE;
    tx.data[1] = (uint8_t)(cmd_code & 0xFF);
    tx.data[2] = (uint8_t)((cmd_code >> 8) & 0xFF);
    tx.data[3] = device_id;

    // Ставим фрейм в TX-очередь через общий helper.
    // Helper сам решает, надо ли будить CAN-задачу.
    CAN_QueueTxFrame(&tx);
}

void CAN_SendAck(uint16_t cmd_code)
{
    CanTxFrame_t tx;
    memset(&tx, 0, sizeof(tx));

    tx.header.ExtId = CAN_BUILD_ID(CAN_PRIORITY_NORMAL,
                                   CAN_MSG_TYPE_ACK,
                                   CAN_ADDR_CONDUCTOR,
                                   AppConfig_GetPerformerID());

    tx.header.IDE = CAN_ID_EXT;
    tx.header.RTR = CAN_RTR_DATA;
    tx.header.DLC = 8;

    tx.data[0] = (uint8_t)(cmd_code & 0xFF);
    tx.data[1] = (uint8_t)((cmd_code >> 8) & 0xFF);

    // Ставим фрейм в TX-очередь через общий helper.
    // Helper сам решает, надо ли будить CAN-задачу.
    CAN_QueueTxFrame(&tx);
}

void CAN_SendNackPublic(uint16_t cmd_code, uint16_t error_code)
{
    CAN_SendNack(cmd_code, error_code);
}

void CAN_SendData(uint16_t cmd_code, uint8_t *data, uint8_t len)
{
    (void)cmd_code;

    CanTxFrame_t tx;
    memset(&tx, 0, sizeof(tx));

    tx.header.ExtId = CAN_BUILD_ID(CAN_PRIORITY_NORMAL,
                                   CAN_MSG_TYPE_DATA_DONE_LOG,
                                   CAN_ADDR_CONDUCTOR,
                                   AppConfig_GetPerformerID());

    tx.header.IDE = CAN_ID_EXT;
    tx.header.RTR = CAN_RTR_DATA;
    tx.header.DLC = 8;

    tx.data[0] = CAN_SUB_TYPE_DATA;
    tx.data[1] = 0x80; // EOT=1, Seq=0

    for (uint8_t i = 0; i < len && i < 6; i++) {
        tx.data[2 + i] = data[i];
    }

    // Ставим фрейм в TX-очередь через общий helper.
    // Helper сам решает, надо ли будить CAN-задачу.
    CAN_QueueTxFrame(&tx);
}

// ============================================================
// Основная задача
// ============================================================

void app_start_task_can_handler(void *argument)
{
    CanRxFrame_t rx_frame;
    CanTxFrame_t tx_frame;
    uint32_t txMailbox;

    // --- Настройка CAN-фильтра (Advanced: PerformerID + Broadcast) ---
    CAN_FilterTypeDef sFilterConfig;
    // ВНИМАНИЕ: Чтобы принимать и PerformerID, и Broadcast (0x00),
    // аппаратная маска для поля DstAddr [23:16] должна быть 0x00,
    // либо нужно использовать два банка фильтров.
    // Для надежности используем программную фильтрацию DstAddr внутри задачи.

    uint32_t filter_id   = 0x00000000 | CAN_ID_EXT; // Принимаем всё (фильтрация программно)
    uint32_t filter_mask = 0x00000000 | CAN_ID_EXT;

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

    // --- Активация прерываний RX FIFO0 и диагностических CAN fault events ---
    if (HAL_CAN_ActivateNotification(&hcan,
                                     CAN_IT_RX_FIFO0_MSG_PENDING |
                                     CAN_IT_RX_FIFO0_FULL |
                                     CAN_IT_RX_FIFO0_OVERRUN |
                                     CAN_IT_ERROR_WARNING |
                                     CAN_IT_ERROR_PASSIVE |
                                     CAN_IT_BUSOFF |
                                     CAN_IT_LAST_ERROR_CODE |
                                     CAN_IT_ERROR) != HAL_OK) {
        Error_Handler();
    }

    // --- Основной цикл (event-driven) ---
    for (;;) {
        // Ожидаем любой из флагов: RX или TX
        AppWatchdog_Heartbeat(APP_WDG_CLIENT_CAN);
        uint32_t flags = osThreadFlagsWait(FLAG_CAN_RX | FLAG_CAN_TX,
                                           osFlagsWaitAny,
                                           APP_WATCHDOG_TASK_IDLE_TIMEOUT_MS);
        AppWatchdog_Heartbeat(APP_WDG_CLIENT_CAN);

        if ((flags & osFlagsError) != 0U) {
            continue;
        }

        // === Обработка входящих сообщений (RX) ===
        if (flags & FLAG_CAN_RX) {
            while (osMessageQueueGet(can_rx_queueHandle, &rx_frame, NULL, 0) == osOK) {
                // Проверяем Extended ID
                if (rx_frame.header.IDE != CAN_ID_EXT) {
                    g_can_diag.dropped_not_ext++;
                    continue;
                }

                // Извлекаем поля из 29-bit CAN ID
                uint32_t can_id = rx_frame.header.ExtId;
                uint8_t msg_type = CAN_GET_MSG_TYPE(can_id);
                uint8_t dst_addr = CAN_GET_DST_ADDR(can_id);

                // --- Программная фильтрация адреса (PerformerID или Broadcast) ---
                if (dst_addr != AppConfig_GetPerformerID() && dst_addr != CAN_ADDR_BROADCAST) {
                    g_can_diag.dropped_wrong_dst++;
                    continue;
                }

                // Обрабатываем только команды
                if (msg_type != CAN_MSG_TYPE_COMMAND) {
                    g_can_diag.dropped_wrong_type++;
                    continue;
                }

                // Директива 2.0: Строгий DLC=8 для всех команд типа COMMAND
                if (rx_frame.header.DLC != 8) {
                    g_can_diag.dropped_wrong_dlc++;
                    continue;
                }

                // Упаковываем в ParsedCanCommand_t
                ParsedCanCommand_t parsed;
                parsed.cmd_code = (uint16_t)(rx_frame.data[0] |
                                             ((uint16_t)rx_frame.data[1] << 8));
                parsed.device_id = rx_frame.data[2]; // Это теперь 0-based индекс
                parsed.data_len = 5; // Всегда 5 байт (data[3..7])

                for (uint8_t i = 0; i < 5; i++) {
                    parsed.data[i] = rx_frame.data[3 + i];
                }

                // Отправляем в dispatcher_queue. Если очередь заполнена,
                // ACK/NACK отправить нельзя: политика ответов принадлежит Dispatcher.
                if (osMessageQueuePut(dispatcher_queueHandle, &parsed, 0, 0) == osOK) {
                    g_can_diag.rx_total++;
                } else {
                    g_can_diag.dispatcher_queue_overflow++;
                }
            }
        }

        // === Обработка исходящих сообщений (TX) ===
        if (flags & FLAG_CAN_TX) {
            while (osMessageQueueGet(can_tx_queueHandle, &tx_frame, NULL, 0) == osOK) {
                // --- Защита от переполнения Mailbox (Advanced) ---
                uint32_t start_tick = osKernelGetTickCount();
                while (HAL_CAN_GetTxMailboxesFreeLevel(&hcan) == 0) {
                    if ((osKernelGetTickCount() - start_tick) > 10) { // Таймаут 10мс
                        break;
                    }
                    osDelay(1);
                }

                if (HAL_CAN_GetTxMailboxesFreeLevel(&hcan) > 0) {
                    if (HAL_CAN_AddTxMessage(&hcan, &tx_frame.header, tx_frame.data, &txMailbox) == HAL_OK) {
                        g_can_diag.tx_total++;
                    } else {
                        g_can_diag.tx_hal_error++;
                        g_can_diag.last_hal_error = HAL_CAN_GetError(&hcan);
                        g_can_diag.last_esr = hcan.Instance->ESR;
                    }
                } else {
                    g_can_diag.tx_mailbox_timeout++;
                }
            }
        }
    }
}
