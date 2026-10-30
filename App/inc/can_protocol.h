#ifndef CAN_PROTOCOL_H_
#define CAN_PROTOCOL_H_

#include <stdint.h>
#include <stdbool.h>

// ============================================================
// Приоритеты (биты 28-26 CAN ID)
// ============================================================
#define CAN_PRIORITY_HIGH       0   // Команды от дирижера
#define CAN_PRIORITY_NORMAL     1   // Ответы от исполнителей

// ============================================================
// Типы сообщений (биты 25-24 CAN ID)
// ============================================================
#define CAN_MSG_TYPE_COMMAND        0   // Команда (Conductor -> Executor)
#define CAN_MSG_TYPE_ACK            1   // Подтверждение (Executor -> Conductor)
#define CAN_MSG_TYPE_NACK           2   // Ошибка (Executor -> Conductor)
#define CAN_MSG_TYPE_DATA_DONE_LOG  3   // DATA/DONE/LOG (Executor -> Conductor)

// ============================================================
// Подтипы для MSG_TYPE_DATA_DONE_LOG (первый байт payload)
// ============================================================
#define CAN_SUB_TYPE_DONE       0x01
#define CAN_SUB_TYPE_DATA       0x02
#define CAN_SUB_TYPE_LOG        0x03

// ============================================================
// Адреса узлов (биты 23-16 dst, 15-8 src)
// ============================================================
#define CAN_ADDR_BROADCAST      0x00
#define CAN_ADDR_HOST           0x01
#define CAN_ADDR_CONDUCTOR      0x10
#define CAN_ADDR_MOTOR_BOARD    0x20  // Наш адрес (плата шаговых двигателей)

// ============================================================
// Типы устройств (для команды INFO)
// ============================================================
#define CAN_DEVICE_TYPE_MOTION  0x01  // Плата управления моторами

// ============================================================
// Коды команд моторов (байты 0-1 payload, Little-Endian)
// ============================================================
#define CAN_CMD_MOTOR_ROTATE            0x0101
#define CAN_CMD_MOTOR_HOME              0x0102
#define CAN_CMD_MOTOR_START_CONTINUOUS  0x0103
#define CAN_CMD_MOTOR_STOP              0x0104

// ============================================================
// Сервисные команды (0xF001 - 0xF00F)
// ============================================================
#define CAN_CMD_SRV_GET_DEVICE_INFO     0xF001  // Получить тип и версию
#define CAN_CMD_SRV_REBOOT              0xF002  // Перезагрузка
#define CAN_CMD_SRV_FLASH_COMMIT        0xF003  // Сохранить настройки в Flash
#define CAN_CMD_SRV_GET_UID             0xF004  // Получить Unique ID (3 пакета)
#define CAN_CMD_SRV_SET_NODE_ID         0xF005  // Установить новый CAN NodeID
#define CAN_CMD_SRV_FACTORY_RESET       0xF006  // Сброс к заводским настройкам
#define CAN_CMD_SRV_GET_STATUS          0xF007  // Получить диагностический статус

// Общий optional engineering contract из dds240_global_config.h.
#define CAN_CMD_SRV_GET_DIAG_CAPS  0xF020U
#define CAN_CMD_SRV_GET_DIAG_BLOCK 0xF021U
#define CAN_DIAG_BLOCK_MOTION_TMC  0xD103U
#define CAN_DIAG_CAP_FLAG_READ_ONLY 0x01U
#define CAN_DIAG_CAP_FLAG_INDEXED   0x02U
#define CAN_DIAG_CAP_FLAG_EXPERIMENTAL 0x80U

/* MOTION_TMC schema v1: startup snapshot, index=motor_id; current implementation index 0 only.
 * DATA sequence is 0..N-1, EOT (0x80) only on the last record; DONE selector=0.
 * F020 request bytes 2..7 are zero. F021 flags/reserved are zero.
 * The block reads RAM only: no UART writes, retries or register reads on request.
 * Register values are meaningful only when the matching VALID_MASK bit is set.
 */
#define CAN_DIAG_MOTION_TMC_SCHEMA_VERSION          1U
#define CAN_DIAG_TMC_ITEM_MOTOR_ID                  0x0001U
#define CAN_DIAG_TMC_ITEM_STATE                     0x0002U
#define CAN_DIAG_TMC_ITEM_READY                     0x0003U
#define CAN_DIAG_TMC_ITEM_STAGE                     0x0004U
#define CAN_DIAG_TMC_ITEM_HAL_STATUS                0x0005U
#define CAN_DIAG_TMC_ITEM_VALID_MASK                0x0006U
#define CAN_DIAG_TMC_ITEM_IOIN                      0x0007U
#define CAN_DIAG_TMC_ITEM_GCONF                     0x0008U
#define CAN_DIAG_TMC_ITEM_CHOPCONF                  0x0009U
#define CAN_DIAG_TMC_ITEM_IFCNT                     0x000AU
#define CAN_DIAG_TMC_ITEM_GSTAT                     0x000BU
#define CAN_DIAG_TMC_ITEM_DRV_STATUS                0x000CU
#define CAN_DIAG_TMC_STATE_NOT_STARTED             0U
#define CAN_DIAG_TMC_STATE_CONFIGURING             1U
#define CAN_DIAG_TMC_STATE_READY                   2U
#define CAN_DIAG_TMC_STATE_FAILED                  3U
#define CAN_DIAG_TMC_VALID_IOIN                    (1U << 0)
#define CAN_DIAG_TMC_VALID_GCONF                   (1U << 1)
#define CAN_DIAG_TMC_VALID_CHOPCONF                (1U << 2)
#define CAN_DIAG_TMC_VALID_IFCNT                   (1U << 3)
#define CAN_DIAG_TMC_VALID_GSTAT                   (1U << 4)
#define CAN_DIAG_TMC_VALID_DRV_STATUS              (1U << 5)
/* STAGE: 0 not started, 1 handle init, 2 IOIN/version/ENN, 3 NODECONF,
 * 4 GCONF read, 5 GCONF write, 6 GCONF verification, 7 IHOLD_IRUN,
 * 8 TPOWERDOWN, 9 VACTUAL, 10 CHOPCONF read, 11 CHOPCONF write,
 * 12 microstep setup, 13 CHOPCONF verification, 14 IFCNT, 15 GSTAT,
 * 16 DRV_STATUS, 17 complete. HAL_STATUS: 0 OK, 1 ERROR, 2 BUSY, 3 TIMEOUT.
 * STATE determines startup outcome; during CONFIGURING HAL_STATUS may be from
 * the preceding operation. READY is configuration readiness, not physical motion.
 */

// Магический ключ для опасных операций
#define SRV_MAGIC_REBOOT                0x55AA  // Ключ для перезагрузки
#define SRV_MAGIC_FACTORY_RESET         0xDEAD  // Ключ для Factory Reset

// ============================================================
// Макрос построения 29-bit Extended CAN ID
// ============================================================
#define CAN_BUILD_ID(priority, msg_type, dst_addr, src_addr) \
    ((uint32_t)(((priority) & 0x07) << 26) | \
                 (((msg_type) & 0x03) << 24) | \
                 (((dst_addr) & 0xFF) << 16) | \
                 (((src_addr) & 0xFF) << 8))

// ============================================================
// Макросы извлечения полей из 29-bit CAN ID
// ============================================================
#define CAN_GET_PRIORITY(id)    ((uint8_t)(((id) >> 26) & 0x07))
#define CAN_GET_MSG_TYPE(id)    ((uint8_t)(((id) >> 24) & 0x03))
#define CAN_GET_DST_ADDR(id)    ((uint8_t)(((id) >> 16) & 0xFF))
#define CAN_GET_SRC_ADDR(id)    ((uint8_t)(((id) >> 8)  & 0xFF))

// ============================================================
// Коды ошибок для NACK-ответов
// ============================================================
// Common executor NACK namespace: одинаковые значения для всех плат.
#define CAN_ERR_NONE                0x0000
#define CAN_ERR_UNKNOWN_CMD         0x0001
#define CAN_ERR_INVALID_DEVICE_ID   0x0002
#define CAN_ERR_DEVICE_BUSY         0x0003
#define CAN_ERR_INVALID_KEY         0x0004
#define CAN_ERR_FLASH_WRITE         0x0005
#define CAN_ERR_INVALID_PARAM       0x0006

// Motion aliases: motor_id является low-level channel/resource id.
#define CAN_ERR_INVALID_MOTOR_ID    CAN_ERR_INVALID_DEVICE_ID
#define CAN_ERR_MOTOR_BUSY          CAN_ERR_DEVICE_BUSY

// ============================================================
// Метрики GET_STATUS (0xF007): metric_id:uint16 LE + value:uint32 LE
// ============================================================
#define CAN_STATUS_RX_TOTAL             0x0001
#define CAN_STATUS_TX_TOTAL             0x0002
#define CAN_STATUS_RX_QUEUE_OVERFLOW    0x0003
#define CAN_STATUS_TX_QUEUE_OVERFLOW    0x0004
#define CAN_STATUS_DISPATCHER_OVERFLOW  0x0005
#define CAN_STATUS_DROP_NOT_EXT         0x0006
#define CAN_STATUS_DROP_WRONG_DST       0x0007
#define CAN_STATUS_DROP_WRONG_TYPE      0x0008
#define CAN_STATUS_DROP_WRONG_DLC       0x0009
#define CAN_STATUS_TX_MAILBOX_TIMEOUT   0x000A
#define CAN_STATUS_TX_HAL_ERROR         0x000B
#define CAN_STATUS_ERROR_CALLBACK       0x000C
#define CAN_STATUS_ERROR_WARNING        0x000D
#define CAN_STATUS_ERROR_PASSIVE        0x000E
#define CAN_STATUS_BUS_OFF              0x000F
#define CAN_STATUS_LAST_HAL_ERROR       0x0010
#define CAN_STATUS_LAST_ESR             0x0011
#define CAN_STATUS_APP_QUEUE_OVERFLOW   0x0012

// ============================================================
// Прототипы функций ответов (реализованы в task_can_handler.c)
// ============================================================
void CAN_SendAck(uint16_t cmd_code);
void CAN_SendNackPublic(uint16_t cmd_code, uint16_t error_code);
void CAN_SendDone(uint16_t cmd_code, uint8_t device_id);
void CAN_SendData(uint16_t cmd_code, uint8_t sequence_info,
                  const uint8_t *data, uint8_t len);

/* D106 v1: last completed UART exchange, twelve scalar records (see protocol doc). */
#define CAN_DIAG_BLOCK_MOTION_UART_TRACE 0xD106U
#define CAN_DIAG_MOTION_UART_TRACE_SCHEMA_VERSION 1U

#endif /* CAN_PROTOCOL_H_ */
