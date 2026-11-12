/*
 * app_safety.h
 *
 * Единая точка аварийного перевода исполнительных выходов в безопасное
 * состояние. Владелец: слой safety. Не использует RTOS, CAN, очереди и
 * динамическую память; вызывается из Error_Handler, обработчиков fault
 * и супервизора watchdog. Физическая реализация — MotionDriver_AllSafe.
 */
#ifndef APP_SAFETY_H_
#define APP_SAFETY_H_

#include <stdbool.h>

void AppSafety_AllSafe(void);

/*
 * Подготовка к сбросу (F002/F006): атомарно ставит латч и переводит
 * выходы в безопасное состояние. После вызова новые активации запрещены
 * до перезапуска MCU. Не использует RTOS, CAN и динамическую память.
 */
void AppSafety_PrepareReset(void);
bool AppSafety_IsResetPending(void);

#endif /* APP_SAFETY_H_ */
