#include "radar_iq.h"
const unsigned radar_iq_target_bytes=sizeof(RadarIq);
const unsigned radar_iq_time_offset=offsetof(RadarIq,source_ms);
const unsigned radar_iq_iq_offset=offsetof(RadarIq,iq);
const unsigned radar_iq_flags_offset=offsetof(RadarIq,flags);
