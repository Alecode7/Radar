#include "product_runtime.h"
#include "product_protocol.h"
#include "product_board.h"
#include "wm_osal.h"
#include "freertos/task.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static P1State state;
static P1Parser parser;
static SemaphoreHandle_t mutex,intake_mutex;
static ShTransport *intake;
static wm_device_t *host,*radar;
static uint8_t response[P1_FRAME_MAX];
static uint32_t known_monitor,last_core_sequence,last_status_poll;
static unsigned known_monitoring,known_control;
static uint32_t now_ms(void) {return wm_os_internal_get_time_ms();}
static int number(const char *line,const char *key,unsigned long *value) {
    const char *p=line;size_t n=strlen(key);char *end;
    while((p=strstr(p,key))!=NULL) {
        if((p==line || p[-1]==' ') && p[n]=='=') {
            if(p[n+1]<'0' || p[n+1]>'9')return 0;
            *value=strtoul(p+n+1,&end,10);return *end==0 || *end==' ' || *end=='\r' || *end=='\n';
        }
        p++;
    }
    return 0;
}
static int hex64(const char *text,uint64_t *out) {
    unsigned i;uint64_t value=0;
    for(i=0;i<16u;i++) {
        unsigned char c=(unsigned char)text[i];unsigned d;
        if(c>='0' && c<='9')d=c-'0';else if(c>='a' && c<='f')d=c-'a'+10u;
        else if(c>='A' && c<='F')d=c-'A'+10u;else return 0;
        value=(value<<4)|d;
    }
    if(text[16] && text[16]!=' ')return 0;
    *out=value;return 1;
}
static void send_host(const uint8_t *bytes,size_t n) {
    size_t sent=0;uint32_t start=now_ms();
    while(sent<n && now_ms()-start<200u) {
        int count=wm_drv_uart_write(host,bytes+sent,(uint32_t)(n-sent));
        if(count<0 || (size_t)count>n-sent)break;
        if(count==0)vTaskDelay(1);else sent+=(size_t)count;
    }
    if(sent!=n) {xSemaphoreTake(mutex,portMAX_DELAY);state.faults|=16u;xSemaphoreGive(mutex);}
}
static void received(const uint8_t *bytes,size_t n,void *context) {
    size_t size;(void)context;
    xSemaphoreTake(mutex,portMAX_DELAY);size=p1_handle(&state,bytes,n,response,sizeof(response),now_ms());xSemaphoreGive(mutex);
    if(size)send_host(response,size);
}
static void synchronize(void) {
    uint32_t monitor;unsigned monitoring,control;
    xSemaphoreTake(mutex,portMAX_DELAY);
    monitor=state.monitor_id;monitoring=state.monitoring;control=state.ctrl;
    xSemaphoreGive(mutex);
    if(monitor!=known_monitor || monitoring!=known_monitoring ||
       (control==P1_PENDING && known_control!=P1_PENDING && !monitoring)) {
        xSemaphoreTake(intake_mutex,portMAX_DELAY);
        sh_event(intake,monitoring?RXL_START:RXL_STOP);
        xSemaphoreGive(intake_mutex);
        known_monitor=monitor;known_monitoring=monitoring;
    }
    known_control=control;
}
static void service_once(void) {
    uint8_t buffer[128];char command[80];
        uint32_t now;uint64_t op=0;unsigned action=0,mount=0,height=0,pitch=0;int have,count;size_t size;
        count=wm_drv_uart_read(host,buffer,sizeof(buffer),20);now=now_ms();
        if(count>0)p1_parser_feed(&parser,buffer,(size_t)count,now,received,NULL);
        else if(count<0) {xSemaphoreTake(mutex,portMAX_DELAY);state.faults|=16u;xSemaphoreGive(mutex);}
        p1_parser_poll(&parser,now_ms(),received,NULL);
        xSemaphoreTake(mutex,portMAX_DELAY);
        now=now_ms(); /* callbacks/TX may have advanced time since UART read */
        p1_tick(&state,now);
        if(state.boot_id && state.ctrl!=P1_PENDING && now-last_status_poll>=5000u) {
            (void)p1_begin_control(&state,2,now);last_status_poll=now;
        }
        have=p1_control_due(&state,now,&op,&action);
        mount=state.config.mount_enabled;height=state.config.mount_height;pitch=state.config.mount_pitch;
        size=p1_push(&state,response,sizeof(response),now);
        xSemaphoreGive(mutex);
        synchronize();
        if(have) {
            int n;
            if(action==1u || action==3u)n=snprintf(command,sizeof(command),"mmwc pctl %08lx%08lx %u %u %u %u\r\n",(unsigned long)(op>>32),(unsigned long)op,action,mount,height,pitch);
            else n=snprintf(command,sizeof(command),"mmwc pctl %08lx%08lx %u\r\n",(unsigned long)(op>>32),(unsigned long)op,action);
            if(n>0 && (size_t)n<sizeof(command))(void)wm_drv_uart_write(radar,(const uint8_t *)command,(uint32_t)n);
        }
        if(size)send_host(response,size);
        if(count<=0)vTaskDelay(1);
}
static void task(void *arg) {(void)arg;for(;;)service_once();}
int product_runtime_init(wm_device_t *radar_device,SemaphoreHandle_t lock,ShTransport *transport) {
    uint64_t boot=0;uint8_t config[64];int opened,loaded;
    radar=radar_device;intake_mutex=lock;intake=transport;mutex=xSemaphoreCreateMutex();
    if(!mutex)return -1;
    opened=product_board_open(&host,&boot);if(opened==-1)return -2;
    p1_init(&state,boot,now_ms(),product_board_save,NULL);p1_parser_init(&parser);
    loaded=product_board_load(config,sizeof(config));
    if(loaded>0)(void)p1_load_config(&state,config,sizeof(config));else if(loaded<0)state.faults|=4u;
    xSemaphoreTake(intake_mutex,portMAX_DELAY);sh_event(intake,RXL_STOP);xSemaphoreGive(intake_mutex);
    if(xTaskCreate(task,"product",3072,NULL,3,NULL)!=pdPASS)return -3;
    return opened;
}
void product_runtime_line(const char *line,int input_status) {
    unsigned long p,b,a,ss,bv,seq,rate,age;uint32_t now=now_ms(),epoch;const char *found;unsigned context=0;
    if(!mutex)return;
    xSemaphoreTake(intake_mutex,portMAX_DELAY);epoch=intake->intake.status.epoch;
    xSemaphoreTake(mutex,portMAX_DELAY);
    if(strstr(line,"FW_VERIFY_VERSION=")) {
        p1_source(&state,strstr(line,"UART_RADAR_PROTOCOL_20261120_V29")!=NULL,now);
        if(state.source_ready) {(void)p1_begin_control(&state,2,now);last_status_poll=now;}
    } else if((found=strstr(line,"PCTL V=1 OP="))!=NULL) {
        uint64_t op;unsigned action,running;int ret,matched=0,clock_known=0;unsigned long clock_ok=0;
        found+=12;
        if(hex64(found,&op) && sscanf(found+16," ACTION=%u RET=%d RUN=%u",&action,&ret,&running)==3) {
            matched=state.ctrl==P1_PENDING && op==state.operation && action==state.pending_action;
            clock_known=number(found,"CLOCK_OK",&clock_ok);
            if(matched && number(found,"MOUNT",&seq) && number(found,"HEIGHT",&rate) && number(found,"PITCH",&age)) {
                p1_install_observation(&state,(unsigned)seq,(unsigned)rate,(unsigned)age,now);
                if(action==2u && !ret && state.install_known && clock_known && clock_ok==1u)state.source_ready=state.source_seen=1;
            }
            p1_control_ack(&state,op,action,ret,running,now);
            if(matched && clock_known && !clock_ok)p1_source(&state,0,now);
        }
    } else if(input_status>=0 && strncmp(line,"SLEEP ",6)==0) {
        if(number(line,"P",&p) && number(line,"BED",&b) && number(line,"A",&a) && number(line,"SS",&ss) && number(line,"BV",&bv) &&
           p<=1 && b<=1 && a<=1 && ss<=3 && bv<=1) {
            p1_sleep(&state,(unsigned)p,(unsigned)b,(unsigned)a,(unsigned)ss,(unsigned)bv,now);context=1;
        }
    } else if(strncmp(line,"BRC ",4)==0) {
        if(number(line,"SEQ",&seq) && number(line,"RATE",&rate) && number(line,"AGE_MS",&age) && rate<=80)
            p1_breath(&state,(uint32_t)seq,(uint16_t)rate,(uint32_t)age,now);
    } else if(strncmp(line,"HRSTREAM ",9)==0) {
        if((number(line,"TXDROP",&seq) && seq) || (number(line,"FORMAT_DROP",&seq) && seq))state.faults|=64u;
        if(number(line,"CLOCK_OK",&seq) && !seq)p1_source(&state,0,now);
    }
    p1_input_epoch(&state,epoch,now,context);
    xSemaphoreGive(mutex);
    xSemaphoreGive(intake_mutex);
}
void product_runtime_heart(const ShReport *r) {
    if(!mutex || !r || !r->sequence || r->sequence==last_core_sequence)return;
    last_core_sequence=r->sequence;
    xSemaphoreTake(intake_mutex,portMAX_DELAY);
    if(!sh_epoch_current(intake,r->epoch) || intake->mode!=r->mode) {xSemaphoreGive(intake_mutex);return;}
    xSemaphoreTake(mutex,portMAX_DELAY);
    p1_input_epoch(&state,r->epoch,now_ms(),0);
    p1_heart(&state,r->raw.bpm,r->raw.quality,r->raw.harmonic_conflict,r->raw.fresh,r->age_ms,r->epoch,now_ms());
    xSemaphoreGive(mutex);
    xSemaphoreGive(intake_mutex);
}
int product_runtime_control(unsigned action) {
    int result;
    if(!mutex)return P1_NOT_READY;
    xSemaphoreTake(mutex,portMAX_DELAY);result=p1_begin_control(&state,action,now_ms());xSemaphoreGive(mutex);return result;
}
int product_runtime_busy(void) {
    int busy;if(!mutex)return 0;
    xSemaphoreTake(mutex,portMAX_DELAY);busy=state.monitoring || state.ctrl==P1_PENDING;xSemaphoreGive(mutex);return busy;
}
