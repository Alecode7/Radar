#ifndef SLEEP_ASYNC_LOG_H
#define SLEEP_ASYNC_LOG_H
#include <stdint.h>
#include <stddef.h>
#define SLEEP_TRACE_PROFILE 1
#define SLEEP_DATA_STACK_BYTES 4096u
typedef struct {
    uint32_t used,high,refused,refused_bytes,sent,format_refused,stack_free;
} SleepTraceStats;
int sleep_trace_init(void);
int sleep_trace_ready(void);
int sleep_trace_write(const char *data,size_t length);
int sleep_trace_record(const char *data,size_t length);
int sleep_trace_printf(const char *format,...);
void sleep_trace_get(SleepTraceStats *out);
#endif
