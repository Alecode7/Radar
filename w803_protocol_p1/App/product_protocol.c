#include "product_protocol.h"
#include <string.h>
#define CAPS 0x1ffbu
#define RESTRICTIONS 32u /* current estimator ranges; alert settings cannot expand them */
#define UNKNOWN UINT32_MAX
#define FRESH_MS 10000u
#define SOURCE_MS 6000u
static uint16_t g16(const uint8_t *p) {return (uint16_t)(p[0]|((uint16_t)p[1]<<8));}
static uint32_t g32(const uint8_t *p) {return p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static uint64_t g64(const uint8_t *p) {return g32(p)|((uint64_t)g32(p+4)<<32);}
static void w16(uint8_t *p,uint16_t v) {p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);}
static void w32(uint8_t *p,uint32_t v) {unsigned i;for(i=0;i<4u;i++)p[i]=(uint8_t)(v>>(8u*i));}
static void w64(uint8_t *p,uint64_t v) {w32(p,(uint32_t)v);w32(p+4,(uint32_t)(v>>32));}
static uint32_t next(uint32_t v) {return v==UINT32_MAX?1u:v+1u;}
static uint32_t add(uint32_t a,uint32_t b) {return b>UINT32_MAX-a?UINT32_MAX:a+b;}
size_t p1_size(void) {return sizeof(P1State);}
uint16_t p1_crc(const uint8_t *p,size_t n) {
    uint16_t c=0xffffu;size_t i;unsigned j;
    for(i=0;i<n;i++) {c^=(uint16_t)p[i]<<8;for(j=0;j<8u;j++)c=(uint16_t)((c&0x8000u)?((uint32_t)c<<1)^0x1021u:(uint32_t)c<<1);}
    return c;
}
size_t p1_frame(unsigned kind,unsigned id,uint32_t seq,const uint8_t *body,size_t n,uint8_t *out,size_t cap) {
    if(!out || !seq || n>P1_FRAME_MAX-16u || cap<n+16u || (n && !body))return 0;
    out[0]=0x52;out[1]=0x53;out[2]=1;out[3]=1;out[4]=(uint8_t)kind;out[5]=0;
    w16(out+6,(uint16_t)id);w32(out+8,seq);w16(out+12,(uint16_t)n);
    if(n)memmove(out+14,body,n);
    w16(out+14+n,p1_crc(out+2,n+12u));return n+16u;
}
static int header(const uint8_t *p) {
    return p[0]==0x52 && p[1]==0x53 && p[2]==1 && p[3]==1 &&
        p[4]>=1u && p[4]<=4u && !p[5] && g32(p+8) && g16(p+12)<=P1_FRAME_MAX-16u;
}
static int frame_valid(const uint8_t *p,size_t n) {
    return p && n>=16u && n<=P1_FRAME_MAX && header(p) && n==16u+g16(p+12) &&
        g16(p+n-2u)==p1_crc(p+2,n-4u);
}
void p1_parser_init(P1Parser *p) {memset(p,0,sizeof(*p));}
static void consume(P1Parser *p) {if(p->used) {p->used--;memmove(p->bytes,p->bytes+1,p->used);}}
static void drain(P1Parser *p,P1Receive receive,void *ctx) {
    while(p->used) {
        size_t n;
        if(p->bytes[0]!=0x52 || (p->used>=2 && p->bytes[1]!=0x53)) {consume(p);continue;}
        if(p->used<14u)return;
        if(!header(p->bytes)) {p->errors++;consume(p);continue;}
        n=16u+g16(p->bytes+12);
        if(p->used<n)return;
        if(!frame_valid(p->bytes,n)) {p->errors++;consume(p);continue;}
        if(receive)receive(p->bytes,n,ctx);
        p->used=(uint16_t)(p->used-n);memmove(p->bytes,p->bytes+n,p->used);
    }
}
void p1_parser_poll(P1Parser *p,uint32_t now,P1Receive receive,void *ctx) {
    if(p->used && now-p->last_ms>200u) {
        p->errors++;
        while(p->used) {consume(p);drain(p,receive,ctx);}
    }
}
void p1_parser_feed(P1Parser *p,const uint8_t *data,size_t n,uint32_t now,P1Receive receive,void *ctx) {
    size_t i;
    if(!p || (n && !data))return;
    p1_parser_poll(p,now,receive,ctx);
    for(i=0;i<n;i++) {
        if(p->used>=P1_FRAME_MAX) {p->errors++;consume(p);}
        p->bytes[p->used++]=data[i];p->last_ms=now;drain(p,receive,ctx);
    }
}
void p1_config_default(P1Config *c) {
    memset(c,0,sizeof(*c));c->revision=1;c->report_ms=2000;c->heartbeat_ms=1000;c->enabled=255;
    c->br_low=100;c->br_high=250;c->hr_low=400;c->hr_high=1200;
    c->confirm_ms=10000;c->recover_ms=10000;c->missing_br_ms=60000;c->missing_hr_ms=180000;
    c->long_bed_ms=8u*3600000u;c->away_ms=600000;c->min_hr_quality=2;c->reject_harmonic=1;
}
void p1_config_encode(const P1Config *c,uint8_t *b) {
    memset(b,0,P1_CONFIG_BYTES);w16(b,1);w16(b+2,P1_CONFIG_BYTES);w32(b+4,c->revision);
    w16(b+8,c->report_ms);w16(b+10,c->heartbeat_ms);w16(b+12,c->enabled);
    w16(b+16,c->br_low);w16(b+18,c->br_high);w16(b+20,c->hr_low);w16(b+22,c->hr_high);
    w32(b+24,c->confirm_ms);w32(b+28,c->recover_ms);w32(b+32,c->missing_br_ms);w32(b+36,c->missing_hr_ms);
    w32(b+40,c->long_bed_ms);w32(b+44,c->away_ms);b[48]=c->min_hr_quality;b[49]=c->reject_harmonic;
    b[50]=c->mount_enabled;w16(b+52,c->mount_height);w16(b+54,c->mount_pitch);
}
int p1_config_decode(const uint8_t *b,size_t n,P1Config *c) {
    unsigned i;
    if(n!=P1_CONFIG_BYTES || g16(b)!=1 || g16(b+2)!=P1_CONFIG_BYTES || !g32(b+4) || g16(b+14))return 0;
    if(b[51])return 0;
    for(i=56;i<P1_CONFIG_BYTES;i++)if(b[i])return 0;
    memset(c,0,sizeof(*c));c->revision=g32(b+4);c->report_ms=g16(b+8);c->heartbeat_ms=g16(b+10);c->enabled=g16(b+12);
    c->br_low=g16(b+16);c->br_high=g16(b+18);c->hr_low=g16(b+20);c->hr_high=g16(b+22);
    c->confirm_ms=g32(b+24);c->recover_ms=g32(b+28);c->missing_br_ms=g32(b+32);c->missing_hr_ms=g32(b+36);
    c->long_bed_ms=g32(b+40);c->away_ms=g32(b+44);c->min_hr_quality=b[48];c->reject_harmonic=b[49];
    c->mount_enabled=b[50];c->mount_height=g16(b+52);c->mount_pitch=g16(b+54);
    return c->report_ms>=200 && c->report_ms<=5000 && c->heartbeat_ms>=500 && c->heartbeat_ms<=10000 &&
        !(c->enabled&~255u) && c->br_low>=30 && c->br_high<=800 && c->br_low<c->br_high &&
        c->hr_low>=200 && c->hr_high<=2500 && c->hr_low<c->hr_high &&
        c->confirm_ms>=1000 && c->confirm_ms<=600000 && c->recover_ms>=1000 && c->recover_ms<=600000 &&
        c->missing_br_ms>=10000 && c->missing_br_ms<=3600000 && c->missing_hr_ms>=10000 && c->missing_hr_ms<=3600000 &&
        c->long_bed_ms>=60000 && c->long_bed_ms<=86400000 && c->away_ms>=10000 && c->away_ms<=86400000 &&
        c->min_hr_quality>=1 && c->min_hr_quality<=3 && c->reject_harmonic<=1 &&
        c->mount_enabled<=1 && c->mount_height<=300 && c->mount_pitch<=80;
}
static int current(const P1State *s) {return s->monitoring && s->actual_running &&
    (s->ctrl==P1_RUNNING || (s->ctrl==P1_PENDING && s->pending_action==2)) && s->have_sleep && s->now-s->last_sleep<SOURCE_MS;}
static int hr_eligible(const P1State *s) {
    return s->have_hr && s->hr_quality>=s->config.min_hr_quality &&
        (!s->config.reject_harmonic || !s->hr_harmonic) && s->now-s->last_hr<=FRESH_MS;
}
static int br_eligible(const P1State *s) {return s->have_br && s->breath_valid && s->now-s->last_br<=FRESH_MS;}
static void event(P1State *s,unsigned rule,unsigned transition,unsigned reason,uint16_t value,uint16_t threshold,uint32_t measure) {
    uint8_t *e;
    if(s->event_last==UINT32_MAX) {s->faults|=32u;s->event_overwrites=add(s->event_overwrites,1);return;}
    s->event_last=next(s->event_last);e=s->events[(s->event_last-1u)%P1_EVENT_CAP];
    memset(e,0,P1_EVENT_BYTES);w32(e,s->event_last);w64(e+4,s->uptime_ms);w32(e+12,s->monitor_id);
    e[16]=(uint8_t)rule;e[17]=(uint8_t)transition;w16(e+18,(uint16_t)reason);w16(e+20,value);w16(e+22,threshold);w32(e+24,measure);
    if(s->event_count<P1_EVENT_CAP)s->event_count++;else s->event_overwrites=add(s->event_overwrites,1);
    s->events_total=add(s->events_total,1);
}
static void end_rules(P1State *s,unsigned reason) {
    unsigned i;
    for(i=0;i<8;i++) {
        if(s->rules[i].active)event(s,i,P1_EVENT_END,reason,0,0,0);
        memset(&s->rules[i],0,sizeof(s->rules[i]));
    }
}
static void clear_measurements(P1State *s) {
    s->have_sleep=s->have_br=s->have_hr=s->have_good_br=s->have_good_hr=0;
    s->presence=s->bed=s->motion=s->sleep_state=0;s->br=s->hr=UINT16_MAX;s->br_seq=s->hr_seq=0;
    s->was_in_bed=0;s->bed_since=s->away_since=s->now;
}
void p1_init(P1State *s,uint64_t boot,uint32_t now,P1Store store,void *ctx) {
    memset(s,0,sizeof(*s));s->boot_id=boot;s->now=s->last_tick=now;s->ctrl=P1_STOPPED;
    s->store=store;s->store_context=ctx;p1_config_default(&s->config);clear_measurements(s);
    s->faults|=1u; /* radar capability/version has not yet been observed */
    if(!boot)s->faults|=8u;
}
int p1_load_config(P1State *s,const uint8_t *b,size_t n) {
    P1Config c;if(!p1_config_decode(b,n,&c))return 0;
    s->config=c;s->stored=1;return 1;
}
void p1_source(P1State *s,int ready,uint32_t now) {
    s->now=now;s->source_ready=(uint8_t)(ready!=0);s->source_seen=1;s->source_resets++;s->install_known=0;
    if(s->monitoring || s->ctrl==P1_PENDING) {end_rules(s,3);s->monitor_stop=now;s->monitoring=0;s->ctrl=P1_CONTROL_FAULT;}
    s->actual_running=0;clear_measurements(s);
    if(ready)s->faults&=~1u;else s->faults|=1u;
}
void p1_input_epoch(P1State *s,uint32_t generation,uint32_t now,unsigned have_current_context) {
    if(s->generation==generation)return;
    s->now=now;
    if(have_current_context) {
        unsigned i;
        s->have_hr=0;s->hr_seq=0;
        for(i=2;i<=5;i++)if(i!=4) {
            if(s->rules[i].active)event(s,i,P1_EVENT_END,6,0,0,0);
            memset(&s->rules[i],0,sizeof(s->rules[i]));
        }
    } else {end_rules(s,6);clear_measurements(s);}
    s->generation=generation;
}
void p1_install_observation(P1State *s,unsigned enabled,unsigned height,unsigned pitch,uint32_t now) {
    if(enabled>1u || height>300u || pitch>80u)return;
    s->install_known=1;s->install_enabled=(uint8_t)enabled;s->install_height=(uint16_t)height;
    s->install_pitch=(uint16_t)pitch;s->install_at=now;
}
static int install_matches(const P1State *s) {
    return s->install_known && s->install_enabled==s->config.mount_enabled &&
        (!s->config.mount_enabled || (s->install_height==s->config.mount_height && s->install_pitch==s->config.mount_pitch));
}
void p1_sleep(P1State *s,unsigned presence,unsigned bed,unsigned motion,unsigned sleep_state,unsigned bv,uint32_t now) {
    uint8_t next_bed=(uint8_t)(bed?2:1);s->now=now;
    if(!s->have_sleep || next_bed!=s->bed || now-s->last_sleep>=SOURCE_MS) {
        unsigned i;
        s->bed_since=s->away_since=now;
        s->have_br=s->have_hr=s->have_good_br=s->have_good_hr=0;
        if(next_bed==2) {s->was_in_bed=1;s->last_good_br=s->last_good_hr=now;}
        for(i=0;i<7;i++)if(s->rules[i].active) {event(s,i,P1_EVENT_END,4,0,0,0);memset(&s->rules[i],0,sizeof(s->rules[i]));}
    }
    s->presence=(uint8_t)(presence?2:1);s->bed=next_bed;s->motion=(uint8_t)(motion?2:1);
    s->sleep_state=(uint8_t)(sleep_state<=3u?sleep_state+1u:0u);s->breath_valid=(uint8_t)(bv!=0);
    s->last_sleep=now;s->have_sleep=1;
    if(next_bed==1)s->have_br=s->have_hr=0;
}
void p1_breath(P1State *s,uint32_t seq,uint16_t rate,uint32_t age,uint32_t now) {
    s->now=now;
    if(!seq || rate<3u || rate>80u || age>FRESH_MS)return;
    if(s->monitoring && (age>now-s->monitor_start || (s->have_sleep && s->bed==2 && age>now-s->bed_since)))return;
    if(seq==s->br_seq)return;
    s->br_seq=seq;s->br=(uint16_t)(rate*10u);s->last_br=now-age;s->have_br=1;
    if(current(s) && s->bed==2 && s->motion==1 && s->breath_valid) {
        s->last_good_br=s->last_br;s->have_good_br=1;
        s->br_sum+=s->br;s->br_count=add(s->br_count,1);
    }
}
void p1_heart(P1State *s,int bpm,unsigned quality,unsigned harmonic,unsigned fresh,uint32_t age,uint32_t generation,uint32_t now) {
    s->now=now;s->generation=generation;
    if(bpm<=0 || bpm>250 || age==UNKNOWN) {s->have_hr=0;return;}
    s->hr=(uint16_t)(bpm*10);s->hr_quality=(uint8_t)quality;s->hr_harmonic=(uint8_t)(harmonic!=0);s->have_hr=1;
    s->last_hr=now-age;
    if(fresh) {
        s->hr_seq=next(s->hr_seq);
        if(current(s) && s->bed==2 && s->motion==1 && hr_eligible(s)) {
            s->last_good_hr=s->last_hr;s->have_good_hr=1;
            if(s->monitoring) {s->hr_sum+=s->hr;s->hr_count=add(s->hr_count,1);}
        }
    }
}
int p1_begin_control(P1State *s,unsigned action,uint32_t now) {
    s->now=now;
    if(action>2)return P1_BAD_ARGUMENT;
    if(!s->boot_id || (!s->source_ready && action==1u))return P1_NOT_READY;
    if(action==1u && !s->install_known)return P1_NOT_READY;
    if(s->ctrl==P1_PENDING) {
        if(action!=0u || s->pending_action==0u)return P1_BUSY;
        /* STOP may replace an in-flight START; its older reply is ignored. */
    } else if(action==1u && s->ctrl==P1_RUNNING && s->monitoring && s->actual_running)return P1_OK;
    else if(action==0u && s->source_ready && s->ctrl==P1_STOPPED && !s->monitoring && !s->actual_running)return P1_OK;
    if(action==0u) {end_rules(s,1);s->monitor_stop=now;s->monitoring=0;clear_measurements(s);}
    if(action==1u && !install_matches(s)) {
        if(s->actual_running)return P1_BUSY;
        action=3u; /* install desired geometry before enabling acquisition */
    }
    s->pending_action=(uint8_t)action;s->ctrl=P1_PENDING;s->pending_at=now;s->control_tries=0;s->have_sent_control=0;
    s->control_counter=next(s->control_counter);
    s->operation=((uint64_t)(uint32_t)(s->boot_id^(s->boot_id>>32))<<32)|s->control_counter;
    if(!(s->operation>>32))s->operation|=UINT64_C(1)<<32;
    return P1_OK;
}
int p1_control_due(P1State *s,uint32_t now,uint64_t *op,unsigned *action) {
    if(s->ctrl!=P1_PENDING || s->control_tries>=3u)return 0;
    if(s->have_sent_control && now-s->last_control_send<700u)return 0;
    s->last_control_send=now;s->control_tries++;s->have_sent_control=1;*op=s->operation;*action=s->pending_action;return 1;
}
void p1_control_ack(P1State *s,uint64_t op,unsigned action,int ret,unsigned running,uint32_t now) {
    if(s->ctrl!=P1_PENDING || op!=s->operation || action!=s->pending_action)return;
    s->now=now;s->actual_running=(uint8_t)(running!=0);s->control_error=(uint32_t)ret;
    if(action==3u) {
        if(ret || running || !install_matches(s)) {s->ctrl=P1_CONTROL_FAULT;s->faults|=1u;return;}
        s->pending_action=1;s->pending_at=now;s->control_tries=0;s->have_sent_control=0;
        s->control_counter=next(s->control_counter);s->operation=(s->operation&UINT64_C(0xffffffff00000000))|s->control_counter;
        return;
    }
    if(ret || running>1u || (action<2u && running!=action) || (action==1u && !install_matches(s))) {s->ctrl=P1_CONTROL_FAULT;s->faults|=1u;return;}
    s->faults&=~1u;s->ctrl=running?P1_RUNNING:P1_STOPPED;
    if(action==1u && !s->monitoring) {
        s->monitor_id=next(s->monitor_id);s->monitor_start=now;s->monitor_stop=0;s->monitoring=1;
        s->in_bed_ms=s->active_ms=s->br_count=s->hr_count=s->events_total=0;s->br_sum=s->hr_sum=0;
        end_rules(s,2);clear_measurements(s);
    }
    if(action==0u || !running) {end_rules(s,1);s->monitor_stop=now;s->monitoring=0;clear_measurements(s);}
}
static void rule(P1State *s,unsigned index,int condition,uint16_t value,uint16_t threshold,uint32_t sequence) {
    P1Rule *r=&s->rules[index];uint32_t wait=condition?s->config.confirm_ms:s->config.recover_ms;
    if(!(s->config.enabled&(1u<<index)) || !s->monitoring || (s->bed!=2u && index<7u && current(s))) {
        if(r->active)event(s,index,P1_EVENT_END,4,value,threshold,sequence);
        memset(r,0,sizeof(*r));return;
    }
    if(condition<0) {
        if(r->active && r->state!=0)event(s,index,P1_EVENT_UNKNOWN,5,value,threshold,sequence);
        r->state=0;r->tracking=0;r->evidence=0;return;
    }
    if(r->active && r->state==0)event(s,index,P1_EVENT_RESUME,0,value,threshold,sequence);
    r->state=1;
    if((condition!=0)==(r->active!=0)) {r->tracking=0;r->evidence=0;return;}
    if(!r->tracking) {r->tracking=1;r->since=s->now;r->last_sequence=sequence;r->evidence=1;}
    else if(sequence!=r->last_sequence) {r->last_sequence=sequence;if(r->evidence<255)r->evidence++;}
    /* Time-only rules already encode their delay and do not count held rates. */
    if(index>=4u)wait=0;
    if(s->now-r->since>=wait && (index>=4u || r->evidence>=2u)) {
        r->active=(uint8_t)(condition!=0);r->tracking=0;r->evidence=0;
        event(s,index,r->active?P1_EVENT_ACTIVE:P1_EVENT_RECOVER,0,value,threshold,sequence);
    }
}
void p1_tick(P1State *s,uint32_t now) {
    uint32_t dt=now-s->last_tick;int live,bed,br,hr;unsigned i;
    s->uptime_ms+=dt;s->now=s->last_tick=now;
    if(s->ctrl==P1_PENDING && now-s->pending_at>=3000u) {
        s->ctrl=P1_CONTROL_FAULT;s->control_error=UINT32_MAX;s->faults|=1u;
        if(s->monitoring) {s->monitor_stop=now;s->monitoring=0;end_rules(s,3);clear_measurements(s);}
    }
    live=current(s);bed=live && s->bed==2;br=bed && s->motion==1 && br_eligible(s);hr=bed && s->motion==1 && hr_eligible(s);
    if(s->monitoring && dt<=SOURCE_MS && live) {
        if(bed)s->in_bed_ms=add(s->in_bed_ms,dt);
        if(s->motion==2)s->active_ms=add(s->active_ms,dt);
    }
    rule(s,0,br?(s->br<s->config.br_low):-1,s->br,s->config.br_low,s->br_seq);
    rule(s,1,br?(s->br>s->config.br_high):-1,s->br,s->config.br_high,s->br_seq);
    rule(s,2,hr?(s->hr<s->config.hr_low):-1,s->hr,s->config.hr_low,s->hr_seq);
    rule(s,3,hr?(s->hr>s->config.hr_high):-1,s->hr,s->config.hr_high,s->hr_seq);
    rule(s,4,bed?(now-(s->have_good_br?s->last_good_br:s->bed_since)>=s->config.missing_br_ms):-1,0,0,s->br_seq);
    rule(s,5,bed?(now-(s->have_good_hr?s->last_good_hr:s->bed_since)>=s->config.missing_hr_ms):-1,0,0,s->hr_seq);
    rule(s,6,bed?(now-s->bed_since>=s->config.long_bed_ms):-1,0,0,0);
    rule(s,7,live?(s->bed==1 && s->was_in_bed && now-s->away_since>=s->config.away_ms):-1,0,0,0);
    for(i=0;i<8;i++)if(!s->monitoring)s->rules[i].state=0;
}
static void result_body(P1State *s,uint8_t *b) {
    int live=current(s);uint32_t flags=live?0u:4u;
    memset(b,0,56);w64(b,s->uptime_ms);w32(b+8,s->generation);
    if(live) {b[12]=s->presence;b[13]=s->bed;b[14]=s->motion;b[15]=s->sleep_state;}
    w16(b+16,UINT16_MAX);w16(b+18,UINT16_MAX);w32(b+24,UNKNOWN);w32(b+28,UNKNOWN);
    w32(b+40,live?(s->bed==2?(s->now-s->bed_since)/1000u:0u):UNKNOWN);
    w32(b+44,live?(s->bed==1?(s->now-s->away_since)/1000u:0u):UNKNOWN);
    if(live && s->bed==2) {
        if(s->have_br) {w16(b+16,s->br);b[20]=(uint8_t)(s->now-s->last_br<=FRESH_MS && s->breath_valid && s->motion==1?1:2);w32(b+24,s->now-s->last_br);w32(b+32,s->br_seq);}
        if(s->have_hr && s->now-s->last_hr<=45000u) {
            w16(b+18,s->hr);b[21]=(uint8_t)(s->now-s->last_hr<=FRESH_MS && s->motion==1?1:2);
            b[22]=1;b[23]=s->hr_quality;w32(b+28,s->now-s->last_hr);w32(b+36,s->hr_seq);
            if(s->hr_harmonic)flags|=1u;
        }
        if(s->motion==2)flags|=2u;
    }
    w32(b+48,flags);
}
static size_t control_body(P1State *s,uint8_t *b) {
    memset(b,0,32);w64(b,s->boot_id);w32(b+8,s->monitor_id);w64(b+12,s->operation);
    b[20]=s->ctrl;b[21]=s->actual_running;b[22]=s->monitoring;b[23]=s->pending_action;
    w32(b+24,s->control_error);w32(b+28,s->faults);return 32;
}
static void alerts_body(P1State *s,uint8_t *b) {
    unsigned i;uint32_t active=0,unknown=0,pending=0;
    for(i=0;i<8;i++) {if(s->rules[i].active)active|=1u<<i;if(!s->rules[i].state)unknown|=1u<<i;if(s->rules[i].tracking)pending|=1u<<i;}
    memset(b,0,40);w64(b,s->boot_id);w32(b+8,s->monitor_id);w32(b+12,s->config.enabled);
    w32(b+16,active);w32(b+20,unknown);w32(b+24,pending);w32(b+28,s->event_last);w32(b+32,s->event_ack);w32(b+36,s->event_overwrites);
}
static int needs_write(unsigned id) {return id==P1_START || id==P1_STOP || id==P1_CONFIG_SET || id==P1_CONFIG_SAVE || id==P1_CONFIG_DEFAULT || id==P1_EVENT_ACK;}
size_t p1_handle(P1State *s,const uint8_t *in,size_t n,uint8_t *out,size_t cap,uint32_t now) {
    uint8_t body[P1_FRAME_MAX-16u];uint16_t id,length,code=0,detail=0;uint32_t seq;uint64_t token=0;
    size_t used=4,encoded;unsigned scoped=0;const uint8_t *data;
    if(!out || cap<P1_FRAME_MAX || !frame_valid(in,n) || in[4]!=1)return 0;
    p1_tick(s,now);id=g16(in+6);seq=g32(in+8);length=g16(in+12);data=in+14;memset(body,0,sizeof(body));
    if(id>=P1_OPEN) {scoped=1;used=12;if(length>=8)token=g64(data);w64(body+4,token);}
    if(scoped && id!=P1_OPEN) {
        if(length<8) {code=P1_BAD_LENGTH;detail=8;}
        else if(!token || token!=s->token)code=P1_SESSION_REQUIRED;
        else if(seq<=s->highest_sequence) {
            if(n==s->last_request_size && !memcmp(in,s->last_request,n)) {memcpy(out,s->last_reply,s->last_reply_size);return s->last_reply_size;}
            code=seq==s->highest_sequence?P1_CONFLICT:P1_OLD_SEQUENCE;
        } else if(needs_write(id) && !s->write_session)code=P1_BOOT_CHANGED;
        if(code)goto reply;
    }
    if(id==P1_CAPS) {
        if(length) {code=P1_BAD_LENGTH;goto reply;}
        body[4]=body[5]=1;w16(body+6,2);w32(body+8,CAPS);w16(body+12,P1_FRAME_MAX-16u);w16(body+14,P1_FRAME_MAX-16u);
        w16(body+16,P1_RESULT_PUSH);w16(body+18,56);w32(body+20,s->config.report_ms);w32(body+24,RESTRICTIONS);
        w64(body+28,s->uptime_ms);memcpy(body+36,"W803_PROTOCOL_P1_20261120",25);used=68;
    } else if(id==P1_IDENTITY) {
        if(length) {code=P1_BAD_LENGTH;goto reply;}
        w64(body+4,s->boot_id);w64(body+12,s->uptime_ms);w32(body+20,CAPS);w32(body+24,s->faults);w16(body+28,7);w16(body+30,1);used=32;
    } else if(id==P1_OPEN) {
        if(length!=8u && length!=16u) {code=P1_BAD_LENGTH;detail=16;goto reply;}
        if(!token) {code=P1_BAD_ARGUMENT;goto reply;}
        if(length==16u && (!s->boot_id || g64(data+8)!=s->boot_id)) {code=P1_BOOT_CHANGED;goto reply;}
        if(s->token==token) {
            if(seq!=s->open_sequence) {code=P1_OLD_SEQUENCE;goto reply;}
        } else {
            s->token=token;s->highest_sequence=s->open_sequence=seq;s->write_session=(uint8_t)(length==16u);
            s->last_request_size=s->last_reply_size=0;s->stream_mask=0;s->event_cursor=s->event_last;
        }
        w64(body+12,s->uptime_ms);w32(body+20,CAPS);w32(body+24,RESTRICTIONS);used=28;
        if(length==16u) {w64(body+28,s->boot_id);used=36;}
    } else if(id==P1_RESULT || id==P1_LINK_RESULT) {
        if(length!=(id==P1_RESULT?0u:8u)) {code=P1_BAD_LENGTH;detail=id==P1_RESULT?0:8;goto reply;}
        if(!s->have_sleep) {code=P1_NOT_READY;goto reply;}
        result_body(s,body+used);used+=56;
    } else if(id==P1_HEALTH) {
        if(length!=8u) {code=P1_BAD_LENGTH;detail=8;goto reply;}
        w64(body+12,s->uptime_ms);w32(body+20,s->have_sleep?now-s->last_sleep:UNKNOWN);
        w32(body+24,(s->have_sleep?1u:0u)|(current(s)?0u:2u));
        body[28]=(uint8_t)(s->source_ready?2u:1u);body[29]=(uint8_t)(s->source_ready?6u:1u);used=32;
    } else if(id==P1_START || id==P1_STOP || id==P1_CONTROL) {
        if(length!=8u) {code=P1_BAD_LENGTH;detail=8;goto reply;}
        if(id!=P1_CONTROL)code=(uint16_t)p1_begin_control(s,id==P1_START?1u:0u,now);
        if(!code)used+=control_body(s,body+used);
    } else if(id==P1_SNAPSHOT) {
        if(length!=8u) {code=P1_BAD_LENGTH;detail=8;goto reply;}
        w64(body+12,s->boot_id);w32(body+20,s->monitor_id);w32(body+24,s->faults);result_body(s,body+28);used=84;
    } else if(id==P1_STREAM) {
        if(length!=12u) {code=P1_BAD_LENGTH;detail=12;goto reply;}
        if(g32(data+8)&~3u) {code=P1_BAD_ARGUMENT;goto reply;}
        s->stream_mask=(uint8_t)g32(data+8);w32(body+12,s->stream_mask);used=16;
    } else if(id==P1_CONFIG_GET || id==P1_CONFIG_SET || id==P1_CONFIG_DEFAULT || id==P1_CONFIG_SAVE) {
        P1Config c;uint8_t bytes[P1_CONFIG_BYTES];
        unsigned expected=id==P1_CONFIG_GET?8u:id==P1_CONFIG_SET?72u:12u;
        if(length!=expected) {code=P1_BAD_LENGTH;detail=(uint16_t)expected;goto reply;}
        if(id!=P1_CONFIG_GET) {
            uint32_t revision=id==P1_CONFIG_SET?g32(data+12):g32(data+8);
            if(!s->source_ready || !s->install_known || s->ctrl==P1_CONTROL_FAULT) {code=P1_NOT_READY;goto reply;}
            if(s->actual_running || s->monitoring || s->ctrl==P1_PENDING) {code=P1_BUSY;goto reply;}
            if(revision!=s->config.revision) {code=P1_CONFLICT;goto reply;}
            if(id==P1_CONFIG_SAVE) {
                p1_config_encode(&s->config,bytes);
                if(!s->stored && (!s->store || !s->store(bytes,sizeof(bytes),s->store_context))) {s->faults|=4u;code=P1_STORAGE;goto reply;}
                s->stored=1;s->faults&=~4u;
            } else {
                if(s->config.revision==UINT32_MAX) {code=P1_CONFLICT;goto reply;}
                if(id==P1_CONFIG_SET) {if(!p1_config_decode(data+8,64,&c)) {code=P1_BAD_ARGUMENT;goto reply;}}
                else p1_config_default(&c);
                c.revision=s->config.revision+1u;s->config=c;s->stored=0;end_rules(s,2);
            }
        }
        p1_config_encode(&s->config,body+12);body[76]=s->stored;used=77;
    } else if(id==P1_ALERTS) {
        if(length!=8u) {code=P1_BAD_LENGTH;detail=8;goto reply;}
        alerts_body(s,body+12);used=52;
    } else if(id==P1_LIMITS) {
        if(length!=8u) {code=P1_BAD_LENGTH;detail=8;goto reply;}
        w64(body+12,s->boot_id);w16(body+20,120);w16(body+22,240);w16(body+24,480);w16(body+26,1200);
        w16(body+28,200);w16(body+30,5000);w32(body+32,3);used=36;
    } else if(id==P1_INSTALL) {
        if(length!=8u) {code=P1_BAD_LENGTH;detail=8;goto reply;}
        w64(body+12,s->boot_id);body[20]=s->install_known;body[21]=s->install_enabled;
        w16(body+22,s->install_height);w16(body+24,s->install_pitch);w16(body+26,(uint16_t)(install_matches(s)?0u:1u));
        w32(body+28,s->install_known?now-s->install_at:UNKNOWN);w32(body+32,s->config.revision);used=36;
    } else if(id==P1_EVENTS) {
        uint32_t after,oldest,start,number,count=0;
        if(length!=12u) {code=P1_BAD_LENGTH;detail=12;goto reply;}
        after=g32(data+8);oldest=s->event_count?s->event_last-s->event_count+1u:0u;
        if(after>s->event_last) {code=P1_BAD_ARGUMENT;goto reply;}
        start=after+1u;if(start<oldest)start=oldest;
        w64(body+12,s->boot_id);w32(body+20,s->monitor_id);w32(body+24,oldest);w32(body+28,s->event_last);
        w32(body+32,s->event_overwrites);w32(body+36,(uint32_t)(oldest && after<oldest-1u));used=44;
        for(number=start;s->event_count && number<=s->event_last && count<5u;number++,count++) {
            memcpy(body+used,s->events[(number-1u)%P1_EVENT_CAP],P1_EVENT_BYTES);used+=P1_EVENT_BYTES;
        }
        w32(body+40,count);
    } else if(id==P1_EVENT_ACK) {
        if(length!=12u) {code=P1_BAD_LENGTH;detail=12;goto reply;}
        if(g32(data+8)>s->event_last || g32(data+8)<s->event_ack) {code=P1_BAD_ARGUMENT;goto reply;}
        s->event_ack=g32(data+8);w32(body+12,s->event_ack);used=16;
    } else if(id==P1_STATS) {
        if(length!=8u) {code=P1_BAD_LENGTH;detail=8;goto reply;}
        memset(body+12,0,56);w64(body+12,s->boot_id);w32(body+20,s->monitor_id);w32(body+24,s->monitor_start);
        w32(body+28,s->monitor_stop);w32(body+32,s->monitor_id?((s->monitoring?now:s->monitor_stop)-s->monitor_start):0);
        w32(body+36,s->in_bed_ms);w32(body+40,s->active_ms);w32(body+44,s->br_count);w32(body+48,s->hr_count);
        w16(body+52,s->br_count?(uint16_t)(s->br_sum/s->br_count):UINT16_MAX);
        w16(body+54,s->hr_count?(uint16_t)(s->hr_sum/s->hr_count):UINT16_MAX);w32(body+56,s->events_total);
        body[60]=s->monitoring;used=68;
    } else code=P1_UNSUPPORTED;
reply:
    if(code)used=scoped?12u:4u;
    w16(body,code);w16(body+2,detail);encoded=p1_frame(2,id,seq,body,used,out,cap);
    if(scoped && id!=P1_OPEN && token && token==s->token && seq>s->highest_sequence) {
        s->highest_sequence=seq;s->last_request_size=(uint16_t)n;s->last_reply_size=(uint16_t)encoded;
        memcpy(s->last_request,in,n);memcpy(s->last_reply,out,encoded);
    }
    return encoded;
}
size_t p1_push(P1State *s,uint8_t *out,size_t cap,uint32_t now) {
    uint8_t b[80];unsigned id=0;size_t n=0;
    p1_tick(s,now);memset(b,0,sizeof(b));
    if(now-s->last_heartbeat_send>=s->config.heartbeat_ms) {
        s->last_heartbeat_send=now;w64(b,s->boot_id);w64(b+8,s->uptime_ms);w64(b+16,s->token);
        w32(b+24,s->monitor_id);w32(b+28,s->faults);b[32]=s->ctrl;b[33]=s->monitoring;id=P1_HEARTBEAT;n=36;
    } else if((s->stream_mask&2u) && s->token && s->event_cursor<s->event_last) {
        uint32_t oldest=s->event_last-s->event_count+1u;
        s->event_cursor++;if(s->event_cursor<oldest)s->event_cursor=oldest;
        w64(b,s->boot_id);w64(b+8,s->token);memcpy(b+16,s->events[(s->event_cursor-1u)%P1_EVENT_CAP],P1_EVENT_BYTES);
        id=P1_EVENT_PUSH;n=48;
    } else if((s->stream_mask&1u) && s->token && now-s->last_result_send>=s->config.report_ms) {
        s->last_result_send=now;result_body(s,b);id=P1_RESULT_PUSH;n=56;
    }
    if(!id)return 0;
    s->push_sequence=next(s->push_sequence);return p1_frame(4,id,s->push_sequence,b,n,out,cap);
}
