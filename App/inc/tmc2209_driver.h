/*
 * tmc2209_driver.h
 *
 *  Created on: Dec 8, 2025
 *      Author: andrey
 */

#ifndef TMC2209_DRIVER_H_
#define TMC2209_DRIVER_H_

#include <stdint.h>
#include "main.h" // Для HAL_UART_HandleTypeDef

// Определения для протокола UART (TMC UART Register Access)
// Эти значения взяты из даташита TMC2209
#define TMC2209_UART_SYNC_BYTE      0x05
#define TMC2209_UART_SLAVE_ADDRESS  0x00 // Адрес по умолчанию для записи/чтения
#define TMC2209_UART_READ           0x00
#define TMC2209_UART_WRITE          0x80

// Регистры TMC2209, которые мы будем использовать (из даташита)
// Это только примеры, список будет пополняться
typedef enum {
    TMC2209_GCONF       = 0x00, // Global Configuration
    TMC2209_GSTAT       = 0x01, // Global Status
    TMC2209_IFCNT       = 0x02, // Счётчик принятых UART-записей
    TMC2209_NODECONF    = 0x03, // SENDDELAY
    TMC2209_IOIN        = 0x06, // Input/Output pins (0x04 — OTP_PROG!)
    TMC2209_PWMCONF     = 0x70, // PWM Configuration
    TMC2209_CHOPCONF    = 0x6C, // Chopper Configuration
    TMC2209_DRVSTATUS   = 0x6F, // Driver Status
    TMC2209_VACTUAL     = 0x22, // Actual motor velocity
    TMC2209_MSCNT       = 0x6A, // Microstep counter
    TMC2209_MSCURACT    = 0x6B, // Microstep current actual
    TMC2209_SGTHRS      = 0x40, // StallGuard threshold
    TMC2209_COOLCONF    = 0x42, // CoolStep configuration
    TMC2209_TPOWERDOWN  = 0x11, // Delay before power down
    TMC2209_IHOLD_IRUN  = 0x10, // Current settings
} TMC2209_Register_t;

/* Last completed exchange per handle. words: sequence, addr/reg/write,
 * arm/tx/result/reply-offset (FF = unavailable), tx/rx lengths, elapsed ms,
 * HAL errors, TX[8], RX[12], reserved. Byte arrays packed little endian. */
typedef struct { uint32_t words[12]; } TMC2209_Trace_t;
typedef struct {
    UART_HandleTypeDef *huart;
    uint8_t slave_addr;
    TMC2209_Trace_t trace;
} TMC2209_Handle_t;
void TMC2209_GetTrace(const TMC2209_Handle_t *htmc, TMC2209_Trace_t *out);

// Прототипы функций драйвера
HAL_StatusTypeDef TMC2209_Init(TMC2209_Handle_t* htmc, UART_HandleTypeDef* huart, uint8_t slave_addr);
HAL_StatusTypeDef TMC2209_WriteRegister(TMC2209_Handle_t* htmc, TMC2209_Register_t reg, uint32_t value);
HAL_StatusTypeDef TMC2209_ReadRegister(TMC2209_Handle_t* htmc, TMC2209_Register_t reg, uint32_t* value);
// Единственный владелец UART — TMC manager. IRQ UART должен быть включён.
HAL_StatusTypeDef TMC2209_WriteVerified(TMC2209_Handle_t* htmc, TMC2209_Register_t reg, uint32_t value);

// Дополнительные функции (будут реализованы позже)
HAL_StatusTypeDef TMC2209_SetMotorCurrent(TMC2209_Handle_t* htmc, uint8_t run_current_percent, uint8_t hold_current_percent);
HAL_StatusTypeDef TMC2209_SetMicrosteps(TMC2209_Handle_t* htmc, uint16_t microsteps);
HAL_StatusTypeDef TMC2209_SetSpreadCycle(TMC2209_Handle_t* htmc, uint8_t enable);

#endif /* TMC2209_DRIVER_H_ */
