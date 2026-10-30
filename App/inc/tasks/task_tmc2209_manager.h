#ifndef TASK_TMC2209_MANAGER_H_
#define TASK_TMC2209_MANAGER_H_
#include <stdbool.h>
#include <stdint.h>
#include "tmc2209_driver.h"

// Third physical output; UART address is selected by MS1/MS2, independently.
#define TMC_BENCH_MOTOR_ID 2U
#define TMC_BENCH_UART2_MOTOR_ID 6U
#define TMC_UART_COUNT 2U
#define TMC_BENCH_UART_ADDRESS 0U

/* Видно в Live Expressions; стадии соответствуют CHECK_STAGE в task_tmc2209_manager.c. */
typedef struct {
    uint32_t state;
    uint32_t valid_mask;
    uint32_t stage;
    uint32_t hal_status;
    uint32_t ioin;
    uint32_t gconf;
    uint32_t chopconf;
    uint32_t ifcnt;
    uint32_t gstat;
    uint32_t drv_status;
    uint32_t ready;
} TMC_Diagnostics_t;
extern volatile TMC_Diagnostics_t g_tmc_diag[TMC_UART_COUNT];
// Копия в task context; чтение не вызывает UART и не меняет состояние мотора.
bool TMCManager_GetDiagnostics(uint8_t motor_id, TMC_Diagnostics_t *out);
bool TMCManager_GetTrace(uint8_t motor_id, TMC2209_Trace_t *out);
bool TMCManager_IsReady(uint8_t motor_id);
void app_start_task_tmc2209_manager(void *argument);
#endif
