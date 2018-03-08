/*
 * Copyright (c) 2016, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * *    * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#define LOG_NIDEBUG 0

#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <stdlib.h>

#define ATRACE_TAG (ATRACE_TAG_POWER | ATRACE_TAG_HAL)
#define LOG_TAG "QCOM PowerHAL"
#include <utils/Log.h>
#include <cutils/trace.h>
#include <hardware/hardware.h>
#include <hardware/power.h>

#include "utils.h"
#include "metadata-defs.h"
#include "hint-data.h"
#include "performance.h"
#include "power-common.h"
#include "powerhintparser.h"

static int sustained_mode_handle = 0;
static int vr_mode_handle = 0;
static int launch_handle = 0;
static int sustained_performance_mode = 0;
static int vr_mode = 0;
static int launch_mode = 0;
#define CHECK_HANDLE(x) (((x)>0) && ((x)!=-1))

int is_perf_hint_active(int hint)
{
    switch (hint) {
        case SUSTAINED_PERF_HINT_ID:
            return sustained_performance_mode != 0;
        case VR_MODE_HINT_ID:
            return vr_mode != 0;
        case VR_MODE_SUSTAINED_PERF_HINT_ID:
            return vr_mode != 0 && sustained_performance_mode != 0;
    }
    return 0;
}

static int process_sustained_perf_hint(void *data)
{
    int duration = 0;
    int *resource_values = NULL;
    int resources = 0;

    if (data && sustained_performance_mode == 0) {
        if (vr_mode == 0) { // Sustained mode only.
            resource_values = getPowerhint(SUSTAINED_PERF_HINT_ID, &resources);
            if (!resource_values) {
                ALOGE("Can't get sustained perf hints from xml ");
                return HINT_NONE;
            }
            sustained_mode_handle = interaction_with_handle(
                sustained_mode_handle, duration, resources, resource_values);
            if (!CHECK_HANDLE(sustained_mode_handle)) {
                ALOGE("Failed interaction_with_handle for sustained_mode_handle");
                return HINT_NONE;
            }
        } else if (vr_mode == 1) { // Sustained + VR mode.
            release_request(vr_mode_handle);
            resource_values = getPowerhint(VR_MODE_SUSTAINED_PERF_HINT_ID, &resources);
            if (!resource_values) {
                ALOGE("Can't get VR mode sustained perf hints from xml ");
                return HINT_NONE;
            }
            sustained_mode_handle = interaction_with_handle(
                sustained_mode_handle, duration, resources, resource_values);
            if (!CHECK_HANDLE(sustained_mode_handle)) {
                ALOGE("Failed interaction_with_handle for sustained_mode_handle");
                return HINT_NONE;
            }
        }
        sustained_performance_mode = 1;
    } else if (sustained_performance_mode == 1) {
        release_request(sustained_mode_handle);
        if (vr_mode == 1) { // Switch back to VR Mode.
            resource_values = getPowerhint(VR_MODE_HINT_ID, &resources);
            if (!resource_values) {
                ALOGE("Can't get VR mode perf hints from xml ");
                return HINT_NONE;
            }
            vr_mode_handle = interaction_with_handle(
                vr_mode_handle, duration, resources, resource_values);
            if (!CHECK_HANDLE(vr_mode_handle)) {
                ALOGE("Failed interaction_with_handle for vr_mode_handle");
                return HINT_NONE;
            }
        }
        sustained_performance_mode = 0;
    }
    return HINT_HANDLED;
}

static int process_vr_mode_hint(void *data)
{
    int duration = 0;
    int *resource_values = NULL;
    int resources = 0;

    if (data && vr_mode == 0) {
        if (sustained_performance_mode == 0) { // VR mode only.
            resource_values = getPowerhint(VR_MODE_HINT_ID, &resources);
            if (!resource_values) {
                ALOGE("Can't get VR mode perf hints from xml ");
                return HINT_NONE;
            }
            vr_mode_handle = interaction_with_handle(
                vr_mode_handle, duration, resources, resource_values);
            if (!CHECK_HANDLE(vr_mode_handle)) {
                ALOGE("Failed interaction_with_handle for vr_mode_handle");
                return HINT_NONE;
            }
        } else if (sustained_performance_mode == 1) { // Sustained + VR mode.
            release_request(sustained_mode_handle);
            resource_values = getPowerhint(VR_MODE_SUSTAINED_PERF_HINT_ID, &resources);
            if (!resource_values) {
                ALOGE("Can't get VR mode sustained perf hints from xml ");
                return HINT_NONE;
            }
            vr_mode_handle = interaction_with_handle(
                vr_mode_handle, duration, resources, resource_values);
            if (!CHECK_HANDLE(vr_mode_handle)) {
                ALOGE("Failed interaction_with_handle for vr_mode_handle");
                return HINT_NONE;
            }
        }
        vr_mode = 1;
    } else if (vr_mode == 1) {
        release_request(vr_mode_handle);
        if (sustained_performance_mode == 1) { // Switch back to sustained Mode.
            resource_values = getPowerhint(SUSTAINED_PERF_HINT_ID, &resources);
            if (!resource_values) {
                ALOGE("Can't get sustained perf hints from xml ");
                return HINT_NONE;
            }
            sustained_mode_handle = interaction_with_handle(
                sustained_mode_handle, duration, resources, resource_values);
            if (!CHECK_HANDLE(sustained_mode_handle)) {
                ALOGE("Failed interaction_with_handle for sustained_mode_handle");
                return HINT_NONE;
            }
        }
        vr_mode = 0;
    }

    return HINT_HANDLED;
}

static int process_boost(int boost_handle, int duration)
{
    int *resource_values;
    int resources;

    resource_values = getPowerhint(BOOST_HINT_ID, &resources);

    if (resource_values != NULL) {
        boost_handle = interaction_with_handle(
            boost_handle, duration, resources, resource_values);
        if (!CHECK_HANDLE(boost_handle)) {
            ALOGE("Failed interaction_with_handle for boost_handle");
        }
    }

    return boost_handle;
}

static int process_video_encode_hint(void *data)
{
    static int boost_handle = -1;

    if (data) {
        // TODO: remove the launch boost based on camera launch time
        int duration = 2000; // boosts 2s for starting encoding
        boost_handle = process_boost(boost_handle, duration);
        ALOGD("LAUNCH ENCODER-ON: %d MS", duration);
        int *resource_values = NULL;
        int resources = 0;
        resource_values = getPowerhint(DEFAULT_VIDEO_ENCODE_HINT_ID, &resources);
        if (resource_values != NULL)
            perform_hint_action(DEFAULT_VIDEO_ENCODE_HINT_ID, resource_values, resources);
        ALOGD("Video Encode hint start");
        return HINT_HANDLED;
    } else {
        undo_hint_action(DEFAULT_VIDEO_ENCODE_HINT_ID);
        ALOGD("Video Encode hint stop");
        return HINT_HANDLED;
    }
    return HINT_NONE;
}

static int process_activity_launch_hint(void *data)
{
    // boost will timeout in 1.25s
    int duration = 1250;
    ATRACE_BEGIN("launch");
    if (sustained_performance_mode || vr_mode) {
        ATRACE_END();
        return HINT_HANDLED;
    }

    ALOGD("LAUNCH HINT: %s", data ? "ON" : "OFF");
    // restart the launch hint if the framework has not yet released
    // this shouldn't happen, but we've seen bugs where it could
    if (data) {
        launch_handle = process_boost(launch_handle, duration);
        if (launch_handle > 0) {
            launch_mode = 1;
            ALOGD("Activity launch hint handled");
            ATRACE_INT("launch_lock", 1);
            ATRACE_END();
            return HINT_HANDLED;
        } else {
            ATRACE_END();
            return HINT_NONE;
        }
    } else if (data == NULL  && launch_mode == 1) {
        // framework release hints aren't necessarily reliable
        // always wait the full duration
        // release_request(launch_handle);
        ATRACE_INT("launch_lock", 0);
        launch_mode = 0;
        ATRACE_END();
        return HINT_HANDLED;
    }
    ATRACE_END();
    return HINT_NONE;
}

int power_hint_override(power_hint_t hint, void *data)
{
    int ret_val = HINT_NONE;
    switch(hint) {
        case POWER_HINT_VIDEO_ENCODE:
            ret_val = process_video_encode_hint(data);
            break;
        case POWER_HINT_SUSTAINED_PERFORMANCE:
            ret_val = process_sustained_perf_hint(data);
            break;
        case POWER_HINT_VR_MODE:
            ret_val = process_vr_mode_hint(data);
            break;
        case POWER_HINT_LAUNCH:
            ret_val = process_activity_launch_hint(data);
            break;
        default:
            break;
    }
    return ret_val;
}

int set_interactive_override(int UNUSED(on))
{
    return HINT_HANDLED; /* Don't excecute this code path, not in use */
}
