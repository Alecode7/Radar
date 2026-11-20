#ifndef RSP_RESULT_H
#define RSP_RESULT_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#if defined(_WIN32) && defined(RSP_RESULT_EXPORTS)
#define RSP_RESULT_API __declspec(dllexport)
#else
#define RSP_RESULT_API
#endif
#define RSP_RESULT_PAYLOAD 56u
#define RSP_RESULT_FRAME 72u
#define RSP_RESULT_ID 0x8101u
#define RSP_RESULT_HCON 1u
#define RSP_RESULT_MOTION 2u
#define RSP_RESULT_STALE 4u
#define RSP_RESULT_BR_TIME_UNKNOWN 8u
#define RSP_RESULT_HEX_DIAGNOSTIC 16u
#define RSP_RESULT_CLOCK_FAULT 32u
typedef struct {
    uint64_t uptime_ms;
    uint32_t generation;
    uint8_t presence,bed,motion,sleep_state;
    uint16_t breath_x10,heart_x10;
    uint8_t breath_state,heart_state,heart_kind,heart_quality;
    uint32_t breath_age_ms,heart_age_ms,breath_sample_seq,heart_sample_seq;
    uint32_t in_bed_s,out_bed_s,flags,faults;
} RspResult;
typedef void (*RspResultReceive)(const RspResult *,uint32_t,void *);
typedef struct {uint8_t bytes[RSP_RESULT_FRAME];uint16_t used;uint32_t last_ms,errors;} RspResultParser;
RSP_RESULT_API uint16_t rsp_result_crc(const uint8_t *data,size_t length);
RSP_RESULT_API size_t rsp_result_encode(const RspResult *,uint32_t,uint8_t *,size_t);
RSP_RESULT_API int rsp_result_decode(const uint8_t *,size_t,RspResult *,uint32_t *);
RSP_RESULT_API void rsp_result_parser_init(RspResultParser *);
RSP_RESULT_API void rsp_result_parser_feed(RspResultParser *,const uint8_t *,size_t,uint32_t,RspResultReceive,void *);
RSP_RESULT_API void rsp_result_parser_poll(RspResultParser *,uint32_t,RspResultReceive,void *);
void rsp_result_runtime_update(void);
int rsp_result_runtime_frame(uint8_t *,size_t);
int rsp_result_runtime_dump(void);
RSP_RESULT_API void rsp_result_runtime_stream(bool enabled);
#endif
