#ifndef SLEEP_ASYNC_LOG_IMPL_H
#define SLEEP_ASYNC_LOG_IMPL_H
#include "sleep_async_log.h"
#include "sleep_trace_ring.h"
#include "hal_uart.h"
#include "rsp_keepalive.h"
#include <stdarg.h>
#include <stdio.h>

/* One allocation from the existing heap. No extra HR core/history, no UART
 * reinitialization, no baud/pin/DMA changes. Only this task writes TX FIFO
 * after installing the existing SDK printf hook and routing Shell TX here. */
typedef struct {
    SleepTraceRing ring;
    uart_dev_t *uart;
    TaskHandle_t task;
    uint32_t format_refused;
    uint8_t formatting,ready,line_start;
    char format[1280];
} SleepTraceState;
static SleepTraceState *s_trace;
extern void printf_hook_install(int (*fn)(char));

int sleep_trace_ready(void) {return s_trace!=NULL && s_trace->ready;}
int sleep_trace_write(const char *data,size_t length) {
    UBaseType_t mask;int accepted;
    if(!sleep_trace_ready() || data==NULL || length==0u)return 0;
    mask=portSET_INTERRUPT_MASK_FROM_ISR();
    accepted=sleep_ring_write(&s_trace->ring,data,length);
    if(accepted)s_trace->line_start=(data[length-1u]=='\n' || data[length-1u]=='\r');
    portCLEAR_INTERRUPT_MASK_FROM_ISR(mask);
    return accepted?(int)length:0;
}
int sleep_trace_record(const char *data,size_t length) {
    UBaseType_t mask;size_t prefix;
    if(!sleep_trace_ready() || data==NULL || length==0u)return 0;
    mask=portSET_INTERRUPT_MASK_FROM_ISR();prefix=s_trace->line_start?0u:2u;
    if(length+prefix>SLEEP_TRACE_CAPACITY-s_trace->ring.used) {
        s_trace->ring.refused++;s_trace->ring.refused_bytes+=(uint32_t)(length+prefix);
        portCLEAR_INTERRUPT_MASK_FROM_ISR(mask);return 0;
    }
    if(prefix)sleep_ring_write(&s_trace->ring,"\r\n",2u);
    sleep_ring_write(&s_trace->ring,data,length);
    s_trace->line_start=(data[length-1u]=='\n' || data[length-1u]=='\r');
    portCLEAR_INTERRUPT_MASK_FROM_ISR(mask);return (int)length;
}
static int sleep_trace_char(char c) {sleep_trace_write(&c,1u);return 0;}
int sleep_trace_printf(const char *format,...) {
    UBaseType_t mask;va_list ap;int n,accepted=0;
    if(!sleep_trace_ready()) {va_start(ap,format);n=vprintf(format,ap);va_end(ap);return n;}
    mask=portSET_INTERRUPT_MASK_FROM_ISR();
    if(s_trace->formatting) {
        s_trace->format_refused++;portCLEAR_INTERRUPT_MASK_FROM_ISR(mask);return -1;
    }
    s_trace->formatting=1u;portCLEAR_INTERRUPT_MASK_FROM_ISR(mask);
    va_start(ap,format);n=vsnprintf(s_trace->format,sizeof(s_trace->format),format,ap);va_end(ap);
    if(n>0 && (size_t)n<sizeof(s_trace->format))accepted=sleep_trace_record(s_trace->format,(size_t)n);
    else if(n!=0) {
        mask=portSET_INTERRUPT_MASK_FROM_ISR();s_trace->format_refused++;portCLEAR_INTERRUPT_MASK_FROM_ISR(mask);
    }
    mask=portSET_INTERRUPT_MASK_FROM_ISR();s_trace->formatting=0u;portCLEAR_INTERRUPT_MASK_FROM_ISR(mask);
    return accepted==n?n:-1;
}
static void sleep_trace_task(void *unused) {
    (void)unused;
    for(;;) {
        /* RTOS time drives liveness even if sampling stops or HR clock fails.
         * No new task, timer allocation or serial peripheral ownership. */
        rsp_result_runtime_poll((uint32_t)(((uint64_t)xTaskGetTickCount()*1000u)/configTICK_RATE_HZ));
        unsigned batch=0u;UBaseType_t mask=portSET_INTERRUPT_MASK_FROM_ISR();
        /* No hardware wait loop: fill only currently available FIFO slots.
         * Bounded critical section, then sleep one tick so HR can progress. */
        while(batch<64u && s_trace->ring.used && (s_trace->uart->FSR&TFNF_Msk)!=0u) {
            s_trace->uart->TDR=sleep_ring_front(&s_trace->ring);
            sleep_ring_pop(&s_trace->ring);batch++;
        }
        portCLEAR_INTERRUPT_MASK_FROM_ISR(mask);
        vTaskDelay(1u);
    }
}
int sleep_trace_init(void) {
    HAL_Dev_t *dev;
    if(sleep_trace_ready())return 0;
    if(configTICK_RATE_HZ<1000u || CONFIG_PM || CONFIG_PRINTF_UART_NUM!=CONFIG_SHELL_UART_NUM)return -1;
    dev=HAL_DEV_Find(HAL_DEV_TYPE_UART,CONFIG_PRINTF_UART_NUM);
    if(dev==NULL || dev->reg==NULL)return -2;
    s_trace=pvPortMalloc(sizeof(*s_trace));if(s_trace==NULL)return -3;
    memset(s_trace,0,sizeof(*s_trace));s_trace->uart=(uart_dev_t *)dev->reg;s_trace->line_start=1u;
    if(xTaskCreate(sleep_trace_task,"sleep_tx",2048u/sizeof(StackType_t),NULL,5u,&s_trace->task)!=pdPASS) {
        vPortFree(s_trace);s_trace=NULL;return -4;
    }
    s_trace->ready=1u;printf_hook_install(sleep_trace_char);return 0;
}
void sleep_trace_get(SleepTraceStats *out) {
    UBaseType_t mask;
    memset(out,0,sizeof(*out));if(!sleep_trace_ready())return;
    mask=portSET_INTERRUPT_MASK_FROM_ISR();
    out->used=s_trace->ring.used;out->high=s_trace->ring.high;
    out->refused=s_trace->ring.refused;out->refused_bytes=s_trace->ring.refused_bytes;
    out->sent=s_trace->ring.sent;out->format_refused=s_trace->format_refused;
    portCLEAR_INTERRUPT_MASK_FROM_ISR(mask);
    out->stack_free=(uint32_t)uxTaskGetStackHighWaterMark(s_trace->task)*sizeof(StackType_t);
}
#endif
