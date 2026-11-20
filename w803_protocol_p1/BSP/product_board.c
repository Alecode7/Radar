#include "product_board.h"
#include "board_config.h"
#include "wm_drv_rng.h"
#include "wm_error.h"
#include "wm_nvs.h"
#include "product_protocol.h"
#include <string.h>
static wm_nvs_handle_t storage;
static unsigned storage_open,slot;
static uint32_t stored_generation;
static uint32_t get32(const uint8_t *p) {return p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static void put32(uint8_t *p,uint32_t v) {unsigned i;for(i=0;i<4u;i++)p[i]=(uint8_t)(v>>(8u*i));}
static int valid(const uint8_t *b,size_t n) {
    P1Config c;
    return n==80 && get32(b)==0x31474643u && get32(b+4) &&
        b[78]==0 && b[79]==0 && p1_crc(b,76)==(uint16_t)(b[76]|((uint16_t)b[77]<<8)) &&
        get32(b+72)==~get32(b+4) && p1_config_decode(b+8,64,&c);
}
int product_board_open(wm_device_t **device,uint64_t *boot) {
    wm_drv_uart_config_t cfg;uint64_t random[2]={0,0};unsigned attempt;
    *boot=0;
    *device=wm_drv_uart_init(LINK_PRODUCT_UART,LINK_PRODUCT_RX_BYTES,LINK_PRODUCT_TX_BYTES);
    if(!*device || wm_drv_uart_get_config(*device,&cfg)!=WM_ERR_SUCCESS || cfg.baudrate!=LINK_BAUD)return -1;
    /* The device table selects the true hardware RNG, not a fixed PRNG seed. */
    for(attempt=0;attempt<4u;attempt++) {
        if(wm_drv_fill_random(random,sizeof(random))==WM_ERR_SUCCESS && random[0] && random[0]!=UINT64_MAX && random[0]!=random[1]) {
            *boot=random[0];break;
        }
    }
    return *boot?0:-2;
}
int product_board_load(uint8_t *out,size_t capacity) {
    uint8_t a[80],b[80];size_t na=sizeof(a),nb=sizeof(b);int va,vb,ra,rb;
    if(capacity<64)return -1;
    if(wm_nvs_open(WM_NVS_DEF_PARTITION,"rsp_p1",WM_NVS_OP_READ_WRITE,&storage)!=WM_ERR_SUCCESS)return -1;
    storage_open=1;
    ra=wm_nvs_get_binary(storage,"cfg_a",a,&na);rb=wm_nvs_get_binary(storage,"cfg_b",b,&nb);
    va=ra==WM_ERR_SUCCESS && valid(a,na);vb=rb==WM_ERR_SUCCESS && valid(b,nb);
    if(!va && !vb) {slot=0;stored_generation=0;return (ra==WM_ERR_SUCCESS || rb==WM_ERR_SUCCESS)?-2:0;}
    slot=(unsigned)(vb && (!va || get32(b+4)>get32(a+4)));
    stored_generation=get32((slot?b:a)+4);memcpy(out,(slot?b:a)+8,64);return 1;
}
int product_board_save(const uint8_t *config,size_t n,void *context) {
    uint8_t record[80],verify[80];size_t size=sizeof(verify);uint16_t crc;const char *key;
    (void)context;
    if(!storage_open || n!=64 || stored_generation==UINT32_MAX)return 0;
    memset(record,0,sizeof(record));put32(record,0x31474643u);put32(record+4,stored_generation+1u);
    memcpy(record+8,config,64);put32(record+72,~(stored_generation+1u));crc=p1_crc(record,76);
    record[76]=(uint8_t)crc;record[77]=(uint8_t)(crc>>8);key=slot?"cfg_a":"cfg_b";
    if(wm_nvs_set_binary(storage,key,record,sizeof(record))!=WM_ERR_SUCCESS ||
       wm_nvs_get_binary(storage,key,verify,&size)!=WM_ERR_SUCCESS || size!=sizeof(record) ||
       memcmp(record,verify,sizeof(record)) || !valid(verify,size))return 0;
    stored_generation++;slot^=1u;return 1;
}
