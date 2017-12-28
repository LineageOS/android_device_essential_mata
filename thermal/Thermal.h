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
#ifndef ANDROID_HARDWARE_THERMAL_V1_1_MATA_THERMAL_H
#define ANDROID_HARDWARE_THERMAL_V1_1_MATA_THERMAL_H

#include <android/hardware/thermal/1.1/IThermal.h>
#include <android/hardware/thermal/1.1/IThermalCallback.h>
#include <hidl/Status.h>
#include <hidl/MQDescriptor.h>

namespace android {
namespace hardware {
namespace thermal {
namespace V1_1 {
namespace implementation {

using ::android::hardware::thermal::V1_0::CoolingDevice;
using ::android::hardware::thermal::V1_0::CpuUsage;
using ::android::hardware::thermal::V1_0::Temperature;
using ::android::hardware::thermal::V1_0::ThermalStatus;
using ::android::hardware::thermal::V1_0::ThermalStatusCode;
using ::android::hardware::thermal::V1_1::IThermal;
using ::android::hardware::thermal::V1_1::IThermalCallback;
using ::android::hardware::Return;
using ::android::hardware::Void;
using ::android::hardware::hidl_vec;
using ::android::hardware::hidl_string;
using ::android::hardware::hidl_death_recipient;
using ::android::hidl::base::V1_0::IBase;
using ::android::sp;

struct Thermal : public IThermal {
    // Local functions used internally by thermal-engine follow.
    std::string getSkinSensorType();
    void notifyThrottling(bool isThrottling, const Temperature& temperature);
    Thermal();
    // Methods from ::android::hardware::thermal::V1_0::IThermal follow.
    Return<void> getTemperatures(getTemperatures_cb _hidl_cb)  override;
    Return<void> getCpuUsages(getCpuUsages_cb _hidl_cb)  override;
    Return<void> getCoolingDevices(getCoolingDevices_cb _hidl_cb)  override;
    // Methods from ::android::hardware::thermal::V1_1::IThermal follow.
    Return<void> registerThermalCallback(
        const sp<IThermalCallback>& callback) override;

  private:
    bool enabled;
};

}  // namespace implementation
}  // namespace V1_1
}  // namespace thermal
}  // namespace hardware
}  // namespace android

#endif  // ANDROID_HARDWARE_THERMAL_V1_1_MATA_THERMAL_H
