#include <stdint.h>
#include <string.h>
#include "product_runtime.h"
static uint32_t clock_ms;
static unsigned lock_count[3],order_errors,write_limit=256;
static uint8_t host_input[8192],host_output[16384];
static size_t input_used,output_used,source_used;
static char source_output[8192];
static wm_device_t source_device={3};
static ShTransport source_transport;
static uint32_t report_sequence;
extern void board_reset(void);
uint32_t wm_os_internal_get_time_ms(void) {return clock_ms;}
SemaphoreHandle_t xSemaphoreCreateMutex(void) {return (SemaphoreHandle_t)(uintptr_t)2;}
int xSemaphoreTake(SemaphoreHandle_t sem,uint32_t delay) {
    unsigned id=(unsigned)(uintptr_t)sem;(void)delay;
    if(id==1 && lock_count[2])order_errors++;
    if(id>2 || !id || lock_count[id]) {order_errors++;return 0;}
    lock_count[id]++;return 1;
}
int xSemaphoreGive(SemaphoreHandle_t sem) {
    unsigned id=(unsigned)(uintptr_t)sem;
    if(id>2 || !id || !lock_count[id]) {order_errors++;return 0;}
    lock_count[id]--;return 1;
}
void vTaskDelay(uint32_t n) {clock_ms+=n;}
int xTaskCreate(void (*entry)(void *),const char *name,uint32_t words,void *arg,unsigned priority,TaskHandle_t *handle) {
    (void)entry;(void)name;(void)words;(void)arg;(void)priority;(void)handle;return 1;
}
int wm_drv_uart_read(wm_device_t *dev,void *out,uint32_t capacity,uint32_t timeout) {
    size_t n=input_used<capacity?input_used:capacity;(void)dev;(void)timeout;
    memcpy(out,host_input,n);memmove(host_input,host_input+n,input_used-n);input_used-=n;return (int)n;
}
int wm_drv_uart_write(wm_device_t *dev,const uint8_t *data,uint32_t count) {
    size_t n=count;
    if(dev==&source_device) {
        if(source_used+n>=sizeof(source_output))return -1;
        memcpy(source_output+source_used,data,n);source_used+=n;source_output[source_used]=0;return (int)n;
    }
    if(n>write_limit)n=write_limit;
    if(output_used+n>sizeof(host_output))return -1;
    memcpy(host_output+output_used,data,n);output_used+=n;return (int)n;
}
/* Compile the real runtime, including its parser and synchronization, against
 * memory-only UART/RTOS hooks. No production source logic is copied. */
#include "../App/product_runtime.c"
__declspec(dllexport) int rt_reset(void) {
    clock_ms=0;input_used=output_used=source_used=0;order_errors=0;write_limit=256;report_sequence=0;
    memset(lock_count,0,sizeof(lock_count));known_monitor=last_core_sequence=last_status_poll=0;known_control=known_monitoring=0;
    board_reset();sh_init(&source_transport);return product_runtime_init(&source_device,(SemaphoreHandle_t)(uintptr_t)1,&source_transport);
}
__declspec(dllexport) void rt_time(uint32_t value) {clock_ms=value;}
__declspec(dllexport) void rt_step(void) {service_once();}
__declspec(dllexport) void rt_feed(const uint8_t *p,size_t n) {if(n>sizeof(host_input)-input_used)return;memcpy(host_input+input_used,p,n);input_used+=n;}
__declspec(dllexport) size_t rt_output(uint8_t *p) {size_t n=output_used;memcpy(p,host_output,n);output_used=0;return n;}
__declspec(dllexport) const char *rt_source(void) {return source_output;}
__declspec(dllexport) void rt_clear_source(void) {source_used=0;source_output[0]=0;}
__declspec(dllexport) void rt_line(const char *text,unsigned kind) {
    int accepted=sh_line(&source_transport,text,strlen(text),kind,clock_ms);product_runtime_line(text,accepted);
}
__declspec(dllexport) unsigned rt_paused(void) {return source_transport.intake.status.paused;}
__declspec(dllexport) unsigned rt_lock_errors(void) {return order_errors+lock_count[1]+lock_count[2];}
__declspec(dllexport) void rt_limit(unsigned limit) {write_limit=limit;}
__declspec(dllexport) void rt_heart(int bpm,unsigned old) {
    ShReport r;memset(&r,0,sizeof(r));r.sequence=++report_sequence;r.epoch=source_transport.intake.status.epoch-(old?1u:0u);
    r.mode=source_transport.mode;r.raw.bpm=(int16_t)bpm;r.raw.quality=3;r.raw.fresh=1;
    product_runtime_heart(&r);
}
