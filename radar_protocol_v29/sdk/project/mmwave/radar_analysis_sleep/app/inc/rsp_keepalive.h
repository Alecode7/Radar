#ifndef RSP_KEEPALIVE_H
#define RSP_KEEPALIVE_H
#include "rsp_result.h"
#define RSP_KEEPALIVE_ID 0x8102u
#define RSP_KEEPALIVE_FRAME 32u
/* Status flags: snapshot exists, stale/invalid snapshot, algorithm clock fault.
 * Heartbeat proves only task/protocol liveness, never measurement validity. */
#define RSP_KEEP_HAVE 1u
#define RSP_KEEP_STALE 2u
#define RSP_KEEP_CLOCK_FAULT 4u
RSP_RESULT_API size_t rsp_keepalive_encode(uint64_t,uint32_t,uint32_t,uint32_t,uint8_t *,size_t);
RSP_RESULT_API int rsp_keepalive_runtime_frame(uint8_t *,size_t);
RSP_RESULT_API void rsp_result_runtime_poll(uint32_t scheduler_ms);
#endif
