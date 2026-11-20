#include "board_uart.h"
#include "board_config.h"
#include "wm_dt.h"
#include "wm_error.h"

int board_uart_open(wm_device_t **radar,wm_device_t **console) {
    wm_drv_uart_config_t cfg;
    /* UART0 was already initialized by wm_system for logs (512-byte RX).
     * Reinitializing it returns ALREADY_INITED; keep its existing driver. */
    *console=wm_dt_get_device_by_name(LINK_CONSOLE_UART);
    if(*console==NULL || wm_drv_uart_get_config(*console,&cfg)!=WM_ERR_SUCCESS || cfg.baudrate!=LINK_BAUD) return -1;
    *radar=wm_drv_uart_init(LINK_RADAR_UART,LINK_RADAR_RX_BYTES,LINK_RADAR_TX_BYTES);
    if(*radar==NULL || wm_drv_uart_get_config(*radar,&cfg)!=WM_ERR_SUCCESS || cfg.baudrate!=LINK_BAUD) return -2;
    return 0;
}
