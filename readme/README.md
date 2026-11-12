# DDS-240 shared documentation pointer

This project uses the shared DDS-240 documentation folder from the STM32CubeIDE
workspace instead of keeping a local copy of ecosystem documents.

## Shared documentation

Common documentation root:

```text
/home/andrey/STM32CubeIDE/workspace_1.19.0/DDS-240_readme
```

Ecosystem documentation:

```text
/home/andrey/STM32CubeIDE/workspace_1.19.0/DDS-240_readme/DDS-240_eko_system
```

Step motors executor documentation:

```text
/home/andrey/STM32CubeIDE/workspace_1.19.0/DDS-240_readme/DDS-240_eko_system/Step_motors
```

## Key entry points

Ecosystem standard:

```text
/home/andrey/STM32CubeIDE/workspace_1.19.0/DDS-240_readme/DDS-240_eko_system/DDS-240_ECOSYSTEM_STANDARD.md
```

Next session prompt:

```text
/home/andrey/STM32CubeIDE/workspace_1.19.0/DDS-240_readme/DDS-240_eko_system/NEXT_SESSION_PROMPT.md
```

Step motors executor report:

```text
/home/andrey/STM32CubeIDE/workspace_1.19.0/DDS-240_readme/DDS-240_eko_system/Step_motors/STEP_MOTORS_EXECUTOR_REPORT.md
```

Step motors unification matrix:

```text
/home/andrey/STM32CubeIDE/workspace_1.19.0/DDS-240_readme/DDS-240_eko_system/Step_motors/STEP_MOTORS_UNIFICATION_MATRIX.md
```

Step motors quick entry:

```text
/home/andrey/STM32CubeIDE/workspace_1.19.0/DDS-240_readme/DDS-240_eko_system/Step_motors/NEXT_SESSION_PROMPT.md
```

Step motors executor refactoring plan:

```text
/home/andrey/STM32CubeIDE/workspace_1.19.0/DDS-240_readme/DDS-240_eko_system/Step_motors/STEP_MOTORS_EXECUTOR_REFACTORING_PLAN.md
```

Step motors architecture guide:

```text
/home/andrey/STM32CubeIDE/workspace_1.19.0/DDS-240_readme/DDS-240_eko_system/Step_motors/executor_architecture_guide.md
```

## Motor 0 UART repair — 2026-10-26

User reports the hardware backfeed issue resolved; physical rotation is still
unverified. This working-tree revision is a **motor_id 0 bench configuration**:
USART1 PB6/PB7, UART node address 0 (MS1/MS2 must match), STEP PA8, DIR PB0,
EN PA4. Only motor 0 is initialized over UART and admitted to motion; other
motors remain disabled. STOP remains available for all eight selectors.
This is not acceptance or unification of all eight channels.

UART CRC, reply master address, IOIN/PWMCONF/TPOWERDOWN addresses and current
fields are corrected. RX IRQ is armed before TX, accepting echo or no echo.
Writes are checked using IFCNT, readable configuration is read back; failed
reads never produce zero-filled CHOPCONF writes. GCONF preserves the existing
VREF/Rsense mode. 70/40 percent are register scale settings, not measured amps.
Motor 0 uses 16 microsteps and external STEP (VACTUAL=0). TIM1_CC and USART1
handlers are installed in USER CODE; do not generate duplicate handlers.
GPIO initial EN=HIGH is synchronized in .ioc and main.c. PWM start failures
are propagated and stale timer flags are cleared before each start.

Initialization is attempted once after reset with EN high. Supply logic and
motor power before reset. If startup fails, motion returns the existing common
NACK DEVICE_BUSY (0x0003); this is not a new UART-specific error code. Inspect
`g_tmc0_diag` in CubeIDE: `ready=1`, `stage=17`, `hal_status=0` mean startup
configuration verified. It is a startup snapshot, not live motor telemetry.
`hal_status`: 0 OK, 1 ERROR, 2 BUSY, 3 TIMEOUT.
Stage 2: IOIN read, TMC2209 version and EN check; 3: NODECONF/IFCNT;
4–6: GCONF; 7: current; 8: TPOWERDOWN; 9: VACTUAL;
10–13: CHOPCONF/microstep; 14: IFCNT; 15: GSTAT; 16: DRV_STATUS.
A failed startup needs diagnosis and reset; it does not silently retry writes.

Host regression commands (no hardware):

```sh
gcc -std=c11 -Wall -Wextra -Werror -Itests/stubs -IApp/inc -IApp/inc/tasks -IApp/inc/drivers tests/test_tmc2209.c App/src/drivers/tmc2209_driver.c -o /tmp/test_tmc2209
/tmp/test_tmc2209
gcc -std=c11 -Wall -Wextra -Werror -Itests/motion_stubs -IApp/inc -IApp/inc/drivers tests/test_motion_driver.c App/src/drivers/motion_driver.c -o /tmp/test_motion_driver
/tmp/test_motion_driver
```

Both C regressions PASS. Debug build PASS without reported warnings/errors:
text=50264, data=256, bss=14616; ELF contains strong TIM1_CC and USART1 IRQ
symbols. No flash or real CAN operation performed. Bench acceptance pending:
verify startup diagnostics, continuous 200 STEP/s + STOP, then ROTATE
100 STEP at 200 STEP/s with terminal DONE and physical observation.
The existing CAN1_SCE handler gap and broader MU-00..08 scope remain open.

Reference: [TMC2209 datasheet rev 1.09](https://www.analog.com/media/en/technical-documentation/data-sheets/tmc2209_datasheet_rev1.09.pdf).

## 26.10.2026 — MOTION_TMC: диагностика запуска motor_id 0 через Studio

Реализован согласованный optional engineering contract: F020 объявляет D103,
schema v1, READ_ONLY|INDEXED|EXPERIMENTAL; F021 читает индекс 0. Возвращаются
12 записей с последовательностью 00..0A/8B и DONE selector=0. Схема полей
зафиксирована в dds240_global_config.h. F007 сохраняет прежние 18 метрик.

Это сохранённый RAM-снимок настройки после reset: state, ready, stage,
hal_status, valid_mask, IOIN/GCONF/CHOPCONF/IFCNT/GSTAT/DRV_STATUS.
Запрос не обращается к UART, не переписывает регистры, не снимает блокировку
и не запускает движение. Неуспешно прочитанные регистры помечаются как
невалидные; Studio показывает «нет данных», не трактуя нули RAM как измерения.
Индексы 1..7/ALL не реализованы и получают NACK INVALID_DEVICE_ID.

В Conductor Emulator → Motion Test добавлена кнопка
«Прочитать диагностику TMC / UART»: F020 → проверка D103/schema/flags → F021.
Используются общий транспорт, журнал и service transaction parser; работают
с включённым и выключенным Listen. Потерянные DATA, неверная последовательность,
отсутствие EOT/ACK/DONE, NACK и неподдерживаемая схема не становятся успехом.
В панели видны этап/статус ошибки и валидные регистры; reset инвалидирует снимок.

Проверено офлайн: Debug build PASS (text51024/data256/bss14624), C-тесты UART,
snapshot и production diagnostics handler PASS; кадры C-обработчика приняты
парсером Studio. Полная регрессия Studio: 510 PASS. Это CODE/BUILD/OFFLINE;
новая версия не прошивалась ассистентом, реальный F020/F021 BENCH pending.

Ранее пользователь сообщил об устранении подпитки; вращение пока не подтверждено.
Запуск 0x0103 motor0 в16:43:40 дал ACK→NACK0003. В текущей bench-прошивке
это может означать неподтверждённую UART-настройку; диагноз по stage ещё не получен.
Следующий опыт: прошить новый ELF, перезапустить Studio, выбрать motor_id0,
нажать кнопку диагностики и сохранить фактический результат.


2026-10-27: добавлена передача последнего UART-обмена через CAN D106; см. [UART_TRACE_PROTOCOL.md](UART_TRACE_PROTOCOL.md). Требуются новая прошивка и перезапуск Studio.


## 2026-10-27 — исправление ложного BUSY после STOP

Актуальное поведение заменяет описанное выше ограничение запуска по UART ready.
Удалена проверка TMCManager_IsReady из контроллера движения. Ошибка настройки
TMC по UART сохраняется в D103/D106, но больше не блокирует STEP/DIR/EN и не
преобразуется в CAN_ERR_DEVICE_BUSY. Все физические каналы проходят обычную
проверку занятости оси и группы общего таймера. STOP прекращает STEP, отключает
EN и освобождает ось/таймер; последующий START может запустить PWM при ready=0.
CAN BUSY по-прежнему возможен при занятом моторе/таймере либо переполнении
очереди команд. Успешный START означает запуск PWM, а не подтверждение вращения.
Без успешной UART-настройки ток и микрошаг не считаются подтверждёнными:
движение использует фактические настройки драйвера, которые удалось установить
или которые действуют после его включения. Диагностика не скрывает эту ошибку.

Проверено: host-тест настоящего контроллера (STOP→START при недоступном UART,
повторный START занятой оси, конфликт группы, независимость таймеров,
освобождение после HAL failure, отмена конечного движения). Изменения Studio и
формата CAN не требуются. Для стенда требуется прошить новую Debug-сборку.


## 2026-10-28 — UART-стенд перенесён на третий выход

По запросу пользователя UART-настройка/снимки D103,D106 относятся к motor_id=2
(третий выход STEP PA10 / DIR PB2 / EN PA6). Физический UART остаётся USART1,
адрес TMC остаётся0 при MS1=MS2=0. В manager добавлены отдельные константы
TMC_BENCH_MOTOR_ID=2 и TMC_BENCH_UART_ADDRESS=0, диагностика g_tmc_diag.
D103,D106 принимают index2; index0/1 отклоняются INVALID_DEVICE_ID.
Studio разрешает index2, сохраняя index0 для старых стендовых образов.
Контракт блоков/schema не меняется. STEP/DIR/EN по-прежнему не блокируются
ошибкой UART. UART-настройка однократная после reset; кнопка читает RAM-снимок.

Это смена привязки диагностики, не мультиплексирование физической UART-линии.
При нескольких драйверах с адресом0 на USART1 невозможно выделить третий
выбором motor_id. Настройка предполагает единственный отвечающий драйвер с
адресом0 на этой линии. Количество установленных драйверов запрошено у пользователя.

Проверено: host C UART/configuration tests, production D103/D106 encoder с index2
и отклонением index0/1, STOP/START controller regression; Studio workflow с
запросами2D103/2D106 и защитой от ответа старого index0. На стенде не проверено.


## 2026-10-28 — добавлен второй UART, первый сохранён

USART1 PB6/PB7: D103/D106 index2 (выход3); USART2 PA2/PA3: index6 (выход7).
На каждой отдельной линии UART-адрес0, согласно незадействованным MS1/MS2.
Номер motor_id выбирает диагностируемую линию; не изменяет адрес TMC и не
доказывает соответствие фактической разводки STEP/EN таблице прошивки.

Manager последовательно настраивает обе линии при запуске даже после ошибки
первой. Добавлен USART2 IRQ, priority6, HAL_UART_IRQHandler; код в USER CODE.
Диагностика g_tmc_diag[0]/[1], handle и последняя TX/RX-трасса разделены по
линиям. Ошибка UART не блокирует вращение через STEP/DIR/EN. Чтение D103/D106
по CAN не повторяет UART-запрос и не запускает движение. Схема блоков v1
сохранена. Studio поддерживает index2 и6, index0 сохранён для старых образов;
в строке трассы указан USART1/USART2. Для index6: F021 payload
`21 F0 03 D1 06 00 00 00`, затем `21 F0 06 D1 06 00 00 00`.

Host-тесты проверяют успешную настройку UART2 при молчащем UART1 и обратный
случай, независимость готовности/регистров/трасс, сохранение трассы первой
линии после работы второй, CAN selector2/6, Studio index2/6 workflow.
Debug build PASS. На физической плате ещё не проверено; необходима прошивка
новой сборки и перезапуск Studio. На выходе7 выбирать motor_id6.


## 2026-10-30 — остановка до переделки платы (актуальный checkpoint)

Пользователь остановил работы: планируется переделка платы. Вращение двигателя
НЕ подтверждено. Программный успех START/DONE не означает вращение. Подпитка
устранена по сообщению пользователя. На стенде установлен один драйвер.
Реакция удержания обнаружена при выборе выхода 7 / motor_id=6.

Выполнено в прошивке: исправлены UART CRC/разбор ответа с master address FF,
приём с эхом, проверка записей через IFCNT; добавлены сохранённые диагностические
блоки D103 (настройка) и D106 (TX/RX) через CAN F020/F021. Настройка и трассы
разделены для USART1 (PB6/PB7, index2) и USART2 (PA2/PA3, index6), адрес0.
Ошибка UART больше не превращается в BUSY движения. STOP отключает выбранный
канал и освобождает его таймер. На каждой группе из четырёх каналов допускается
один активный мотор; STOP другого канала не снимает занятость владельца таймера.
Начальные уровни EN высокие, успешный START включает EN низким уровнем.
Studio поддерживает диагностику index2/6 и показывает используемый USART;
index0 оставлен для совместимости со старой прошивкой.

Фактический CAN-журнал выхода7: START/STOP дают ACK/DONE selector6.
D103: state=FAILED, ready=0, stage2 (IOIN), HAL_TIMEOUT, регистры невалидны.
D106: USART2, адрес0, регистр06, TX `05 00 06 6F`, RX те же4 байта;
RX arm/TX HAL_OK, ошибки UART0, timeout21мс. Получено только эхо, корректного
ответа TMC нет. Ранее аналогичная картина была на USART1. Это снимки запуска;
чтение диагностики не инициирует новый UART-обмен. Причина отсутствия ответа
и причина отсутствия вращения аппаратно не локализованы.

Проверка через ST-LINK в HOTPLUG при пользовательском ПУСК→СТОП→ПУСК:
выход7 = TIM2_CH3 / PB10 STEP, PB14 EN, PB12 DIR по таблице прошивки.
При ПУСК: CR1=1, CCER=0x100, PSC=4, ARR=63999, CCR3=32000; CNT меняется,
PB10 в GPIO IDR наблюдался и0, и1; PB14=0. При СТОП: CR1=0, CCER=0,
CNT неподвижен, PB10=0, PB14=1. Повторный ПУСК возобновляет счёт/переключения.
При тактировании таймера64МГц расчётная частота STEP200Гц, заполнение50%
(2.5мс высокий / 2.5мс низкий уровень, скважность2).
Это чтение регистров/состояния вывода МК, не осциллограмма на входе драйвера.
Ассистент не прошивал плату и не запускал мотор во время этой проверки.

Пользователь наблюдает короткие импульсы непосредственно на обмотке.
ШИМ силового моста регулирует ток и не равен STEP200Гц/50%.
Численных измерений осциллограммы нет; тип щупа и схема заземления не уточнены.
До следующих измерений уточнить безопасное дифференциальное подключение:
обычную заземлённую массу осциллографа нельзя подключать к выходу моста.

MS1/MS2 сейчас не выведены: при00 адрес0, пиновый режим1/8 микрошагов.
GCONF.mstep_reg_select=1 выбирает MRES вместо пинов; успешная настройка
прошивки задаёт1/16, но на этом стенде до записи настроек дело не дошло.
I_scale_analog (GCONF bit0) имеет reset1; прошивка сохраняет его значение.
Фактическое значение не прочитано. Изменение напряжения потенциометра не
доказывает значение бита и ток обмотки. Ток зависит от VREF/Rsense и режима;
фактический ток не измерен. Источник: TMC2209 datasheet rev1.09,
https://www.analog.com/media/en/technical-documentation/data-sheets/TMC2209_datasheet_rev1.09.pdf

После переделки: сначала получить схему/распиновку новой ревизии и сверить
STEP/DIR/EN, UART TX/RX/PDN_UART, питание/VCC_IO/GND, VREF/Rsense, пары обмоток.
Предусмотреть разные MS1/MS2 адреса0..3 внутри каждой UART-группы и доступные
контрольные точки. DIP — обсуждавшийся вариант, окончательная схема не утверждена.
Нынешний manager — стендовый: один адрес0 на каждой линии, индексы2/6;
полный опрос/настройку восьми независимо адресуемых драйверов ещё нужно реализовать
после согласования новой схемы. Выбор motor_id не адресует одинаковые UART-адреса.
Затем проверить IOIN/IFCNT и настройку, после этого STEP/EN и реальное вращение.
Не продолжать аппаратные эксперименты до готовности новой платы и команды пользователя.

Ранее выполненные проверки: host UART/configuration, CAN diagnostics encoder,
motion controller и PWM driver PASS; Debug build PASS (text52120/data256/bss14776),
Studio585 tests PASS. Это программная проверка, не аппаратная приёмка.
Открытый исторический пункт CAN1_SCE_IRQHandler остаётся отдельной задачей:
обработчик отсутствует в Core/Src/stm32f1xx_it.c; не считать его исправленным.

## 2026-11-12 — MU-02: структура App приведена к общей схеме

Драйверы перенесены в `App/inc/drivers` и `App/src/drivers`; добавлена
safety-граница `app_safety.h/.c` (делегирует `MotionDriver_AllSafe`, без
латча сброса); `tasks/watchdog.h` переименован в `tasks/task_watchdog.h`;
удалён legacy `command_protocol.h` (внутренний `CommandID_t` перенесён в
`task_motion_controller.h`); удалена orphan-очередь `tmc_manager_queue` и
`TMC_MANAGER_QUEUE_LEN`. Include-пути, тесты и команды в этом README
обновлены на новые пути.

Проверено: clean Debug build PASS, 0 ошибок, 0 предупреждений;
text=52108, data=256, bss=14768. ELF пересобран. HOST-прогон после MU-02
не выполнялся; на плате ничего не прошивалось и не запускалось.
BENCH остаётся N/A до готовности новой платы (MU-00).
Изменения `.cproject` и `Debug/` локальные: оба пути в `.gitignore`.


## 2026-11-12 — MU-03: CAN-транспорт приведён к контракту

- Два аппаратных банка bxCAN: bank0 — broadcast COMMAND, bank1 — прямой COMMAND на runtime NodeID; маска `(0x03FF<<19)|(1<<2)`; `CAN_UpdateDirectFilter` вызывается при F005 до DONE.
- Канонический порядок программной валидации: IDE → RTR → DLC → type → src → dst. RTR и source отбрасываются без счётчиков; останов на первой ошибке без ACK/NACK.
- Добавлен `CAN1_SCE_IRQHandler` (`HAL_CAN_IRQHandler`), поэтому счётчики ошибок CAN в F007 получают обработчик.
- PRIMASK save/restore в снимке диагностики и при consume completion.
- F005: группа `0x20..0x2F`, NACK `0x0006`, RAM-only, обновление bank1 до DONE.
- Clean Debug build: 0 ошибок / 0 предупреждений, text=52304, data=256, bss=14768. HOST не прогонялся, BENCH не выполнялся (стенд после переделки платы).

`CAN_SendDataFragmented` не добавлялся: текущие последовательности DATA корректны, helper не требуется контрактом.


## 2026-11-12 — MU-04: сервисы, конфигурация, Flash

- F001 уже отдавал `CAN_DEVICE_TYPE_MOTION 0x01`; дублирующие
  `CAN_DEVICE_TYPE_MOTOR/THERMO` из `app_config.h` удалены.
- `AppConfig_Init`: проверка `osMutexNew` (NULL → `Error_Handler`),
  проверка группы NodeID `0x20..0x2F` при загрузке; невалидная запись
  заменяется RAM-дефолтами без автоматической записи во Flash.
- `AppConfig_Commit`: Unlock/Erase/Program/Lock входят в итоговый результат.
- `AppConfig_FactoryReset` возвращает bool; ошибка Flash → NACK
  `CAN_ERR_FLASH_WRITE` без сброса. PrepareReset/латч остаются в MU-05.
- Clean Debug build: 0 ошибок / 0 предупреждений, text=52456, data=256,
  bss=14768. HOST не прогонялся, BENCH не выполнялся (стенд после
  переделки платы).

## 2026-11-12 — MU-05: safety, reset, watchdog

- `AppSafety_PrepareReset()` / `AppSafety_IsResetPending()`: латч сброса и safe-state одной операцией под сохранённым PRIMASK; после латча новые активации запрещены до перезапуска MCU.
- F002/F006: ключ → `PrepareReset` → DONE/NACK → ограниченное ожидание слива TX (`CAN_WaitTxIdle(100)`) → сброс. При ошибке F006 — NACK FLASH_WRITE без сброса; латч остаётся.
- Motion controller: при латче отменяет движения, сбрасывает владение TIM-группами и не отправляет поздний DONE; STOP остаётся разрешён.
- `Error_Handler`: IRQ off → `AppSafety_AllSafe()` → DSB/ISB → ожидание сброса. Watchdog при потере прогресса вызывает `Error_Handler`, а не крутит RTOS-цикл.
- Приоритет `task_watchdog` поднят до 56 (выше CAN 40); TMC исключён из критических клиентов до MU-07 (heartbeat = прогресс, не существование задачи).
- Debug clean build: 0 ошибок, 0 предупреждений; text=52952 / data=256 / bss=14768. HOST/BENCH NOT RUN; прошивка не заливалась.

## 2026-11-12 — MU-06: Motion domain

- Валидация скорости выполняется до мутации planner; отклонённая команда не меняет target/direction/steps_to_go.
- `delta == 0` → DONE без запуска STEP.
- HOME отклоняется NACK `INVALID_PARAM`: без процедуры/датчика ложный DONE не выдаётся.
- START_CONTINUOUS со speed=0 → NACK `INVALID_PARAM` (не состояние «вращение»).
- `default` контроллера → NACK `UNKNOWN_CMD`; каждый путь имеет терминальный ответ.
- Зафиксированные ограничения: ускорение не реализовано (постоянная частота); позиция в RAM, после reset относительное движение считается от нуля; ошибка UART не блокирует STEP/DIR/EN (решение 27.10.2026).
- Debug clean build: 0 ошибок, 0 предупреждений; text=52968 / data=256 / bss=14768. HOST/BENCH NOT RUN; прошивка не заливалась.

## 2026-11-12 — Post-MU-06: hardening и HOST

- Reset-латч: отмена движения выполняется один раз (abort-once), а не каждые 10 мс при задержанном сбросе.
- HOST-обвязка: добавлен include `App/inc/drivers` для engineering-diagnostics; в motion-стабы добавлены `__get_PRIMASK`/`__set_PRIMASK`, в тест контроллера — стаб `AppSafety_IsResetPending`.
- HOST 4/4 PASS: check_motion_controller.py, check_engineering_diagnostics.py, test_tmc2209, test_motion_driver.
- Debug clean build: 0 ошибок, 0 предупреждений; text=53000 / data=256 / bss=14768. BENCH NOT RUN; прошивка не заливалась.
