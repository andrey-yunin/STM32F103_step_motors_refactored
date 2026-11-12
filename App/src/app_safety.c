/*
 * app_safety.c
 *
 * Safety-граница исполнителя Motion: аварийный перевод выходов и латч
 * подготовки к сбросу. Не использует RTOS, CAN, очереди и динамическую
 * память; вызывается из task context, Error_Handler и fault-обработчиков.
 */
#include "app_safety.h"
#include "main.h"
#include "motion_driver.h"

static volatile bool s_reset_pending = false;

void AppSafety_AllSafe(void)
{
    MotionDriver_AllSafe();
}

bool AppSafety_IsResetPending(void)
{
    return s_reset_pending;
}

void AppSafety_PrepareReset(void)
{
    // Латч и безопасное состояние — одна операция. Исходное состояние IRQ
    // восстанавливаем: вызывающий код мог уже запрещать прерывания.
    const uint32_t saved_primask = __get_PRIMASK();
    __disable_irq();
    s_reset_pending = true;
    AppSafety_AllSafe();
    __DSB();
    __set_PRIMASK(saved_primask);
}
