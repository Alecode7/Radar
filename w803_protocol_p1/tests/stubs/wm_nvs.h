#ifndef MOCK_NVS_H
#define MOCK_NVS_H
#include <stddef.h>
#define WM_NVS_DEF_PARTITION "nvs"
#define WM_NVS_OP_READ_WRITE 1
typedef void *wm_nvs_handle_t;
int wm_nvs_open(const char *,const char *,int,wm_nvs_handle_t *);
int wm_nvs_get_binary(wm_nvs_handle_t,const char *,void *,size_t *);
int wm_nvs_set_binary(wm_nvs_handle_t,const char *,const void *,size_t);
#endif
