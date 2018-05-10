/*
 * Copyright (C) 2016 The Android Open Source Project
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
#ifndef ANDROID_HARDWARE_DUMPSTATE_V1_0_DUMPSTATEDEVICE_H
#define ANDROID_HARDWARE_DUMPSTATE_V1_0_DUMPSTATEDEVICE_H

#include <android/hardware/dumpstate/1.0/IDumpstateDevice.h>
#include <hidl/MQDescriptor.h>
#include <hidl/Status.h>

namespace android {
namespace hardware {
namespace dumpstate {
namespace V1_0 {
namespace implementation {

using ::android::hardware::dumpstate::V1_0::IDumpstateDevice;
using ::android::hardware::hidl_array;
using ::android::hardware::hidl_handle;
using ::android::hardware::hidl_string;
using ::android::hardware::hidl_vec;
using ::android::hardware::Return;
using ::android::hardware::Void;
using ::android::sp;

struct DumpstateDevice : public IDumpstateDevice {
  // Methods from ::android::hardware::dumpstate::V1_0::IDumpstateDevice follow.
  Return<void> dumpstateBoard(const hidl_handle& h) override;
};

}  // namespace implementation
}  // namespace V1_0
}  // namespace dumpstate
}  // namespace hardware
}  // namespace android

#endif  // ANDROID_HARDWARE_DUMPSTATE_V1_0_DUMPSTATEDEVICE_H
