/* C2: frozen V6/Python decisions with binary64 frequency rotation and periodic
 * literal re-anchors. Sample accumulation order and decision rules are kept;
 * floating-point roundoff is measured, not claimed bit-identical.
 * no float conversion, half-grid normalization, H4 or display candidate policy.
 * Oracle SHA256 abf33867d87c35a74de68a39f405306ac32d29db8db44b5eb4ba7b61212e9ceb.
 * Checkpoints cancel actual discontinuities/budget expiry, never relax gates. */
#include "heart_rate_core.h"
#include <math.h>
#include <string.h>

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O2")
#endif
#define HR_PI 3.1415926535897932384626433832795
#ifndef HR_DFT_DIRECT
#define HR_DFT_DIRECT 1
#endif
#ifndef HR_SPECTRAL_F32
#define HR_SPECTRAL_F32 0
#endif
#if HR_DFT_DIRECT != 1 || HR_SPECTRAL_F32 != 0
#error "C2 supports the binary64 spectrum paths only; other numeric backends are disabled"
#endif
_Static_assert(sizeof(((HrCore *)0)->scratch.spectral_f32)<=HR_FREQ_COUNT*sizeof(double),
               "Float input workspace must not overlap double power output");
typedef struct { double bpm,score; } Peak;
typedef struct { double bpm,score,spread,rank; int support; } Consensus;
static double minimum(double a,double b) { return a<b?a:b; }
static double maximum(double a,double b) { return a>b?a:b; }
static uint32_t elapsed(uint32_t now,uint32_t then) { return now-then; }
/* Reproduce the reference's seconds conversion/subtraction at boundaries.
 * Keep the unsigned clock for wrap detection and device freshness. */
static double seconds_between(uint32_t now,uint32_t then) {
    double result=(double)now/1000.0-(double)then/1000.0;
    return now>=then?result:result+4294967.296;
}
typedef struct { double hi,lo; } Sum;
static void sum_add(Sum *sum,double value) {
    double total=sum->hi+value;
    sum->lo+=fabs(sum->hi)>=fabs(value)?(sum->hi-total)+value:(value-total)+sum->hi;
    sum->hi=total;
}
static double sum_value(const Sum *sum) { return sum->hi+sum->lo; }
static int round_even(double value) {
    double base=floor(value),fraction=value-base;
    int integer=(int)base;
    return integer+(fraction>0.5 || (fraction==0.5 && (integer&1)));
}
static void sort_values(double *values,unsigned count) {
    unsigned i,j;
    for(i=1;i<count;i++) {
        double value=values[i];j=i;
        while(j>0 && values[j-1]>value) { values[j]=values[j-1];j--; }
        values[j]=value;
    }
}
static double median(double *values,unsigned count) {
    sort_values(values,count);
    return count&1u?values[count/2u]:(values[count/2u-1u]+values[count/2u])*0.5;
}
static HrSample *sample_at(HrCore *c,unsigned index) { return &c->samples[(c->head+index)%HR_SAMPLE_CAP]; }
static void clear_histories(HrCore *c) {
    c->head=0;c->count=0;c->have_smoothed=0;c->have_accepted=0;
    c->candidate_count=0;c->breath_count=0;c->consistent=0;
}
size_t hr_core_size(void) { return sizeof(HrCore); }
size_t hr_result_size(void) { return sizeof(HrResult); }
size_t hr_workspace_size(void) { return sizeof(HrWorkspace); }
void hr_core_attach(HrCore *c,HrWorkspace *w) {
    if(c!=NULL) c->workspace=w;
    if(w!=NULL) { memset(&w->stats,0,sizeof(w->stats));w->frequency_rotation=HR_DEFAULT_FREQ_ROTATION; }
}
int hr_core_set_spectrum_mode(HrCore *c,unsigned rotation) {
    if(c==NULL || c->workspace==NULL || rotation>1u)return -1;
#if !HR_FREQ_RECURSION
    if(rotation)return -1;
#endif
    c->workspace->frequency_rotation=rotation;return 0;
}
unsigned hr_core_spectrum_mode(const HrCore *c) {
    return c!=NULL && c->workspace!=NULL?c->workspace->frequency_rotation:0u;
}
void hr_core_work_stats(const HrCore *c,HrWorkStats *out) {
    if(out!=NULL) {
        if(c!=NULL && c->workspace!=NULL) *out=c->workspace->stats;
        else memset(out,0,sizeof(*out));
    }
}
void hr_core_init(HrCore *c) { if(c!=NULL) memset(c,0,sizeof(*c)); }
static void reset_preserving_workspace(HrCore *c) {
    HrWorkspace *w=c->workspace;unsigned mode=hr_core_spectrum_mode(c);
    hr_core_init(c);hr_core_attach(c,w);(void)hr_core_set_spectrum_mode(c,mode);
}

int hr_core_push(HrCore *c,uint32_t ms,uint32_t frame,uint16_t bin,
    uint8_t flags,uint8_t fs,const int16_t iq[6]) {
    HrSample *s;
    if(c==NULL || fs!=5u || iq==NULL || bin>32767u || (flags&~7u)!=0u) return -1;
    if(c->have_frame && frame<c->last_frame) reset_preserving_workspace(c);
    if(c->have_frame && elapsed(ms,c->last_ms)>UINT32_C(0x7fffffff)) {
        reset_preserving_workspace(c);return -3;
    }
    if(flags&HR_IN_BED) { c->have_out_since=0;c->out_reset=0; }
    else if(!c->have_out_since) { c->have_out_since=1;c->out_since_ms=ms; }
    else if(!c->out_reset && seconds_between(ms,c->out_since_ms)>=2.0) { clear_histories(c);c->out_reset=1; }
    while(c->count && seconds_between(ms,sample_at(c,0)->ms)>75.0) {
        c->head=(uint16_t)((c->head+1u)%HR_SAMPLE_CAP);c->count--;
    }
    if(c->count==HR_SAMPLE_CAP) { reset_preserving_workspace(c);return -2; }
    s=sample_at(c,c->count++);s->ms=ms;s->frame=frame;s->bin=bin;s->flags=flags;s->reserved=0;
    memcpy(s->iq,iq,sizeof(s->iq));c->last_frame=frame;c->last_ms=ms;c->have_frame=1;
    return 0;
}

static void observe_breath(HrCore *c,uint8_t rate,uint32_t now) {
    if(rate>=8u && rate<=35u) {
        if(c->breath_count==24u) { memmove(c->breaths,c->breaths+1,23u*sizeof(c->breaths[0]));c->breath_count--; }
        c->breaths[c->breath_count].rate=rate;c->breaths[c->breath_count].ms=now;c->breath_count++;
    }
    while(c->breath_count && seconds_between(now,c->breaths[0].ms)>75.0) {
        memmove(c->breaths,c->breaths+1,(--c->breath_count)*sizeof(c->breaths[0]));
    }
}
static int median_breath(HrCore *c,uint32_t now) {
    double rates[24];unsigned n=0,i;
    for(i=0;i<c->breath_count;i++) if(seconds_between(now,c->breaths[i].ms)<=60.0) rates[n++]=c->breaths[i].rate;
    return n?round_even(median(rates,n)):0;
}
static int nearest_third(HrCore *c,double bpm,int breath) {
    double rates[25],ordered[25],best=0,best_distance=1e20,lower,upper;
    unsigned i,n=0,lo,hi;
    if(breath<8) return 0;
    for(i=0;i<c->breath_count;i++) if(c->breaths[i].rate>=8u) rates[n++]=c->breaths[i].rate;
    if(!n) rates[n++]=breath;
    memcpy(ordered,rates,n*sizeof(double));sort_values(ordered,n);
    lo=(unsigned)((n-1u)*0.15);hi=(unsigned)((n-1u)*0.85);lower=ordered[lo];upper=ordered[hi];
    for(i=0;i<n;i++) {
        double harmonic=rates[i]*3.0,distance=fabs(bpm-harmonic);
        if(rates[i]<lower || rates[i]>upper) continue;
        if(distance<best_distance) { best_distance=distance;best=harmonic; }
    }
    if(fabs(bpm-breath*3.0)<best_distance) best=breath*3.0;
    return round_even(best);
}
static int high_harmonic(HrCore *c,double bpm,int breath,int *harmonic) {
    unsigned i,j,n=0;int rates[25],best_order=0,best_h=0;
    double distance,best_distance=1e20;
    *harmonic=0;if(bpm<85.0 || breath<8) return 0;
    for(i=0;i<c->breath_count;i++) if(c->breaths[i].rate>=8u && c->breaths[i].rate<=35u) rates[n++]=c->breaths[i].rate;
    for(i=0;i<n && rates[i]!=breath;i++) { }
    if(i==n) rates[n++]=breath;
    for(i=0;i<n;i++) for(j=4;j<=6;j++) {
        int h=rates[i]*(int)j;distance=fabs(bpm-h);
        if(distance<best_distance || (distance==best_distance && ((int)j<best_order || ((int)j==best_order && h<best_h)))) {
            best_distance=distance;best_order=(int)j;best_h=h;
        }
    }
    if(best_distance<=4.0) { *harmonic=best_h;return best_order; }
    return 0;
}
static int harmonic_conflict(HrCore *c,double bpm,int breath) {
    unsigned i,n=0;double xmean=0,ymean=0,lo=1e9,hi=-1e9;
    double numerator=0,xe=0,ye=0,correlation,spread;
    if(breath<8 || fabs(bpm-breath*3.0)>6.0 || !nearest_third(c,bpm,breath)) return 0;
    for(i=0;i<c->candidate_count;i++) if(c->candidates[i].breath>=8u) {
        double x=c->candidates[i].bpm,y=c->candidates[i].breath*3.0;
        xmean+=x;ymean+=y;lo=minimum(lo,y);hi=maximum(hi,y);n++;
    }
    if(n<10u) return 1;
    xmean/=n;ymean/=n;
    for(i=0;i<c->candidate_count;i++) if(c->candidates[i].breath>=8u) {
        double x=c->candidates[i].bpm-xmean,y=c->candidates[i].breath*3.0-ymean;
        numerator+=x*y;xe+=x*x;ye+=y*y;
    }
    correlation=xe<=1e-9 && ye>1e-9?0.0:(xe*ye>0.0?numerator/sqrt(xe*ye):1.0);
    spread=sqrt(xe/n);
    return !(hi-lo>=9.0 && fabs(correlation)<0.20 && spread<=2.5);
}
static unsigned trailing(HrCore *c,double center) {
    unsigned n=0,i=c->candidate_count;
    while(i>0) { i--;if(fabs(c->candidates[i].bpm-center)>6.0) break;n++; }
    return n;
}
static int confirmed(HrCore *c,unsigned required,double *value) {
    double values[20],center;unsigned i;
    if(c->candidate_count<required) return 0;
    for(i=0;i<required;i++) values[i]=c->candidates[c->candidate_count-required+i].bpm;
    center=median(values,required);
    for(i=0;i<required;i++) if(fabs(values[i]-center)>6.0) return 0;
    *value=center;return 1;
}
static int held(HrCore *c,uint32_t now) {
    return c->have_smoothed && c->have_accepted && seconds_between(now,c->last_accepted_ms)<=45.0?round_even(c->smoothed):-1;
}
static int strong(unsigned support,double score,double spread,double share) {
    return support==3u && score>=3.5 && spread<=3.0 && share>=0.80;
}
static double anchor(HrCore *c) {
    double recent[6],selected[6],center;unsigned start,i,n=0,m=0;
    if(c->have_smoothed && c->smoothed>=58.0 && c->smoothed<=95.0) return c->smoothed;
    start=c->candidate_count>6u?c->candidate_count-6u:0u;
    for(i=start;i<c->candidate_count;i++) if(c->candidates[i].bpm>=60.0 && c->candidates[i].bpm<=90.0) recent[n++]=c->candidates[i].bpm;
    if(n<3u) return -1.0;
    center=median(recent,n);
    for(i=0;i<n;i++) if(fabs(recent[i]-center)<=6.0) selected[m++]=recent[i];
    return m>=3u?median(selected,m):-1.0;
}
static Consensus consensus(HrCore *c,Peak peaks[3][6],unsigned sizes[3]) {
    Consensus candidates[18],none={-1.0,0.0,999.0,0.0,0},value,temp,best;
    unsigned n=0,a,p,b,q,k,i,j;double continuity,best_rank;
    for(a=0;a<3;a++) for(p=0;p<sizes[a];p++) {
        double center=peaks[a][p].bpm,total=0,weighted=0,lo=1e9,hi=-1e9;
        Sum score_sum={0,0},weighted_sum={0,0};
        int support=0;
        for(b=0;b<3;b++) {
            int found=-1;double best_score=-1;
            for(q=0;q<sizes[b];q++) if(fabs(peaks[b][q].bpm-center)<=4.0 && peaks[b][q].score>best_score) {
                best_score=peaks[b][q].score;found=(int)q;
            }
            if(found>=0) {
                Peak match=peaks[b][found];support++;sum_add(&score_sum,match.score);sum_add(&weighted_sum,match.bpm*match.score);
                lo=minimum(lo,match.bpm);hi=maximum(hi,match.bpm);
            }
        }
        if(support<2) continue;
        total=sum_value(&score_sum);weighted=sum_value(&weighted_sum);
        /* Identical frequency-grid peaks have exactly this weighted mean.
         * Avoid 60.5 becoming 60.50000000000001 and changing de-dup/confirmation. */
        value.bpm=weighted/maximum(total,1e-9);value.score=total;value.spread=hi-lo;
        value.support=support;value.rank=total+1.5*(support-1)-(hi-lo)*0.25;
        for(k=0;k<n && round_even(candidates[k].bpm)!=round_even(value.bpm);k++) { }
        if(k==n) candidates[n++]=value;
        else if(value.rank>candidates[k].rank) candidates[k]=value;
    }
    if(!n) return none;
    for(i=1;i<n;i++) {
        temp=candidates[i];j=i;
        while(j>0 && candidates[j-1].rank<temp.rank) { candidates[j]=candidates[j-1];j--; }
        candidates[j]=temp;
    }
    best=candidates[0];continuity=anchor(c);best_rank=-1e20;
    if(continuity>=0.0) for(i=0;i<n;i++) {
        value=candidates[i];
        if(value.support==3 && value.score>=3.5 && value.spread<=3.0 && fabs(value.bpm-continuity)<=8.0 && value.rank>=candidates[0].rank-3.5) {
            double rank=value.rank-fabs(value.bpm-continuity)*0.35;
            if(rank>best_rank) { best_rank=rank;best=value; }
        }
    }
    return best;
}

static unsigned segments(HrCore *c,unsigned quiet_count,uint32_t now,int bins[2],unsigned bin_count,unsigned *usable,double *duration) {
    unsigned b,i,n=0,count=0,first=0,last=0;
    *usable=0;*duration=0.0;
    for(b=0;b<bin_count;b++) {
        count=0;
        for(i=0;i<=quiet_count;i++) {
            HrSample *s=i<quiet_count?sample_at(c,c->quiet[i]):NULL;
            int match=s!=NULL && ((double)s->ms/1000.0>=(double)now/1000.0-60.0) && s->bin==(unsigned)bins[b];
            int split=0;
            if(match && count) {
                HrSample *previous=sample_at(c,c->quiet[last]);
                split=s->frame-previous->frame>4u || seconds_between(s->ms,previous->ms)>0.8;
            }
            if(count && (split || i==quiet_count)) {
                double span=seconds_between(sample_at(c,c->quiet[last])->ms,sample_at(c,c->quiet[first])->ms);
                if(count>=12u && span>=2.0 && n<HR_SEG_CAP) {
                    c->segments[n].first=(uint16_t)first;c->segments[n].last=(uint16_t)last;
                    c->segments[n].count=(uint16_t)count;c->segments[n].bin=(uint16_t)bins[b];
                    n++;*usable+=count;*duration+=span;
                }
                count=0;
            }
            if(match) { if(!count) first=i;last=i;count++; }
        }
    }
    return n;
}

static int segment_powers(HrCore *c,const HrSegment *segment,unsigned antenna,hr_yield_fn yield,void *context) {
    unsigned i,n=0,k,j,half,width;double offset=0.0,previous=0.0,xmean=0,ymean=0,den=0,num=0,slope,rate;
    uint32_t first_ms=sample_at(c,c->quiet[segment->first])->ms;
    double *real=c->scratch.spectral.real,*imag=c->scratch.spectral.imag;
    Sum xsum={0,0},ysum={0,0},dsum={0,0},nsum={0,0};
    if(yield!=NULL && yield(context,HR_PHASE_PREPROCESS)) return 0;
    for(i=segment->first;i<=segment->last;i++) {
        HrSample *s=sample_at(c,c->quiet[i]);double phase;
        if(s->bin!=segment->bin) continue;
        c->times[n]=seconds_between(s->ms,first_ms);
        phase=atan2((double)s->iq[antenna*2u+1u],(double)s->iq[antenna*2u]);
        if(n) { double delta=phase-previous;if(delta>HR_PI) offset-=2.0*HR_PI;else if(delta< -HR_PI) offset+=2.0*HR_PI; }
        c->values[n]=phase+offset;previous=phase;sum_add(&xsum,c->times[n]);sum_add(&ysum,c->values[n]);n++;
        if((n&31u)==0u && yield!=NULL && yield(context,HR_PHASE_PREPROCESS)) return 0;
    }
    xmean=sum_value(&xsum)/n;ymean=sum_value(&ysum)/n;
    for(i=0;i<n;i++) { double dx=c->times[i]-xmean;sum_add(&dsum,dx*dx);sum_add(&nsum,dx*(c->values[i]-ymean)); }
    den=sum_value(&dsum);num=sum_value(&nsum);
    slope=den>0.0?num/den:0.0;c->scratch.prefix[0]=0.0;
    for(i=0;i<n;i++) { c->values[i]-=ymean+slope*(c->times[i]-xmean);c->scratch.prefix[i+1u]=c->scratch.prefix[i]+c->values[i]; }
    rate=(n-1u)/c->times[n-1u];width=(unsigned)round_even(rate*1.5);if(width<3u) width=3u;half=width/2u;
    if(yield!=NULL && yield(context,HR_PHASE_WINDOW)) return 0;
    for(i=0;i<n;i++) {
        unsigned left=i>half?i-half:0u,right=i+half+1u;if(right>n) right=n;
        c->values[i]-=(c->scratch.prefix[right]-c->scratch.prefix[left])/(right-left);
        c->values[i]*=0.5-0.5*cos(2.0*HR_PI*i/(n-1u));
    }
    memset(real,0,HR_FREQ_COUNT*sizeof(double));memset(imag,0,HR_FREQ_COUNT*sizeof(double));
    if(yield!=NULL && yield(context,HR_PHASE_DFT)) return 0;
#if HR_DFT_DIRECT
    for(k=0;k<HR_FREQ_COUNT;k++) {
        double omega=2.0*HR_PI*((48.0+k*0.5)/60.0);
        for(i=0;i<n;i++) {
            double angle=omega*c->times[i];
            real[k]+=c->values[i]*cos(angle);imag[k]-=c->values[i]*sin(angle);
        }
        if((k&7u)==7u && yield!=NULL && yield(context,HR_PHASE_DFT)) return 0;
    }
#elif HR_SPECTRAL_F32
    {
        float *rf=c->scratch.spectral_f32.real,*ifreq=c->scratch.spectral_f32.imag;
        memset(rf,0,HR_FREQ_COUNT*sizeof(float));memset(ifreq,0,HR_FREQ_COUNT*sizeof(float));
        for(i=0;i<n;i++) {
            double angle=2.0*HR_PI*0.8*c->times[i],delta=2.0*HR_PI*(0.5/60.0)*c->times[i];
            float re=cosf((float)angle),im=sinf((float)angle),dr=cosf((float)delta),di=sinf((float)delta),value=(float)c->values[i];
            for(k=0;k<HR_FREQ_COUNT;k++) {
                float next=re*dr-im*di;
                rf[k]+=value*re;ifreq[k]-=value*im;im=im*dr+re*di;re=next;
            }
            if((i&15u)==15u && yield!=NULL && yield(context,HR_PHASE_DFT)) return 0;
        }
        /* spectral.imag begins at byte 1160. The float arrays use only
         * bytes 0..1159, so converting powers cannot overwrite pending input. */
        for(j=0;j<HR_FREQ_COUNT;j++) { double r=rf[j],v=ifreq[j];imag[j]=r*r+v*v; }
    }
#else
    for(i=0;i<n;i++) {
        double angle=2.0*HR_PI*0.8*c->times[i],delta=2.0*HR_PI*(0.5/60.0)*c->times[i];
        double re=cos(angle),im=sin(angle),dr=cos(delta),di=sin(delta),value=c->values[i];
        for(k=0;k<HR_FREQ_COUNT;k++) {
            double next=re*dr-im*di;
            real[k]+=value*re;imag[k]-=value*im;im=im*dr+re*di;re=next;
        }
        if((i&15u)==15u && yield!=NULL && yield(context,HR_PHASE_DFT)) return 0;
    }
#endif
#if HR_DFT_DIRECT || !HR_SPECTRAL_F32
    for(j=0;j<HR_FREQ_COUNT;j++) imag[j]=real[j]*real[j]+imag[j]*imag[j];
#endif
    return 1;
}

static unsigned antenna_peaks(HrCore *c,unsigned count,unsigned antenna,int breath,Peak peaks[6],hr_yield_fn yield,void *context) {
    unsigned i,k,n=0,indices[HR_FREQ_COUNT],selected=0;double weight_sum=0,base;
    double *real=c->scratch.spectral.real,*power=c->scratch.spectral.imag;
    memset(c->aggregate,0,sizeof(c->aggregate));
    for(i=0;i<count;i++) {
        HrSegment *segment=&c->segments[i];double duration,weight;
        if(yield!=NULL && yield(context,HR_PHASE_SPECTRUM)) return UINT32_MAX;
        if(!segment_powers(c,segment,antenna,yield,context)) return UINT32_MAX;
        if(yield!=NULL && yield(context,HR_PHASE_PEAKS)) return UINT32_MAX;
        memcpy(real,power,HR_FREQ_COUNT*sizeof(double));base=median(real,HR_FREQ_COUNT)+1e-9;
        duration=seconds_between(sample_at(c,c->quiet[segment->last])->ms,sample_at(c,c->quiet[segment->first])->ms);
        weight=minimum(duration,10.0);weight_sum+=weight;
        for(k=0;k<HR_FREQ_COUNT;k++) c->aggregate[k]+=minimum(power[k]/base,100.0)*weight;
        if(yield!=NULL && yield(context,HR_PHASE_PEAKS)) return UINT32_MAX;
    }
    if(weight_sum<=0.0) return 0;
    for(i=0;i<HR_FREQ_COUNT;i++) { c->aggregate[i]/=weight_sum;real[i]=c->aggregate[i]; }
    base=median(real,HR_FREQ_COUNT)+1e-9;
    for(i=1;i+1u<HR_FREQ_COUNT;i++) if(c->aggregate[i]>=c->aggregate[i-1] && c->aggregate[i]>=c->aggregate[i+1]) indices[n++]=i;
    for(i=1;i<n;i++) { unsigned item=indices[i],j=i;
        while(j>0 && c->aggregate[indices[j-1]]<c->aggregate[item]) { indices[j]=indices[j-1];j--; }
        indices[j]=item;
    }
    for(i=0;i<n;i++) {
        double bpm=48.0+indices[i]*0.5,score,weight=1.0;
        for(k=0;k<selected && fabs(bpm-peaks[k].bpm)>=4.0;k++) { }
        if(k<selected) continue;
        if(breath>=8) for(k=2;k<=4;k+=2) {
            double distance=fabs(bpm-breath*(double)k);
            if(distance<=2.0) weight=minimum(weight,0.20);else if(distance<=4.0) weight=minimum(weight,0.55);
        }
        score=log1p(c->aggregate[indices[i]]/base)*weight;
        peaks[selected].bpm=bpm;peaks[selected++].score=score;
        if(selected==6u) break;
    }
    return selected;
}

#include "heart_rate_shared_impl.h"

static int finish(HrCore *c,HrResult *r) {
    r->accepted_ms=c->have_accepted?c->last_accepted_ms:UINT32_MAX;return 1;
}
int hr_core_estimate(HrCore *c,uint8_t breath,uint32_t now,HrResult *r,hr_yield_fn yield,void *context) {
    unsigned i,j,nquiet=0,unique=0,dominant=0,neighbor_count=0,usable=0,nsegments,bins_count=1,required;
    unsigned peak_sizes[3];int bins[2],rate,high_harmonic_bpm=0,order;double duration=0,confirmed_value;
    Peak peaks[3][6];Consensus chosen;
    if(c==NULL || r==NULL) return 0;
    if(c->workspace!=NULL) memset(&c->workspace->stats,0,sizeof(c->workspace->stats));
    memset(r,0,sizeof(*r));r->bpm=-1;r->target_bin=-1;r->target_bins[0]=-1;r->target_bins[1]=-1;
    r->candidate_bpm=-1.0;r->reason=HR_WAIT_BED;
    if(yield!=NULL && yield(context,HR_PHASE_CHECK)) return 0;
    if(!c->count) {r->breath=breath;return finish(c,r);}
    observe_breath(c,breath,now);rate=median_breath(c,now);r->breath=(uint8_t)rate;
    if(c->have_smoothed && seconds_between(now,c->last_accepted_ms)>120.0) { c->have_smoothed=0;c->consistent=0; }
    if(rate<8) { c->candidate_count=0;r->reason=HR_WAIT_BREATH;return finish(c,r); }
    for(i=0;i<c->count;i++) {
        HrSample *s=sample_at(c,i);
        if(seconds_between(now,s->ms)>75.0) continue;
        if((s->flags&HR_ACTIVE) && seconds_between(now,s->ms)<=3.0) { r->quality=1;r->reason=HR_MOTION;return finish(c,r); }
        if((s->flags&(HR_VALID|HR_IN_BED|HR_ACTIVE))==(HR_VALID|HR_IN_BED)) c->quiet[nquiet++]=(uint16_t)i;
    }
    duration=nquiet>=2u?seconds_between(sample_at(c,c->quiet[nquiet-1])->ms,sample_at(c,c->quiet[0])->ms):0.0;
    r->duration_s=duration;
    if(nquiet<80u || duration<20.0) { r->reason=HR_QUIET_SHORT;return finish(c,r); }
    for(i=0;i<nquiet;i++) {
        uint16_t bin=sample_at(c,c->quiet[i])->bin;
        for(j=0;j<unique && c->scratch.bins.ids[j]!=bin;j++) { }
        if(j==unique) { c->scratch.bins.ids[unique]=bin;c->scratch.bins.counts[unique++]=0; }
        c->scratch.bins.counts[j]++;
    }
    for(i=1;i<unique;i++) if(c->scratch.bins.counts[i]>c->scratch.bins.counts[dominant]) dominant=i;
    bins[0]=c->scratch.bins.ids[dominant];bins[1]=-1;r->target_bin=(int16_t)bins[0];
    for(j=0;j<2;j++) {
        int neighbor=bins[0]+(j?1:-1);
        for(i=0;i<unique;i++) if(c->scratch.bins.ids[i]==neighbor && c->scratch.bins.counts[i]>neighbor_count) {
            neighbor_count=c->scratch.bins.counts[i];bins[1]=neighbor;
        }
    }
    if(neighbor_count>=maximum(12.0,(int)(c->scratch.bins.counts[dominant]*0.25))) bins_count=2;
    else { neighbor_count=0;bins[1]=-1; }
    r->bin_share=(c->scratch.bins.counts[dominant]+neighbor_count)/(double)nquiet;
    if(bins_count==2u && bins[1]<bins[0]) { int swap=bins[0];bins[0]=bins[1];bins[1]=swap; }
    r->target_count=(uint8_t)bins_count;r->target_bins[0]=(int16_t)bins[0];r->target_bins[1]=(int16_t)bins[1];
    if(r->bin_share<0.65) { c->candidate_count=0;r->reason=HR_BIN_UNSTABLE;r->quality=1;r->duration_s=0;return finish(c,r); }
    nsegments=segments(c,nquiet,now,bins,bins_count,&usable,&duration);r->duration_s=duration;
    if(usable<80u || duration<30.0) { r->quality=1;r->reason=HR_LOCKING;return finish(c,r); }
    if(c->workspace!=NULL) {
        if(!shared_antenna_peaks(c,nsegments,rate,peaks,peak_sizes,yield,context)) {
            r->reason=HR_RESET;return 0;
        }
    } else {
        /* Literal V25 fallback exists for independent native comparisons.
         * Device init requires a workspace; it never silently falls back. */
        for(i=0;i<3;i++) {
            peak_sizes[i]=antenna_peaks(c,nsegments,i,rate,peaks[i],yield,context);
            if(peak_sizes[i]==UINT32_MAX) { r->reason=HR_RESET;return 0; }
        }
    }
    if(yield!=NULL && yield(context,HR_PHASE_CONSENSUS)) { r->reason=HR_RESET;return 0; }
    chosen=consensus(c,peaks,peak_sizes);r->support=(uint8_t)chosen.support;
    r->cluster_score=chosen.score;r->candidate_bpm=chosen.bpm;
    if(chosen.bpm<0.0 || chosen.support<2) { c->consistent=0;c->candidate_count=0;r->reason=HR_ANTENNAS;r->quality=1;return finish(c,r); }
    r->quality=1;
    if(!strong((unsigned)chosen.support,chosen.score,chosen.spread,r->bin_share)) { r->reason=HR_WEAK;return finish(c,r); }
    order=high_harmonic(c,chosen.bpm,rate,&high_harmonic_bpm);
    if(order) { r->reason=HR_HIGH_HARMONIC;r->harmonic_order=(uint8_t)order;r->harmonic_bpm=(int16_t)high_harmonic_bpm;r->harmonic_conflict=1;return finish(c,r); }
    if(c->candidate_count==20u) { memmove(c->candidates,c->candidates+1,19u*sizeof(c->candidates[0]));c->candidate_count--; }
    c->candidates[c->candidate_count].ms=now;c->candidates[c->candidate_count].bpm=chosen.bpm;
    c->candidates[c->candidate_count++].breath=(uint8_t)rate;
    while(c->candidate_count && seconds_between(now,c->candidates[0].ms)>90.0) memmove(c->candidates,c->candidates+1,(--c->candidate_count)*sizeof(c->candidates[0]));
    required=!c->have_smoothed?5u:fabs(chosen.bpm-c->smoothed)>5.0?7u:3u;
    if(harmonic_conflict(c,chosen.bpm,rate) && required<6u) required=6u;
    if(!confirmed(c,required,&confirmed_value)) {
        r->bpm=(int16_t)held(c,now);r->harmonic_bpm=(int16_t)nearest_third(c,chosen.bpm,rate);
        r->harmonic_conflict=(uint8_t)harmonic_conflict(c,chosen.bpm,rate);
        r->reason=r->harmonic_conflict?HR_CONFIRMING_HARMONIC:HR_CONFIRMING;return finish(c,r);
    }
    r->harmonic_bpm=(int16_t)nearest_third(c,confirmed_value,rate);
    r->harmonic_conflict=(uint8_t)harmonic_conflict(c,confirmed_value,rate);
    if(r->harmonic_conflict && !(confirmed_value>=60.0 && confirmed_value<=90.0 &&
        ((!c->have_smoothed && trailing(c,confirmed_value)>=6u) ||
         (c->have_smoothed && (fabs(confirmed_value-c->smoothed)<=5.0 || trailing(c,confirmed_value)>=7u))))) {
        r->bpm=(int16_t)held(c,now);r->reason=r->bpm>=0?HR_HOLD_HARMONIC:HR_WAIT_HARMONIC;
        r->harmonic_order=3;return finish(c,r);
    }
    if(!c->have_smoothed) { c->smoothed=confirmed_value;c->have_smoothed=1; }
    else if(fabs(confirmed_value-c->smoothed)>5.0) c->smoothed=confirmed_value; /* strong candidate already required */
    else c->smoothed=c->smoothed*0.65+confirmed_value*0.35;
    c->last_accepted_ms=now;c->have_accepted=1;c->consistent=(uint8_t)trailing(c,confirmed_value);
    if(chosen.support==3 && r->bin_share>=0.80 && chosen.spread<=4.0 && chosen.score>=5.0) r->quality=2;
    if(chosen.support==3 && r->bin_share>=0.90 && chosen.spread<=2.5 && chosen.score>=8.0 && c->consistent>=2u) r->quality=3;
    r->bpm=(int16_t)round_even(c->smoothed);r->fresh=1;
    r->harmonic_bpm=(int16_t)nearest_third(c,r->bpm,rate);r->harmonic_conflict=(uint8_t)harmonic_conflict(c,r->bpm,rate);
    r->harmonic_order=r->harmonic_conflict?3u:0u;if(r->harmonic_conflict) r->quality=1;
    r->reason=r->harmonic_conflict?HR_OVERLAP:HR_ENGINEERING;
    return finish(c,r);
}

int hr_core_maybe(HrCore *c,uint8_t breath,uint32_t now,HrResult *r) {
    if(!hr_core_due(c,now)) return 0;
    c->last_estimate_ms=now;return hr_core_estimate(c,breath,now,r,NULL,NULL);
}
int hr_core_due(const HrCore *c,uint32_t now) {
    return c!=NULL && seconds_between(now,c->last_estimate_ms)>=5.0;
}
