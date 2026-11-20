#ifndef RADAR_IQ_H
#define RADAR_IQ_H
#include <stddef.h>
#include <stdint.h>
#if defined(_WIN32) && defined(RADAR_IQ_EXPORTS)
#define RADAR_IQ_API __declspec(dllexport)
#else
#define RADAR_IQ_API
#endif

/* Host/target struct, never a wire packet. Device sample time is preserved.
 * ENQ describes the old radar-side worker only; do not reinterpret it as
 * successful reception/enqueue on W803, or as a physical signal-quality flag. */
typedef struct {
    uint32_t frame,source_ms,generation,sequence;
    int16_t iq[6];
    uint16_t bin;
    uint8_t flags,breath,source_version,source_enqueued,sample_rate,reserved;
} RadarIq;

enum RadarIqStatus { RADAR_IQ_NOT_INPUT=0,RADAR_IQ_OK=1,RADAR_IQ_MALFORMED=-1,RADAR_IQ_UNSUPPORTED=-2 };
#define RADAR_IQ_MAX_LINE 2048u
RADAR_IQ_API size_t radar_iq_size(void);
/* Bounded complete original radar text line, after L1 framing, before logging.
 * Decode V25/26/27 HRIN only. HRRAW has no source timestamp: do not fabricate
 * one from PC/W803 arrival time. Output is unchanged on any non-OK return.
 * No heap allocation, UART access, scheduling or heart-rate state changes. */
RADAR_IQ_API int radar_iq_parse(const char *line,size_t length,RadarIq *output);
#endif
