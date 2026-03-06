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

#define CAN_DATA_MAX_LEN            8 // Максимальная длина поля данных CAN-фрейма (в байтах)

// =============================================================================
//                             НАСТРОЙКИ ОЧЕРЕДЕЙ FREERTOS
// =============================================================================

// -- ДЛИНА ОЧЕРЕДЕЙ (количество элементов) --

// Очередь для приема сырых CAN-фреймов
#define CAN_RX_QUEUE_LEN            10

// Очередь для отправки CAN-фреймов
#define CAN_TX_QUEUE_LEN            10


// Очередь для передачи команд парсеру
#define PARSER_QUEUE_LEN            10

// Очередь для заданий на движение
#define MOTION_QUEUE_LEN            5

// Очередь для команд TMC-драйверам
#define TMC_MANAGER_QUEUE_LEN       5

// Флаги для Task_CAN_Handler (osThreadFlags)
#define FLAG_CAN_RX  0x01  // Новый фрейм в can_rx_queue
#define FLAG_CAN_TX  0x02  // Новый фрейм в can_tx_queue для отправки



// -- РАЗМЕР ЭЛЕМЕНТОВ ОЧЕРЕДЕЙ --
// Мы будем определять размер через sizeof(struct) при создании очереди,
// чтобы не дублировать информацию и избежать ошибок.

// Cтруктура задания для Task_Motion_Controller
typedef struct {
	uint8_t motor_id;
	uint8_t command_id;   // added 05/03/2026. Код команды (CMD_MOVE_ABSOLUTE, CMD_STOP и т.д.)
	uint16_t cmd_code;    // added 05/03/2026 CAN-код команды дирижера (0x0101, 0x0102...) для ACK/NACK/DONE
	uint8_t device_id;    // added 05/03/2026 Логический ID устройства (для DONE-ответа дирижеру)
    uint8_t direction;
    uint32_t steps;
    uint32_t speed_steps_per_sec;
    uint32_t acceleration_steps_per_sec2;
    } MotionCommand_t;

// Структура для хранения полного Rx CAN-фрейма (header + data)
typedef struct {
	CAN_RxHeaderTypeDef header;
    uint8_t data[CAN_DATA_MAX_LEN];
    } CanRxFrame_t;

    // Структура для хранения полного Tx CAN-фрейма (header + data)
typedef struct {
	CAN_TxHeaderTypeDef header;
	uint8_t data[CAN_DATA_MAX_LEN];
    } CanTxFrame_t;


// added 05/03/2026 Промежуточная структура: CAN Handler → Command Parser
// Содержит распакованные поля CAN-фрейма без прикладного парсинга параметров
typedef struct {
	uint16_t cmd_code;      // CAN-код команды (байты 0-1 payload, LE)
	uint8_t  device_id;     // Логический ID устройства (байт 2 payload)
    uint8_t  data[5];       // Сырые данные параметров (байты 3-7 payload)
    uint8_t  data_len;      // Количество байт в data (DLC - 3)
    } ParsedCanCommand_t;



#endif // APP_CONFIG_H




