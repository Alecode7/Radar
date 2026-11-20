#define LOG_TAG "W803_LINK"
#include "board_config.h"
#include "board_uart.h"
#include "link_stream.h"
#include "snapshot_heart.h"
#include "wm_log.h"
#include "wm_heap.h"
#include "wm_osal.h"
#include "wm_drv_rcc.h"
#include "wm_dt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

typedef struct { uint32_t sequence,length,kind,hash;char text[LINK_LINE_CAP]; } Line;
typedef struct {
 uint32_t epoch,start,last,last_yield,phase,checks,cancel;
 uint32_t phase_ms[HR_PHASE_COUNT],lock_ms,yield_ms,kernel_ms,account_ms,copy_ms,ingest_ms,task_ticks,stack_bytes;
} ComputeContext;
typedef struct {
 ShTicket ticket;HrResult result;ShReport report;HrWorkStats work;ComputeContext time;
 uint32_t group,mode,input_hash;int status,commit;
} HeartEvent;
static LinkStream stream;
static ShTransport transport;
static ShEngine engine;
/* Receiver history and the stable calculation snapshot stay off task stack. */
static ShSnapshot snapshot;
static TaskHandle_t heart_handle;
static SemaphoreHandle_t intake_mutex;
static wm_device_t *radar,*console;
static QueueHandle_t lines,heart_events;
static uint32_t sequence,queue_drops,trace_skipped,read_errors,last_rx,compute_group;
static volatile uint32_t uart_errors,uart_error_mask,result_log_drops,prepare_failures,raw_trace,status_requested;
static char log_buffer[LINK_LINE_CAP+256u];

static uint32_t now_ms(void) { return wm_os_internal_get_time_ms(); }
/* Only output_task writes diagnostic UART after application startup. */
static void link_log(const char *format,...) {
 va_list args;int n;size_t count;
 memcpy(log_buffer,"W803_LINK: ",11u);
 va_start(args,format);n=vsnprintf(log_buffer+11u,sizeof(log_buffer)-13u,format,args);va_end(args);
 if(n<0)return;
 count=(size_t)n;if(count>=sizeof(log_buffer)-13u)count=sizeof(log_buffer)-14u;
 count+=11u;log_buffer[count++]='\r';log_buffer[count++]='\n';
 (void)wm_drv_uart_write(console,(const uint8_t *)log_buffer,(uint32_t)count);
}
static void queue_text(const char *text) {
 Line line;memset(&line,0,sizeof(line));line.sequence=0;line.kind=UINT32_MAX;
 line.length=(uint32_t)strlen(text);if(line.length>=LINK_LINE_CAP)line.length=LINK_LINE_CAP-1u;
 memcpy(line.text,text,line.length);
 if(xQueueSend(lines,&line,0)!=pdPASS) { taskENTER_CRITICAL();queue_drops++;taskEXIT_CRITICAL(); }
}
static void uart_event(wm_device_t *dev,wm_drv_uart_evt_t *event,void *context) {
 (void)dev;(void)context;
 if(event->type==WM_DRV_UART_RX_ERROR) { uart_errors++;uart_error_mask|=(uint32_t)event->error; }
}
static void completed_line(void *context,const char *text,uint32_t length,uint32_t kind) {
 Line line;int accepted;(void)context;
 xSemaphoreTake(intake_mutex,portMAX_DELAY);
 accepted=sh_line(&transport,text,length,kind,now_ms());
 xSemaphoreGive(intake_mutex);
 if(accepted==2 && heart_handle!=NULL)xTaskNotifyGive(heart_handle);
 line.sequence=++sequence;
 if(!raw_trace && kind!=LINK_VERSION && kind!=LINK_SLEEP &&
    strstr(text,"Command Success")==NULL && strstr(text,"FW_READY")==NULL) {
  trace_skipped++;return;
 }
 line.length=length;line.kind=kind;line.hash=link_line_hash(text,length);
 memcpy(line.text,text,length+1u);
 if(xQueueSend(lines,&line,0)!=pdPASS) { taskENTER_CRITICAL();queue_drops++;taskEXIT_CRITICAL(); }
}
static void receive_task(void *arg) {
 uint8_t buffer[512];int n;uint32_t handled=0;(void)arg;
 for(;;) {
  if(handled!=uart_errors) {
   handled=uart_errors;
   xSemaphoreTake(intake_mutex,portMAX_DELAY);sh_event(&transport,RXL_IOFAULT);xSemaphoreGive(intake_mutex);
   stream.discard=1;stream.used=0;
  }
  n=wm_drv_uart_read(radar,buffer,sizeof(buffer),20);
  if(n>0) { last_rx=now_ms();link_stream_feed(&stream,buffer,(size_t)n,completed_line,NULL); }
  else if(n<0) {
   read_errors++;
   xSemaphoreTake(intake_mutex,portMAX_DELAY);sh_event(&transport,RXL_IOFAULT);xSemaphoreGive(intake_mutex);
   stream.discard=1;stream.used=0;vTaskDelay(pdMS_TO_TICKS(10));
  }
 }
}
static void status(void) {
 RxlStatus r;ShStatus h;LinkStats s;uint32_t drops,skips,errors,mask,reads,rx,now;
 taskENTER_CRITICAL();
 link_stream_stats(&stream,&s);drops=queue_drops;skips=trace_skipped;errors=uart_errors;
 mask=uart_error_mask;reads=read_errors;rx=last_rx;
 taskEXIT_CRITICAL();
 xSemaphoreTake(intake_mutex,portMAX_DELAY);
 now=now_ms();rxl_status(&transport.intake,now,&r);sh_status(&transport,now,&h);
 xSemaphoreGive(intake_mutex);
 link_log("STATUS ID=R1 MS=%lu BYTES=%lu LINES=%lu VER=%lu RAW=%lu INPUT=%lu SLEEP=%lu DROP=%lu TRACE_RAW=%lu TRACE_SKIP=%lu UARTERR=%lu ERRMASK=%lu READERR=%lu LONG=%lu BINARY=%lu RXAGE=%lu HEAP=%u MINHEAP=%u",
  (unsigned long)now,(unsigned long)s.bytes,(unsigned long)s.lines,(unsigned long)s.kind_count[LINK_VERSION],
  (unsigned long)s.kind_count[LINK_RAW],(unsigned long)s.kind_count[LINK_INPUT],(unsigned long)s.kind_count[LINK_SLEEP],
  (unsigned long)drops,(unsigned long)raw_trace,(unsigned long)skips,(unsigned long)errors,(unsigned long)mask,
  (unsigned long)reads,(unsigned long)s.overlong,(unsigned long)s.binary_bytes,
  (unsigned long)(s.bytes?now-rx:UINT32_MAX),(unsigned)wm_heap_get_free_size(),(unsigned)wm_heap_get_minimum_ever_free_size());
 link_log("RXSTATE EPOCH=%lu STATE=%lu COUNT=%lu ACCEPT=%lu DUP=%lu BAD=%lu FGAP=%lu NGAP=%lu SRC_GEN=%lu SRC_MS=%lu SRC_N=%lu AGE=%lu HEART_COMPUTE=1",
  (unsigned long)r.epoch,(unsigned long)r.state,(unsigned long)r.count,(unsigned long)r.accepted,
  (unsigned long)r.duplicates,(unsigned long)r.bad,(unsigned long)r.frame_gaps,(unsigned long)r.n_gaps,
  (unsigned long)r.generation,(unsigned long)r.source_ms,(unsigned long)r.n,(unsigned long)r.age_ms);
 link_log("HEARTSTAT ID=R1 MODE=%lu PENDING=%lu REQUESTS=%lu TAKEN=%lu SKIPPED=%lu PUSHED=%lu RETIRED=%lu HIST_LOST=%lu SNAP_FAIL=%lu MRESET=%lu EST=%lu COMPLETE=%lu CANCEL=%lu BUDGET=%lu OVER_5S=%lu COST_MS=%lu LAG_MS=%lu RESULT_LOG_DROP=%lu PENDING_CANCEL=%lu PREP_FAIL=%lu",
  (unsigned long)h.mode,(unsigned long)h.pending,(unsigned long)h.requests,(unsigned long)h.taken,
  (unsigned long)h.skipped,(unsigned long)h.pushed,(unsigned long)h.retired,(unsigned long)h.lost,
  (unsigned long)h.snapshot_failures,(unsigned long)h.math_resets,(unsigned long)h.attempts,
  (unsigned long)h.completed,(unsigned long)h.canceled,(unsigned long)h.budget,(unsigned long)h.over_cycle,
  (unsigned long)h.last_cost,(unsigned long)h.last_lag,(unsigned long)result_log_drops,(unsigned long)h.pending_canceled,(unsigned long)prepare_failures);
 link_log("PROFILE CPU_CFG_MHZ=%d DOUBLE_BITS=%u TICK_HZ=%u RUNTIME_STATS=%u IRQ_TIME_MAY_BE_ATTRIBUTED=1 RX_PRIO=4 CTRL_PRIO=3 HEART_PRIO=2 LOG_PRIO=1",
  wm_drv_rcc_get_config_clock(wm_dt_get_device_by_name("rcc"),WM_RCC_TYPE_CPU),
  (unsigned)(8u*sizeof(double)),(unsigned)configTICK_RATE_HZ,(unsigned)configGENERATE_RUN_TIME_STATS);
}
static void print_heart(const HeartEvent *e) {
 const ComputeContext *c=&e->time;const HrResult *r=&e->result;
 link_log("HEART ORIGIN=W803 ID=R1 GROUP=%lu MODE=%lu INPUT_HASH=%08lx EPOCH=%lu GEN=%lu F=%lu N=%lu SRC_MS=%lu STATUS=%d COMMIT=%d CORE_BPM=%d REASON=%u Q=%u CORE_FRESH=%u HCON=%u CONF_MS=%lu COST_MS=%lu KERNEL_MS=%lu TASK_TICKS=%lu LOCK_MS=%lu YIELD_MS=%lu ACCOUNT_MS=%lu COPY_MS=%lu INGEST_MS=%lu LAG_MS=%lu ON_TIME=%u LIVE_AT_COMMIT=%u AGE_MS=%lu SKIPPED=%lu SEGMENTS=%lu NSAMP=%lu TRIG=%lu FREQ=%lu CHECK_MS=%lu PREP_MS=%lu WINDOW_MS=%lu DFT_MS=%lu PEAK_MS=%lu CONS_MS=%lu STACK_FREE_BYTES=%lu CANCEL=%lu",
  (unsigned long)e->group,(unsigned long)e->mode,(unsigned long)e->input_hash,
  (unsigned long)e->ticket.epoch,(unsigned long)e->ticket.sample.generation,(unsigned long)e->ticket.sample.frame,
  (unsigned long)e->ticket.sample.sequence,(unsigned long)e->ticket.sample.source_ms,e->status,e->commit,
  e->status==1?r->bpm:-1,e->status==1?r->reason:0u,e->status==1?r->quality:0u,
  e->status==1?r->fresh:0u,e->status==1?r->harmonic_conflict:0u,
  (unsigned long)(e->status==1?r->accepted_ms:UINT32_MAX),
  (unsigned long)(e->report.completed_ms-c->start),(unsigned long)c->kernel_ms,(unsigned long)c->task_ticks,
  (unsigned long)c->lock_ms,(unsigned long)c->yield_ms,(unsigned long)c->account_ms,(unsigned long)c->copy_ms,(unsigned long)c->ingest_ms,
  (unsigned long)(e->report.completed_ms-e->ticket.received_ms),
  (unsigned)(e->commit?e->report.on_time:0u),(unsigned)(e->commit?e->report.current:0u),
  (unsigned long)(e->commit?e->report.age_ms:UINT32_MAX),(unsigned long)e->report.skipped,
  (unsigned long)e->work.segments,(unsigned long)e->work.samples,(unsigned long)e->work.trig_calls,
  (unsigned long)e->work.frequencies,(unsigned long)c->phase_ms[HR_PHASE_CHECK],
  (unsigned long)c->phase_ms[HR_PHASE_PREPROCESS],(unsigned long)c->phase_ms[HR_PHASE_WINDOW],
  (unsigned long)c->phase_ms[HR_PHASE_DFT],(unsigned long)c->phase_ms[HR_PHASE_PEAKS],
  (unsigned long)c->phase_ms[HR_PHASE_CONSENSUS],(unsigned long)c->stack_bytes,(unsigned long)c->cancel);
}
static void output_task(void *arg) {
 Line line;HeartEvent event;uint32_t last=0,now;(void)arg;
 for(;;) {
  while(xQueueReceive(heart_events,&event,0)==pdPASS)print_heart(&event);
  if(xQueueReceive(lines,&line,pdMS_TO_TICKS(20))==pdPASS) {
   if(line.kind==UINT32_MAX)link_log("%s",line.text);
   else link_log("RX SEQ=%lu KIND=%lu LEN=%lu HASH=%08lx |%s",
     (unsigned long)line.sequence,(unsigned long)line.kind,(unsigned long)line.length,(unsigned long)line.hash,line.text);
  }
  now=now_ms();
  if(now-last>=LINK_STATUS_INTERVAL_MS || status_requested) { status_requested=0;status();last=now; }
 }
}
static int radar_command(const char *command) {
 uint32_t length=(uint32_t)strlen(command);int sent=wm_drv_uart_write(radar,(const uint8_t *)command,length);
 queue_text(sent==(int)length?"TX LOCAL_WRITE_OK=1 RADAR_ACK_REQUIRED=1":"TX LOCAL_WRITE_OK=0");
 return sent==(int)length;
}
static void dispatch(const char *command) {
 if(strcmp(command,"link start")==0) {
  queue_text("REQUEST START STEPS=STOP,REPORT_POINTS_0,START");
  xSemaphoreTake(intake_mutex,portMAX_DELAY);sh_event(&transport,RXL_START);
  xSemaphoreGive(intake_mutex);
  if(!radar_command("mmwc stop\r\n"))return;
  vTaskDelay(pdMS_TO_TICKS(250));
  if(!radar_command("mmwc report points 0\r\n"))return;
  vTaskDelay(pdMS_TO_TICKS(250));
  (void)radar_command("mmwc start\r\n");
 } else if(strcmp(command,"link stop")==0) {
  xSemaphoreTake(intake_mutex,portMAX_DELAY);sh_event(&transport,RXL_STOP);xSemaphoreGive(intake_mutex);
  queue_text("REQUEST STOP");(void)radar_command("mmwc stop\r\n");
 } else if(strcmp(command,"link status")==0)status_requested=1;
 else if(strcmp(command,"link trace on")==0) { raw_trace=1;queue_text("TRACE RAW=1 FULL_FORWARDING=1"); }
 else if(strcmp(command,"link trace off")==0) { raw_trace=0;queue_text("TRACE RAW=0 SUMMARY=1 SOURCE_LINES_STILL_COUNTED=1"); }
 else if(strcmp(command,"link spectrum direct")==0 || strcmp(command,"link spectrum rotate16")==0) {
  unsigned mode=strcmp(command,"link spectrum rotate16")==0?1u:0u;
  xSemaphoreTake(intake_mutex,portMAX_DELAY);(void)sh_set_mode(&transport,mode);
  xSemaphoreGive(intake_mutex);
  queue_text(mode?"SPECTRUM MODE=ROTATE16 REANCHOR=16 DOUBLE_BITS=64 KNOWN_DECISION_DIFFERENCES=1":"SPECTRUM MODE=DIRECT ORIGINAL_ARITHMETIC=1");
 } else if(strcmp(command,"link compare")==0 || strcmp(command,"link compare off")==0) {
  queue_text("COMPARE AVAILABLE=0 RELEASE=R1 SINGLE_MODE_ONLY=1");
 } else if(*command)queue_text("HELP COMMANDS=link_start,link_stop,link_status,link_trace_on/off,link_spectrum_direct/rotate16");
}
static void console_task(void *arg) {
 char command[80];uint8_t bytes[32];unsigned used=0,discard=0,i;int n;(void)arg;
 for(;;) {
  n=wm_drv_uart_read(console,bytes,sizeof(bytes),50);
  if(n<0) { vTaskDelay(pdMS_TO_TICKS(10));continue; }
  for(i=0;i<(unsigned)n;i++) {
   if(bytes[i]=='\r' || bytes[i]=='\n') {
    if(!discard && used) { command[used]='\0';dispatch(command); }
    used=discard=0;
   } else if(bytes[i]<32u || bytes[i]>126u)discard=1;
   else if(!discard) { if(used+1u<sizeof(command))command[used++]=(char)bytes[i];else discard=1; }
  }
 }
}
static uint32_t task_ticks(void) {
#if configGENERATE_RUN_TIME_STATS == 1 && configUSE_TRACE_FACILITY == 1
 TaskStatus_t info;vTaskGetInfo(NULL,&info,pdFALSE,eInvalid);return info.ulRunTimeCounter;
#else
 return 0;
#endif
}
static int checkpoint(void *context,unsigned phase) {
 ComputeContext *c=context;uint32_t before=now_ms(),after;int current;
 c->phase_ms[c->phase]+=before-c->last;c->last=before;c->phase=phase<HR_PHASE_COUNT?phase:0;c->checks++;
 xSemaphoreTake(intake_mutex,portMAX_DELAY);current=sh_epoch_current(&transport,c->epoch);
 xSemaphoreGive(intake_mutex);after=now_ms();c->lock_ms+=after-before;
 if(!current) { c->cancel=SH_CANCEL_EPOCH;return 1; }
 if(after-c->start>=SH_BUDGET_MS) { c->cancel=SH_CANCEL_BUDGET;return 1; }
 if(after-c->last_yield>=20u) { vTaskDelay(1);c->last_yield=now_ms();c->yield_ms+=c->last_yield-after; }
 return 0;
}
static void begin_timing(ComputeContext *c,uint32_t epoch,uint32_t copy_ms) {
 memset(c,0,sizeof(*c));c->epoch=epoch;c->copy_ms=copy_ms;
 vTaskDelay(1);c->task_ticks=task_ticks();c->start=c->last=c->last_yield=now_ms();
}
static void end_timing(ComputeContext *c) {
 uint32_t now=now_ms(),initial=c->task_ticks;
 c->phase_ms[c->phase]+=now-c->last;c->kernel_ms=now-c->start;
 /* Close the current running slice before reading the task's accumulated stats. */
 vTaskDelay(1);c->task_ticks=task_ticks()-initial;c->account_ms=now_ms()-now;
 c->stack_bytes=(uint32_t)uxTaskGetStackHighWaterMark(NULL)*(uint32_t)sizeof(StackType_t);
}
static void emit_event(const HeartEvent *event) {
 if(xQueueSend(heart_events,event,0)!=pdPASS)result_log_drops++;
}
static void heart_task(void *arg) {
 static HeartEvent event;
 ShPrepareInfo info;HrResult result;HrWorkStats work;ComputeContext context;
 int have,prepared,computed,committed;
 uint32_t copy_start,copy_ms,ingest_start,ingest_ms,finish,group,input_hash;(void)arg;
 for(;;) {
  copy_start=now_ms();
  xSemaphoreTake(intake_mutex,portMAX_DELAY);have=sh_take(&transport,&snapshot);
  xSemaphoreGive(intake_mutex);copy_ms=now_ms()-copy_start;
  if(have!=1) { ulTaskNotifyTake(pdTRUE,pdMS_TO_TICKS(100));continue; }
  group=++compute_group;
  input_hash=link_line_hash((const char *)snapshot.samples,snapshot.count*(uint32_t)sizeof(snapshot.samples[0]));
  ingest_start=now_ms();prepared=sh_prepare(&engine,&snapshot,&info);ingest_ms=now_ms()-ingest_start;
  xSemaphoreTake(intake_mutex,portMAX_DELAY);sh_account_prepare(&transport,&info);xSemaphoreGive(intake_mutex);
  if(prepared) {
   prepare_failures++;queue_text("ERROR SNAPSHOT_PREPARE_FAILED=1 INPUT_HISTORY_ISOLATED=1");
   xSemaphoreTake(intake_mutex,portMAX_DELAY);sh_event(&transport,RXL_IOFAULT);xSemaphoreGive(intake_mutex);continue;
  }
  begin_timing(&context,snapshot.ticket.epoch,copy_ms);memset(&result,0,sizeof(result));result.bpm=-1;
  context.ingest_ms=ingest_ms;
  computed=sh_compute(&engine,&snapshot,&result,checkpoint,&context);
  end_timing(&context);hr_core_work_stats(&engine.core,&work);
  xSemaphoreTake(intake_mutex,portMAX_DELAY);finish=now_ms();
  if(computed==-1)sh_event(&transport,RXL_IOFAULT);
  committed=sh_finish(&transport,&snapshot,&result,computed,context.start,finish,context.cancel);
  memset(&event,0,sizeof(event));(void)sh_report(&transport,finish,&event.report);
  xSemaphoreGive(intake_mutex);
  event.ticket=snapshot.ticket;event.result=result;event.time=context;event.group=group;
  event.mode=snapshot.mode;event.status=computed;event.commit=committed;event.input_hash=input_hash;
  event.report.completed_ms=finish;event.report.skipped=snapshot.skipped_before;
  event.work=work;emit_event(&event);
 }
}
int main(void) {
 int ret;
 link_stream_init(&stream);sh_init(&transport);sh_engine_init(&engine);
 intake_mutex=xSemaphoreCreateMutex();if(intake_mutex==NULL)return 0;
 ret=board_uart_open(&radar,&console);if(ret!=0) { wm_log_error("UART_INIT_FAILED RET=%d",ret);return 0; }
 link_log("BOOT ID=%s CPU_CFG_MHZ=%d LIVE_RADAR=1 HEART_COMPUTE=1 AUTO_COMMANDS=0 DEFAULT_MODE=ROTATE16 REANCHOR=16 TRACE_RAW=0 DOUBLE_BITS=%u SOURCE_CYCLE_MS=%u BUDGET_MS=%u AUTO_COMPARE=0 VERSION=1.0.0",
  LINK_FIRMWARE_ID,wm_drv_rcc_get_config_clock(wm_dt_get_device_by_name("rcc"),WM_RCC_TYPE_CPU),
  (unsigned)(8u*sizeof(double)),SH_CYCLE_MS,SH_BUDGET_MS);
 lines=xQueueCreate(LINK_QUEUE_LENGTH,sizeof(Line));heart_events=xQueueCreate(8,sizeof(HeartEvent));
 if(lines==NULL || heart_events==NULL) { wm_log_error("QUEUE_FAILED");return 0; }
 if(wm_drv_uart_register_callback(radar,uart_event,NULL)!=0) { wm_log_error("UART_CALLBACK_FAILED");return 0; }
 if(xTaskCreate(heart_task,"heart",4096,NULL,2,&heart_handle)!=pdPASS ||
    xTaskCreate(receive_task,"radarrx",3072,NULL,4,NULL)!=pdPASS ||
    xTaskCreate(output_task,"linklog",2048,NULL,1,NULL)!=pdPASS ||
    xTaskCreate(console_task,"linkcmd",1536,NULL,3,NULL)!=pdPASS) {
  wm_log_error("TASK_FAILED");return 0;
 }
 queue_text("READY RADAR_UART=3 TX=PB0 RX=PB1 PC_UART=0 BAUD=115200 NO_AUTO_START=1 ID=R1");
 return 0;
}
