#ifndef RADAR_CONTROL_H
#define RADAR_CONTROL_H
#include <stdint.h>
#include <string.h>
typedef struct {uint32_t prefix,highest,signature;uint64_t last;unsigned action,running;int result;} RadarControl;
typedef int (*RadarControlAction)(void *);
typedef unsigned (*RadarControlRunning)(void *);
/* Status establishes the current controller prefix. Sequence never rolls
 * within a prefix. Replays cannot restart sampling or re-clear its history. */
static int radar_control_apply(RadarControl *s,uint64_t op,unsigned action,uint32_t signature,
    RadarControlAction start,RadarControlAction stop,RadarControlRunning running,void *context,unsigned *state) {
    uint32_t prefix=(uint32_t)(op>>32),sequence=(uint32_t)op;int ret=0;
    if(!prefix || !sequence || action>3u) {*state=running(context);return -100;}
    if(s->last==op) {
        if(s->action!=action || s->signature!=signature) {*state=running(context);return -102;}
        *state=s->running;return s->result;
    }
    if(prefix!=s->prefix) {
        if(action!=2u) {*state=running(context);return -101;}
        s->prefix=prefix;s->highest=0;
    }
    if(sequence<=s->highest) {*state=running(context);return -103;}
    s->highest=sequence;
    if(action==1u)ret=start(context); /* callback verifies config, then starts only if stopped */
    else if(action==0u && running(context))ret=stop(context);
    else if(action==3u)ret=start(context); /* caller supplies install callback */
    *state=running(context);s->last=op;s->action=action;s->signature=signature;s->result=ret;s->running=*state;return ret;
}
#endif
