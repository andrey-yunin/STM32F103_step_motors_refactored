#ifndef MOTION_TEST_HAL_H
#define MOTION_TEST_HAL_H
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
typedef struct { uint32_t CCER,CR1,BDTR,PSC,ARR,EGR,SR,CCR[4]; } TIM_TypeDef;
typedef struct { TIM_TypeDef *Instance; uint32_t Channel; } TIM_HandleTypeDef;
typedef struct { uint32_t odr; } GPIO_TypeDef;
extern TIM_TypeDef tim1, tim2;
extern GPIO_TypeDef gpioa,gpiob;
#define TIM1 (&tim1)
#define TIM2 (&tim2)
#define GPIOA (&gpioa)
#define GPIOB (&gpiob)
#define GPIO_PIN_0 (1U<<0)
#define GPIO_PIN_1 (1U<<1)
#define GPIO_PIN_2 (1U<<2)
#define GPIO_PIN_3 (1U<<3)
#define GPIO_PIN_4 (1U<<4)
#define GPIO_PIN_5 (1U<<5)
#define GPIO_PIN_6 (1U<<6)
#define GPIO_PIN_7 (1U<<7)
#define GPIO_PIN_12 (1U<<12)
#define GPIO_PIN_13 (1U<<13)
#define GPIO_PIN_14 (1U<<14)
#define GPIO_PIN_15 (1U<<15)
#define GPIO_PIN_SET 1
#define GPIO_PIN_RESET 0
#define TIM_CHANNEL_1 0
#define TIM_CHANNEL_2 1
#define TIM_CHANNEL_3 2
#define TIM_CHANNEL_4 3
#define HAL_TIM_ACTIVE_CHANNEL_1 1
#define HAL_TIM_ACTIVE_CHANNEL_2 2
#define HAL_TIM_ACTIVE_CHANNEL_3 4
#define HAL_TIM_ACTIVE_CHANNEL_4 8
#define TIM_CCER_CC1E 1
#define TIM_CCER_CC2E 16
#define TIM_CCER_CC3E 256
#define TIM_CCER_CC4E 4096
#define TIM_CR1_CEN 1
#define TIM_BDTR_MOE 0x8000
#define TIM_EGR_UG 1
#define __HAL_TIM_SET_COMPARE(h,c,v) ((h)->Instance->CCR[c]=(v))
#define __HAL_RCC_GPIOA_CLK_ENABLE() ((void)0)
#define __HAL_RCC_GPIOB_CLK_ENABLE() ((void)0)
#define __HAL_RCC_TIM1_CLK_ENABLE() ((void)0)
#define __HAL_RCC_TIM2_CLK_ENABLE() ((void)0)
#define __disable_irq() ((void)0)
#define __enable_irq() ((void)0)
#define HAL_OK 0
void HAL_GPIO_WritePin(GPIO_TypeDef *,uint16_t,int);
int HAL_TIM_PWM_Start(TIM_HandleTypeDef *,uint32_t);
int HAL_TIM_PWM_Start_IT(TIM_HandleTypeDef *,uint32_t);
int HAL_TIM_PWM_Stop(TIM_HandleTypeDef *,uint32_t);
int HAL_TIM_PWM_Stop_IT(TIM_HandleTypeDef *,uint32_t);
#endif
