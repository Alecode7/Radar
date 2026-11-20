#ifndef SLEEP_HEART_RATE_DEVICE_H
#define SLEEP_HEART_RATE_DEVICE_H
#include <stdint.h>
#include <stdbool.h>
#include "heart_result_policy.h"

typedef struct {
    uint32_t sequence,generation,report_ms,computed_ms,last_confirm_ms,age_ms,cost_ms,max_cost_ms,dropped;
    uint32_t stack_free_bytes;
    uint16_t bpm,raw_bpm,reason,bin;
    uint8_t state,quality,support,harmonic_conflict,have_confirm;
} HeartRateDeviceResult;

/* Lifetime diagnostics survive result invalidation. COST/MAX in HRDEV now
 * describe every attempted estimate, including canceled work, not only the
 * last successfully published result. Times are elapsed wall time. */
typedef struct {
    uint32_t received,processed,estimates,completed,aborted,budget_aborts,overflows;
    uint32_t queue_peak,last_cost_ms,max_cost_ms,last_start_ms,last_finish_ms;
    uint32_t stack_free_bytes,phase,last_phase,spectral_estimates;
    uint32_t phase_ms[8],yield_ms;
    uint8_t busy;
} HeartRateDeviceDiagnostics;

int heart_rate_device_init(void);
void heart_rate_device_reset(void);
void heart_rate_device_start(void);
void heart_rate_device_stop(void);
/* Task context only, nonblocking queue submission from the radar callback. */
void heart_rate_device_submit(uint32_t frame,uint16_t bin,bool valid,bool active,
    bool in_bed,uint8_t breath,const int16_t iq[6]);
void heart_rate_device_get(HeartRateDeviceResult *result);
void heart_rate_device_get_display(HeartRateDisplay *result);
void heart_rate_device_get_diagnostics(HeartRateDeviceDiagnostics *diagnostics);
/* Called only after the existing HRRAW line completed, in that same producer
 * task, to avoid interleaving a worker's printf into the middle of HRRAW. */
void heart_rate_device_emit_pending(void);
uint32_t heart_rate_device_core_bytes(void);
#endif
