#include "tmc2209_driver.h"
#include <stddef.h>
#include <string.h>
#include "FreeRTOS.h"
#include "task.h"

#define TMC_UART_TIMEOUT_MS 20U

void TMC2209_GetTrace(const TMC2209_Handle_t *htmc, TMC2209_Trace_t *out)
{
    taskENTER_CRITICAL();
    *out = htmc->trace;
    taskEXIT_CRITICAL();
}

static void trace_finish(TMC2209_Handle_t *htmc, const uint8_t *tx, uint8_t tx_len,
                         const uint8_t *rx, uint8_t arm, uint8_t sent,
                         uint8_t result, uint8_t offset, uint32_t started)
{
    UART_HandleTypeDef *uart = htmc->huart;
    TMC2209_Trace_t trace = {0};
    trace.words[0] = htmc->trace.words[0] + 1U;
    trace.words[1] = tx[1] | ((uint32_t)(tx[2] & 0x7fU) << 8) |
                     ((uint32_t)(tx_len == 8U) << 16);
    trace.words[2] = arm | ((uint32_t)sent << 8) | ((uint32_t)result << 16) |
                     ((uint32_t)offset << 24);
    trace.words[4] = HAL_GetTick() - started;
    /* Freeze IRQ before counting/copying; AbortReceive resets RxXferCount. */
    taskENTER_CRITICAL();
    uint8_t received = rx != NULL && arm == HAL_OK ? (uint8_t)(12U - uart->RxXferCount) : 0U;
    trace.words[5] = uart->ErrorCode;
    if (rx != NULL) (void)HAL_UART_AbortReceive(uart);
    trace.words[3] = tx_len | ((uint32_t)received << 8);
    for (uint8_t i = 0; i < tx_len; ++i) trace.words[6U + i / 4U] |= (uint32_t)tx[i] << (8U * (i % 4U));
    for (uint8_t i = 0; i < received; ++i) trace.words[8U + i / 4U] |= (uint32_t)rx[i] << (8U * (i % 4U));
    htmc->trace = trace;
    taskEXIT_CRITICAL();
}

/* TMC2209: биты каждого байта поступают в CRC от младшего к старшему. */
static uint8_t tmc_crc8(const uint8_t *data, size_t length)
{
    uint8_t crc = 0;
    for (size_t i = 0; i < length; ++i) {
        uint8_t byte = data[i];
        for (uint8_t bit = 0; bit < 8U; ++bit) {
            crc = (uint8_t)((crc << 1) ^ (((crc >> 7) ^ (byte & 1U)) ? 0x07U : 0U));
            byte >>= 1;
        }
    }
    return crc;
}

static int valid_handle(const TMC2209_Handle_t *htmc)
{
    return htmc != NULL && htmc->huart != NULL && htmc->slave_addr <= 3U;
}

HAL_StatusTypeDef TMC2209_Init(TMC2209_Handle_t *htmc, UART_HandleTypeDef *huart, uint8_t address)
{
    if (htmc == NULL || huart == NULL || address > 3U) return HAL_ERROR;
    htmc->huart = huart;
    htmc->slave_addr = address;
    taskENTER_CRITICAL();
    memset(&htmc->trace, 0, sizeof(htmc->trace));
    taskEXIT_CRITICAL();
    return HAL_OK;
}

HAL_StatusTypeDef TMC2209_WriteRegister(TMC2209_Handle_t *htmc, TMC2209_Register_t reg, uint32_t value)
{
    if (!valid_handle(htmc)) return HAL_ERROR;
    uint8_t tx[8] = {0x05, htmc->slave_addr, (uint8_t)(0x80U | reg),
                    (uint8_t)(value >> 24), (uint8_t)(value >> 16),
                    (uint8_t)(value >> 8), (uint8_t)value, 0};
    tx[7] = tmc_crc8(tx, 7);
    // HAL_OK означает передачу MCU; принятие TMC проверяет WriteVerified.
    uint32_t started = HAL_GetTick();
    HAL_StatusTypeDef status = HAL_UART_Transmit(htmc->huart, tx, sizeof(tx), TMC_UART_TIMEOUT_MS);
    trace_finish(htmc, tx, sizeof(tx), NULL, 0xff, status, status, 0xff, started);
    return status;
}

HAL_StatusTypeDef TMC2209_ReadRegister(TMC2209_Handle_t *htmc, TMC2209_Register_t reg, uint32_t *value)
{
    if (!valid_handle(htmc) || value == NULL) return HAL_ERROR;
    UART_HandleTypeDef *uart = htmc->huart;
    uint8_t tx[4] = {0x05, htmc->slave_addr, (uint8_t)reg, 0};
    // 4 байта возможного эха + 8 байт ответа. Приём работает уже во время TX.
    uint8_t rx[12] = {0};
    tx[3] = tmc_crc8(tx, 3);
    uint32_t trace_started = HAL_GetTick();
    (void)HAL_UART_AbortReceive(uart);
    __HAL_UART_CLEAR_OREFLAG(uart); // SR/DR: удалить старое эхо и ошибки приёма.
    HAL_StatusTypeDef status = HAL_UART_Receive_IT(uart, rx, sizeof(rx));
    if (status != HAL_OK) {
        trace_finish(htmc, tx, sizeof(tx), rx, status, 0xff, status, 0xff, trace_started);
        return status;
    }
    status = HAL_UART_Transmit(uart, tx, sizeof(tx), TMC_UART_TIMEOUT_MS);
    if (status != HAL_OK) {
        trace_finish(htmc, tx, sizeof(tx), rx, HAL_OK, status, status, 0xff, trace_started);
        return status;
    }
    uint32_t started = HAL_GetTick();
    do {
        if (uart->ErrorCode != HAL_UART_ERROR_NONE) {
            trace_finish(htmc, tx, sizeof(tx), rx, HAL_OK, HAL_OK, HAL_ERROR, 0xff, trace_started);
            return HAL_ERROR;
        }
        uint16_t received = (uint16_t)(sizeof(rx) - uart->RxXferCount);
        __DMB(); // IRQ сначала записывает байт, затем уменьшает RxXferCount.
        for (uint16_t offset = 0; offset + 8U <= received; ++offset) {
            const uint8_t *reply = &rx[offset];
            if (reply[0] == 0x05U && reply[1] == 0xFFU && reply[2] == (uint8_t)reg &&
                tmc_crc8(reply, 7) == reply[7]) {
                uint32_t result = ((uint32_t)reply[3] << 24) | ((uint32_t)reply[4] << 16) |
                                  ((uint32_t)reply[5] << 8) | reply[6];
                trace_finish(htmc, tx, sizeof(tx), rx, HAL_OK, HAL_OK, HAL_OK, (uint8_t)offset, trace_started);
                *value = result;
                return HAL_OK;
            }
        }
        if (received == sizeof(rx)) break;
    } while ((uint32_t)(HAL_GetTick() - started) < TMC_UART_TIMEOUT_MS);
    trace_finish(htmc, tx, sizeof(tx), rx, HAL_OK, HAL_OK, HAL_TIMEOUT, 0xff, trace_started);
    return HAL_TIMEOUT;
}

HAL_StatusTypeDef TMC2209_WriteVerified(TMC2209_Handle_t *htmc, TMC2209_Register_t reg, uint32_t value)
{
    uint32_t before, after;
    HAL_StatusTypeDef status = TMC2209_ReadRegister(htmc, TMC2209_IFCNT, &before);
    if (status != HAL_OK) return status;
    status = TMC2209_WriteRegister(htmc, reg, value);
    if (status != HAL_OK) return status;
    // Пауза после записи: завершение обработки датаграммы и эха перед чтением.
    HAL_Delay(1);
    status = TMC2209_ReadRegister(htmc, TMC2209_IFCNT, &after);
    if (status != HAL_OK) return status;
    return (uint8_t)(after - before) == 1U ? HAL_OK : HAL_ERROR;
}

HAL_StatusTypeDef TMC2209_SetMotorCurrent(TMC2209_Handle_t *htmc, uint8_t run, uint8_t hold)
{
    if (!valid_handle(htmc) || run == 0U || run > 100U || hold > 100U) return HAL_ERROR;
    // Это доля шкалы, не амперы: физический ток зависит от VREF/Rsense/VSENSE.
    uint32_t irun = (uint32_t)run * 31U / 100U;
    uint32_t ihold = (uint32_t)hold * 31U / 100U;
    // IHOLD[4:0], IRUN[12:8], IHOLDDELAY[19:16]. TPOWERDOWN — отдельный регистр.
    return TMC2209_WriteVerified(htmc, TMC2209_IHOLD_IRUN, ihold | (irun << 8) | (4UL << 16));
}

HAL_StatusTypeDef TMC2209_SetMicrosteps(TMC2209_Handle_t *htmc, uint16_t microsteps)
{
    if (!valid_handle(htmc) || microsteps == 0U || microsteps > 256U ||
        (microsteps & (microsteps - 1U)) != 0U) return HAL_ERROR;
    uint32_t mres = 0U;
    for (uint16_t n = 256U; n > microsteps; n >>= 1) ++mres;
    uint32_t chopconf;
    HAL_StatusTypeDef status = TMC2209_ReadRegister(htmc, TMC2209_CHOPCONF, &chopconf);
    if (status != HAL_OK) return status; // Никогда не подменять ошибку чтения нулём.
    chopconf = (chopconf & ~(0xFUL << 24)) | (mres << 24);
    status = TMC2209_WriteVerified(htmc, TMC2209_CHOPCONF, chopconf);
    if (status != HAL_OK) return status;
    uint32_t actual;
    status = TMC2209_ReadRegister(htmc, TMC2209_CHOPCONF, &actual);
    return status == HAL_OK && actual == chopconf ? HAL_OK : HAL_ERROR;
}

HAL_StatusTypeDef TMC2209_SetSpreadCycle(TMC2209_Handle_t *htmc, uint8_t enable)
{
    if (!valid_handle(htmc) || enable > 1U) return HAL_ERROR;
    uint32_t gconf;
    HAL_StatusTypeDef status = TMC2209_ReadRegister(htmc, TMC2209_GCONF, &gconf);
    if (status != HAL_OK) return status;
    gconf = (gconf & ~(1UL << 2)) | ((uint32_t)enable << 2);
    return TMC2209_WriteVerified(htmc, TMC2209_GCONF, gconf);
}
