#include "product_board.h"
#include "wm_nvs.h"
#include <string.h>
static uint8_t records[2][80];static size_t lengths[2];
static int fail_after=-1,mode;static unsigned writes;
static wm_device_t device;
wm_device_t *wm_drv_uart_init(const char *name,uint32_t rx,uint32_t tx) {(void)name;(void)rx;(void)tx;return mode==3?0:&device;}
int wm_drv_uart_get_config(wm_device_t *d,wm_drv_uart_config_t *c) {(void)d;c->baudrate=115200;return 0;}
int wm_drv_fill_random(void *out,uint32_t n) {
    uint64_t values[2]={UINT64_C(0x1122334455667788),UINT64_C(0x8877665544332211)};
    if(mode==2)return -1;if(mode==1)values[1]=values[0];
    if(n!=sizeof(values))return -1;memcpy(out,values,n);return 0;
}
int wm_nvs_open(const char *partition,const char *group,int flags,wm_nvs_handle_t *out) {
    if(strcmp(partition,"nvs") || strcmp(group,"rsp_p1") || flags!=1)return -1;
    *out=&device;return 0;
}
int wm_nvs_get_binary(wm_nvs_handle_t h,const char *key,void *out,size_t *n) {
    unsigned slot=(unsigned)(strcmp(key,"cfg_b")==0);(void)h;
    if(!lengths[slot] || *n<lengths[slot])return -1;
    memcpy(out,records[slot],lengths[slot]);*n=lengths[slot];return 0;
}
int wm_nvs_set_binary(wm_nvs_handle_t h,const char *key,const void *data,size_t n) {
    unsigned slot=(unsigned)(strcmp(key,"cfg_b")==0);size_t accepted=n;(void)h;
    if(n>80)return -1;
    writes++;
    if(fail_after>=0 && (size_t)fail_after<accepted)accepted=(size_t)fail_after;
    memcpy(records[slot],data,accepted);lengths[slot]=accepted;
    return accepted==n?0:-1;
}
__declspec(dllexport) void board_reset(void) {memset(records,0,sizeof(records));lengths[0]=lengths[1]=0;writes=0;fail_after=-1;mode=0;}
__declspec(dllexport) void board_failure(int after) {fail_after=after;}
__declspec(dllexport) void board_mode(int value) {mode=value;}
__declspec(dllexport) unsigned board_writes(void) {return writes;}
__declspec(dllexport) int board_load(uint8_t *out) {return product_board_load(out,64);}
__declspec(dllexport) int board_save(const uint8_t *p) {return product_board_save(p,64,0);}
__declspec(dllexport) int board_open(uint64_t *boot) {wm_device_t *uart;return product_board_open(&uart,boot);}
__declspec(dllexport) size_t board_record(unsigned slot,uint8_t *out) {if(slot>1)return 0;memcpy(out,records[slot],lengths[slot]);return lengths[slot];}
__declspec(dllexport) void board_inject(unsigned slot,const uint8_t *data,size_t n) {if(slot>1 || n>80)return;memcpy(records[slot],data,n);lengths[slot]=n;}
