#ifndef SLEEP_DETECT_H
#define SLEEP_DETECT_H

#include <stdint.h>
#include <stdbool.h>
#include "radar_types.h"

#define BREATH_WINDOW_SIZE 100

#define SLEEP_STATE_OUT_OF_BED 0U
#define SLEEP_STATE_IN_BED_ACTIVE 1U
#define SLEEP_STATE_IN_BED_QUIET 2U
#define SLEEP_STATE_LIKELY_SLEEP 3U

typedef struct {
    uint8_t has_person;
    uint8_t is_active;
    uint8_t in_bed;
    uint8_t breath_valid;
    uint8_t breath_rate;
    uint8_t sleep_state;
    uint16_t motion_points_total;
    uint16_t micro_points_total;
    uint16_t motion_points_bed;
    uint16_t micro_points_bed;
    uint16_t active_motion_points;
    uint16_t person_present_streak;
    uint16_t person_absent_streak;
    uint16_t bed_present_streak;
    uint16_t bed_absent_streak;
    uint16_t breath_present_streak;
    uint16_t breath_absent_streak;
    uint16_t breath_signal;
    uint16_t breath_quality;
    uint16_t breath_period_frames;
    uint16_t breath_rate_candidate;
    uint16_t breath_rate_repeat_count;
    uint16_t breath_rate_stable;
    uint16_t sleep_quiet_streak;
    uint16_t sleep_active_streak;
    uint16_t sleep_breath_streak;
    uint8_t motion_intensity;
    uint8_t turn_event;
    uint16_t turn_count;
    uint16_t sleep_turn_count;
    uint16_t bed_enter_count;
    uint16_t bed_exit_count;
    uint32_t in_bed_frames;
    uint32_t out_bed_frames;
    uint8_t breath_valid_ratio;
    uint8_t breath_rate_avg;
    uint8_t breath_state;
    uint32_t active_frames;
    uint32_t quiet_frames;
    uint32_t likely_sleep_frames;
    uint32_t longest_quiet_frames;
    uint32_t sleep_onset_frames;
    uint16_t sleep_segment_count;
    uint32_t longest_likely_sleep_frames;
    uint16_t sleep_interruption_count;
    uint8_t active_ratio;
    uint8_t quiet_ratio;
    uint8_t likely_sleep_ratio;
    uint8_t breath_rate_output_ratio;
    uint8_t session_end;
    uint8_t breath_rate_min;
    uint8_t breath_rate_max;
    uint8_t breath_phase_valid;
    uint16_t breath_target_bin;
    int16_t breath_phase_mrad;
    int16_t breath_phase_delta_mrad;
    uint32_t breath_phase_amp;
    uint16_t breath_phase_quality;
} SleepDetectResult;

typedef struct {
    int16_t bed_x_min_cm;
    int16_t bed_x_max_cm;
    int16_t bed_y_min_cm;
    int16_t bed_y_max_cm;
    int16_t bed_z_min_cm;
    int16_t bed_z_max_cm;
    uint16_t enter_confirm_frames;
    uint16_t exit_confirm_frames;
    uint16_t active_motion_points_th;
    uint16_t active_velocity_abs_th_cm_s;
    uint16_t micro_presence_points_th;
    uint16_t breath_signal_enter_th;
    uint16_t breath_signal_exit_th;
    uint16_t report_interval_frames;
} SleepDetectConfig;

void sleep_detect_init(const SleepDetectConfig *cfg);
void sleep_detect_reset(void);
void sleep_detect_update(const PointCloud3D *motion_points,
                         uint16_t motion_points_num,
                         const PointCloud3D *micro_points,
                         uint16_t micro_points_num);
void sleep_detect_update_breath_phase(bool valid,
                                      uint16_t target_bin,
                                      int32_t phase_mrad,
                                      uint32_t amplitude);
const SleepDetectResult *sleep_detect_get_result(void);
bool sleep_detect_should_report(void);
bool sleep_detect_is_point_in_bed_zone(const PointCloud3D *point);

#endif
