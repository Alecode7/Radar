#ifndef HEART_RESULT_POLICY_H
#define HEART_RESULT_POLICY_H
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "heart_rate_core.h"

typedef struct {
    uint32_t sequence,generation,report_ms,confirmed_ms,age_ms;
    uint16_t bpm,bin;
    uint8_t state,kind,quality,harmonic;
} HeartRateDisplay;
/* kind:0 none,1 confirmed,2 candidate; state:0 none,1 <=10s,2 display held. */
typedef struct {HeartRateDisplay value;uint64_t age;uint32_t age_at;bool have;} HrHomeCache;
typedef struct {
    double mean;uint32_t generation,ms;int16_t bin;uint8_t count;bool have;
} HrHomeCandidate;

unsigned hr_home_round(double value);
void hr_home_clear_candidate(HrHomeCandidate *s);
unsigned hr_home_candidate(HrHomeCandidate *s,const HrResult *r,uint32_t generation,uint32_t ms,unsigned breath,bool core_public);
void hr_home_clear_cache(HrHomeCache *s,uint32_t generation);
void hr_home_accept(HrHomeCache *s,uint32_t generation,uint32_t now,uint32_t confirmed,unsigned bpm,unsigned kind,unsigned quality,unsigned harmonic,int bin);
void hr_home_get(HrHomeCache *s,uint32_t now,HeartRateDisplay *out);
#endif
