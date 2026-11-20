/* Only the adapter's printf is redirected; keep the host CRT declarations. */
#undef printf
#undef _NO_CRT_STDIO_INLINE
#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include "v6_heart_device.h"
static uint32_t clock_ms,line_count;
static int clock_fault;
static char last_line[768];
int hr_clock_init(void) {return clock_fault?-4:0;}
bool hr_clock_ready(void) {return clock_fault==0;}
uint32_t hr_clock_now_ms(void) {return clock_ms;}
size_t xPortGetFreeHeapSize(void) {return 100000u;}
size_t xPortGetMinimumEverFreeHeapSize(void) {return 99000u;}
int stream_test_printf(const char *format,...) {
    va_list args;int n;
    va_start(args,format);n=vsnprintf(last_line,sizeof(last_line),format,args);va_end(args);
    if(n>=0)line_count++;
    return n;
}
__declspec(dllexport) void test_clock(uint32_t ms,int fault) {clock_ms=ms;clock_fault=fault;}
__declspec(dllexport) uint32_t test_count(void) {return line_count;}
__declspec(dllexport) const char *test_line(void) {return last_line;}
__declspec(dllexport) int test_init(void) {return v6_heart_init();}
__declspec(dllexport) void test_start(void) {v6_heart_start();}
__declspec(dllexport) void test_stop(void) {v6_heart_stop();}
__declspec(dllexport) void test_reset(void) {v6_heart_reset();}
__declspec(dllexport) void test_status(bool bed,uint8_t breath) {v6_heart_observe_status(bed,breath);}
__declspec(dllexport) void test_submit(uint32_t frame,uint16_t bin,bool active,const int16_t *iq) {v6_heart_submit(frame,bin,active,iq);}
__declspec(dllexport) void test_emit(void) {v6_heart_emit();}
