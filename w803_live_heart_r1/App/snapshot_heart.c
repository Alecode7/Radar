#include "snapshot_heart.h"
#include <string.h>
_Static_assert(sizeof(HrResult)==56u,"Heart result layout changed");
_Static_assert(sizeof(ShTicket)==44u,"Ticket layout changed");
size_t sh_transport_size(void) { return sizeof(ShTransport); }
size_t sh_engine_size(void) { return sizeof(ShEngine); }
size_t sh_snapshot_size(void) { return sizeof(ShSnapshot); }
size_t sh_report_size(void) { return sizeof(ShReport); }
size_t sh_status_size(void) { return sizeof(ShStatus); }
static double source_seconds(uint32_t now,uint32_t then) {
 double d=(double)now/1000.0-(double)then/1000.0;
 return now>=then?d:d+4294967.296;
}
static void isolate(ShTransport *s) {
 if(s->have_pending)s->stats.pending_canceled++;
 s->have_pending=0;s->have_published=0;s->last_requested_ms=0;
}
void sh_init(ShTransport *s) {
 memset(s,0,sizeof(*s));rxl_init(&s->intake);s->mode=HR_DEFAULT_FREQ_ROTATION;
}
void sh_engine_init(ShEngine *e) {
 memset(e,0,sizeof(*e));hr_core_init(&e->core);hr_core_attach(&e->core,&e->workspace);
 e->mode=HR_DEFAULT_FREQ_ROTATION;
}
void sh_event(ShTransport *s,unsigned event) { rxl_event(&s->intake,event);isolate(s); }
int sh_set_mode(ShTransport *s,unsigned rotation) {
 if(rotation>1u)return -1;
 if(s->mode==rotation)return 0;
 /* Explicit mode change is a math discontinuity, not an automatic radar start. */
 sh_event(s,RXL_IOFAULT);s->mode=rotation;return 1;
}
static int accepted(ShTransport *s,uint32_t before,uint32_t received_ms) {
 const RadarIq *row=&s->intake.history[(s->intake.head+s->intake.status.count-1u)%RXL_CAPACITY];
 if(before!=s->intake.status.epoch)isolate(s);
 s->stats.received++;
 if(source_seconds(row->source_ms,s->last_requested_ms)<5.0)return 1;
 if(s->have_pending)s->stats.skipped++;
 s->ticket.sample=*row;s->ticket.epoch=s->intake.status.epoch;s->ticket.received_ms=received_ms;
 s->have_pending=1;s->last_requested_ms=row->source_ms;s->stats.requests++;
 return 2;
}
int sh_enqueue(ShTransport *s,const RadarIq *sample,uint32_t received_ms) {
 uint32_t before=s->intake.status.epoch;int value=rxl_push(&s->intake,sample,received_ms);
 if(value==1)return accepted(s,before,received_ms);
 if(before!=s->intake.status.epoch)isolate(s);
 return value;
}
int sh_line(ShTransport *s,const char *line,size_t size,unsigned kind,uint32_t received_ms) {
 uint32_t before=s->intake.status.epoch;int value=rxl_line(&s->intake,line,size,kind,received_ms);
 if(value==1)return accepted(s,before,received_ms);
 if(before!=s->intake.status.epoch)isolate(s);
 return value;
}
int sh_take(ShTransport *s,ShSnapshot *snapshot) {
 uint32_t i;
 if(!s->have_pending)return 0;
 snapshot->ticket=s->ticket;snapshot->mode=s->mode;snapshot->skipped_before=s->stats.skipped;snapshot->count=0;
 s->have_pending=0;s->stats.taken++;
 if(snapshot->ticket.epoch!=s->intake.status.epoch)return -1;
 for(i=0;i<s->intake.status.count;i++) {
  const RadarIq *row=&s->intake.history[(s->intake.head+i)%RXL_CAPACITY];
  if(row->generation==snapshot->ticket.sample.generation && row->sequence<=snapshot->ticket.sample.sequence)
   snapshot->samples[snapshot->count++]=*row;
 }
 if(!snapshot->count || snapshot->samples[snapshot->count-1u].sequence!=snapshot->ticket.sample.sequence) {
  s->stats.snapshot_failures++;s->have_published=0;return -1;
 }
 return 1;
}
int sh_epoch_current(const ShTransport *s,uint32_t epoch) {
 return s->intake.status.epoch==epoch && !s->intake.status.paused && s->intake.status.bed!=0u;
}
int sh_prepare(ShEngine *e,const ShSnapshot *snapshot,ShPrepareInfo *info) {
 uint32_t i,first=0;const RadarIq *row;
 memset(info,0,sizeof(*info));
 if(!snapshot->count || snapshot->count>RXL_CAPACITY)return -1;
 if(e->epoch!=snapshot->ticket.epoch || e->mode!=snapshot->mode) {
  sh_engine_init(e);e->epoch=snapshot->ticket.epoch;e->mode=snapshot->mode;info->reset=1;
  if(hr_core_set_spectrum_mode(&e->core,e->mode))return -1;
 }
 while(first<snapshot->count && e->have_n && snapshot->samples[first].sequence<=e->last_n)first++;
 if(first==snapshot->count)return -1;
 row=&snapshot->samples[first];
 if(e->have_n && row->sequence!=e->last_n+1u) {
  uint32_t missing=row->sequence-e->last_n-1u;
  if(source_seconds(snapshot->ticket.sample.source_ms,row->source_ms)>=75.0) {
   /* Only samples older than the original analysis history were retired. */
   info->retired=missing;
  } else {
   /* Unprocessed samples inside the required history really were lost. */
   info->lost=missing;info->reset=1;
   sh_engine_init(e);e->epoch=snapshot->ticket.epoch;e->mode=snapshot->mode;
   if(hr_core_set_spectrum_mode(&e->core,e->mode))return -1;
   first=0;
  }
 }
 for(i=first;i<snapshot->count;i++) {
  row=&snapshot->samples[i];
  if(row->generation!=snapshot->ticket.sample.generation ||
     (e->have_n && row->sequence!=e->last_n+1u && !(info->retired && i==first)))return -1;
  if(hr_core_push(&e->core,row->source_ms,row->frame,row->bin,row->flags,row->sample_rate,row->iq))return -1;
  e->last_n=row->sequence;e->have_n=1;info->pushed++;
 }
 return 0;
}
void sh_account_prepare(ShTransport *s,const ShPrepareInfo *info) {
 s->stats.pushed+=info->pushed;s->stats.retired+=info->retired;s->stats.lost+=info->lost;
 if(info->reset)s->stats.math_resets++;
 if(info->lost)s->have_published=0;
}
int sh_compute(ShEngine *e,const ShSnapshot *snapshot,HrResult *result,hr_yield_fn yield,void *context) {
 int value;
 if(e->epoch!=snapshot->ticket.epoch || !hr_core_due(&e->core,snapshot->ticket.sample.source_ms))return -1;
 if(hr_core_set_spectrum_mode(&e->core,snapshot->mode))return -1;
 e->core.last_estimate_ms=snapshot->ticket.sample.source_ms;
 value=hr_core_estimate(&e->core,snapshot->ticket.sample.breath,snapshot->ticket.sample.source_ms,result,yield,context);
 if(!value) { e->epoch=0;return -2; }
 return 1;
}
int sh_finish(ShTransport *s,const ShSnapshot *snapshot,const HrResult *result,int status,uint32_t start,uint32_t end,unsigned cancel) {
 const ShTicket *t=&snapshot->ticket;ShReport *r=&s->published;
 s->stats.attempts++;s->stats.last_cost=end-start;s->stats.last_lag=end-t->received_ms;
 if(end-start>=SH_CYCLE_MS)s->stats.over_cycle++;
 if(status!=1) {
  s->stats.canceled++;
  if(cancel==SH_CANCEL_BUDGET) { s->stats.budget++;sh_event(s,RXL_IOFAULT); }
  return 0;
 }
 s->stats.completed++;
 if(!sh_epoch_current(s,t->epoch) || s->mode!=snapshot->mode) { s->stats.canceled++;return 0; }
 memset(r,0,sizeof(*r));r->raw=*result;r->epoch=t->epoch;r->generation=t->sample.generation;
 r->frame=t->sample.frame;r->n=t->sample.sequence;r->source_ms=t->sample.source_ms;
 r->received_ms=t->received_ms;r->completed_ms=end;r->cost_ms=end-start;r->lag_ms=end-t->received_ms;
 r->on_time=(r->lag_ms<SH_CYCLE_MS);r->current=1;r->sequence=++s->stats.produced;
 r->mode=snapshot->mode;r->skipped=snapshot->skipped_before;s->have_published=1;return 1;
}
int sh_report(const ShTransport *s,uint32_t now,ShReport *out) {
 RxlStatus rx;
 memset(out,0,sizeof(*out));out->raw.bpm=-1;out->age_ms=UINT32_MAX;
 if(!s->have_published)return 0;
 *out=s->published;rxl_status(&s->intake,now,&rx);
 out->current=(uint32_t)(sh_epoch_current(s,out->epoch) && rx.state==1u && s->mode==out->mode);
 if(out->raw.accepted_ms!=UINT32_MAX)out->age_ms=(out->source_ms-out->raw.accepted_ms)+(now-out->received_ms);
 else out->age_ms=UINT32_MAX;
 out->on_time=(uint32_t)(out->on_time && now-out->received_ms<SH_CYCLE_MS);return 1;
}
void sh_status(const ShTransport *s,uint32_t now,ShStatus *out) {
 RxlStatus rx;*out=s->stats;rxl_status(&s->intake,now,&rx);
 out->epoch=rx.epoch;out->pending=s->have_pending;out->rx_state=rx.state;
 out->source_ms=rx.source_ms;out->source_n=rx.n;out->source_gen=rx.generation;out->mode=s->mode;
}
