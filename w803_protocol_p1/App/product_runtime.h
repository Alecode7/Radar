#ifndef PRODUCT_RUNTIME_H
#define PRODUCT_RUNTIME_H
#include "board_uart.h"
#include "snapshot_heart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
int product_runtime_init(wm_device_t *,SemaphoreHandle_t,ShTransport *);
void product_runtime_line(const char *,int);
void product_runtime_heart(const ShReport *);
int product_runtime_control(unsigned);
int product_runtime_busy(void);
#endif
