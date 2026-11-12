"""Exercise the real controller loop with queued commands and recording hardware boundaries."""
from pathlib import Path
import subprocess
import tempfile
root=Path(__file__).resolve().parents[1]
source=(root/'App/src/tasks/task_motion_controller.c').read_text()
source='\n'.join(line for line in source.splitlines() if not line.startswith('#include'))
config=(root/'App/inc/app_config.h').read_text()
end=config.index('} MotionCommand_t;')+len('} MotionCommand_t;')
command=config[config.rfind('typedef struct {',0,end):end]
preamble=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <setjmp.h>
#include <stdio.h>
#include "motion_planner.h"
#include "task_motion_controller.h"
#include "can_protocol.h"
#define MOTOR_COUNT 8
#define MOTION_MAX_SAFE_SPEED_STEPS_PER_SEC 20000U
#define MOTION_COMPLETION_POLL_TIMEOUT_MS 10U
#define APP_WDG_CLIENT_MOTION 0
#define APP_WATCHDOG_TEST_STALL_MOTION_AFTER_ROTATE 0
#define osOK 0
static int motion_queueHandle;
static jmp_buf exit_loop;
static unsigned next_command, start_count, stop_count, response_count, uart_checks;
static unsigned fail_start_at=999;
static int completed=-1;
static struct {uint16_t cmd,error;} responses[32];
static bool running[8];
void AppWatchdog_Heartbeat(int client) {(void)client;}
bool TMCManager_IsReady(uint8_t id) {(void)id;++uart_checks;return false;}
bool AppSafety_IsResetPending(void) { return false; }
void MotionDriver_Init(void) {}
void MotionDriver_SetDirection(uint8_t id,bool forward) {(void)id;(void)forward;}
bool MotionDriver_StartMotor(uint8_t id,uint32_t speed) {
    assert(speed==100);++start_count;
    if(start_count==fail_start_at)return false;
    running[id]=true;return true;
}
bool MotionDriver_StartFinite(uint8_t id,uint32_t speed,uint32_t steps) {
    assert(steps==10);return MotionDriver_StartMotor(id,speed);
}
void MotionDriver_StopMotor(uint8_t id) {++stop_count;running[id]=false;}
bool MotionDriver_ConsumeCompletedMotor(uint8_t *id) {
    if(completed<0)return false;*id=(uint8_t)completed;running[*id]=false;completed=-1;return true;
}
void CAN_SendDone(uint16_t cmd,uint8_t id) {(void)id;responses[response_count].cmd=cmd;responses[response_count++].error=0;}
void CAN_SendNackPublic(uint16_t cmd,uint16_t error) {responses[response_count].cmd=cmd;responses[response_count++].error=error;}
'''
queue=r'''
static MotionCommand_t commands[32];
static unsigned command_count;
static int osMessageQueueGet(int handle,void *out,void *priority,uint32_t timeout) {
    (void)handle;(void)priority;(void)timeout;
    if(next_command==command_count)longjmp(exit_loop,1);
    memcpy(out,&commands[next_command++],sizeof(MotionCommand_t));return osOK;
}
'''
main=r'''
static void add(uint8_t id,uint8_t kind) {
    commands[command_count++]=(MotionCommand_t){.motor_id=id,.device_id=id,.command_id=kind,
        .cmd_code=kind==CMD_STOP?0x104:kind==CMD_SET_SPEED?0x103:0x101,
        .speed_steps_per_sec=100,.steps=10,.direction=1};
}
int main(void) {
    // Reproduce STOP -> START with failed UART readiness, then real occupancy.
    add(0,CMD_STOP); add(0,CMD_SET_SPEED); add(0,CMD_SET_SPEED); add(1,CMD_SET_SPEED);
    add(0,CMD_STOP); add(0,CMD_SET_SPEED); add(0,CMD_STOP);
    // Sibling axis can acquire the timer after STOP; second timer is independent.
    add(1,CMD_SET_SPEED);add(4,CMD_SET_SPEED);add(1,CMD_STOP);add(4,CMD_STOP);
    // HAL failure must release the axis/group so retry succeeds.
    add(0,CMD_SET_SPEED);add(0,CMD_SET_SPEED);add(0,CMD_STOP);
    // Finite command may be cancelled and immediately followed by START.
    add(0,CMD_MOVE_RELATIVE);add(0,CMD_STOP);add(0,CMD_SET_SPEED);add(0,CMD_STOP);
    fail_start_at=5;
    if(!setjmp(exit_loop))app_start_task_motion_controller(NULL);
    assert(uart_checks==0 && start_count==8 && stop_count==8);
    assert(response_count==17); // finite move gets no immediate DONE
    for(unsigned i=0;i<response_count;i++) {
        uint16_t expected=(i==2 || i==3)?CAN_ERR_DEVICE_BUSY:i==11?CAN_ERR_INVALID_PARAM:0;
        assert(responses[i].error==expected);
    }
    assert(responses[0].cmd==0x104 && responses[1].cmd==0x103);
    for(unsigned i=0;i<8;i++)assert(!running[i]&&!g_motor_active[i]);
    assert(!MotionController_IsGroupBusy(0)&&!MotionController_IsGroupBusy(1));
    puts("PASS: failed UART does not gate STEP, STOP/START, real axis/group BUSY, independent timers, HAL failure cleanup, finite cancellation");
}
'''
with tempfile.TemporaryDirectory(prefix='motion-controller-') as tmp:
    p=Path(tmp);(p/'test.c').write_text(preamble+command+queue+source+main)
    subprocess.run(['gcc','-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-parameter','-Wno-misleading-indentation','-I'+str(root/'App/inc'),'-I'+str(root/'App/inc/tasks'),str(p/'test.c'),str(root/'App/src/motion_planner.c'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True)
