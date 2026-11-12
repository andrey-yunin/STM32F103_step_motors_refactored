#include "task_tmc2209_manager.h"
#include "main.h"
#include "cmsis_os.h"
#include "tmc2209_driver.h"
#include "task_watchdog.h"
#include "can_protocol.h"
#include "FreeRTOS.h"
#include "task.h"

extern UART_HandleTypeDef huart1, huart2;
volatile TMC_Diagnostics_t g_tmc_diag[TMC_UART_COUNT];
static TMC2209_Handle_t bench_motors[TMC_UART_COUNT];
static UART_HandleTypeDef *const bench_uarts[TMC_UART_COUNT] = {&huart1, &huart2};

static int BusForMotor(uint8_t motor_id)
{
    if (motor_id == TMC_BENCH_MOTOR_ID) return 0;
    if (motor_id == TMC_BENCH_UART2_MOTOR_ID) return 1;
    return -1;
}

bool TMCManager_IsReady(uint8_t motor_id)
{
    int bus = BusForMotor(motor_id);
    return bus >= 0 && g_tmc_diag[bus].ready != 0U;
}

bool TMCManager_GetDiagnostics(uint8_t motor_id, TMC_Diagnostics_t *out)
{
    int bus = BusForMotor(motor_id);
    if (bus < 0 || out == NULL) return false;
    taskENTER_CRITICAL();
    *out = g_tmc_diag[bus];
    taskEXIT_CRITICAL();
    return true;
}

bool TMCManager_GetTrace(uint8_t motor_id, TMC2209_Trace_t *out)
{
    int bus = BusForMotor(motor_id);
    if (bus < 0 || out == NULL) return false;
    TMC2209_GetTrace(&bench_motors[bus], out);
    return true;
}

static HAL_StatusTypeDef ConfigureBenchMotor(unsigned bus)
{
    volatile TMC_Diagnostics_t *diag = &g_tmc_diag[bus];
    TMC2209_Handle_t *motor = &bench_motors[bus];
    uint32_t value, gconf, chopconf;
    HAL_StatusTypeDef status;
#define CHECK_STAGE(number, operation) do { \
    diag->stage = (number); \
    status = (operation); \
    diag->hal_status = status; \
    if (status != HAL_OK) return status; \
} while (0)

    CHECK_STAGE(1, TMC2209_Init(motor, bench_uarts[bus], TMC_BENCH_UART_ADDRESS));
    CHECK_STAGE(2, TMC2209_ReadRegister(motor, TMC2209_IOIN, &value));
    diag->ioin = value;
    diag->valid_mask |= CAN_DIAG_TMC_VALID_IOIN;
    // VERSION=0x21 идентифицирует TMC2209. ENN должен оставаться HIGH.
    if ((value >> 24) != 0x21U || (value & 1U) == 0U) return HAL_ERROR;
    CHECK_STAGE(3, TMC2209_WriteVerified(motor, TMC2209_NODECONF, 2UL << 8));
    CHECK_STAGE(4, TMC2209_ReadRegister(motor, TMC2209_GCONF, &gconf));
    // Сохраняем аппаратный источник тока/VREF и Rsense; включаем UART и MRES.
    gconf = (gconf | (1UL << 6) | (1UL << 7)) & ~(1UL << 9);
    CHECK_STAGE(5, TMC2209_WriteVerified(motor, TMC2209_GCONF, gconf));
    CHECK_STAGE(6, TMC2209_ReadRegister(motor, TMC2209_GCONF, &value));
    diag->gconf = value;
    diag->valid_mask |= CAN_DIAG_TMC_VALID_GCONF;
    if (value != gconf) return HAL_ERROR;
    CHECK_STAGE(7, TMC2209_SetMotorCurrent(motor, 70, 40));
    CHECK_STAGE(8, TMC2209_WriteVerified(motor, TMC2209_TPOWERDOWN, 20));
    // Вращение только от внешнего STEP; внутренний генератор выключен.
    CHECK_STAGE(9, TMC2209_WriteVerified(motor, TMC2209_VACTUAL, 0));
    CHECK_STAGE(10, TMC2209_ReadRegister(motor, TMC2209_CHOPCONF, &chopconf));
    // Один шаг на положительный фронт; защиты не отключаем.
    chopconf &= ~((1UL << 29) | (1UL << 30) | (1UL << 31));
    if ((chopconf & 0xFU) == 0U) chopconf |= 3U;
    CHECK_STAGE(11, TMC2209_WriteVerified(motor, TMC2209_CHOPCONF, chopconf));
    CHECK_STAGE(12, TMC2209_SetMicrosteps(motor, 16));
    CHECK_STAGE(13, TMC2209_ReadRegister(motor, TMC2209_CHOPCONF, &value));
    diag->chopconf = value;
    diag->valid_mask |= CAN_DIAG_TMC_VALID_CHOPCONF;
    if (value != ((chopconf & ~(0xFUL << 24)) | (4UL << 24))) return HAL_ERROR;
    CHECK_STAGE(14, TMC2209_ReadRegister(motor, TMC2209_IFCNT, &value));
    diag->ifcnt = value;
    diag->valid_mask |= CAN_DIAG_TMC_VALID_IFCNT;
    CHECK_STAGE(15, TMC2209_ReadRegister(motor, TMC2209_GSTAT, &value));
    diag->gstat = value;
    diag->valid_mask |= CAN_DIAG_TMC_VALID_GSTAT;
    if ((value & 6U) != 0U) return HAL_ERROR;
    CHECK_STAGE(16, TMC2209_ReadRegister(motor, TMC2209_DRVSTATUS, &value));
    diag->drv_status = value;
    diag->valid_mask |= CAN_DIAG_TMC_VALID_DRV_STATUS;
    // ot, s2ga/b, s2vsa/b. Open-load при отключённом EN не является отказом.
    if ((value & 0x3EU) != 0U) return HAL_ERROR;
    diag->stage = 17;
    __DMB();
    diag->ready = 1;
    return HAL_OK;
#undef CHECK_STAGE
}

void app_start_task_tmc2209_manager(void *argument)
{
    (void)argument;
    osDelay(100);
    // Independent buses: a missing driver on UART1 must not prevent UART2 setup.
    for (unsigned bus = 0; bus < TMC_UART_COUNT; ++bus) {
        g_tmc_diag[bus].state = CAN_DIAG_TMC_STATE_CONFIGURING;
        HAL_StatusTypeDef status = ConfigureBenchMotor(bus);
        taskENTER_CRITICAL();
        g_tmc_diag[bus].hal_status = status;
        g_tmc_diag[bus].state = status == HAL_OK ? CAN_DIAG_TMC_STATE_READY : CAN_DIAG_TMC_STATE_FAILED;
        taskEXIT_CRITICAL();
    }
    // Ошибка не маскируется повторными записями. STOP доступен; новый опыт после reset.
    // TMC пока не критичный клиент watchdog: heartbeat = progress, а не
    // «задача жива». Вернётся в MU-07 вместе с event-driven job executor.
    for (;;) {
        osDelay(APP_WATCHDOG_TASK_IDLE_TIMEOUT_MS);
    }
}
