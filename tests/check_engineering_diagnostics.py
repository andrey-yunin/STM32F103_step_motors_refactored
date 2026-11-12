"""Compile the actual dispatcher helpers with a recording CAN boundary, no hardware."""
from pathlib import Path
import subprocess
import tempfile
root = Path(__file__).resolve().parents[1]
source = (root / 'App/src/tasks/task_dispatcher.c').read_text()
# Exercise the production encoder and handler, not a rewritten Python encoder.
helpers = source[source.index('static void SendScalarData'):source.index('void app_start_task_dispatcher')]
preamble = r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "can_protocol.h"
#include "tmc2209_driver.h"
#include "task_tmc2209_manager.h"
typedef struct { uint16_t cmd_code; uint8_t device_id; uint8_t data[5]; uint8_t data_len; } ParsedCanCommand_t;
static unsigned count, done, nack, snapshots;
static uint8_t frames[12][8];
bool TMCManager_GetTrace(uint8_t id, TMC2209_Trace_t *out) { memset(out,0,sizeof(*out)); out->words[0]=id==2?123:456; return id==2||id==6; }
static TMC_Diagnostics_t sample = {.state=3, .stage=2, .hal_status=3};
void CAN_SendData(uint16_t cmd, uint8_t seq, const uint8_t *data, uint8_t len) {
    (void)cmd; assert(len==6 && count<12); frames[count][0]=2; frames[count][1]=seq;
    memcpy(frames[count++]+2,data,6);
}
void CAN_SendDone(uint16_t cmd,uint8_t selector) { (void)cmd;assert(selector==0);++done; }
void CAN_SendNackPublic(uint16_t cmd,uint16_t error) { (void)cmd;nack=error; }
bool TMCManager_GetDiagnostics(uint8_t id,TMC_Diagnostics_t *out) {
    ++snapshots; if(id!=TMC_BENCH_MOTOR_ID && id!=TMC_BENCH_UART2_MOTOR_ID)return false;*out=sample;return true;
}
'''
main = r'''
static void clear(void) {count=done=nack=snapshots=0;}
int main(void) {
    ParsedCanCommand_t r={.cmd_code=0xF020,.data_len=5};
    assert(HandleEngineeringDiagnostics(&r));
    assert(count==2 && done==1 && !nack && !snapshots);
    const uint8_t cap[]={2,0,3,0xd1,1,0x83,0,0};assert(!memcmp(frames[0],cap,8));
    clear();r.data[4]=1;assert(HandleEngineeringDiagnostics(&r));assert(nack==6&&!count&&!done);
    clear();r.data[4]=0;r.cmd_code=0xF021;r.device_id=3;r.data[0]=0xd1;r.data[1]=2;
    TMC_Diagnostics_t before=sample;
    assert(HandleEngineeringDiagnostics(&r));
    assert(count==12 && done==1 && !nack && snapshots==1 && !memcmp(&before,&sample,sizeof(sample)));
    for(unsigned i=0;i<12;i++) {
        assert(frames[i][0]==2 && frames[i][1]==(i==11?0x8b:i));
        assert(frames[i][2]==i+1 && frames[i][3]==0);
        for(unsigned b=0;b<8;b++) { printf("%02x",frames[i][b]); }
        puts("");
    }
    assert(frames[0][4]==2);
    assert(frames[1][4]==3 && frames[3][4]==2 && frames[4][4]==3);
    clear();r.data[1]=1;HandleEngineeringDiagnostics(&r);assert(nack==2&&!count&&!done);
    clear();r.data[1]=0;HandleEngineeringDiagnostics(&r);assert(nack==2&&!count&&!done);
    clear();r.data[1]=2;r.data[2]=1;HandleEngineeringDiagnostics(&r);assert(nack==6&&!snapshots);
    clear();r.data[2]=0;r.device_id=6;HandleEngineeringDiagnostics(&r);assert(count==12&&done==1&&frames[0][4]==123&&frames[11][1]==0x8b);
    clear();r.data[1]=6;HandleEngineeringDiagnostics(&r);assert(count==12&&done==1&&frames[0][4]==0xc8&&frames[0][5]==1);
    clear();r.device_id=3;HandleEngineeringDiagnostics(&r);assert(count==12&&frames[0][4]==6&&done==1);
    clear();r.device_id=4;HandleEngineeringDiagnostics(&r);assert(nack==6&&!snapshots);
    clear();r.cmd_code=0x0103;assert(!HandleEngineeringDiagnostics(&r)&&!count&&!nack&&!done);
    clear();r.cmd_code=0xF020;r.data_len=4;HandleEngineeringDiagnostics(&r);assert(nack==6);
    clear();SendStatusMetric(0xF007,1,42);assert(count==1&&frames[0][1]==0x80&&frames[0][4]==42);
}
'''
with tempfile.TemporaryDirectory(prefix='motion-diag-c-') as tmp:
    path = Path(tmp)
    (path/'test.c').write_text(preamble+helpers+main)
    subprocess.run(['gcc','-std=c11','-Wall','-Wextra','-Werror','-I'+str(root/'tests/stubs'),'-I'+str(root/'App/inc'),
                    '-I'+str(root/'App/inc/tasks'),'-I'+str(root/'App/inc/drivers'),str(path/'test.c'),'-o',str(path/'test')],check=True)
    output = subprocess.check_output([str(path/'test')],text=True)
    # Synthetic frames from production C; not bench evidence.
    print(output,end='')
print('PASS: production diagnostics handler, sequencing, validation, read-only snapshot, F007 unchanged')
