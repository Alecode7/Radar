#ifndef MCU_RSP_H
#define MCU_RSP_H
#include <stddef.h>
#include <stdint.h>
#ifdef _WIN32
#define MCU_API __declspec(dllexport)
#else
#define MCU_API
#endif
#define MCU_RSP_MAX 256u
enum {MCU_RSP_IDENTITY=0,MCU_RSP_CAPS=1,MCU_RSP_OPEN=2,MCU_RSP_READY=3,MCU_RSP_BACKOFF=4,MCU_RSP_BLOCKED=5};
typedef uint64_t (*McuRspToken)(void *);
typedef struct {
    uint8_t rx[MCU_RSP_MAX],tx[MCU_RSP_MAX],reply[MCU_RSP_MAX],result[72];
    uint16_t rx_used,tx_size,reply_size,pending_id,last_reply_id;
    uint32_t sequence,pending_sequence,first_sent,last_sent,rx_ms,next_poll,last_result_ms;
    uint32_t errors,reconnects,reply_sequence;
    uint64_t boot_id,token,previous_token;
    uint8_t phase,pending,attempts,result_valid,reply_ready;
    uint16_t last_error;
    McuRspToken fresh_token;void *context;
} McuRsp;
/* Single task owns feed/poll/begin. Returned TX bytes must be fully accepted
 * by the board UART or disconnect() called. init/poll never auto-start radar.
 * Token callback must supply a fresh nonzero 64-bit value per reconnect. */
MCU_API size_t mcu_rsp_size(void);
MCU_API void mcu_rsp_init(McuRsp *,McuRspToken,void *);
MCU_API void mcu_rsp_disconnect(McuRsp *,uint32_t);
MCU_API size_t mcu_rsp_poll(McuRsp *,uint32_t,uint8_t *,size_t);
MCU_API void mcu_rsp_feed(McuRsp *,const uint8_t *,size_t,uint32_t);
/* extra excludes token. Ready and no outstanding transaction required.
 * Write/control retry retains exactly the same frame; after timeout the
 * application must query actual state, never blindly submit a new write. */
MCU_API size_t mcu_rsp_begin(McuRsp *,uint16_t,const uint8_t *,size_t,uint32_t,uint8_t *,size_t);
/* Returns 72-byte boot+monitor+faults+result copy. Ages are advanced locally.
 * Independent keepalive/event packets NEVER renew last_result_ms. */
MCU_API int mcu_rsp_result(const McuRsp *,uint32_t,uint8_t *,size_t);
MCU_API int mcu_rsp_reply(McuRsp *,uint16_t *,uint16_t *,uint8_t *,size_t);
#endif
