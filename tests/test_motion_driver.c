#include <assert.h>
#include <stdio.h>
#include "motion_driver.h"
TIM_TypeDef tim1, tim2;
GPIO_TypeDef gpioa,gpiob;
TIM_HandleTypeDef htim1={&tim1,HAL_TIM_ACTIVE_CHANNEL_1}, htim2={&tim2,HAL_TIM_ACTIVE_CHANNEL_1};
static int fail, running;
void HAL_GPIO_WritePin(GPIO_TypeDef *p,uint16_t pin,int high) { if(high)p->odr|=pin;else p->odr&=~pin; }
int HAL_TIM_PWM_Start(TIM_HandleTypeDef *h,uint32_t ch) { (void)h;(void)ch; if(fail)return 1;running=1;return 0; }
int HAL_TIM_PWM_Start_IT(TIM_HandleTypeDef *h,uint32_t ch) { return HAL_TIM_PWM_Start(h,ch); }
int HAL_TIM_PWM_Stop(TIM_HandleTypeDef *h,uint32_t ch) { (void)h;(void)ch;running=0;return 0; }
int HAL_TIM_PWM_Stop_IT(TIM_HandleTypeDef *h,uint32_t ch) { return HAL_TIM_PWM_Stop(h,ch); }
extern void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *);
int main(void)
{
    uint8_t id=255;
    MotionDriver_Init(); assert(gpioa.odr&GPIO_PIN_4);
    fail=1; assert(!MotionDriver_StartMotor(0,200) && !running && (gpioa.odr&GPIO_PIN_4));
    fail=0; tim1.SR=0xffff;
    assert(MotionDriver_StartMotor(0,200) && running && !(gpioa.odr&GPIO_PIN_4));
    assert(tim1.SR==0 && 64000000UL/((tim1.PSC+1)*(tim1.ARR+1))==200);
    for(int n=0;n<100;n++)HAL_TIM_PWM_PulseFinishedCallback(&htim1);
    assert(running && !MotionDriver_ConsumeCompletedMotor(&id));
    MotionDriver_StopMotor(0); assert(!running && (gpioa.odr&GPIO_PIN_4));
    assert(MotionDriver_StartFinite(0,200,100));
    for(int n=0;n<99;n++)HAL_TIM_PWM_PulseFinishedCallback(&htim1);
    assert(running && !MotionDriver_ConsumeCompletedMotor(&id));
    HAL_TIM_PWM_PulseFinishedCallback(&htim1);
    assert(!running && (gpioa.odr&GPIO_PIN_4));
    assert(MotionDriver_ConsumeCompletedMotor(&id) && id==0);
    assert(!MotionDriver_ConsumeCompletedMotor(&id));
    assert(MotionDriver_StartFinite(0,200,100)); MotionDriver_StopMotor(0);
    HAL_TIM_PWM_PulseFinishedCallback(&htim1); assert(!MotionDriver_ConsumeCompletedMotor(&id));
    fail=1; assert(!MotionDriver_StartFinite(0,200,100) && (gpioa.odr&GPIO_PIN_4));
    puts("PASS: PWM failure cleanup, 200 Hz setup, stale flags, continuous/finite completion, STOP");
}
