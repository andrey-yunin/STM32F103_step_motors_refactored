/*
   * device_mapping.c
   *
   *  Created on: Mar 5, 2026
   *      Author: andrey
   */

  #include "device_mapping.h"

  /*
   * Таблица соответствия: логический device_id -> физический motor_id (0-7)
   *
   * Физ. индекс | Таймер/Канал | Пин  | Логический ID | Назначение
   * ------------|--------------|------|---------------|------------------
   *     0       | TIM1_CH1     | PA8  |  1            | Дозатор XY
   *     1       | TIM1_CH2     | PA9  |  2            | Дозатор Z
   *     2       | TIM1_CH3     | PA10 |  3            | Дозатор шприц
   *     3       | TIM1_CH4     | PA11 | 10            | Реакционный диск
   *     4       | TIM2_CH1     | PA0  | 11            | Диск реагентов
   *     5       | TIM2_CH2     | PA1  | 20            | Миксер XY
   *     6       | TIM2_CH3     | PB10 | 21            | Миксер Z
   *     7       | TIM2_CH4     | PB11 | --            | Резерв (свободен)
   */

  uint8_t DeviceMapping_ToPhysicalId(uint8_t device_id)
  {
      switch (device_id) {
          case DEV_DISPENSER_MOTOR_XY:        return 0;
          case DEV_DISPENSER_MOTOR_Z:         return 1;
          case DEV_DISPENSER_MOTOR_SYRINGE:   return 2;
          case DEV_REACTION_DISK_MOTOR:       return 3;
          case DEV_REAGENT_SAMPLE_DISK_MOTOR: return 4;
          case DEV_MIXER_MOTOR_XY:            return 5;
          case DEV_MIXER_MOTOR_Z:             return 6;
          default:                            return MOTOR_ID_INVALID;
      }
  }
