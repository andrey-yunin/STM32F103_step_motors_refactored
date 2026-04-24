# Подробный отчет о разработке прошивки для "Исполнителя" на STM32F103

## Оглавление

1.  **Начальная постановка задачи и анализ требований**
    *   1.1. Цель проекта
    *   1.2. Обзор аппаратной платформы
    *   1.3. Общая концепция архитектуры "Исполнителя"
2.  **Этап 1: Конфигурация проекта STM32CubeIDE и аппаратных ресурсов**
    *   2.1. Выбор и подтверждение микроконтроллера
    *   2.2. Настройка тактирования (Clock Configuration)
    *   2.3. Настройка периферии
        *   2.3.1. FDCAN (Controller Area Network)
        *   2.3.2. USART (для драйверов TMC2209)
        *   2.3.3. GPIO (для STEP/DIR/ENABLE)
        *   2.3.4. TIM2 (для генерации STEP-импульсов)
    *   2.4. Настройка FreeRTOS
        *   2.4.1. Выбор CMSIS_V2
        *   2.4.2. Корректировка размера кучи (Heap Size)
        *   2.4.3. Устранение ошибки "RAM overflow"
    *   2.5. Управление версиями (Git и GitHub)
        *   2.5.1. Инициализация локального репозитория
        *   2.5.2. Настройка `.gitignore`
        *   2.5.3. Загрузка проекта на GitHub
3.  **Этап 2: Проектирование и реализация программной архитектуры**
    *   3.1. Определение и уточнение архитектуры задач FreeRTOS
        *   3.1.1. Финальная архитектура FreeRTOS (4 задачи, 5 очередей)
    *   3.2. Проектирование CAN-адресации для мульти-устройств
        *   3.2.1. Иерархическая схема CAN ID
        *   3.2.2. Механизм определения `Performer_ID`
    *   3.3. **Концепция работы с глобальными переменными (Золотое Правило)**
        *   3.3.1. Категории глобальных переменных
        *   3.3.2. Файлы для определений и объявлений `extern`
    *   3.4. Создание файловой структуры приложения и "скелетов" модулей
        *   3.4.1. Структура папок `App/inc` и `App/src`
        *   3.4.2. Настройка `Source Location` и `Include Paths` в IDE
    *   3.5. Реализация вспомогательных модулей
        *   3.5.1. `app_config.h` (Константы и структуры конфигурации)
        *   3.5.2. `app_queues.h` (Объявления хэндлов очередей FreeRTOS)
        *   3.5.3. `app_globals.h` и `app_globals.c` (Глобальные переменные приложения)
        *   3.5.4. `command_protocol.h` (Определение команд CAN)
        *   3.5.5. `tmc2209_driver.h/.c` (Низкоуровневый драйвер TMC2209)
        *   3.5.6. `motion_planner.h/.c` (Планировщик движения)
        *   3.5.7. `motor_gpio.h/.c` (Абстракция GPIO для моторов)
4.  **Этап 3: Интеграция задач FreeRTOS и логики (Подготовка к Первому Тесту)**
    *   4.1. Настройка очередей FreeRTOS в `main.c`
    *   4.2. Подключение задач к `main.c`
    *   4.3. Реализация `Task_CAN_Handler`
    *   4.4. Реализация `Task_Command_Parser`
    *   4.5. Реализация `Task_TMC_Manager`
    *   4.6. Реализация `Task_Motion_Controller` и его прерывания
5.  **План Первого Теста и дальнейшие шаги**
    *   5.1. Необходимое оборудование и подключение
    *   5.2. Пример тестовой CAN-команды
    *   5.3. Ожидаемый результат
    *   5.4. Последующие улучшения и доработки
6.  **Этап Рефакторинга: Переход на аппаратный ШИМ (20.02.2026)**
    *   6.1. Выбор стратегии масштабирования таймеров
    *   6.2. Конфигурация таймеров TIM1 и TIM2 в STM32CubeMX (20.02.2026)
    *   6.3. Финальная распиновка управляющих сигналов (20.02.2026)

---

## 1. Начальная постановка задачи и анализ требований

### 1.1. Цель проекта
Создание прошивки для "Исполнителя" (Performer) в системе биохимического анализатора. "Исполнитель" является подчиненным устройством, управляемым "Дирижером" (Conductor) по CAN-шине, и отвечает за точное управление 8 шаговыми двигателями.

### 1.2. Обзор аппаратной платформы
*   **Микроконтроллер:** STM32F103C8T6 (серия STM32F1, 20 КБ RAM).
*   **Исполнительные узлы:** 8 драйверов шаговых двигателей TMC2209.
*   **Связь с "Дирижером":** CAN-шина.
*   **Внутренняя архитектура:** Использование FreeRTOS для многозадачности.

### 1.3. Общая концепция архитектуры "Исполнителя"
Прошивка будет модульной, с использованием FreeRTOS для управления concurrency. Основные компоненты включают:
*   Обработчик CAN-сообщений.
*   Парсер команд.
*   Контроллер движения (генерация STEP/DIR).
*   Менеджер драйверов TMC2209 (конфигурация по UART).

---

## 2. Этап 1: Конфигурация проекта STM32CubeIDE и аппаратных ресурсов

### 2.1. Выбор и подтверждение микроконтроллера
Изначально было предположение об STM32H723, но в ходе аудита проекта `STM32F103_step_motors_refactored.ioc` подтвержден микроконтроллер **STM32F103C8T6**. Все настройки и код адаптированы под эту платформу.

### 2.2. Настройка тактирования (Clock Configuration)
Система настроена на работу от внутреннего высокоскоростного генератора (HSI) с использованием PLL для достижения максимальной стабильной частоты.
*   **Источник:** HSI (8 MHz).
*   **PLL Multiplier:** x16 (HSI/2 -> 4 MHz * 16 = 64 MHz).
*   **SYSCLK (HCLK):** 64 MHz.
*   **APB1 Clock:** 32 MHz (что критично для CAN).

### 2.3. Настройка периферии

#### 2.3.1. FDCAN (Controller Area Network)
*   Включен FDCAN1.
*   **Скорость:** 500 кбит/с, достигнуто настройкой `Prescaler=4`, `Time Segment 1=11`, `Time Segment 2=4`.
*   **Прерывания:** `CAN1 RX0 interrupts` включены в `NVIC Settings`.

#### 2.3.2. USART (для драйверов TMC2209)
*   **USART1 и USART2** включены в асинхронном режиме.
*   **Скорость:** 115200 бит/с.
*   **Адресация:** 8 драйверов распределены на 2 UART (4 на USART1, 4 на USART2), с уникальной адресацией (0-3) на каждом UART через пины MS1/MS2.

#### 2.3.3. GPIO (для STEP/DIR/ENABLE)
*   Для каждого из 8 моторов выделены пины GPIO: `STEP_X` (GPIOA), `DIR_X` (GPIOB), `EN_X` (GPIOB).
*   Все пины сконфигурированы как `GPIO_Output`.

#### 2.3.4. TIM2 (для генерации STEP-импульсов)
*   `TIM2` сконфигурирован как 32-битный таймер общего назначения.
*   Режим: Output Compare No Output на Channel 1.
*   **Прерывания:** `TIM2 global interrupt` включены в `NVIC Settings`.

### 2.4. Настройка FreeRTOS

#### 2.4.1. Выбор CMSIS_V2
FreeRTOS включен и настроен с использованием интерфейса CMSIS_V2.

#### 2.4.2. Корректировка размера кучи (Heap Size)
*   Изначально `configTOTAL_HEAP_SIZE` был 3072 байта.
*   **Проблема:** Возникла ошибка "RAM overflow" при компоновке.
*   **Решение:** Увеличен до `4096` байт.

#### 2.4.3. Устранение ошибки "RAM overflow"
Ошибка была решена путем уменьшения стеков задач (до 128-256 слов) и корректировки `configTOTAL_HEAP_SIZE`.

### 2.5. Управление версиями (Git и GitHub)

*   **Инициализация:** Локальный Git-репозиторий инициализирован в корне проекта.
*   **`.gitignore`:** Настроен для игнорирования файлов сборки STM32CubeIDE.
*   **GitHub:** Проект успешно загружен на удаленный репозиторий GitHub с использованием Personal Access Token.

---

## 3. Этап 2: Проектирование и реализация программной архитектуры

### 3.1. Определение и уточнение архитектуры задач FreeRTOS

В ходе обсуждения была сформирована следующая архитектура для обеспечения модульности и эффективного использования ресурсов.

#### 3.1.1. Финальная архитектура FreeRTOS (4 задачи, 5 очередей)
*   **Задачи (4 шт.):**
    1.  `Task_CAN_Handler`: Обработка входящих/исходящих CAN-сообщений.
    2.  `Task_Command_Parser`: Парсинг и диспетчеризация CAN-команд.
    3.  `Task_Motion_Controller`: Генерация STEP/DIR сигналов, управление движением.
    4.  `Task_TMC_Manager`: Конфигурация и мониторинг драйверов TMC2209.
*   **Очереди (5 шт.):** Для асинхронной и потокобезопасной связи между задачами и прерываниями.
    1.  `can_rx_queueHandle` (ISR -> `Task_CAN_Handler`)
    2.  `can_tx_queueHandle` (Различные задачи -> `Task_CAN_Handler`)
    3.  `parser_queueHandle` (`Task_CAN_Handler` -> `Task_Command_Parser`)
    4.  `motion_queueHandle` (`Task_Command_Parser` -> `Task_Motion_Controller`)
    5.  `tmc_manager_queueHandle` (`Task_Command_Parser` -> `Task_TMC_Manager`)

### 3.2. Проектирование CAN-адресации для мульти-устройств

#### 3.2.1. Иерархическая схема CAN ID
Для поддержки до 16 "исполнителей", каждый с 8 моторами, используется иерархическая схема кодирования ID в 11-битном `StdId` CAN-фрейма:
`StdId = 0x100 | (ID_Исполнителя << 3) | ID_Мотора`
*   `ID_Исполнителя`: 4 бита (0-15).
*   `ID_Мотора`: 3 бита (0-7).
*   `0x100`: Базовый адрес для команд.

#### 3.2.2. Механизм определения `Performer_ID`
*   `Performer_ID` хранится в Flash-памяти микроконтроллера.
*   Механизм первоначального "провизионинга" (установки ID) будет реализован позже (например, через широковещательную CAN-команду, использующую уникальный серийный номер STM32).
*   На данном этапе `g_performer_id` инициализируется в `0xFF` и для теста принимается как `0`.

### 3.3. **Концепция работы с глобальными переменными (Золотое Правило)**

Для обеспечения чистоты кода, предотвращения ошибок "multiple definition" и повышения безопасности, принята следующая концепция:

#### 3.3.1. Категории глобальных переменных
1.  **Хэндлы периферии HAL** (`hcan`, `htim2`, `huartX`): Генерируются CubeMX в `main.c`, объявляются `extern` в `main.h`. Доступ через `#include "main.h"`.
2.  **Хэндлы объектов FreeRTOS** (`..._queueHandle`): Определяются в `main.c`, объявляются `extern` в `App/inc/app_queues.h`. Доступ через `#include "app_queues.h"`.
3.  **Прочие глобальные переменные приложения** (`g_performer_id`, `motor_states`, `tmc_drivers`, `g_active_motor_id`): Определяются в `App/src/app_globals.c`, объявляются `extern` в `App/inc/app_globals.h`. Доступ через `#include "app_globals.h"`.

#### 3.3.2. Файлы для определений и объявлений `extern`
*   **`App/inc/app_globals.h`**: Содержит `extern` объявления для `g_performer_id`, `g_active_motor_id`, `motor_states`, `tmc_drivers`.
*   **`App/src/app_globals.c`**: Содержит определения для `g_performer_id`, `g_active_motor_id`, `motor_states`, `tmc_drivers`.

### 3.4. Создание файловой структуры приложения и "скелетов" модулей
*   Созданы папки `App/inc` (для заголовочных файлов) и `App/src` (для файлов исходного кода), с подпапками `tasks`.
*   Настройка `Source Location` и `Include Paths` в IDE выполнена для корректной сборки проекта.

### 3.5. Реализация вспомогательных модулей

#### 3.5.1. `app_config.h` (Константы и структуры конфигурации)
Централизованный файл для констант приложения и FreeRTOS очередей.
```c
#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include "main.h" // Для CAN_RxHeaderTypeDef, CAN_TxHeaderTypeDef
#include "cmsis_os.h" // Для FreeRTOS типов

// =============================================================================
//                             ОБЩИЕ НАСТРОЙКИ ПРИЛОЖЕНИЯ
// =============================================================================

#define MOTOR_COUNT                 8 // Общее количество моторов в системе
#define CAN_DATA_MAX_LEN            8 // Максимальная длина поля данных CAN-фрейма (в байтах)

// =============================================================================
//                             НАСТРОЙКИ ОЧЕРЕДЕЙ FREERTOS
// =============================================================================

// -- ДЛИНА ОЧЕРЕДЕЙ (количество элементов) --
#define CAN_RX_QUEUE_LEN            10
#define CAN_TX_QUEUE_LEN            10
#define PARSER_QUEUE_LEN            10
#define MOTION_QUEUE_LEN            5
#define TMC_MANAGER_QUEUE_LEN       5

// -- СТРУКТУРЫ ЭЛЕМЕНТОВ ОЧЕРЕДЕЙ --
// Структура для хранения полного входящего CAN-фрейма (header + data)
typedef struct {
    CAN_RxHeaderTypeDef header;
    uint8_t data[CAN_DATA_MAX_LEN];
} CanRxFrame_t;

// Структура для хранения полного исходящего CAN-фрейма (header + data)
typedef struct {
    CAN_TxHeaderTypeDef header;
    uint8_t data[CAN_DATA_MAX_LEN];
} CanTxFrame_t;

// Структура задания для Task_Motion_Controller
typedef struct {
    uint8_t motor_id;
    uint8_t direction; // Будет перезаписано MotionPlanner
    uint32_t steps;    // Количество шагов для относительного движения (или целевая позиция для абсолютного)
    uint32_t speed_steps_per_sec; // Желаемая скорость
    uint32_t acceleration_steps_per_sec2; // Желаемое ускорение
} MotionCommand_t;

#endif // APP_CONFIG_H
```

#### 3.5.2. `app_queues.h` (Объявления хэндлов очередей FreeRTOS)
```c
#ifndef APP_QUEUES_H
#define APP_QUEUES_H

#include "cmsis_os.h" // Для osMessageQueueId_t

extern osMessageQueueId_t can_rx_queueHandle;      // Для приема сырых CAN-фреймов (ISR -> CAN Handler)
extern osMessageQueueId_t can_tx_queueHandle;      // Для отправки CAN-сообщений (любая задача -> CAN Handler)
extern osMessageQueueId_t parser_queueHandle;      // Для передачи CAN-фреймов (CAN Handler -> Command Parser)
extern osMessageQueueId_t motion_queueHandle;      // Для передачи заданий движения (Command Parser -> Motion Controller)
extern osMessageQueueId_t tmc_manager_queueHandle; // Для передачи команд TMC (Command Parser -> TMC Manager)

#endif // APP_QUEUES_H
```

#### 3.5.3. `app_globals.h` и `app_globals.c` (Глобальные переменные приложения)
`app_globals.h`:
```c
#ifndef APP_GLOBALS_H
#define APP_GLOBALS_H

#include <stdint.h>
#include "motion_planner.h" // Для MotorMotionState_t
#include "tmc2209_driver.h" // Для TMC2209_Handle_t
#include "app_config.h"     // Для MOTOR_COUNT

extern uint8_t g_performer_id;
extern volatile int8_t g_active_motor_id;
extern MotorMotionState_t motor_states[MOTOR_COUNT];
extern TMC2209_Handle_t tmc_drivers[MOTOR_COUNT];

#endif // APP_GLOBALS_H
```
`app_globals.c`:
```c
#include "app_globals.h"

uint8_t g_performer_id = 0xFF;
volatile int8_t g_active_motor_id = -1;
MotorMotionState_t motor_states[MOTOR_COUNT];
TMC2209_Handle_t tmc_drivers[MOTOR_COUNT];
```

#### 3.5.4. `command_protocol.h` (Определение команд CAN)
```c
#ifndef COMMAND_PROTOCOL_H
#define COMMAND_PROTOCOL_H

#include <stdint.h>

typedef enum {
    CMD_MOVE_ABSOLUTE       = 0x01,
    CMD_MOVE_RELATIVE       = 0x02,
    CMD_SET_SPEED           = 0x03,
    CMD_SET_ACCELERATION    = 0x04,
    CMD_STOP                = 0x05,
    CMD_GET_STATUS          = 0x06,
    CMD_SET_CURRENT         = 0x07,
    CMD_ENABLE_MOTOR        = 0x08,
    CMD_PERFORMER_ID_SET    = 0x09,
} CommandID_t;

typedef struct {
    uint8_t     motor_id;
    CommandID_t command_id;
    int32_t     payload;
} CAN_Command_t;

#endif // COMMAND_PROTOCOL_H
```

#### 3.5.5. `tmc2209_driver.h/.c` (Низкоуровневый драйвер TMC2209)
`tmc2209_driver.h`:
```c
#ifndef TMC2209_DRIVER_H
#define TMC2209_DRIVER_H

#include <stdint.h>
#include "main.h" // Для HAL_UART_HandleTypeDef

#define TMC2209_UART_SYNC_BYTE      0x05
#define TMC2209_UART_SLAVE_ADDRESS  0x00
#define TMC2209_UART_READ         0x00
#define TMC2209_UART_WRITE        0x80

typedef enum {
    TMC2209_GCONF       = 0x00,
    TMC2209_GSTAT       = 0x01,
    TMC2209_IOIN        = 0x04,
    TMC2209_PWMCONF     = 0x30,
    TMC2209_CHOPCONF    = 0x6C,
    TMC2209_DRVSTATUS   = 0x6F,
    TMC2209_VACTUAL     = 0x22,
    TMC2209_MSCNT       = 0x6A,
    TMC2209_MSCURACT    = 0x6B,
    TMC2209_SGTHRS      = 0x40,
    TMC2209_COOLCONF    = 0x42,
    TMC2209_TPOWERDOWN  = 0x31,
    TMC2209_IHOLD_IRUN  = 0x10,
} TMC2209_Register_t;

typedef struct {
    UART_HandleTypeDef* huart;
    uint8_t             slave_addr;
} TMC2209_Handle_t;

HAL_StatusTypeDef TMC2209_Init(TMC2209_Handle_t* htmc, UART_HandleTypeDef* huart, uint8_t slave_addr);
HAL_StatusTypeDef TMC2209_WriteRegister(TMC2209_Handle_t* htmc, TMC2209_Register_t reg, uint32_t value);
HAL_StatusTypeDef TMC2209_ReadRegister(TMC2209_Handle_t* htmc, TMC2209_Register_t reg, uint32_t* value);
HAL_StatusTypeDef TMC2209_SetMotorCurrent(TMC2209_Handle_t* htmc, uint8_t run_current_percent, uint8_t hold_current_percent);
HAL_StatusTypeDef TMC2209_SetMicrosteps(TMC2209_Handle_t* htmc, uint16_t microsteps);
HAL_StatusTypeDef TMC2209_SetSpreadCycle(TMC2209_Handle_t* htmc, uint8_t enable);

#endif // TMC2209_DRIVER_H
```
`tmc2209_driver.c`:
```c
#include "tmc2209_driver.h"
#include <string.h>

static const uint8_t tmc_crc_table[256] = {
    0x00, 0x07, 0x0E, 0x09, 0x1C, 0x1B, 0x12, 0x15, 0x38, 0x3F, 0x36, 0x31, 0x24, 0x23, 0x2A, 0x2D,
    0x70, 0x77, 0x7E, 0x79, 0x6C, 0x6B, 0x62, 0x65, 0x48, 0x4F, 0x46, 0x41, 0x54, 0x53, 0x5A, 0x5D,
    0xE0, 0xE7, 0xEE, 0xE9, 0xFC, 0xFB, 0xF2, 0xF5, 0xD8, 0xDF, 0xD6, 0xD1, 0xC4, 0xC3, 0xCA, 0xCD,
    0x90, 0x97, 0x9E, 0x99, 0x8C, 0x8B, 0x82, 0x85, 0xA8, 0xAF, 0xA6, 0xA1, 0xB4, 0xB3, 0xBA, 0xBD,
    0xC7, 0xC0, 0xC9, 0xCE, 0xDB, 0xDC, 0xD5, 0xD2, 0xFF, 0xF8, 0xF1, 0xF6, 0xE3, 0xE4, 0xED, 0xEA,
    0xB7, 0xB0, 0xB9, 0xBE, 0xAB, 0xAC, 0xA5, 0xA2, 0x8F, 0x88, 0x81, 0x86, 0x93, 0x94, 0x9D, 0x9A,
    0x27, 0x20, 0x29, 0x2E, 0x3B, 0x3C, 0x35, 0x32, 0x1F, 0x18, 0x11, 0x16, 0x03, 0x04, 0x0D, 0x0A,
    0x57, 0x50, 0x59, 0x5E, 0x4B, 0x4C, 0x45, 0x42, 0x6F, 0x68, 0x61, 0x66, 0x73, 0x74, 0x7D, 0x7A,
    0x89, 0x8E, 0x87, 0x80, 0x95, 0x92, 0x9B, 0x9C, 0xB1, 0xB6, 0xBF, 0xB8, 0xAD, 0xAA, 0xA3, 0xA4,
    0xF9, 0xFE, 0xF7, 0xF0, 0xE5, 0xE2, 0xEB, 0xEC, 0xC1, 0xC6, 0xCF, 0xC8, 0xDD, 0xDA, 0xD3, 0xD4,
    0x69, 0x6E, 0x67, 0x60, 0x75, 0x72, 0x7B, 0x7C, 0x51, 0x56, 0x5F, 0x58, 0x4D, 0x4A, 0x43, 0x44,
    0x19, 0x1E, 0x17, 0x10, 0x05, 0x02, 0x0B, 0x0C, 0x21, 0x26, 0x2F, 0x28, 0x3D, 0x3A, 0x33, 0x34,
    0x4E, 0x49, 0x40, 0x47, 0x52, 0x55, 0x5C, 0x5B, 0x76, 0x71, 0x78, 0x7F, 0x6A, 0x6D, 0x64, 0x63,
    0x3E, 0x39, 0x30, 0x37, 0x22, 0x25, 0x2C, 0x2B, 0x06, 0x01, 0x08, 0x0F, 0x1A, 0x1D, 0x14, 0x13,
    0xAE, 0xA9, 0xA0, 0xA7, 0xB2, 0xB5, 0xBC, 0xBB, 0x96, 0x91, 0x98, 0x9F, 0x8A, 0x8D, 0x84, 0x83,
    0xDE, 0xD9, 0xD0, 0xD7, 0xC2, 0xC5, 0xCC, 0xCB, 0xE6, 0xE1, 0xE8, 0xEF, 0xFA, 0xFD, 0xF4, 0xF3
};

static uint8_t tmc_crc8(uint8_t *data, size_t length) {
    uint8_t crc = 0;
    for (size_t i = 0; i < length; i++) {
        crc = tmc_crc_table[crc ^ data[i]];
    }
    return crc;
}

HAL_StatusTypeDef TMC2209_Init(TMC2209_Handle_t* htmc, UART_HandleTypeDef* huart, uint8_t slave_addr) {
    if (htmc == NULL || huart == NULL || slave_addr > 3) {
        return HAL_ERROR;
    }
    htmc->huart = huart;
    htmc->slave_addr = slave_addr;
    return HAL_OK;
}

HAL_StatusTypeDef TMC2209_WriteRegister(TMC2209_Handle_t* htmc, TMC2209_Register_t reg, uint32_t value) {
    uint8_t tx_buf[8];
    tx_buf[0] = TMC2209_UART_SYNC_BYTE;
    tx_buf[1] = htmc->slave_addr;
    tx_buf[2] = TMC2209_UART_WRITE | reg;
    tx_buf[3] = (uint8_t)(value >> 24);
    tx_buf[4] = (uint8_t)(value >> 16);
    tx_buf[5] = (uint8_t)(value >> 8);
    tx_buf[6] = (uint8_t)(value);
    tx_buf[7] = tmc_crc8(tx_buf, 7);

    return HAL_UART_Transmit(htmc->huart, tx_buf, sizeof(tx_buf), HAL_MAX_DELAY);
}

HAL_StatusTypeDef TMC2209_ReadRegister(TMC2209_Handle_t* htmc, TMC2209_Register_t reg, uint32_t* value) {
    uint8_t tx_buf[4];
    uint8_t rx_buf[8];

    tx_buf[0] = TMC2209_UART_SYNC_BYTE;
    tx_buf[1] = htmc->slave_addr;
    tx_buf[2] = TMC2209_UART_READ | reg;
    tx_buf[3] = tmc_crc8(tx_buf, 3);

    if (HAL_UART_Transmit(htmc->huart, tx_buf, sizeof(tx_buf), HAL_MAX_DELAY) != HAL_OK) {
        return HAL_ERROR;
    }

    if (HAL_UART_Receive(htmc->huart, rx_buf, sizeof(rx_buf), 100) != HAL_OK) {
        return HAL_ERROR;
    }

    if (rx_buf[0] != TMC2209_UART_SYNC_BYTE ||
        rx_buf[1] != htmc->slave_addr ||
        (rx_buf[2] & 0x7F) != reg ||
        tmc_crc8(rx_buf, 7) != rx_buf[7]) {
        return HAL_ERROR;
    }

    *value = ((uint32_t)rx_buf[3] << 24) |
             ((uint32_t)rx_buf[4] << 16) |
             ((uint32_t)rx_buf[5] << 8)  |
             ((uint32_t)rx_buf[6]);

    return HAL_OK;
}

HAL_StatusTypeDef TMC2209_SetMotorCurrent(TMC2209_Handle_t* htmc, uint8_t run_current_percent, uint8_t hold_current_percent) {
    if (htmc == NULL || run_current_percent == 0 || run_current_percent > 100 || hold_current_percent > 100) {
        return HAL_ERROR;
    }

    uint8_t ihold = (uint8_t)((float)hold_current_percent / 100.0f * 31.0f);
    uint8_t irun = (uint8_t)((float)run_current_percent / 100.0f * 31.0f);
    uint8_t tpowerdown = 10;

    uint32_t value = ((uint32_t)tpowerdown << 24) | ((uint32_t)ihold << 16) | ((uint32_t)irun << 8) | (0x4 << 0);

    return TMC2209_WriteRegister(htmc, TMC2209_IHOLD_IRUN, value);
}

HAL_StatusTypeDef TMC2209_SetMicrosteps(TMC2209_Handle_t* htmc, uint16_t microsteps) {
    if (htmc == NULL) {
        return HAL_ERROR;
    }

    uint32_t mres_val = 0;
    switch (microsteps) {
        case 256: mres_val = 0; break;
        case 128: mres_val = 1; break;
        case 64:  mres_val = 2; break;
        case 32:  mres_val = 3; break;
        case 16:  mres_val = 4; break;
        case 8:   mres_val = 5; break;
        case 4:   mres_val = 6; break;
        case 2:   mres_val = 7; break;
        case 1:   mres_val = 8; break;
        default: return HAL_ERROR;
    }

    uint32_t chopconf_reg;
    if (TMC2209_ReadRegister(htmc, TMC2209_CHOPCONF, &chopconf_reg) != HAL_OK) {
        chopconf_reg = 0;
    }

    chopconf_reg &= ~((uint32_t)0xF << 24);
    chopconf_reg |= (mres_val << 24);

    return TMC2209_WriteRegister(htmc, TMC2209_CHOPCONF, chopconf_reg);
}

HAL_StatusTypeDef TMC2209_SetSpreadCycle(TMC2209_Handle_t* htmc, uint8_t enable) {
    (void)htmc;
    (void)enable;
    return HAL_ERROR;
}
```

## 5. План Первого Теста и дальнейшие шаги

### 5.1. Необходимое оборудование и подключение
*   Плата "Исполнителя" с микроконтроллером STM32F103 и драйверами TMC2209.
*   Подключенные шаговые двигатели.
*   Источник питания для платы и моторов.
*   CAN-USB адаптер, подключенный к ПК и к шине CAN "Исполнителя".
*   Программа для отправки CAN-сообщений (например, Busmaster, CAN-Hacker GUI).

### 5.2. Пример тестовой CAN-команды
Отправьте одно CAN-сообщение с ПК:
*   **ID:** `0x100` (Адрес для `ID_Исполнителя = 0`, `ID_Мотора = 0`).
*   **DLC:** `5` байт.
*   **Data:** `02 E8 03 00 00`
    *   `02`: `CMD_MOVE_RELATIVE`
    *   `E8 03 00 00`: Payload `1000` (0x03E8) в little-endian формате.
    (Команда: "Мотор 0, сделать 1000 шагов вперед")

### 5.3. Ожидаемый результат
*   Мотор №0 должен провернуться на определенный угол.

### 5.4. Последующие улучшения и доработки
После успешного первого теста, план развития включает:
*   Реализацию "правильной" синхронизации задачи/прерывания через FreeRTOS семафоры.
*   Полную реализацию планировщика движения с трапецеидальным профилем (ускорение/замедление).
*   Расширение логики для одновременного управления несколькими моторами.
*   Реализацию механизма провизионинга `Performer_ID` (запись в Flash и протокол присвоения).
*   Добавление обработки ошибок и отправки ответных CAN-сообщений.

---

## 6. Этап Рефакторинга: Переход на аппаратный ШИМ (20.02.2026)

### 6.1. Выбор стратегии масштабирования таймеров
**Проблема:** Необходимо управлять 8-ю шаговыми двигателями, задавая для каждого независимую скорость. Скорость определяется частотой STEP-импульсов. Ключевое аппаратное ограничение заключается в том, что все каналы одного таймера (например, `TIM1`) разделяют общую базу времени и, следовательно, могут генерировать импульсы только с одной, общей для всех каналов, частотой.

**Рассмотренные варианты:**

1.  **Меньше таймеров, больше каналов (2 таймера по 4 канала):**
    *   **Конфигурация:** `TIM1` (CH1-4) для моторов 0-3; `TIM2` (CH1-4) для моторов 4-7.
    *   **Преимущества:** Сохраняет `TIM3` и `TIM4` свободными для других системных нужд, что является хорошей практикой для будущего расширения функционала.
    *   **Недостатки:** Позволяет иметь только 2 независимые группы скоростей одновременно (одна скорость для всех активных моторов на `TIM1`, вторая — для `TIM2`).

2.  **Больше таймеров, меньше каналов (4 таймера по 2 канала):**
    *   **Конфигурация:** `TIM1` (CH1-2) для моторов 0-1; `TIM2` (CH1-2) для моторов 2-3, и т.д.
    *   **Преимущества:** Увеличивает гибкость, позволяя иметь до 4-х независимо управляемых групп скоростей.
    *   **Недостатки:** Задействует все доступные таймеры общего назначения, не оставляя резерва для других подсистем.

**Решение:**

Принято решение остановиться на **Варианте 1 (2 таймера по 4 канала)**.

**Обоснование:** Изначальная архитектура проекта позволяла двигать только один мотор в один момент времени. Переход к архитектуре, позволяющей одновременно управлять двумя группами моторов с независимыми скоростями, является значительным шагом вперед и полностью удовлетворяет целям рефакторинга по повышению производительности и масштабируемости. Данный подход представляет собой наиболее сбалансированное и прагматичное решение, которое решает текущие проблемы, не создавая избыточной сложности и сохраняя ресурсы для будущего развития.

### 6.2. Конфигурация таймеров TIM1 и TIM2 в STM32CubeMX (20.02.2026)
**Цель:** Настроить TIM1 и TIM2 для генерации 8 независимых ШИМ-сигналов (по 4 канала на каждый таймер) для управления STEP-сигналами шаговых двигателей.

**Настройка TIM1 и TIM2:**
1.  **Активация каналов:** Для каждого из таймеров (`TIM1` и `TIM2`) были активированы все четыре канала (`Channel 1`, `Channel 2`, `Channel 3`, `Channel 4`) в режиме `PWM Generation CHx`.
2.  **Параметры таймеров (вкладка `Parameter Settings`):**
    *   **`Prescaler` (Предделитель): `63`**
        *   **Пояснение:** Тактовая частота APB2 (для `TIM1`) и APB1 (для `TIM2`) установлена на `64 МГц`. Для получения удобной тактовой частоты таймера в `1 МГц` (1 микросекунда на тик) используется предделитель 64. Так как аппаратный регистр предделителя работает по формуле `(значение_регистра + 1)`, то для деления на 64 в регистр записывается `63`.
        *   `Частота таймера = 64 МГц / (63 + 1) = 64 МГц / 64 = 1 МГц`.
    *   **`Counter Period (ARR)` (Период счетчика/Автоперезагрузка): `999`**
        *   **Пояснение:** С частотой таймера `1 МГц` каждый тик равен 1 микросекунде. Установка `Counter Period` в `999` (фактически 1000 тиков) означает, что таймер будет сбрасываться каждые 1000 микросекунд, что соответствует частоте ШИМ `1 кГц`. Это значение будет служить базовым периодом.
        *   `Частота ШИМ = 1 МГц / (999 + 1) = 1 МГц / 1000 = 1 кГц`.
    *   **`Pulse` (Ширина импульса, значение CCR): `500`**
        *   **Пояснение:** Это значение определяет, сколько тиков таймера импульс будет находиться в высоком состоянии. Установка `Pulse` в `500` (половина от `Counter Period`) создает ШИМ-сигнал со скважностью примерно 50%. Это стандартная скважность для STEP-импульсов.

**Важно:** Указанные значения `Counter Period` и `Pulse` являются начальными настройками в CubeMX. В процессе работы программное обеспечение будет динамически изменять эти параметры для каждого канала, чтобы управлять скоростью вращения двигателей.
---

### 6.3. Финальная распиновка управляющих сигналов (20.02.2026)
**Цель:** Зафиксировать окончательное назначение GPIO-пинов для всех управляющих сигналов после настройки в STM32CubeMX, учитывая аппаратные ограничения и требования периферии.

**Полная таблица распиновки:**

| Тип сигнала | Назначение  | Пин    | Примечания                                      |
| :---------- | :---------- | :----- | :---------------------------------------------- |
| **STEP (ШИМ)** | `TIM1_CH1`  | `PA8`  | Выход ШИМ для STEP-сигнала мотора              |
|             | `TIM1_CH2`  | `PA9`  | Выход ШИМ для STEP-сигнала мотора              |
|             | `TIM1_CH3`  | `PA10` | Выход ШИМ для STEP-сигнала мотора              |
|             | `TIM1_CH4`  | `PA11` | Выход ШИМ для STEP-сигнала мотора              |
|             | `TIM2_CH1`  | `PA0`  | Выход ШИМ для STEP-сигнала мотора              |
|             | `TIM2_CH2`  | `PA1`  | Выход ШИМ для STEP-сигнала мотора              |
|             | `TIM2_CH3`  | `PB10` | Выход ШИМ для STEP-сигнала мотора              |
|             | `TIM2_CH4`  | `PB11` | Выход ШИМ для STEP-сигнала мотора              |
| **DIR (GPIO)** | `DIR_1`     | `PB0`  | Управление направлением вращения мотора 1      |
|             | `DIR_2`     | `PB1`  | Управление направлением вращения мотора 2      |
|             | `DIR_3`     | `PB2`  | Управление направлением вращения мотора 3      |
|             | `DIR_4`     | `PB3`  | Управление направлением вращения мотора 4      |
|             | `DIR_5`     | `PB4`  | Управление направлением вращения мотора 5      |
|             | `DIR_6`     | `PB5`  | Управление направлением вращения мотора 6      |
|             | `DIR_7`     | `PB12` | Управление направлением вращения мотора 7      |
|             | `DIR_8`     | `PB13` | Управление направлением вращения мотора 8      |
| **EN (GPIO)** | `EN_1`      | `PA4`  | Включение/отключение драйвера мотора 1         |
|             | `EN_2`      | `PA5`  | Включение/отключение драйвера мотора 2         |
|             | `EN_3`      | `PA6`  | Включение/отключение драйвера мотора 3         |
|             | `EN_4`      | `PA7`  | Включение/отключение драйвера мотора 4         |
|             | `EN_5`      | `PA12` | Включение/отключение драйвера мотора 5         |
|             | `EN_6`      | `PA15` | Включение/отключение драйвера мотора 6         |
|             | `EN_7`      | `PB14` | Включение/отключение драйвера мотора 7         |
|             | `EN_8`      | `PB15` | Включение/отключение драйвера мотора 8         |
| **CAN**     | `CAN_RX`    | `PB8`  | Прием данных по CAN-шине (автоматический ремап) |
|             | `CAN_TX`    | `PB9`  | Передача данных по CAN-шине (автоматический ремап) |
| **UART1**   | `UART1_RX`  | `PB7`  | Прием данных UART1 (для TMC2209)               |
|             | `UART1_TX`  | `PB6`  | Передача данных UART1 (для TMC2209)            |
| **UART2**   | `UART2_RX`  | `PA3`  | Прием данных UART2 (для TMC2209)               |
|             | `UART2_TX`  | `PA2`  | Передача данных UART2 (для TMC2209)            |
---

## 7. Этап: Аудит и подготовка к интеграции с дирижером по CAN (05.03.2026)

### 7.1. Цель аудита
Оценить готовность прошивки исполнителя к физическому соединению и обмену данными с дирижером (Conductor) по CAN-шине. Выявить несовместимости протоколов, архитектурные проблемы и баги, блокирующие интеграцию.

### 7.2. Справочные материалы дирижера
Протокол дирижера определен в файлах `readme/conductor_interface/`:
*   `can_packer.h` — формат CAN ID (29-bit Extended), коды команд, макросы упаковки/распаковки
*   `device_mapping.h` — логические идентификаторы устройств анализатора
*   `executor_simulator.c` — эталонная реализация ответов исполнителя (ACK/DONE)
*   `can_message.h` — структура CAN-сообщения внутри дирижера

### 7.3. Выявленные критические несовместимости

#### 7.3.1. Полное несовпадение формата CAN ID
**Дирижер** использует 29-bit Extended CAN ID:
```
ID[28:26] = Priority (3 бита)
ID[25:24] = MsgType (2 бита): COMMAND=0, ACK=1, NACK=2, DATA_DONE_LOG=3
ID[23:16] = DstAddr (8 бит): MOTOR_BOARD=0x20
ID[15:8]  = SrcAddr (8 бит): CONDUCTOR=0x10
ID[7:0]   = Reserved
```
Макрос: `CAN_BUILD_ID(priority, msg_type, dst_addr, src_addr)`

**Исполнитель** использует 11-bit Standard CAN ID:
```
StdId = 0x100 | (Performer_ID << 4) | Motor_ID
```

Форматы полностью несовместимы. Ни один кадр от дирижера не будет принят корректно.

#### 7.3.2. Несовпадение формата payload команд
**Дирижер** (пример `MOTOR_ROTATE`, DLC=8):
```
data[0..1] = command_code (uint16_t LE), например 0x0101
data[2]    = motor_id (логический, из device_mapping.h)
data[3..6] = steps (int32_t LE)
data[7]    = speed (uint8_t, масштабированная)
```

**Исполнитель** (текущий формат):
```
data[0]    = CommandID_t (1 байт), например 0x02
data[1..4] = payload (int32_t LE)
motor_id извлекается из CAN ID, а не из payload
```

#### 7.3.3. Отсутствие механизма ответов ACK/DONE
Дирижер ожидает от исполнителя:
*   **ACK** — немедленное подтверждение приема команды (MsgType=1)
*   **DONE** — уведомление о завершении операции (MsgType=3, SubType=0x01)

В текущем коде исполнителя ответы практически не реализованы.

#### 7.3.4. Нет маппинга логических device_id в физические motor_id
Дирижер оперирует идентификаторами из `device_mapping.h`: `DEV_DISPENSER_MOTOR_XY=1`, `DEV_REACTION_DISK_MOTOR=10` и т.д. Исполнитель работает с индексами массивов `0-7`. Требуется таблица трансляции.

### 7.4. Выявленные баги в текущем коде

#### 7.4.1. Отсутствие обработчика прерывания CAN RX FIFO0
**Файл:** `Core/Src/stm32f1xx_it.c`
В файле присутствует `CAN1_RX1_IRQHandler` (FIFO1), но callback `HAL_CAN_RxFifo0MsgPendingCallback` работает с FIFO0, и в `task_can_handler.c` активируется `CAN_IT_RX_FIFO0_MSG_PENDING`. Обработчик `CAN1_RX0_IRQHandler` **отсутствует** — CAN-приём, вероятно, не работает.
**Решение:** Проверить настройки NVIC в .ioc-файле, включить `CAN1 RX0 interrupts`.

#### 7.4.2. Чтение неинициализированной переменной в task_command_parser.c
**Файл:** `App/src/tasks/task_command_parser.c`, строка 36
Внутри цикла `for(;;)` отсутствует вызов `osMessageQueueGet()`. Переменная `received_command` используется без заполнения — undefined behavior. Также `osDelay(1)` расположен **за** закрывающей скобкой `for(;;)` — недоступен.

#### 7.4.3. Несоответствие типов в motion_queue
**Файл:** `main.c:166` — очередь создана с `sizeof(MotionCommand_t)`
**Файл:** `task_motion_controller.c:21` — из очереди читается `CAN_Command_t`
Структуры имеют разный размер и разные поля — undefined behavior при чтении из очереди.

#### 7.4.4. TIM1 в режиме One Pulse Mode
**Файл:** `main.c:415` — `HAL_TIM_OnePulse_Init(&htim1, TIM_OPMODE_SINGLE)`
Данный режим останавливает таймер после первого периода, что конфликтует с непрерывной генерацией PWM для STEP-сигналов.

### 7.5. Замечания среднего приоритета

*   **CAN-фильтр** (task_can_handler.c) — принимает все сообщения (mask=0x0000). Необходимо настроить фильтрацию по `CAN_ADDR_MOTOR_BOARD (0x20)` в поле DstAddr.
*   **TMC2209 инициализация** (task_tmc2209_manager.c) — инициализируются только 4 из 8 драйверов.
*   **CAN скорость** — дирижер работает на STM32H723 с FDCAN. Необходимо согласовать битрейт (текущий: 500 кбит/с при APB1=32 МГц).
*   **Изменение PSC/ARR таймера** (motion_driver.c:132) — влияет на все каналы одного таймера одновременно. Это задокументированное ограничение архитектуры "2 таймера по 4 канала".

### 7.6. Архитектурные решения, принятые при обсуждении (05.03.2026)

В ходе детального обсуждения цепочки обработки CAN-сообщений были приняты следующие ключевые решения:

#### 7.6.1. Разделение ответственности: CAN Handler vs Command Parser

**Решение:** Task_CAN_Handler работает только на транспортном уровне, Task_Command_Parser — на прикладном.

**CAN Handler (транспортный уровень):**
*   Приём CAN-фреймов из `can_rx_queue`
*   Проверка формата: Extended ID, DstAddr = `CAN_ADDR_MOTOR_BOARD` или `BROADCAST`
*   Проверка MsgType = `CAN_MSG_TYPE_COMMAND`
*   Извлечение `cmd_code` (2 байта), `device_id` (1 байт), raw data (до 5 байт)
*   Упаковка в промежуточную структуру `ParsedCanCommand_t` → `parser_queue`
*   Отправка фреймов из `can_tx_queue` в CAN-периферию
*   **NACK** только на транспортные ошибки (неверный формат, неизвестная команда на этом уровне)

**Command Parser (прикладной уровень):**
*   Приём `ParsedCanCommand_t` из `parser_queue`
*   Полный парсинг параметров команды (steps, speed, direction из raw data)
*   Трансляция `device_id` → `physical_motor_id` через `DeviceMapping_ToPhysicalId()`
*   Формирование `MotionCommand_t` → `motion_queue`
*   Диспетчеризация команд TMC → `tmc_manager_queue`
*   **NACK** при невалидном `device_id` (`MOTOR_ID_INVALID`)

#### 7.6.2. Механизм обработки TX: osThreadFlags

**Проблема:** Блокирующее ожидание `osWaitForever` на `can_rx_queue` в CAN Handler делало невозможной своевременную отправку TX-фреймов из `can_tx_queue`.

**Решение:** Использовать `osThreadFlags` (события) вместо прямого ожидания на очереди:
*   `FLAG_CAN_RX (0x01)` — устанавливается в `HAL_CAN_RxFifo0MsgPendingCallback` после помещения фрейма в `can_rx_queue`
*   `FLAG_CAN_TX (0x02)` — устанавливается любой задачей после помещения фрейма в `can_tx_queue`
*   CAN Handler ожидает `osThreadFlagsWait(FLAG_CAN_RX | FLAG_CAN_TX, osFlagsWaitAny, osWaitForever)` и обрабатывает соответствующее событие

Это обеспечивает event-driven обработку без поллинга, минимальное потребление CPU и отсутствие взаимной блокировки RX/TX.

#### 7.6.3. ACK из Motion Controller, а не из CAN Handler

**Решение:** ACK отправляется из Task_Motion_Controller, а не из CAN Handler.

**Обоснование:** ACK означает «команда принята к исполнению». Это невозможно гарантировать на транспортном уровне — нужно проверить, что мотор не занят (`g_motor_active`). Если мотор занят, вместо ACK отправляется NACK с кодом `CAN_ERR_MOTOR_BUSY`.

**Логика в Motion Controller:**
1.  Получить `MotionCommand_t` из `motion_queue`
2.  Проверить `motor_id < MOTOR_COUNT`
3.  Проверить `!g_motor_active[motor_id]`
4.  Если занят → NACK (`CAN_ERR_MOTOR_BUSY`)
5.  Если свободен → ACK, затем выполнение команды
6.  По завершении движения → DONE

Для отправки ACK/NACK/DONE из Motion Controller в `MotionCommand_t` добавлены поля `cmd_code` (uint16_t) и `device_id` (uint8_t).

#### 7.6.4. Формат DONE-ответа

**Формат:** DLC=8, MsgType=`CAN_MSG_TYPE_DATA_DONE_LOG`
```
data[0] = CAN_SUB_TYPE_DONE (0x01)
data[1] = cmd_code & 0xFF
data[2] = (cmd_code >> 8) & 0xFF
data[3] = device_id
data[4..7] = 0x00
```

Этого достаточно для идентификации завершённой операции при последовательном выполнении: `(src_addr + cmd_code + device_id)` однозначно определяют, какая именно команда завершена.

#### 7.6.5. Идентификация команд: job_id не нужен для текущей архитектуры

**Решение:** Не вводить job_id для последовательного режима работы.

**Обоснование:** При последовательном выполнении (текущий режим) каждый мотор выполняет не более одной команды одновременно. Комбинация `(src_addr, cmd_code, device_id)` однозначно идентифицирует операцию. Биты [7:0] 29-bit CAN ID зарезервированы — при переходе к параллельному выполнению их можно использовать как sequence number.

#### 7.6.6. Промежуточная структура ParsedCanCommand_t

**Решение:** Ввести `ParsedCanCommand_t` как элемент `parser_queue` вместо `CAN_Command_t`.

```c
typedef struct {
    uint16_t cmd_code;      // CAN-код команды (0x0101, 0x0102, ...)
    uint8_t  device_id;     // Логический ID устройства из payload
    uint8_t  data[5];       // Сырые данные параметров (data[3..7] CAN-фрейма)
    uint8_t  data_len;      // Длина данных параметров
} ParsedCanCommand_t;
```

CAN Handler заполняет эту структуру, Command Parser извлекает из неё конкретные параметры в зависимости от `cmd_code`.

#### 7.6.7. Расширение MotionCommand_t

**Решение:** Добавить в `MotionCommand_t` поля для обратной связи:
*   `uint16_t cmd_code` — CAN-код команды (для ACK/NACK/DONE)
*   `uint8_t device_id` — логический ID устройства (для DONE)

Эти поля прокидываются через всю цепочку: CAN Handler → Parser → Motion Controller, позволяя Motion Controller формировать корректные ответы дирижеру.

#### 7.6.8. Исключение лопатки миксера из маппинга

**Решение:** `DEV_MIXER_PADDLE_MOTOR (22)` исключён из таблицы маппинга.

**Обоснование:** Лопатка миксера — обычный DC-мотор (вращение), а не шаговый. Управление осуществляется с другой платы. На плате шаговых двигателей 7 активных моторов, физический индекс 7 (`TIM2_CH4`, `PB11`) — свободный резерв.

Маппинг: `1→0, 2→1, 3→2, 10→3, 11→4, 20→5, 21→6`

#### 7.6.9. Command Parser остаётся отдельной задачей

**Решение:** Не объединять Command Parser с CAN Handler.

**Обоснование:** Сохранение Parser как отдельной задачи FreeRTOS обеспечивает:
*   Чёткое разделение транспортного и прикладного уровней
*   Изоляцию изменений при модификации протокола
*   Возможность независимой приоритизации задач
*   Буферизацию через `parser_queue` — CAN Handler не блокируется на парсинге

### 7.7. Реализация архитектурных решений (06.03.2026)

Все задачи Фазы C Этапа 5 выполнены. Сборка проходит без ошибок и предупреждений.

#### 7.7.1. Изменённые файлы

| Файл | Изменения |
|:-----|:----------|
| `App/inc/app_config.h` | Добавлены: `ParsedCanCommand_t`, поля `cmd_code`/`device_id` в `MotionCommand_t`, флаги `FLAG_CAN_RX`/`FLAG_CAN_TX` |
| `App/inc/app_queues.h` | Добавлен `extern osThreadId_t task_can_handleHandle` |
| `App/inc/can_protocol.h` | Добавлены прототипы `CAN_SendAck()`, `CAN_SendNackPublic()` |
| `Core/Src/main.c` | `parser_queue` → `sizeof(ParsedCanCommand_t)` |
| `Core/Src/stm32f1xx_it.c` | Добавлен `osThreadFlagsSet(FLAG_CAN_RX)` в CAN RX callback |
| `App/src/tasks/task_can_handler.c` | Полная переработка: транспортный уровень, event-driven через osThreadFlags, публичные ACK/NACK/DONE |
| `App/src/tasks/task_command_parser.c` | Полная переработка: приём `ParsedCanCommand_t`, трансляция device_id, парсинг параметров по CAN-кодам дирижера |
| `App/src/tasks/task_motion_controller.c` | Добавлены ACK (после проверки busy), NACK (`MOTOR_BUSY`), DONE, исправлен расчёт direction |
| `App/src/tasks/task_tmc2209_manager.c` | Раскомментирована инициализация всех 8 драйверов (USART1 + USART2) |

#### 7.7.2. Текущий статус

*   **Сборка:** Успешна, без ошибок и предупреждений
*   **Готовность к тестированию:** Фаза D — подготовка тестового CAN-кадра
*   **Известные ограничения:**
    *   DONE для `CMD_MOVE_RELATIVE`/`CMD_MOVE_ABSOLUTE` пока не отправляется автоматически (нет механизма подсчёта шагов)
    *   `tmc_manager_queue` временно использует `sizeof(CAN_Command_t)` — будет заменено при интеграции TMC-команд от дирижера

---

## 8. Аудит и обоснование перехода к «Золотому эталону» (03.04.2026)

### 8.1. Текущие архитектурные риски
По результатам аудита версии от 06.03.2026 выявлены следующие отклонения от целевой промышленной архитектуры DDS-240:
1.  **Нарушение иерархии ответов:** Задача `MotionController` напрямую взаимодействует с CAN-транспортом для отправки `DONE`. Это затрудняет отладку и делает невозможным централизованный контроль состояния исполнителя.
2.  **Отсутствие сервисного слоя:** Плата является «черным ящиком» для сервисной утилиты — невозможно удаленно узнать версию прошивки, UID чипа или изменить сетевой адрес без перепрошивки.
3.  **Статическая конфигурация:** NodeID и маппинг моторов жестко зашиты в коде, что препятствует масштабируемости системы (нельзя поставить две одинаковые платы в одну сеть без модификации кода).

### 8.2. Техническое обоснование рефакторинга
Для устранения рисков внедряется концепция **Unified Dispatcher** и **Flash-based Config**:
*   **Unified Dispatcher (Диспетчер):** Становится единственной точкой входа и выхода для прикладных сообщений. Реализует паттерн «Транзакция», гарантируя Дирижеру ответ (ACK) в течение 10мс.
*   **Encapsulation (Инкапсуляция):** Уход от использования `extern` глобальных переменных. Состояние системы (например, флаги активности моторов) защищено `osMutex`. Доступ осуществляется только через атомарные API-функции, что исключает Race Conditions.
*   **Virtual Mapping:** Переход от жесткого маппинга к табличному во Flash позволяет абстрагировать логические ID Дирижера (140-159) от физических каналов платы.
*   **Service Infrastructure:** Внедрение команд группы `0xFxxx` обеспечивает соответствие спецификации `SERVICE_TOOL_SPECIFICATION.md`, позволяя выполнять диагностику и настройку платы в полевых условиях.

### 8.3. Ожидаемые показатели
*   **Унификация:** Совместимость кода Диспетчера с другими платами проекта (термодатчики, помпы) — до 80%.
*   **Надежность:** Исключение таймаутов на стороне Дирижера за счет гарантированного ACK.
*   **Безопасность:** Доступ к состоянию моторов теперь возможен только через `MotionController_IsMotorActive()`, что исключает несанкционированное изменение состояния из других потоков.

### 8.4. Результаты стабилизации (03.04.2026)
1.  **Renaming:** Проведена полная миграция с `Command Parser` на `Unified Dispatcher`. Все системные сущности переименованы.
2.  **Lifecycle:** Внедрен промышленный цикл ответа: `ACK` отправляется Диспетчером немедленно (до 1мс), `DONE` — по завершении физической операции.
3.  **Data Isolation:** Модуль `app_globals` полностью удален. Состояние моторов инкапсулировано внутри `task_motion_controller.c`, драйверов — в `task_tmc2209_manager.c`. Доступ к данным осуществляется через потокобезопасные API-функции.
4.  **Protocol Sync:** Файл `can_protocol.h` обновлен, добавлены прототипы для унифицированных ответов.

**Статус:** Проект успешно собирается, архитектурные «хвосты» старой реализации удалены. Проведена подготовка к внедрению энергонезависимой памяти (Flash) и сервисного слоя.

---

## 8. Аудит и обоснование перехода к «Золотому эталону» (06.04.2026)

### 8.1. Выявленные критические несоответствия
В ходе перекрестного аудита с проектом `temp_sensors` выявлены следующие риски:
1.  **Конфликт ресурсов (Hardware):** Использование PWM для 8 моторов на 2 таймерах не обеспечивает независимость скоростей. Это требует перехода на программную генерацию шагов (DDA) в высокоприоритетном прерывании.
2.  **Сетевая изоляция:** Текущий CAN-фильтр блокирует широковещательные команды (Broadcast), что делает невозможным групповой останов или поиск устройств.
3.  **Ненадежность TX:** Отсутствие проверки состояния почтовых ящиков CAN может привести к потере статусных сообщений при высокой плотности трафика.
4.  **Протокольный разрыв:** Формат `CAN_SendData` в моторах не содержит байта Sequence Info (`0x80`), который является обязательным в эталонном проекте датчиков.

### 8.2. Техническая стратегия "Взаимного улучшения"
Принято решение сосредоточиться на **едином стандарте Advanced** для "скелета" прошивок:
*   **Unified CAN Handler:** Поддержка Broadcast + Безопасная отправка (Mailbox check) — **Реализовано**.
*   **Standard Dispatcher:** Использование транзакционного цикла (ACK-DATA-DONE) с общими кодами ошибок — **Реализовано**.
*   **Modular Hardware Layer:** Четкая изоляция логики движения (Planner) от генерации импульсов (Driver).

### 8.3. Перспективные задачи
Реформа подсистемы движения (DDA/TIM3) для обеспечения полной независимости 8 каналов перенесена в **Бэклог (Backlog)**. Данная модернизация будет проведена только при возникновении технологической необходимости в независимых скоростях вращения.

---

## 9. Этап: Интеграция с Дирижером и актуализация документации (08.04.2026)

### 9.1. Цель этапа
Обеспечение полной совместимости "Исполнителя" с "Дирижером" (Conductor) в рамках экосистемы DDS-240. Создание нормативной документации для разработчиков управляющего ПО.

### 9.2. Создание CONDUCTOR_INTEGRATION_GUIDE.md
Разработано и внедрено руководство по интеграции, описывающее:
*   **Сетевые параметры:** Подтвержден NodeID `0x20` и скорость 500 kbps.
*   **Реестр устройств:** Определен диапазон логических ID моторов (140-147), соответствующий физическим осям 0-7.
*   **Спецификация команд:** Детально описаны форматы `ROTATE (0x0101)`, `HOME (0x0102)`, `START_CONTINUOUS (0x0103)` и `STOP (0x0104)`.
*   **Кодирование данных:** Зафиксированы специфические множители для передачи скорости (`speed/4` для вращения и `speed/100` для непрерывного режима).

### 9.3. Синхронизация жизненного цикла транзакций
Приведено к единому стандарту поведение ответов:
1.  **ACK:** Гарантированная отправка Диспетчером в течение 50 мс после получения команды.
2.  **DONE:** Отправка после физического завершения движения (для `ROTATE/HOME`) или немедленно (для сервисных команд и `STOP`).
3.  **NACK:** Унификация кодов ошибок (0x0001 - Unknown, 0x0002 - Invalid ID, 0x0003 - Motor Busy).

### 9.4. Сервисная инфраструктура
Документированы и подтверждены в коде команды обслуживания (`0xF0xx`):
*   `GET_INFO (0xF001)`: Возвращает тип устройства (0x01 - Motion) и количество каналов (8).
*   `REBOOT (0xF002)`: Программный сброс с защитным ключом `0x55AA`.
*   `FLASH_COMMIT (0xF003)`: Сохранение текущих настроек во Flash.
*   `FACTORY_RESET (0xF006)`: Полный сброс настроек с ключом `0xDEAD`.

### 9.5. Унификация экосистемы (Директива 2.0)
Согласно архитектурному решению Дирижера от 08.04.2026, проведена глубокая унификация транспортного и прикладного уровней:
*   **Строгий DLC=8:** Внедрена фильтрация на уровне `Task_CAN_Handler`. Все входящие команды обязаны иметь длину 8 байт для упрощения обработки на bxCAN.
*   **0-based Индексация:** Удален сложный маппинг логических ID (140-147) во Flash. Плата переведена на прямую адресацию каналов `0..7`. Ответственность за маппинг Хост-ID перенесена на Дирижер.
*   **Синхронизация ключей:** Ключи для `REBOOT` (0x55AA) и `FACTORY_RESET` (0xDEAD) приведены к единому стандарту экосистемы DDS-240.

**Статус:** Прошивка "Исполнителя" полностью соответствует Директиве 2.0. Достигнута максимальная совместимость и упрощение протокола взаимодействия. Проект готов к финальному тестированию в составе системы.

---

## 10. Актуализация рефакторинга и подготовка к CAN smoke-test (22.04.2026)

### 10.1. Цель работ
На этапе 22.04.2026 проект приведен к состоянию, пригодному для первичного физического тестирования CAN-уровня без подключенных нагрузок. Основной фокус был на проверке соответствия уже реализованного функционала паттерну DDS-240 и проекту `STM32F103_pumps_valves`, без внедрения дополнительных механизмов диагностики CAN-шины.

### 10.2. Выполненные изменения в транспортном CAN-слое
1.  **Строгий формат Executor CAN:** входящие команды принимаются только как Extended ID, `COMMAND` frame, адресованные текущему `PerformerID` или `CAN_ADDR_BROADCAST`, со строгим `DLC=8`.
2.  **Унификация ответов:** `ACK`, `NACK`, `DATA`, `DONE` отправляются с `DLC=8` и нулевым padding за счет `memset(&tx, 0, sizeof(tx))`.
3.  **Единая постановка TX:** добавлен helper `CAN_QueueTxFrame()`. Он ставит ответ в `can_tx_queueHandle` и будит CAN-задачу только при успешной постановке в очередь.
4.  **Корректный ISR RX path:** `HAL_CAN_RxFifo0MsgPendingCallback()` теперь будит CAN-задачу только если входящий фрейм реально помещен в `can_rx_queueHandle`.
5.  **Mailbox guard:** в TX-цикле CAN handler сохранена ограниченная по времени проверка свободного hardware mailbox перед `HAL_CAN_AddTxMessage()`.

### 10.3. Синхронизация с DDS-240 pattern
1.  **CAN 1 Mbit/s:** `.ioc` и `MX_CAN_Init()` приведены к `Prescaler=2`, `BS1=11TQ`, `BS2=4TQ`. В `.ioc` зафиксировано `CAN.CalculateBaudRate=1000000`.
2.  **TX FIFO priority:** включен `TransmitFifoPriority = ENABLE`.
3.  **Сервисные команды:** в `can_protocol.h` добавлены и используются `CAN_CMD_SRV_FACTORY_RESET` (`0xF006`) и `SRV_MAGIC_FACTORY_RESET` (`0xDEAD`).
4.  **Flash reservation:** linker script переведен на `FLASH LENGTH = 63K`, чтобы последняя страница `0x0800FC00` оставалась зарезервированной под конфигурацию.
5.  **Heap:** `configTOTAL_HEAP_SIZE` увеличен до `8192`, что соответствует текущей задаче стабилизации запуска FreeRTOS объектов.

### 10.4. Границы текущего этапа
На данном этапе сознательно не внедрялись:
1.  `CAN_CMD_SRV_GET_STATUS (0xF007)` и `CAN_STATUS_*` - это статистика CAN-шины и отдельный диагностический этап.
2.  `CAN1_SCE_IRQHandler()` и `HAL_CAN_ErrorCallback()` - будут нужны при внедрении CAN diagnostics.
3.  Watchdog supervisor и fault indication для сервисного инженера.
4.  Механизм автоматического `DONE` по фактическому завершению движения с подсчетом шагов.
5.  Полное тестирование `SET_NODE_ID`, `FLASH_COMMIT`, `REBOOT`, `FACTORY_RESET`.

### 10.5. Сборка и статический контроль
После рефакторинга проект успешно собирается командой:

```bash
make -C Debug all -j4
```

Итоговый размер после последней правки CAN helper и ISR callback:

```text
text=43044, data=256, bss=14448, total=57748
```

Проверка whitespace:

```bash
git diff --check
```

результат: замечаний нет.

### 10.6. Первичный физический CAN smoke-test
Физическое тестирование начато на стенде с CANable/SocketCAN, интерфейс `can0`.

#### F001 GET_DEVICE_INFO
Команда:

```bash
cansend can0 00201000#01F0000000000000
```

Фактический результат:

```text
TX 00201000 [8] 01 F0 00 00 00 00 00 00
RX 05102000 [8] 01 F0 00 00 00 00 00 00
RX 07102000 [8] 02 80 01 01 00 08 0C 22
RX 07102000 [8] 02 80 07 14 52 16 30 30
RX 07102000 [8] 02 80 30 30 30 32 00 00
RX 07102000 [8] 01 01 F0 00 00 00 00 00
```

Оценка: последовательность `ACK + DATA + DATA + DATA + DONE` соответствует ожидаемому формату. Тип устройства `0x01`, версия `1.0`, количество каналов `8`.

#### F004 GET_UID
Команда:

```bash
cansend can0 00201000#04F0000000000000
```

Фактический результат:

```text
TX 00201000 [8] 04 F0 00 00 00 00 00 00
RX 05102000 [8] 04 F0 00 00 00 00 00 00
RX 07102000 [8] 02 80 0C 22 07 14 52 16
RX 07102000 [8] 02 80 30 30 30 30 30 32
RX 07102000 [8] 01 04 F0 00 00 00 00 00
```

Оценка: последовательность `ACK + DATA + DATA + DONE` соответствует ожидаемому формату. UID, полученный по CAN:

```text
0C 22 07 14 52 16 30 30 30 30 30 32
```

### 10.7. Расширенный CAN acceptance без нагрузок

После первичного discovery выполнены негативные и адресные проверки CAN-уровня. Все тесты проводились на том же стенде CANable/SocketCAN при скорости 1 Mbit/s.

#### Unknown command
Команда:

```bash
cansend can0 00201000#FFFF000000000000
```

Фактический результат:

```text
TX 00201000 [8] FF FF 00 00 00 00 00 00
RX 05102000 [8] FF FF 00 00 00 00 00 00
RX 06102000 [8] FF FF 01 00 00 00 00 00
```

Оценка: валидный транспортный кадр принят, затем прикладная логика вернула `NACK 0x0001 Unknown Command`.

#### Invalid motor id
Команда:

```bash
cansend can0 00201000#0101080000000000
```

Фактический результат:

```text
TX 00201000 [8] 01 01 08 00 00 00 00 00
RX 05102000 [8] 01 01 00 00 00 00 00 00
RX 06102000 [8] 01 01 02 00 00 00 00 00
```

Оценка: команда `ROTATE 0x0101` с каналом `8` отклонена как выход за допустимый диапазон `0..7`, код ошибки `0x0002`.

#### Foreign destination
Команда:

```bash
cansend can0 00211000#01F0000000000000
```

Фактический результат:

```text
TX 00211000 [8] 01 F0 00 00 00 00 00 00
```

Оценка: кадр с `DstAddr=0x21` проигнорирован без `ACK/NACK`, что соответствует transport-level policy.

#### Short DLC
Команда:

```bash
cansend can0 00201000#01F0
```

Фактический результат:

```text
TX 00201000 [2] 01 F0
```

Оценка: кадр с `DLC != 8` проигнорирован без `ACK/NACK`, что соответствует strict executor frame policy.

#### Broadcast GET_DEVICE_INFO
Команда:

```bash
cansend can0 00001000#01F0000000000000
```

Фактический результат:

```text
TX 00001000 [8] 01 F0 00 00 00 00 00 00
RX 05102000 [8] 01 F0 00 00 00 00 00 00
RX 07102000 [8] 02 80 01 01 00 08 0C 22
RX 07102000 [8] 02 80 07 14 52 16 30 30
RX 07102000 [8] 02 80 30 30 30 32 00 00
RX 07102000 [8] 01 01 F0 00 00 00 00 00
```

Оценка: broadcast `DstAddr=0x00` для `F001 GET_DEVICE_INFO` принимается, ответ формируется от фактического NodeID `0x20`.

### 10.8. Текущий статус
Базовый CAN Level A и расширенная часть CAN acceptance без нагрузок пройдены: `F001`, `F004`, unknown command, invalid motor id, foreign destination, short DLC и broadcast discovery работают согласно DDS-240 pattern. Проект готов к переходу на доменные команды без подключенных нагрузок.

Тесты `F002`, `F003`, `F005`, `F006` отложены на отдельный этап, чтобы не смешивать smoke-test транспорта с изменением конфигурации и reset-сценариями. На момент этого раздела `F007 GET_STATUS` еще не входил в smoke-test 22.04.2026; фактическая реализация и стендовая проверка закрыты позже в разделе `10.11`.

### 10.9. Доменные команды Motion без нагрузок

После закрытия CAN acceptance выполнены безопасные доменные проверки Motion без подключенных нагрузок. Цель этапа - подтвердить путь `CAN -> Dispatcher -> MotionController -> MotionDriver` до перехода к физическим тестам движения.

#### STOP motor 0
Команда:

```bash
cansend can0 00201000#0401000000000000
```

Фактический результат:

```text
TX 00201000 [8] 04 01 00 00 00 00 00 00
RX 05102000 [8] 04 01 00 00 00 00 00 00
RX 07102000 [8] 01 04 01 00 00 00 00 00
```

Оценка: `STOP` для канала `0` обработан, `DONE` вернулся с `device_id=0`.

#### STOP motor 7
Команда:

```bash
cansend can0 00201000#0401070000000000
```

Фактический результат:

```text
TX 00201000 [8] 04 01 07 00 00 00 00 00
RX 05102000 [8] 04 01 00 00 00 00 00 00
RX 07102000 [8] 01 04 01 07 00 00 00 00
```

Оценка: верхняя граница валидного диапазона `0..7` обрабатывается корректно.

#### HOME motor 0 без движения
Команда:

```bash
cansend can0 00201000#020100C800000000
```

Фактический результат:

```text
TX 00201000 [8] 02 01 00 C8 00 00 00 00
RX 05102000 [8] 02 01 00 00 00 00 00 00
RX 07102000 [8] 01 02 01 00 00 00 00 00
```

Оценка: `HOME` трактуется как движение к абсолютной позиции `0`; при текущей позиции `0` движение не требуется, поэтому `DONE` приходит сразу.

#### START_CONTINUOUS speed=0
Команда:

```bash
cansend can0 00201000#0301000000000000
```

Фактический результат:

```text
TX 00201000 [8] 03 01 00 00 00 00 00 00
RX 05102000 [8] 03 01 00 00 00 00 00 00
RX 07102000 [8] 01 03 01 00 00 00 00 00
```

Оценка: команда доходит до `MotionController`, устанавливает скорость `0` и возвращает `DONE`. Так как мотор не активен, запуск PWM не выполняется.

#### ROTATE + STOP
Команда запуска:

```bash
cansend can0 00201000#0101000100000032
```

Фактический результат запуска:

```text
TX 00201000 [8] 01 01 00 01 00 00 00 32
RX 05102000 [8] 01 01 00 00 00 00 00 00
```

Команда остановки:

```bash
cansend can0 00201000#0401000000000000
```

Фактический результат остановки:

```text
TX 00201000 [8] 04 01 00 00 00 00 00 00
RX 05102000 [8] 04 01 00 00 00 00 00 00
RX 07102000 [8] 01 04 01 00 00 00 00 00
```

Оценка на момент исторического no-load прогона: `ROTATE` переводил мотор `0` в active-state и возвращал только `ACK`; автоматический `DONE` по завершению шагов еще не был реализован. `STOP` снимал active-state и возвращал `DONE`.

#### MOTOR_BUSY
Последовательность:

```bash
cansend can0 00201000#0101000100000032
cansend can0 00201000#0101000100000032
cansend can0 00201000#0401000000000000
```

Фактический результат:

```text
TX 00201000 [8] 01 01 00 01 00 00 00 32
RX 05102000 [8] 01 01 00 00 00 00 00 00
TX 00201000 [8] 01 01 00 01 00 00 00 32
RX 05102000 [8] 01 01 00 00 00 00 00 00
RX 06102000 [8] 01 01 03 00 00 00 00 00
TX 00201000 [8] 04 01 00 00 00 00 00 00
RX 05102000 [8] 04 01 00 00 00 00 00 00
RX 07102000 [8] 01 04 01 00 00 00 00 00
```

Оценка: повторный `ROTATE` на активный канал отклоняется как `NACK 0x0003 Motor Busy`; последующий `STOP` возвращает канал в неактивное состояние.

### 10.10. Статус перед физическим движением

Доменный CAN-путь Motion без нагрузок был подтвержден для старой реализации. На 24.04.2026 код изменен: `ROTATE` получил finite STEP completion и самостоятельный `DONE`; перед физическим движением теперь нужен повторный no-load regression уже без штатного `STOP`, затем измерение STEP/EN.

### 10.11. CAN diagnostics `GET_STATUS` и транспортные счетчики

23.04.2026 добавлен и проверен диагностический сервис `F007 GET_STATUS` с общим набором метрик DDS-240 `0x0001..0x0012`. Реализация включает snapshot API для счетчиков CAN handler, учет transport drops, переполнений очередей, TX mailbox/HAL ошибок, CAN error callback/status counters, `last_hal_error`, `last_esr` и application queue overflow.

Сборка после внедрения прошла:

```bash
PATH=/home/andrey/st/stm32cubeide_1.19.0/plugins/com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.13.3.rel1.linux64_1.0.0.202410170706/tools/bin:$PATH make -C Debug all -j4
```

Размер:

```text
text=43892, data=256, bss=14520, total=58668
```

Команда:

```bash
cansend can0 00201000#07F0000000000000
```

Фактически подтвержденный жизненный цикл: `ACK -> DATA metrics 0x0001..0x0012 -> DONE`. Первый чистый снимок после reset показал `RX_TOTAL=1`, `TX_TOTAL=1`, остальные счетчики `0`. `TX_TOTAL=1` в этом снимке ожидаем: snapshot снимается после отправки ACK текущего `F007`, но до DATA/DONE этой же транзакции.

Проверены transport negative counters:

| Тест | Команда | Подтвержденный результат по следующему `F007` |
|:-----|:--------|:----------------------------------------------|
| Short DLC | `cansend can0 00201000#01F0` | Ответ отсутствует, `DROP_WRONG_DLC (0x0009) = 1`. |
| Foreign destination | `cansend can0 00211000#01F0000000000000` | Ответ отсутствует, `DROP_WRONG_DST (0x0007) = 1`. |
| Wrong message type | `cansend can0 05201000#01F0000000000000` | Ответ отсутствует, `DROP_WRONG_TYPE (0x0008) = 1`. |
| Standard CAN ID | `cansend can0 120#01F0000000000000` | Ответ отсутствует, `DROP_NOT_EXT (0x0006) = 0`, так как кадр отсекается аппаратным CAN-фильтром до firmware. |

Финальный снимок после серии negative tests:

```text
0x0001 RX_TOTAL        = 5
0x0002 TX_TOTAL        = 81
0x0003 RX_QUEUE_OVF    = 0
0x0004 TX_QUEUE_OVF    = 0
0x0005 DISPATCHER_OVF  = 0
0x0006 DROP_NOT_EXT    = 0
0x0007 DROP_WRONG_DST  = 1
0x0008 DROP_WRONG_TYPE = 1
0x0009 DROP_WRONG_DLC  = 1
0x000A..0x0012         = 0
```

Выводы:

1. `F007 GET_STATUS` соответствует DDS-240 pattern: все DATA-метрики приходят до `DONE`, `DONE` завершает транзакцию.
2. `DROP_WRONG_DLC`, `DROP_WRONG_DST` и `DROP_WRONG_TYPE` растут адресно и не смешиваются с прикладными `NACK`.
3. Standard ID на текущей настройке bxCAN отфильтрован аппаратно; это безопасное поведение, но счетчик `DROP_NOT_EXT` физически не проверяется без изменения фильтра.
4. Очереди, mailbox guard, HAL CAN error и CAN fault counters в нормальном стендовом прогоне остались нулевыми.
5. CAN-level диагностика Motion считается закрытой для no-load этапа. Дальнейшие открытые блоки на момент этого прогона: `F002/F003/F005/F006`, safe-state hook, IWDG supervisor и архитектура завершения `ROTATE/HOME` перед физическими нагрузками.

### 10.12. Регрессия после внедрения Motion safe-state hook

23.04.2026 внедрен `MotionDriver_AllSafe()` и подключен к startup, `Error_Handler()` и fault handlers. После этого выполнена повторная регрессия на плате без подключенной нагрузки, только с поднятой CAN-шиной.

Сборка после внедрения safe-state прошла:

```bash
PATH=/home/andrey/st/stm32cubeide_1.19.0/plugins/com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.13.3.rel1.linux64_1.0.0.202410170706/tools/bin:$PATH make -C Debug -j4 all
```

Размер:

```text
text=44180, data=256, bss=14520, total=58956
```

Подтвержденный стендовый прогон:

| Проверка | Команда | Фактический результат |
|:---------|:--------|:----------------------|
| `F001 GET_DEVICE_INFO` | `cansend can0 00201000#01F0000000000000` | `ACK + 3 DATA + DONE` |
| `F004 GET_UID` | `cansend can0 00201000#04F0000000000000` | `ACK + 2 DATA + DONE` |
| `F007 GET_STATUS` | `cansend can0 00201000#07F0000000000000` | `ACK + DATA metrics 0x0001..0x0012 + DONE`; snapshot: `RX_TOTAL=3`, `TX_TOTAL=10`, error/drop/fault counters `0` |
| `STOP motor 0` | `cansend can0 00201000#0401000000000000` | `ACK + DONE`, `device_id=0` |
| `STOP motor 7` | `cansend can0 00201000#0401070000000000` | `ACK + DONE`, `device_id=7` |
| `ROTATE + STOP` | `ROTATE: cansend can0 00201000#010100E803000019`; затем `STOP` | `ROTATE` вернул только `ACK`; `STOP` вернул `ACK + DONE` |
| `MOTOR_BUSY` | два `ROTATE` подряд без `STOP` | первый `ROTATE` вернул `ACK`; второй вернул `ACK + NACK 0x0003 Motor Busy`; последующий `STOP` вернул `ACK + DONE` |

Вывод: внедрение safe-state не нарушило CAN service path, `GET_STATUS`, `STOP`, active-state для `ROTATE` и защиту `MOTOR_BUSY`. Проверка выполнена без механической нагрузки, поэтому она подтверждает регрессию транспорта и логики состояния, но не является физическим тестом движения.

### 10.13. IWDG supervisor integration

23.04.2026 внедрен IWDG supervisor по проверенному Fluidics pattern.

Состав изменения:

- включен HAL IWDG module;
- IWDG настроен как STM32F103 reference profile: `Prescaler=256`, `Reload=624`;
- добавлена отдельная задача `task_watchdog`;
- `HAL_IWDG_Refresh()` вызывается только из `task_watchdog`;
- критические задачи публикуют heartbeat: CAN handler, Dispatcher, MotionController, TMC manager;
- ожидания критических задач переведены с `osWaitForever` на `APP_WATCHDOG_TASK_IDLE_TIMEOUT_MS = 500 ms`;
- при отсутствии прогресса любого клиента supervisor вызывает `MotionDriver_AllSafe()` и прекращает refresh IWDG.

Сборка после внедрения прошла:

```bash
PATH=/home/andrey/st/stm32cubeide_1.19.0/plugins/com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.13.3.rel1.linux64_1.0.0.202410170706/tools/bin:$PATH make -C Debug -j4 all
```

Размер:

```text
text=44824, data=256, bss=14552, total=59632
```

Стендовый idle-тест без нагрузки выполнен 23.04.2026 после прошивки IWDG build. Условия: на плате нет нагрузки, поднята только CAN-шина.

Последовательность проверки:

```bash
cansend can0 00201000#07F0000000000000
# wait ~134 s
cansend can0 00201000#01F0000000000000
cansend can0 00201000#07F0000000000000
```

Первый `F007` после reset:

```text
RX_TOTAL = 3
TX_TOTAL = 26
0x0003..0x0012 = 0
```

После паузы около 134 секунд, `F001` и повторного `F007`:

```text
RX_TOTAL = 5
TX_TOTAL = 51
0x0003..0x0012 = 0
```

Вывод: idle-тест без ложного watchdog reset пройден. Счетчики не обнулились, а монотонно выросли `RX_TOTAL 3 -> 5` и `TX_TOTAL 26 -> 51`, значит за время ожидания reset не происходил. CAN error/drop/fault counters остались нулевыми.

Fault-injection тест выполнен 23.04.2026 на временной прошивке с включенным `APP_WATCHDOG_TEST_STALL_MOTION_AFTER_ROTATE = 1U`. Hook останавливал heartbeat `MotionController` после `ROTATE` motor 0.

Последовательность проверки:

```bash
cansend can0 00201000#01F0000000000000
cansend can0 00201000#07F0000000000000
cansend can0 00201000#010100E803000019
# wait ~20 s
cansend can0 00201000#01F0000000000000
cansend can0 00201000#07F0000000000000
```

До fault-injection:

```text
F001: ACK + DATA + DONE
F007: RX_TOTAL = 2, TX_TOTAL = 6, 0x0003..0x0012 = 0
```

После `ROTATE`:

```text
ROTATE: ACK only, DONE отсутствует
```

После ожидания и повторного `F001/F007`:

```text
F001: ACK + DATA + DONE
F007: RX_TOTAL = 2, TX_TOTAL = 6, 0x0003..0x0012 = 0
```

Вывод: watchdog fault-injection пройден на no-load стенде. Отсутствие `DONE` после `ROTATE`, последующее восстановление связи через `F001` и сброс диагностических счетчиков к значениям свежего запуска подтверждают watchdog reset и recovery. Safe-state перед прекращением refresh подтвержден на уровне software path: supervisor при потере heartbeat вызывает `MotionDriver_AllSafe()` и только затем перестает обновлять IWDG. Физические уровни STEP/EN в этом тесте не измерялись, так как нагрузка не подключена.

После теста fault-injection hook возвращен в выключенное состояние `APP_WATCHDOG_TEST_STALL_MOTION_AFTER_ROTATE = 0U`.

Статус: интеграция, сборка, idle-приемка и no-load fault-injection/recovery закрыты. Открытым остается только физическое подтверждение safe-state уровней при подключении измерительного оборудования или нагрузки.

## 11. Архитектурное решение по `DONE` и насосам

23.04.2026 уточнена общая семантика `DONE` для экосистемы DDS-240.

Принятое правило:

- `ACK` - команда принята в обработку;
- `DONE` - команда завершена по своему контракту;
- `NACK`/`ERROR` - команда не выполнена;
- аварийный safe-state/watchdog recovery не является штатным `DONE`.

Ключевое следствие: смысл `DONE` не должен зависеть от типа устройства. Различаться должны только постусловия команд.

Для Motion:

- `MOTOR_ROTATE` и `MOTOR_HOME` являются finite-командами;
- `DONE` должен отправляться только после фактического завершения движения, остановки STEP/PWM и обновления состояния оси;
- текущая реализация Motion пока не закрывает это требование для `ROTATE/HOME`, поэтому блок остается отдельной архитектурной задачей после safe-state/IWDG.

Для Fluidics:

- дозирование насосом измеряется временем работы насоса;
- целевая recipe-команда - `PUMP_RUN_DURATION(pump_id, duration_ms)`;
- `duration_ms` рассчитывает Дирижер из Host-объема и калибровки;
- Fluidics Executor сам включает насос, выдерживает `duration_ms`, выключает насос и только потом отправляет `DONE`;
- `PUMP_START/PUMP_STOP` остаются сервисными/state-командами, но не должны быть основным способом дозирования через `WAIT_MS` на Дирижере.

### 11.1. Разделение finite и state/continuous команд

23.04.2026 отдельно зафиксировано, что `STOP` не является частью штатного завершения finite-команд. Предыдущие тесты `ROTATE + STOP` отражали только ограничение текущей реализации, а не целевую архитектуру.

Finite-команды имеют внутреннее физическое условие завершения:

- `MOTOR_ROTATE(steps, speed)`: исполнитель получает `steps`, генерирует ровно `abs(steps)` STEP-импульсов, останавливает PWM/STEP, обновляет позицию и отправляет `DONE`;
- `MOTOR_HOME(...)`: исполнитель движется до home-condition, останавливает PWM/STEP, обновляет home/position state и отправляет `DONE`;
- `PUMP_RUN_DURATION(duration_ms)`: исполнитель выдерживает время, выключает насос и отправляет `DONE`.

State/continuous-команды переводят устройство в состояние:

- `MOTOR_START_CONTINUOUS(speed)`: включает continuous PWM и отправляет `DONE` после успешного входа в режим;
- `MOTOR_STOP`: останавливает активное/continuous/прерванное движение и отправляет собственный `DONE`;
- `PUMP_START/PUMP_STOP`: сервисное или ручное управление состоянием, не основной recipe-примитив дозирования.

Следствие для Motion: следующий блок реализации - не "ROTATE + STOP", а самостоятельное завершение `MOTOR_ROTATE`: счетчик оставшихся STEP должен дойти до нуля, после чего исполнитель сам останавливает канал и отправляет `DONE`.

### 11.2. Разделение уровней команд

23.04.2026 дополнительно зафиксировано разделение терминов, чтобы не смешивать Host API, low-level CAN и внутреннюю реализацию прошивки.

Уровни команд:

- Host-level команды: команды верхнего API, например `REACTION_ROTATE`, `SAMPLE_ROTATE`, `REAGENT_ROTATE`, `DISPENSER_MOVE`. Их получает Дирижер от Host и сам оркестрирует выполнение;
- low-level Executor команды: CAN `cmd_code`, которые Дирижер отправляет конкретной плате-исполнителю, например `MOTOR_ROTATE 0x0101`, `MOTOR_HOME 0x0102`, `MOTOR_START_CONTINUOUS 0x0103`, `MOTOR_STOP 0x0104`, сервисные `0xF001..0xF007`;
- внутренние команды прошивки: `CommandID_t` (`CMD_MOVE_RELATIVE`, `CMD_SET_SPEED`, `CMD_STOP` и т.п.), которые существуют только внутри Motion Executor и не являются внешним CAN/Host контрактом;
- физический уровень: TIM/PWM/STEP/DIR/EN, который реализует уже принятую low-level команду.

Правило для дальнейших отчетов: `ACK/DONE/NACK` Motion Executor описывать только относительно внешнего low-level `cmd_code`. Внутренние `CommandID_t` можно упоминать только как implementation detail.

Текущая фактическая матрица Motion Executor по внешним low-level командам:

- `0xF001 GET_DEVICE_INFO`: `ACK -> DATA -> DONE`;
- `0xF002 REBOOT`: `ACK -> DONE -> reset` при верном magic key, иначе `ACK -> NACK`;
- `0xF003 FLASH_COMMIT`: `ACK -> DONE` при успешном commit, иначе `ACK -> NACK`;
- `0xF004 GET_UID`: `ACK -> DATA -> DONE`;
- `0xF005 SET_NODE_ID`: `ACK -> DONE` при валидном NodeID, иначе `ACK -> NACK`;
- `0xF006 FACTORY_RESET`: `ACK -> DONE -> reset` при верном magic key, иначе `ACK -> NACK`;
- `0xF007 GET_STATUS`: `ACK -> DATA metrics -> DONE`;
- `0x0104 MOTOR_STOP`: `ACK -> DONE`;
- `0x0103 MOTOR_START_CONTINUOUS`: `ACK -> enter continuous PWM state -> DONE`; TIM-группа остается занятой до `MOTOR_STOP`;
- `0x0101 MOTOR_ROTATE`: в текущем коде `ACK -> STEP finite completion -> DONE`; `DONE` отправляется после генерации `abs(steps)` STEP, остановки PWM/STEP и обновления позиции;
- `0x0102 MOTOR_HOME`: настоящий `DONE` возможен только после home-condition; текущий путь через движение к позиции `0` не считать закрытой реализацией HOME.

Следствие для Host-level команд: Host не получает эти `DONE` напрямую от Motion Executor. Дирижер переводит Host-команду в одну или несколько low-level команд, ждет их `DONE` от исполнителей и только затем отправляет Host-level `DONE` по своему протоколу.

### 11.3. Корреляция Host `DONE`, Executor `DONE` и локальных completion-событий

24.04.2026 дополнительно уточнено, что слово `DONE` допустимо использовать только для внешних протокольных ответов. Внутренние события прошивки не должны называться `DONE`, чтобы не смешивать уровни принятия решений.

Уровень Host API:

- Host отправляет Дирижеру команду верхнего уровня: например `WASH_STATION_FILL`, `SAMPLE_ROTATE`, `DISPENSER_ASPIRATE`, `REACTION_ROTATE`;
- `Host DONE` отправляет только Дирижер;
- `Host DONE` означает, что весь recipe/job верхнего уровня завершен успешно;
- если один из атомарных шагов не получил подтверждения, завершился `NACK`, timeout или recovery, успешный `Host DONE` отправлять нельзя; сценарий должен завершаться ошибкой по политике Host-протокола.

Уровень Дирижера:

- Дирижер является владельцем recipe/job;
- Дирижер переводит Host-команду в последовательность atomic actions;
- для каждого atomic action Дирижер формирует low-level CAN-команду конкретному Executor;
- `Executor DONE` продвигает только один atomic action, а не весь Host-рецепт;
- `JobManager` имеет право отправить `Host DONE` только после получения успешного завершения всех atomic actions рецепта.

Уровень Executor low-level CAN:

- Executor принимает от Дирижера только low-level `cmd_code`;
- `ACK` означает, что команда принята к обработке;
- `DONE` означает, что именно эта low-level команда достигла своего постусловия;
- `NACK` означает, что low-level команда не будет выполнена или не может быть выполнена;
- `DONE` всегда возвращается с исходным low-level `cmd_code` и `device_id`, чтобы Дирижер мог сопоставить ответ с ожидаемым atomic action.

Уровень внутренних событий прошивки:

- локальные события `steps_remaining == 0`, `timer_expired`, `home_switch_triggered`, `motion_complete_event`, `pump_duration_expired` не являются `DONE`;
- эти события должны называться `COMPLETE`, `EXPIRED`, `STOPPED`, `FAULT`, но не `DONE`;
- внешний CAN `DONE` отправляет доменная задача Executor из task context после проверки состояния, обновления модели ресурса и безопасного завершения физического действия;
- из ISR допустимо только остановить критический выход и поставить внутреннее событие/flag/queue item, но не формировать протокольный `DONE`.

Следствие для Motion:

- `MOTOR_ROTATE` должен завершаться внутренним событием `move_complete`, когда счетчик STEP дошел до нуля;
- `MotionController` после этого обновляет `current_position`, `steps_to_go`, active-state и отправляет low-level `DONE`;
- `STOP` во время active finite move является отдельной командой отмены/остановки и не превращает исходный `MOTOR_ROTATE` в успешный `DONE`;
- `MOTOR_HOME` требует отдельного `home-condition`; без датчика/условия нельзя считать настоящий `HOME DONE` реализованным.

Следствие для Fluidics:

- старая схема `PUMP_START -> WAIT_MS -> PUMP_STOP` означала, что Дирижер сам контролирует длительность работы насоса;
- целевая схема `PUMP_RUN_DURATION(duration_ms)` передает физический временной интервал исполнителю;
- локальное событие `pump_duration_expired` выключает насос и только после этого Fluidics Executor отправляет `DONE` по `0x0201`;
- `PUMP_START/PUMP_STOP` остаются state/service-командами, где `DONE` означает "насос включен" или "насос выключен", но не завершение дозирования объема.

### 11.4. Контракт скорости и ускорения для Motion

24.04.2026 отдельно зафиксирован владелец параметров движения, чтобы не смешивать Host API, recipe-конфигурацию и внутренний Motion Planner.

Host API:

- обычные технологические команды Host не должны требовать от пользователя скорость и ускорение двигателя;
- Host задает предметную цель: слот, кювету, объем, позицию, тип операции;
- скорость и ускорение не являются частью пользовательского контракта верхнего уровня, если отдельная сервисная/инженерная команда явно не предусматривает ручную настройку.

Дирижер:

- Дирижер переводит Host-параметры в физические параметры atomic action;
- для `MOTOR_ROTATE` Дирижер обязан передать `steps` и `speed`;
- `speed` является контрактом recipe-level atomic action: это простое числовое ограничение/целевая скорость, выбранная из recipe/action config или технологического профиля операции;
- текущий проект Дирижера уже содержит `.speed` и `.speed_source` в `ACTION_ROTATE_MOTOR`, а packer упаковывает скорость в `MOTOR_ROTATE`;
- Дирижер не передает `acceleration` в `MOTOR_ROTATE`;
- Дирижер не должен считать `SET_SPEED/SET_ACCELERATION` отдельными Host-рецептными манипуляциями, если целевая команда уже содержит нужный физический параметр.

Motion Executor:

- `MOTOR_ROTATE(steps, speed)` является finite-командой;
- `speed` в payload является requested target/max speed для данной low-level команды;
- Motion валидирует `speed` против локального профиля оси: `min_speed`, `max_safe_speed`, допустимый диапазон таймера/драйвера;
- если `speed` выходит за локальные безопасные пределы, базовая политика - `ACK -> NACK INVALID_PARAM` без запуска движения; молчаливое ограничение скорости допустимо только если оно отдельно описано в контракте и учтено Дирижером при timeout;
- для ненулевого `steps` значение `speed=0` должно считаться invalid-param, иначе команда может перейти в active-state и никогда не завершиться;
- для `steps=0` движение не требуется, допустим быстрый `ACK -> DONE` после валидации команды;
- Motion обязан использовать скорость из текущей low-level команды при запуске finite movement, а не только сохраненное/default `max_speed_steps_per_sec`;
- после завершения finite movement Motion может обновить свое внутреннее состояние, но не должен превращать командную скорость в неявный глобальный permanent config, если это не описано отдельной service/config командой.

Acceleration:

- ускорение сейчас не входит в payload `MOTOR_ROTATE 0x0101`;
- текущее поле `acceleration_steps_per_sec2` является внутренним состоянием Motion Planner и имеет default-значение;
- полноценный профиль разгона/торможения пока не реализован: `MotionPlanner_GetNextFrequency()` возвращает максимальную скорость как заглушку;
- на текущем этапе acceleration считается локальной конфигурацией Motion Executor и реализуется внутри motion planner/driver, а не является частью внешнего low-level CAN-контракта;
- acceleration определяет, как именно исполнитель физически выходит на command `speed` и тормозит перед завершением, но не меняет payload `MOTOR_ROTATE`;
- локальный профиль ускорения должен быть per-axis или per-motor-class, потому что допустимый разгон зависит от механики, массы, тока драйвера, микрошагов и риска срыва шагов;
- расширять CAN payload ради acceleration сейчас нецелесообразно, потому что strict `DLC=8` для `MOTOR_ROTATE` уже занят: `cmd_code + motor_id + int32 steps + uint8 speed`;
- если позже потребуется внешнее управление профилями, предпочтительный путь - не ломать `MOTOR_ROTATE`, а ввести profile-id, отдельную config/service-команду или новую версию команды с явно описанным контрактом.

Статус выявленного gap после правок 24.04.2026:

- `task_dispatcher.c` парсит `speed` из `MOTOR_ROTATE` в `motion_cmd.speed_steps_per_sec`;
- `task_motion_controller.c` теперь применяет command `speed` при запуске finite `MOTOR_ROTATE`;
- `speed=0` при ненулевых `steps` отклоняется как `CAN_ERR_INVALID_PARAM`;
- прежний риск "Дирижер передал speed, но Motion выполнил движение по default/max_speed" снят на уровне кода; требуется no-load regression на плате.

### 11.5. Граница текущего industrial baseline и перспективного DDA/TIM3

24.04.2026 принято решение не привязывать текущую приемку `MOTOR_ROTATE -> DONE` к переходу на DDA/TIM3.

Текущий industrial baseline для Motion:

- сохранить текущий TIM1/TIM2 PWM path для генерации STEP;
- считать аппаратный ресурс двумя независимыми motion-группами:
  - `TIM1 group`: моторы `0..3`, общий `PSC/ARR`, один активный motion profile в момент времени;
  - `TIM2 group`: моторы `4..7`, общий `PSC/ARR`, один активный motion profile в момент времени;
- разрешить параллельную работу максимум двух движений одновременно: одно на `TIM1 group` и одно на `TIM2 group`;
- внутри одной timer group на текущем этапе разрешать только один active motor; конфликтующая команда на другой мотор той же группы должна получать `MOTOR_BUSY`;
- реализовать finite completion для `MOTOR_ROTATE` через счетчик STEP-событий;
- остановить PWM/STEP при достижении `remaining_steps == 0`;
- отправлять low-level `DONE` из `MotionController` после обновления состояния оси;
- применять и валидировать command `speed`;
- не менять внешний контракт `MOTOR_ROTATE(steps, speed)`.

Профиль скорости:

- `speed` приходит от Дирижера отдельно для каждой low-level команды;
- acceleration/deceleration на текущем этапе являются единым локальным профилем Motion Executor для всех моторов;
- состояние профиля выполнения хранится на уровне timer group: active motor, target speed, current speed, remaining steps, phase acceleration/cruise/deceleration;
- одинаковые правила acceleration/deceleration для всех моторов считаются допустимыми для текущего промышленного baseline.

Следствие для Дирижера:

- Дирижер должен учитывать `MOTOR_BUSY` как занятость ресурса motion group, а не только конкретного motor_id;
- если рецепт потенциально запускает два движения параллельно, они допустимы только на разных группах `TIM1` и `TIM2`;
- operation timeout для `MOTOR_ROTATE` рассчитывается по command `steps/speed` с запасом, но реальное время может быть больше из-за локального acceleration/deceleration profile;
- если нужна строгая оценка времени с учетом профиля, Дирижер должен использовать консервативный запас или будущий общий калькулятор времени движения.

DDA/TIM3 переносится в перспективное развитие без блокировки текущего промышленного стандарта. Возвращаться к нему следует только при подтвержденной технологической необходимости: независимые скорости нескольких осей сверх ограничений TIM1/TIM2 PWM, более сложные профили движения, централизованный software step scheduler или расширенная multi-axis coordination.

Следствие: текущая реализация должна быть написана с чистыми границами, чтобы DDA/TIM3 можно было добавить позже за тем же low-level контрактом либо через новую версию команды, если когда-нибудь потребуется расширенный motion profile.

### 11.6. Реализация baseline `MOTOR_ROTATE -> DONE`

24.04.2026 в код Motion Executor внесен baseline finite completion для `MOTOR_ROTATE` на текущем TIM1/TIM2 PWM path.

Реализовано:

- `MotionDriver_StartFinite(motor_id, frequency, steps)` запускает PWM в interrupt mode и хранит счетчик оставшихся STEP;
- `HAL_TIM_PWM_PulseFinishedCallback()` уменьшает счетчик STEP, на нуле останавливает PWM/STEP, выключает EN и выставляет внутренний completion flag;
- `MotionDriver_ConsumeCompletedMotor()` отдает completion в `MotionController`;
- `MotionController` отправляет low-level `DONE` только из task context, после обновления `current_position`, `steps_to_go` и active-state;
- `MOTOR_ROTATE` использует `speed` из payload текущей команды и отклоняет `speed=0` при ненулевых `steps` как `CAN_ERR_INVALID_PARAM`;
- `steps=0` остается быстрым successful completion без запуска PWM;
- `MOTOR_START_CONTINUOUS` при `speed>0` входит в continuous PWM state, удерживает TIM-группу до `MOTOR_STOP` и отправляет `DONE` после включения режима;
- добавлена занятость shared timer resource: `TIM1 group = motors 0..3`, `TIM2 group = motors 4..7`, конфликт на занятой группе возвращает `MOTOR_BUSY`;
- для TIM1 подключен `TIM1_CC_IRQHandler`, TIM2 использует существующий `TIM2_IRQHandler`.

Сборка после правок:

```text
make -C Debug -j4 all
Result: PASS
text=47736 data=256 bss=14640
```

Ограничения текущего шага:

- физическая проверка STEP/EN после completion еще не выполнена;
- `MOTOR_HOME` с настоящим home-condition не закрыт и остается отдельным hardware-блоком;
- полноценный acceleration/deceleration profile еще не реализован, текущий baseline выполняет constant-speed finite move.

### 11.7. Точка остановки на 24.04.2026 и вход на 27.04.2026

Текущая кодовая точка:

- safety, CAN service path, `F007 GET_STATUS`, IWDG supervisor и no-load recovery база уже были закрыты ранее;
- сегодня закрыт кодовый baseline `MOTOR_ROTATE -> DONE`: finite STEP counter, auto-stop PWM/STEP, обновление state и `DONE` из task context;
- дополнительно приведен к контракту `MOTOR_START_CONTINUOUS`: при `speed>0` это state-команда входа в continuous PWM mode, выход через `MOTOR_STOP`;
- сборка после всех правок проходит: `text=47736`, `data=256`, `bss=14640`;
- отчеты и ecosystem/playbook синхронизированы с разделением Host DONE, Executor DONE и внутренних completion-событий.

Что еще не считать проверенным:

- `MOTOR_ROTATE -> DONE` пока подтвержден сборкой, но не стендом;
- не измерено фактическое количество STEP-импульсов и уровень EN после completion;
- не проверен `MOTOR_BUSY` на уровне общей TIM-группы после новой реализации;
- не проверен `START_CONTINUOUS speed>0` после новой реализации;
- настоящий `MOTOR_HOME` не реализован без подтвержденного home-condition;
- acceleration/deceleration остается будущим локальным motion-profile блоком.

Быстрый вход в понедельник, 27.04.2026:

1. Прошить текущую сборку Motion.
2. Поднять CAN и проверить `F001`/`F007`, чтобы убедиться, что плата жива и счетчики стартуют ожидаемо.
3. Выполнить no-load `MOTOR_ROTATE` на малом числе шагов: ожидание `ACK`, затем самостоятельный `DONE` без штатного `STOP`.
4. Проверить negative case: `MOTOR_ROTATE steps != 0, speed=0` должен дать `ACK + NACK(CAN_ERR_INVALID_PARAM)`.
5. Проверить group busy: запустить `ROTATE` на motor `0`, затем конфликтующий `ROTATE` на motor `1`; ожидание `MOTOR_BUSY`. Аналогично отдельно для `TIM2 group` при необходимости.
6. Проверить `START_CONTINUOUS speed>0`: ожидание `ACK + DONE`, затем `MOTOR_STOP -> ACK + DONE`.
7. Только после no-load CAN regression переходить к логическому анализатору/осциллографу: количество STEP равно `abs(steps)`, после completion STEP остановлен, EN в idle.

Документационные правки внесены в:

- `readme/DDS-240_eko_system/DDS-240_ECOSYSTEM_STANDARD.md`;
- `readme/DDS-240_eko_system/CONDUCTOR_INTEGRATION_GUIDE.md`;
- `readme/DDS-240_eko_system/dds240_global_config.h`;
- `readme/DDS-240_eko_system/FLUIDICS_PUMP_RUN_DURATION_MIGRATION.md`;
- `readme/DDS-240_eko_system/EXECUTOR_INDUSTRIALIZATION_PLAYBOOK.md`;
- `readme/DDS-240_eko_system/EXECUTOR_TESTING_GUIDE.md`;
- `readme/DDS-240_eko_system/SERVICE_INFRASTRUCTURE_CONCEPT.md`;
- `readme/Commands_API/CAN_Protocol/2_Frame_Format.md`;
- `readme/Commands_API/CAN_Protocol/3_Application_Layer.md`;
- `readme/Commands_API/CAN_Protocol/4_Examples.md`;
- `readme/Commands_API/CAN_Protocol/5_Low_Level_Commands.md`;
- `readme/Commands_API/CAN_Protocol/6_Parameter_Packing.md`;
- `readme/Commands_API/CAN_Protocol/7_Full_CAN_Frame_Mapping.md`;
- `readme/executor_architecture_guide.md`;
- `readme/refactoring_plan.md`.
