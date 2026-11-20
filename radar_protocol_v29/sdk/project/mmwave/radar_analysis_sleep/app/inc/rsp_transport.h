#ifndef RSP_TRANSPORT_H
#define RSP_TRANSPORT_H
#include "rsp_query.h"
/* One task owns init/feed/poll. Never call from an ISR or writer callback.
 * The writer consumes/copies 0..length bytes synchronously, never retains ptr.
 * A negative return or return >length faults the binding until re-init.
 * BINARY_DEDICATED requires a BSP-enforced exclusive TX/RX/log ownership.
 * HEX_DIAGNOSTIC is a simulation/diagnostic binding, not a product UART. */
enum {RSP_PORT_BINARY_DEDICATED=1,RSP_PORT_HEX_DIAGNOSTIC=2};
typedef int (*RspTransportWrite)(const uint8_t *,size_t,void *);
typedef struct {
    RspQueryParser parser;
    RspQueryProvider provider;
    RspTransportWrite write;
    void *context;
    uint8_t tx[RSP_QUERY_MAX_RESPONSE];
    uint16_t tx_size,tx_offset;
    uint32_t dropped,write_errors;
    uint8_t mode,faulted;
} RspTransport;
RSP_RESULT_API int rsp_transport_init(RspTransport *,const RspQueryProvider *,RspTransportWrite,void *,unsigned);
RSP_RESULT_API void rsp_transport_feed(RspTransport *,const uint8_t *,size_t,uint32_t);
RSP_RESULT_API void rsp_transport_poll(RspTransport *,uint32_t);
#endif
