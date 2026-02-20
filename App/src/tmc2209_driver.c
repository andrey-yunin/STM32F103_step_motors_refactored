/*
 * tmc2209_driver.c
 *
 *  Created on: Dec 8, 2025
 *      Author: andrey
 */


#include "tmc2209_driver.h"
#include <string.h> // Для memcpy

 // --- Приватные функции ---

 // CRC8 таблица (полином 0x07)
static const uint8_t tmc_crc_table[256] = {
		0x00, 0x07, 0x0E, 0x09, 0x1C, 0x1B, 0x12, 0x15, 0x38, 0x3F, 0x36, 0x31, 0x24, 0x23, 0x2A, 0x2D,
        0x70, 0x77, 0x7E, 0x79, 0x6C, 0x6B, 0x62, 0x65, 0x48, 0x4F, 0x46, 0x41, 0x54, 0x53, 0x5A, 0x5D,
        0xE0, 0xE7, 0xEE, 0xE9, 0xFC, 0xFB, 0xF2, 0xF5, 0xD8, 0xDF, 0xD6, 0xD1, 0xC4, 0xC3, 0xCA, 0xCD,
        0x90, 0x97, 0x9E, 0x99, 0x8C, 0x8B, 0x82, 0x85, 0xA8, 0xAF, 0xA6, 0xA1, 0xB4, 0xB3, 0xBA, 0xBD,
        0xC7, 0xC0, 0xC9, 0xCE, 0xDB, 0xDC, 0xD5, 0xD2, 0xFF, 0xF8, 0xF1, 0xF6, 0xE3, 0xE4, 0xED, 0xEA,
        0xB7, 0xB0, 0xB9, 0xBE, 0xAB, 0xAC, 0xA5, 0xA2, 0x8F, 0x88, 0x81, 0x86, 0x93, 0x94, 0x9D, 0x9A,
        0x27, 0x20, 0x29, 0x2E, 0x3B, 0x3C, 0x35, 0x32, 0x1F, 0x18, 0x11, 0x16, 0x03, 0x04, 0x0D, 0x0A,
        0x57, 0x50, 0x59, 0x5E, 0x4B, 0x4C, 0x45, 0x42, 0x6F, 0x68, 0x61, 0x66, 0x73, 0x74, 0x7D, 0x7A,
        0x89, 0x8E, 0x87, 0x80, 0x95, 0x92, 0x9B, 0x9C, 0xB1, 0xB6, 0xBF, 0xB8, 0xAD, 0xAA, 0xA3, 0xA4,
        0xF9, 0xFE, 0xF7, 0xF0, 0xE5, 0xE2, 0xEB, 0xEC, 0xC1, 0xC6, 0xCF, 0xC8, 0xDD, 0xDA, 0xD3, 0xD4,
        0x69, 0x6E, 0x67, 0x60, 0x75, 0x72, 0x7B, 0x7C, 0x51, 0x56, 0x5F, 0x58, 0x4D, 0x4A, 0x43, 0x44,
        0x19, 0x1E, 0x17, 0x10, 0x05, 0x02, 0x0B, 0x0C, 0x21, 0x26, 0x2F, 0x28, 0x3D, 0x3A, 0x33, 0x34,
        0x4E, 0x49, 0x40, 0x47, 0x52, 0x55, 0x5C, 0x5B, 0x76, 0x71, 0x78, 0x7F, 0x6A, 0x6D, 0x64, 0x63,
        0x3E, 0x39, 0x30, 0x37, 0x22, 0x25, 0x2C, 0x2B, 0x06, 0x01, 0x08, 0x0F, 0x1A, 0x1D, 0x14, 0x13,
        0xAE, 0xA9, 0xA0, 0xA7, 0xB2, 0xB5, 0xBC, 0xBB, 0x96, 0x91, 0x98, 0x9F, 0x8A, 0x8D, 0x84, 0x83,
        0xDE, 0xD9, 0xD0, 0xD7, 0xC2, 0xC5, 0xCC, 0xCB, 0xE6, 0xE1, 0xE8, 0xEF, 0xFA, 0xFD, 0xF4, 0xF3
     };

static uint8_t tmc_crc8(uint8_t *data, size_t length) {
	uint8_t crc = 0;
    for (size_t i = 0; i < length; i++) {
    	crc = tmc_crc_table[crc ^ data[i]];
        }
    return crc;
    }

// --- Публичные функции ---

/**
 * @brief Инициализирует дескриптор TMC2209.
 * @param htmc Указатель на структуру TMC2209_Handle_t.
 * @param huart Указатель на структуру UART_HandleTypeDef, к которой подключен драйвер.
 * @param slave_addr Адрес драйвера на шине UART (0-3).
 * @retval HAL_StatusTypeDef HAL_OK если успешно, иначе код ошибки.
 */
HAL_StatusTypeDef TMC2209_Init(TMC2209_Handle_t* htmc, UART_HandleTypeDef* huart, uint8_t slave_addr) {
	if (htmc == NULL || huart == NULL || slave_addr > 3) {
    return HAL_ERROR; // Проверка входных параметров
    }
htmc->huart = huart;
htmc->slave_addr = slave_addr;
return HAL_OK;
 }


/**
 * @brief Отправляет команду записи в регистр TMC2209.
 * @param htmc Указатель на структуру TMC2209_Handle_t.
 * @param reg Регистр для записи.
 * @param value Значение для записи.
 * @retval HAL_StatusTypeDef HAL_OK если успешно, иначе код ошибки.
 */
HAL_StatusTypeDef TMC2209_WriteRegister(TMC2209_Handle_t* htmc, TMC2209_Register_t reg, uint32_t value) {
	uint8_t tx_buf[8]; // Sync, SlaveAddr, WriteFlag+RegisterAddr, Value(4 bytes), CRC
    tx_buf[0] = TMC2209_UART_SYNC_BYTE;
    tx_buf[1] = htmc->slave_addr;
    tx_buf[2] = TMC2209_UART_WRITE | reg;
    tx_buf[3] = (uint8_t)(value >> 24);
    tx_buf[4] = (uint8_t)(value >> 16);
    tx_buf[5] = (uint8_t)(value >> 8);
    tx_buf[6] = (uint8_t)(value);
    tx_buf[7] = tmc_crc8(tx_buf, 7);

// Отправка данных по UART. Добавить таймаут
return HAL_UART_Transmit(htmc->huart, tx_buf, sizeof(tx_buf), HAL_MAX_DELAY);
}

/**
 * @brief Отправляет команду чтения регистра TMC2209 и получает ответ.
 * @param htmc Указатель на структуру TMC2209_Handle_t.
 * @param reg Регистр для чтения.
 * @param value Указатель для сохранения прочитанного значения.
 * @retval HAL_StatusTypeDef HAL_OK если успешно, иначе код ошибки.
 */
HAL_StatusTypeDef TMC2209_ReadRegister(TMC2209_Handle_t* htmc, TMC2209_Register_t reg, uint32_t* value) {
uint8_t tx_buf[4]; // Sync, SlaveAddr, ReadFlag+RegisterAddr, CRC
uint8_t rx_buf[8]; // Sync, SlaveAddr, ReadFlag+RegisterAddr, Value(4 bytes), CRC

tx_buf[0] = TMC2209_UART_SYNC_BYTE;
tx_buf[1] = htmc->slave_addr;
tx_buf[2] = TMC2209_UART_READ | reg;
tx_buf[3] = tmc_crc8(tx_buf, 3);

// Очистить буфер приема UART перед отправкой запроса, чтобы не читать старые данные
// HAL_UART_AbortReceive_IT(htmc->huart); // Или аналогичная функция для очистки
// Очистка буфера приема может быть специфичной для HAL или требовать прямого доступа к регистрам

// Отправка запроса на чтение
if (HAL_UART_Transmit(htmc->huart, tx_buf, sizeof(tx_buf), HAL_MAX_DELAY) != HAL_OK) {
	return HAL_ERROR;
     }

// Прием ответа
// Драйвер TMC2209 отправляет ответ через некоторое время после запроса.
// Возможно, потребуется небольшой таймаут между Transmit и Receive, если драйвер не мгновенный.
if (HAL_UART_Receive(htmc->huart, rx_buf, sizeof(rx_buf), 100) != HAL_OK) { // Таймаут 100 мс
   return HAL_ERROR;
     }

// Проверка ответа (SYNC, SlaveAddr, ReadFlag+RegisterAddr, CRC)
if (rx_buf[0] != TMC2209_UART_SYNC_BYTE ||
    rx_buf[1] != htmc->slave_addr ||
    (rx_buf[2] & 0x7F) != reg || // Проверяем, что ответ пришел от нужного регистра
    tmc_crc8(rx_buf, 7) != rx_buf[7]) {
    return HAL_ERROR; // Некорректный ответ или ошибка CRC
     }

// Извлечение значения
*value = ((uint32_t)rx_buf[3] << 24) |
         ((uint32_t)rx_buf[4] << 16) |
         ((uint32_t)rx_buf[5] << 8)  |
         ((uint32_t)rx_buf[6]);

return HAL_OK;
   }

/**
 * @brief Устанавливает ток удержания (hold) и рабочий ток (run) для драйвера TMC2209.
 * @param htmc Указатель на структуру TMC2209_Handle_t.
 * @param run_current_percent Процент от максимального тока для рабочего режима (1-100%).
 * @param hold_current_percent Процент от максимального тока для режима удержания (0-100%).
 * @retval HAL_StatusTypeDef HAL_OK если успешно, иначе код ошибки.
 */

HAL_StatusTypeDef TMC2209_SetMotorCurrent(TMC2209_Handle_t* htmc, uint8_t run_current_percent, uint8_t hold_current_percent) {
	if (htmc == NULL || run_current_percent == 0 || run_current_percent > 100 || hold_current_percent > 100) {
		return HAL_ERROR;
		}

	// Рекомендуемые значения для IHOLD и IRUN
	// IHOLD = 0..31 (ток удержания)
	// IRUN = 0..31 (рабочий ток)
	// TPOWERDOWN = 0..255 (задержка перед снижением тока до IHOLD)
	// Значение 31 соответствует ~99% от максимального тока, 1 - ~32%
	// Ток вычисляется: Current_RMS = (IRUN+1)/32 * (Vsense/R_Sense) / 1.41
	// Мы упрощаем и просто масштабируем 0-31 на 0-100%.

	uint8_t ihold = (uint8_t)((float)hold_current_percent / 100.0f * 31.0f);
	uint8_t irun = (uint8_t)((float)run_current_percent / 100.0f * 31.0f);
	uint8_t tpowerdown = 10; // 1 секунда (10*100мс) - время, через которое ток снижается до IHOLD после остановки

	// Регистр IHOLD_IRUN: TPOWERDOWN:24, IHOLD:16, IRUN:8, TSTRT:0
	uint32_t value = ((uint32_t)tpowerdown << 24) | ((uint32_t)ihold << 16) | ((uint32_t)irun << 8) | (0x4 << 0); // TSTRT=4

	return TMC2209_WriteRegister(htmc, TMC2209_IHOLD_IRUN, value);

}

/**
 * @brief Устанавливает разрешение микрошага для драйвера TMC2209.
 * @param htmc Указатель на структуру TMC2209_Handle_t.
 * @param microsteps Количество микрошагов (1, 2, 4, 8, 16, 32, 64, 128, 256).
 * @retval HAL_StatusTypeDef HAL_OK если успешно, иначе код ошибки.
 */

HAL_StatusTypeDef TMC2209_SetMicrosteps(TMC2209_Handle_t* htmc, uint16_t microsteps) {
	if (htmc == NULL) {
		return HAL_ERROR;
		}
	// MRES - Microstep Resolution
	// 0: 256 microsteps (default)
	// 1: 128 microsteps
	// 2: 64 microsteps
	// 3: 32 microsteps
	// 4: 16 microsteps
	// 5: 8 microsteps
	// 6: 4 microsteps
	// 7: 2 microsteps
	// 8: Fullstep

	uint32_t mres_val = 0; // По умолчанию 256
	switch (microsteps) {
         case 256: mres_val = 0; break;
         case 128: mres_val = 1; break;
	     case 64:  mres_val = 2; break;
	     case 32:  mres_val = 3; break;
	     case 16:  mres_val = 4; break;
	     case 8:   mres_val = 5; break;
	     case 4:   mres_val = 6; break;
	     case 2:   mres_val = 7; break;
	     case 1:   mres_val = 8; break;
	     default: return HAL_ERROR; // Неподдерживаемое значение микрошага
	}


	// Регистр CHOPCONF. Нам нужен только бит MRES (биты 24-27).
	// Сначала читаем текущее значение регистра, чтобы не перезаписать другие настройки
	uint32_t chopconf_reg;
	if (TMC2209_ReadRegister(htmc, TMC2209_CHOPCONF, &chopconf_reg) != HAL_OK) {
		// Если не удалось прочитать, попробуем записать напрямую (может быть, регистр сброшен)
	    chopconf_reg = 0; // Или дефолтное значение
	    }

	    // Очищаем биты MRES и устанавливаем новое значение
	   chopconf_reg &= ~((uint32_t)0xF << 24); // Очистка MRES (биты 24-27)
	   chopconf_reg |= (mres_val << 24);

	   return TMC2209_WriteRegister(htmc, TMC2209_CHOPCONF, chopconf_reg);





}

HAL_StatusTypeDef TMC2209_SetSpreadCycle(TMC2209_Handle_t* htmc, uint8_t enable) {
// Установка/сброс бита TSTEP в регистре GCONF для включения/отключения SpreadCycle (или StealthChop)
(void)htmc;
(void)enable;
return HAL_ERROR; // Пока не реализовано
}
