#ifndef HEART_RATE_SHARED_IMPL_H
#define HEART_RATE_SHARED_IMPL_H
#if HR_NUMERIC_BACKEND == 3
#define HR_CHIP_ADD(sum,correction,term) do { \
    float hr_term=(term)-(correction),hr_total=(sum)+hr_term; \
    (correction)=(hr_total-(sum))-hr_term;(sum)=hr_total; \
} while(0)
#endif
/* V27: same phase/filter/DFT/consensus method. Hardware-float spectrum
 * variants are explicitly selected and differentially checked. */
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
    /* The exact double Hann coefficient depends only on segment length.
     * Reuse it across antennas and subsequent equal-length segments. */
    if(c->workspace->hann_count!=n) {
        for(i=0;i<n;i++) c->workspace->hann[i]=0.5-0.5*cos(2.0*HR_PI*i/(n-1u));
        c->workspace->hann_count=n;
    }
    for(i=0;i<n;i++) {
        unsigned left=i>half?i-half:0u,right=i+half+1u;if(right>n) right=n;
        c->values[i]-=(c->scratch.prefix[right]-c->scratch.prefix[left])/(right-left);
        c->values[i]*=c->workspace->hann[i];
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
#if HR_NUMERIC_BACKEND == 0
    double *filtered[3]={c->values,w->filtered.precise[0],w->filtered.precise[1]};
#else
    float *filtered[3]={w->filtered.chip[0],w->filtered.chip[1],w->filtered.chip[2]};
#endif
    double *aggregate[3]={c->aggregate,w->aggregate[0],w->aggregate[1]};
    double *power[3]={c->scratch.spectral.imag,w->power[0],w->power[1]};
    double weight_sum=0.0;unsigned segment_index,a,k,i,n=0;
    memset(c->aggregate,0,sizeof(c->aggregate));memset(w->aggregate,0,sizeof(w->aggregate));
    for(segment_index=0;segment_index<count;segment_index++) {
        HrSegment *segment=&c->segments[segment_index];double duration,weight;
        if(yield!=NULL && yield(context,HR_PHASE_SPECTRUM)) return 0;
        /* Prepare antenna0 last, leaving its original buffer intact. */
        for(a=3;a>0;) {
            a--;if(!shared_prepare(c,segment,a,&n,yield,context)) return 0;
#if HR_NUMERIC_BACKEND == 0
            if(a>0) memcpy(filtered[a],c->values,n*sizeof(double));
#else
            for(i=0;i<n;i++) filtered[a][i]=(float)c->values[i];
#endif
        }
        w->stats.segments++;w->stats.samples+=n;
        if(yield!=NULL && yield(context,HR_PHASE_DFT)) return 0;
#if HR_NUMERIC_BACKEND == 2
        memset(w->recurrent,0,sizeof(w->recurrent));
        for(i=0;i<n;i++) {
            double angle=2.0*HR_PI*0.8*c->times[i],delta=2.0*HR_PI*(0.5/60.0)*c->times[i];
            float re=cosf((float)angle),im=sinf((float)angle),dr=cosf((float)delta),di=sinf((float)delta);
            float v0=filtered[0][i],v1=filtered[1][i],v2=filtered[2][i];
            for(k=0;k<HR_FREQ_COUNT;k++) {
                float next=re*dr-im*di;
                w->recurrent[0][0][k]+=v0*re;w->recurrent[0][1][k]-=v0*im;
                w->recurrent[1][0][k]+=v1*re;w->recurrent[1][1][k]-=v1*im;
                w->recurrent[2][0][k]+=v2*re;w->recurrent[2][1][k]-=v2*im;
                im=im*dr+re*di;re=next;
            }
            w->stats.trig_calls+=4u;
            if((i&15u)==15u && yield!=NULL && yield(context,HR_PHASE_DFT)) return 0;
        }
        for(a=0;a<3;a++) for(k=0;k<HR_FREQ_COUNT;k++) {
            double real=w->recurrent[a][0][k],imag=w->recurrent[a][1][k];
            power[a][k]=real*real+imag*imag;
        }
        w->stats.frequencies+=HR_FREQ_COUNT;
        if(yield!=NULL && yield(context,HR_PHASE_DFT)) return 0;
#else
        for(k=0;k<HR_FREQ_COUNT;k++) {
            double omega=2.0*HR_PI*((48.0+k*0.5)/60.0);
#if HR_NUMERIC_BACKEND == 0
            double r0=0.0,r1=0.0,r2=0.0,j0=0.0,j1=0.0,j2=0.0;
#else
            float r0=0.0f,r1=0.0f,r2=0.0f,j0=0.0f,j1=0.0f,j2=0.0f;
#if HR_NUMERIC_BACKEND == 3
            float cr0=0.0f,cr1=0.0f,cr2=0.0f,cj0=0.0f,cj1=0.0f,cj2=0.0f;
#endif
#endif
            for(i=0;i<n;i++) {
#if HR_NUMERIC_BACKEND == 0
                double angle=omega*c->times[i],co=cos(angle),si=sin(angle);
#else
                float angle=(float)(omega*c->times[i]),co=cosf(angle),si=sinf(angle);
#endif
#if HR_NUMERIC_BACKEND == 3
                HR_CHIP_ADD(r0,cr0,filtered[0][i]*co);HR_CHIP_ADD(j0,cj0,-filtered[0][i]*si);
                HR_CHIP_ADD(r1,cr1,filtered[1][i]*co);HR_CHIP_ADD(j1,cj1,-filtered[1][i]*si);
                HR_CHIP_ADD(r2,cr2,filtered[2][i]*co);HR_CHIP_ADD(j2,cj2,-filtered[2][i]*si);
#else
                r0+=filtered[0][i]*co;j0-=filtered[0][i]*si;
                r1+=filtered[1][i]*co;j1-=filtered[1][i]*si;
                r2+=filtered[2][i]*co;j2-=filtered[2][i]*si;
#endif
            }
            {
                double re0=r0,im0=j0,re1=r1,im1=j1,re2=r2,im2=j2;
                power[0][k]=re0*re0+im0*im0;power[1][k]=re1*re1+im1*im1;power[2][k]=re2*re2+im2*im2;
            }
            w->stats.trig_calls+=2u*n;w->stats.frequencies++;
            /* A finer checkpoint bounds deadline overshoot without
             * changing a single numerical accumulation. */
            if(yield!=NULL && yield(context,HR_PHASE_DFT)) return 0;
        }
#endif
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
#if HR_NUMERIC_BACKEND == 3
#undef HR_CHIP_ADD
#endif
#endif
