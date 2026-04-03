/*
 * app_flash.h
 *
 *  Created on: Apr 3, 2026
 *      Author: andrey
 */

#ifndef APP_FLASH_H_
#define APP_FLASH_H_


#include <stdint.h>
#include <stdbool.h>
#include "app_config.h"

// Адрес последней страницы Flash (Page 63 для 64KB STM32F103)
#define APP_CONFIG_FLASH_ADDR    0x0800FC00
#define APP_CONFIG_MAGIC         0x55AAEEFF // Ключ валидности данных

/**
 * @brief Структура конфигурации устройства, хранимая во Flash.
 * Размер кратен 4 байтам для выравнивания во Flash.
 */
 typedef struct {
	 uint32_t magic;             // Метка инициализации памяти
     uint32_t performer_id;      // Настраиваемый CAN NodeID платы (0x20)
     uint8_t  motor_map[8];      // Таблица маппинга: [Idx] -> Logical ID
     uint16_t reserved;          // Резерв для выравнивания
     uint16_t checksum;          // Контрольная сумма структуры
} AppConfig_t;


/**
 * @brief Чтение 96-битного уникального идентификатора чипа (MCU UID).
 */
void AppConfig_GetMCU_UID(uint8_t* out_uid);

/**
 * @brief Инициализирует конфигурацию (загрузка из Flash или Default) и создает Mutex.
 */
void AppConfig_Init(void);

/**
 * @brief Безопасное чтение логического ID для физического индекса драйвера.
 */
uint8_t AppConfig_GetMotorLogicalID(uint8_t physical_idx);

/**
 * @brief Безопасная запись логического ID в таблицу маппинга (в RAM).
 */
void AppConfig_SetMotorLogicalID(uint8_t physical_idx, uint8_t logical_id);

/**
 * @brief Чтение текущего CAN ID платы.
 */
uint32_t AppConfig_GetPerformerID(void);

/**
 * @brief Безопасная запись CAN ID платы (в RAM).
 */
void AppConfig_SetPerformerID(uint32_t id);

/**
 * @brief Сохранение всех изменений из RAM во Flash.
 */
bool AppConfig_Commit(void);



#endif /* APP_FLASH_H_ */
