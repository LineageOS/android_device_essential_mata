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

#include <cerrno>
#include <vector>

#include <android-base/logging.h>

#include "Thermal.h"
#include "thermal-helper.h"

namespace android {
namespace hardware {
namespace thermal {
namespace V1_1 {
namespace implementation {

Thermal::Thermal() : enabled(initThermal()) {}

namespace {

// Saves the IThermalCallback client object registered from the
// framework for sending thermal events to the framework thermal event bus.
sp<IThermalCallback> gThermalCallback;

struct ThermalDeathRecipient : hidl_death_recipient {
    virtual void serviceDied(
        uint64_t cookie __unused, const wp<IBase>& who __unused) {
        gThermalCallback = nullptr;
        LOG(ERROR) << "IThermalCallback HIDL service died";
    }
};

sp<ThermalDeathRecipient> gThermalCallbackDied = nullptr;

} // anonymous namespace

// Methods from ::android::hardware::thermal::V1_0::IThermal follow.
Return<void> Thermal::getTemperatures(getTemperatures_cb _hidl_cb) {
    ThermalStatus status;
    status.code = ThermalStatusCode::SUCCESS;
    hidl_vec<Temperature> temperatures;
    temperatures.resize(kTemperatureNum);

    if (!enabled) {
        status.code = ThermalStatusCode::FAILURE;
        status.debugMessage = "Unsupported hardware";
        _hidl_cb(status, temperatures);
        return Void();
    }

    ssize_t ret = fillTemperatures(&temperatures);
    if (ret < 0) {
        status.code = ThermalStatusCode::FAILURE;
        status.debugMessage = strerror(-ret);
    }
    _hidl_cb(status, temperatures);

    for (auto& t : temperatures) {
        LOG(DEBUG) << "getTemperatures "
                   << " Type: " << static_cast<int>(t.type)
                   << " Name: " << t.name
                   << " CurrentValue: " << t.currentValue
                   << " ThrottlingThreshold: " << t.throttlingThreshold
                   << " ShutdownThreshold: " << t.shutdownThreshold
                   << " VrThrottlingThreshold: " << t.vrThrottlingThreshold;
    }

    return Void();
}

Return<void> Thermal::getCpuUsages(getCpuUsages_cb _hidl_cb) {
    ThermalStatus status;
    status.code = ThermalStatusCode::SUCCESS;
    hidl_vec<CpuUsage> cpuUsages;
    cpuUsages.resize(kCpuNum);

    if (!enabled) {
        status.code = ThermalStatusCode::FAILURE;
        status.debugMessage = "Unsupported hardware";
        _hidl_cb(status, cpuUsages);
        return Void();
    }

    ssize_t ret = fillCpuUsages(&cpuUsages);
    if (ret < 0) {
        status.code = ThermalStatusCode::FAILURE;
        status.debugMessage = strerror(-ret);
    }

    for (auto& u : cpuUsages) {
        LOG(DEBUG) << "getCpuUsages "
                   << " Name: " << u.name
                   << " Active: " << u.active
                   << " Total: " << u.total
                   << " IsOnline: " << u.isOnline;
    }

    _hidl_cb(status, cpuUsages);
    return Void();
}

Return<void> Thermal::getCoolingDevices(getCoolingDevices_cb _hidl_cb) {
    ThermalStatus status;
    status.code = ThermalStatusCode::SUCCESS;
    hidl_vec<CoolingDevice> coolingDevices;

    if (!enabled) {
        status.code = ThermalStatusCode::FAILURE;
        status.debugMessage = "Unsupported hardware";
        _hidl_cb(status, coolingDevices);
        return Void();
    }

    LOG(DEBUG) << "No Cooling Device";
    _hidl_cb(status, coolingDevices);
    return Void();
}

// Methods from ::android::hardware::thermal::V1_1::IThermal follow.

Return<void> Thermal::registerThermalCallback(
    const sp<IThermalCallback>& callback) {
    gThermalCallback = callback;

    if (gThermalCallback != nullptr) {
        if (gThermalCallbackDied == nullptr)
            gThermalCallbackDied = new ThermalDeathRecipient();

        if (gThermalCallbackDied != nullptr)
            gThermalCallback->linkToDeath(
                gThermalCallbackDied, 0x451F /* cookie, unused */);
        LOG(INFO) << "ThermalCallback registered";
    } else {
        LOG(INFO) << "ThermalCallback unregistered";
    }
    return Void();
}

// Local functions used internally by thermal-engine follow.

std::string Thermal::getSkinSensorType() {
    return getTargetSkinSensorType();
}

void Thermal::notifyThrottling(
    bool isThrottling, const Temperature& temperature) {
    if (gThermalCallback != nullptr) {
        Return<void> ret =
            gThermalCallback->notifyThrottling(isThrottling, temperature);
        if (!ret.isOk()) {
          if (ret.isDeadObject()) {
              gThermalCallback = nullptr;
              LOG(WARNING) << "Dropped throttling event, ThermalCallback died";
          } else {
              LOG(WARNING) <<
                  "Failed to send throttling event to ThermalCallback";
          }
        }
    } else {
        LOG(WARNING) <<
            "Dropped throttling event, no ThermalCallback registered";
    }
}

}  // namespace implementation
}  // namespace V1_1
}  // namespace thermal
}  // namespace hardware
}  // namespace android
