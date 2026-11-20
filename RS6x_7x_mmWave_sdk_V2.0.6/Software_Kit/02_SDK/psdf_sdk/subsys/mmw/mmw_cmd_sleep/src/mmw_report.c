/*
 **************************************************************************************************
 *          Copyright (c) 2022 Possumic Technology. all rights reserved.
 **************************************************************************************************
 */
#include <hal_types.h>
#include "mmw_ctrl.h"
#include "mmw_report.h"
#include "mmw_alg_debug.h"

#include "hif_com.h"
#include "hif_checksum.h"
#include "log.h"
#include "util.h"

#include "mmw_app_pointcloud_config.h"
#include "mmw_alg_pointcloud.h"
#include "mmw_app_micro_pointcloud.h"
#include "mmw_alg_micro_cube.h"
#include "mmw_point_cloud_psic_lib.h"
#include "sleep_detect.h"
#include "heart_rate_device.h"
#include "heart_iq_capture.h"
#include "heart_sampling_profile.h"
#include "heart_fast_frontend.h"
#include "heart_fast_packet.h"
#include <math.h>
#include <stdio.h>
#include "sleep_async_log.h"
#define printf sleep_trace_printf
#include "heart_target_window.h"
#include "heart_raw_record.h"
#include "heart_clock_capture.h"
#include "hr_migration_clock.h"
/* Owned by the single data callback; HFF is copied before HRRAW reuses it. */
static char s_fast_line[448];
static bool s_fast_input_valid;
static uint16_t s_fast_input_bin;


#ifndef MIN
#define MIN(x, y)					((x) < (y) ? (x) : (y))
#endif

#define SLEEP_PROCESS_FRAME_DIV		1U
#define SLEEP_HEART_RAW_ENABLE		0U
#define SLEEP_HEART_SAFE_REPORT_ENABLE	1U
#define SLEEP_BINARY_POINT_UPLOAD_ENABLE	0U
#define SLEEP_HEART_BIN_RADIUS		1U
#define SLEEP_HEART_BIN_COUNT		(SLEEP_HEART_BIN_RADIUS * 2U + 1U)

/* 
 * The fisrt and second frame after call mmw_start() contains invalid data, so we should bypass 2st frame
 * This flag is set in cmd_mmw_report() function, and count down in mmw_ctrl_data_cube_spi/uart_report_cb/mmw_ctrl_data_point_report_cb()
 */
static uint8_t s_mmw_radar_analysis_skip_frame_status;
static bool s_sleep_report_cb_seen;
static bool s_sleep_process_seen;
static bool s_sleep_breath_target_valid;
static uint16_t s_sleep_breath_target_range_idx;
static bool s_sleep_heart_target_valid;
static uint16_t s_sleep_heart_target_range_idx;
/* Match the PC estimator's last positive BR from the SLEEP status stream.
 * Do not silently replace that input with a newer per-frame BR mid-interval. */
static uint8_t s_sleep_heart_breath_reference;

#if (CONFIG_MMW_MICRO_POINT_CLOUD)
extern uint8_t g_micro_point_en;
#endif
/* cfar method select: 0/1 */
extern uint8_t g_cfar_method;

int get_tx_rx_num(uint8_t *tx, uint8_t *rx)
{
	int ret = 0;
	uint8_t work = 0;
	uint8_t txrx = 0;

	ret = mmw_mode_get(&txrx, &work);
	if(ret) {
		printk("mmw mode get fail ! ret = %d \r\n", ret);
		return ret;
	}

	switch(txrx) {
		case MMW_MIMO_1T1R:
			*tx = 1;
			*rx = 1;
			break;
		case MMW_MIMO_1T3R:
			*tx = 1;
			*rx = 3;
			break;
		case MMW_MIMO_2T3R:
			*tx = 2;
			*rx = 3;
			break;
		case MMW_MIMO_1T4R:
			*tx = 1;
			*rx = 4;
			break;
		case MMW_MIMO_2T4R:
			*tx = 2;
			*rx = 4;
			break;
		case MMW_MIMO_1T2R:
			*tx = 1;
			*rx = 2;
			break;
		default:
			ret = -1;
			break;
	}
	return ret;
}

struct cube_report_info_t{
	MMW_FRAME_UPLOAD report_hdr;
	uint32_t report_time_ms;
	uint32_t next_frame_time;
	uint32_t frame_cb_cnt;
	uint16_t range_fft_num;
	uint16_t doppler_fft_num;
	uint16_t range_idx;
	uint16_t dop_idx;
	uint16_t block_cnt;
    uint16_t block_idx;
    uint16_t finish_idx;
	uint8_t  aborted;
	uint8_t  reserve;
	uint8_t tx_cnt;
	uint8_t rx_cnt;
	uint8_t tx_idx;
	uint8_t rx_idx;
} cube_report_info;

struct cube_retry_info_t{
    HOST_POLL_APP_CUBE *retry_cfg;
    uint16_t retry_block_idx;
    uint16_t retry_offset;
    uint16_t retry_arr_idx;
} cube_retry_info;

#define CUBE_CACHE_BLOCK    (4)
OSI_Semaphore_t cube_report_sem;
uint8_t *cube_cache[CUBE_CACHE_BLOCK];
bool cube_access = false;

Detection3D_Data D_data_test = {0x00};
Detection3D_State D_State_test = {0x00};

void cube_report_frame_reset(void)
{
	cube_report_info.report_hdr.offset = 0;
	cube_report_info.range_idx = 0;
	cube_report_info.dop_idx   = 0;
	cube_report_info.tx_idx = 0;
	cube_report_info.rx_idx = 0;

	cube_report_info.block_idx = 0;
	cube_report_info.finish_idx = 0;
	cube_report_info.aborted   = 0;
}

static bool is_cube_report_frame_finish(void)
{
	return (cube_report_info.block_idx && cube_report_info.finish_idx >= cube_report_info.block_idx);
}


static void cube_access_ctrl(bool enable)
{
    if(enable == cube_access) {
        return ;
    } else {
        cube_access = enable;
        if(enable) {
            mmw_motion_cube_access_open();
        } else {
            mmw_motion_cube_access_close();
        }
    }
}

void cube_report_callback(hif_queue_item_t *hif_queue_item, int8_t status);
static int hif_dma_cube_report_handler(uint8_t idx)
{
	HIF_MsgHdr_t hifHdr = {0x00};
	uint8_t *cube_data = NULL;
	uint16_t read_num = 0;
	uint16_t offset = CUBE_HEAD_LEN;
	uint16_t read_cnt = 0;
    int ret = 0;

    hifHdr.flag = (1<<2);
	hifHdr.msg_id = 0xC1;
	hifHdr.type = 2;

    LOG_IO(0x1000, 11, 1, 0);
	//memset(cube_cache[idx], 0, CUBE_SIZE_MAX);
	cube_data = cube_cache[idx];

    for(uint32_t i=0; i<cube_report_info.tx_cnt*cube_report_info.rx_cnt*cube_report_info.doppler_fft_num;i++ ) {
        if((CUBE_SIZE_MAX - offset) >= 4 * (cube_report_info.range_fft_num - cube_report_info.range_idx)) {
            read_num = cube_report_info.range_fft_num - cube_report_info.range_idx;
        } else {
            read_num = (CUBE_SIZE_MAX - offset) / 4;
        }
        __mmw_fft_range((complex16_cube *)&cube_data[offset], cube_report_info.range_idx, read_num, cube_report_info.tx_idx, cube_report_info.rx_idx, cube_report_info.dop_idx);
        offset = offset + (4*read_num);
        cube_report_info.range_idx += read_num;
        if(cube_report_info.range_idx >= cube_report_info.range_fft_num) {
            cube_report_info.range_idx = 0;
            cube_report_info.dop_idx ++;
            if(cube_report_info.dop_idx == cube_report_info.doppler_fft_num) {
                cube_report_info.dop_idx = 0;
                cube_report_info.rx_idx ++;
                if(cube_report_info.rx_idx == cube_report_info.rx_cnt) {
                    cube_report_info.rx_idx = 0;
                    cube_report_info.tx_idx ++;
                    if(cube_report_info.tx_idx == cube_report_info.tx_cnt) {
                        cube_report_info.tx_idx = 0;
                    }
                }
            }
        }
        read_cnt = (offset - CUBE_HEAD_LEN) / 4;
        if(offset == (CUBE_SIZE_MAX) || (cube_report_info.report_hdr.frame_len - cube_report_info.report_hdr.offset - read_cnt) == 0) {
            memcpy(cube_data, &cube_report_info.report_hdr, sizeof(MMW_FRAME_UPLOAD));
            hifHdr.length = offset;
            ret = HIF_MsgReport(&hifHdr, cube_data, cube_report_callback);
            cube_report_info.report_hdr.offset += read_cnt;
            LOG_IO(0x1000, 11, 0, 0);
            return ret;
        }
    }
	return -1;
}

void cube_report_retry_callback(hif_queue_item_t *hif_queue_item, int8_t status);
static int hif_dma_cube_report_retry_send(uint32_t retry_offset, uint8_t retry_idx)
{
    uint16_t retry_range_idx = retry_offset % cube_report_info.range_fft_num;
    uint16_t retry_dop = (retry_offset - retry_range_idx) / cube_report_info.range_fft_num;
    uint16_t retry_dop_idx = retry_dop % cube_report_info.doppler_fft_num;
    uint16_t retry_rx = (retry_dop - retry_dop_idx) / cube_report_info.doppler_fft_num;
    uint8_t retry_rx_idx = retry_rx % cube_report_info.rx_cnt;
    uint16_t retry_tx = (retry_rx - retry_rx_idx) / cube_report_info.rx_cnt;
    uint8_t retry_tx_idx = retry_tx % cube_report_info.tx_cnt;

    MMW_FRAME_UPLOAD retry_report_hdr;
	HIF_MsgHdr_t hifHdr = {0x00};
	uint8_t *cube_data = NULL;
	uint16_t read_num = 0;
	uint16_t offset = CUBE_HEAD_LEN;
	uint16_t read_cnt = 0;

    hifHdr.flag = (1<<2);
	hifHdr.msg_id = 0xC1;
	hifHdr.type = 2;

    LOG_IO(0x1000, 11, 1, 0);
	//memset(cube_cache[retry_idx], 0, CUBE_SIZE_MAX);
	cube_data = cube_cache[retry_idx];

    for(uint32_t i = 0; i < cube_report_info.tx_cnt*cube_report_info.rx_cnt*cube_report_info.doppler_fft_num; i++) {
        if((CUBE_SIZE_MAX - offset) >= 4 * (cube_report_info.range_fft_num - retry_range_idx)) {
            read_num = cube_report_info.range_fft_num - retry_range_idx;
        } else {
            read_num = (CUBE_SIZE_MAX - offset) / 4;
        }
        __mmw_fft_range((complex16_cube *)&cube_data[offset], retry_range_idx, read_num, retry_tx_idx, retry_rx_idx, retry_dop_idx);
        offset = offset + (4*read_num);
        retry_range_idx += read_num;
        if(retry_range_idx >= cube_report_info.range_fft_num) {
            retry_range_idx = 0;
            retry_dop_idx ++;
            if(retry_dop_idx == cube_report_info.doppler_fft_num) {
                retry_dop_idx = 0;
                retry_rx_idx ++;
                if(retry_rx_idx == cube_report_info.rx_cnt) {
                    retry_rx_idx = 0;
                    retry_tx_idx ++;
                    if(retry_tx_idx == cube_report_info.tx_cnt) {
                        retry_tx_idx = 0;
                    }
                }
            }
        }
        read_cnt = (offset - CUBE_HEAD_LEN) / 4;
        retry_report_hdr.frame_len = cube_report_info.report_hdr.frame_len;
        retry_report_hdr.offset = retry_offset;
        retry_report_hdr.frame_idx = cube_report_info.report_hdr.frame_idx;
        if(offset == (CUBE_SIZE_MAX) || (retry_report_hdr.frame_len - retry_report_hdr.offset - read_cnt) == 0) {
            memcpy(cube_data, &retry_report_hdr, sizeof(MMW_FRAME_UPLOAD));
            hifHdr.length = offset;
		 	LOG_IO(0x1000, 11, 0, 0);
            return HIF_MsgReport(&hifHdr, cube_data, cube_report_retry_callback);
        }
    }
	return -1;
}

void cube_report_retry_callback(hif_queue_item_t *hif_queue_item, int8_t status)
{
	/* 1. frame is aborted.
     * 2. time is not enough */
    if (cube_report_info.aborted || time_after(csi_coret_get_value(), cube_report_info.next_frame_time)) {
		if (cube_retry_info.retry_cfg) {
			OSI_Free(cube_retry_info.retry_cfg);
			cube_retry_info.retry_cfg = NULL;
			cube_access_ctrl(false);
		}
        return ;
    }

    if(cube_retry_info.retry_cfg && cube_retry_info.retry_cfg->retry_num) {
        LOG_IO(0x1000, 11, 0, 6);
		uint32_t frame_len = cube_report_info.report_hdr.frame_len;
		uint32_t retry_arr_idx = cube_retry_info.retry_arr_idx;
		uint32_t offset = cube_retry_info.retry_cfg->retry[retry_arr_idx].offset + cube_retry_info.retry_offset;
		uint32_t read_num = MIN(CUBE_DATA_SIZE / 4, cube_retry_info.retry_cfg->retry[retry_arr_idx].len);
		if (read_num && (offset + read_num <= frame_len)) {
			hif_dma_cube_report_retry_send(offset, (cube_retry_info.retry_block_idx++) % CUBE_CACHE_BLOCK);
			cube_retry_info.retry_offset += read_num;
			LOG_IO(0x1000, 11, 0, 2);
			if (cube_retry_info.retry_offset == cube_retry_info.retry_cfg->retry[retry_arr_idx].len) {
				cube_retry_info.retry_cfg->retry_num--;
				cube_retry_info.retry_arr_idx += 1;
				cube_retry_info.retry_offset = 0;
				LOG_IO(0x1000, 11, 0, 4);
			}
		} else {
			cube_retry_info.retry_cfg->retry_num = 0;
		}
		if (cube_retry_info.retry_cfg->retry_num == 0) {
			cube_access_ctrl(false);
			OSI_Free(cube_retry_info.retry_cfg);
			cube_retry_info.retry_cfg = NULL;
		}
    }
}

void cube_report_retry_handler(HOST_POLL_APP_CUBE *data)
{
	uint32_t retry_num = data->retry_num;
    if (retry_num) {
		/* 1. frame report is not finish
		 * 2. last retry is in progress
		 * 3. new frame is coming */
		if (cube_report_info.aborted || !is_cube_report_frame_finish() || cube_retry_info.retry_cfg ||
			time_after(csi_coret_get_value(), cube_report_info.next_frame_time)) {
			return ;
		}
	    cube_retry_info.retry_block_idx = 0;
	    cube_retry_info.retry_offset = 0;
	    cube_retry_info.retry_arr_idx = 0;

		uint32_t size = sizeof(HOST_POLL_APP_CUBE) + retry_num * sizeof(HOST_CUBE_RETRY);
        cube_retry_info.retry_cfg = (HOST_POLL_APP_CUBE *)OSI_Malloc(size);
        if(cube_retry_info.retry_cfg == NULL) {
            return ;
        }
        memcpy(cube_retry_info.retry_cfg, (uint8_t *)data, sizeof(HOST_POLL_APP_CUBE) + retry_num * sizeof(HOST_CUBE_RETRY));

		uint32_t retry_arr_idx = cube_retry_info.retry_arr_idx;
		uint32_t frame_len = cube_report_info.report_hdr.frame_len;
        cube_access_ctrl(true);
        for(int i = 0; i < CUBE_CACHE_BLOCK; i++) {
            if (retry_num) {
				uint32_t offset = cube_retry_info.retry_cfg->retry[retry_arr_idx].offset + cube_retry_info.retry_offset;
				uint32_t read_num = MIN(CUBE_DATA_SIZE / 4, cube_retry_info.retry_cfg->retry[retry_arr_idx].len);
				if (read_num && (offset + read_num <= frame_len)) {
                	hif_dma_cube_report_retry_send(offset, (cube_retry_info.retry_block_idx++) % CUBE_CACHE_BLOCK);
                	cube_retry_info.retry_offset += read_num;
	                LOG_IO(0x1000, 11, 0, 2);
	                if (cube_retry_info.retry_offset == cube_retry_info.retry_cfg->retry[retry_arr_idx].len) {
	                    retry_num--;
	                    retry_arr_idx += 1;
	                    cube_retry_info.retry_offset = 0;
	                    LOG_IO(0x1000, 11, 0, 4);
	                }
				} else {
					retry_num = 0;
				}
                if (retry_num == 0) {
                    cube_access_ctrl(false);
                    OSI_Free(cube_retry_info.retry_cfg);
                    cube_retry_info.retry_cfg = NULL;
                    break;
                }
            }
        }
		cube_retry_info.retry_cfg->retry_num = retry_num;
		cube_retry_info.retry_arr_idx = retry_arr_idx;
    }
}

void cube_report_callback(hif_queue_item_t *hif_queue_item, int8_t status)
{
	uint32_t time_start;
	SET_TIME_START(time_start);
	cube_report_info.finish_idx++;
	if (!cube_report_info.aborted) {
		/* there are blocks to fill. */
		if (status == 0 && cube_report_info.block_idx < cube_report_info.block_cnt) {
			hif_dma_cube_report_handler((cube_report_info.block_idx++) % CUBE_CACHE_BLOCK);
		} else if (cube_report_info.finish_idx >= cube_report_info.block_idx) {
			/* All cube read done, release waitting of frame callback. */
			OSI_SemaphoreRelease(&cube_report_sem);
		}
	}
	SET_TIME_ESCAPE(time_start, time_start);
	UPDATE_TIME_DBG_INFO(time_start, "cube-bk", g_time_dbg_info.report_block);
}

void mmw_data_cube_spi_report_deinit(void)
{
	cube_report_info.aborted = true;
	mmw_data_report_hif_deinit();
    for (int i = 0; i < CUBE_CACHE_BLOCK; i++) {
        if (cube_cache[i]) {
            OSI_Free(cube_cache[0]);
		    cube_cache[0] = NULL;
        }
    }
	OSI_SemaphoreDelete(&cube_report_sem);
	//memset(&cube_report_info, 0, sizeof(struct cube_report_info_t));
	//HIF_MsgReport_BurstDeinit();
}

extern uint8_t g_dc_suppression_flag;
int mmw_ctrl_data_cube_spi_report_cb(void *mmw_data, void *arg)
{
	int ret = 0;
	/* Skip default 2 frames to wait for clutter coverage */
	if (!s_mmw_radar_analysis_skip_frame_status) { /* s_mmw_radar_analysis_skip_frame_status is 0 indicates that the invalid data frames has skipped */
		uint32_t time_now = csi_coret_get_value();
		cube_report_info.report_hdr.frame_idx = cube_report_info.frame_cb_cnt++;

		/* last frame is not finish */
		if (cube_report_info.finish_idx < cube_report_info.block_idx) {
			/* printf("spi report drop\n") */;
			return ret;
		}
		if (cube_report_info.report_hdr.frame_idx == 0) {
			uint32_t period_ms = 0;
			uint32_t frame_num = 0;
			mmw_frame_get(&period_ms,  &frame_num);
			uint32_t interval_period = 0;
			uint32_t interval_num = 0;;
			mmw_interval_get(&interval_period,  &interval_num);
			interval_period = (interval_period * interval_num) / 1000;
			if(period_ms > interval_period) {
				cube_report_info.report_time_ms = period_ms - interval_period;
			} else {
				return ret; /* no time to report */
			}    
		}

		if (g_dc_suppression_flag) {
			if (MMW_ERR_CODE_SUCCESS != mmw_psic_dc_suppression_update()) {
				printf("condition error!\r\n");
			}
		}

		/* reset report status */
		cube_report_frame_reset();
		cube_access_ctrl(true);
		for(int i = 0; i < CUBE_CACHE_BLOCK; i++) {
			if(cube_report_info.block_idx < cube_report_info.block_cnt) {
				hif_dma_cube_report_handler((cube_report_info.block_idx++) % CUBE_CACHE_BLOCK);
			}
		}
		/* 500us is margin */
		cube_report_info.next_frame_time = time_now + US_TO_CORET(cube_report_info.report_time_ms*1000 - 500);

		ret = OSI_SemaphoreWait(&cube_report_sem, cube_report_info.report_time_ms);
		if(ret != OSI_STATUS_OK) {
			cube_report_info.aborted = true;
		}
		cube_access_ctrl(false);

		SET_TIME_ESCAPE(time_now, time_now);
		UPDATE_TIME_DBG_INFO(time_now, "report", g_time_dbg_info.report);
	} else {
		s_mmw_radar_analysis_skip_frame_status--; /* count skipped frames */
		/* when clutter_rm_method is DC,skip the last frame init DC */
		if (g_dc_suppression_flag && !s_mmw_radar_analysis_skip_frame_status) {
			mmw_clutter_halt_set(MMW_HALT_CLUTTER_UPDATE_ENABLE);
			mmw_psic_dc_suppression_init();
			/* mmw_psic_dc_suppression_init will clear cluuter and needs to be update once */
			mmw_psic_dc_suppression_update();
		}
	}

	return ret;
}


void mmw_report_param_get(void)
{
	int ret = 0;

	D_State_test.proc_frame_num = 0;

	ret = get_tx_rx_num(&cube_report_info.tx_cnt, &cube_report_info.rx_cnt);
	if(ret) {
		printf("mmw mode get fail ! ret = %d \r\n", ret);
		return;
	}
	ret = mmw_fft_num_get(&cube_report_info.range_fft_num, &cube_report_info.doppler_fft_num);
	if(ret) {
		printk("fft num get fail ! ret = %d \r\n", ret);
		return;
	}

	uint32_t frame_len = cube_report_info.tx_cnt * cube_report_info.rx_cnt *
	                     cube_report_info.range_fft_num * cube_report_info.doppler_fft_num;
	cube_report_info.block_cnt = (frame_len * 4 + CUBE_DATA_SIZE - 1) / CUBE_DATA_SIZE;
	cube_report_info.frame_cb_cnt = 0;
	cube_report_info.report_hdr.frame_len = frame_len;
	cube_report_info.report_hdr.frame_idx = 0;
	cube_report_frame_reset();
}


void mmw_data_report_hif_init(uint8_t mode, uint32_t rate, uint8_t type)
{
	int ret = 0;

	HIF_InitParam_t hifInitParam;
	if (mode == HIF_COM_TYPE_SPI) {
#if CONFIG_R3_DATA_BOX_UPLOAD_DATA_CUBE
        if(type) {
            for(uint8_t i = 0; i<CUBE_CACHE_BLOCK; i++) {
                if(cube_cache[i] == NULL) {
                    cube_cache[i] = (uint8_t *)OSI_Malloc(CUBE_SIZE_MAX);
                }
            }

            OSI_SemaphoreCreate(&cube_report_sem, 0, 1);
        }
#endif
		HIF_DefaultInitParam(&hifInitParam, HIF_COM_TYPE_SPI, 8000000);
	} else if (mode == HIF_COM_TYPE_UART) {
#if CONFIG_R3_DATA_BOX_UPLOAD_DATA_CUBE
        if(cube_cache[0] == NULL) {
		    cube_cache[0] = (uint8_t *)OSI_Malloc(CUBE_SIZE_MAX);
	    }
		if (rate == 0) {
			rate = 10000000;
		}
#endif
		HIF_DefaultInitParam(&hifInitParam, HIF_COM_TYPE_UART, rate);
	} else if (mode == HIF_COM_TYPE_IIC) {
		HIF_DefaultInitParam(&hifInitParam, HIF_COM_TYPE_IIC, 100000);
	} else {
		return ;
	}

	ret = HIF_Init(&hifInitParam);

	if(ret) {
		printf("host interface init fail\r\n");
	}

    //HIF_ExtControl(HIF_SET_SIMPLEX_MODE, (void *)&singleMode, sizeof(uint32_t));
#if (CONFIG_MMW_MICRO_POINT_CLOUD)
	if ((D_data_test.micro_points == NULL) && g_micro_point_en) {
		D_data_test.micro_points = OSI_Malloc(sizeof(PointCloud3D)*MICRO_POINT_MAX);
	}
#endif
}

void mmw_data_report_hif_deinit(void)
{
	mmw_ctrl_callback_cfg(NULL, MMW_DATA_TYPE_DISABLE, NULL);
	HIF_DeInit();
#if (CONFIG_MMW_MICRO_POINT_CLOUD)
	if ((D_data_test.micro_points != NULL) && g_micro_point_en) {
		OSI_Free(D_data_test.micro_points);
		D_data_test.micro_points = NULL;
	}
#endif
	if(cube_cache[0]) {
		OSI_Free(cube_cache[0]);
		cube_cache[0] = NULL;
	}

	memset(&cube_report_info, 0, sizeof(struct cube_report_info_t));
}

int mmw_ctrl_data_cube_uart_report_cb(void *mmw_data, void *arg)
{
	int ret = 0;
	
	if (!s_mmw_radar_analysis_skip_frame_status) {
		HIF_MsgHdr_t msg = {0x00};
		uint8_t *cube_data = NULL;
		uint16_t read_num = 0;
		uint16_t offset = CUBE_HEAD_LEN;
		uint16_t read_cnt = 0;
		uint16_t i=0;
		
		uint32_t time_start;
		SET_TIME_START(time_start);

		msg.flag = 1<<2;
		msg.msg_id = 0xC1;
		msg.type = 2;

		cube_data = cube_cache[0];
		memset(cube_data, 0, CUBE_SIZE_MAX);
		if (g_dc_suppression_flag) {
			if (MMW_ERR_CODE_SUCCESS != mmw_psic_dc_suppression_update()) {
				printf("condition error!\r\n");
			}
		}
			
		ret = mmw_motion_cube_access_open();
		if(ret) {
			printk("cube access open fail ! ret = %d \r\n", ret);
			return 0;
		}

		for (i = 0; i < cube_report_info.tx_cnt * cube_report_info.rx_cnt * cube_report_info.doppler_fft_num;) {
			if((CUBE_HEAD_LEN + CUBE_DATA_SIZE - offset) >= 4 * (cube_report_info.range_fft_num - cube_report_info.range_idx)) {
				read_num = cube_report_info.range_fft_num - cube_report_info.range_idx;
			} else {
				read_num = (CUBE_HEAD_LEN + CUBE_DATA_SIZE - offset) / 4;
			}
			__mmw_fft_range((complex16_cube *)&cube_data[offset], cube_report_info.range_idx, read_num, cube_report_info.tx_idx, cube_report_info.rx_idx, cube_report_info.dop_idx);

			offset = offset + (4*read_num);
			cube_report_info.range_idx += read_num;
			if(cube_report_info.range_idx >= cube_report_info.range_fft_num) {
				cube_report_info.range_idx = 0;
				cube_report_info.dop_idx++;
				i++;
				if(cube_report_info.dop_idx == cube_report_info.doppler_fft_num) {
					cube_report_info.dop_idx = 0;
					cube_report_info.rx_idx ++;
					if(cube_report_info.rx_idx == cube_report_info.rx_cnt) {
						cube_report_info.rx_idx = 0;
						cube_report_info.tx_idx ++;
						if(cube_report_info.tx_idx == cube_report_info.tx_cnt) {
							cube_report_info.tx_idx = 0;
						}
					}
				}
			}
			read_cnt = (offset - CUBE_HEAD_LEN) / 4;
			if(offset >= (CUBE_HEAD_LEN + CUBE_DATA_SIZE) || (cube_report_info.report_hdr.frame_len - cube_report_info.report_hdr.offset - read_cnt) == 0) {
				msg.length = offset;
				memcpy(cube_data, &cube_report_info.report_hdr, sizeof(MMW_FRAME_UPLOAD));
				HIF_MsgReport(&msg, cube_data, NULL);
				if((cube_report_info.report_hdr.frame_len - cube_report_info.report_hdr.offset - read_cnt) == 0) {
					break;
				}
				cube_report_info.report_hdr.offset += read_cnt;
				offset = CUBE_HEAD_LEN;
			}
		}


		cube_report_info.report_hdr.frame_idx ++;
		cube_report_info.report_hdr.offset = 0;
		cube_report_info.dop_idx = 0;
		cube_report_info.range_idx = 0;
		cube_report_info.tx_idx = 0;
		cube_report_info.rx_idx = 0;

		ret = mmw_motion_cube_access_close();
		if(ret) {
			printk("cube access open fail ! ret = %d \r\n", ret);
		}
		SET_TIME_ESCAPE(time_start, time_start);
		UPDATE_TIME_DBG_INFO(time_start, "report", g_time_dbg_info.report);
	} else {
		s_mmw_radar_analysis_skip_frame_status--; /* count skipped frames */
		/* when clutter_rm_method is DC,skip the last frame init DC */
		if (g_dc_suppression_flag && !s_mmw_radar_analysis_skip_frame_status) {
			mmw_clutter_halt_set(MMW_HALT_CLUTTER_UPDATE_ENABLE);
			mmw_psic_dc_suppression_init();
			/* mmw_psic_dc_suppression_init will clear cluuter and needs to be update once */
			mmw_psic_dc_suppression_update();
		}
	}
	
	return 0;
}

void send_obj_data_Callback(hif_queue_item_t *hif_queue_item, int8_t status)
{
}
static void mmw_msg_send_obj_data(MMW_FRAME_UPLOAD *report_hdr, TL_HEADER *tl, uint8_t *payload, uint32_t len)
{
	HIF_MsgHdr_t msg = {0x00};
	msg.flag = 1<<2;
	msg.msg_id = 0xC3;
	msg.type = 2;
	msg.length = 0;

    payload_node_t *payload_head = NULL;

    payload_head = HIF_MsgReport_ListAddData(payload_head, report_hdr, sizeof(MMW_FRAME_UPLOAD));
	if (tl != NULL) {
		payload_head = HIF_MsgReport_ListAddData(payload_head, tl, sizeof(TL_HEADER));
	}
	if (len) {
		payload_head = HIF_MsgReport_ListAddData(payload_head, payload, len);
	}

    HIF_MsgReport_ListStart(&msg, payload_head, NULL);

    if (tl != NULL) {
        report_hdr->offset += (sizeof(TL_HEADER)>>2);
    }
    if(len) {
        report_hdr->offset += (len>>2);
    }
}

void mmw_pointcloud_upload(MMW_FRAME_UPLOAD *frame_hdr, TL_HEADER *tl, uint8_t *payload, uint32_t payload_len)
{
	do {
	 	uint32_t packet_len = MIN(payload_len, POINT_DATA_SIZE);
		payload_len -= packet_len;
		mmw_msg_send_obj_data(frame_hdr, tl, payload, packet_len);
		payload   += packet_len;
		tl = NULL;
	} while (payload_len > 0);
}

void tracker_targets_upload(MMW_FRAME_UPLOAD *frame_hdr, ClusterTrackElm *buff, uint32_t total_num)
{
	TL_HEADER tl;
	TL_HEADER *tl_ptr = &tl;
	tl.type   = 5;
	tl.flag   = 1;
	tl.length = total_num*sizeof(ClusterTrackElm);
	do {
		uint32_t packet_num = MIN(total_num, POINT_DATA_SIZE/sizeof(ClusterTrackElm));
		total_num -= packet_num;
		mmw_msg_send_obj_data(frame_hdr, tl_ptr, (uint8_t *)buff, packet_num*sizeof(ClusterTrackElm));
		buff += packet_num;
		tl_ptr = NULL;
		OSI_MSleep(1);
	} while (total_num > 0);
}

extern uint8_t  gui_data_report;
int mmw_detection3d_data_report(Detection3D_Data *data, Detection3D_State *state, void *arg)
{
	MMW_FRAME_UPLOAD frame_hdr;
	bool upload_point   = false;
	#if (CONFIG_MMW_APP_TRACK_ENABLE)
	bool upload_targets = false;
	#endif
	uint32_t frame_num  = state->proc_frame_num;

	switch (gui_data_report) {
	case GUI_DATA_TYPE_POINTS:
		upload_point = true;
		break;
	case GUI_DATA_TYPE_TRACKING:
	#if (CONFIG_MMW_APP_TRACK_ENABLE)
		upload_targets = true;
	#endif
		break;
	case GUI_DATA_TYPE_POINTS_TRACKING:
		upload_point   = true;
	#if (CONFIG_MMW_APP_TRACK_ENABLE)
		upload_targets = true;
	#endif
		break;
	default:
		return 0;
	}

	/* Setup frame header */
	frame_hdr.frame_idx = frame_num;
	frame_hdr.frame_len = 0;
	frame_hdr.offset	 = 0;
#if 1 /* (CONFIG_MMW_APP_POINT_CLOUD_ENABLE) */
	if (upload_point) {
		frame_hdr.frame_len += ((sizeof(PointCloud_Polar)*data->motion_points_num  + sizeof(TL_HEADER))>>2);
	}
#endif

#if (CONFIG_MMW_MICRO_POINT_CLOUD)
	if (upload_point && g_micro_point_en) {
		frame_hdr.frame_len += ((sizeof(PointCloud_Polar)*data->micro_points_num  + sizeof(TL_HEADER))>>2);
	}
#endif
#if (CONFIG_MMW_APP_TRACK_ENABLE)
	if (upload_targets) {
		frame_hdr.frame_len += ((sizeof(ClusterTrackElm)*data->tracker_objs_num  + sizeof(TL_HEADER))>>2);
	}
#endif

	/* Output data packets */
#if 1 /*CONFIG_MMW_APP_POINT_CLOUD_ENABLE) */
	if (upload_point) { /* motion points packets */
		TL_HEADER tl;
		uint32_t total_len = sizeof(PointCloud_Polar) * data->motion_points_num;
		tl.type    = MMW_TL_TYPE_POINTS;
		tl.flag    = MMW_TL_FLAG_CART;
		tl.length = total_len;
		mmw_pointcloud_upload(&frame_hdr, &tl, (uint8_t *)data->motion_points, tl.length);
	}
#endif

#if (CONFIG_MMW_MICRO_POINT_CLOUD)
	if (upload_point && g_micro_point_en) {
		TL_HEADER tl;
		tl.type    = MMW_TL_TYPE_POINTS;
		tl.flag    = mmw_coordinate_get()|MMW_TL_FLAG_MICRO;
		tl.length  = sizeof(PointCloud_Polar) * data->micro_points_num;
		/* output micro points packets */
		mmw_pointcloud_upload(&frame_hdr, &tl, (uint8_t *)data->micro_points, tl.length);
	}
#endif

#if (CONFIG_MMW_APP_TRACK_ENABLE)
	if (upload_targets) {
		/* tracker targes packets */
		tracker_targets_upload(&frame_hdr, data->tracker_objs, data->tracker_objs_num);
	}
#endif
	(void)frame_hdr;
	return 0;
}

#if (CONFIG_MMW_MICRO_POINT_CLOUD)

typedef struct micro_report_arg {
	PointCloud3D *buffer;
	uint32_t  point_num;
	int32_t   micro_range_unit;  //cm-q4
	int32_t   micro_veloc_unit;  //cm/s_q7
	uint8_t   coordinate_cart;
	uint8_t   breath_target_valid;
	uint16_t  breath_target_range_idx;
	uint32_t  breath_target_snr;
} MICRO_REPORT_ARG;
MICRO_REPORT_ARG g_micro_arg;

typedef struct sleep_breath_phase_sample {
	bool valid;
	uint8_t antenna_idx;
	uint16_t range_idx;
	int16_t real;
	int16_t imag;
	int16_t antenna_real[MMW_ANT_CHANNEL_NUM];
	int16_t antenna_imag[MMW_ANT_CHANNEL_NUM];
	int32_t phase_mrad;
	uint32_t amplitude;
} SLEEP_BREATH_PHASE_SAMPLE;

typedef struct sleep_heart_raw_sample {
	bool valid;
	uint8_t center_slot;
	uint8_t bin_count;
	uint16_t center_range_idx;
	uint16_t range_idx[SLEEP_HEART_BIN_COUNT];
	complex16_cube antenna_iq[SLEEP_HEART_BIN_COUNT][MMW_ANT_CHANNEL_NUM];
} SLEEP_HEART_RAW_SAMPLE;

static uint32_t sleep_complex_power(const complex16_cube *value)
{
	int32_t real = value->real;
	int32_t imag = value->imag;
	return (uint32_t)(real * real + imag * imag);
}

static SLEEP_BREATH_PHASE_SAMPLE sleep_breath_phase_sample_get(uint16_t range_idx)
{
	SLEEP_BREATH_PHASE_SAMPLE sample = {0};
	complex16_cube *chirps = micro_cube_chirps_get(range_idx);
	uint16_t chirp_num = micro_cube_chirp_num_get();
	uint32_t best_power = 0;
	complex16_cube best = {0};

	sample.range_idx = range_idx;
	if (chirps == NULL || chirp_num == 0) {
		return sample;
	}

	for (uint32_t ant_idx = 0; ant_idx < MMW_ANT_CHANNEL_NUM; ++ant_idx) {
		complex16_cube cur = chirps[ant_idx * chirp_num + chirp_num - 1];
		uint32_t power = sleep_complex_power(&cur);
		sample.antenna_real[ant_idx] = cur.real;
		sample.antenna_imag[ant_idx] = cur.imag;
		if (power > best_power) {
			best_power = power;
			best = cur;
			sample.antenna_idx = (uint8_t)ant_idx;
		}
	}

	if (best_power == 0) {
		return sample;
	}

	sample.valid = true;
	sample.real = best.real;
	sample.imag = best.imag;
	sample.amplitude = best_power;
	sample.phase_mrad = (int32_t)(atan2f((float)best.imag, (float)best.real) * 1000.0f);
	return sample;
}

static bool sleep_heart_capture_read(void *context,uint16_t range_idx,int16_t iq[6])
{
	uint16_t chirps=*(const uint16_t *)context;
	uint16_t first=0,end=0;
	complex16_cube *source;
	if (micro_cube_range_get(&first,&end)!=MMW_ERR_CODE_SUCCESS || range_idx<first || range_idx>=end ||
		micro_cube_chirp_num_get()!=chirps) return false;
	source=micro_cube_chirps_get(range_idx);
	if (source==NULL || chirps==0) return false;
	for (uint32_t ant=0;ant<3;ant++) {
		iq[ant*2]=source[ant*chirps+chirps-1].real;
		iq[ant*2+1]=source[ant*chirps+chirps-1].imag;
	}
	return true;
}

#if !HEART_FAST_PROFILE
static void sleep_heart_capture_report(uint32_t frame_idx,uint16_t selected)
{
	static char line[HR_IQ_CAPTURE_BUFFER_BYTES];
	static uint32_t last_error_ms;
	const SleepDetectResult *status=sleep_detect_get_result();
	HeartRateDeviceResult device;
	uint16_t range_first=0,range_end=0,first,end,chirps=micro_cube_chirp_num_get();
	uint8_t flags=1u|(status->in_bed?2u:0u)|(status->is_active?4u:0u);
	size_t length=0;
	heart_rate_device_get(&device);
	/* The SDK accessor itself does not check the upper range bound. Respect
	 * its [range_first,range_end) contract BEFORE reading any bin. */
	if (micro_cube_range_get(&range_first,&range_end)==MMW_ERR_CODE_SUCCESS && range_end>range_first && chirps>0) {
		first=range_first>4u?range_first:4u;
		end=range_end<28u?range_end:28u;
		if (first>=end) { first=range_first;end=(uint16_t)MIN((uint32_t)range_end,(uint32_t)first+HR_IQ_CAPTURE_MAX_BINS); }
		length=hr_iq_capture_encode(line,sizeof(line),device.generation,frame_idx,device.report_ms,
			flags,first,(uint16_t)(end-first),range_first,range_end,selected,sleep_heart_capture_read,&chirps);
	}
	if (length>0) printf("%s",line);
	else if ((uint32_t)(device.report_ms-last_error_ms)>=5000u) {
		last_error_ms=device.report_ms;
		printf("HRIQERR V=1 GEN=%lu F=%lu R0=%u R1=%u CHIRPS=%u\r\n",
			(unsigned long)device.generation,(unsigned long)frame_idx,range_first,range_end,chirps);
	}
}
#endif

#include "rsp_result.h"
static void sleep_heart_device_feed(uint32_t frame_idx,const SLEEP_BREATH_PHASE_SAMPLE *sample)
{
#if CONFIG_SLEEP_HEART_DEVICE
	s_fast_input_valid=sample->valid;s_fast_input_bin=sample->range_idx; /* diagnostic copy only */
	const SleepDetectResult *result=sleep_detect_get_result();
	int16_t iq[6];
	for (uint32_t ant=0;ant<3;ant++) {
		iq[ant*2]=sample->antenna_real[ant];iq[ant*2+1]=sample->antenna_imag[ant];
	}
	heart_rate_device_submit(frame_idx,sample->range_idx,sample->valid,result->is_active,
	                         result->in_bed,s_sleep_heart_breath_reference,iq);
	heart_rate_device_emit_pending();
	rsp_result_runtime_update();
	/* Same cached micro-cube / last-chirp source as the original HRIN, read
	 * immediately into a checked line. Formal HR input was already copied. */
#if !HEART_FAST_PROFILE
	sleep_heart_capture_report(frame_idx,sample->range_idx);
#endif
#else
	(void)frame_idx;(void)sample;
#endif
}

#if SLEEP_HEART_SAFE_REPORT_ENABLE
static void sleep_heart_safe_report(uint32_t frame_idx,
						 const SLEEP_BREATH_PHASE_SAMPLE *sample)
{
	const SleepDetectResult *result = sleep_detect_get_result();

	if (!result->in_bed || !sample->valid) {
		return;
	}

	HeartRawRecord row;int length;
	_Static_assert(MMW_ANT_CHANNEL_NUM==3u,"Legacy HRRAW requires the configured RS6130 1T3R");
	row.frame=frame_idx;row.person=result->has_person;row.active=result->is_active;row.bed=result->in_bed;
	row.bin=sample->range_idx;row.antenna=sample->antenna_idx;row.real=sample->real;row.imag=sample->imag;
	row.amplitude=sample->amplitude;row.phase=sample->phase_mrad;row.delta=result->breath_phase_delta_mrad;
	for(unsigned a=0;a<3u;a++) {row.iq[2u*a]=sample->antenna_real[a];row.iq[2u*a+1u]=sample->antenna_imag[a];}
	length=hrraw_encode(s_fast_line,sizeof(s_fast_line),&row);
	if(length>0)sleep_trace_record(s_fast_line,(size_t)length);
}
#endif

static SLEEP_HEART_RAW_SAMPLE sleep_heart_raw_sample_get(uint16_t center_range_idx)
{
	SLEEP_HEART_RAW_SAMPLE sample = {0};
	uint16_t range_fft_num = 0;
	uint16_t doppler_fft_num = 0;
	uint16_t bin_count;
	uint16_t start_range_idx;

	mmw_fft_num_get(&range_fft_num, &doppler_fft_num);
	if (range_fft_num == 0 || doppler_fft_num == 0 || center_range_idx >= range_fft_num) {
		return sample;
	}

	bin_count = MIN((uint16_t)SLEEP_HEART_BIN_COUNT, range_fft_num);
	start_range_idx = center_range_idx > SLEEP_HEART_BIN_RADIUS ?
		(uint16_t)(center_range_idx - SLEEP_HEART_BIN_RADIUS) : 0;
	if ((uint32_t)start_range_idx + bin_count > range_fft_num) {
		start_range_idx = range_fft_num - bin_count;
	}

	if (mmw_motion_cube_access_open() != MMW_ERR_CODE_SUCCESS) {
		return sample;
	}
	for (uint32_t bin_slot = 0; bin_slot < bin_count; ++bin_slot) {
		uint16_t range_idx = start_range_idx + bin_slot;
		sample.range_idx[bin_slot] = range_idx;
		__mmw_fft_ants(sample.antenna_iq[bin_slot], doppler_fft_num >> 1, range_idx);
	}
	mmw_motion_cube_access_close();

	sample.valid = true;
	sample.bin_count = (uint8_t)bin_count;
	sample.center_range_idx = center_range_idx;
	sample.center_slot = (uint8_t)(center_range_idx - start_range_idx);
	return sample;
}

static void sleep_heart_raw_report(uint32_t frame_idx,
						   const SLEEP_HEART_RAW_SAMPLE *sample)
{
	const SleepDetectResult *result = sleep_detect_get_result();
	const complex16_cube *center_iq;
	uint8_t best_antenna_idx = 0;
	uint32_t best_power = 0;
	int32_t phase_mrad;

	if (!result->in_bed || !sample->valid || sample->center_slot >= sample->bin_count) {
		return;
	}

	center_iq = sample->antenna_iq[sample->center_slot];
	for (uint32_t ant_idx = 0; ant_idx < MMW_ANT_CHANNEL_NUM; ++ant_idx) {
		uint32_t power = sleep_complex_power(&center_iq[ant_idx]);
		if (power > best_power) {
			best_power = power;
			best_antenna_idx = (uint8_t)ant_idx;
		}
	}
	phase_mrad = (int32_t)(atan2f((float)center_iq[best_antenna_idx].imag,
								  (float)center_iq[best_antenna_idx].real) * 1000.0f);

	printf("HRRAW F=%lu FS=5 V=1 P=%u A=%u BED=%u BIN=%u ANT=%u I=%d Q=%d AMP=%lu PH=%ld DPH=%d",
	       (unsigned long)frame_idx,
	       result->has_person,
	       result->is_active,
	       result->in_bed,
	       sample->center_range_idx,
	       best_antenna_idx,
	       center_iq[best_antenna_idx].real,
	       center_iq[best_antenna_idx].imag,
	       (unsigned long)best_power,
	       (long)phase_mrad,
	       result->breath_phase_delta_mrad);
	for (uint32_t ant_idx = 0; ant_idx < MMW_ANT_CHANNEL_NUM; ++ant_idx) {
		printf(" I%lu=%d Q%lu=%d",
		       (unsigned long)ant_idx,
		       center_iq[ant_idx].real,
		       (unsigned long)ant_idx,
		       center_iq[ant_idx].imag);
	}
	printf(" N=%u", sample->bin_count);
	for (uint32_t bin_slot = 0; bin_slot < sample->bin_count; ++bin_slot) {
		printf(" B%lu=%u", (unsigned long)bin_slot, sample->range_idx[bin_slot]);
		for (uint32_t ant_idx = 0; ant_idx < MMW_ANT_CHANNEL_NUM; ++ant_idx) {
			printf(" B%luI%lu=%d B%luQ%lu=%d",
			       (unsigned long)bin_slot,
			       (unsigned long)ant_idx,
			       sample->antenna_iq[bin_slot][ant_idx].real,
			       (unsigned long)bin_slot,
			       (unsigned long)ant_idx,
			       sample->antenna_iq[bin_slot][ant_idx].imag);
		}
	}
	printf("\r\n");
}

static uint16_t mpc_snr_trans_db(uint32_t snr_lin) //10*log10
{
	uint16_t snr_db = 0;
	while (snr_lin >= 4) {
		snr_db += 3;
		snr_lin = snr_lin>>1;
	}
	return snr_db + (snr_lin == 3 ? 5 : (snr_lin == 2 ? 3 : 0));
}

int mmw_micro_point_handle(uint32_t range_idx, int veloc_idx,
					uint32_t snr_linear, MmwAngleInfo_t *angle, void *arg)
{
	MICRO_REPORT_ARG *report = (MICRO_REPORT_ARG *)arg;
	int32_t range_cm_q4 = range_idx * report->micro_range_unit;
	if (!report->coordinate_cart) {
		PointCloud_Polar *polar = (PointCloud_Polar *)report->buffer;
		polar[report->point_num].range = (range_cm_q4)>>4;
		polar[report->point_num].azim	= (angle->azimuth*100)>>7;
		polar[report->point_num].elev	= (angle->elevation*100)>>7;
	} else {
		PointCloud_Cart *cart = (PointCloud_Cart *)report->buffer;
		int32_t range_cmq1 = (range_cm_q4>>3);
        /* Adjust mount position in dev-board, change radar coordinate to user coordinate */
		mmw_micro_frame_trans_radar_coord_to_user_coord(
							(range_cmq1 * angle->sinValue_X) >> 16,
							(range_cmq1 * angle->sinValue_Y) >> 16,
							(range_cmq1 * angle->sinValue_Z) >> 16,
							&cart[report->point_num]
							);
	}
	report->buffer[report->point_num].polar.vel = (veloc_idx * report->micro_veloc_unit)>>7;
	report->buffer[report->point_num].polar.snr = mpc_snr_trans_db(snr_linear) * 100;
	if (report->coordinate_cart &&
	    sleep_detect_is_point_in_bed_zone(&report->buffer[report->point_num]) &&
	    snr_linear >= report->breath_target_snr) {
		report->breath_target_valid = 1;
		report->breath_target_range_idx = (uint16_t)range_idx;
		report->breath_target_snr = snr_linear;
	}
	return (++report->point_num >= MICRO_POINT_MAX);
}
#endif

/*
 * Diagnostic reflection position, not a person classification or a tracked target.
 * Called only at the existing SLEEP report cadence, independently of P/BED.
 * Prefer micro points for a stationary body; use motion only without micro data.
 * All signed user-coordinate values are accepted. Never modify the input points.
 * Score at most 16 evenly spaced seeds against the first 256 source points:
 * O(16 * min(source_n, 256)), at most 4096 point/seed comparisons per report.
 * Average only the winning seed's 45 cm spherical neighborhood, not the cloud.
 * N counts that neighborhood within the scanned prefix, not all source points.
 * Scalar locals only (well below 1 KB); no allocation or persistent target state.
 */
static void sleep_target_diagnostic_report(const Detection3D_Data *data)
{
	enum { MAX_POINTS = 256, MAX_SEEDS = 16, RADIUS_CM = 45 };
	const PointCloud3D *points = data->micro_points;
	const char *source = "MICRO";
	uint16_t source_n = data->micro_points_num;
	uint16_t scan_n, seed_n;
	uint16_t best_count = 0;
	int32_t best_snr = 0;
	int32_t best_x = 0, best_y = 0, best_z = 0;

	if (points == NULL || source_n == 0) {
		points = data->motion_points;
		source_n = data->motion_points_num;
		source = "MOTION";
	}
	if (points == NULL || source_n == 0) {
		printf("TARGET V=0\r\n");
		return;
	}

	scan_n = MIN(source_n, MAX_POINTS);
	seed_n = MIN(scan_n, MAX_SEEDS);
	for (uint16_t seed = 0; seed < seed_n; ++seed) {
		uint16_t seed_idx = seed_n > 1 ?
			(uint16_t)((uint32_t)seed * (scan_n - 1U) / (seed_n - 1U)) : 0;
		const PointCloud_Cart *center = &points[seed_idx].cart;
		uint16_t count = 0;
		int32_t snr = 0;
		int32_t sum_x = 0, sum_y = 0, sum_z = 0;

		for (uint16_t i = 0; i < scan_n; ++i) {
			const PointCloud_Cart *point = &points[i].cart;
			int32_t dx = (int32_t)point->x - center->x;
			int32_t dy = (int32_t)point->y - center->y;
			int32_t dz = (int32_t)point->z - center->z;

			/* Bound each delta before squaring: safe over the full int16 range. */
			if (dx < -RADIUS_CM || dx > RADIUS_CM ||
			    dy < -RADIUS_CM || dy > RADIUS_CM ||
			    dz < -RADIUS_CM || dz > RADIUS_CM) {
				continue;
			}
			if (dx * dx + dy * dy + dz * dz > RADIUS_CM * RADIUS_CM) {
				continue;
			}
			++count;
			snr += point->snr;
			sum_x += point->x;
			sum_y += point->y;
			sum_z += point->z;
		}
		/* Equal support: prefer higher summed SNR; exact ties keep the first seed. */
		if (count > best_count || (count == best_count && snr > best_snr)) {
			best_count = count;
			best_snr = snr;
			best_x = sum_x;
			best_y = sum_y;
			best_z = sum_z;
		}
	}

	/* Every seed supports itself, so best_count is nonzero for any source data. */
	printf("TARGET V=1 X=%d Y=%d Z=%d N=%u SRC=%s\r\n",
	       (int)(best_x / best_count),
	       (int)(best_y / best_count),
	       (int)(best_z / best_count),
	       (unsigned int)best_count, source);
}

#if HEART_FAST_PROFILE
/* The vendor helper keeps the same gain normalization as the micro cache.
 * It needs the micro-cube antenna setup, so fast reads wait for cube_ready. */
extern void mmw_fft_ants_auto_gain(complex16_cube *buffer,uint16_t interval_id,uint16_t range_idx);
static HfFrontend s_fast_front;
static uint32_t s_fast_rf,s_fast_gen,s_fast_reads,s_fast_errors,s_fast_max_ms,s_fast_last_warning;
static uint16_t s_fast_first=13u,s_fast_mask;
static HeartTargetWindow s_fast_target;
static volatile bool s_fast_reset_pending=true;
/* Owned only by the data callback. Keep acquisition scratch off its stack. */
static struct {
    HeartRateDeviceResult device;
    complex16_cube ants[MMW_ANT_CHANNEL_NUM];
    int16_t iq[18];int32_t phase[9];uint32_t meta[14];
} s_fast_sample;
static struct {
    uint32_t read,filter,encode,enqueue,slow,callback,overruns;
} s_fast_cost;
static uint32_t sleep_fast_now(void) {
	return (uint32_t)(((uint64_t)xTaskGetTickCount()*1000u)/configTICK_RATE_HZ);
}
static __xip_text uint32_t sleep_fast_hw_hi(void) {return csi_coret_get_valueh();}
static __xip_text uint32_t sleep_fast_hw_lo(void) {return (uint32_t)csi_coret_get_value();}
static void sleep_fast_reset(void) {
	/* Shell requests a reset; only the data callback owns filter memory. */
	taskENTER_CRITICAL();s_fast_reset_pending=true;taskEXIT_CRITICAL();
}
static void sleep_fast_apply_reset(void) {
	bool pending;
	taskENTER_CRITICAL();pending=s_fast_reset_pending;s_fast_reset_pending=false;taskEXIT_CRITICAL();
	if(!pending)return;
	hf_reset(&s_fast_front);s_fast_rf=0;s_fast_gen=0;s_fast_reads=0;s_fast_errors=0;
	s_fast_max_ms=0;s_fast_last_warning=0;s_fast_first=13u;s_fast_mask=0;
	ht_init(&s_fast_target);s_fast_input_valid=false;s_fast_input_bin=0u;
	memset(&s_fast_cost,0,sizeof(s_fast_cost));
}
static void sleep_fast_warning(unsigned code,uint32_t value) {
	uint32_t now=sleep_fast_now();s_fast_errors++;
	if(s_fast_errors==1u || now-s_fast_last_warning>=5000u) {
		s_fast_last_warning=now;
		printf("HFFWAIT V=1 CODE=%u VALUE=%lu RF=%lu\r\n",code,(unsigned long)value,(unsigned long)s_fast_rf);
	}
}
static void sleep_fast_capture(void) {
	const SleepDetectResult *result=sleep_detect_get_result();
	HeartRateDeviceResult *device=&s_fast_sample.device;
	int16_t *iq=s_fast_sample.iq;int32_t *phase=s_fast_sample.phase;uint32_t *meta=s_fast_sample.meta;
	uint16_t ranges=0,dopplers=0;uint32_t start=sleep_fast_now(),sample_ms=start,stage,elapsed;
	bool auto_gain,any=false;unsigned b,a;int length;
	if(!micro_cube_ready()) {sleep_fast_warning(1u,0u);return;}
	mmw_fft_num_get(&ranges,&dopplers);
	if(ranges<3u || !dopplers) {sleep_fast_warning(2u,ranges);return;}
	s_fast_first=s_fast_target.first;
	if((uint32_t)s_fast_first+3u>ranges)s_fast_first=(uint16_t)(ranges-3u);
	auto_gain=mmw_fft_autogain_get()!=0u;
	if(mmw_motion_cube_access_open()!=MMW_ERR_CODE_SUCCESS) {sleep_fast_warning(3u,0u);return;}
	stage=sleep_fast_now();
	for(b=0;b<3u;b++) {
		complex16_cube *ants=s_fast_sample.ants;
		if(auto_gain)mmw_fft_ants_auto_gain(ants,dopplers>>1,(uint16_t)(s_fast_first+b));
		else __mmw_fft_ants(ants,dopplers>>1,(uint16_t)(s_fast_first+b));
		for(a=0;a<3u;a++) {
			iq[(b*3u+a)*2u]=ants[a].real;iq[(b*3u+a)*2u+1u]=ants[a].imag;
			if(ants[a].real || ants[a].imag)any=true;
		}
	}
	mmw_motion_cube_access_close();elapsed=sleep_fast_now()-stage;
	if(elapsed>s_fast_cost.read)s_fast_cost.read=elapsed;
	heart_rate_device_get(device);
	if(s_fast_gen!=device->generation) {hf_reset(&s_fast_front);s_fast_gen=device->generation;}
	stage=sleep_fast_now();s_fast_mask=hf_push(&s_fast_front,sample_ms,s_fast_first,iq,phase);
	if(!s_fast_target.locked) {s_fast_mask=0u;memset(phase,0,sizeof(s_fast_sample.phase));}
	elapsed=sleep_fast_now()-stage;if(elapsed>s_fast_cost.filter)s_fast_cost.filter=elapsed;
	meta[0]=1u;meta[1]=device->generation;meta[2]=s_fast_rf;meta[3]=D_State_test.proc_frame_num;meta[4]=sample_ms;
	meta[5]=auto_gain?2u:1u;meta[6]=20u;meta[7]=5u;
	meta[8]=(any?1u:0u)|(result->in_bed?2u:0u)|(result->is_active?4u:0u);
	meta[9]=s_fast_first;meta[10]=3u;meta[11]=hf_slow_due(s_fast_rf)?1u:0u;meta[12]=s_fast_mask;meta[13]=s_fast_front.gaps;
	stage=sleep_fast_now();length=hff_encode(s_fast_line,sizeof(s_fast_line),meta,iq,phase);
	elapsed=sleep_fast_now()-stage;if(elapsed>s_fast_cost.encode)s_fast_cost.encode=elapsed;
	stage=sleep_fast_now();
	if(length>0 && sleep_trace_record(s_fast_line,(size_t)length)==length)s_fast_reads++;
	else sleep_fast_warning(4u,0u);
	elapsed=sleep_fast_now()-stage;if(elapsed>s_fast_cost.enqueue)s_fast_cost.enqueue=elapsed;
	start=sleep_fast_now()-start;if(start>s_fast_max_ms)s_fast_max_ms=start;
}
#endif

/* The callback function when 2D-FFT finished in point cloud application */
static int sleep_slow_point_cb(void *mmw_data, void *arg)
{
	if (!s_sleep_report_cb_seen) {
		s_sleep_report_cb_seen = true;
		printf("SLEEP_CB_ENTER skip=%u\r\n", s_mmw_radar_analysis_skip_frame_status);
	}
	/* Skip default 2 frames to wait for clutter coverage */
	if (!s_mmw_radar_analysis_skip_frame_status) { /* s_mmw_point_cloud_skip_frame_status is 0 indicates that the invalid data frames has skipped */
		uint32_t time_start, time_motion = 0, time_micro = 0, time_report = 0;
		SLEEP_BREATH_PHASE_SAMPLE breath_phase_sample = {0};
	#if SLEEP_HEART_RAW_ENABLE
		SLEEP_HEART_RAW_SAMPLE heart_raw_sample = {0};
	#endif
		bool sleep_process_frame =
			(D_State_test.proc_frame_num % SLEEP_PROCESS_FRAME_DIV) == 0;

		float sin_y, sin_z, sin_x; /* x, y, z, vel, */

		uint32_t range_mm, range_reol_mm, velocity_mm, veloc_resol_mm;
		uint16_t range_fft_num, doppler_fft_num;
		float range_bin_size_cm, dop_bin_size_cm;
		int16_t doppler_idx_half;
		float user_coordinate[3];
		mmw_range_get(&range_mm, &range_reol_mm);
		mmw_fft_num_get(&range_fft_num, &doppler_fft_num);

		SET_TIME_START(time_start);

		/*--------------------- MOTION POINT PROCESS --------------------- */
		PointCloudBuffer_t *ptr_point_cloud_buffer;
		if (!s_sleep_process_seen) {
			s_sleep_process_seen = true;
			sleep_detect_reset();
			printf("SLEEP_CB_START\r\n");
		}
		if (g_cfar_method == 0) {
			ptr_point_cloud_buffer = mmw_point_cloud_process();
		} else if (g_cfar_method == 1) {
			ptr_point_cloud_buffer = mmw_point_cloud_process_sw_cfar();
		} else {
			ptr_point_cloud_buffer = 0;
		}

		SET_TIME_ESCAPE(time_motion, time_start);
		UPDATE_TIME_DBG_INFO(time_motion, "motion", g_time_dbg_info.motion_point);
		SET_TIME_START(time_start);

		/*---------------------  MICRO MOTION POINT PROCESS --------------------- */
		#if (CONFIG_MMW_MICRO_POINT_CLOUD)
		if (g_micro_point_en) {
			if (D_State_test.proc_frame_num == 0) {
				/* init parameters on first frame */
				uint32_t frame_period, dummy;
				uint32_t micro_dop_num = mmw_micro_doppler_num_get();
				if (mmw_micro_frame_rate_get() != SLEEP_PROCESS_FRAME_DIV) {
					mmw_micro_frame_rate_set(SLEEP_PROCESS_FRAME_DIV);
				}
				mmw_frame_get(&frame_period, &dummy);
				frame_period = frame_period * mmw_micro_frame_rate_get();
#if HEART_FAST_PROFILE
				frame_period *= HEART_SLOW_DIV;
#endif
				g_micro_arg.coordinate_cart = mmw_coordinate_get();
				g_micro_arg.micro_range_unit = (range_mm<<4)/10/range_fft_num;
				g_micro_arg.micro_veloc_unit = (5*100<<7)/(frame_period * micro_dop_num);
				g_micro_arg.buffer = D_data_test.micro_points;
			}
			bool micro_update = mmw_micro_point_frame();
			/* auto gain function must be cleared manually as soon as gain factor is not used,
			   to make auto gain in next frame works well.
			   Hardware reuired gain factor cleared before chirp transmit period
			*/
			mmw_psic_auto_gain_clear();
			if (micro_update) {
				if (s_sleep_breath_target_valid) {
					breath_phase_sample = sleep_breath_phase_sample_get(s_sleep_breath_target_range_idx);
				}
				g_micro_arg.point_num = 0;
				g_micro_arg.breath_target_valid = 0;
				g_micro_arg.breath_target_range_idx = 0;
				g_micro_arg.breath_target_snr = 0;
				mmw_micro_point_process(&mmw_micro_point_handle, &g_micro_arg);
				D_data_test.micro_points_num = g_micro_arg.point_num;
				s_sleep_breath_target_valid = g_micro_arg.breath_target_valid ? true : false;
				s_sleep_breath_target_range_idx = g_micro_arg.breath_target_range_idx;
				if (g_micro_arg.breath_target_valid) {
					s_sleep_heart_target_valid = true;
					s_sleep_heart_target_range_idx = g_micro_arg.breath_target_range_idx;
				}
			}
		} else {
			D_data_test.micro_points_num = 0;
			/* auto gain function must be cleared manually as soon as gain factor is not used,
			   to make auto gain in next frame works well.
			   Hardware reuired gain factor cleared before chirp transmit period
			*/
			mmw_psic_auto_gain_clear();
		}
		ADD_TIME_ESCAPE(time_micro, time_start);
		UPDATE_TIME_DBG_INFO(time_micro, "micro", g_time_dbg_info.micro_point);
		SET_TIME_START(time_start);
	#else
		D_data_test.micro_points_num = 0;
		(void)time_micro;
		/* auto gain function must be cleared manually as soon as gain factor is not used,
		   to make auto gain in next frame works well.
		   Hardware reuired gain factor cleared before chirp transmit period
		*/
		mmw_psic_auto_gain_clear();
	#endif
	#if SLEEP_HEART_RAW_ENABLE
		if (s_sleep_heart_target_valid) {
			heart_raw_sample = sleep_heart_raw_sample_get(s_sleep_heart_target_range_idx);
		}
	#endif

		if (ptr_point_cloud_buffer == NULL) {
			D_data_test.motion_points_num = 0;
			D_data_test.tracker_objs_num = 0;
			printf("SLEEP_NO_MOTION_BUFFER\r\n");
			if (sleep_process_frame) {
				sleep_detect_update(NULL, 0, D_data_test.micro_points, D_data_test.micro_points_num);
				sleep_detect_update_breath_phase(breath_phase_sample.valid,
				                                 breath_phase_sample.range_idx,
				                                 breath_phase_sample.phase_mrad,
				                                 breath_phase_sample.amplitude);
			}
			sleep_heart_safe_report(D_State_test.proc_frame_num, &breath_phase_sample);
			sleep_heart_device_feed(D_State_test.proc_frame_num, &breath_phase_sample);
			if (sleep_process_frame && sleep_detect_should_report()) {
				const SleepDetectResult *sleep_result = sleep_detect_get_result();
				if (!sleep_result->in_bed) s_sleep_heart_breath_reference=0;
				else if (sleep_result->breath_rate>0) s_sleep_heart_breath_reference=sleep_result->breath_rate;
				sleep_target_diagnostic_report(&D_data_test);
				printf("SLEEP P=%u A=%u BED=%u BV=%u BR=%u SS=%u SQ=%u SA=%u SB=%u MI=%u TE=%u TC=%u STC=%u BT=%lu OT=%lu BE=%u BX=%u BVR=%u ABR=%u BS=%u AT=%lu QT=%lu ST=%lu LQ=%lu SL=%lu SC=%u LS=%lu SI=%u AR=%u QR=%u SR=%u RVR=%u SE=%u BRmin=%u BRmax=%u MP=%u MCP=%u Bbin=%u Bph=%d Bpd=%d Bamp=%lu Bpq=%u\r\n",
				       sleep_result->has_person,
				       sleep_result->is_active,
				       sleep_result->in_bed,
				       sleep_result->breath_valid,
				       sleep_result->breath_rate,
				       sleep_result->sleep_state,
				       sleep_result->sleep_quiet_streak,
				       sleep_result->sleep_active_streak,
				       sleep_result->sleep_breath_streak,
				       sleep_result->motion_intensity,
				       sleep_result->turn_event,
				       sleep_result->turn_count,
				       sleep_result->sleep_turn_count,
				       (unsigned long)sleep_result->in_bed_frames,
				       (unsigned long)sleep_result->out_bed_frames,
				       sleep_result->bed_enter_count,
				       sleep_result->bed_exit_count,
				       sleep_result->breath_valid_ratio,
				       sleep_result->breath_rate_avg,
				       sleep_result->breath_state,
				       (unsigned long)sleep_result->active_frames,
				       (unsigned long)sleep_result->quiet_frames,
				       (unsigned long)sleep_result->likely_sleep_frames,
				       (unsigned long)sleep_result->longest_quiet_frames,
				       (unsigned long)sleep_result->sleep_onset_frames,
				       sleep_result->sleep_segment_count,
				       (unsigned long)sleep_result->longest_likely_sleep_frames,
				       sleep_result->sleep_interruption_count,
				       sleep_result->active_ratio,
				       sleep_result->quiet_ratio,
				       sleep_result->likely_sleep_ratio,
				       sleep_result->breath_rate_output_ratio,
				       sleep_result->session_end,
				       sleep_result->breath_rate_min,
				       sleep_result->breath_rate_max,
				       D_data_test.motion_points_num,
				       D_data_test.micro_points_num,
				       sleep_result->breath_target_bin,
				       sleep_result->breath_phase_mrad,
				       sleep_result->breath_phase_delta_mrad,
				       (unsigned long)sleep_result->breath_phase_amp,
				       sleep_result->breath_phase_quality);
			}
			D_State_test.proc_frame_num++;
			return 0;
		}

		/*---------------------  DATA CONVERT PROCESS --------------------- */
		mmw_velocity_get(&velocity_mm, &veloc_resol_mm);
		range_bin_size_cm = (float)range_mm / range_fft_num * 0.1f;
		dop_bin_size_cm = (float)velocity_mm / doppler_fft_num * 0.2f; /* velocity_mm is single size */
		doppler_idx_half = doppler_fft_num >> 1;

		/* Convert to cartisian coordinated system, an extra memory copy will be applied */
		if (D_data_test.motion_points == 0){
			//memset(&D_data_test, 0, sizeof(D_data_test));
			mmw_process_mem_alloc((void**)&D_data_test.motion_points, sizeof(*D_data_test.motion_points) * ptr_point_cloud_buffer->point_cloud_num);
		}
		if (ptr_point_cloud_buffer->point_cloud_num > 0 && ptr_point_cloud_buffer->ptr_motion_point_cloud_data == NULL) {
			printf("SLEEP_MOTION_DATA_NULL num=%u\r\n", ptr_point_cloud_buffer->point_cloud_num);
			ptr_point_cloud_buffer->point_cloud_num = 0;
		}
		if (ptr_point_cloud_buffer->point_cloud_num > 0 && D_data_test.motion_points == 0) {
			printf("SLEEP_MOTION_ALLOC_FAIL num=%u\r\n", ptr_point_cloud_buffer->point_cloud_num);
			ptr_point_cloud_buffer->point_cloud_num = 0;
		}
		for (int point_cloud_idx = 0; point_cloud_idx < ptr_point_cloud_buffer->point_cloud_num; point_cloud_idx++) {
			/*
			 * unit cm in D_data_test structure,
			 * x = range * cos(phi) * sin(theta)
			 * y = range * cos(phi) * cos(theta)
			 * z = range * sin(phi)
			 * or
			 * x = range * sin(x)
			 * y = range * sqrt(1-sin(x)^2-sin(z)^2)
			 * z = range * sin(z)
			*/
			sin_x = ptr_point_cloud_buffer->ptr_motion_point_cloud_data[point_cloud_idx].azi_phase;
			sin_z = ptr_point_cloud_buffer->ptr_motion_point_cloud_data[point_cloud_idx].ele_phase;
			sin_y = 1.f - sin_x * sin_x - sin_z * sin_z;
			if (sin_y < 0.f) {
				sin_y = 0.f;
			}
			sin_y = sqrtf(sin_y);
				mmw_point_cloud_trans_radar_coord_to_user_coord(ptr_point_cloud_buffer->ptr_motion_point_cloud_data[point_cloud_idx].range_idx * sin_x * range_bin_size_cm,
									   ptr_point_cloud_buffer->ptr_motion_point_cloud_data[point_cloud_idx].range_idx * sin_y * range_bin_size_cm,
									   ptr_point_cloud_buffer->ptr_motion_point_cloud_data[point_cloud_idx].range_idx * sin_z * range_bin_size_cm,
									   user_coordinate
									);

			D_data_test.motion_points[point_cloud_idx].cart.x = user_coordinate[0];
			D_data_test.motion_points[point_cloud_idx].cart.y = user_coordinate[1];
			D_data_test.motion_points[point_cloud_idx].cart.z = user_coordinate[2];
			D_data_test.motion_points[point_cloud_idx].cart.vel = (ptr_point_cloud_buffer->ptr_motion_point_cloud_data[point_cloud_idx].doppler_idx - doppler_idx_half) * dop_bin_size_cm;
			D_data_test.motion_points[point_cloud_idx].cart.snr = ptr_point_cloud_buffer->ptr_motion_point_cloud_data[point_cloud_idx].sig_snr * 100;
		}

		D_data_test.motion_points_num = ptr_point_cloud_buffer->point_cloud_num;
		D_data_test.tracker_objs_num  = 0;

		mmw_process_mem_free((void**) &ptr_point_cloud_buffer->ptr_motion_point_cloud_data);
		mmw_process_mem_free((void**) &ptr_point_cloud_buffer);
		SET_TIME_ESCAPE(time_start, time_start);
		UPDATE_TIME_DBG_INFO(time_start, "convert", g_time_dbg_info.motion_convert);
		SET_TIME_START(time_start);

		/*---------------------  DATA REPORT PROCESS --------------------- */
		/* This is a block function, free memory after this functon returns */
		if (sleep_process_frame) {
			sleep_detect_update(D_data_test.motion_points,
						   D_data_test.motion_points_num,
						   D_data_test.micro_points,
						   D_data_test.micro_points_num);
			sleep_detect_update_breath_phase(breath_phase_sample.valid,
			                                 breath_phase_sample.range_idx,
			                                 breath_phase_sample.phase_mrad,
			                                 breath_phase_sample.amplitude);
		}
		sleep_heart_safe_report(D_State_test.proc_frame_num, &breath_phase_sample);
		sleep_heart_device_feed(D_State_test.proc_frame_num, &breath_phase_sample);
		if (sleep_process_frame && sleep_detect_should_report()) {
			const SleepDetectResult *sleep_result = sleep_detect_get_result();
			if (!sleep_result->in_bed) s_sleep_heart_breath_reference=0;
			else if (sleep_result->breath_rate>0) s_sleep_heart_breath_reference=sleep_result->breath_rate;
			sleep_target_diagnostic_report(&D_data_test);
			printf("SLEEP P=%u A=%u BED=%u BV=%u BR=%u SS=%u SQ=%u SA=%u SB=%u MI=%u TE=%u TC=%u STC=%u BT=%lu OT=%lu BE=%u BX=%u BVR=%u ABR=%u BS=%u AT=%lu QT=%lu ST=%lu LQ=%lu SL=%lu SC=%u LS=%lu SI=%u AR=%u QR=%u SR=%u RVR=%u SE=%u BRmin=%u BRmax=%u MT=%u MB=%u McT=%u McB=%u Act=%u PP=%u PA=%u BP=%u BA=%u BrP=%u BrA=%u Bsig=%u Bq=%u Bper=%u Brc=%u Brr=%u Brs=%u Bbin=%u Bph=%d Bpd=%d Bamp=%lu Bpq=%u\r\n",
			       sleep_result->has_person,
			       sleep_result->is_active,
			       sleep_result->in_bed,
			       sleep_result->breath_valid,
			       sleep_result->breath_rate,
			       sleep_result->sleep_state,
			       sleep_result->sleep_quiet_streak,
			       sleep_result->sleep_active_streak,
			       sleep_result->sleep_breath_streak,
			       sleep_result->motion_intensity,
			       sleep_result->turn_event,
			       sleep_result->turn_count,
			       sleep_result->sleep_turn_count,
			       (unsigned long)sleep_result->in_bed_frames,
			       (unsigned long)sleep_result->out_bed_frames,
			       sleep_result->bed_enter_count,
			       sleep_result->bed_exit_count,
			       sleep_result->breath_valid_ratio,
			       sleep_result->breath_rate_avg,
			       sleep_result->breath_state,
			       (unsigned long)sleep_result->active_frames,
			       (unsigned long)sleep_result->quiet_frames,
			       (unsigned long)sleep_result->likely_sleep_frames,
			       (unsigned long)sleep_result->longest_quiet_frames,
			       (unsigned long)sleep_result->sleep_onset_frames,
			       sleep_result->sleep_segment_count,
			       (unsigned long)sleep_result->longest_likely_sleep_frames,
			       sleep_result->sleep_interruption_count,
			       sleep_result->active_ratio,
			       sleep_result->quiet_ratio,
			       sleep_result->likely_sleep_ratio,
			       sleep_result->breath_rate_output_ratio,
			       sleep_result->session_end,
			       sleep_result->breath_rate_min,
			       sleep_result->breath_rate_max,
			       sleep_result->motion_points_total,
			       sleep_result->motion_points_bed,
			       sleep_result->micro_points_total,
			       sleep_result->micro_points_bed,
			       sleep_result->active_motion_points,
			       sleep_result->person_present_streak,
			       sleep_result->person_absent_streak,
			       sleep_result->bed_present_streak,
			       sleep_result->bed_absent_streak,
			       sleep_result->breath_present_streak,
			       sleep_result->breath_absent_streak,
				       sleep_result->breath_signal,
				       sleep_result->breath_quality,
				       sleep_result->breath_period_frames,
				       sleep_result->breath_rate_candidate,
				       sleep_result->breath_rate_repeat_count,
				       sleep_result->breath_rate_stable,
				       sleep_result->breath_target_bin,
				       sleep_result->breath_phase_mrad,
				       sleep_result->breath_phase_delta_mrad,
				       (unsigned long)sleep_result->breath_phase_amp,
				       sleep_result->breath_phase_quality);
		}
	#if SLEEP_BINARY_POINT_UPLOAD_ENABLE
		mmw_detection3d_data_report(&D_data_test, &D_State_test, NULL);
	#endif
		ADD_TIME_ESCAPE(time_report, time_start);
		UPDATE_TIME_DBG_INFO(time_report, "report", g_time_dbg_info.report);

		mmw_process_mem_free((void**)&D_data_test.motion_points);
		D_State_test.proc_frame_num++;
		
		if (g_dc_suppression_flag) {
			if (MMW_ERR_CODE_SUCCESS != mmw_psic_dc_suppression_update()) {
				printf("condition error!\r\n");
			}
		}
	} else {
		s_mmw_radar_analysis_skip_frame_status--; /* count skipped frames */
		/* when clutter_rm_method is DC,skip the last frame init DC */
		if (g_dc_suppression_flag && !s_mmw_radar_analysis_skip_frame_status) {
			mmw_clutter_halt_set(MMW_HALT_CLUTTER_UPDATE_ENABLE);
			mmw_psic_dc_suppression_init();
			/* mmw_psic_dc_suppression_init will clear cluuter and needs to be update once */
			mmw_psic_dc_suppression_update();
		}
	}

	return 0;
}

int mmw_ctrl_data_point_report_cb(void *mmw_data,void *arg) {
#if HEART_FAST_PROFILE
	uint32_t begin=sleep_fast_now(),actual_period=0,frames=0,stage,elapsed;
	MmwPsicMimoRxNum_t mimo;
	sleep_fast_apply_reset();s_fast_rf++;
	mmw_frame_get(&actual_period,&frames);mmw_psic_lib_sdk_get_tx_rx_num(&mimo);
	if(actual_period!=HEART_RADAR_PERIOD_MS || mimo.ant_tx_num!=1u || mimo.ant_rx_num!=3u || mimo.mimo_rx_num!=3u) {
		if(actual_period!=HEART_RADAR_PERIOD_MS)sleep_fast_warning(5u,actual_period);
		else sleep_fast_warning(6u,((uint32_t)mimo.ant_tx_num<<16)|((uint32_t)mimo.ant_rx_num<<8)|mimo.mimo_rx_num);
		mmw_psic_auto_gain_clear();
		if(g_dc_suppression_flag && !s_mmw_radar_analysis_skip_frame_status)mmw_psic_dc_suppression_update();
		return 0;
	}
	if(!s_mmw_radar_analysis_skip_frame_status)sleep_fast_capture();
	stage=sleep_fast_now();
	if(hf_slow_due(s_fast_rf)) {
		s_fast_input_valid=false;
		sleep_slow_point_cb(mmw_data,arg);
		{
			const SleepDetectResult *r=sleep_detect_get_result();uint16_t ranges=0,dopplers=0;
			mmw_fft_num_get(&ranges,&dopplers);
			unsigned event=ht_update(&s_fast_target,r->in_bed,r->is_active,
				s_fast_input_valid,s_fast_input_bin,ranges);
			if(event) {
				uint32_t gaps=s_fast_front.gaps;hf_reset(&s_fast_front);s_fast_front.gaps=gaps;s_fast_mask=0u;
				if(r->in_bed || (event&HT_EXIT)) {
					heart_rate_device_get(&s_fast_sample.device);
					printf("HFTARGET V=1 GEN=%lu RF=%lu SF=%lu EPOCH=%lu EVENT=%u TARGET=%u FIRST=%u LOCK=%u STREAK=%u MISSING=%u\r\n",
						(unsigned long)s_fast_sample.device.generation,(unsigned long)s_fast_rf,(unsigned long)D_State_test.proc_frame_num,
						(unsigned long)s_fast_target.epoch,event,s_fast_target.target,s_fast_target.first,
						s_fast_target.locked,s_fast_target.streak,s_fast_target.missing);
				}
			}
		}
		elapsed=sleep_fast_now()-stage;if(elapsed>s_fast_cost.slow)s_fast_cost.slow=elapsed;
	} else {
		mmw_psic_auto_gain_clear();
		if(g_dc_suppression_flag && !s_mmw_radar_analysis_skip_frame_status)mmw_psic_dc_suppression_update();
	}
	elapsed=sleep_fast_now()-begin;if(elapsed>s_fast_cost.callback)s_fast_cost.callback=elapsed;
	if(elapsed>=HEART_RADAR_PERIOD_MS)s_fast_cost.overruns++;
	if(s_fast_rf%100u==0u) {
		SleepTraceStats log;sleep_trace_get(&log);
		printf("HFSTAT V=1 GEN=%lu RF=%lu READ=%lu ERR=%lu GAP=%lu MAXMS=%lu MASK=%u LOCK=%u FRONT_BYTES=%u STACK=%lu LOGQ=%lu LOGHIGH=%lu LOGDROP=%lu LOGBYTES=%lu LOGFMT=%lu LOGSTACK=%lu\r\n",
			(unsigned long)s_fast_gen,(unsigned long)s_fast_rf,(unsigned long)s_fast_reads,(unsigned long)s_fast_errors,
			(unsigned long)s_fast_front.gaps,(unsigned long)s_fast_max_ms,s_fast_mask,s_fast_target.locked?1u:0u,(unsigned)sizeof(s_fast_front),
			(unsigned long)(uxTaskGetStackHighWaterMark(NULL)*sizeof(StackType_t)),
			(unsigned long)log.used,(unsigned long)log.high,(unsigned long)log.refused,(unsigned long)log.refused_bytes,
			(unsigned long)log.format_refused,(unsigned long)log.stack_free);
		printf("HFCOST V=1 GEN=%lu RF=%lu READ=%lu FILTER=%lu ENCODE=%lu ENQUEUE=%lu SLOW=%lu CALLBACK=%lu OVERRUN=%lu\r\n",
			(unsigned long)s_fast_gen,(unsigned long)s_fast_rf,(unsigned long)s_fast_cost.read,(unsigned long)s_fast_cost.filter,
			(unsigned long)s_fast_cost.encode,(unsigned long)s_fast_cost.enqueue,(unsigned long)s_fast_cost.slow,
			(unsigned long)s_fast_cost.callback,(unsigned long)s_fast_cost.overruns);
		{
			uint32_t hi,lo,t0=(uint32_t)xTaskGetTickCount(),t1;
			hc_read(sleep_fast_hw_hi,sleep_fast_hw_lo,&hi,&lo);t1=(uint32_t)xTaskGetTickCount();
			printf("HFCLOCK V=1 GEN=%lu RF=%lu SF=%lu T0=%lu T1=%lu THZ=%lu MS=%lu HI=%lu LO=%lu CPUHZ=%lu\r\n",
				(unsigned long)s_fast_gen,(unsigned long)s_fast_rf,(unsigned long)D_State_test.proc_frame_num,
				(unsigned long)t0,(unsigned long)t1,(unsigned long)configTICK_RATE_HZ,
				(unsigned long)(((uint64_t)t0*1000u)/configTICK_RATE_HZ),
				(unsigned long)hi,(unsigned long)lo,(unsigned long)configCPU_CLOCK_HZ);
		}
	}
	return 0;
#else
	static uint32_t callbacks,max_cost;uint32_t begin=hr_clock_now_ms(),elapsed;
	int ret=sleep_slow_point_cb(mmw_data,arg);
	elapsed=hr_clock_now_ms()-begin;if(elapsed>max_cost)max_cost=elapsed;
	if(++callbacks%25u==0u) {
		HrClockSnapshot clock;SleepTraceStats log;hr_clock_snapshot(&clock);sleep_trace_get(&log);
		printf("HRPORT V=1 F=%lu HRMS=%lu OSMS=%lu HI=%lu LO=%lu HZ=%lu FAULT=%lu CBMAX=%lu STACK=%lu LOGQ=%lu LOGHIGH=%lu LOGDROP=%lu LOGFMT=%lu LOGSTACK=%lu\r\n",
			(unsigned long)D_State_test.proc_frame_num,(unsigned long)clock.ms,(unsigned long)clock.os_ms,
			(unsigned long)clock.hi,(unsigned long)clock.lo,(unsigned long)clock.hz,(unsigned long)clock.fault,
			(unsigned long)max_cost,(unsigned long)(uxTaskGetStackHighWaterMark(NULL)*sizeof(StackType_t)),
			(unsigned long)log.used,(unsigned long)log.high,(unsigned long)log.refused,
			(unsigned long)log.format_refused,(unsigned long)log.stack_free);
	}
	return ret;
#endif
}

void radar_analysis_reset_skip_frame_status(void) {
#if HEART_FAST_PROFILE
	sleep_fast_reset();
#endif
	s_mmw_radar_analysis_skip_frame_status = RADAR_ANALYSIS_SKIP_FRAME_NUM;
	s_sleep_report_cb_seen = false;
	s_sleep_process_seen = false;
	s_sleep_breath_target_valid = false;
	s_sleep_breath_target_range_idx = 0;
	s_sleep_heart_target_valid = false;
	s_sleep_heart_target_range_idx = 0;
	s_sleep_heart_breath_reference = 0;
	sleep_detect_reset();
	heart_rate_device_reset();
	printf("SLEEP_REPORT_ARM skip=%u\r\n", s_mmw_radar_analysis_skip_frame_status);
}
