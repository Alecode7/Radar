#ifndef MOCK_WM_UART_H
#define MOCK_WM_UART_H
#include <stdint.h>
typedef struct {unsigned dummy;} wm_device_t;
typedef struct {uint32_t baudrate;} wm_drv_uart_config_t;
wm_device_t *wm_drv_uart_init(const char *,uint32_t,uint32_t);
int wm_drv_uart_get_config(wm_device_t *,wm_drv_uart_config_t *);
int wm_drv_uart_read(wm_device_t *,void *,uint32_t,uint32_t);
int wm_drv_uart_write(wm_device_t *,const uint8_t *,uint32_t);
#endif
