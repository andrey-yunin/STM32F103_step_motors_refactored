# Fluidics / Conductor Migration Note: Pump Finite Commands

**Дата:** 23.04.2026  
**Статус:** плановое изменение после завершения текущего блока Motion.

## 1. Решение

Единый смысл `DONE` для DDS-240:

- `ACK` - команда принята в обработку;
- `DONE` - команда завершена по своему контракту;
- `NACK`/`ERROR` - команда не выполнена;
- аварийный safe-state, watchdog recovery и защитный fault не являются штатным `DONE`.

Для насосов основным рецептным primitive становится:

```text
PUMP_RUN_DURATION(pump_id, duration_ms)
```

Дирижер рассчитывает `duration_ms` из Host-параметров и калибровки. Fluidics Executor сам включает насос, выдерживает время, выключает насос и только после этого отправляет `DONE`.

`PUMP_START` и `PUMP_STOP` остаются для ручного режима, сервиса, диагностики и аварийного управления, но не должны быть основным способом дозирования в рецептах.

## 2. Послание исполнителю насосов

Требуемое изменение логики Fluidics Executor:

1. Реализовать `0x0201 PUMP_RUN_DURATION`.
2. Payload:
   - bytes `0..1`: `cmd_code = 0x0201` Little-Endian;
   - byte `2`: `pump_id` / `ch_idx`;
   - bytes `3..6`: `duration_ms:uint32` Little-Endian;
   - byte `7`: `0x00`.
3. `duration_ms=0` отклонять `NACK INVALID_PARAM` или доменным эквивалентом.
4. При получении команды:
   - проверить индекс насоса;
   - проверить занятость канала;
   - включить насос только после успешного старта локального one-shot timer;
   - пометить канал `BUSY/RUNNING`;
   - по истечении timer выключить насос;
   - снять `BUSY/RUNNING`;
   - отправить `DONE` по `0x0201`.
5. Если timer не создан/не стартовал, насос оставить `OFF`, вернуть `NACK`, `DONE` не отправлять.
6. При fault handler, watchdog recovery или safe-state выключить все насосы/клапаны. `DONE` из fault path не отправлять.
7. `PUMP_START/PUMP_STOP` сохранить как state/service-команды:
   - `PUMP_START DONE` = насос включен, safety timeout активирован;
   - `PUMP_STOP DONE` = насос выключен.

Минимальная приемка Fluidics v2:

- `PUMP_RUN_DURATION 2000 ms`: `ACK`, насос ON около 2000 ms, насос OFF, затем `DONE`;
- `PUMP_RUN_DURATION duration=0`: `ACK + NACK`, насос остается OFF;
- повторная команда на занятый насос: `ACK + NACK DEVICE_BUSY`;
- fault-injection после ON: насос уходит OFF, `DONE` не отправляется, Дирижер видит operation timeout/recovery.

## 3. Послание Дирижеру

Требуемое изменение логики Conductor:

1. Для Host-команд с объемом жидкости рассчитывать время работы насоса:

```text
duration_ms = Fluidics_CalcPumpDurationMs(pump_id, volume_ul, operation_type)
```

2. В рецептах заменить дозирование через:

```text
PUMP_START -> WAIT_MS -> PUMP_STOP
```

на одну атомарную команду:

```text
PUMP_RUN_DURATION(duration_ms)
```

3. `WAIT_MS` оставить только для технологических пауз, стабилизации, выдержки реакции и других пауз, которые не являются локально управляемой физикой исполнительного устройства.
4. Operation timeout Дирижера для `PUMP_RUN_DURATION` должен быть больше `duration_ms` на технологический запас:

```text
operation_timeout_ms = duration_ms + transport_margin_ms + executor_margin_ms
```

5. Host `DONE` отправлять только после завершения всего рецепта. Executor `DONE` по `PUMP_RUN_DURATION` продвигает только один атомарный шаг рецепта.
6. Если `ACK` получен, но `DONE` не пришел в operation timeout, рецепт не продолжается как успешный. Дирижер выполняет recovery/discovery и сверку состояния узла.
7. `PUMP_START/PUMP_STOP` оставить доступными для manual/service flows, но не использовать в основных recipe-командах дозирования.

Минимальная приемка Conductor v2:

- Host `WASH_STATION_FILL(volume_ul, cuvette)` формирует `PUMP_RUN_DURATION` с рассчитанным `duration_ms`;
- Host `WASH_STATION_WASH` не использует `START_PUMP + WAIT_MS + STOP_PUMP` для дозирующих шагов;
- `DONE` от Fluidics по `PUMP_RUN_DURATION` приходит после выключения насоса и только тогда продвигает рецепт;
- Host `DONE` приходит только после завершения всех atomic-шагов recipe.

## 4. Открытые проектные решения

До реализации нужно зафиксировать:

- таблицу калибровки `pump_id -> ul_per_ms` или `ms_per_ul`;
- минимальное и максимальное допустимое `duration_ms`;
- округление объема в миллисекунды;
- отдельные коэффициенты для забора, подачи и слива, если гидравлика отличается;
- поведение при неизвестной калибровке: `NACK`/Host error, а не запуск по default.
