/* Host-тест настоящего C-драйвера; UART-модель проверяет кадры и эмулирует TMC. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "tmc2209_driver.h"
#include "task_tmc2209_manager.h"
#include "watchdog.h"

UART_HandleTypeDef huart1, huart2;
static UART_HandleTypeDef *silent_uart;
static uint8_t *rx_buffer;
static uint32_t regs[128], tick;
static unsigned writes, echo, corrupt, silent, drop_write, wrong_master, wrong_reg, tx_fail, rx_fail, uart_error;
static int failed_read = -1;

/* Независимая реализация CRC: отражённый LFSR + обращение результата. */
static uint8_t wire_crc(const uint8_t *p, unsigned n)
{
    uint8_t crc=0, reversed=0;
    while(n--) {
        crc ^= *p++;
        for(unsigned i=0;i<8;i++) crc=(uint8_t)((crc>>1)^((crc&1)?0xE0:0));
    }
    for(unsigned i=0;i<8;i++) { reversed=(uint8_t)((reversed<<1)|(crc&1)); crc>>=1; }
    return reversed;
}
HAL_StatusTypeDef HAL_UART_Receive_IT(UART_HandleTypeDef *u,uint8_t *buf,uint16_t size)
{
    assert(size==12); rx_buffer=buf; u->RxXferCount=size; u->ErrorCode=0;
    return rx_fail?HAL_BUSY:HAL_OK;
}
HAL_StatusTypeDef HAL_UART_AbortReceive(UART_HandleTypeDef *u)
{ rx_buffer=NULL; u->RxXferCount=0; return HAL_OK; }
HAL_StatusTypeDef HAL_UART_Transmit(UART_HandleTypeDef *u,const uint8_t *p,uint16_t n,uint32_t timeout)
{
    assert(timeout==20); assert(p[0]==5 && p[1]==0);
    assert(wire_crc(p,n-1)==p[n-1]);
    if(tx_fail) return HAL_TIMEOUT;
    u->ErrorCode=uart_error;
    unsigned reg=p[2]&0x7f;
    if(n==8) {
        assert(p[2]&0x80); ++writes;
        if(!drop_write) {
            regs[reg]=((uint32_t)p[3]<<24)|((uint32_t)p[4]<<16)|((uint32_t)p[5]<<8)|p[6];
            regs[2]=(regs[2]+1)&255;
        }
        return HAL_OK;
    }
    assert(n==4 && rx_buffer!=NULL);
    unsigned offset=echo?4:0;
    if(echo) memcpy(rx_buffer,p,4);
    u->RxXferCount=(uint16_t)(12-offset);
    if(silent || u==silent_uart || (int)reg==failed_read) return HAL_OK;
    uint32_t v=regs[reg];
    uint8_t reply[8]={5,wrong_master?0:255,(uint8_t)(wrong_reg?reg^1:reg),
                     (uint8_t)(v>>24),(uint8_t)(v>>16),(uint8_t)(v>>8),(uint8_t)v,0};
    reply[7]=wire_crc(reply,7)^(corrupt?1:0);
    memcpy(rx_buffer+offset,reply,8); u->RxXferCount=(uint16_t)(4-offset);
    return HAL_OK;
}
uint32_t HAL_GetTick(void) { return tick++; }
void HAL_Delay(uint32_t ms) { tick+=ms; }
void osDelay(uint32_t ticks) { tick+=ticks; }
void AppWatchdog_Heartbeat(AppWatchdogClient_t client) { (void)client; }

/* Статическая функция конфигурации вызывается без запуска бесконечной RTOS-задачи. */
#include "../App/src/tasks/task_tmc2209_manager.c"

static void reset(void)
{
    memset(regs,0,sizeof(regs)); regs[6]=0x21000001; regs[0]=0x101; regs[0x6c]=0x10000053;
    tick=writes=echo=corrupt=silent=drop_write=wrong_master=wrong_reg=tx_fail=rx_fail=uart_error=0;
    failed_read=-1; rx_buffer=NULL; silent_uart=NULL; memset((void *)&g_tmc_diag,0,sizeof(g_tmc_diag));
}
int main(void)
{
    const uint8_t known[]={5,0,0}; assert(wire_crc(known,3)==0x48);
    TMC2209_Handle_t h; uint32_t value;
    reset(); assert(TMC2209_Init(&h,&huart1,0)==HAL_OK);
    for(echo=0;echo<2;echo++) {
        value=0; assert(TMC2209_ReadRegister(&h,TMC2209_IOIN,&value)==HAL_OK);
        assert(value==0x21000001 && rx_buffer==NULL);
        TMC2209_Trace_t trace; TMC2209_GetTrace(&h,&trace);
        assert(trace.words[3] == (4U | ((echo ? 12U : 8U) << 8)));
        assert(trace.words[2] == ((echo ? 4UL : 0UL) << 24));
        assert((trace.words[6] & 0xffffffU) == 0x060005U);
        assert((trace.words[8] & 0xffffffU) == (echo ? 0x060005U : 0x06ff05U));
    }
    echo=1; corrupt=1; value=0xdeadbeef;
    assert(TMC2209_ReadRegister(&h,TMC2209_IOIN,&value)!=HAL_OK && value==0xdeadbeef && !rx_buffer);
    corrupt=0; wrong_master=1; assert(TMC2209_ReadRegister(&h,TMC2209_IOIN,&value)!=HAL_OK);
    wrong_master=0; wrong_reg=1; assert(TMC2209_ReadRegister(&h,TMC2209_IOIN,&value)!=HAL_OK);
    wrong_reg=0; silent=1; assert(TMC2209_ReadRegister(&h,TMC2209_IOIN,&value)==HAL_TIMEOUT);
    TMC2209_Trace_t trace; TMC2209_GetTrace(&h,&trace);
    assert(trace.words[3] == 0x404 && trace.words[8] == trace.words[6]);
    assert(trace.words[2] == 0xff030000U);
    echo=0; assert(TMC2209_ReadRegister(&h,TMC2209_IOIN,&value)==HAL_TIMEOUT);
    TMC2209_GetTrace(&h,&trace); assert(trace.words[3]==4 && trace.words[8]==0);
    silent=0; tx_fail=1; assert(TMC2209_ReadRegister(&h,TMC2209_IOIN,&value)==HAL_TIMEOUT && !rx_buffer);
    TMC2209_GetTrace(&h,&trace); assert(trace.words[2]==0xff030300U && trace.words[3]==4);
    tx_fail=0; rx_fail=1;
    assert(TMC2209_ReadRegister(&h,TMC2209_IOIN,&value)==HAL_BUSY);
    TMC2209_GetTrace(&h,&trace); assert(trace.words[2]==0xff02ff02U && trace.words[3]==4);
    rx_fail=0; uart_error=8;
    assert(TMC2209_ReadRegister(&h,TMC2209_IOIN,&value)==HAL_ERROR);
    TMC2209_GetTrace(&h,&trace); assert(trace.words[5]==8 && (trace.words[3]>>8)==8);
    uart_error=0; regs[2]=255;
    assert(TMC2209_WriteVerified(&h,TMC2209_VACTUAL,0)==HAL_OK && regs[2]==0);
    drop_write=1; assert(TMC2209_WriteVerified(&h,TMC2209_VACTUAL,0)==HAL_ERROR); drop_write=0;
    assert(TMC2209_SetMotorCurrent(&h,70,40)==HAL_OK && regs[0x10]==0x0004150c);
    failed_read=0x6c; unsigned before=writes;
    assert(TMC2209_SetMicrosteps(&h,16)!=HAL_OK && writes==before);
    failed_read=-1;
    assert(TMC2209_SetMicrosteps(&h,16)==HAL_OK && regs[0x6c]==0x14000053);
    assert(TMC2209_SetMicrosteps(&h,3)==HAL_ERROR);
    assert(TMC2209_ReadRegister(NULL,TMC2209_GCONF,&value)==HAL_ERROR);
    for(unsigned mode=0;mode<2;mode++) {
        reset(); echo=mode; assert(ConfigureBenchMotor(0)==HAL_OK);
        assert(g_tmc_diag[0].ready && g_tmc_diag[0].stage==17 && TMCManager_IsReady(2));
        assert(!TMCManager_IsReady(0) && !TMCManager_IsReady(1) && !TMCManager_IsReady(7));
        assert(regs[0]==0x1c1 && regs[0x6c]==0x14000053 && regs[0x11]==20);
        assert(regs[0x10]==0x4150c && regs[3]==0x200 && regs[0x22]==0 && writes==7);
    }
    reset(); regs[0x6c]=0x04000000;
    assert(ConfigureBenchMotor(0)==HAL_OK && (regs[0x6c]&15)==3);
    reset(); regs[6]=0x20000001; assert(ConfigureBenchMotor(0)==HAL_ERROR && !writes && !g_tmc_diag[0].ready);
    reset(); regs[6]=0x21000000; assert(ConfigureBenchMotor(0)==HAL_ERROR && !writes);
    reset(); drop_write=1; assert(ConfigureBenchMotor(0)==HAL_ERROR && !g_tmc_diag[0].ready);
    reset(); silent=1; assert(ConfigureBenchMotor(0)==HAL_TIMEOUT && !writes);
    reset(); regs[1]=4; assert(ConfigureBenchMotor(0)==HAL_ERROR && !g_tmc_diag[0].ready);
    reset(); regs[0x6f]=2; assert(ConfigureBenchMotor(0)==HAL_ERROR && !g_tmc_diag[0].ready);
    TMC_Diagnostics_t copied;
    TMC_Diagnostics_t prior=g_tmc_diag[0];
    assert(TMCManager_GetDiagnostics(2,&copied));
    assert(!memcmp(&prior,&copied,sizeof(prior)));
    assert(!TMCManager_GetDiagnostics(0,&copied) && !TMCManager_GetDiagnostics(1,&copied) && !TMCManager_GetDiagnostics(2,NULL));
    // UART1 silence cannot contaminate UART2 results or erase its trace.
    reset(); silent_uart=&huart1;
    assert(ConfigureBenchMotor(0)==HAL_TIMEOUT);
    TMC2209_Trace_t first_trace, second_trace, first_after;
    assert(TMCManager_GetTrace(2,&first_trace));
    assert(first_trace.words[0]==1 && first_trace.words[3]==4);
    assert(ConfigureBenchMotor(1)==HAL_OK);
    assert(TMCManager_IsReady(6) && !TMCManager_IsReady(2));
    assert(TMCManager_GetTrace(6,&second_trace));
    assert(second_trace.words[0]>1 && (second_trace.words[2]&0x00ffffffU)==0);
    assert(TMCManager_GetTrace(2,&first_after));
    assert(!memcmp(&first_trace,&first_after,sizeof(first_trace)));
    assert(TMCManager_GetDiagnostics(6,&copied) && copied.ready && copied.valid_mask==63);
    assert(TMCManager_GetDiagnostics(2,&copied) && !copied.ready && copied.stage==2);
    assert(!TMCManager_GetTrace(0,&trace) && !TMCManager_GetTrace(6,NULL));
    // Reverse failure direction: previously successful first bus is preserved.
    reset(); assert(ConfigureBenchMotor(0)==HAL_OK); silent_uart=&huart2;
    assert(ConfigureBenchMotor(1)==HAL_TIMEOUT);
    assert(TMCManager_IsReady(2) && !TMCManager_IsReady(6));
    puts("PASS: independent UART1/UART2 snapshots and traces; snapshot, CRC, echo/no-echo, bad/missing replies, timeout cleanup, IFCNT/wrap, register fields, motor2/address0 init and faults");
}
