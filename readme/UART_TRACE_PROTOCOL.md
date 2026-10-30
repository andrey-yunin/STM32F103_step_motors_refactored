## 2026-10-27 — UART bytes through CAN (D106 v1)

Added read-only `CAN_DIAG_BLOCK_MOTION_UART_TRACE=0xD106`, schema 1, flags 0x83.
F020 advertises D103 and D106; D103 v1 remains unchanged. F021 D106 currently
supports only motor_id 0 / USART1. Each block has 12 DATA records, sequences
00..0A,8B followed by DONE. Each record is item_id u16 + value u32, little endian.
Each response fits the existing 16-entry TX queue in isolation; Studio rejects
missing/out-of-order fragments rather than publishing partial data.

D106 item definitions:

| ID | Value |
|---|---|
| 1 | Completed exchange sequence; 0 = no completed exchange |
| 2 | byte0 slave address, byte1 register (without write bit), byte2 write flag 0/1, byte3 zero |
| 3 | bytes: RX arm HAL status, TX HAL status, operation HAL status, valid reply offset; FF = not attempted/not found |
| 4 | byte0 TX requested length, byte1 RX captured length, upper bytes zero |
| 5 | Elapsed milliseconds including TX and receive wait |
| 6 | HAL UART ErrorCode captured before abort |
| 7,8 | TX requested bytes 0..7, packed in wire order from least significant byte |
| 9,10,11 | RX captured bytes 0..11, same packing |
| 12 | Reserved, zero |

The trace records the last completed low-level read/write, not a continuous log.
During configuration a read can return the preceding completed exchange. Invalid
and partial replies, including echo-only responses, remain visible. RX count is
captured before HAL_UART_AbortReceive clears it. IRQ is masked during final capture
and publication; diagnostics only copies RAM and never initiates UART traffic.
Writes capture TX only (RX arm FF); verified writes subsequently read IFCNT, which
becomes the last exchange. TX bytes are the requested HAL buffer, not proof of bytes
on the physical pin; HAL_OK confirms completion from the MCU peripheral viewpoint.

Studio automatically fetches D106 after D103 when advertised with supported schema.
It displays TX, RX, separate RX-arm/TX/operation statuses, errors and valid-response
offset; older firmware still shows D103 with a missing-trace notice.

Validation: host C UART model covers valid replies with/without echo, echo-only,
silence, malformed replies, TX timeout and RX-arm failure; production dispatcher
encoder is exercised on host. Firmware builds; offline Studio tests cover discovery,
sequential block reads, byte decoding and malformed records. Hardware verification
requires flashing the new image and restarting Studio; no bench success is claimed.


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
