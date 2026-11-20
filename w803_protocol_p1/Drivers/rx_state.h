#ifndef RXL_STATE_H
#define RXL_STATE_H
#include "radar_iq.h"
#include <stdint.h>
#include <stddef.h>
#if defined(_WIN32) && defined(RXL_EXPORTS)
#define RXL_API __declspec(dllexport)
#else
#define RXL_API
#endif
#define RXL_CAPACITY 416u
/* Transport silence is diagnostic only. It does not erase history or change
 * the original heart core's confirmation/expiry rules. No estimator runs here. */
#define RXL_SILENCE_MS 3000u
enum { RXL_VERSION=1,RXL_START=2,RXL_STOP=3,RXL_IOFAULT=4 };
typedef struct {
 uint32_t epoch,count,accepted,duplicates,bad,frame_gaps,n_gaps,resets;
 uint32_t versions,bed0,paused,state,source_ms,frame,n,generation;
 uint32_t received_ms,age_ms,have_source,bed;
} RxlStatus;
typedef struct {
 RadarIq history[RXL_CAPACITY];RxlStatus status;
 uint32_t head,blocked_generation,blocked;
} RxlState;
/* Caller serializes feed/control/status/snapshot. No hardware or heap here. */
RXL_API size_t rxl_size(void);
RXL_API size_t rxl_status_size(void);
RXL_API void rxl_init(RxlState *s);
RXL_API void rxl_event(RxlState *s,unsigned event);
RXL_API int rxl_push(RxlState *s,const RadarIq *sample,uint32_t received_ms);
RXL_API int rxl_line(RxlState *s,const char *line,size_t size,unsigned kind,uint32_t received_ms);
RXL_API void rxl_status(const RxlState *s,uint32_t now_ms,RxlStatus *out);
RXL_API size_t rxl_snapshot(const RxlState *s,RadarIq *out,size_t capacity,uint32_t *epoch);
#endif
