#ifndef SLEEP_MOUNT_MATH_H
#define SLEEP_MOUNT_MATH_H

/* Pure math; no MCU/RTOS dependencies. All distances are centimetres.
 * Upright radar axes, positive pitch downward; height is above the mattress.
 * The caller supplies cached cos(pitch) and sin(pitch), with pitch in radians.
 * This changes the ROI coordinate frame only, never the raw point/telemetry.
 */
typedef struct {
    float x;
    float y;
    float z;
} SleepMountPoint;

static inline SleepMountPoint sleep_mount_to_bed(float x, float y, float z,
                                                float height_cm,
                                                float cos_pitch,
                                                float sin_pitch)
{
    SleepMountPoint bed;

    bed.x = x;
    bed.y = y * cos_pitch + z * sin_pitch;
    bed.z = height_cm - y * sin_pitch + z * cos_pitch;
    return bed;
}

#endif
