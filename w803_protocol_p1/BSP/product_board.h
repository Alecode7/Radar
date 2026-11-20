#ifndef PRODUCT_BOARD_H
#define PRODUCT_BOARD_H
#include <stddef.h>
#include <stdint.h>
#include "wm_drv_uart.h"
int product_board_open(wm_device_t **device,uint64_t *boot);
int product_board_load(uint8_t *,size_t);
int product_board_save(const uint8_t *,size_t,void *);
#endif
