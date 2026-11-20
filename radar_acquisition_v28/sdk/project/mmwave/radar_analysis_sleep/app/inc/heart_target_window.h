#ifndef HEART_TARGET_WINDOW_H
#define HEART_TARGET_WINDOW_H
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#define HT_STABLE_UPDATES 10u
#define HT_MISSING_UPDATES 5u
#define HT_ENTER 1u
#define HT_EXIT 2u
#define HT_MOVE 4u
#define HT_LOCK 8u
#define HT_INVALID 16u
typedef struct {
    uint16_t first,target;
    uint8_t streak,missing,locked,bed,have_window;
    uint32_t epoch;
} HeartTargetWindow;
void ht_init(HeartTargetWindow *s);
unsigned ht_update(HeartTargetWindow *s,bool bed,bool active,bool fresh,uint16_t target,uint16_t ranges);
#ifdef HF_AUX_IMPLEMENTATION
void ht_init(HeartTargetWindow *s) {
    memset(s,0,sizeof(*s));s->first=13u;s->target=UINT16_MAX;
}
/* Called once per original 200ms detector update, using THIS update's fresh
 * target, never the sticky last-valid target. Adjacent selected bins within
 * the same three-bin window do not splice physical channel histories. */
unsigned ht_update(HeartTargetWindow *s,bool bed,bool active,
                                     bool fresh,uint16_t target,uint16_t ranges) {
    unsigned event=0u;
    if(s->bed!=(uint8_t)bed) {
        event|=bed?HT_ENTER:HT_EXIT;s->bed=(uint8_t)bed;
        s->locked=0u;s->streak=0u;s->missing=0u;s->have_window=0u;
    }
    if(!fresh || ranges<3u || target>=ranges) {
        s->streak=0u;
        if(s->missing<HT_MISSING_UPDATES)s->missing++;
        if(s->locked && s->missing>=HT_MISSING_UPDATES) {s->locked=0u;event|=HT_INVALID;}
    } else {
        s->target=target;s->missing=0u;
        if(!s->have_window || target<s->first || (uint32_t)target>=(uint32_t)s->first+3u) {
            uint16_t first=target?target-1u:0u;
            if((uint32_t)first+3u>ranges)first=(uint16_t)(ranges-3u);
            s->first=first;s->have_window=1u;s->locked=0u;s->streak=0u;event|=HT_MOVE;
        }
        if(!bed || active) s->streak=0u;
        else if(!s->locked) {
            if(s->streak<HT_STABLE_UPDATES)s->streak++;
            if(s->streak>=HT_STABLE_UPDATES) {s->locked=1u;event|=HT_LOCK;}
        }
    }
    if(!bed)s->locked=0u;
    if(event)s->epoch++;
    return event;
}
#endif
#endif
