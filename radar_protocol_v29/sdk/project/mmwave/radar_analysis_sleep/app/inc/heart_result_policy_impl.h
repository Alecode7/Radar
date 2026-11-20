#ifndef HEART_RESULT_POLICY_IMPL_H
#define HEART_RESULT_POLICY_IMPL_H
#include "heart_result_policy.h"
unsigned hr_home_round(double value) {
    double base=floor(value),part=value-base;unsigned n=(unsigned)base;
    return n+(part>0.5 || (part==0.5 && (n&1u)));
}
void hr_home_clear_candidate(HrHomeCandidate *s) {memset(s,0,sizeof(*s));}
unsigned hr_home_candidate(HrHomeCandidate *s,const HrResult *r,uint32_t generation,
    uint32_t ms,unsigned breath,bool core_public) {
    bool eligible=!core_public && r->candidate_bpm>=48.0 && r->candidate_bpm<=120.0 &&
        r->support==3u && r->bin_share>=0.8 && r->cluster_score>=3.5 &&
        !r->harmonic_conflict && r->reason==HR_CONFIRMING;
    if(!eligible) {hr_home_clear_candidate(s);return 0u;}
    bool same=s->have && s->generation==generation && ms-s->ms>0u && ms-s->ms<=7000u &&
        abs(r->target_bin-s->bin)<=1 && fabs(r->candidate_bpm-s->mean)<=3.0;
    s->count=(uint8_t)(same?(s->count<6u?s->count+1u:6u):1u);
    s->mean=same?s->mean*0.5+r->candidate_bpm*0.5:r->candidate_bpm;
    s->generation=generation;s->ms=ms;s->bin=r->target_bin;s->have=true;
    return s->count>=(breath<8u?3u:2u)?hr_home_round(s->mean):0u;
}
void hr_home_clear_cache(HrHomeCache *s,uint32_t generation) {
    memset(s,0,sizeof(*s));s->value.generation=generation;s->value.age_ms=UINT32_MAX;
}
void hr_home_accept(HrHomeCache *s,uint32_t generation,uint32_t now,uint32_t confirmed,
    unsigned bpm,unsigned kind,unsigned quality,unsigned harmonic,int bin) {
    s->value.sequence++;
    if(!s->value.sequence)s->value.sequence=1u;
    s->value.generation=generation;s->value.confirmed_ms=confirmed;
    s->value.bpm=(uint16_t)bpm;s->value.bin=bin<0?UINT16_MAX:(uint16_t)bin;
    s->value.kind=(uint8_t)kind;s->value.quality=(uint8_t)quality;s->value.harmonic=(uint8_t)harmonic;
    s->age=(uint32_t)(now-confirmed);s->age_at=now;s->have=true;
}
void hr_home_get(HrHomeCache *s,uint32_t now,HeartRateDisplay *out) {
    if(s->have) {s->age+=(uint32_t)(now-s->age_at);s->age_at=now;}
    *out=s->value;out->report_ms=now;
    out->age_ms=s->have?(s->age>=UINT32_MAX?UINT32_MAX-1u:(uint32_t)s->age):UINT32_MAX;
    out->state=(uint8_t)(s->have?(s->age<=10000u?1u:2u):0u);
}
#endif
