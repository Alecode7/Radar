#ifndef LINK_STREAM_H
#define LINK_STREAM_H
#include <stddef.h>
#include <stdint.h>
#if defined(_WIN32) && defined(LINK_EXPORTS)
#define LINK_API __declspec(dllexport)
#else
#define LINK_API
#endif
#define LINK_LINE_CAP 2048u
enum LinkKind { LINK_OTHER=0,LINK_VERSION=1,LINK_RAW=2,LINK_INPUT=3,LINK_SLEEP=4 };
typedef struct {
    uint32_t bytes,lines,overlong,binary_bytes;
    uint32_t kind_count[5],last_frame[2],frame_gaps[2],duplicates[2],backwards[2],bad_frames[2];
    uint32_t have_frame[2],last_source_ms,have_source_ms;
} LinkStats;
typedef struct { LinkStats stats; uint32_t used,discard; char line[LINK_LINE_CAP]; } LinkStream;
typedef void (*link_line_fn)(void *,const char *,uint32_t,uint32_t);
LINK_API size_t link_stream_size(void);
LINK_API void link_stream_init(LinkStream *s);
LINK_API void link_stream_feed(LinkStream *s,const uint8_t *data,size_t size,link_line_fn fn,void *context);
LINK_API void link_stream_stats(const LinkStream *s,LinkStats *stats);
LINK_API uint32_t link_line_hash(const char *text,uint32_t size);
#endif
