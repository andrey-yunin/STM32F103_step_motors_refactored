/*
 * app_config.h
 *
 *  Created on: Dec 9, 2025
 *      Author: andrey
 */

#ifndef APP_CONFIG_H_
#define APP_CONFIG_H_

#include <main.h>

// =============================================================================
//                             ОБЩИЕ НАСТРОЙКИ ПРИЛОЖЕНИЯ
// =============================================================================

#define MOTOR_COUNT                 8 // Общее количество моторов в системе
#define CAN_DATA_MAX_LEN            8 // Максимальная длина поля данных CAN-фрейма

// =============================================================================
//                             НАСТРОЙКИ ОЧЕРЕДЕЙ FREERTOS
// =============================================================================

#define CAN_RX_QUEUE_LEN            10
#define CAN_TX_QUEUE_LEN            10
#define DISPATCHER_QUEUE_LEN        10
#define MOTION_QUEUE_LEN            5
#define TMC_MANAGER_QUEUE_LEN       5

// Флаги для Task_CAN_Handler (osThreadFlags)
#define FLAG_CAN_RX                 0x01
#define FLAG_CAN_TX                 0x02

// =============================================================================
//                             СТРУКТУРЫ ДАННЫХ
// =============================================================================

/**
 * @brief Промежуточная структура: CAN Handler -> Dispatcher
 * Содержит распакованные поля CAN-фрейма по стандарту DDS-240
 */
typedef struct {
    uint16_t cmd_code;      // Код команды (байты 0-1 payload, Little-Endian)
    uint8_t  device_id;     // ID целевого устройства/канала (байт 2 payload)
    uint8_t  data[5];       // Параметры команды (байты 3-7 payload)
    uint8_t  data_len;      // Длина полезных данных в массиве data
} ParsedCanCommand_t;

/**
 * @brief Структура задания для Task_Motion_Controller
 */
typedef struct {
    uint8_t  motor_id;      // Физический индекс мотора (0..7)
    uint8_t  command_id;    // Внутренний код (CMD_MOVE_RELATIVE, CMD_STOP...)
    uint16_t cmd_code;      // Оригинальный CAN-код (для ответов ACK/DONE)
    uint8_t  device_id;     // Логический ID дирижера (для ответов DONE)
    uint8_t  direction;     // Направление (1 - CW, 0 - CCW)
    uint32_t steps;         // Кол-во шагов
    uint32_t speed_steps_per_sec;
    uint32_t acceleration_steps_per_sec2;
} MotionCommand_t;

/**
 * @brief Обертки для HAL CAN фреймов
 */
typedef struct {
    CAN_RxHeaderTypeDef header;
    uint8_t data[CAN_DATA_MAX_LEN];
} CanRxFrame_t;

typedef struct {
    CAN_TxHeaderTypeDef header;
    uint8_t data[CAN_DATA_MAX_LEN];
} CanTxFrame_t;

#endif // APP_CONFIG_H
