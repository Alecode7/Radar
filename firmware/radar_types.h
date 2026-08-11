#ifndef RADAR_TYPES_H
#define RADAR_TYPES_H

#include <stdint.h>

/* Minimal point-cloud types required by the standalone sleep algorithm. */
typedef struct {
    int16_t range;
    int16_t azim;
    int16_t elev;
    int16_t vel;
    int16_t snr;
    int16_t track_uid;
} PointCloud_Polar;

typedef struct {
    int16_t x;
    int16_t y;
    int16_t z;
    int16_t vel;
    int16_t snr;
    int16_t track_uid;
} PointCloud_Cart;

typedef union {
    PointCloud_Polar polar;
    PointCloud_Cart cart;
} PointCloud3D;

#endif
