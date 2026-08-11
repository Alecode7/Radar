#include "sleep_detect.h"
#include <stddef.h>

#ifndef ABS
#define ABS(x) ((x) < 0 ? -(x) : (x))
#endif

#define BED_ENTER_MOTION_POINTS_TH 2
#define BED_KEEP_MOTION_POINTS_TH 8
#define BREATH_SAMPLE_PERIOD_MS 200U
#define BREATH_RATE_MIN_BPM 6U
#define BREATH_RATE_MAX_BPM 24U
#define BREATH_RATE_STABLE_MIN_BPM 12U
#define BREATH_RATE_MIN_WINDOW_SAMPLES 50U
#define BREATH_RATE_VALID_REPEAT_TH 2U
#define BREATH_RATE_SWITCH_REPEAT_TH 6U
#define BREATH_RATE_RISE_REPEAT_TH 10U
#define BREATH_RATE_OUTPUT_WARMUP_TH 14U
#define BREATH_RATE_CANDIDATE_TOLERANCE_BPM 4U
#define BREATH_RATE_STABLE_TOLERANCE_BPM 3U
#define BREATH_PHASE_DELTA_MAX_MRAD 2200
#define SLEEP_QUIET_CONFIRM_FRAMES 50U
#define SLEEP_LIKELY_SLEEP_FRAMES 1500U
#define SLEEP_BREATH_STABLE_FRAMES 300U
#define SLEEP_BREATH_LOSS_HOLD_FRAMES 100U
#define MOTION_INTENSITY_STRONG_POINTS_TH 8U
#define MOTION_INTENSITY_LARGE_POINTS_TH 18U
#define MOTION_INTENSITY_LARGE_ACTIVE_TH 8U
#define TURN_QUIET_BEFORE_FRAMES 20U
#define TURN_MIN_MOTION_FRAMES 5U
#define TURN_RECOVERY_QUIET_FRAMES 10U
#define TURN_CONFIRM_TIMEOUT_FRAMES 300U
#define TURN_EVENT_HOLD_FRAMES 15U
#define TURN_COOLDOWN_FRAMES 150U
#define SLEEP_TURN_RETURN_TIMEOUT_FRAMES 1800U
#define BREATH_STATE_INVALID 0U
#define BREATH_STATE_LOW 1U
#define BREATH_STATE_NORMAL 2U
#define BREATH_STATE_HIGH 3U
#define BREATH_STATE_LOW_MAX_BPM 11U
#define BREATH_STATE_NORMAL_MAX_BPM 20U

typedef struct {
    SleepDetectConfig cfg;
    SleepDetectResult result;
    uint32_t frame_count;
    uint16_t person_present_streak;
    uint16_t person_absent_streak;
    uint16_t bed_present_streak;
    uint16_t bed_absent_streak;
    uint16_t breath_present_streak;
    uint16_t breath_absent_streak;
    uint16_t sleep_quiet_streak;
    uint16_t sleep_active_streak;
    uint16_t sleep_breath_streak;
    uint16_t sleep_breath_loss_streak;
    uint16_t turn_count;
    uint16_t sleep_turn_count;
    uint16_t turn_cooldown_frames;
    uint16_t turn_event_hold_frames;
    uint16_t turn_candidate_frames;
    uint16_t turn_candidate_motion_frames;
    uint16_t turn_candidate_quiet_frames;
    bool turn_candidate_pending;
    bool turn_candidate_started_in_sleep;
    bool sleep_turn_pending;
    uint16_t sleep_turn_pending_frames;
    uint16_t bed_enter_count;
    uint16_t bed_exit_count;
    uint32_t in_bed_frames;
    uint32_t out_bed_frames;
    uint32_t breath_valid_frames;
    uint32_t breath_rate_sum;
    uint32_t breath_rate_samples;
    uint32_t active_frames;
    uint32_t quiet_frames;
    uint32_t likely_sleep_frames;
    uint32_t current_quiet_frames;
    uint32_t longest_quiet_frames;
    uint32_t sleep_onset_frames;
    uint32_t current_likely_sleep_frames;
    uint32_t longest_likely_sleep_frames;
    uint16_t sleep_segment_count;
    uint16_t sleep_interruption_count;
    bool sleep_interruption_pending;
    uint16_t session_end_hold_frames;
    uint8_t breath_rate_min;
    uint8_t breath_rate_max;
    uint8_t last_in_bed;
    uint8_t last_sleep_state;
    uint16_t breath_signal_window[BREATH_WINDOW_SIZE];
    uint16_t breath_signal_index;
    uint16_t breath_signal_count;
    uint16_t breath_rate_candidate;
    uint16_t breath_rate_repeat_count;
    uint16_t breath_rate_stable;
    uint16_t breath_rate_output_count;
    int32_t breath_phase_window[BREATH_WINDOW_SIZE];
    uint16_t breath_phase_index;
    uint16_t breath_phase_count;
    int32_t breath_phase_unwrapped_mrad;
    int32_t breath_phase_last_mrad;
    bool breath_phase_has_last;
} SleepDetectState;

static SleepDetectState g_sleep_state;

static uint8_t sleep_detect_percent(uint32_t part, uint32_t total)
{
    uint32_t percent;

    if (total == 0) {
        return 0;
    }
    if (part >= total) {
        return 100;
    }

    percent = (uint32_t)(((uint64_t)part * 100U) / total);
    return percent > 100U ? 100U : (uint8_t)percent;
}

static uint8_t sleep_detect_breath_state(void)
{
    uint8_t rate = g_sleep_state.result.breath_rate;

    if (!g_sleep_state.result.breath_valid || rate == 0) {
        return BREATH_STATE_INVALID;
    }
    if (rate <= BREATH_STATE_LOW_MAX_BPM) {
        return BREATH_STATE_LOW;
    }
    if (rate <= BREATH_STATE_NORMAL_MAX_BPM) {
        return BREATH_STATE_NORMAL;
    }

    return BREATH_STATE_HIGH;
}

static SleepDetectConfig sleep_detect_default_config(void)
{
    SleepDetectConfig cfg;

    cfg.bed_x_min_cm = -50;
    cfg.bed_x_max_cm = 50;
    cfg.bed_y_min_cm = 50;
    cfg.bed_y_max_cm = 180;
    cfg.bed_z_min_cm = -70;
    cfg.bed_z_max_cm = 70;
    cfg.enter_confirm_frames = 8;
    cfg.exit_confirm_frames = 24;
    cfg.active_motion_points_th = 3;
    cfg.active_velocity_abs_th_cm_s = 12;
    cfg.micro_presence_points_th = 3;
    cfg.breath_signal_enter_th = 8;
    cfg.breath_signal_exit_th = 4;
    cfg.report_interval_frames = 10;

    return cfg;
}

static void sleep_detect_push_breath_signal(uint16_t value)
{
    g_sleep_state.breath_signal_window[g_sleep_state.breath_signal_index] = value;
    g_sleep_state.breath_signal_index = (g_sleep_state.breath_signal_index + 1) % BREATH_WINDOW_SIZE;
    if (g_sleep_state.breath_signal_count < BREATH_WINDOW_SIZE) {
        g_sleep_state.breath_signal_count++;
    }
}

static void sleep_detect_push_breath_phase(int32_t value)
{
    g_sleep_state.breath_phase_window[g_sleep_state.breath_phase_index] = value;
    g_sleep_state.breath_phase_index = (g_sleep_state.breath_phase_index + 1) % BREATH_WINDOW_SIZE;
    if (g_sleep_state.breath_phase_count < BREATH_WINDOW_SIZE) {
        g_sleep_state.breath_phase_count++;
    }
}

static void sleep_detect_reset_breath_phase_window(void)
{
    g_sleep_state.breath_phase_index = 0;
    g_sleep_state.breath_phase_count = 0;
    g_sleep_state.breath_phase_unwrapped_mrad = 0;
    g_sleep_state.breath_phase_last_mrad = 0;
    g_sleep_state.breath_phase_has_last = false;
    g_sleep_state.breath_rate_candidate = 0;
    g_sleep_state.breath_rate_repeat_count = 0;
    g_sleep_state.breath_rate_stable = 0;
    g_sleep_state.breath_rate_output_count = 0;
    g_sleep_state.result.breath_phase_delta_mrad = 0;
    g_sleep_state.result.breath_phase_quality = 0;
    g_sleep_state.result.breath_period_frames = 0;
    g_sleep_state.result.breath_rate = 0;
    g_sleep_state.result.breath_rate_candidate = 0;
    g_sleep_state.result.breath_rate_repeat_count = 0;
    g_sleep_state.result.breath_rate_stable = 0;
}

static int32_t sleep_detect_get_breath_phase_sample(uint16_t index)
{
    uint16_t buffer_index;

    if (g_sleep_state.breath_phase_count < BREATH_WINDOW_SIZE) {
        buffer_index = index;
    } else {
        buffer_index = (g_sleep_state.breath_phase_index + index) % BREATH_WINDOW_SIZE;
    }

    return g_sleep_state.breath_phase_window[buffer_index];
}

static uint16_t sleep_detect_avg_breath_signal(void)
{
    uint32_t sum = 0;

    if (g_sleep_state.breath_signal_count == 0) {
        return 0;
    }

    for (uint16_t i = 0; i < g_sleep_state.breath_signal_count; ++i) {
        sum += g_sleep_state.breath_signal_window[i];
    }

    return (uint16_t)(sum / g_sleep_state.breath_signal_count);
}

static uint16_t sleep_detect_estimate_breath_rate(uint16_t *period_frames_out)
{
    uint16_t min_lag = (uint16_t)((60U * 1000U) / (BREATH_RATE_MAX_BPM * BREATH_SAMPLE_PERIOD_MS));
    uint16_t max_lag = (uint16_t)((60U * 1000U) / (BREATH_RATE_MIN_BPM * BREATH_SAMPLE_PERIOD_MS));
    int32_t best_score = -1;
    uint16_t best_lag = 0;

    *period_frames_out = 0;

    if (g_sleep_state.breath_signal_count < BREATH_RATE_MIN_WINDOW_SAMPLES) {
        return 0;
    }

    if (max_lag >= g_sleep_state.breath_signal_count) {
        max_lag = g_sleep_state.breath_signal_count - 1;
    }

    for (uint16_t lag = min_lag; lag <= max_lag; ++lag) {
        int32_t score = 0;
        uint16_t samples = g_sleep_state.breath_signal_count - lag;
        for (uint16_t i = lag; i < g_sleep_state.breath_signal_count; ++i) {
            uint16_t cur = g_sleep_state.breath_signal_window[i];
            uint16_t prev = g_sleep_state.breath_signal_window[i - lag];
            score += (int32_t)cur * (int32_t)prev;
        }
        score = samples > 0 ? (score / samples) : 0;
        if (score > best_score ||
            (score == best_score && lag > best_lag)) {
            best_score = score;
            best_lag = lag;
        }
    }

    if (best_lag == 0) {
        return 0;
    }

    *period_frames_out = best_lag;
    return (uint16_t)((60U * 1000U) / (best_lag * BREATH_SAMPLE_PERIOD_MS));
}

static uint16_t sleep_detect_estimate_phase_breath_rate(uint16_t *period_frames_out)
{
    uint16_t min_lag = (uint16_t)((60U * 1000U) / (BREATH_RATE_MAX_BPM * BREATH_SAMPLE_PERIOD_MS));
    uint16_t max_lag = (uint16_t)((60U * 1000U) / (BREATH_RATE_MIN_BPM * BREATH_SAMPLE_PERIOD_MS));
    int64_t best_score = 0;
    int64_t mean = 0;
    uint16_t best_lag = 0;
    uint16_t count = g_sleep_state.breath_phase_count;

    *period_frames_out = 0;

    if (count < BREATH_RATE_MIN_WINDOW_SAMPLES) {
        return 0;
    }

    if (max_lag >= count) {
        max_lag = count - 1;
    }

    for (uint16_t i = 0; i < count; ++i) {
        mean += sleep_detect_get_breath_phase_sample(i);
    }
    mean /= count;

    for (uint16_t lag = min_lag; lag <= max_lag; ++lag) {
        int64_t score = 0;
        uint16_t samples = count - lag;
        for (uint16_t i = lag; i < count; ++i) {
            int64_t cur = sleep_detect_get_breath_phase_sample(i) - mean;
            int64_t prev = sleep_detect_get_breath_phase_sample(i - lag) - mean;
            score += cur * prev;
        }
        score = samples > 0 ? (score / samples) : 0;
        if (score > best_score ||
            (score == best_score && lag > best_lag)) {
            best_score = score;
            best_lag = lag;
        }
    }

    if (best_lag == 0) {
        return 0;
    }

    *period_frames_out = best_lag;
    return (uint16_t)((60U * 1000U) / (best_lag * BREATH_SAMPLE_PERIOD_MS));
}

static uint16_t sleep_detect_stabilize_breath_rate(uint16_t estimated_rate)
{
    uint16_t diff;
    uint16_t stable_diff;

    if (g_sleep_state.breath_rate_stable > 0 &&
        g_sleep_state.breath_rate_stable < BREATH_RATE_STABLE_MIN_BPM) {
        g_sleep_state.breath_rate_stable = 0;
    }

    if (g_sleep_state.breath_rate_candidate > 0 &&
        g_sleep_state.breath_rate_candidate < BREATH_RATE_STABLE_MIN_BPM) {
        g_sleep_state.breath_rate_candidate = 0;
        g_sleep_state.breath_rate_repeat_count = 0;
    }

    if (estimated_rate > 0 && estimated_rate < BREATH_RATE_STABLE_MIN_BPM) {
        estimated_rate = 0;
    }

    if (estimated_rate == 0) {
        g_sleep_state.result.breath_rate_candidate = g_sleep_state.breath_rate_candidate;
        g_sleep_state.result.breath_rate_repeat_count = g_sleep_state.breath_rate_repeat_count;
        g_sleep_state.result.breath_rate_stable = g_sleep_state.breath_rate_stable;
        return g_sleep_state.breath_rate_stable;
    }

    if (g_sleep_state.breath_rate_candidate == 0) {
        g_sleep_state.breath_rate_candidate = estimated_rate;
        g_sleep_state.breath_rate_repeat_count = 1;
    } else {
        diff = (g_sleep_state.breath_rate_candidate > estimated_rate) ?
               (g_sleep_state.breath_rate_candidate - estimated_rate) :
               (estimated_rate - g_sleep_state.breath_rate_candidate);
        if (diff <= BREATH_RATE_CANDIDATE_TOLERANCE_BPM) {
            g_sleep_state.breath_rate_candidate = (uint16_t)((g_sleep_state.breath_rate_candidate + estimated_rate) / 2U);
            if (g_sleep_state.breath_rate_repeat_count < 0xFFFF) {
                g_sleep_state.breath_rate_repeat_count++;
            }
        } else {
            g_sleep_state.breath_rate_candidate = estimated_rate;
            g_sleep_state.breath_rate_repeat_count = 1;
        }
    }

    if (g_sleep_state.breath_rate_repeat_count >= BREATH_RATE_VALID_REPEAT_TH) {
        if (g_sleep_state.breath_rate_stable == 0) {
            g_sleep_state.breath_rate_stable = g_sleep_state.breath_rate_candidate;
        } else {
            stable_diff = (g_sleep_state.breath_rate_stable > g_sleep_state.breath_rate_candidate) ?
                          (g_sleep_state.breath_rate_stable - g_sleep_state.breath_rate_candidate) :
                          (g_sleep_state.breath_rate_candidate - g_sleep_state.breath_rate_stable);
            if (stable_diff <= BREATH_RATE_STABLE_TOLERANCE_BPM) {
                if (g_sleep_state.breath_rate_candidate > g_sleep_state.breath_rate_stable) {
                    if (g_sleep_state.breath_rate_repeat_count >= BREATH_RATE_RISE_REPEAT_TH) {
                        g_sleep_state.breath_rate_stable++;
                    }
                } else if (g_sleep_state.breath_rate_candidate < g_sleep_state.breath_rate_stable) {
                    g_sleep_state.breath_rate_stable =
                        (uint16_t)((g_sleep_state.breath_rate_stable +
                                    g_sleep_state.breath_rate_candidate + 1U) / 2U);
                }
            } else if (g_sleep_state.breath_rate_repeat_count >= BREATH_RATE_SWITCH_REPEAT_TH) {
                g_sleep_state.breath_rate_stable = g_sleep_state.breath_rate_candidate;
            }
        }
    }

    g_sleep_state.result.breath_rate_candidate = g_sleep_state.breath_rate_candidate;
    g_sleep_state.result.breath_rate_repeat_count = g_sleep_state.breath_rate_repeat_count;
    g_sleep_state.result.breath_rate_stable = g_sleep_state.breath_rate_stable;

    return g_sleep_state.breath_rate_stable;
}

static uint16_t sleep_detect_public_breath_rate(uint16_t stable_rate, bool has_valid_estimate)
{
    if (stable_rate == 0) {
        g_sleep_state.breath_rate_output_count = 0;
        return 0;
    }

    if (has_valid_estimate && g_sleep_state.breath_rate_output_count < 0xFFFF) {
        g_sleep_state.breath_rate_output_count++;
    }

    if (g_sleep_state.breath_rate_output_count < BREATH_RATE_OUTPUT_WARMUP_TH) {
        return 0;
    }

    return stable_rate;
}

static uint8_t sleep_detect_motion_intensity(uint16_t motion_in_bed, uint16_t active_motion_points)
{
    if (active_motion_points >= MOTION_INTENSITY_LARGE_ACTIVE_TH ||
        motion_in_bed >= MOTION_INTENSITY_LARGE_POINTS_TH) {
        return 3;
    }

    if (active_motion_points >= g_sleep_state.cfg.active_motion_points_th ||
        motion_in_bed >= MOTION_INTENSITY_STRONG_POINTS_TH) {
        return 2;
    }

    if (motion_in_bed > 0) {
        return 1;
    }

    return 0;
}

static void sleep_detect_update_turn_event(uint16_t motion_in_bed, uint16_t active_motion_points)
{
    bool in_bed = g_sleep_state.result.has_person && g_sleep_state.result.in_bed;
    bool strong_motion = active_motion_points >= g_sleep_state.cfg.active_motion_points_th ||
                         motion_in_bed >= MOTION_INTENSITY_STRONG_POINTS_TH;
    bool breath_recovered = g_sleep_state.result.breath_rate > 0 ||
                            (g_sleep_state.result.breath_valid &&
                             g_sleep_state.result.breath_rate_stable >= BREATH_RATE_STABLE_MIN_BPM);
    bool turn_candidate = in_bed &&
                          strong_motion &&
                          g_sleep_state.sleep_quiet_streak >= TURN_QUIET_BEFORE_FRAMES;

    if (g_sleep_state.turn_cooldown_frames > 0) {
        g_sleep_state.turn_cooldown_frames--;
    }
    if (g_sleep_state.turn_event_hold_frames > 0) {
        g_sleep_state.turn_event_hold_frames--;
    }

    if (g_sleep_state.turn_candidate_pending) {
        if (!in_bed) {
            g_sleep_state.turn_candidate_pending = false;
            g_sleep_state.turn_candidate_started_in_sleep = false;
            g_sleep_state.turn_candidate_frames = 0;
            g_sleep_state.turn_candidate_motion_frames = 0;
            g_sleep_state.turn_candidate_quiet_frames = 0;
        } else {
            if (g_sleep_state.turn_candidate_frames < 0xFFFF) {
                g_sleep_state.turn_candidate_frames++;
            }
            if (strong_motion && g_sleep_state.turn_candidate_motion_frames < 0xFFFF) {
                g_sleep_state.turn_candidate_motion_frames++;
            }
            if (strong_motion || g_sleep_state.result.is_active) {
                g_sleep_state.turn_candidate_quiet_frames = 0;
            } else if (g_sleep_state.turn_candidate_quiet_frames < 0xFFFF) {
                g_sleep_state.turn_candidate_quiet_frames++;
            }

            if (g_sleep_state.turn_candidate_motion_frames >= TURN_MIN_MOTION_FRAMES &&
                g_sleep_state.turn_candidate_quiet_frames >= TURN_RECOVERY_QUIET_FRAMES &&
                breath_recovered) {
                if (g_sleep_state.turn_count < 0xFFFF) {
                    g_sleep_state.turn_count++;
                }
                if (g_sleep_state.turn_candidate_started_in_sleep) {
                    g_sleep_state.sleep_turn_pending = true;
                    g_sleep_state.sleep_turn_pending_frames = 0;
                }
                g_sleep_state.turn_event_hold_frames = TURN_EVENT_HOLD_FRAMES;
                g_sleep_state.turn_cooldown_frames = TURN_COOLDOWN_FRAMES;
                g_sleep_state.turn_candidate_pending = false;
                g_sleep_state.turn_candidate_frames = 0;
                g_sleep_state.turn_candidate_motion_frames = 0;
                g_sleep_state.turn_candidate_quiet_frames = 0;
                g_sleep_state.turn_candidate_started_in_sleep = false;
            } else if (g_sleep_state.turn_candidate_frames >= TURN_CONFIRM_TIMEOUT_FRAMES) {
                g_sleep_state.turn_candidate_pending = false;
                g_sleep_state.turn_candidate_started_in_sleep = false;
                g_sleep_state.turn_candidate_frames = 0;
                g_sleep_state.turn_candidate_motion_frames = 0;
                g_sleep_state.turn_candidate_quiet_frames = 0;
            }
        }
    } else if (turn_candidate && g_sleep_state.turn_cooldown_frames == 0) {
        g_sleep_state.turn_candidate_pending = true;
        g_sleep_state.turn_candidate_frames = 0;
        g_sleep_state.turn_candidate_motion_frames = 1;
        g_sleep_state.turn_candidate_quiet_frames = 0;
        g_sleep_state.turn_candidate_started_in_sleep =
            g_sleep_state.result.sleep_state == SLEEP_STATE_LIKELY_SLEEP;
    }

    g_sleep_state.result.turn_event = g_sleep_state.turn_event_hold_frames > 0 ? 1 : 0;
    g_sleep_state.result.turn_count = g_sleep_state.turn_count;
}

static void sleep_detect_update_session_stats(void)
{
    bool in_bed = g_sleep_state.result.has_person && g_sleep_state.result.in_bed;

    if (in_bed && !g_sleep_state.last_in_bed) {
        if (g_sleep_state.bed_enter_count < 0xFFFF) {
            g_sleep_state.bed_enter_count++;
        }
        g_sleep_state.in_bed_frames = 0;
        g_sleep_state.breath_valid_frames = 0;
        g_sleep_state.breath_rate_sum = 0;
        g_sleep_state.breath_rate_samples = 0;
        g_sleep_state.active_frames = 0;
        g_sleep_state.quiet_frames = 0;
        g_sleep_state.likely_sleep_frames = 0;
        g_sleep_state.current_quiet_frames = 0;
        g_sleep_state.longest_quiet_frames = 0;
        g_sleep_state.sleep_onset_frames = 0;
        g_sleep_state.current_likely_sleep_frames = 0;
        g_sleep_state.longest_likely_sleep_frames = 0;
        g_sleep_state.sleep_segment_count = 0;
        g_sleep_state.sleep_interruption_count = 0;
        g_sleep_state.sleep_interruption_pending = false;
        g_sleep_state.session_end_hold_frames = 0;
        g_sleep_state.breath_rate_min = 0;
        g_sleep_state.breath_rate_max = 0;
        g_sleep_state.turn_count = 0;
        g_sleep_state.sleep_turn_count = 0;
        g_sleep_state.turn_cooldown_frames = 0;
        g_sleep_state.turn_event_hold_frames = 0;
        g_sleep_state.turn_candidate_frames = 0;
        g_sleep_state.turn_candidate_motion_frames = 0;
        g_sleep_state.turn_candidate_quiet_frames = 0;
        g_sleep_state.turn_candidate_pending = false;
        g_sleep_state.turn_candidate_started_in_sleep = false;
        g_sleep_state.sleep_turn_pending = false;
        g_sleep_state.sleep_turn_pending_frames = 0;
        g_sleep_state.result.turn_event = 0;
        g_sleep_state.last_sleep_state = g_sleep_state.result.sleep_state;
    } else if (!in_bed && g_sleep_state.last_in_bed) {
        if (g_sleep_state.bed_exit_count < 0xFFFF) {
            g_sleep_state.bed_exit_count++;
        }
        g_sleep_state.out_bed_frames = 0;
        g_sleep_state.current_quiet_frames = 0;
        g_sleep_state.current_likely_sleep_frames = 0;
        g_sleep_state.sleep_interruption_pending = false;
        g_sleep_state.sleep_turn_pending = false;
        g_sleep_state.sleep_turn_pending_frames = 0;
        g_sleep_state.session_end_hold_frames =
            g_sleep_state.cfg.report_interval_frames < 0xFFFFU
                ? g_sleep_state.cfg.report_interval_frames + 1U
                : 0xFFFFU;
    }

    if (in_bed) {
        if (g_sleep_state.in_bed_frames < 0xFFFFFFFFUL) {
            g_sleep_state.in_bed_frames++;
        }
        g_sleep_state.out_bed_frames = 0;

        if (g_sleep_state.result.breath_valid || g_sleep_state.result.breath_rate > 0) {
            if (g_sleep_state.breath_valid_frames < 0xFFFFFFFFUL) {
                g_sleep_state.breath_valid_frames++;
            }
        }

        if (g_sleep_state.result.breath_rate > 0) {
            g_sleep_state.breath_rate_sum += g_sleep_state.result.breath_rate;
            if (g_sleep_state.breath_rate_samples < 0xFFFFFFFFUL) {
                g_sleep_state.breath_rate_samples++;
            }
            if (g_sleep_state.breath_rate_min == 0 ||
                g_sleep_state.result.breath_rate < g_sleep_state.breath_rate_min) {
                g_sleep_state.breath_rate_min = g_sleep_state.result.breath_rate;
            }
            if (g_sleep_state.result.breath_rate > g_sleep_state.breath_rate_max) {
                g_sleep_state.breath_rate_max = g_sleep_state.result.breath_rate;
            }
        }

        if (g_sleep_state.result.sleep_state == SLEEP_STATE_LIKELY_SLEEP) {
            if (g_sleep_state.likely_sleep_frames < 0xFFFFFFFFUL) {
                g_sleep_state.likely_sleep_frames++;
            }
        } else if (g_sleep_state.result.is_active) {
            if (g_sleep_state.active_frames < 0xFFFFFFFFUL) {
                g_sleep_state.active_frames++;
            }
        } else if (g_sleep_state.quiet_frames < 0xFFFFFFFFUL) {
            g_sleep_state.quiet_frames++;
        }

        if (g_sleep_state.result.is_active) {
            g_sleep_state.current_quiet_frames = 0;
        } else {
            if (g_sleep_state.current_quiet_frames < 0xFFFFFFFFUL) {
                g_sleep_state.current_quiet_frames++;
            }
            if (g_sleep_state.current_quiet_frames > g_sleep_state.longest_quiet_frames) {
                g_sleep_state.longest_quiet_frames = g_sleep_state.current_quiet_frames;
            }
        }

        if (g_sleep_state.result.sleep_state == SLEEP_STATE_LIKELY_SLEEP) {
            if (g_sleep_state.last_sleep_state != SLEEP_STATE_LIKELY_SLEEP) {
                if (g_sleep_state.sleep_segment_count < 0xFFFFU) {
                    g_sleep_state.sleep_segment_count++;
                }
                if (g_sleep_state.sleep_interruption_pending) {
                    if (g_sleep_state.sleep_interruption_count < 0xFFFFU) {
                        g_sleep_state.sleep_interruption_count++;
                    }
                    g_sleep_state.sleep_interruption_pending = false;
                }
                if (g_sleep_state.sleep_turn_pending) {
                    if (g_sleep_state.sleep_turn_count < 0xFFFFU) {
                        g_sleep_state.sleep_turn_count++;
                    }
                    g_sleep_state.sleep_turn_pending = false;
                    g_sleep_state.sleep_turn_pending_frames = 0;
                }
                if (g_sleep_state.sleep_onset_frames == 0) {
                    g_sleep_state.sleep_onset_frames = g_sleep_state.in_bed_frames;
                }
            }
            if (g_sleep_state.current_likely_sleep_frames < 0xFFFFFFFFUL) {
                g_sleep_state.current_likely_sleep_frames++;
            }
            if (g_sleep_state.current_likely_sleep_frames >
                g_sleep_state.longest_likely_sleep_frames) {
                g_sleep_state.longest_likely_sleep_frames =
                    g_sleep_state.current_likely_sleep_frames;
            }
        } else {
            if (g_sleep_state.last_sleep_state == SLEEP_STATE_LIKELY_SLEEP &&
                g_sleep_state.result.sleep_state == SLEEP_STATE_IN_BED_ACTIVE &&
                g_sleep_state.result.is_active) {
                g_sleep_state.sleep_interruption_pending = true;
            }
            g_sleep_state.current_likely_sleep_frames = 0;
        }
        if (g_sleep_state.sleep_turn_pending &&
            g_sleep_state.result.sleep_state != SLEEP_STATE_LIKELY_SLEEP) {
            if (g_sleep_state.sleep_turn_pending_frames <
                SLEEP_TURN_RETURN_TIMEOUT_FRAMES) {
                g_sleep_state.sleep_turn_pending_frames++;
            } else {
                g_sleep_state.sleep_turn_pending = false;
                g_sleep_state.sleep_turn_pending_frames = 0;
            }
        }
        g_sleep_state.last_sleep_state = g_sleep_state.result.sleep_state;
    } else {
        if (g_sleep_state.out_bed_frames < 0xFFFFFFFFUL) {
            g_sleep_state.out_bed_frames++;
        }
        g_sleep_state.last_sleep_state = SLEEP_STATE_OUT_OF_BED;
    }

    g_sleep_state.last_in_bed = in_bed ? 1 : 0;
    g_sleep_state.result.bed_enter_count = g_sleep_state.bed_enter_count;
    g_sleep_state.result.bed_exit_count = g_sleep_state.bed_exit_count;
    g_sleep_state.result.in_bed_frames = g_sleep_state.in_bed_frames;
    g_sleep_state.result.out_bed_frames = g_sleep_state.out_bed_frames;
    g_sleep_state.result.turn_count = g_sleep_state.turn_count;
    g_sleep_state.result.sleep_turn_count = g_sleep_state.sleep_turn_count;
    g_sleep_state.result.breath_state = sleep_detect_breath_state();
    g_sleep_state.result.active_frames = g_sleep_state.active_frames;
    g_sleep_state.result.quiet_frames = g_sleep_state.quiet_frames;
    g_sleep_state.result.likely_sleep_frames = g_sleep_state.likely_sleep_frames;
    g_sleep_state.result.longest_quiet_frames = g_sleep_state.longest_quiet_frames;
    g_sleep_state.result.sleep_onset_frames = g_sleep_state.sleep_onset_frames;
    g_sleep_state.result.sleep_segment_count = g_sleep_state.sleep_segment_count;
    g_sleep_state.result.longest_likely_sleep_frames =
        g_sleep_state.longest_likely_sleep_frames;
    g_sleep_state.result.sleep_interruption_count =
        g_sleep_state.sleep_interruption_count;
    g_sleep_state.result.active_ratio =
        sleep_detect_percent(g_sleep_state.active_frames, g_sleep_state.in_bed_frames);
    g_sleep_state.result.quiet_ratio =
        sleep_detect_percent(g_sleep_state.quiet_frames, g_sleep_state.in_bed_frames);
    g_sleep_state.result.likely_sleep_ratio =
        sleep_detect_percent(g_sleep_state.likely_sleep_frames, g_sleep_state.in_bed_frames);
    g_sleep_state.result.breath_rate_output_ratio =
        sleep_detect_percent(g_sleep_state.breath_rate_samples, g_sleep_state.in_bed_frames);
    g_sleep_state.result.breath_rate_min = g_sleep_state.breath_rate_min;
    g_sleep_state.result.breath_rate_max = g_sleep_state.breath_rate_max;
    g_sleep_state.result.session_end = g_sleep_state.session_end_hold_frames > 0 ? 1 : 0;
    if (g_sleep_state.session_end_hold_frames > 0) {
        g_sleep_state.session_end_hold_frames--;
    }

    if (g_sleep_state.in_bed_frames > 0) {
        g_sleep_state.result.breath_valid_ratio =
            sleep_detect_percent(g_sleep_state.breath_valid_frames, g_sleep_state.in_bed_frames);
    } else {
        g_sleep_state.result.breath_valid_ratio = 0;
    }

    if (g_sleep_state.breath_rate_samples > 0) {
        uint32_t avg = g_sleep_state.breath_rate_sum / g_sleep_state.breath_rate_samples;
        g_sleep_state.result.breath_rate_avg = avg > 255U ? 255U : (uint8_t)avg;
    } else {
        g_sleep_state.result.breath_rate_avg = 0;
    }
}

static void sleep_detect_update_sleep_state(void)
{
    bool in_bed = g_sleep_state.result.has_person && g_sleep_state.result.in_bed;
    bool active = g_sleep_state.result.is_active ? true : false;
    bool breath_good = g_sleep_state.result.breath_valid ||
                       (g_sleep_state.result.breath_rate > 0);
    bool breath_support =
        g_sleep_state.result.breath_signal >= g_sleep_state.cfg.breath_signal_exit_th ||
        g_sleep_state.result.breath_quality >= g_sleep_state.cfg.breath_signal_exit_th;

    if (!in_bed) {
        g_sleep_state.sleep_quiet_streak = 0;
        g_sleep_state.sleep_active_streak = 0;
        g_sleep_state.sleep_breath_streak = 0;
        g_sleep_state.sleep_breath_loss_streak = 0;
        g_sleep_state.result.sleep_state = SLEEP_STATE_OUT_OF_BED;
    } else if (active) {
        g_sleep_state.sleep_quiet_streak = 0;
        g_sleep_state.sleep_breath_streak = 0;
        g_sleep_state.sleep_breath_loss_streak = 0;
        if (g_sleep_state.sleep_active_streak < 0xFFFF) {
            g_sleep_state.sleep_active_streak++;
        }
        g_sleep_state.result.sleep_state = SLEEP_STATE_IN_BED_ACTIVE;
    } else {
        g_sleep_state.sleep_active_streak = 0;
        if (g_sleep_state.sleep_quiet_streak < 0xFFFF) {
            g_sleep_state.sleep_quiet_streak++;
        }
        if (breath_good) {
            g_sleep_state.sleep_breath_loss_streak = 0;
            if (g_sleep_state.sleep_breath_streak < 0xFFFF) {
                g_sleep_state.sleep_breath_streak++;
            }
        } else if (g_sleep_state.result.sleep_state == SLEEP_STATE_LIKELY_SLEEP &&
                   breath_support &&
                   g_sleep_state.sleep_breath_loss_streak <
                       SLEEP_BREATH_LOSS_HOLD_FRAMES) {
            g_sleep_state.sleep_breath_loss_streak++;
        } else {
            g_sleep_state.sleep_breath_streak = 0;
            g_sleep_state.sleep_breath_loss_streak = 0;
        }

        if (g_sleep_state.sleep_quiet_streak >= SLEEP_LIKELY_SLEEP_FRAMES &&
            g_sleep_state.sleep_breath_streak >= SLEEP_BREATH_STABLE_FRAMES) {
            g_sleep_state.result.sleep_state = SLEEP_STATE_LIKELY_SLEEP;
        } else if (g_sleep_state.sleep_quiet_streak >= SLEEP_QUIET_CONFIRM_FRAMES) {
            g_sleep_state.result.sleep_state = SLEEP_STATE_IN_BED_QUIET;
        } else {
            g_sleep_state.result.sleep_state = SLEEP_STATE_IN_BED_ACTIVE;
        }
    }

    g_sleep_state.result.sleep_quiet_streak = g_sleep_state.sleep_quiet_streak;
    g_sleep_state.result.sleep_active_streak = g_sleep_state.sleep_active_streak;
    g_sleep_state.result.sleep_breath_streak = g_sleep_state.sleep_breath_streak;
}

void sleep_detect_reset(void)
{
    SleepDetectConfig cfg = g_sleep_state.cfg;
    g_sleep_state = (SleepDetectState){0};
    g_sleep_state.cfg = cfg;
}

void sleep_detect_init(const SleepDetectConfig *cfg)
{
    g_sleep_state.cfg = cfg ? *cfg : sleep_detect_default_config();
    sleep_detect_reset();
}

static bool point_in_bed_zone(const PointCloud3D *point)
{
    return point->cart.x >= g_sleep_state.cfg.bed_x_min_cm &&
           point->cart.x <= g_sleep_state.cfg.bed_x_max_cm &&
           point->cart.y >= g_sleep_state.cfg.bed_y_min_cm &&
           point->cart.y <= g_sleep_state.cfg.bed_y_max_cm &&
           point->cart.z >= g_sleep_state.cfg.bed_z_min_cm &&
           point->cart.z <= g_sleep_state.cfg.bed_z_max_cm;
}

bool sleep_detect_is_point_in_bed_zone(const PointCloud3D *point)
{
    if (point == NULL) {
        return false;
    }

    return point_in_bed_zone(point);
}

void sleep_detect_update(const PointCloud3D *motion_points,
                         uint16_t motion_points_num,
                         const PointCloud3D *micro_points,
                         uint16_t micro_points_num)
{
    uint16_t motion_in_bed = 0;
    uint16_t micro_in_bed = 0;
    uint16_t active_motion_points = 0;
    uint16_t motion_points_total = motion_points_num;
    uint16_t micro_points_total = micro_points_num;
    uint16_t motion_points_bed = 0;
    uint16_t micro_points_bed = 0;
    bool person_detected;
    bool bed_detected;
    bool breath_detected;

    if (motion_points == NULL) {
        motion_points_num = 0;
    }
    if (micro_points == NULL) {
        micro_points_num = 0;
    }

    g_sleep_state.frame_count++;

    for (uint16_t i = 0; i < motion_points_num; ++i) {
        if (point_in_bed_zone(&motion_points[i])) {
            motion_in_bed++;
            if (ABS(motion_points[i].cart.vel) >= g_sleep_state.cfg.active_velocity_abs_th_cm_s) {
                active_motion_points++;
            }
        }
    }

    for (uint16_t i = 0; i < micro_points_num; ++i) {
        if (point_in_bed_zone(&micro_points[i])) {
            micro_in_bed++;
        }
    }

    motion_points_bed = motion_in_bed;
    micro_points_bed = micro_in_bed;

    g_sleep_state.result.motion_points_total = motion_points_total;
    g_sleep_state.result.micro_points_total = micro_points_total;
    g_sleep_state.result.motion_points_bed = motion_points_bed;
    g_sleep_state.result.micro_points_bed = micro_points_bed;
    g_sleep_state.result.active_motion_points = active_motion_points;

    sleep_detect_push_breath_signal(micro_in_bed);
    g_sleep_state.result.breath_signal = micro_in_bed;
    g_sleep_state.result.breath_quality = sleep_detect_avg_breath_signal();
    g_sleep_state.result.breath_period_frames = 0;
    g_sleep_state.result.breath_rate_candidate = g_sleep_state.breath_rate_candidate;
    g_sleep_state.result.breath_rate_repeat_count = g_sleep_state.breath_rate_repeat_count;
    g_sleep_state.result.breath_rate_stable = g_sleep_state.breath_rate_stable;

    person_detected = g_sleep_state.result.has_person;
    bed_detected = g_sleep_state.result.in_bed;

    if (!g_sleep_state.result.in_bed) {
        bool enter_motion = (motion_in_bed >= BED_ENTER_MOTION_POINTS_TH) ||
                            (active_motion_points >= 1);
        person_detected = enter_motion;
        bed_detected = enter_motion;
    } else {
        bool valid_micro = micro_in_bed >= g_sleep_state.cfg.micro_presence_points_th;
        bool strong_motion = motion_in_bed >= BED_KEEP_MOTION_POINTS_TH;
        bool keep_bed = strong_motion || valid_micro;
        bed_detected = keep_bed;
        person_detected = keep_bed;
    }

    breath_detected = g_sleep_state.result.in_bed &&
                      (active_motion_points == 0) &&
                      (g_sleep_state.result.breath_quality >= g_sleep_state.cfg.breath_signal_enter_th) &&
                      (micro_in_bed >= g_sleep_state.cfg.micro_presence_points_th);

    if (g_sleep_state.result.breath_valid &&
        ((g_sleep_state.result.breath_quality < g_sleep_state.cfg.breath_signal_exit_th) ||
         (micro_in_bed == 0))) {
        breath_detected = false;
    }


    if (person_detected) {
        if (g_sleep_state.person_present_streak < 0xFFFF) {
            g_sleep_state.person_present_streak++;
        }
        g_sleep_state.person_absent_streak = 0;
    } else {
        g_sleep_state.person_present_streak = 0;
        if (g_sleep_state.person_absent_streak < 0xFFFF) {
            g_sleep_state.person_absent_streak++;
        }
    }

    g_sleep_state.result.person_present_streak = g_sleep_state.person_present_streak;
    g_sleep_state.result.person_absent_streak = g_sleep_state.person_absent_streak;

    if (bed_detected) {
        if (g_sleep_state.bed_present_streak < 0xFFFF) {
            g_sleep_state.bed_present_streak++;
        }
        g_sleep_state.bed_absent_streak = 0;
    } else {
        g_sleep_state.bed_present_streak = 0;
        if (g_sleep_state.bed_absent_streak < 0xFFFF) {
            g_sleep_state.bed_absent_streak++;
        }
    }

    g_sleep_state.result.bed_present_streak = g_sleep_state.bed_present_streak;
    g_sleep_state.result.bed_absent_streak = g_sleep_state.bed_absent_streak;

    if (breath_detected) {
        if (g_sleep_state.breath_present_streak < 0xFFFF) {
            g_sleep_state.breath_present_streak++;
        }
        g_sleep_state.breath_absent_streak = 0;
    } else {
        g_sleep_state.breath_present_streak = 0;
        if (g_sleep_state.breath_absent_streak < 0xFFFF) {
            g_sleep_state.breath_absent_streak++;
        }
    }

    g_sleep_state.result.breath_present_streak = g_sleep_state.breath_present_streak;
    g_sleep_state.result.breath_absent_streak = g_sleep_state.breath_absent_streak;

    if (g_sleep_state.person_present_streak >= g_sleep_state.cfg.enter_confirm_frames) {
        g_sleep_state.result.has_person = 1;
    } else if (g_sleep_state.person_absent_streak >= g_sleep_state.cfg.exit_confirm_frames) {
        g_sleep_state.result.has_person = 0;
    }

    if (g_sleep_state.bed_present_streak >= g_sleep_state.cfg.enter_confirm_frames) {
        g_sleep_state.result.in_bed = 1;
    } else if (g_sleep_state.bed_absent_streak >= g_sleep_state.cfg.exit_confirm_frames) {
        g_sleep_state.result.in_bed = 0;
    }

    g_sleep_state.result.is_active = active_motion_points >= g_sleep_state.cfg.active_motion_points_th ? 1 : 0;
    g_sleep_state.result.motion_intensity =
        sleep_detect_motion_intensity(motion_in_bed, active_motion_points);

    if (g_sleep_state.breath_present_streak >= g_sleep_state.cfg.enter_confirm_frames) {
        g_sleep_state.result.breath_valid = 1;
    } else if (g_sleep_state.breath_absent_streak >= 8) {
        g_sleep_state.result.breath_valid = 0;
    }

    if (g_sleep_state.result.breath_valid) {
        uint16_t period_frames = 0;
        uint16_t estimated_rate = sleep_detect_estimate_breath_rate(&period_frames);
        uint16_t stable_rate = 0;

        g_sleep_state.result.breath_period_frames = period_frames;
        if (estimated_rate >= BREATH_RATE_STABLE_MIN_BPM && estimated_rate <= BREATH_RATE_MAX_BPM) {
            stable_rate = sleep_detect_stabilize_breath_rate(estimated_rate);
            g_sleep_state.result.breath_rate = (uint8_t)sleep_detect_public_breath_rate(stable_rate, true);
        } else {
            stable_rate = sleep_detect_stabilize_breath_rate(0);
            g_sleep_state.result.breath_rate_stable = g_sleep_state.breath_rate_stable;
            g_sleep_state.result.breath_rate = (uint8_t)sleep_detect_public_breath_rate(stable_rate, false);
        }
    } else {
        g_sleep_state.result.breath_period_frames = 0;
        if (!g_sleep_state.result.in_bed) {
            g_sleep_state.result.breath_rate = 0;
            g_sleep_state.breath_rate_candidate = 0;
            g_sleep_state.breath_rate_repeat_count = 0;
            g_sleep_state.breath_rate_stable = 0;
            g_sleep_state.breath_rate_output_count = 0;
            g_sleep_state.result.breath_rate_candidate = 0;
            g_sleep_state.result.breath_rate_repeat_count = 0;
            g_sleep_state.result.breath_rate_stable = 0;
        } else {
            g_sleep_state.result.breath_rate =
                (uint8_t)sleep_detect_public_breath_rate(g_sleep_state.breath_rate_stable, false);
            g_sleep_state.result.breath_rate_candidate = g_sleep_state.breath_rate_candidate;
            g_sleep_state.result.breath_rate_repeat_count = g_sleep_state.breath_rate_repeat_count;
            g_sleep_state.result.breath_rate_stable = g_sleep_state.breath_rate_stable;
        }
    }

    sleep_detect_update_turn_event(motion_in_bed, active_motion_points);
    sleep_detect_update_sleep_state();
    sleep_detect_update_session_stats();

    (void)motion_points_total;
    (void)micro_points_total;
    (void)motion_points_bed;
    (void)micro_points_bed;
}

void sleep_detect_update_breath_phase(bool valid,
                                      uint16_t target_bin,
                                      int32_t phase_mrad,
                                      uint32_t amplitude)
{
    int32_t delta;
    uint16_t period_frames = 0;
    uint16_t estimated_rate;

    g_sleep_state.result.breath_phase_valid = valid ? 1 : 0;
    g_sleep_state.result.breath_target_bin = target_bin;
    g_sleep_state.result.breath_phase_mrad = (int16_t)phase_mrad;
    g_sleep_state.result.breath_phase_amp = amplitude;

    if (!valid || !g_sleep_state.result.in_bed || g_sleep_state.result.is_active) {
        sleep_detect_reset_breath_phase_window();
        return;
    }

    if (!g_sleep_state.breath_phase_has_last) {
        g_sleep_state.breath_phase_has_last = true;
        g_sleep_state.breath_phase_last_mrad = phase_mrad;
        g_sleep_state.breath_phase_unwrapped_mrad = phase_mrad;
        g_sleep_state.result.breath_phase_delta_mrad = 0;
        return;
    }

    delta = phase_mrad - g_sleep_state.breath_phase_last_mrad;
    while (delta > 3142) {
        delta -= 6283;
    }
    while (delta < -3142) {
        delta += 6283;
    }

    if (delta > BREATH_PHASE_DELTA_MAX_MRAD || delta < -BREATH_PHASE_DELTA_MAX_MRAD) {
        g_sleep_state.breath_phase_last_mrad = phase_mrad;
        g_sleep_state.result.breath_phase_delta_mrad = 0;
        return;
    }

    g_sleep_state.breath_phase_last_mrad = phase_mrad;
    g_sleep_state.breath_phase_unwrapped_mrad += delta;
    g_sleep_state.result.breath_phase_delta_mrad = (int16_t)delta;
    sleep_detect_push_breath_phase(g_sleep_state.breath_phase_unwrapped_mrad);
    g_sleep_state.result.breath_phase_quality = g_sleep_state.breath_phase_count;

    estimated_rate = sleep_detect_estimate_phase_breath_rate(&period_frames);
    g_sleep_state.result.breath_period_frames = period_frames;
    if (estimated_rate >= BREATH_RATE_STABLE_MIN_BPM && estimated_rate <= BREATH_RATE_MAX_BPM) {
        uint16_t stable_rate = sleep_detect_stabilize_breath_rate(estimated_rate);
        g_sleep_state.result.breath_valid = 1;
        g_sleep_state.result.breath_rate = (uint8_t)sleep_detect_public_breath_rate(stable_rate, true);
    } else {
        uint16_t stable_rate = sleep_detect_stabilize_breath_rate(0);
        g_sleep_state.result.breath_rate = (uint8_t)sleep_detect_public_breath_rate(stable_rate, false);
    }
}

const SleepDetectResult *sleep_detect_get_result(void)
{
    return &g_sleep_state.result;
}

bool sleep_detect_should_report(void)
{
    uint16_t interval = g_sleep_state.cfg.report_interval_frames;

    if (interval == 0) {
        interval = 1;
    }

    return (g_sleep_state.frame_count % interval) == 0;
}
