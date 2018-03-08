/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "android.hardware.power@1.1-service.wahoo"

#include <android/log.h>
#include <android-base/file.h>
#include <android-base/properties.h>
#include <android-base/strings.h>
#include <utils/Log.h>
#include "Power.h"
#include "power-common.h"
#include "power-helper.h"

/* RPM runs at 19.2Mhz. Divide by 19200 for msec */
#define RPM_CLK 19200

extern struct stat_pair rpm_stat_map[];

namespace android {
namespace hardware {
namespace power {
namespace V1_1 {
namespace implementation {

using ::android::hardware::power::V1_0::Feature;
using ::android::hardware::power::V1_0::PowerHint;
using ::android::hardware::power::V1_0::PowerStatePlatformSleepState;
using ::android::hardware::power::V1_0::Status;
using ::android::hardware::power::V1_1::PowerStateSubsystem;
using ::android::hardware::hidl_vec;
using ::android::hardware::Return;
using ::android::hardware::Void;

Power::Power() {
    power_init();
    mInteractionHandler.Init();
}

// Methods from ::android::hardware::power::V1_0::IPower follow.
Return<void> Power::setInteractive(bool interactive)  {
    if (!isSupportedGovernor()) {
        return Void();
    }
    power_set_interactive(interactive ? 1 : 0);
    return Void();
}

Return<void> Power::powerHint(PowerHint hint, int32_t data) {
    if (android::base::GetProperty("init.svc.perfd", "") != "running") {
        ALOGW("perfd is not started");
        return Void();
    }

    power_hint_t h = static_cast<power_hint_t>(hint);
    if (!isSupportedGovernor()) {
        return Void();
    }
    if (h == POWER_HINT_INTERACTION) {
        mInteractionHandler.Acquire(data);
        return Void();
    }
    power_hint(h, data ? &data : NULL);
    return Void();
}

Return<void> Power::setFeature(Feature /*feature*/, bool /*activate*/)  {
    //Nothing to do
    return Void();
}

Return<void> Power::getPlatformLowPowerStats(getPlatformLowPowerStats_cb _hidl_cb) {

    hidl_vec<PowerStatePlatformSleepState> states;
    uint64_t stats[MAX_PLATFORM_STATS * MAX_RPM_PARAMS] = {0};
    uint64_t *values;
    struct PowerStatePlatformSleepState *state;
    int ret;

    states.resize(PLATFORM_SLEEP_MODES_COUNT);

    ret = extract_platform_stats(stats);
    if (ret != 0) {
        states.resize(0);
        goto done;
    }

    /* Update statistics for XO_shutdown */
    state = &states[RPM_MODE_XO];
    state->name = "XO_shutdown";
    values = stats + (RPM_MODE_XO * MAX_RPM_PARAMS);

    state->residencyInMsecSinceBoot = values[1];
    state->totalTransitions = values[0];
    state->supportedOnlyInSuspend = false;
    state->voters.resize(XO_VOTERS);
    for(size_t i = 0; i < XO_VOTERS; i++) {
        int voter = static_cast<int>(i + XO_VOTERS_START);
        state->voters[i].name = rpm_stat_map[voter].label;
        values = stats + (voter * MAX_RPM_PARAMS);
        state->voters[i].totalTimeInMsecVotedForSinceBoot = values[0] / RPM_CLK;
        state->voters[i].totalNumberOfTimesVotedSinceBoot = values[1];
    }

    /* Update statistics for VMIN state */
    state = &states[RPM_MODE_VMIN];
    state->name = "VMIN";
    values = stats + (RPM_MODE_VMIN * MAX_RPM_PARAMS);

    state->residencyInMsecSinceBoot = values[1];
    state->totalTransitions = values[0];
    state->supportedOnlyInSuspend = false;
    state->voters.resize(VMIN_VOTERS);
    //Note: No filling of state voters since VMIN_VOTERS = 0

done:
    _hidl_cb(states, Status::SUCCESS);
    return Void();
}

static int get_wlan_low_power_stats(struct PowerStateSubsystem &subsystem) {

    uint64_t stats[WLAN_POWER_PARAMS_COUNT] = {0};
    struct PowerStateSubsystemSleepState *state;
    int ret;

    ret = extract_wlan_stats(stats);
    if (ret)
        return ret;

    subsystem.name = "wlan";
    subsystem.states.resize(WLAN_STATES_COUNT);

    /* Update statistics for Active State */
    state = &subsystem.states[WLAN_STATE_ACTIVE];
    state->name = "Active";
    state->residencyInMsecSinceBoot = stats[CUMULATIVE_TOTAL_ON_TIME_MS];
    state->totalTransitions = stats[DEEP_SLEEP_ENTER_COUNTER];
    state->lastEntryTimestampMs = 0; //FIXME need a new value from Qcom
    state->supportedOnlyInSuspend = false;

    /* Update statistics for Deep-Sleep state */
    state = &subsystem.states[WLAN_STATE_DEEP_SLEEP];
    state->name = "Deep-Sleep";
    state->residencyInMsecSinceBoot = stats[CUMULATIVE_SLEEP_TIME_MS];
    state->totalTransitions = stats[DEEP_SLEEP_ENTER_COUNTER];
    state->lastEntryTimestampMs = stats[LAST_DEEP_SLEEP_ENTER_TSTAMP_MS];
    state->supportedOnlyInSuspend = false;

    return 0;
}

// Methods from ::android::hardware::power::V1_1::IPower follow.
Return<void> Power::getSubsystemLowPowerStats(getSubsystemLowPowerStats_cb _hidl_cb) {

    hidl_vec<PowerStateSubsystem> subsystems;
    int ret;

    subsystems.resize(SUBSYSTEM_COUNT);

    //We currently have only one Subsystem for WLAN
    ret = get_wlan_low_power_stats(subsystems[SUBSYSTEM_WLAN]);
    if (ret != 0)
        goto done;

    //Add query for other subsystems here

done:
    _hidl_cb(subsystems, Status::SUCCESS);
    return Void();
}

bool Power::isSupportedGovernor() {
    std::string buf;
    if (android::base::ReadFileToString(SCALING_GOVERNOR_PATH, &buf)) {
        buf = android::base::Trim(buf);
    }
    // Only support EAS 1.2, legacy EAS and HMP
    if (buf == SCHEDUTIL_GOVERNOR || buf == SCHED_GOVERNOR || buf == INTERACTIVE_GOVERNOR) {
        return true;
    } else {
        ALOGE("Governor not supported by powerHAL, skipping");
        return false;
    }
}

Return<void> Power::powerHintAsync(PowerHint hint, int32_t data) {
    // just call the normal power hint in this oneway function
    return powerHint(hint, data);
}

}  // namespace implementation
}  // namespace V1_1
}  // namespace power
}  // namespace hardware
}  // namespace android
