# Руководство по архитектуре исполнителя ДДС-240

## 1. Назначение документа

Данный документ описывает типовую архитектуру прошивки исполнителя (Executor) для биохимического анализатора ДДС-240. Архитектура разработана и верифицирована на плате шаговых двигателей (Motor Board, `CAN_ADDR = 0x20`) и предназначена для применения на всех исполнителях системы:

| Исполнитель | CAN-адрес | Назначение |
|:------------|:----------|:-----------|
| Motor Board | `0x20` | Управление шаговыми двигателями (7 шт.) |
| Pump Board | `0x30` | Управление насосами |
| Thermo Board | `0x40` | Управление термостатированием |

---

## 2. Общая архитектура

### 2.1. Стек технологий

- **MCU:** STM32 (F1/F4/H7 — в зависимости от задач)
- **RTOS:** FreeRTOS, интерфейс CMSIS_V2
- **CAN:** bxCAN или FDCAN, 29-bit Extended ID
- **Протокол:** Совместим с дирижером (`can_packer.h`)

### 2.2. Архитектура задач (4 задачи)

```
┌─────────────┐     ┌──────────────────┐     ┌─────────────────────┐
│  CAN Handler│────→│ Command Parser   │────→│ Domain Controller   │
│ (транспорт) │     │ (прикладной)     │     │ (исполнение)        │
└──────┬──────┘     └──────────────────┘     └─────────────────────┘
       │                                              │
       │◄─────── can_tx_queue (ответы) ◄──────────────┘
       │
┌──────┴──────┐
│  Hardware   │
│  Manager    │
│ (драйверы)  │
└─────────────┘
```

| Задача | Роль | Аналог на Motor Board |
|:-------|:-----|:----------------------|
| CAN Handler | Транспортный уровень CAN | `task_can_handler.c` |
| Command Parser | Прикладной парсинг команд | `task_command_parser.c` |
| Domain Controller | Исполнение команд, управление оборудованием | `task_motion_controller.c` |
| Hardware Manager | Инициализация и настройка аппаратных драйверов | `task_tmc2209_manager.c` |

### 2.3. Очереди (5 шт.)

| Очередь | Направление | Тип элемента | Назначение |
|:--------|:------------|:-------------|:-----------|
| `can_rx_queue` | ISR → CAN Handler | `CanRxFrame_t` | Сырые входящие CAN-фреймы |
| `parser_queue` | CAN Handler → Parser | `ParsedCanCommand_t` | Распакованные команды |
| `domain_queue` | Parser → Domain Controller | Специфичная структура | Команды для исполнения |
| `hw_manager_queue` | Parser → HW Manager | Специфичная структура | Команды конфигурации |
| `can_tx_queue` | Все задачи → CAN Handler | `CanTxFrame_t` | Исходящие CAN-фреймы |

---

## 3. Протокол CAN (29-bit Extended ID)

### 3.1. Формат CAN ID

```
Бит:  [28:26]    [25:24]     [23:16]    [15:8]     [7:0]
Поле: Priority   MsgType     DstAddr    SrcAddr    Reserved
```

**Макрос построения:**
```c
#define CAN_BUILD_ID(priority, msg_type, dst_addr, src_addr) \
    ((uint32_t)(((priority) & 0x07) << 26) | \
               (((msg_type) & 0x03) << 24) | \
               (((dst_addr) & 0xFF) << 16) | \
               (((src_addr) & 0xFF) << 8))
```

### 3.2. Типы сообщений (MsgType)

| Значение | Тип | Направление | Описание |
|:---------|:----|:------------|:---------|
| 0 | COMMAND | Conductor → Executor | Команда от дирижера |
| 1 | ACK | Executor → Conductor | Команда принята к исполнению |
| 2 | NACK | Executor → Conductor | Команда отклонена (с кодом ошибки) |
| 3 | DATA_DONE_LOG | Executor → Conductor | Данные / завершение / лог |

### 3.3. Адреса узлов

| Адрес | Узел |
|:------|:-----|
| `0x00` | Broadcast |
| `0x01` | Host (ПК) |
| `0x10` | Conductor (дирижер) |
| `0x20` | Motor Board |
| `0x30` | Pump Board |
| `0x40` | Thermo Board |

### 3.4. Формат payload команды

```
Байт:  [0:1]        [2]          [3:7]
Поле:  cmd_code(LE) device_id    параметры (зависят от команды)
```

### 3.5. Коды ошибок NACK

| Код | Константа | Описание |
|:----|:----------|:---------|
| `0x0000` | `CAN_ERR_NONE` | Нет ошибки |
| `0x0001` | `CAN_ERR_UNKNOWN_CMD` | Неизвестная команда |
| `0x0002` | `CAN_ERR_INVALID_DEVICE_ID` | Невалидный ID устройства |
| `0x0003` | `CAN_ERR_DEVICE_BUSY` | Устройство занято |

---

## 4. Разделение ответственности задач

### 4.1. CAN Handler — транспортный уровень

**Обязанности:**
- Настройка CAN-фильтра по своему адресу (`DstAddr`)
- Запуск CAN-периферии и активация прерываний
- Event-driven цикл через `osThreadFlagsWait(FLAG_CAN_RX | FLAG_CAN_TX)`
- По `FLAG_CAN_RX`: валидация фрейма (ExtID, DstAddr, MsgType, DLC >= 3), упаковка в `ParsedCanCommand_t` → `parser_queue`
- По `FLAG_CAN_TX`: извлечение из `can_tx_queue`, отправка через `HAL_CAN_AddTxMessage`

**НЕ делает:**
- Не парсит параметры команд
- Не отправляет ACK (кроме транспортных ошибок)
- Не знает о бизнес-логике

**Публичные функции отправки ответов:**
```c
void CAN_SendAck(uint16_t cmd_code);
void CAN_SendNackPublic(uint16_t cmd_code, uint16_t error_code);
void CAN_SendDone(uint16_t cmd_code, uint8_t device_id);
```

Все функции помещают фрейм в `can_tx_queue` и вызывают `osThreadFlagsSet(handle, FLAG_CAN_TX)`.

### 4.2. Command Parser — прикладной уровень

**Обязанности:**
- Приём `ParsedCanCommand_t` из `parser_queue`
- Трансляция логического `device_id` → физический ID через таблицу маппинга
- NACK при невалидном `device_id`
- Парсинг параметров по `cmd_code` (steps, speed, direction и т.д.)
- Формирование доменной структуры команды → `domain_queue`
- Диспетчеризация команд конфигурации → `hw_manager_queue`

**НЕ делает:**
- Не работает с CAN напрямую
- Не выполняет команды
- Не отправляет ACK (кроме NACK при ошибке маппинга)

### 4.3. Domain Controller — исполнение

**Обязанности:**
- Приём команд из `domain_queue`
- Проверка занятости устройства → NACK `DEVICE_BUSY` или ACK
- Выполнение команды (управление оборудованием)
- Отправка DONE по завершении операции

**Логика ответов:**
```
1. Получить команду
2. Валидация ID
3. Проверка занятости:
   - Занят → NACK (DEVICE_BUSY)
   - Свободен → ACK
4. Выполнение команды
5. По завершении → DONE
```

`DONE` всегда означает достижение постусловия команды, а не просто старт действия. Постусловие задается спецификацией команды:

- для `MOTOR_ROTATE`/`MOTOR_HOME` - движение физически завершено, выход STEP остановлен, состояние оси обновлено;
- для `PUMP_RUN_DURATION` - насос отработал заданное время, выключен, операция завершена;
- для `PUMP_START`/`PUMP_STOP`/`VALVE_OPEN`/`VALVE_CLOSE` - ресурс переведен в требуемое состояние;
- для аварийного safe-state/watchdog recovery `DONE` не отправляется автоматически; Дирижер обнаруживает незавершенную команду по timeout и выполняет recovery.

Терминология уровней:

- `Host DONE` отправляет Дирижер после завершения всего recipe/job;
- `Executor DONE` отправляет исполнитель Дирижеру по одной low-level atomic-команде;
- внутренние события прошивки (`timer_expired`, `steps_remaining == 0`, `home_switch_triggered`) не являются `DONE`;
- ISR/callback не формирует CAN `DONE`; он только останавливает критический выход при необходимости и передает событие доменной задаче.

Для Motion-команд `speed` в `MOTOR_ROTATE/HOME/START_CONTINUOUS` выбирает Дирижер как параметр recipe-level atomic action и передает в payload. Исполнитель обязан проверить эту скорость против локального профиля оси до запуска STEP. `acceleration` не является частью текущего low-level payload `MOTOR_ROTATE`; если прошивка хранит `acceleration_steps_per_sec2`, это локальный motion-profile параметр Motion Planner/Driver, а не Host-рецептная манипуляция.

Если несколько каналов исполнителя разделяют один аппаратный timer base, занятость должна проверяться на уровне shared hardware group. Для текущего Motion STM32F103 baseline это две motion-группы: `TIM1 group` для моторов `0..3` и `TIM2 group` для моторов `4..7`. В каждой группе разрешен только один active motion profile; конфликтующие команды возвращают `MOTOR_BUSY`.

### 4.4. Hardware Manager — драйверы

**Обязанности:**
- Инициализация аппаратных драйверов при старте
- Обработка команд конфигурации (ток, режим, калибровка)
- Мониторинг состояния оборудования

---

## 5. Механизм событий (osThreadFlags)

### 5.1. Проблема

Блокирующее ожидание `osWaitForever` на RX-очереди в CAN Handler делает невозможной своевременную отправку TX-фреймов.

### 5.2. Решение

```c
#define FLAG_CAN_RX  0x01
#define FLAG_CAN_TX  0x02

// CAN Handler — основной цикл
for (;;)
{
    uint32_t flags = osThreadFlagsWait(
        FLAG_CAN_RX | FLAG_CAN_TX,
        osFlagsWaitAny,
        osWaitForever
    );

    if (flags & FLAG_CAN_RX) { /* обработка входящих */ }
    if (flags & FLAG_CAN_TX) { /* отправка исходящих */ }
}
```

### 5.3. Источники флагов

| Флаг | Кто устанавливает | Когда |
|:-----|:------------------|:------|
| `FLAG_CAN_RX` | ISR (`HAL_CAN_RxFifo0MsgPendingCallback`) | После помещения фрейма в `can_rx_queue` |
| `FLAG_CAN_TX` | Любая задача (через `CAN_SendAck/Done/Nack`) | После помещения фрейма в `can_tx_queue` |

### 5.4. Требования

В `app_queues.h` необходим `extern` хэндла задачи CAN Handler:
```c
extern osThreadId_t task_can_handleHandle;
```

---

## 6. Промежуточные структуры данных

### 6.1. ParsedCanCommand_t (общая для всех исполнителей)

```c
typedef struct {
    uint16_t cmd_code;      // CAN-код команды (байты 0-1 payload, LE)
    uint8_t  device_id;     // Логический ID устройства (байт 2 payload)
    uint8_t  data[5];       // Сырые данные параметров (байты 3-7 payload)
    uint8_t  data_len;      // Количество байт в data (DLC - 3)
} ParsedCanCommand_t;
```

Эта структура **одинакова** для всех исполнителей — она отражает формат протокола дирижера.

### 6.2. Доменная структура (специфична для каждого исполнителя)

Пример для Motor Board:
```c
typedef struct {
    uint8_t  motor_id;
    uint8_t  command_id;
    uint16_t cmd_code;       // Для ACK/NACK/DONE
    uint8_t  device_id;      // Для DONE
    uint8_t  direction;
    uint32_t steps;
    uint32_t speed_steps_per_sec;          // Из low-level команды или профиля Дирижера
    uint32_t acceleration_steps_per_sec2;  // Локальный профиль Motion, если не задан отдельный config-контракт
} MotionCommand_t;
```

Для Pump Board это будет, например:
```c
typedef struct {
    uint8_t  pump_id;
    uint8_t  command_id;
    uint16_t cmd_code;
    uint8_t  device_id;
    uint32_t volume_ul;      // Объём в микролитрах
    uint16_t flow_rate;      // Скорость потока
} PumpCommand_t;
```

**Обязательные поля** `cmd_code` и `device_id` — присутствуют во всех доменных структурах для формирования ответов дирижеру.

---

## 7. Маппинг устройств (Device Mapping)

### 7.1. Принцип

Дирижер оперирует логическими ID устройств из `device_mapping.h`. Каждый исполнитель транслирует свои ID в локальные физические индексы.

### 7.2. Реализация

```c
// device_mapping.h
#define DEVICE_ID_INVALID  0xFF
uint8_t DeviceMapping_ToPhysicalId(uint8_t device_id);

// device_mapping.c
uint8_t DeviceMapping_ToPhysicalId(uint8_t device_id)
{
    switch (device_id)
    {
        case DEV_XXX:  return 0;
        case DEV_YYY:  return 1;
        // ...
        default:       return DEVICE_ID_INVALID;
    }
}
```

### 7.3. Где вызывается

В **Command Parser**, до формирования доменной команды. При `DEVICE_ID_INVALID` — отправляется NACK.

---

## 8. Формат ответов дирижеру

### 8.1. ACK (команда принята)

```
CAN ID:  CAN_BUILD_ID(PRIORITY_NORMAL, MSG_TYPE_ACK, ADDR_CONDUCTOR, MY_ADDR)
DLC:     2
Data:    [cmd_code_lo] [cmd_code_hi]
```

**Когда отправлять:** Из Domain Controller, после проверки что устройство не занято, перед началом исполнения.

### 8.2. NACK (команда отклонена)

```
CAN ID:  CAN_BUILD_ID(PRIORITY_NORMAL, MSG_TYPE_NACK, ADDR_CONDUCTOR, MY_ADDR)
DLC:     4
Data:    [cmd_code_lo] [cmd_code_hi] [error_code_lo] [error_code_hi]
```

**Когда отправлять:**
- Из Command Parser — при невалидном `device_id`
- Из Domain Controller — при занятости устройства (`DEVICE_BUSY`)

### 8.3. DONE (операция завершена)

```
CAN ID:  CAN_BUILD_ID(PRIORITY_NORMAL, MSG_TYPE_DATA_DONE_LOG, ADDR_CONDUCTOR, MY_ADDR)
DLC:     4
Data:    [SUB_TYPE_DONE(0x01)] [cmd_code_lo] [cmd_code_hi] [device_id]
```

**Когда отправлять:** Из Domain Controller, после завершения операции.

---

## 9. Чеклист для нового исполнителя

### 9.1. Общие модули (копировать и адаптировать)

- [ ] `app_config.h` — константы, `ParsedCanCommand_t`, доменная структура, флаги
- [ ] `app_queues.h` — extern очередей и хэндла CAN Handler задачи
- [ ] `app_globals.h/.c` — глобальные переменные
- [ ] `can_protocol.h` — константы протокола (изменить только `CAN_ADDR_*`)
- [ ] `device_mapping.h/.c` — таблица маппинга своих устройств

### 9.2. Задачи (копировать структуру, менять логику)

- [ ] `task_can_handler.c` — **копировать без изменений**, только `CAN_ADDR` в фильтре
- [ ] `task_command_parser.c` — адаптировать switch по своим `cmd_code`
- [ ] Domain Controller — писать с нуля под свою предметную область
- [ ] Hardware Manager — писать с нуля под свои драйверы

### 9.3. Прерывания и CubeMX

- [ ] CAN RX0 interrupt включен в NVIC
- [ ] `HAL_CAN_RxFifo0MsgPendingCallback` с `osThreadFlagsSet(FLAG_CAN_RX)`
- [ ] `parser_queue` создана с `sizeof(ParsedCanCommand_t)`
- [ ] Хэндл задачи CAN Handler доступен глобально

### 9.4. Верификация

- [ ] Сборка без ошибок и предупреждений
- [ ] Тестовый CAN-кадр: `CAN_BUILD_ID(0, 0, MY_ADDR, 0x10)`, DLC=8
- [ ] Проверить: ACK приходит → команда выполняется → DONE приходит

---

## 10. Известные ограничения текущей версии

1. **Motion DONE для finite-движений** — пока не отправляется автоматически при завершении движения (нужен механизм подсчета шагов в прерывании таймера)
2. **job_id** — не реализован. При последовательном исполнении команд (текущий режим) комбинация `(src_addr, cmd_code, device_id)` достаточна. Биты [7:0] CAN ID зарезервированы для sequence number при переходе к параллельному исполнению
3. **TMC-команды по CAN** — очередь `hw_manager_queue` временно использует `CAN_Command_t`, будет заменена при интеграции
4. **Fluidics v2 migration** — для рецептного дозирования насосом целевой командой является `PUMP_RUN_DURATION`; текущие `PUMP_START/PUMP_STOP` должны остаться как state/service-команды, а не основной способ дозирования через таймер Дирижера.

---

*Документ создан: 06.03.2026*
*Основан на архитектуре Motor Board (STM32F103, CAN_ADDR=0x20)*
