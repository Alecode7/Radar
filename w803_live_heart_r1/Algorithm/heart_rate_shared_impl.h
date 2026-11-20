#ifndef HEART_RATE_SHARED_IMPL_H
#define HEART_RATE_SHARED_IMPL_H
/* Same frequency grid and timestamp-based DFT, binary64 rotation along
 * frequency. Re-anchor with the literal original expression every 16 points.
 * Roundoff differs; confirmation/quality rules are not modified. */
#if HR_FREQ_REANCHOR < 1
#error "Frequency re-anchor interval must be positive"
#endif
static int shared_frequency_power(HrCore *c,unsigned n,double *filtered[3],
    double *power[3],hr_yield_fn yield,void *context) {
    HrWorkspace *w=c->workspace;unsigned i,k;
#if HR_FREQ_RECURSION
    double *co=w->rotation[0],*si=w->rotation[1];
    double *step_co=w->rotation[2],*step_si=w->rotation[3];
    if(w->frequency_rotation)for(i=0;i<n;i++) {
        double step=2.0*HR_PI*c->times[i]/120.0;
        step_co[i]=cos(step);step_si[i]=sin(step);
        if((i&31u)==0u && yield!=NULL && yield(context,HR_PHASE_DFT))return 0;
    }
    if(w->frequency_rotation)w->stats.trig_calls+=2u*n;
#endif
    for(k=0;k<HR_FREQ_COUNT;k++) {
        double omega=2.0*HR_PI*((48.0+k*0.5)/60.0);
        double r0=0.0,r1=0.0,r2=0.0,j0=0.0,j1=0.0,j2=0.0;
        for(i=0;i<n;i++) {
            double cosine,sine;
#if HR_FREQ_RECURSION
            if(!w->frequency_rotation || k%HR_FREQ_REANCHOR==0u) {
                double angle=omega*c->times[i];cosine=cos(angle);sine=sin(angle);
            } else {
                double old_co=co[i],old_si=si[i];
                cosine=old_co*step_co[i]-old_si*step_si[i];
                sine=old_si*step_co[i]+old_co*step_si[i];
            }
            if(w->frequency_rotation) { co[i]=cosine;si[i]=sine; }
#else
            double angle=omega*c->times[i];cosine=cos(angle);sine=sin(angle);
#endif
            /* Preserve sample accumulation order for every antenna/frequency. */
            r0+=filtered[0][i]*cosine;j0-=filtered[0][i]*sine;
            r1+=filtered[1][i]*cosine;j1-=filtered[1][i]*sine;
            r2+=filtered[2][i]*cosine;j2-=filtered[2][i]*sine;
        }
        power[0][k]=r0*r0+j0*j0;power[1][k]=r1*r1+j1*j1;power[2][k]=r2*r2+j2*j2;
#if HR_FREQ_RECURSION
        if(!w->frequency_rotation || k%HR_FREQ_REANCHOR==0u)w->stats.trig_calls+=2u*n;
#else
        w->stats.trig_calls+=2u*n;
#endif
        w->stats.frequencies++;
        if(yield!=NULL && yield(context,HR_PHASE_DFT))return 0;
    }
    return 1;
}
#if defined(HR_SPECTRUM_TEST_API)
#include <stdlib.h>
HR_API int hr_spectrum_probe(const double *times,const double *signals,unsigned n,
    double *output,HrWorkStats *stats) {
    HrCore *c;HrWorkspace *w;double *filtered[3],*power[3];unsigned a;int ok;
    if(!times || !signals || !output || !stats || n<2u || n>HR_SAMPLE_CAP)return 0;
    c=(HrCore *)calloc(1,sizeof(*c));w=(HrWorkspace *)calloc(1,sizeof(*w));
    if(!c || !w) { free(c);free(w);return 0; }
    hr_core_attach(c,w);memcpy(c->times,times,n*sizeof(double));
    filtered[0]=c->values;filtered[1]=w->filtered[0];filtered[2]=w->filtered[1];
    power[0]=c->scratch.spectral.imag;power[1]=w->power[0];power[2]=w->power[1];
    for(a=0;a<3u;a++)memcpy(filtered[a],signals+a*n,n*sizeof(double));
    ok=shared_frequency_power(c,n,filtered,power,NULL,NULL);
    for(a=0;a<3u;a++)memcpy(output+a*HR_FREQ_COUNT,power[a],HR_FREQ_COUNT*sizeof(double));
    *stats=w->stats;free(w);free(c);return ok;
}
#endif
static int shared_prepare(HrCore *c,const HrSegment *segment,unsigned antenna,
    unsigned *count,hr_yield_fn yield,void *context) {
    unsigned i,n=0,half,width;double offset=0.0,previous=0.0,xmean,ymean,den,num,slope,rate;
    uint32_t first_ms=sample_at(c,c->quiet[segment->first])->ms;
    Sum xsum={0,0},ysum={0,0},dsum={0,0},nsum={0,0};
    if(yield!=NULL && yield(context,HR_PHASE_PREPROCESS)) return 0;
    for(i=segment->first;i<=segment->last;i++) {
        HrSample *s=sample_at(c,c->quiet[i]);double phase;
        if(s->bin!=segment->bin) continue;
        c->times[n]=seconds_between(s->ms,first_ms);
        phase=atan2((double)s->iq[antenna*2u+1u],(double)s->iq[antenna*2u]);
        if(n) {double delta=phase-previous;if(delta>HR_PI) offset-=2.0*HR_PI;else if(delta< -HR_PI) offset+=2.0*HR_PI;}
        c->values[n]=phase+offset;previous=phase;sum_add(&xsum,c->times[n]);sum_add(&ysum,c->values[n]);n++;
        if((n&31u)==0u && yield!=NULL && yield(context,HR_PHASE_PREPROCESS)) return 0;
    }
    xmean=sum_value(&xsum)/n;ymean=sum_value(&ysum)/n;
    for(i=0;i<n;i++) {double dx=c->times[i]-xmean;sum_add(&dsum,dx*dx);sum_add(&nsum,dx*(c->values[i]-ymean));}
    den=sum_value(&dsum);num=sum_value(&nsum);
    slope=den>0.0?num/den:0.0;c->scratch.prefix[0]=0.0;
    for(i=0;i<n;i++) {c->values[i]-=ymean+slope*(c->times[i]-xmean);c->scratch.prefix[i+1u]=c->scratch.prefix[i]+c->values[i];}
    rate=(n-1u)/c->times[n-1u];width=(unsigned)round_even(rate*1.5);if(width<3u) width=3u;half=width/2u;
    if(yield!=NULL && yield(context,HR_PHASE_WINDOW)) return 0;
    for(i=0;i<n;i++) {
        unsigned left=i>half?i-half:0u,right=i+half+1u;if(right>n) right=n;
        c->values[i]-=(c->scratch.prefix[right]-c->scratch.prefix[left])/(right-left);
        c->values[i]*=0.5-0.5*cos(2.0*HR_PI*i/(n-1u));
    }
    *count=n;return 1;
}
static unsigned shared_find_peaks(HrCore *c,double *power,double weight_sum,int breath,Peak peaks[6]) {
    unsigned i,k,n=0,indices[HR_FREQ_COUNT],selected=0;double base;
    double *real=c->scratch.spectral.real;
    if(weight_sum<=0.0) return 0;
    for(i=0;i<HR_FREQ_COUNT;i++) {power[i]/=weight_sum;real[i]=power[i];}
    base=median(real,HR_FREQ_COUNT)+1e-9;
    for(i=1;i+1u<HR_FREQ_COUNT;i++) if(power[i]>=power[i-1] && power[i]>=power[i+1]) indices[n++]=i;
    for(i=1;i<n;i++) {unsigned item=indices[i],j=i;
        while(j>0 && power[indices[j-1]]<power[item]) {indices[j]=indices[j-1];j--;}
        indices[j]=item;
    }
    for(i=0;i<n;i++) {
        double bpm=48.0+indices[i]*0.5,score,weight=1.0;
        for(k=0;k<selected && fabs(bpm-peaks[k].bpm)>=4.0;k++) {}
        if(k<selected) continue;
        if(breath>=8) for(k=2;k<=4;k+=2) {
            double distance=fabs(bpm-breath*(double)k);
            if(distance<=2.0) weight=minimum(weight,0.20);else if(distance<=4.0) weight=minimum(weight,0.55);
        }
        score=log1p(power[indices[i]]/base)*weight;
        peaks[selected].bpm=bpm;peaks[selected++].score=score;
        if(selected==6u) break;
    }
    return selected;
}
static int shared_antenna_peaks(HrCore *c,unsigned count,int breath,Peak peaks[3][6],
    unsigned sizes[3],hr_yield_fn yield,void *context) {
    HrWorkspace *w=c->workspace;
    double *filtered[3]={c->values,w->filtered[0],w->filtered[1]};
    double *aggregate[3]={c->aggregate,w->aggregate[0],w->aggregate[1]};
    double *power[3]={c->scratch.spectral.imag,w->power[0],w->power[1]};
    double weight_sum=0.0;unsigned segment_index,a,k,n=0;
    memset(c->aggregate,0,sizeof(c->aggregate));memset(w->aggregate,0,sizeof(w->aggregate));
    for(segment_index=0;segment_index<count;segment_index++) {
        HrSegment *segment=&c->segments[segment_index];double duration,weight;
        if(yield!=NULL && yield(context,HR_PHASE_SPECTRUM)) return 0;
        /* Prepare antenna0 last, leaving its original buffer intact. */
        for(a=3;a>0;) {
            a--;if(!shared_prepare(c,segment,a,&n,yield,context)) return 0;
            if(a>0) memcpy(filtered[a],c->values,n*sizeof(double));
        }
        w->stats.segments++;w->stats.samples+=n;
        if(yield!=NULL && yield(context,HR_PHASE_DFT)) return 0;
        if(!shared_frequency_power(c,n,filtered,power,yield,context))return 0;
        if(yield!=NULL && yield(context,HR_PHASE_PEAKS)) return 0;
        duration=seconds_between(sample_at(c,c->quiet[segment->last])->ms,sample_at(c,c->quiet[segment->first])->ms);
        weight=minimum(duration,10.0);weight_sum+=weight;
        for(a=0;a<3;a++) {
            double base;
            memcpy(c->scratch.spectral.real,power[a],HR_FREQ_COUNT*sizeof(double));
            base=median(c->scratch.spectral.real,HR_FREQ_COUNT)+1e-9;
            for(k=0;k<HR_FREQ_COUNT;k++) aggregate[a][k]+=minimum(power[a][k]/base,100.0)*weight;
        }
        if(yield!=NULL && yield(context,HR_PHASE_PEAKS)) return 0;
    }
    for(a=0;a<3;a++) sizes[a]=shared_find_peaks(c,aggregate[a],weight_sum,breath,peaks[a]);
    return 1;
}
#endif
