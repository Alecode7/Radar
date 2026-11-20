#include "radar_iq.h"
#include <string.h>

enum { V,N,GEN,F,MS,BIN,FLAGS,BR,I0,Q0,I1,Q1,I2,Q2,ENQ,FS,FIELD_COUNT };
static const char *const keys[FIELD_COUNT]={"V","N","GEN","F","MS","BIN","FLAGS","BR","I0","Q0","I1","Q1","I2","Q2","ENQ","FS"};
_Static_assert(sizeof(RadarIq)==36u,"Unexpected RadarIq ABI");

size_t radar_iq_size(void) { return sizeof(RadarIq); }
static int space(unsigned char c) { return c==' ' || c=='\t' || c=='\r' || c=='\n'; }
static int decimal(const char *begin,const char *end,unsigned field,uint32_t *value) {
    uint32_t number=0,digit;int negative=0;
    if(begin==end) return 0;
    if(*begin=='-') { negative=1;begin++; }
    if(begin==end || (negative && (field<I0 || field>Q2))) return 0;
    while(begin<end) {
        if(*begin<'0' || *begin>'9') return 0;
        digit=(uint32_t)(*begin-'0');
        if(number>(UINT32_MAX-digit)/10u) return 0;
        number=number*10u+digit;begin++;
    }
    if(field>=I0 && field<=Q2) {
        if(number>(negative?32768u:32767u)) return 0;
        *value=negative?(uint32_t)(-(int32_t)number):number;
    } else *value=number;
    return 1;
}
int radar_iq_parse(const char *line,size_t length,RadarIq *output) {
    const char *p,*end,*start,*equals;uint32_t values[FIELD_COUNT]={0},seen=0;unsigned i,found=0;RadarIq result;
    if(line==NULL || output==NULL || length>=RADAR_IQ_MAX_LINE) return RADAR_IQ_MALFORMED;
    p=line;end=line+length;
    for(i=0;i<length;i++) {
        unsigned char c=(unsigned char)line[i];
        if((c<32u && !space(c)) || c>126u) return RADAR_IQ_MALFORMED;
    }
    /* Accept an optional shell/log prefix, but only an exact HRIN token. */
    while(p<end) {
        while(p<end && space((unsigned char)*p)) p++;
        start=p;while(p<end && !space((unsigned char)*p)) p++;
        if(p-start==4 && memcmp(start,"HRIN",4u)==0) { found=1;break; }
    }
    if(!found) return RADAR_IQ_NOT_INPUT;
    while(p<end) {
        while(p<end && space((unsigned char)*p)) p++;
        if(p==end) break;
        start=p;while(p<end && !space((unsigned char)*p)) p++;
        equals=(const char *)memchr(start,'=',(size_t)(p-start));
        if(equals==NULL || equals==start) return RADAR_IQ_MALFORMED;
        for(i=0;i<FIELD_COUNT;i++) {
            size_t n=strlen(keys[i]);
            if((size_t)(equals-start)==n && memcmp(start,keys[i],n)==0) break;
        }
        if(i==FIELD_COUNT) continue;
        if((seen&(1u<<i))!=0u || !decimal(equals+1,p,i,&values[i])) return RADAR_IQ_MALFORMED;
        seen|=1u<<i;
    }
    if((seen&((1u<<FS)-1u))!=((1u<<FS)-1u)) return RADAR_IQ_MALFORMED;
    if(values[V]<25u || values[V]>27u) return RADAR_IQ_UNSUPPORTED;
    if((seen&(1u<<FS))!=0u && values[FS]!=5u) return RADAR_IQ_UNSUPPORTED;
    if(values[BIN]>UINT16_MAX || values[FLAGS]>7u || values[BR]>UINT8_MAX || values[ENQ]>1u) return RADAR_IQ_MALFORMED;
    memset(&result,0,sizeof(result));
    result.frame=values[F];result.source_ms=values[MS];result.generation=values[GEN];result.sequence=values[N];
    for(i=0;i<6u;i++) result.iq[i]=(int16_t)(int32_t)values[I0+i];
    result.bin=(uint16_t)values[BIN];result.flags=(uint8_t)values[FLAGS];result.breath=(uint8_t)values[BR];
    result.source_version=(uint8_t)values[V];result.source_enqueued=(uint8_t)values[ENQ];result.sample_rate=5u;
    *output=result;return RADAR_IQ_OK;
}
