#ifndef SNAPSHOT_HEART_H
#define SNAPSHOT_HEART_H
#include "rx_state.h"
#include "heart_rate_core.h"
#if defined(_WIN32) && defined(SH_EXPORTS)
#define SH_API __declspec(dllexport)
#else
#define SH_API
#endif
#define SH_CYCLE_MS 5000u
#define SH_BUDGET_MS 120000u
enum { SH_CANCEL_EPOCH=1,SH_CANCEL_BUDGET=2 };
typedef struct { RadarIq sample;uint32_t epoch,received_ms; } ShTicket;
typedef struct {
 ShTicket ticket;uint32_t count,mode,skipped_before;
 RadarIq samples[RXL_CAPACITY];
} ShSnapshot;
typedef struct {
 uint32_t pushed,retired,lost,reset;
} ShPrepareInfo;
typedef struct {
 HrResult raw;
 uint32_t epoch,generation,frame,n,source_ms,received_ms,completed_ms;
 uint32_t cost_ms,lag_ms,age_ms,on_time,current,sequence,mode,skipped;
} ShReport;
typedef struct {
 uint32_t epoch,pending,requests,taken,skipped,received,pushed,retired,lost,math_resets;
 uint32_t attempts,completed,canceled,budget,over_cycle,last_cost,last_lag,produced;
 uint32_t rx_state,source_ms,source_n,source_gen,mode,snapshot_failures,pending_canceled;
} ShStatus;
typedef struct {
 RxlState intake;ShTicket ticket;ShStatus stats;ShReport published;
 uint32_t have_pending,have_published,last_requested_ms,mode;
} ShTransport;
typedef struct {
 HrCore core;HrWorkspace workspace;uint32_t epoch,last_n,have_n,mode;
} ShEngine;
SH_API size_t sh_transport_size(void);
SH_API size_t sh_engine_size(void);
SH_API size_t sh_snapshot_size(void);
SH_API size_t sh_report_size(void);
SH_API size_t sh_status_size(void);
SH_API void sh_init(ShTransport *s);
SH_API void sh_engine_init(ShEngine *e);
SH_API void sh_event(ShTransport *s,unsigned event);
SH_API int sh_set_mode(ShTransport *s,unsigned rotation);
/* Caller serializes transport only. No kernel mathematics under this lock. */
SH_API int sh_line(ShTransport *s,const char *line,size_t size,unsigned kind,uint32_t received_ms);
SH_API int sh_enqueue(ShTransport *s,const RadarIq *sample,uint32_t received_ms);
SH_API int sh_take(ShTransport *s,ShSnapshot *snapshot);
SH_API int sh_epoch_current(const ShTransport *s,uint32_t epoch);
/* Prepare/compute have one owner. Snapshot is stable during the whole call. */
SH_API int sh_prepare(ShEngine *e,const ShSnapshot *snapshot,ShPrepareInfo *info);
SH_API void sh_account_prepare(ShTransport *s,const ShPrepareInfo *info);
SH_API int sh_compute(ShEngine *e,const ShSnapshot *snapshot,HrResult *result,hr_yield_fn yield,void *context);
SH_API int sh_finish(ShTransport *s,const ShSnapshot *snapshot,const HrResult *result,int status,uint32_t start,uint32_t end,unsigned cancel);
SH_API int sh_report(const ShTransport *s,uint32_t now,ShReport *out);
SH_API void sh_status(const ShTransport *s,uint32_t now,ShStatus *out);
#endif
