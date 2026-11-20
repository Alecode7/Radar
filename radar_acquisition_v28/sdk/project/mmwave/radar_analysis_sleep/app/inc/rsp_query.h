#ifndef RSP_QUERY_H
#define RSP_QUERY_H
#include "rsp_result.h"

#define RSP_QUERY_CAPS_ID 0x0100u
#define RSP_QUERY_RESULT_ID 0x0101u
#define RSP_QUERY_MAX_PAYLOAD 16u
#define RSP_QUERY_MAX_REQUEST 32u
#define RSP_QUERY_MAX_RESPONSE 84u
#define RSP_QUERY_CAPS_BODY 64u
enum {RSP_QUERY_OK=0,RSP_QUERY_UNSUPPORTED=1,RSP_QUERY_BAD_LENGTH=2,
      RSP_QUERY_NOT_READY=5,RSP_QUERY_INTERNAL=8};

/* read_result returns a complete RSP1.1 RESULT frame, or <=0 if unavailable.
 * Providers/callbacks run synchronously in the caller's task, never an ISR.
 * Data queries are read-only; OPEN changes only volatile communication binding.
 * Read retries query NEW snapshots, not cached replies. No boot identity or
 * hardware control/configuration transaction is implied. */
typedef struct RspSession RspSession;
typedef struct {
    int (*read_result)(uint8_t *,size_t,void *);
    uint64_t (*uptime_ms)(void *);
    const char *firmware_tag;
    void *context;
    RspSession *session;
    int (*read_health)(uint8_t *,size_t,void *);
} RspQueryProvider;
typedef struct {
    uint8_t bytes[RSP_QUERY_MAX_REQUEST];
    uint16_t used;
    uint32_t last_ms,errors;
} RspQueryParser;
typedef void (*RspQueryRequest)(const uint8_t *,size_t,void *);

/* A complete malformed request is silently discarded (return 0). Output must
 * have MAX_RESPONSE bytes; on insufficient capacity nothing is written. */
RSP_RESULT_API size_t rsp_query_handle(const RspQueryProvider *,const uint8_t *,size_t,uint8_t *,size_t);
RSP_RESULT_API void rsp_query_parser_init(RspQueryParser *);
RSP_RESULT_API void rsp_query_parser_feed(RspQueryParser *,const uint8_t *,size_t,uint32_t,RspQueryRequest,void *);
RSP_RESULT_API void rsp_query_parser_poll(RspQueryParser *,uint32_t,RspQueryRequest,void *);
RSP_RESULT_API int rsp_query_runtime_hex(const char *hex);
RSP_RESULT_API int rsp_query_runtime_pipe(const char *hex);
RSP_RESULT_API void rsp_query_runtime_init_state(uint8_t initialization,uint8_t stage);
#endif
