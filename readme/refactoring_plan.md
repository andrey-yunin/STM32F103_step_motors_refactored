# Пошаговый план рефакторинга системы управления движением

## 1. Общая цель

Миграция системы управления 8 шаговыми двигателями с программной генерации импульсов (в прерывании таймера) на **аппаратную генерацию** с использованием ШИМ (PWM) на отдельных каналах таймеров. Это обеспечит стабильность, производительность и масштабируемость системы.

---

## 2. План работ по файлам

### **Этап 0: Конфигурация оборудования и генерация кода (STM32CubeMX)**

**Это обязательный предварительный шаг, который выполняется пользователем вручную.** Без него дальнейший рефакторинг невозможен.

**Файл:** `STM32F103_step_motors_refactored.ioc`

**Действия в STM32CubeMX (20.02.2026):**

- [x] **1. Очистка GPIO:** Убедитесь, что все предыдущие назначения GPIO, относящиеся к управлению двигателями (STEP, DIR, ENABLE), удалены или освобождены.
- [x] **2. Настройка Таймеров (TIM1 и TIM2):**
    - [x] Активируйте **TIM1** и **TIM2**.
    - [x] Для каждого из таймеров включите все четыре канала (Channel 1, 2, 3, 4) в режим **`PWM Generation CHx`**.
    - [x] **Параметры таймеров (вкладка `Parameter Settings`):**
        - [x] `Prescaler`: **63** (для тактовой частоты таймера 1 МГц)
        - [x] `Counter Period (ARR)`: **999** (для базовой частоты ШИМ 1 кГц)
        - [x] `Pulse` (для каждого канала): **500** (для скважности ~50%)
- [x] **3. Настройка CAN (Автоматическое переназначение):**
    - [x] Активируйте CAN1. Убедитесь, что пины настроены следующим образом (CubeMX должен был их автоматически переназначить, если ранее были конфликты):
        - [x] `CAN_RX`: `PB8`
        - [x] `CAN_TX`: `PB9`
- [x] **4. Настройка UART (для драйверов TMC2209):**
    - [x] Активируйте `USART1` и `USART2` в асинхронном режиме.
    - [x] **Финальная распиновка UART:**
        - [x] `UART1_RX`: `PB7`
        - [x] `UART1_TX`: `PB6`
        - [x] `UART2_RX`: `PA3`
        - [x] `UART2_TX`: `PA2`
- [x] **5. Настройка GPIO (DIR и ENABLE):**
    - [x] Сконфигурируйте следующие пины как **`GPIO_Output`**. Для каждого пина рекомендуется установить `User Label` для ясности.
    - [x] **Пины направления (DIR):**
        - [x] `PB0` (DIR_1)
        - [x] `PB1` (DIR_2)
        - [x] `PB2` (DIR_3)
        - [x] `PB3` (DIR_4)
        - [x] `PB4` (DIR_5)
        - [x] `PB5` (DIR_6)
        - [x] `PB12` (DIR_7)
        - [x] `PB13` (DIR_8)
    - [x] **Пины включения (ENABLE):**
        - [x] `PA4` (EN_1)
        - [x] `PA5` (EN_2)
        - [x] `PA6` (EN_3)
        - [x] `PA7` (EN_4)
        - [x] `PA12` (EN_5)
        - [x] `PA15` (EN_6)
        - [x] `PB14` (EN_7)
        - [x] `PB15` (EN_8)
- [x] **6. Проверка пинов STEP (Автоматическое назначение):** Убедитесь, что CubeMX автоматически назначил следующие пины для ШИМ-каналов:
    - [x] `TIM1_CH1`: `PA8`
    *   `TIM1_CH2`: `PA9`
    *   `TIM1_CH3`: `PA10`
    *   `TIM1_CH4`: `PA11`
    *   `TIM2_CH1`: `PA0`
    *   `TIM2_CH2`: `PA1`
    *   `TIM2_CH3`: `PB10`
    *   `TIM2_CH4`: `PB11`
- [x] **7. Генерация кода:**
    - [x] После завершения всех настроек, выполните **`Project -> Generate Code`** (Ctrl+Shift+G). Это обновит проект, добавив в него код инициализации HAL для сконфигурированной периферии.

---

### **Этап 1: Создание и подготовка нового драйвера движения**

- [x] **1. Файлы**: `App/src/motor_gpio.c`, `App/inc/motor_gpio.h`
    - [x] **Действие:** Переименовать файлы в `motion_driver.c` и `motion_driver.h`.

- [x] **2. Файл**: `App/inc/motion_driver.h`
    - [x] **Действие:** Полностью очистить и определить новый интерфейс (API).
    ```c
    #ifndef __MOTION_DRIVER_H
    #define __MOTION_DRIVER_H

    #include "stm32f1xx_hal.h"
    #include <stdbool.h>

    void MotionDriver_Init(void);
    void MotionDriver_SetDirection(uint8_t motor_id, bool forward);
    void MotionDriver_StartMotor(uint8_t motor_id, uint32_t frequency);
    void MotionDriver_StopMotor(uint8_t motor_id);
    void MotionDriver_PulseGenerationCompleted_Callback(TIM_HandleTypeDef *htim);

    #endif // __MOTION_DRIVER_H
    ```

- [x] **3. Файл**: `App/src/motion_driver.c`
    - [x] **Действие:** Очистить и создать "заглушки" для новых функций.

---

### **Этап 2: Деактивация старого механизма генерации шагов**

- [x] **4. Файл**: `Core/Src/stm32f1xx_it.c`
    - [x] **Действие:** Найти `HAL_TIM_OC_DelayElapsedCallback`. Закомментировать или удалить из нее код, связанный с `Motor_ToggleStepPin`.

- [x] **5. Файл**: `App/src/tasks/task_motion_controller.c`
    - [x] **Действие:** Найти и закомментировать строку `HAL_TIM_OC_Start_IT(...)`.

---

### **Этап 3: Интеграция и реализация нового драйвера**

- [x] **6. Файл**: `App/src/motion_driver.c`
    - [x] **Действие:** Наполнить функции реальным кодом, используя `HAL_TIM_PWM_Start`, `HAL_TIM_PWM_Stop` и другие функции HAL, сгенерированные на Этапе 0.

- [x] **7. Файл**: `App/src/tasks/task_motion_controller.c`
    - [x] **Действие:** Переписать логику задачи для использования нового API из `motion_driver.h`.

- [x] **8. Файлы по всему проекту**:
    - [x] **Действие:** Заменить все `#include "motor_gpio.h"` на `#include "motion_driver.h"`.

---

### **Этап 4: Адаптация планировщика**

- [x] **9. Файл**: `App/src/motion_planner.c` (и `.h`)
    - [x] **Действие:** Адаптировать планировщик для работы с частотами, а не с периодами импульсов, для реализации профилей ускорения.

---

### **Этап 5: Интеграция с дирижером по CAN (05.03.2026)**

**Цель:** Привести CAN-протокол исполнителя в полное соответствие с протоколом дирижера (`can_packer.h`, `device_mapping.h`), исправить выявленные баги, реализовать финальную архитектуру обработки команд.

**Фаза A: Исправление критических багов**

- [x] **10. Файл**: `.ioc` (STM32CubeMX)
    - [x] **Действие:** Включить прерывание `CAN1 RX0 interrupts` в NVIC Settings. Перегенерировать код.
    - [x] **Проверка:** В `stm32f1xx_it.c` появился `USB_LP_CAN1_RX0_IRQHandler`.

- [x] **11. Файл**: `Core/Src/stm32f1xx_it.c`
    - [x] **Действие:** Убедиться, что `CAN1_RX0_IRQHandler` вызывает `HAL_CAN_IRQHandler(&hcan)`. Проверено.

- [x] **12. Файл**: `App/src/tasks/task_command_parser.c`
    - [x] **Действие:** Добавлен `osMessageQueueGet()` в начало цикла. Убран недоступный `osDelay(1)`.

- [x] **13. Файл**: `.ioc` (STM32CubeMX)
    - [x] **Действие:** Убран One Pulse Mode для TIM1. Включен AutomaticOutput для MOE. Перегенерирован код.

- [x] **14. Файлы**: `task_motion_controller.c`, `app_config.h`
    - [x] **Действие:** Унифицирован тип данных в motion_queue. Добавлено поле `command_id` в `MotionCommand_t`. Исправлены ссылки `.payload` → `.steps`.

**Фаза B: Создание модулей протокола и маппинга**

- [x] **15. Файл**: `App/inc/can_protocol.h` (СОЗДАН)
    - [x] **Действие:** Константы протокола дирижера (адаптация `can_packer.h`):
        - Приоритеты, типы сообщений, адреса узлов
        - Коды команд: `CAN_CMD_MOTOR_ROTATE` (0x0101), `HOME` (0x0102), `START_CONTINUOUS` (0x0103), `STOP` (0x0104)
        - Макросы `CAN_BUILD_ID`, `CAN_GET_PRIORITY/MSG_TYPE/DST_ADDR/SRC_ADDR`
        - Коды ошибок: `CAN_ERR_UNKNOWN_CMD`, `CAN_ERR_INVALID_MOTOR_ID`, `CAN_ERR_MOTOR_BUSY`
        - Прототип `CAN_SendDone(uint16_t cmd_code, uint8_t device_id)`

- [x] **16. Файлы**: `App/inc/device_mapping.h`, `App/src/device_mapping.c` (СОЗДАНЫ)
    - [x] **Действие:** Таблица маппинга логических device_id в физические motor_id:
        - 7 активных шаговых моторов: `1→0, 2→1, 3→2, 10→3, 11→4, 20→5, 21→6`
        - `DEV_MIXER_PADDLE_MOTOR (22)` исключён (DC-мотор, другая плата)
        - Индекс 7 (`TIM2_CH4`, `PB11`) — свободный резерв
        - `MOTOR_ID_INVALID = 0xFF` для невалидных ID

**Фаза C: Переработка архитектуры задач**

- [x] **17. Файл**: `App/inc/app_config.h`
    - [x] **Действие:** Добавлена `ParsedCanCommand_t` (промежуточная структура CAN Handler → Parser).
    - [x] **Действие:** Добавлены в `MotionCommand_t` поля `cmd_code` (uint16_t) и `device_id` (uint8_t) для ACK/NACK/DONE.
    - [x] **Действие:** Добавлены определения `FLAG_CAN_RX` (0x01) и `FLAG_CAN_TX` (0x02).

- [x] **18. Файл**: `Core/Src/main.c`
    - [x] **Действие:** Изменён размер элемента `parser_queue` на `sizeof(ParsedCanCommand_t)`.

- [x] **19. Файлы**: `Core/Src/stm32f1xx_it.c`, `App/inc/app_queues.h`
    - [x] **Действие:** Добавлен `osThreadFlagsSet(task_can_handleHandle, FLAG_CAN_RX)` в callback.
    - [x] **Действие:** Добавлен `extern osThreadId_t task_can_handleHandle` в `app_queues.h`.

- [x] **20. Файл**: `App/src/tasks/task_can_handler.c`
    - [x] **Действие:** Полная переработка на транспортный уровень:
        - Реализован `osThreadFlagsWait(FLAG_CAN_RX | FLAG_CAN_TX, osFlagsWaitAny, osWaitForever)`
        - По `FLAG_CAN_RX`: извлечение фрейма, валидация (ExtID, DstAddr, MsgType, DLC>=3), упаковка в `ParsedCanCommand_t` → `parser_queue`
        - По `FLAG_CAN_TX`: извлечение из `can_tx_queue`, отправка через `HAL_CAN_AddTxMessage`
        - Убран весь прикладной парсинг, ACK перенесён в публичную функцию
        - Публичные функции: `CAN_SendAck()`, `CAN_SendNackPublic()`, `CAN_SendDone()` — все с `osThreadFlagsSet(FLAG_CAN_TX)`

- [x] **21. Файл**: `App/src/tasks/task_command_parser.c`
    - [ ] **Действие:** Полная переработка на прикладной уровень:
        - Читать `ParsedCanCommand_t` из `parser_queue`
        - Транслировать `device_id` → `physical_motor_id` через `DeviceMapping_ToPhysicalId()`
        - NACK при `MOTOR_ID_INVALID`
        - Парсинг параметров по `cmd_code`:
            - `CAN_CMD_MOTOR_ROTATE`: steps (int32_t LE из data[0..3]), speed (data[4])
            - `CAN_CMD_MOTOR_HOME`: speed (uint16_t LE из data[0..1])
            - `CAN_CMD_MOTOR_START_CONTINUOUS`: speed (data[0])
            - `CAN_CMD_MOTOR_STOP`: без параметров
        - Формирование `MotionCommand_t` с заполнением `cmd_code` и `device_id`
        - Отправка в `motion_queue`

- [x] **22. Файл**: `App/src/tasks/task_motion_controller.c`
    - [x] **Действие:** Добавлена логика ответов дирижеру:
        - При `g_motor_active[motor_id]` → NACK (`CAN_ERR_MOTOR_BUSY`) с `cmd_code` и `device_id`
        - При успешном приёме → ACK (с `cmd_code`)
        - По завершении движения → DONE (с `cmd_code` и `device_id`) через `CAN_SendDone()`
        - Все ответы через `can_tx_queue` + `osThreadFlagsSet(FLAG_CAN_TX)`

**Фаза D: Финализация и верификация**

- [x] **23. Файл**: `App/src/tasks/task_tmc2209_manager.c`
    - [x] **Действие:** Раскомментирована инициализация всех 8 драйверов. USART1 (моторы 0-3), USART2 (моторы 4-7).

- [ ] **24. Тестирование**
    - [ ] **Действие:** Подготовить тестовый CAN-кадр в формате дирижера:
    - [ ] Пример: `MOTOR_ROTATE` для device_id=1, 1000 шагов:
        - Extended CAN ID = `CAN_BUILD_ID(0, 0, 0x20, 0x10)` = `0x00201000`
        - DLC = 8
        - Data = `01 01 01 E8 03 00 00 64` (cmd=0x0101, device=1, steps=1000, speed=100)
    - [ ] **Ожидаемый результат:**
        1. ACK от исполнителя (ExtID с MsgType=1)
        2. Мотор 0 (physical) вращается
        3. DONE от исполнителя (ExtID с MsgType=3, SubType=0x01, device_id=1)