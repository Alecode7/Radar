#include "link_stream.h"
#include <string.h>

size_t link_stream_size(void) { return sizeof(LinkStream); }
void link_stream_init(LinkStream *s) { memset(s,0,sizeof(*s)); }
void link_stream_stats(const LinkStream *s,LinkStats *stats) { *stats=s->stats; }
uint32_t link_line_hash(const char *text,uint32_t size) {
    uint32_t h=2166136261u,i;
    for(i=0;i<size;i++) { h^=(uint8_t)text[i];h*=16777619u; }
    return h;
}
static const char *token(const char *text,const char *key) {
    const char *p=text;
    while((p=strstr(p,key))!=NULL) {
        if(p==text || p[-1]==' ' || p[-1]=='\t') return p;
        p++;
    }
    return NULL;
}
static int number(const char *text,const char *key,uint32_t *out) {
    const char *p=token(text,key);uint32_t n=0,d;
    if(p==NULL) return 0;
    p+=strlen(key);
    if(*p<'0' || *p>'9') return 0;
    while(*p>='0' && *p<='9') {
        d=(uint32_t)(*p-'0');
        if(n>(UINT32_MAX-d)/10u) return 0;
        n=n*10u+d;p++;
    }
    if(*p!='\0' && *p!=' ' && *p!='\t') return 0;
    *out=n;return 1;
}
static void finish(LinkStream *s,link_line_fn fn,void *context) {
    uint32_t kind=LINK_OTHER,index,frame,delta,ms;
    if(s->discard || !s->used) { s->discard=0;s->used=0;return; }
    s->line[s->used]='\0';s->stats.lines++;
    if(strstr(s->line,"FW_VERIFY_VERSION=")!=NULL) {
        kind=LINK_VERSION;
        s->stats.have_frame[0]=s->stats.have_frame[1]=0;
    } else if(token(s->line,"HRRAW ")!=NULL) kind=LINK_RAW;
    else if(token(s->line,"HRIN ")!=NULL) kind=LINK_INPUT;
    else if(token(s->line,"SLEEP ")!=NULL) kind=LINK_SLEEP;
    s->stats.kind_count[kind]++;
    if(kind==LINK_RAW || kind==LINK_INPUT) {
        index=kind==LINK_RAW?0u:1u;
        if(number(s->line,"F=",&frame)) {
            if(s->stats.have_frame[index]) {
                delta=frame-s->stats.last_frame[index];
                if(delta==0u) s->stats.duplicates[index]++;
                else if(delta<0x80000000u) s->stats.frame_gaps[index]+=delta-1u;
                else s->stats.backwards[index]++;
            }
            s->stats.last_frame[index]=frame;s->stats.have_frame[index]=1;
        } else s->stats.bad_frames[index]++;
        if(kind==LINK_INPUT && number(s->line,"MS=",&ms)) {
            s->stats.last_source_ms=ms;s->stats.have_source_ms=1;
        }
    }
    if(fn!=NULL) fn(context,s->line,s->used,kind);
    s->used=0;
}
void link_stream_feed(LinkStream *s,const uint8_t *data,size_t size,link_line_fn fn,void *context) {
    size_t i;unsigned char ch;
    for(i=0;i<size;i++) {
        ch=data[i];s->stats.bytes++;
        if(ch=='\n') { finish(s,fn,context);continue; }
        if(ch=='\r') continue;
        if(ch!='\t' && (ch<32u || ch>126u)) { s->stats.binary_bytes++;s->discard=1;continue; }
        if(s->discard) continue;
        if(s->used+1u>=LINK_LINE_CAP) { s->stats.overlong++;s->discard=1;continue; }
        s->line[s->used++]=(char)ch;
    }
}
