#ifndef HR_MIGRATION_CLOCK_IMPL_H
#define HR_MIGRATION_CLOCK_IMPL_H
#include "hr_migration_clock.h"
#include "heart_clock_capture.h"
/* Algorithm-local monotonic clock only. No PMU/tick/peripheral writes. */
static uint32_t s_hr_clock_hz,s_hr_clock_last,s_hr_clock_fault;
static uint64_t s_hr_clock_cycles;
static bool s_hr_clock_have;
static uint32_t hr_raw_hi(void) {return csi_coret_get_valueh();}
static uint32_t hr_raw_lo(void) {return (uint32_t)csi_coret_get_value();}
bool hr_clock_ready(void) {return s_hr_clock_hz>=1000u && s_hr_clock_fault==0u;}
int hr_clock_init(void) {
    int32_t hz=HAL_BOARD_GetFreq(CLOCK_CURR_CPU);
    if(hz<1000) {s_hr_clock_fault=1u;return -1;}
    s_hr_clock_hz=(uint32_t)hz;s_hr_clock_fault=0u;s_hr_clock_have=false;
    hr_clock_now_ms();return hr_clock_ready()?0:-1;
}
uint32_t hr_clock_now_ms(void) {
    UBaseType_t mask=portSET_INTERRUPT_MASK_FROM_ISR();uint32_t hi,lo,ms;
    if(!hr_clock_ready()) {ms=s_hr_clock_last;portCLEAR_INTERRUPT_MASK_FROM_ISR(mask);return ms;}
    if(HAL_BOARD_GetFreq(CLOCK_CURR_CPU)!=(int32_t)s_hr_clock_hz) {
        s_hr_clock_fault=2u;ms=s_hr_clock_last;portCLEAR_INTERRUPT_MASK_FROM_ISR(mask);return ms;
    }
    hc_read(hr_raw_hi,hr_raw_lo,&hi,&lo);
    s_hr_clock_cycles=((uint64_t)hi<<32)|lo;
    /* Quotient/remainder avoids cycles*1000 overflow. Milliseconds wrap at
     * uint32 exactly as the original core contract. */
    ms=(uint32_t)((s_hr_clock_cycles/s_hr_clock_hz)*1000u+
        ((s_hr_clock_cycles%s_hr_clock_hz)*1000u)/s_hr_clock_hz);
    if(s_hr_clock_have && (int32_t)(ms-s_hr_clock_last)<0) {s_hr_clock_fault=3u;ms=s_hr_clock_last;}
    else {s_hr_clock_last=ms;s_hr_clock_have=true;}
    portCLEAR_INTERRUPT_MASK_FROM_ISR(mask);return ms;
}
void hr_clock_snapshot(HrClockSnapshot *out) {
    UBaseType_t mask;
    hr_clock_now_ms();mask=portSET_INTERRUPT_MASK_FROM_ISR();
    out->ms=s_hr_clock_last;out->hi=(uint32_t)(s_hr_clock_cycles>>32);out->lo=(uint32_t)s_hr_clock_cycles;
    out->hz=s_hr_clock_hz;out->fault=s_hr_clock_fault;
    out->os_ms=(uint32_t)(((uint64_t)xTaskGetTickCount()*1000u)/configTICK_RATE_HZ);
    portCLEAR_INTERRUPT_MASK_FROM_ISR(mask);
}
uint64_t hr_clock_uptime_ms(void) {
    UBaseType_t mask;uint64_t value;
    hr_clock_now_ms();mask=portSET_INTERRUPT_MASK_FROM_ISR();
    value=s_hr_clock_hz?((s_hr_clock_cycles/s_hr_clock_hz)*1000u+
        ((s_hr_clock_cycles%s_hr_clock_hz)*1000u)/s_hr_clock_hz):0u;
    portCLEAR_INTERRUPT_MASK_FROM_ISR(mask);return value;
}
#endif
