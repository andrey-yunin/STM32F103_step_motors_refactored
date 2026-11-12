/*
 * task_watchdog.h
 *
 *  Created on: Apr 23, 2026
 *      Author: andrey
 */

#ifndef WATCHDOG_H_
#define WATCHDOG_H_

#include <stdint.h>

#define APP_WATCHDOG_TASK_IDLE_TIMEOUT_MS    500U
#define APP_WATCHDOG_SUPERVISOR_PERIOD_MS    1000U

typedef enum {
    APP_WDG_CLIENT_CAN = 0,
    APP_WDG_CLIENT_DISPATCHER,
    APP_WDG_CLIENT_MOTION,
    // TMC не входит в критичные клиенты до MU-07: heartbeat обязан означать
    // прогресс, а не просто существование задачи. Вернётся с job executor.
    APP_WDG_CLIENT_COUNT
} AppWatchdogClient_t;

void AppWatchdog_Heartbeat(AppWatchdogClient_t client);
void app_start_task_watchdog(void *argument);

#endif /* WATCHDOG_H_ */
