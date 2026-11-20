#include "rx_state.h"
#include <string.h>
size_t rxl_size(void) { return sizeof(RxlState); }
size_t rxl_status_size(void) { return sizeof(RxlStatus); }
static void discontinuity(RxlState *s) {
 s->head=0;s->status.count=0;s->status.have_source=0;
 s->status.epoch++;s->status.resets++;
}
void rxl_init(RxlState *s) {
 memset(s,0,sizeof(*s));s->status.epoch=1;s->status.bed=2u;
}
void rxl_event(RxlState *s,unsigned event) {
 discontinuity(s);s->blocked=0;s->status.bed=2u;
 if(event==RXL_VERSION)s->status.versions++;
 if(event==RXL_STOP)s->status.paused=1;
 if(event==RXL_START)s->status.paused=0;
}
int rxl_push(RxlState *s,const RadarIq *p,uint32_t received_ms) {
 uint32_t dn,df,dt;
 if(s->status.paused)return 0;
 if(p->sample_rate!=5u || (p->flags&3u)!=3u) {
  s->status.bad++;discontinuity(s);return -1;
 }
 if(s->blocked) {
  if(p->generation==s->blocked_generation)return 0;
  s->blocked=0;
 }
 if(s->status.have_source) {
  if(p->generation!=s->status.generation)discontinuity(s);
  else {
   dn=p->sequence-s->status.n;df=p->frame-s->status.frame;dt=p->source_ms-s->status.source_ms;
   if(!dn && !df && !dt) {
    const RadarIq *last=&s->history[(s->head+s->status.count-1u)%RXL_CAPACITY];
    if(memcmp(last,p,sizeof(*p))!=0) { s->status.bad++;discontinuity(s);return -1; }
    s->status.duplicates++;return 0;
   }
   /* Frame wrap/reset is a discontinuity; source MS alone may wrap normally. */
   if((p->frame<s->status.frame && df<0x80000000u) ||
      (p->sequence<s->status.n && dn<0x80000000u))discontinuity(s);
   else if(dn>=0x80000000u && df>=0x80000000u && dt>=0x80000000u)discontinuity(s);
   else if(!dn || !df || !dt || dn>=0x80000000u || df>=0x80000000u || dt>=0x80000000u) {
    s->status.bad++;discontinuity(s);return -1;
   } else {
    if(df>1u)s->status.frame_gaps+=df-1u;
    if(dn>1u) { s->status.n_gaps+=dn-1u;discontinuity(s); }
   }
  }
 }
 if(s->status.count==RXL_CAPACITY)s->head=(s->head+1u)%RXL_CAPACITY;
 else s->status.count++;
 s->history[(s->head+s->status.count-1u)%RXL_CAPACITY]=*p;
 s->status.accepted++;s->status.have_source=1;s->status.bed=1;
 s->status.source_ms=p->source_ms;s->status.frame=p->frame;
 s->status.n=p->sequence;s->status.generation=p->generation;s->status.received_ms=received_ms;
 return 1;
}
static int sleep_bed(const char *line,size_t size) {
 size_t i;int found=-1;
 for(i=0;i+4u<=size;i++) {
  if(i && line[i-1u]!=' ' && line[i-1u]!='\t')continue;
  if(memcmp(line+i,"BED=",4u)!=0)continue;
  if(found!=-1 || i+5u>size || (line[i+4u]!='0' && line[i+4u]!='1'))return -1;
  if(i+5u<size && line[i+5u]!=' ' && line[i+5u]!='\t' && line[i+5u]!='\r' && line[i+5u]!='\n')return -1;
  found=line[i+4u]-'0';
 }
 return found;
}
int rxl_line(RxlState *s,const char *line,size_t size,unsigned kind,uint32_t received_ms) {
 RadarIq sample;int value;
 if(kind==1u) { rxl_event(s,RXL_VERSION);return 0; }
 if(kind==3u) {
  value=radar_iq_parse(line,size,&sample);
  if(value==RADAR_IQ_OK)return rxl_push(s,&sample,received_ms);
  s->status.bad++;discontinuity(s);return -1;
 }
 if(kind==4u) {
  value=sleep_bed(line,size);
  if(value<0) { s->status.bad++;discontinuity(s);return -1; }
  if(!value) {
   if(s->status.have_source) { s->blocked=1;s->blocked_generation=s->status.generation; }
   if(s->status.bed!=0u || s->status.count) { discontinuity(s);s->status.bed0++; }
   s->status.bed=0;
  } else s->status.bed=1;
 }
 return 0;
}
void rxl_status(const RxlState *s,uint32_t now_ms,RxlStatus *out) {
 *out=s->status;
 out->age_ms=out->have_source?now_ms-out->received_ms:UINT32_MAX;
 out->state=out->paused?3u:out->bed==0u?2u:!out->have_source?0u:out->age_ms>=RXL_SILENCE_MS?4u:1u;
}
size_t rxl_snapshot(const RxlState *s,RadarIq *out,size_t capacity,uint32_t *epoch) {
 size_t i,count=s->status.count;
 if(epoch)*epoch=s->status.epoch;
 if(capacity<count)return 0;
 for(i=0;i<count;i++)out[i]=s->history[(s->head+(uint32_t)i)%RXL_CAPACITY];
 return count;
}
