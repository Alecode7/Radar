/**
 ******************************************************************************
 * @file    main.c
 * @brief   main define.
 * @verbatim    null
 ******************************************************************************
 * @attention
 *
 * Copyright (C) 2025 POSSUMIC TECHNOLOGY CO., LTD. All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *    1. Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the
 *       distribution.
 *    3. Neither the name of POSSUMIC TECHNOLOGY CO., LTD. nor the names of
 *       its contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 ******************************************************************************
 */


/* Includes.
 * ----------------------------------------------------------------------------
 */
#include <common.h>
#include <stdio.h>

#include "mmw_hif.h"
#include "mmw_ctrl.h"
#include "mmw_alg_doa.h"
#include "mmw_point_cloud_psic_lib.h"
#include "hal_wkio.h"
#include "hal_board.h"
#include "sleep_detect.h"
#include "v6_heart_device.h"
#define HF_AUX_IMPLEMENTATION
#include "heart_clock_capture.h"
#undef HF_AUX_IMPLEMENTATION
#include "hr_migration_clock_impl.h"

#include "log.h"
/* Product protocol is served by W803. The source TX service has no local RSP stream. */
void rsp_result_runtime_poll(uint32_t ms) {(void)ms;}
#include "sleep_async_log_impl.h"
#define HF_AUX_IMPLEMENTATION
#include "heart_raw_record.h"
#undef HF_AUX_IMPLEMENTATION
#define printf sleep_trace_printf
/* Private typedef.
 * ----------------------------------------------------------------------------
 */
/* Private defines.
 * ----------------------------------------------------------------------------
 */
/* Private macros.
 * ----------------------------------------------------------------------------
 */
#if (CONFIG_PM)
#define PM_WKIO_PORT                  WKIO_PORT_A
#define PM_WKIO_PIN                   WKIO_PIN_11
#endif

/* Private variables.
 * ----------------------------------------------------------------------------
 */
#if (CONFIG_PM)
static uint8_t pm_locked = 0;
static struct wakelock wkio_pm_lock = {0x00};
#endif

/* Private function prototypes.
 * ----------------------------------------------------------------------------
 */
/* Exported functions.
 * ----------------------------------------------------------------------------
 */
int human_motion_default_config(void)
{
#if (CONFIG_SOC_RS6240 || CONFIG_SOC_RS7241)
    int ret = mmw_mode_cfg(MMW_MIMO_2T4R, MMW_WORK_MODE_2DFFT);
#elif CONFIG_SOC_RS6130
    int ret = mmw_mode_cfg(MMW_MIMO_1T3R, MMW_WORK_MODE_2DFFT);
#else
	#error "no valid board chosen"
#endif

    if (ret) {
        LOG_PRINT("mode cfg error! %d\n", ret);
        return ret;
    }
    ret = mmw_range_cfg(80*256, 80); /* resol=8cm, dfft_num=20.48m */
    if (ret) {
        LOG_PRINT("range cfg error! %d\n", ret);
        return ret;
    }
    ret = mmw_velocity_cfg(200 * 16, 200); /*resol=0.2m/s, dfft_num=32 */
    if (ret) {
        LOG_PRINT("velocity cfg error! %d\n", ret);
        return ret;
    }
    /* Recovery profile: keep the proven 5 Hz radar and sleep cadence. */
    ret = mmw_frame_cfg(200, 0);
    if (ret) {
        LOG_PRINT("frame cfg error! %d\n", ret);
        return ret;
    }
    ret = mmw_coordinate_config(MMW_COORDINATE_TYPE_CART);
    if (ret) {
        LOG_PRINT("coordinate cfg error! %d\n", ret);
        return ret;
    }
    return 0;
}

#if (CONFIG_PM)
static void pm_wkio_callback(void *arg)
{
    if (pm_locked) {
        pm_locked = 0;
        pm_policy_wake_unlock(&wkio_pm_lock);
    } else {
        pm_locked = 1;
        pm_policy_wake_lock(&wkio_pm_lock);
    }
}

static void pm_wkio_change_policy(void)
{
    pm_locked = 1;
    pm_policy_wake_lock(&wkio_pm_lock);
}

static int wkio_wake_init(void)
{
    HAL_Dev_t *wkioDev;
    HAL_Status_t status = HAL_STATUS_OK;
    WKIO_WakeParam_t wkioIrqParam;

    wkioDev = HAL_WKIO_Init(PM_WKIO_PORT);
    if (wkioDev == NULL) {
        LOG_PRINT("Init wkio fail\n");
        return -1;
    }

    status = HAL_WKIO_SetPinMode(wkioDev, PM_WKIO_PIN, HAL_WKIO_MODE_WAKE);
    if (status != HAL_STATUS_OK) {
        LOG_PRINT("Set wkio mode fail %d\n", status);
        return status;
    }

    wkioIrqParam.event = WKIO_WAKE_EVT_FALLING_EDGE;
    wkioIrqParam.pull = HAL_WKIO_PULL_DOWN;
    wkioIrqParam.callback = pm_wkio_callback;
    wkioIrqParam.arg =  NULL;

    status = HAL_WKIO_SetWakeParam(wkioDev, PM_WKIO_PIN, &wkioIrqParam);
    if (status != HAL_STATUS_OK) {
        LOG_PRINT("Set wkio wake init fail %d\n", status);
        return status;
    }

    status = HAL_WKIO_EnableWake(wkioDev);
    if (status != HAL_STATUS_OK) {
        LOG_PRINT("Set wkio wake enable fail %d\n", status);
        return status;
    }

    return 0;
}
#endif

int main(void)
{
    uint32_t status = 0;


    int cpuid = csi_get_cpu_id();

    LOG_PRINT("Radar Analysis Project\n");
    LOG_PRINT("-------------------------------------------\n");

    #if ((CONFIG_SOC_RS6240_CPUF == 1) || (CONFIG_SOC_RS7241_CPUF == 1))
    if (cpuid) {
        LOG_PRINT("\tCurrent System: CPUF\n");
    } else {
        LOG_PRINT("\tERROR: Current System: CPUS\n");
        return 0;
    }
    #elif ((CONFIG_SOC_RS6240_CPUS == 1) || (CONFIG_SOC_RS6130 == 1))
    if (cpuid == 0) {
        LOG_PRINT("\tCurrent System: CPUS\n");
    } else {
        LOG_PRINT("\tERROR: Current System: CPUF\n");
        return 0;
    }
    #endif

    LOG_PRINT("mmw_ctrl_open\n");
    status = mmw_ctrl_open(true, false, true);
    if (status != 0) {
        LOG_PRINT("mmw_ctrl_open fail %d\n", status);
    }

    status = sleep_trace_init();
    if (status != 0) {printf("SLEEP_FW_INIT_FAIL STAGE=TX RET=%ld\r\n",(long)(int32_t)status);return status;}
    status = human_motion_default_config();
    if (status != 0) {
        LOG_PRINT("human_motion_default_config fail %d\n", status);
    }

    sleep_detect_init(NULL);
    status = v6_heart_init();
    printf("HRFW_INIT RET=%ld MODE=IQ_STREAM HEART_COMPUTE=0 OWNER=W803 FS=5 CORE_BYTES=0 WS_BYTES=0\r\n", (long)(int32_t)status);
    if (status != 0) {
        printf("SLEEP_FW_INIT_FAIL STAGE=IQ_CLOCK RET=%ld\r\n", (long)(int32_t)status);
        return status;
    }
    printf("SLEEP_FW_READY p1812 SAFEHR5 MICROIQ NOPOINTTX\r\n");
    printf("FW_VERIFY_VERSION=UART_RADAR_PROTOCOL_20261120_V29\r\n");

#if (CONFIG_PM)
    wkio_wake_init();
    pm_wkio_change_policy();
#endif
    return 0;
}

/*
 ******************************************************************************
 * (C) COPYRIGHT POSSUMIC TECHNOLOGY
 * END OF FILE
 */
