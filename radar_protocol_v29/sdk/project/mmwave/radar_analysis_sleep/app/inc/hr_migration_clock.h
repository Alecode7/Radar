#ifndef HR_MIGRATION_CLOCK_H
#define HR_MIGRATION_CLOCK_H
#include <stdint.h>
#include <stdbool.h>
#define HR_CLOCK_FAULT_REASON 22u
typedef struct {uint32_t ms,os_ms,hi,lo,hz,fault;} HrClockSnapshot;
int hr_clock_init(void);
bool hr_clock_ready(void);
uint32_t hr_clock_now_ms(void);
uint64_t hr_clock_uptime_ms(void);
void hr_clock_snapshot(HrClockSnapshot *out);
#endif
