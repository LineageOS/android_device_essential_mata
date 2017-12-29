/*
 * Copyright (c) 2017, The Linux Foundation. All rights reserved.
 * Copyright (C) 2017, The LineageOS Project
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

#ifndef __THERMAL_HELPER_H__
#define __THERMAL_HELPER_H__

#include <android/hardware/thermal/1.1/IThermal.h>

namespace android {
namespace hardware {
namespace thermal {
namespace V1_1 {
namespace implementation {

using ::android::hardware::thermal::V1_0::CpuUsage;
using ::android::hardware::thermal::V1_0::Temperature;

constexpr const char *kCpuUsageFile = "/proc/stat";
constexpr const char *kTemperatureFileFormat = "/sys/class/thermal/thermal_zone%d/temp";
constexpr const char *kCpuOnlineFileFormat = "/sys/devices/system/cpu/cpu%d/online";

// thermal-engine.conf
constexpr unsigned int kSkinSensorNum = 8;
constexpr auto         kSkinSensorType = "quiet_therm";
constexpr unsigned int kTsensOffset = 9;
constexpr unsigned int kSkinThrottlingThreshold = 41;
constexpr unsigned int kSkinShutdownThreshold = 47;
constexpr unsigned int kVrThrottledBelowMin = 44;

constexpr unsigned int kBatterySensorNum = 0;
constexpr unsigned int kGpuTsensOffset = 11;
constexpr unsigned int kCpuNum = 8;

constexpr const char *kGpuLabel = "GPU";
constexpr const char *kBatteryLabel = "battery";
constexpr const char *kSkinLabel = "skin";
constexpr const char *kCpuLabel[kCpuNum] = {"CPU0", "CPU1", "CPU2", "CPU3", "CPU4", "CPU5", "CPU6", "CPU7"};
constexpr int kCpuTsensOffset[kCpuNum] = {1, 2, 4, 3, 5, 6, 7, 8};

// Sum of kCpuNum + 3 for GPU, BATTERY, and SKIN.
constexpr unsigned int kTemperatureNum = 3 + kCpuNum;

// qcom, therm-reset-temp
constexpr unsigned int kCpuShutdownThreshold = 115;
// qcom,freq-mitigation-temp
constexpr unsigned int kCpuThrottlingThreshold = 95;

// config_shutdownBatteryTemperature in overlay/frameworks/base/core/res/res/values/config.xml
constexpr unsigned int kBatteryShutdownThreshold = 60;


bool initThermal();
ssize_t fillTemperatures(hidl_vec<Temperature> *temperatures);
ssize_t fillCpuUsages(hidl_vec<CpuUsage> *cpuUsages);
std::string getTargetSkinSensorType();

}  // namespace implementation
}  // namespace V1_1
}  // namespace thermal
}  // namespace hardware
}  // namespace android

#endif //__THERMAL_HELPER_H__
