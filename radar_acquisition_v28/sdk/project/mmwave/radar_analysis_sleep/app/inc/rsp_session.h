#ifndef RSP_SESSION_H
#define RSP_SESSION_H
#include "rsp_query.h"
#include "rsp_keepalive.h"
#define RSP_SESSION_OPEN_ID 0x0110u
#define RSP_SESSION_RESULT_ID 0x0111u
#define RSP_SESSION_HEALTH_ID 0x0112u
#define RSP_HEALTH_BODY 20u
enum {RSP_INIT_UNKNOWN=0,RSP_INIT_BOOTING=1,RSP_INIT_READY=2,RSP_INIT_FAULT=3};
enum {RSP_STAGE_UNKNOWN=0,RSP_STAGE_OPEN=1,RSP_STAGE_LOG=2,RSP_STAGE_CONFIG=3,
      RSP_STAGE_SLEEP=4,RSP_STAGE_HEART=5,RSP_STAGE_COMPLETE=6};
#define RSP_SESSION_REQUIRED 9u
#define RSP_SESSION_OLD_SEQUENCE 10u
#define RSP_SESSION_BAD_ARGUMENT 11u
/* Volatile communication binding, NOT a boot_id or monitoring session.
 * Client supplies a globally non-reused nonzero token on each reconnect.
 * This protects accidental cross-session delivery, not hostile peers. */
struct RspSession {uint64_t token;uint32_t highest_sequence,open_sequence;};
RSP_RESULT_API void rsp_session_reset(RspSession *);
#endif
