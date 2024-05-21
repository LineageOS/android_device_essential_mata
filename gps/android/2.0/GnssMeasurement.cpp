/*
 * Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
 * Not a Contribution
 */
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

#define LOG_TAG "LocSvc_GnssMeasurementInterface"

#include <log_util.h>
#include "GnssMeasurement.h"
#include "MeasurementAPIClient.h"

namespace android {
namespace hardware {
namespace gnss {
namespace V2_0 {
namespace implementation {

void GnssMeasurement::GnssMeasurementDeathRecipient::serviceDied(
        uint64_t cookie, const wp<IBase>& who) {
    LOC_LOGE("%s] service died. cookie: %llu, who: %p",
            __FUNCTION__, static_cast<unsigned long long>(cookie), &who);
    if (mGnssMeasurement != nullptr) {
        mGnssMeasurement->close();
    }
}

GnssMeasurement::GnssMeasurement() {
    mGnssMeasurementDeathRecipient = new GnssMeasurementDeathRecipient(this);
    mApi = new MeasurementAPIClient();
}

GnssMeasurement::~GnssMeasurement() {
    if (mApi) {
        mApi->destroy();
        mApi = nullptr;
    }
}

// Methods from ::android::hardware::gnss::V1_0::IGnssMeasurement follow.
Return<GnssMeasurement::GnssMeasurementStatus> GnssMeasurement::setCallback(
        const sp<V1_0::IGnssMeasurementCallback>& callback)  {

    Return<GnssMeasurement::GnssMeasurementStatus> ret =
        IGnssMeasurement::GnssMeasurementStatus::ERROR_GENERIC;
    if (mGnssMeasurementCbIface != nullptr) {
        LOC_LOGE("%s]: GnssMeasurementCallback is already set", __FUNCTION__);
        return IGnssMeasurement::GnssMeasurementStatus::ERROR_ALREADY_INIT;
    }

    if (callback == nullptr) {
        LOC_LOGE("%s]: callback is nullptr", __FUNCTION__);
        return ret;
    }
    if (mApi == nullptr) {
        LOC_LOGE("%s]: mApi is nullptr", __FUNCTION__);
        return ret;
    }

    clearInterfaces();

    mGnssMeasurementCbIface = callback;
    mGnssMeasurementCbIface->linkToDeath(mGnssMeasurementDeathRecipient, 0);

    return mApi->measurementSetCallback(callback);
}

void GnssMeasurement::clearInterfaces() {
    if (mGnssMeasurementCbIface != nullptr) {
        mGnssMeasurementCbIface->unlinkToDeath(mGnssMeasurementDeathRecipient);
        mGnssMeasurementCbIface = nullptr;
    }
    if (mGnssMeasurementCbIface_1_1 != nullptr) {
        mGnssMeasurementCbIface_1_1->unlinkToDeath(mGnssMeasurementDeathRecipient);
        mGnssMeasurementCbIface_1_1 = nullptr;
    }
    if (mGnssMeasurementCbIface_2_0 != nullptr) {
        mGnssMeasurementCbIface_2_0->unlinkToDeath(mGnssMeasurementDeathRecipient);
        mGnssMeasurementCbIface_2_0 = nullptr;
    }
}

Return<void> GnssMeasurement::close()  {
    if (mApi == nullptr) {
        LOC_LOGE("%s]: mApi is nullptr", __FUNCTION__);
        return Void();
    }

    clearInterfaces();
    mApi->measurementClose();

    return Void();
}

// Methods from ::android::hardware::gnss::V1_1::IGnssMeasurement follow.
Return<GnssMeasurement::GnssMeasurementStatus> GnssMeasurement::setCallback_1_1(
        const sp<V1_1::IGnssMeasurementCallback>& callback, bool enableFullTracking) {

    Return<GnssMeasurement::GnssMeasurementStatus> ret =
        IGnssMeasurement::GnssMeasurementStatus::ERROR_GENERIC;
    if (mGnssMeasurementCbIface_1_1 != nullptr) {
        LOC_LOGE("%s]: GnssMeasurementCallback is already set", __FUNCTION__);
        return IGnssMeasurement::GnssMeasurementStatus::ERROR_ALREADY_INIT;
    }

    if (callback == nullptr) {
        LOC_LOGE("%s]: callback is nullptr", __FUNCTION__);
        return ret;
    }
    if (nullptr == mApi) {
        LOC_LOGE("%s]: mApi is nullptr", __FUNCTION__);
        return ret;
    }

    clearInterfaces();

    mGnssMeasurementCbIface_1_1 = callback;
    mGnssMeasurementCbIface_1_1->linkToDeath(mGnssMeasurementDeathRecipient, 0);

    GnssPowerMode powerMode = enableFullTracking?
            GNSS_POWER_MODE_M1 : GNSS_POWER_MODE_M2;

    return mApi->measurementSetCallback_1_1(callback, powerMode);
}
// Methods from ::android::hardware::gnss::V2_0::IGnssMeasurement follow.
Return<V1_0::IGnssMeasurement::GnssMeasurementStatus> GnssMeasurement::setCallback_2_0(
        const sp<V2_0::IGnssMeasurementCallback>& callback,
        bool enableFullTracking) {

    Return<GnssMeasurement::GnssMeasurementStatus> ret =
        IGnssMeasurement::GnssMeasurementStatus::ERROR_GENERIC;
    if (mGnssMeasurementCbIface_2_0 != nullptr) {
        LOC_LOGE("%s]: GnssMeasurementCallback is already set", __FUNCTION__);
        return IGnssMeasurement::GnssMeasurementStatus::ERROR_ALREADY_INIT;
    }

    if (callback == nullptr) {
        LOC_LOGE("%s]: callback is nullptr", __FUNCTION__);
        return ret;
    }
    if (nullptr == mApi) {
        LOC_LOGE("%s]: mApi is nullptr", __FUNCTION__);
        return ret;
    }

    clearInterfaces();

    mGnssMeasurementCbIface_2_0 = callback;
    mGnssMeasurementCbIface_2_0->linkToDeath(mGnssMeasurementDeathRecipient, 0);

    GnssPowerMode powerMode = enableFullTracking ?
        GNSS_POWER_MODE_M1 : GNSS_POWER_MODE_M2;

    return mApi->measurementSetCallback_2_0(callback, powerMode);
}

}  // namespace implementation
}  // namespace V2_0
}  // namespace gnss
}  // namespace hardware
}  // namespace android
