#ifndef SLEEP_HEART_RATE_CORE_H
#define SLEEP_HEART_RATE_CORE_H
#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32) && defined(HR_TEST_EXPORTS)
#define HR_API __declspec(dllexport)
#else
#define HR_API
#endif

/* Fixed 5Hz / one range bin / three antennas, matching the current SAFEHR5
 * producer. This is not the optional 20Hz simultaneous-multi-bin profile. */
/* A recorded 75s device-time window contains385 inputs at ~195.3ms cadence.
 * Keep the original75s analysis window and allow bounded timing headroom;
 * exhausting384 used to erase accepted/candidate/breath history mid-session. */
#define HR_SAMPLE_CAP 416u
#define HR_FREQ_COUNT 145u
#define HR_SEG_CAP (HR_SAMPLE_CAP / 12u)
#define HR_VALID 1u
#define HR_IN_BED 2u
#define HR_ACTIVE 4u
#ifndef HR_FREQ_RECURSION
#define HR_FREQ_RECURSION 1
#endif
#ifndef HR_FREQ_REANCHOR
#define HR_FREQ_REANCHOR 16u
#endif
#ifndef HR_DEFAULT_FREQ_ROTATION
#define HR_DEFAULT_FREQ_ROTATION HR_FREQ_RECURSION
#endif

enum HrReason {
    HR_WAIT_BED=0,HR_WAIT_BREATH,HR_MOTION,HR_QUIET_SHORT,HR_BIN_UNSTABLE,
    HR_LOCKING,HR_ANTENNAS,HR_WEAK,HR_HIGH_HARMONIC,HR_CONFIRMING,
    HR_CONFIRMING_HARMONIC,HR_HOLD_HARMONIC,HR_WAIT_HARMONIC,
    HR_ENGINEERING,HR_OVERLAP,HR_CAPACITY,HR_RESET,HR_STALE,HR_RESOURCE,HR_INPUT,HR_BUDGET,
    HR_WAIT_UPDATE
};

typedef struct {
    uint32_t ms,frame;
    int16_t iq[6];
    uint16_t bin;
    uint8_t flags,reserved;
} HrSample;
typedef struct { double bpm; uint32_t ms; uint8_t breath; } HrCandidate;
typedef struct { uint32_t ms; uint8_t rate; } HrBreath;
typedef struct { uint16_t first,last,count,bin; } HrSegment;

/* Caller-owned scratch, held in the firmware's static engine, off task stack.
 * One sin/cos pair is shared by all three antenna accumulators. */
typedef struct { uint32_t segments,samples,trig_calls,frequencies; } HrWorkStats;
typedef struct {
    double filtered[2][HR_SAMPLE_CAP];
    double aggregate[2][HR_FREQ_COUNT];
    double power[2][HR_FREQ_COUNT];
    /* Per-sample frequency rotation; all vectors stay off the task stack. */
    double rotation[4][HR_SAMPLE_CAP];
    HrWorkStats stats;
    uint32_t frequency_rotation;
} HrWorkspace;
typedef struct {
    double bin_share,duration_s,cluster_score,candidate_bpm;
    uint32_t accepted_ms;
    int16_t bpm,target_bin,target_bins[2],harmonic_bpm;
    uint16_t reason;
    uint8_t support,quality,target_count,breath,harmonic_conflict,harmonic_order,fresh,reserved;
} HrResult;

typedef struct {
    HrSample samples[HR_SAMPLE_CAP];
    uint16_t head,count;
    uint32_t last_frame,last_ms,last_estimate_ms,last_accepted_ms,out_since_ms;
    uint8_t have_frame,have_accepted,have_out_since,out_reset,have_smoothed,consistent;
    double smoothed;
    HrCandidate candidates[20];
    HrBreath breaths[24];
    uint8_t candidate_count,breath_count;
    uint16_t quiet[HR_SAMPLE_CAP];
    HrSegment segments[HR_SEG_CAP];
    double values[HR_SAMPLE_CAP],times[HR_SAMPLE_CAP];
    double aggregate[HR_FREQ_COUNT];
    union {
        double prefix[HR_SAMPLE_CAP+1u];
        struct { uint16_t ids[HR_SAMPLE_CAP],counts[HR_SAMPLE_CAP]; } bins;
        struct { double real[HR_FREQ_COUNT],imag[HR_FREQ_COUNT]; } spectral;
        /* Single-precision work aliases the existing workspace; the double
         * power output starts AFTER these two float arrays. No extra SRAM. */
        struct { float real[HR_FREQ_COUNT],imag[HR_FREQ_COUNT]; } spectral_f32;
    } scratch;
    HrWorkspace *workspace;
} HrCore;

/* A checkpoint may cancel an obsolete or over-budget estimate. The core
 * returns 0 without accepting a new BPM when the callback returns nonzero. */
enum HrComputePhase { HR_PHASE_IDLE=0,HR_PHASE_CHECK=1,HR_PHASE_SPECTRUM=2,HR_PHASE_CONSENSUS=3,
    HR_PHASE_PREPROCESS=4,HR_PHASE_WINDOW=5,HR_PHASE_DFT=6,HR_PHASE_PEAKS=7,HR_PHASE_COUNT=8 };
typedef int (*hr_yield_fn)(void *context,unsigned phase);
HR_API size_t hr_core_size(void);
HR_API size_t hr_result_size(void);
HR_API size_t hr_workspace_size(void);
HR_API void hr_core_attach(HrCore *core,HrWorkspace *workspace);
HR_API void hr_core_work_stats(const HrCore *core,HrWorkStats *out);
/* Runtime A/B selection. Caller resets decision state when switching modes. */
HR_API int hr_core_set_spectrum_mode(HrCore *core,unsigned rotation);
HR_API unsigned hr_core_spectrum_mode(const HrCore *core);
HR_API void hr_core_init(HrCore *core);
/* Returns 0 on success. Negative means invalid input/capacity; no stale result
 * may be treated as current. uint32 ms wraps normally, frame wrap resets. */
HR_API int hr_core_push(HrCore *core,uint32_t ms,uint32_t frame,uint16_t bin,
    uint8_t flags,uint8_t fs,const int16_t iq[6]);
/* maybe: 0=no update due, 1=an estimate attempt (possibly with no numeric BPM). */
HR_API int hr_core_maybe(HrCore *core,uint8_t breath,uint32_t now_ms,HrResult *result);
HR_API int hr_core_due(const HrCore *core,uint32_t now_ms);
HR_API int hr_core_estimate(HrCore *core,uint8_t breath,uint32_t now_ms,HrResult *result,
    hr_yield_fn yield,void *context);
#endif
