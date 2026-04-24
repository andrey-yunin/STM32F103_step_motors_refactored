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

        ---

        ## 8. Этап: Переход к «Золотому эталону» DDS-240 (Апрель 2026)

        **Цель:** Стандартизация архитектуры по образцу платы термодатчиков: внедрение единого Диспетчера, динамического NodeID и сервисных команд 0xFxxx.

        ### **Шаг 1: Переименование и унификация (Стабилизация)**
        - [x] **1.1. Переименование файлов и сущностей:**
            - [x] `task_command_parser.c/h` -> `task_dispatcher.c/h`
            - [x] `app_start_task_command_parser` -> `app_start_task_dispatcher`
            - [x] `parser_queueHandle` -> `dispatcher_queueHandle`
        - [x] **1.2. Обновление базового протокола (`can_protocol.h`):**
            - [x] Добавление определений сервисных команд (0xF0xx).
            - [x] Добавление Device Type ID (0x20 для моторов).
            - [x] Определение Magic Keys для Reboot/Factory Reset.
        - [x] **1.3. Синхронизация `app_config.h`:**
                    - [x] Приведение структуры `ParsedCanCommand_t` к единому стандарту.
                    - [x] Унификация имен констант очередей (`DISPATCHER_QUEUE_LEN`).
        - [x] **1.4. Инкапсуляция и защита глобальных переменных:**

            - [x] Создание Mutex-защищенных геттеров/сеттеров в прикладных задачах.
            - [x] Изоляция массивов `motor_states` и `tmc_drivers` (перенос в `task_motion_controller.c` и `task_tmc2209_manager.c`).
            - [x] Удаление файлов `app_globals.h/c` за ненадобностью.
            - [x] Перенос `g_performer_id` (временное владение в `task_can_handler.c`).

        ### **Шаг 2: Перенос функционала и ревайринг (Logic Migration)**
        - [ ] **2.1. Рефакторинг цикла Диспетчера:**
        - [ ] Внедрение немедленного `CAN_SendAck()` при получении команды.
        - [ ] Перенос логики отправки `DONE` из `MotionController` в `Dispatcher` (через систему событий или флагов).
        - [ ] **2.2. Централизация ответов:**
        - [ ] Все вызовы `CAN_SendAck`, `CAN_SendDone`, `CAN_SendNack` должны идти только из `Dispatcher` (кроме критических ошибок транспорта).

        ### **Шаг 3: Проверка сборки №1**
        - [ ] **3.1. Компиляция проекта.**
        - [ ] **3.2. Тестирование базового цикла:** `Command -> ACK (Dispatcher) -> DONE (Dispatcher по сигналу от Мотора)`.

        ### **Шаг 4: Инфраструктура и Сервис (Expansion)**
        - [ ] **4.1. Модуль Flash (`app_flash.h/c`):**
        - [ ] Реализация хранения `performer_id` и таблицы маппинга моторов.
        - [ ] Добавление Mutex-защиты доступа к конфигу.
        - [ ] **4.2. Сервисные команды (0xF0xx):**
        - [ ] Реализация `0xF001` (Get Info: UID, Version, Type).
        - [ ] Реализация `0xF002` (Reboot) и `0xF003` (Flash Commit).
        - [ ] Реализация `0xF005` (Set NodeID).
        - [ ] **4.3. Динамическая фильтрация:**
        - [ ] Настройка bxCAN фильтров в `task_can_handler.c` на основе данных из Flash.

        ### **Шаг 5: Итоговая проверка**
        - [ ] **5.1. Полная проверка совместимости с сервисным ПО.**
        - [ ] **5.2. Валидация сохранения маппинга и NodeID после перезагрузки.**

        ---

        ## 8. Этап: Переход к «Золотому эталону» DDS-240 (Апрель 2026)

        **Цель:** Стандартизация архитектуры по образцу платы термодатчиков: внедрение единого Диспетчера, динамического NodeID и сервисных команд 0xFxxx.

        ### **Шаг 1: Синхронизация "Скелета" (Транспорт и Диспетчеризация)**
        - [ ] **1.1. CAN Handler Upgrade:**
            - [ ] Исправить фильтр bxCAN: разрешить прием сообщений с `DstAddr == 0x00` (Broadcast).
            - [ ] Внедрить проверку свободных Mailbox перед `HAL_CAN_AddTxMessage` (Retry logic).
        - [ ] **1.2. Protocol Sync:**
            - [ ] В `CAN_SendData` добавить байт Sequence Info (`0x80`) для соответствия эталону `temp_sensors`.
        - [ ] **1.3. Dispatcher Refactoring:**
            - [ ] Заменить магические числа (литералы ошибок) на константы из `can_protocol.h`.
            - [ ] Синхронизировать структуру сервисных команд (0xF0xx) с проектом датчиков.

        ### **Шаг 3: Сервис и Flash**
        - [x] **3.1. AppConfig & Flash:** Реализована Mutex-защита и CRC16.
        - [ ] **3.2. TMC2209 IT-Mode:** Перевод UART драйверов в неблокирующий режим (прерывания) — *в плане*.

        ---

        ## 9. Перспективная модернизация (Backlog)

        Задачи, которые могут быть реализованы при усложнении технологических требований к устройству:

        ### 9.1. Реформа подсистемы движения (Hardware Reform)
        *   **Independent Speed Control:** Если потребуется независимое движение моторов на разных скоростях, необходим переход на **Interrupt-based DDA** на базе `TIM3` (20-50 кГц).
        *   **Advanced Planning:** Внедрение S-кривых (S-Curve Acceleration) для минимизации рывков при работе с тяжелыми нагрузками.
        *   **Step Counter DONE:** Реализация прерывания для точного подсчета шагов и автоматической отправки `DONE` (в текущей версии `DONE` отправляется контроллером движения по логическому завершению цикла).

---

## 10. Этап: CAN-level стабилизация перед тестами без нагрузок (22.04.2026)

**Цель:** Проверить и довести текущую реализацию CAN-уровня до минимально совместимого состояния с DDS-240 pattern, не добавляя новый диагностический функционал.

### 10.1. Выполнено

- [x] **CAN bitrate:** проект переведен на `1 Mbit/s`:
    - [x] `.ioc`: `CAN.CalculateBaudRate=1000000`, `CAN.Prescaler=2`.
    - [x] `main.c`: `hcan.Init.Prescaler = 2`, `BS1=11TQ`, `BS2=4TQ`.
- [x] **TX FIFO priority:** включен `TransmitFifoPriority = ENABLE`.
- [x] **Strict DLC=8:** CAN handler отбрасывает входящие `COMMAND` frames с `DLC != 8`.
- [x] **Zero padding:** `ACK`, `NACK`, `DATA`, `DONE` формируются через `memset(&tx, 0, sizeof(tx))` и уходят с `DLC=8`.
- [x] **Service constants:** добавлены `CAN_CMD_SRV_FACTORY_RESET` (`0xF006`) и `SRV_MAGIC_FACTORY_RESET` (`0xDEAD`).
- [x] **Factory reset dispatch:** `task_dispatcher.c` использует именованные константы вместо магических чисел `0xF006` и `0xDEAD`.
- [x] **Flash boundary:** `STM32F103C8TX_FLASH.ld` ограничен до `63K`, последняя страница `0x0800FC00` зарезервирована под `AppConfig`.
- [x] **FreeRTOS heap:** `configTOTAL_HEAP_SIZE` увеличен до `8192`.
- [x] **Queue creation checks:** в `main.c` добавлены проверки создания очередей и задач FreeRTOS с уходом в `Error_Handler()` при ошибке.
- [x] **CAN TX helper:** добавлен `CAN_QueueTxFrame()`, флаг `FLAG_CAN_TX` ставится только после успешной постановки кадра в `can_tx_queueHandle`.
- [x] **CAN RX ISR path:** `HAL_CAN_RxFifo0MsgPendingCallback()` ставит `FLAG_CAN_RX` только если фрейм помещен в `can_rx_queueHandle`.
- [x] **Форматирование:** прикладной слой `App/*` и затронутые `Core USER CODE` блоки приведены к единому стилю. `Drivers/Middlewares` не форматировались, чтобы не создавать vendor/CubeMX churn.

### 10.2. Проверка сборки

- [x] `make -C Debug all -j4` проходит.
- [x] Итоговый размер после последней CAN-правки:

```text
text=43044, data=256, bss=14448, total=57748
```

- [x] `git diff --check` чистый.

### 10.3. Физический CAN smoke-test

- [x] `F001 GET_DEVICE_INFO`:
    - [x] Команда: `cansend can0 00201000#01F0000000000000`.
    - [x] Факт: `ACK + 3 DATA + DONE`.
    - [x] Содержимое INFO: `device_type=0x01`, `fw=1.0`, `channels=8`.
- [x] `F004 GET_UID`:
    - [x] Команда: `cansend can0 00201000#04F0000000000000`.
    - [x] Факт: `ACK + 2 DATA + DONE`.
    - [x] UID: `0C 22 07 14 52 16 30 30 30 30 30 32`.
- [x] Unknown command:
    - [x] Команда: `cansend can0 00201000#FFFF000000000000`.
    - [x] Факт: `ACK + NACK(0x0001 Unknown Command)`.
- [x] Invalid motor id:
    - [x] Команда: `cansend can0 00201000#0101080000000000`.
    - [x] Факт: `ACK + NACK(0x0002 Invalid Channel)`.
- [x] Foreign destination:
    - [x] Команда: `cansend can0 00211000#01F0000000000000`.
    - [x] Факт: ответ отсутствует, кадр отброшен на transport-level.
- [x] Short DLC:
    - [x] Команда: `cansend can0 00201000#01F0`.
    - [x] Факт: ответ отсутствует, кадр с `DLC != 8` отброшен на transport-level.
- [x] Broadcast `F001 GET_DEVICE_INFO`:
    - [x] Команда: `cansend can0 00001000#01F0000000000000`.
    - [x] Факт: `ACK + 3 DATA + DONE` от Motion NodeID `0x20`.

### 10.4. Сознательно не включено в текущий этап

- [ ] `F007 GET_STATUS` и `CAN_STATUS_*` - отдельный этап статистики CAN-шины.
- [ ] `CAN1_SCE_IRQHandler()` и `HAL_CAN_ErrorCallback()` - включать вместе с CAN diagnostics.
- [ ] Watchdog/fault indication для сервисного инженера.
- [ ] Полная проверка `F002 REBOOT`, `F003 FLASH_COMMIT`, `F005 SET_NODE_ID`, `F006 FACTORY_RESET`.
- [ ] Реальный тест движения с нагрузкой и `DONE` по фактическому завершению шага.

### 10.5. Следующие тесты без нагрузок

- [x] CAN Level A / acceptance без нагрузок: `F001`, `F004`, unknown command, invalid motor id, foreign destination, short DLC, broadcast discovery.
- [x] Перейти к доменным командам Motion без подключенных нагрузок.
- [x] Проверить безопасную реакцию `STOP` для валидного канала:
    - [x] `STOP motor 0`: `ACK + DONE`.
    - [x] `STOP motor 7`: `ACK + DONE`.
- [x] Проверить `HOME motor 0` без движения:
    - [x] Команда: `cansend can0 00201000#020100C800000000`.
    - [x] Факт: `ACK + DONE`.
- [x] Проверить `START_CONTINUOUS speed=0`:
    - [x] Команда: `cansend can0 00201000#0301000000000000`.
    - [x] Факт: `ACK + DONE`, PWM не стартует при неактивном моторе.
- [x] Проверить `ROTATE` на минимальном количестве шагов без нагрузки:
    - [x] Команда: `cansend can0 00201000#0101000100000032`.
    - [x] Факт: `ACK`, автоматический `DONE` ожидаемо отсутствует.
    - [x] Обязательная остановка: `cansend can0 00201000#0401000000000000`, факт `ACK + DONE`.
- [x] Проверить `MOTOR_BUSY`:
    - [x] Повторный `ROTATE` на активный канал вернул `ACK + NACK(0x0003 Motor Busy)`.
    - [x] Последующий `STOP` вернул `ACK + DONE`.
- [x] Историческое ограничение этого no-load прогона снято в коде 24.04.2026: `ROTATE` получил finite STEP completion и самостоятельный `DONE`; требуется повторный no-load regression без штатного `STOP`.

---

## 11. Этап: доведение Motion до DDS-240 industrial pattern (план на 23.04.2026)

**Цель:** продолжить работу от проверенной точки 22-23.04.2026 без опоры на историю чата. Рабочий ориентир - `readme/DDS-240_eko_system/EXECUTOR_INDUSTRIALIZATION_PLAYBOOK.md`, `DDS-240_ECOSYSTEM_STANDARD.md`, `CONDUCTOR_INTEGRATION_GUIDE.md` раздел `8.9` и отчет `project_report.md` разделы `10.6..10.11`.

Старые разделы плана выше считаются историческими, если они противоречат текущей реализации. Актуальная база - раздел `10` и этот раздел `11`.

### 11.1. Текущая подтвержденная база

- [x] Сборка проходит: `make -C Debug all -j4`.
- [x] CAN transport: `1 Mbit/s`, 29-bit Extended ID, strict `DLC=8`, broadcast, foreign destination drop, TX FIFO priority.
- [x] Сервисный smoke: `F001 GET_DEVICE_INFO`, `F004 GET_UID`.
- [x] Negative/addressing tests: unknown command, invalid motor id, foreign destination, short DLC, broadcast discovery.
- [x] Доменные команды без нагрузок: `STOP 0`, `STOP 7`, `HOME 0` без движения, `START_CONTINUOUS speed=0`, `ROTATE + STOP`, `MOTOR_BUSY`.
- [x] `F007 GET_STATUS`: `ACK -> DATA metrics 0x0001..0x0012 -> DONE`, порядок многокадрового ответа подтвержден.
- [x] Регрессия после внедрения Motion safe-state hook подтверждена без нагрузки: `F001`, `F004`, `F007`, `STOP 0`, `STOP 7`, `ROTATE + STOP`, `MOTOR_BUSY`.
- [x] CAN diagnostics counters: `DROP_WRONG_DLC`, `DROP_WRONG_DST`, `DROP_WRONG_TYPE` проверены физически; queue/HAL/fault counters в нормальном прогоне нулевые.
- [x] Flash config page исключена из application area: `FLASH LENGTH = 63K`, config page `0x0800FC00..0x0800FFFF`.
- [x] RTOS resource checks для очередей и задач добавлены; heap установлен `8192`.

### 11.2. Открытые ограничения

- [ ] `DROP_NOT_EXT` не подтверждается программным счетчиком на текущем bxCAN-фильтре: Standard ID отсекается аппаратно до firmware. Поведение безопасное, ответ отсутствует.
- [ ] `CAN1_SCE_IRQHandler()` и `HAL_CAN_ErrorCallback()` подключены; в нормальном прогоне счетчики ошибок нулевые, но отдельная fault-injection проверка CAN error/status counters не выполнялась.
- [ ] `F002 REBOOT`, `F003 COMMIT`, `F005 SET_NODE_ID`, `F006 FACTORY_RESET` реализованы частично/в коде, но не закрыты стендовыми тестами на Motion.
- [x] Motion safe-state hook выделен как самостоятельный безRTOSный путь и подключен к startup/Error_Handler/fault handlers; стендовая регрессия без нагрузок после внедрения подтверждена 23.04.2026.
- [x] IWDG supervisor внедрен, собирается, прошел idle-тест без ложного reset и no-load fault-injection/recovery 23.04.2026.
- [ ] Физическое подтверждение safe-state уровней STEP/EN при watchdog fault path не выполнялось: на стенде нет нагрузки/измерения выходов.
- [ ] `ROTATE/HOME` не имеют автоматического подсчета шагов, остановки и `DONE` по фактическому завершению.
- [ ] Физические тесты с драйвером/мотором не начинались.

### 11.3. Рекомендуемый порядок работ

#### A. `F007 GET_STATUS` и CAN diagnostics

- [x] Синхронизировать `App/inc/can_protocol.h` с общим набором DDS-240:
    - [x] `CAN_CMD_SRV_GET_STATUS = 0xF007`.
    - [x] `CAN_STATUS_*` metric_id `0x0001..0x0012`.
    - [x] добавлен общий `CAN_ERR_INVALID_PARAM = 0x0006`, проверенные коды `0x0001..0x0005` не изменены.
- [x] Добавить структуру диагностических счетчиков CAN:
    - [x] RX accepted.
    - [x] TX submitted to HAL.
    - [x] RX queue overflow.
    - [x] TX queue overflow.
    - [x] Dispatcher queue overflow.
    - [x] dropped not Extended ID.
    - [x] dropped wrong destination.
    - [x] dropped wrong message type.
    - [x] dropped wrong DLC.
    - [x] TX mailbox timeout.
    - [x] TX HAL error.
    - [x] CAN error callback count.
    - [x] error warning/passive/bus-off counters.
    - [x] last HAL error.
    - [x] last ESR snapshot.
    - [x] domain/application queue overflow.
- [x] Инкрементировать счетчики строго в местах фактического события:
    - [x] transport drops - в `task_can_handler.c`.
    - [x] RX ISR queue overflow - в `stm32f1xx_it.c`.
    - [x] dispatcher queue overflow - при `osMessageQueuePut(dispatcher_queueHandle, ...) != osOK`.
    - [x] motion queue overflow - при `osMessageQueuePut(motion_queueHandle, ...) != osOK`.
    - [x] TX mailbox/HAL errors - в TX path CAN handler.
- [x] Добавить snapshot API, чтобы Dispatcher мог безопасно читать счетчики без прямого доступа к внутренним static-данным CAN handler.
- [x] Реализовать `GET_STATUS`: `ACK -> DATA metric 0x0001..0x0012 -> DONE`.
- [x] Подключить CAN SCE IRQ и `HAL_CAN_ErrorCallback()` для error/status diagnostics.
- [x] Сборка после реализации проходит:
    - [x] `PATH=/home/andrey/st/stm32cubeide_1.19.0/plugins/com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.13.3.rel1.linux64_1.0.0.202410170706/tools/bin:$PATH make -C Debug all -j4`.
    - [x] Размер: `text=43892`, `data=256`, `bss=14520`, `total=58668`.
- [x] Проверить на стенде:
    - [x] `cansend can0 00201000#07F0000000000000`.
    - [x] порядок `ACK -> DATA metrics 0x0001..0x0012 -> DONE`.
    - [x] `Short DLC`: `DROP_WRONG_DLC (0x0009) = 1`.
    - [x] `Foreign dst`: `DROP_WRONG_DST (0x0007) = 1`.
    - [x] `Wrong msg type`: `DROP_WRONG_TYPE (0x0008) = 1`.
    - [x] `Standard ID`: ответа нет, `DROP_NOT_EXT (0x0006) = 0`, так как кадр отсекается аппаратным CAN-фильтром до software.
    - [x] Финальный снимок после negative tests: `RX_TOTAL=5`, `TX_TOTAL=81`, `DROP_WRONG_DST=1`, `DROP_WRONG_TYPE=1`, `DROP_WRONG_DLC=1`, `0x000A..0x0012=0`.
- [ ] Отложено на fault/stress этап: принудительное подтверждение queue overflow, TX mailbox timeout, HAL CAN error и CAN error warning/passive/bus-off counters.

#### B. Проверка сервисного слоя `F002/F003/F005/F006`

- [ ] `REBOOT` с неверным ключом: ожидать `ACK + NACK 0x0004`.
- [ ] `REBOOT` с ключом `0x55AA`: ожидать `ACK + DONE`, reset, затем повторный `F001`.
- [ ] `COMMIT`: ожидать `ACK + DONE`; после него проверить, что плата продолжает отвечать.
- [ ] `SET_NODE_ID` RAM transition: проверить смену `0x20 -> 0x21 -> 0x20` без `COMMIT`.
- [ ] `SET_NODE_ID + COMMIT + REBOOT`: проверить сохранение адреса после reset.
- [ ] `FACTORY_RESET` с ключом `0xDEAD`: проверить возврат default `0x20`.
- [ ] Зафиксировать в `project_report.md` фактические ID, команды и результат.

#### C. Motion safe-state hook

- [x] Выделить безRTOSную функцию безопасного состояния `MotionDriver_AllSafe()`:
    - [x] остановить PWM/STEP на всех каналах;
    - [x] disable всех драйверов;
    - [x] не использовать CAN, queue, mutex, dynamic memory, длительные задержки.
- [x] Вызывать safe-state:
    - [x] при старте после инициализации GPIO/таймеров;
    - [x] в `Error_Handler()`;
    - [x] в fault handlers до аварийного цикла.
- [x] Свести штатную инициализацию Motion к тому же safe-state hook, чтобы не было двух разных путей безопасного отключения.
- [x] Сборка после внедрения прошла через локальный STM32CubeIDE toolchain.
- [x] Проверить регрессию CAN и доменных команд без нагрузок после внедрения:
    - [x] сборка: `text=44180`, `data=256`, `bss=14520`, `total=58956`;
    - [x] `F001 GET_DEVICE_INFO`: `ACK + 3 DATA + DONE`;
    - [x] `F004 GET_UID`: `ACK + 2 DATA + DONE`;
    - [x] `F007 GET_STATUS`: `ACK -> DATA metrics 0x0001..0x0012 -> DONE`, counters без ошибок;
    - [x] `STOP 0`, `STOP 7`: `ACK + DONE`;
    - [x] `ROTATE + STOP`: `ROTATE` дает `ACK`, `STOP` дает `ACK + DONE`;
    - [x] `MOTOR_BUSY`: повторный `ROTATE` дает `ACK + NACK 0x0003`, затем `STOP` дает `ACK + DONE`.
- [ ] Опционально, не блокер текущего этапа: выполнить safe-state hardening из `DDS-240_ECOSYSTEM_STANDARD.md` раздел `2.5.1.1`:
    - [ ] прямой GPIO `BSRR/BRR` в fault path;
    - [ ] дополнительное отключение timer outputs/interrupts pending bits;
    - [ ] повторный safe-state после инициализации TIM1/TIM2.

#### D. IWDG supervisor

- [x] Включить IWDG в CubeMX только после safe-state hook.
- [x] Настроить IWDG по проверенному STM32F103 Fluidics profile: `Prescaler=256`, `Reload=624`.
- [x] Добавить `task_watchdog` как отдельную supervisor-задачу.
- [x] Клиенты heartbeat: CAN handler, Dispatcher, MotionController, TMC manager.
- [x] Перевести ожидания критических задач с бесконечных блокировок на finite timeout, чтобы они могли публиковать heartbeat.
- [x] Единственное место `HAL_IWDG_Refresh()` - supervisor task.
- [x] При отсутствии heartbeat supervisor вызывает `MotionDriver_AllSafe()` и прекращает refresh IWDG.
- [x] Сборка после внедрения проходит:
    - [x] `PATH=/home/andrey/st/stm32cubeide_1.19.0/plugins/com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.13.3.rel1.linux64_1.0.0.202410170706/tools/bin:$PATH make -C Debug -j4 all`;
    - [x] размер: `text=44824`, `data=256`, `bss=14552`, `total=59632`.
- [ ] Проверить:
    - [x] idle без ложного reset: пауза около 134 s, затем `F001` и `F007`; counters сохранились и выросли `RX_TOTAL 3 -> 5`, `TX_TOTAL 26 -> 51`, error/drop/fault counters `0`;
    - [x] fault-injection зависания задачи: временный hook остановил heartbeat `MotionController` после `ROTATE` motor 0;
    - [x] safe-state перед reset подтвержден на уровне software path: supervisor вызывает `MotionDriver_AllSafe()` перед прекращением refresh IWDG;
    - [x] восстановление связи через `F001` после watchdog reset;
    - [x] после теста временный hook выключен: `APP_WATCHDOG_TEST_STALL_MOTION_AFTER_ROTATE = 0U`;
    - [ ] физическое измерение STEP/EN в watchdog fault path при подключенном измерительном оборудовании.

#### E. Архитектура завершения движения `ROTATE/HOME`

- [x] Не начинать как мелкую правку. Архитектурное решение принято и зафиксировано до кодовых изменений.
- [x] Разделить терминологию:
    - [x] Host-level команды: `REACTION_ROTATE`, `SAMPLE_ROTATE`, `REAGENT_ROTATE`, `DISPENSER_MOVE` и т.п.; их получает Дирижер от Host;
    - [x] low-level Executor команды: CAN `cmd_code`, например `MOTOR_ROTATE 0x0101`, `MOTOR_HOME 0x0102`, `MOTOR_START_CONTINUOUS 0x0103`, `MOTOR_STOP 0x0104`, сервисные `0xF001..0xF007`;
    - [x] внутренние команды прошивки: `CommandID_t` (`CMD_MOVE_RELATIVE`, `CMD_SET_SPEED`, `CMD_STOP`), не внешний CAN/Host контракт;
    - [x] физический уровень: TIM/PWM/STEP/DIR/EN.
- [x] Разделить `DONE` по уровням:
    - [x] `Host DONE` отправляет только Дирижер после завершения всего recipe/job;
    - [x] `Executor DONE` завершает только одну low-level atomic-команду Дирижера;
    - [x] внутренние события прошивки не называть `DONE`; использовать `COMPLETE/EXPIRED/STOPPED/FAULT`;
    - [x] CAN `DONE` отправлять из task context после обновления state, а не напрямую из ISR.
- [x] В отчетах по `ACK/DONE/NACK` использовать только внешний low-level `cmd_code`; внутренние `CommandID_t` упоминать только как implementation detail.
- [x] Зафиксировать текущую фактическую матрицу `DONE` для Motion Executor по low-level `cmd_code`.
- [x] Зафиксировать контракт скорости/ускорения:
    - [x] Host API задает технологическую цель, а не скорость/ускорение мотора;
    - [x] `speed` является контрактом recipe-level atomic action; Дирижер выбирает его из recipe/action config или технологического профиля;
    - [x] `MOTOR_ROTATE` low-level payload содержит `steps` и `speed`;
    - [x] Motion Executor обязан применить `speed` текущей finite-команды;
    - [x] Motion Executor валидирует `speed` против локального профиля оси и не запускает движение при небезопасном/невалидном значении;
    - [x] `acceleration` не входит в low-level payload и остается локальной конфигурацией Motion Planner/Driver;
    - [x] acceleration реализует физический профиль разгона/торможения внутри исполнителя и зависит от механики конкретной оси;
    - [x] `CMD_SET_SPEED/CMD_SET_ACCELERATION` считать implementation detail или будущими service/config-командами, но не Host-рецептными манипуляциями.
- [x] Зафиксировать выбранный режим текущего промышленного доведения:
    - [x] оставить текущий TIM1/TIM2 PWM path;
    - [x] ввести две независимые motion-группы: `TIM1 group = motors 0..3`, `TIM2 group = motors 4..7`;
    - [x] в каждой motion-группе разрешить максимум один active motor/profile одновременно;
    - [x] конфликтующую команду на занятую motion-group отклонять как `MOTOR_BUSY`;
    - [x] параллельные движения разрешать только если они находятся на разных motion-группах;
    - [x] `MOTOR_ROTATE` реализовать через счетчик STEP-событий, auto-stop PWM/STEP и low-level `DONE` из MotionController task context;
    - [x] acceleration/deceleration считать единым локальным профилем Motion Executor для всех моторов, с отдельным runtime-state на motion-group;
    - [x] `MOTOR_START_CONTINUOUS` оставить state/continuous-командой на PWM path;
    - [x] DDA/TIM3 не является обязательным условием текущего промышленного стандарта и переносится в перспективное развитие без блокировки `MOTOR_ROTATE -> DONE`.
- [ ] Требования к семантике:
    - [x] `DONE` finite-команды `MOTOR_ROTATE` отправляется только после фактического completion от STEP-счетчика;
    - [x] `MOTOR_ROTATE`: исполнитель получает `steps`, генерирует ровно `abs(steps)` STEP, сам останавливает PWM/STEP, обновляет позицию и отправляет `DONE`;
    - [ ] `MOTOR_HOME`: `DONE` только после home-condition, остановки PWM/STEP и обновления home/position state;
    - [x] `MOTOR_START_CONTINUOUS`: state/continuous команда, `DONE` после успешного входа в continuous mode;
    - [x] `MOTOR_STOP`: отдельная команда остановки active/continuous/прерванного движения, со своим `DONE`;
    - [x] `STOP` не является частью штатного завершения `ROTATE`;
    - [x] `MOTOR_BUSY` для конфликтующих команд, включая занятость общей TIM motion-group.
- [ ] Следующий практический шаг:
    - [x] реализовать completion для `MOTOR_ROTATE` через счетчик STEP-событий текущего TIM1/TIM2 PWM path;
    - [x] завершение делать внутренним событием MotionController, чтобы `DONE` отправлялся из task context, а не из ISR;
    - [x] при `MOTOR_ROTATE` использовать `speed` из текущей команды, а не только сохраненный/default `max_speed_steps_per_sec`;
    - [x] валидировать `speed` по локальным limits оси до запуска STEP;
    - [x] проверять занятость timer motion-group, а не только `motor_id`;
    - [x] хранить baseline runtime-state движения на уровне motion-group: active motor, target/current speed, target steps;
    - [x] для ненулевого `steps` отклонять `speed=0` как invalid-param;
    - [x] для `steps=0` оставлять быстрый `ACK -> DONE` без запуска PWM;
    - [ ] `HOME` с реальным датчиком/condition оставить отдельным hardware-блоком, если нет подтвержденного home input.
- [ ] После сборки прошивки выполнить no-load CAN regression: `ROTATE` должен дать `ACK`, затем самостоятельный `DONE` без штатного `STOP`.
- [ ] В no-load regression проверить `START_CONTINUOUS speed>0 -> ACK + DONE`, занятость TIM-группы и штатный выход через `MOTOR_STOP -> ACK + DONE`.
- [ ] После no-load regression переходить к физическим тестам STEP/EN на измерительном оборудовании и затем с мотором.

#### F. Координация Fluidics / Conductor по `DONE` и насосам

- [x] Зафиксировать единый смысл `DONE`: команда завершена по своему контракту, а не "устройство только стартовало".
- [x] Зафиксировать целевую модель насосов: объем Host-команды переводится Дирижером в `duration_ms`, затем Fluidics Executor выполняет `PUMP_RUN_DURATION` локально и отправляет `DONE` только после выключения насоса.
- [x] Подготовить документационное послание:
    - [x] Fluidics Executor: изменить логику `PUMP_RUN_DURATION`, `DONE` после штатного выключения по `duration_ms`;
    - [x] Conductor: заменить recipe-дозирование `START_PUMP + WAIT_MS + STOP_PUMP` на `PUMP_RUN_DURATION(duration_ms)`.
- [ ] После закрытия текущих корректировок Motion передать изменения в проект Fluidics.
- [ ] После закрытия текущих корректировок Motion передать изменения в проект Conductor packer/job_manager/recipe_store.
- [ ] Для Conductor добавить калибровочную функцию `volume_ul -> duration_ms` и policy ошибок при неизвестной калибровке.
- [ ] Для Fluidics проверить стендово: `PUMP_RUN_DURATION`, `duration=0`, `DEVICE_BUSY`, fault path без `DONE`.

#### G. Перспективное развитие Motion после текущего industrial baseline

- [ ] DDA/TIM3 рассматривать как отдельный roadmap-блок, не как обязательное условие текущего промышленного доведения.
- [ ] Возвращаться к DDA/TIM3 только при подтвержденной технологической необходимости:
    - [ ] независимые скорости нескольких осей на одном таймерном ресурсе;
    - [ ] более сложные профили движения, чем локальный acceleration profile текущего PWM path;
    - [ ] централизованный software step scheduler;
    - [ ] расширенная координация нескольких осей.
- [ ] При проектировании DDA/TIM3 не менять текущий low-level контракт `MOTOR_ROTATE(steps, speed) -> DONE`, если функционально не требуется новая версия команды.

#### H. Точка входа на 27.04.2026

- [x] Точка остановки 24.04.2026 зафиксирована:
    - [x] `MOTOR_ROTATE` кодово переведен на finite STEP completion;
    - [x] `MOTOR_START_CONTINUOUS speed>0` кодово переведен в state-команду входа в continuous PWM mode;
    - [x] command `speed` применяется и валидируется;
    - [x] TIM resource model реализован как две группы: `TIM1 motors 0..3`, `TIM2 motors 4..7`;
    - [x] сборка проходит: `text=47736`, `data=256`, `bss=14640`.
- [ ] Первый блок понедельника - no-load CAN regression текущей прошивки:
    - [ ] `F001 GET_DEVICE_INFO`;
    - [ ] `F007 GET_STATUS`;
    - [ ] `MOTOR_ROTATE small steps, valid speed`: ожидать `ACK -> DONE` без `STOP`;
    - [ ] `MOTOR_ROTATE steps != 0, speed=0`: ожидать `ACK -> NACK(CAN_ERR_INVALID_PARAM)`;
    - [ ] `MOTOR_BUSY` внутри `TIM1 group`;
    - [ ] `MOTOR_BUSY` внутри `TIM2 group`, если есть смысл проверять оба таймера;
    - [ ] параллельный запуск на разных группах, если no-load стенд позволяет;
    - [ ] `START_CONTINUOUS speed>0 -> ACK + DONE`, затем `MOTOR_STOP -> ACK + DONE`.
- [ ] Второй блок понедельника - измерение STEP/EN без механической нагрузки:
    - [ ] количество STEP равно `abs(steps)`;
    - [ ] после completion STEP остановлен;
    - [ ] EN переведен в idle/disabled;
    - [ ] `DONE` появляется после остановки STEP, а не сразу после старта.
- [ ] Третий блок после измерения - решение, можно ли переходить к мотору под нагрузкой или нужен фикс finite counting/IRQ.

### 11.4. Что не делать первым

- [ ] Не начинать с физической нагрузки до safe-state и понятной политики остановки.
- [x] Не добавлять watchdog до подтвержденной регрессии safe-state hook: выполнено ранее.
- [x] Не переделывать `ROTATE/HOME` без решения по генерации STEP и `DONE`: решение принято, `ROTATE` реализован, `HOME` оставлен отдельно.
- [ ] Не привязывать текущую приемку `MOTOR_ROTATE -> DONE` к переходу на DDA/TIM3.
- [ ] Не подключать `dds240_global_config.h` напрямую в проект механически; сначала выполнить diff-аудит и переносить только общие значения.
- [ ] Не считать старые разделы плана выше текущим чеклистом без сверки с разделами `10` и `11`.
