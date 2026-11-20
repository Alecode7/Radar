#ifndef SLEEP_TRACE_RING_H
#define SLEEP_TRACE_RING_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#define SLEEP_TRACE_CAPACITY 8192u
/* Caller serializes short accesses. Writes either commit in full or not at all. */
typedef struct {
    uint32_t head,tail,used,high,refused,refused_bytes,sent;
    char data[SLEEP_TRACE_CAPACITY];
} SleepTraceRing;
static inline int sleep_ring_write(SleepTraceRing *r,const char *data,size_t n) {
    size_t first;
    if(n>SLEEP_TRACE_CAPACITY-r->used) {
        r->refused++;r->refused_bytes+=(uint32_t)n;return 0;
    }
    first=SLEEP_TRACE_CAPACITY-r->head;if(first>n)first=n;
    memcpy(r->data+r->head,data,first);memcpy(r->data,data+first,n-first);
    r->head=(r->head+(uint32_t)n)&(SLEEP_TRACE_CAPACITY-1u);r->used+=(uint32_t)n;
    if(r->used>r->high)r->high=r->used;
    return 1;
}
static inline unsigned char sleep_ring_front(const SleepTraceRing *r) {return (unsigned char)r->data[r->tail];}
static inline void sleep_ring_pop(SleepTraceRing *r) {
    r->tail=(r->tail+1u)&(SLEEP_TRACE_CAPACITY-1u);r->used--;r->sent++;
}
#endif
