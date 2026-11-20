#include "radar_control.h"
#include "mcu_rsp.h"
static RadarControl state;
static unsigned on,starts,stops;
static int fail;
static int start(void *ctx) {(void)ctx;if(on)return 0;starts++;if(!fail)on=1;return fail;}
static int stop(void *ctx) {(void)ctx;stops++;if(!fail)on=0;return fail;}
static unsigned running(void *ctx) {(void)ctx;return on;}
__declspec(dllexport) void ctl_reset(void) {memset(&state,0,sizeof(state));on=starts=stops=0;fail=0;}
__declspec(dllexport) void ctl_failure(int v) {fail=v;}
__declspec(dllexport) int ctl_apply(uint64_t op,unsigned action,unsigned *actual) {return radar_control_apply(&state,op,action,0,start,stop,running,0,actual);}
__declspec(dllexport) unsigned ctl_starts(void) {return starts;}
__declspec(dllexport) unsigned ctl_stops(void) {return stops;}
__declspec(dllexport) unsigned client_phase(const McuRsp *p) {return p->phase;}
__declspec(dllexport) unsigned client_pending(const McuRsp *p) {return p->pending;}
