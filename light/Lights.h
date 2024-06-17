/*
 * SPDX-FileCopyrightText: 2018-2024 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <aidl/android/hardware/light/BnLights.h>

#include <mutex>
#include <vector>

using ::aidl::android::hardware::light::BnLights;
using ::aidl::android::hardware::light::FlashMode;
using ::aidl::android::hardware::light::HwLight;
using ::aidl::android::hardware::light::HwLightState;
using ::aidl::android::hardware::light::LightType;

typedef void (*LightStateHandler)(const HwLightState&);

struct LightBackend {
    LightType type;
    HwLightState state;
    LightStateHandler handler;

    LightBackend(LightType type, LightStateHandler handler) : type(type), handler(handler) {
        this->state.color = 0xff000000;
    }
};

namespace aidl {
namespace android {
namespace hardware {
namespace light {

class Lights : public BnLights {
  public:
    ndk::ScopedAStatus setLightState(int id, const HwLightState& state) override;
    ndk::ScopedAStatus getLights(std::vector<HwLight>* types) override;

  private:
    std::mutex globalLock;
};

}  // namespace light
}  // namespace hardware
}  // namespace android
}  // namespace aidl
