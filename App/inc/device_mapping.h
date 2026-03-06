 /*
   * device_mapping.h
   *
   * Таблица маппинга логических идентификаторов устройств (из протокола дирижера)
   * в физические индексы моторов (0-7) на плате шаговых двигателей.
   *
   *  Created on: Mar 5, 2026
   *      Author: andrey
   */

  #ifndef DEVICE_MAPPING_H_
  #define DEVICE_MAPPING_H_

  #include <stdint.h>
  #include "app_config.h" // Для MOTOR_COUNT

  // ============================================================
  // Логические ID устройств (из протокола дирижера)
  // Должны совпадать с device_mapping.h дирижера
  // ============================================================

  // --- Дозатор (Dispenser), диапазон 1-9 ---
  #define DEV_DISPENSER_MOTOR_XY       1
  #define DEV_DISPENSER_MOTOR_Z        2
  #define DEV_DISPENSER_MOTOR_SYRINGE  3

  // --- Диски и роторы, диапазон 10-19 ---
  #define DEV_REACTION_DISK_MOTOR          10
  #define DEV_REAGENT_SAMPLE_DISK_MOTOR    11

  // --- Миксер (Mixer), диапазон 20-29 ---
  #define DEV_MIXER_MOTOR_XY           20
  #define DEV_MIXER_MOTOR_Z            21
  // DEV_MIXER_PADDLE_MOTOR (22) — обычный DC-мотор, управляется другой платой

  // ============================================================
  // Количество активных шаговых двигателей на этой плате
  // ============================================================
  #define STEPPER_COUNT_ACTIVE        7   // 7 из 8 каналов задействованы

  // ============================================================
  // Значение для "неизвестный ID"
  // ============================================================
  #define MOTOR_ID_INVALID            0xFF

  // ============================================================
  // Функция трансляции: логический device_id -> физический motor_id
  // Возвращает MOTOR_ID_INVALID если device_id не привязан к этой плате
  // ============================================================
  uint8_t DeviceMapping_ToPhysicalId(uint8_t device_id);

  #endif /* DEVICE_MAPPING_H_ */
