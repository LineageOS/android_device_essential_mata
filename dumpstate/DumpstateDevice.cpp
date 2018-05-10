/*
 * Copyright 2016 The Android Open Source Project
 * Copyright 2018 The LineageOS Project
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

#define LOG_TAG "dumpstate"

#include "DumpstateDevice.h"

#include <android-base/properties.h>
#include <android-base/unique_fd.h>
#include <cutils/properties.h>
#include <libgen.h>
#include <log/log.h>
#include <stdlib.h>
#include <string>
#include <fstream>

#include "DumpstateUtil.h"

using android::os::dumpstate::CommandOptions;
using android::os::dumpstate::DumpFileToFd;
using android::os::dumpstate::PropertiesHelper;
using android::os::dumpstate::RunCommandToFd;

static constexpr char SIDECAR_ATTACHED_PATH[] = "/sys/class/sidecar/attached";
static constexpr char SIDECAR_POWER_CONTROL_PATH[] = "/sys/class/sidecar/power_control";

namespace android {
namespace hardware {
namespace dumpstate {
namespace V1_0 {
namespace implementation {

static void DumpSidecar(int fd) {
    int isAttached;
    std::ifstream attached{SIDECAR_ATTACHED_PATH};
    if (!attached) {
        int error = errno;
        ALOGE("Failed to open %s (%d): %s", SIDECAR_ATTACHED_PATH, error,
                strerror(error));
        return;
    } else {
        DumpFileToFd(fd, "Accessory attached", SIDECAR_ATTACHED_PATH);
        attached >> isAttached;
    }

    std::ifstream powerControl{SIDECAR_POWER_CONTROL_PATH};
    if (!powerControl) {
        int error = errno;
            ALOGE("Failed to open %s (%d): %s", SIDECAR_POWER_CONTROL_PATH, error,
                strerror(error));
        return;
    } else {
        DumpFileToFd(fd, "Accessory power control", SIDECAR_POWER_CONTROL_PATH);
    }

    if (isAttached == 0) {
        ALOGE("No accessory currently connected");
        return;
    } else {
        DumpFileToFd(fd, "Accessory", "/dev/ttyACM0");
    }
}

// Methods from ::android::hardware::dumpstate::V1_0::IDumpstateDevice follow.
Return<void> DumpstateDevice::dumpstateBoard(const hidl_handle& handle) {
    if (handle == nullptr || handle->numFds < 1) {
        ALOGE("no FDs\n");
        return Void();
    }

    int fd = handle->data[0];
    if (fd < 0) {
        ALOGE("invalid FD: %d\n", handle->data[0]);
        return Void();
    }

    DumpFileToFd(fd, "CPU present", "/sys/devices/system/cpu/present");
    DumpFileToFd(fd, "CPU online", "/sys/devices/system/cpu/online");
    DumpFileToFd(fd, "INTERRUPTS", "/proc/interrupts");
    DumpFileToFd(fd, "RPM Stats", "/d/rpm_stats");
    DumpFileToFd(fd, "Power Management Stats", "/d/rpm_master_stats");
    DumpFileToFd(fd, "CNSS Pre-Alloc", "/d/cnss-prealloc/status");
    DumpFileToFd(fd, "SMD Log", "/d/ipc_logging/smd/log");
    DumpFileToFd(fd, "BT Logs", "/d/ipc_logging/c171000.uart_pwr/log");
    RunCommandToFd(fd, "ION HEAPS", {"/vendor/bin/sh", "-c", "for d in $(ls -d /d/ion/*); do for f in $(ls $d); do echo --- $d/$f; cat $d/$f; done; done"});
    DumpFileToFd(fd, "dmabuf info", "/d/dma_buf/bufinfo");
    RunCommandToFd(fd, "Temperatures", {"/vendor/bin/sh", "-c", "for f in `ls /sys/class/thermal` ; do type=`cat /sys/class/thermal/$f/type` ; temp=`cat /sys/class/thermal/$f/temp` ; echo \"$type: $temp\" ; done"});
    DumpFileToFd(fd, "cpu0-3 time-in-state", "/sys/devices/system/cpu/cpu0/cpufreq/stats/time_in_state");
    RunCommandToFd(fd, "cpu0-3 cpuidle", {"/vendor/bin/sh", "-c", "for d in $(ls -d /sys/devices/system/cpu/cpu0/cpuidle/state*); do echo \"$d: `cat $d/name` `cat $d/desc` `cat $d/time` `cat $d/usage`\"; done"});
    DumpFileToFd(fd, "cpu4-7 time-in-state", "/sys/devices/system/cpu/cpu4/cpufreq/stats/time_in_state");
    RunCommandToFd(fd, "cpu4-7 cpuidle", {"/vendor/bin/sh", "-c", "for d in $(ls -d /sys/devices/system/cpu/cpu4/cpuidle/state*); do echo \"$d: `cat $d/name` `cat $d/desc` `cat $d/time` `cat $d/usage`\"; done"});
    DumpFileToFd(fd, "MDP xlogs", "/data/vendor/display/mdp_xlog");

    DumpSidecar(fd);
    return Void();
};

}  // namespace implementation
}  // namespace V1_0
}  // namespace dumpstate
}  // namespace hardware
}  // namespace android
