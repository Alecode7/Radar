#ifndef PRODUCT_PROTOCOL_H
#define PRODUCT_PROTOCOL_H
#include <stddef.h>
#include <stdint.h>
#ifdef _WIN32
#define P1_API __declspec(dllexport)
#else
#define P1_API
#endif
#define P1_FRAME_MAX 256u
#define P1_CONFIG_BYTES 64u
#define P1_EVENT_CAP 32u
#define P1_EVENT_BYTES 32u
enum {P1_OK=0,P1_UNSUPPORTED=1,P1_BAD_LENGTH=2,P1_BAD_ARGUMENT=3,P1_BUSY=4,
      P1_NOT_READY=5,P1_CONFLICT=6,P1_STORAGE=7,P1_INTERNAL=8,P1_SESSION_REQUIRED=9,
      P1_OLD_SEQUENCE=10,P1_BOOT_CHANGED=12};
enum {P1_CAPS=0x0100,P1_RESULT=0x0101,P1_IDENTITY=0x0102,P1_OPEN=0x0110,
      P1_LINK_RESULT=0x0111,P1_HEALTH=0x0112,P1_START=0x0120,P1_STOP=0x0121,
      P1_CONTROL=0x0122,P1_SNAPSHOT=0x0123,P1_STREAM=0x0124,
      P1_CONFIG_GET=0x0130,P1_CONFIG_SET=0x0131,P1_CONFIG_SAVE=0x0132,P1_CONFIG_DEFAULT=0x0133,P1_LIMITS=0x0134,P1_INSTALL=0x0135,
      P1_ALERTS=0x0140,P1_EVENTS=0x0141,P1_EVENT_ACK=0x0142,P1_STATS=0x0150,
      P1_RESULT_PUSH=0x8101,P1_HEARTBEAT=0x8200,P1_EVENT_PUSH=0x8201};
enum {P1_STOPPED=0,P1_RUNNING=1,P1_PENDING=2,P1_CONTROL_FAULT=3};
enum {P1_BR_LOW=0,P1_BR_HIGH=1,P1_HR_LOW=2,P1_HR_HIGH=3,
      P1_BR_MISSING=4,P1_HR_MISSING=5,P1_BED_LONG=6,P1_BED_AWAY=7};
enum {P1_EVENT_ACTIVE=1,P1_EVENT_RECOVER=2,P1_EVENT_UNKNOWN=3,P1_EVENT_END=4,P1_EVENT_RESUME=5};
typedef struct {
    uint32_t revision,confirm_ms,recover_ms,missing_br_ms,missing_hr_ms,long_bed_ms,away_ms;
    uint16_t report_ms,heartbeat_ms,enabled,br_low,br_high,hr_low,hr_high;
    uint8_t min_hr_quality,reject_harmonic,mount_enabled;
    uint16_t mount_height,mount_pitch;
} P1Config;
typedef struct {uint8_t state,active,tracking,evidence;uint32_t since,last_sequence;} P1Rule;
typedef int (*P1Store)(const uint8_t *,size_t,void *);
typedef struct {
    uint64_t boot_id,token,uptime_ms,operation;
    uint32_t now,last_tick,highest_sequence,open_sequence,monitor_id;
    uint32_t pending_at,last_control_send,control_counter,control_error,faults;
    uint32_t last_sleep,bed_since,away_since,last_br,last_hr,br_seq,hr_seq;
    uint32_t last_good_br,last_good_hr,generation,radar_received,events_total;
    uint32_t monitor_start,monitor_stop,in_bed_ms,active_ms,br_count,hr_count;
    uint64_t br_sum,hr_sum;
    uint32_t event_last,event_count,event_ack,event_cursor,event_overwrites;
    uint32_t last_result_send,last_heartbeat_send,push_sequence,source_resets;
    uint16_t br,hr;
    uint8_t ctrl,actual_running,monitoring,source_ready,source_seen,have_sleep;
    uint8_t presence,bed,motion,sleep_state,breath_valid,hr_quality,hr_harmonic;
    uint8_t have_br,have_hr,have_good_br,have_good_hr,was_in_bed;
    uint8_t pending_action,control_tries,write_session,stream_mask,have_sent_control,stored;
    uint8_t install_known,install_enabled;
    uint16_t install_height,install_pitch;
    uint32_t install_at;
    P1Config config;
    P1Rule rules[8];
    uint8_t events[P1_EVENT_CAP][P1_EVENT_BYTES];
    uint8_t last_request[P1_FRAME_MAX],last_reply[P1_FRAME_MAX];
    uint16_t last_request_size,last_reply_size;
    P1Store store;void *store_context;
} P1State;
typedef struct {uint8_t bytes[P1_FRAME_MAX];uint16_t used;uint32_t last_ms,errors;} P1Parser;
typedef void (*P1Receive)(const uint8_t *,size_t,void *);
P1_API size_t p1_size(void);
P1_API void p1_config_default(P1Config *);
P1_API void p1_config_encode(const P1Config *,uint8_t *);
P1_API int p1_config_decode(const uint8_t *,size_t,P1Config *);
P1_API void p1_init(P1State *,uint64_t,uint32_t,P1Store,void *);
P1_API int p1_load_config(P1State *,const uint8_t *,size_t);
P1_API void p1_tick(P1State *,uint32_t);
P1_API void p1_source(P1State *,int,uint32_t);
P1_API void p1_input_epoch(P1State *,uint32_t,uint32_t,unsigned);
P1_API void p1_install_observation(P1State *,unsigned,unsigned,unsigned,uint32_t);
P1_API void p1_sleep(P1State *,unsigned,unsigned,unsigned,unsigned,unsigned,uint32_t);
P1_API void p1_breath(P1State *,uint32_t,uint16_t,uint32_t,uint32_t);
P1_API void p1_heart(P1State *,int,unsigned,unsigned,unsigned,uint32_t,uint32_t,uint32_t);
P1_API int p1_begin_control(P1State *,unsigned,uint32_t);
P1_API int p1_control_due(P1State *,uint32_t,uint64_t *,unsigned *);
P1_API void p1_control_ack(P1State *,uint64_t,unsigned,int,unsigned,uint32_t);
P1_API size_t p1_handle(P1State *,const uint8_t *,size_t,uint8_t *,size_t,uint32_t);
P1_API size_t p1_push(P1State *,uint8_t *,size_t,uint32_t);
P1_API uint16_t p1_crc(const uint8_t *,size_t);
P1_API size_t p1_frame(unsigned,unsigned,uint32_t,const uint8_t *,size_t,uint8_t *,size_t);
P1_API void p1_parser_init(P1Parser *);
P1_API void p1_parser_feed(P1Parser *,const uint8_t *,size_t,uint32_t,P1Receive,void *);
P1_API void p1_parser_poll(P1Parser *,uint32_t,P1Receive,void *);
#endif
