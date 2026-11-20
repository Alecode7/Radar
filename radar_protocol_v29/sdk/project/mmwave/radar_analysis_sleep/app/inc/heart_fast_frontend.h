#ifndef HEART_FAST_FRONTEND_H
#define HEART_FAST_FRONTEND_H
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>

#define HF_BINS 3u
#define HF_ANTS 3u
#define HF_CHANNELS 9u
#define HF_WARM_SAMPLES 100u
#define HF_PI 3.14159265358979323846f

/* Direct-form-II-transposed, [b0,b1,b2,a1,a2], subtract a feedback terms.
 * fs=20Hz: Butterworth HP2 at0.6Hz, elliptic LP5 pass2Hz/ripple0.5dB,
 * at least40dB above2.5Hz. Coefficients and float implementation are tested
 * against an independent scipy SOS reference, not inferred from their names. */
static const float hf_sos[4][5]={
    {0.8751830924381346f,-1.7503661848762693f,0.8751830924381346f,-1.734725768809275f,0.7660066009432638f},
    {0.016177237496204654f,0.016177237496204654f,0.f,-0.73503515153861f,0.f},
    {1.f,-0.9130757085835723f,1.f,-1.4960391063787157f,0.7125669884862225f},
    {1.f,-1.3843962767978504f,1.f,-1.5480749834779737f,0.9254149438567125f}
};
typedef struct { float previous,unwrapped,z[4][2];uint16_t good;uint8_t initialized; } HfChannel;
typedef struct { uint16_t bin;uint8_t present;HfChannel ant[HF_ANTS]; } HfBin;
typedef struct { HfBin bins[HF_BINS];uint32_t last_ms,gaps;uint8_t have_time; } HfFrontend;

static inline void hf_reset(HfFrontend *front) { memset(front,0,sizeof(*front)); }
static inline float hf_filter(HfChannel *c,float x) {
    unsigned s;
    for(s=0;s<4u;s++) {
        const float *b=hf_sos[s];float y=b[0]*x+c->z[s][0];
        c->z[s][0]=b[1]*x-b[3]*y+c->z[s][1];c->z[s][1]=b[2]*x-b[4]*y;x=y;
    }
    return x;
}
static inline bool hf_slow_due(uint32_t sequence) { return sequence%4u==0u; }

/* ms is callback time, not an ADC-edge timestamp. A timing discontinuity
 * invalidates filter state; it cannot silently be treated as a clean20Hz run.
 * Raw payload is retained separately even while filtered output warms up. */
static inline uint16_t hf_push(HfFrontend *front,uint32_t ms,uint16_t first,
                              const int16_t iq[18],int32_t phase_mrad[9]) {
    unsigned i,a,j;uint16_t mask=0u;bool gap=false;
    if(front->have_time) { uint32_t delta=ms-front->last_ms;gap=delta<25u || delta>85u; }
    if(gap) {
        uint32_t gaps=front->gaps+1u;hf_reset(front);front->gaps=gaps;
    }
    front->last_ms=ms;front->have_time=1u;
    for(i=0;i<HF_BINS;i++) {
        uint16_t bin=(uint16_t)(first+i);HfBin *slot=NULL;
        for(j=0;j<HF_BINS;j++) if(front->bins[j].present && front->bins[j].bin==bin) { slot=&front->bins[j];break; }
        if(slot==NULL) {
            for(j=0;j<HF_BINS;j++) if(!front->bins[j].present || front->bins[j].bin<first || front->bins[j].bin>=first+HF_BINS) {
                slot=&front->bins[j];memset(slot,0,sizeof(*slot));slot->bin=bin;slot->present=1u;break;
            }
        }
        if(slot==NULL) return 0u;
        for(a=0;a<HF_ANTS;a++) {
            unsigned index=i*HF_ANTS+a;HfChannel *c=&slot->ant[a];
            int32_t real=iq[index*2u],imag=iq[index*2u+1u];float angle,y,delta;
            phase_mrad[index]=0;
            if((int64_t)real*real+(int64_t)imag*imag<64) { memset(c,0,sizeof(*c));continue; }
            angle=atan2f((float)imag,(float)real);
            if(!c->initialized) { c->previous=angle;c->unwrapped=0.f;c->initialized=1u; }
            delta=angle-c->previous;
            if(delta>HF_PI) delta-=2.f*HF_PI;else if(delta< -HF_PI) delta+=2.f*HF_PI;
            c->unwrapped+=delta;c->previous=angle;y=hf_filter(c,c->unwrapped);
            if(!isfinite(y) || fabsf(y)>1000.f || !isfinite(c->unwrapped)) { memset(c,0,sizeof(*c));continue; }
            if(c->good<HF_WARM_SAMPLES)c->good++;
            if(c->good>=HF_WARM_SAMPLES) {
                mask|=(uint16_t)(1u<<index);phase_mrad[index]=(int32_t)roundf(y*1000.f);
            }
        }
    }
    return mask;
}
#endif
