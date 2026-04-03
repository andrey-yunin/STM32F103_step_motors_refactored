/*
 * app_flash.c
 *
 *  Created on: Apr 3, 2026
 *      Author: andrey
 */

#include "app_flash.h"
#include "main.h"
#include "cmsis_os.h"
#include <string.h>

// --- Инкапсулированные данные (скрыты внутри модуля) ---
static AppConfig_t g_app_config;
static osMutexId_t configMutex = NULL;

// Атрибуты мьютекса (стандарт FreeRTOS CMSIS_V2)
const osMutexAttr_t configMutex_attr = {
		"configMutex",                          // name
		osMutexRecursive | osMutexPrioInherit,  // attr_bits
		NULL,                                   // cb_mem
		0U                                      // cb_size
};

/**
 * @brief Чтение 96-битного уникального идентификатора чипа (MCU UID).
 */
void AppConfig_GetMCU_UID(uint8_t* out_uid) {
	if (out_uid == NULL) return;
    uint8_t* uid_base = (uint8_t*)0x1FFFF7E8;
    memcpy(out_uid, uid_base, 12);
}


/**
 * @brief Расчет контрольной суммы (CRC16 - ваша доработка для моторов).
 */
static uint16_t CalculateCRC16(AppConfig_t* cfg) {
	uint16_t crc = 0xFFFF;
	uint8_t* p = (uint8_t*)cfg;
    uint16_t len = sizeof(AppConfig_t) - sizeof(cfg->checksum);

    for (uint16_t i = 0; i < len; i++) {
    	crc ^= p[i];
    	for (uint8_t j = 0; j < 8; j++) {
    		if (crc & 1) crc = (crc >> 1) ^ 0xA001;
    		else crc >>= 1;
    		}
    	}
    return crc;
}

void AppConfig_Init(void) {
	// 1. Создание мьютекса для защиты доступа
	if (configMutex == NULL) {
		configMutex = osMutexNew(&configMutex_attr);
		}
	// 2. Чтение данных из Flash
	AppConfig_t* flash_cfg = (AppConfig_t*)APP_CONFIG_FLASH_ADDR;

    // 3. Валидация (Magic + CRC16)
    if (flash_cfg->magic == APP_CONFIG_MAGIC &&
    		flash_cfg->checksum == CalculateCRC16(flash_cfg))
    	{
    	memcpy(&g_app_config, flash_cfg, sizeof(AppConfig_t));
    	}
    else
    	{
    	// "Заводские настройки"
    	memset(&g_app_config, 0, sizeof(AppConfig_t));
    	g_app_config.magic = APP_CONFIG_MAGIC;
    	g_app_config.performer_id = 0x20; // Default NodeID для моторов

    	// Дефолтный маппинг: 140, 141, 142... 147
    	for (uint8_t i = 0; i < 8; i++) {
    		g_app_config.motor_map[i] = 140 + i;
    		}
    	g_app_config.checksum = CalculateCRC16(&g_app_config);
    	AppConfig_Commit(); // Сохраняем дефолт
    	}
    }

uint8_t AppConfig_GetMotorLogicalID(uint8_t physical_idx) {
	uint8_t id = 0xFF;
	if (physical_idx < 8 && osMutexAcquire(configMutex, osWaitForever) == osOK) {
		id = g_app_config.motor_map[physical_idx];
		osMutexRelease(configMutex);
		}
	return id;
}

void AppConfig_SetMotorLogicalID(uint8_t physical_idx, uint8_t logical_id) {
	if (physical_idx < 8 && osMutexAcquire(configMutex, osWaitForever) == osOK) {
		g_app_config.motor_map[physical_idx] = logical_id;
		osMutexRelease(configMutex);
		}
	}

uint32_t AppConfig_GetPerformerID(void) {
	uint32_t id = 0x20;
	if (osMutexAcquire(configMutex, osWaitForever) == osOK) {
		id = g_app_config.performer_id;
		osMutexRelease(configMutex);
		}
	return id;
}

void AppConfig_SetPerformerID(uint32_t id) {
	if (osMutexAcquire(configMutex, osWaitForever) == osOK) {
		g_app_config.performer_id = id;
		osMutexRelease(configMutex);
		}
}

bool AppConfig_Commit(void) {
	bool success = false;
	HAL_StatusTypeDef status = HAL_ERROR;
	uint32_t PageError = 0;
	FLASH_EraseInitTypeDef EraseInitStruct;

	if (osMutexAcquire(configMutex, osWaitForever) == osOK) {
		// 1. Обновляем контрольную сумму
		g_app_config.checksum = CalculateCRC16(&g_app_config);

		// 2. Разблокировка, стирание, запись (атомарно)

		HAL_FLASH_Unlock();

		EraseInitStruct.TypeErase   = FLASH_TYPEERASE_PAGES;
		EraseInitStruct.PageAddress = APP_CONFIG_FLASH_ADDR;
		EraseInitStruct.NbPages     = 1;

		status = HAL_FLASHEx_Erase(&EraseInitStruct, &PageError);

		if (status == HAL_OK) {
			uint32_t* pData = (uint32_t*)&g_app_config;
			uint32_t addr = APP_CONFIG_FLASH_ADDR;
			// Пишем по 4 байта
			for (uint32_t i = 0; i < sizeof(AppConfig_t); i += 4) {
				status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr, *pData);
				if (status != HAL_OK) break;
				addr += 4; pData++;
				}
			}

		HAL_FLASH_Lock();

		success = (status == HAL_OK);
		osMutexRelease(configMutex);
		}
	return success;
	}

