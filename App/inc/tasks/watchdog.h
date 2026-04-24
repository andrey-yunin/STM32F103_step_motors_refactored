/*
 * watchdog.h
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
    APP_WDG_CLIENT_TMC,
    APP_WDG_CLIENT_COUNT
} AppWatchdogClient_t;

void AppWatchdog_Heartbeat(AppWatchdogClient_t client);
void app_start_task_watchdog(void *argument);

#endif /* WATCHDOG_H_ */
