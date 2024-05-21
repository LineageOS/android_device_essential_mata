/*
 * Copyright (c) 2017-2020, The Linux Foundation. All rights reserved.
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

#define LOG_TAG "LocSvc_GnssBatchingInterface"

#include <log_util.h>
#include <BatchingAPIClient.h>
#include "GnssBatching.h"

namespace android {
namespace hardware {
namespace gnss {
namespace V2_1 {
namespace implementation {

void GnssBatching::GnssBatchingDeathRecipient::serviceDied(
        uint64_t cookie, const wp<IBase>& who) {
    LOC_LOGE("%s] service died. cookie: %llu, who: %p",
            __FUNCTION__, static_cast<unsigned long long>(cookie), &who);
    if (mGnssBatching != nullptr) {
        mGnssBatching->stop();
        mGnssBatching->cleanup();
    }
}

GnssBatching::GnssBatching() : mApi(nullptr) {
    mGnssBatchingDeathRecipient = new GnssBatchingDeathRecipient(this);
}

GnssBatching::~GnssBatching() {
    if (mApi != nullptr) {
        mApi->destroy();
        mApi = nullptr;
    }
}


// Methods from ::android::hardware::gnss::V1_0::IGnssBatching follow.
Return<bool> GnssBatching::init(const sp<V1_0::IGnssBatchingCallback>& callback) {
    if (mApi != nullptr) {
        LOC_LOGD("%s]: mApi is NOT nullptr, delete it first", __FUNCTION__);
        mApi->destroy();
        mApi = nullptr;
    }

    mApi = new BatchingAPIClient(callback);
    if (mApi == nullptr) {
        LOC_LOGE("%s]: failed to create mApi", __FUNCTION__);
        return false;
    }

    if (mGnssBatchingCbIface != nullptr) {
        mGnssBatchingCbIface->unlinkToDeath(mGnssBatchingDeathRecipient);
    }
    mGnssBatchingCbIface = callback;
    if (mGnssBatchingCbIface != nullptr) {
        mGnssBatchingCbIface->linkToDeath(mGnssBatchingDeathRecipient, 0 /*cookie*/);
    }

    return true;
}

Return<uint16_t> GnssBatching::getBatchSize() {
    uint16_t ret = 0;
    if (mApi == nullptr) {
        LOC_LOGE("%s]: mApi is nullptr", __FUNCTION__);
    } else {
        ret = mApi->getBatchSize();
    }
    return ret;
}

Return<bool> GnssBatching::start(const IGnssBatching::Options& options) {
    bool ret = false;
    if (mApi == nullptr) {
        LOC_LOGE("%s]: mApi is nullptr", __FUNCTION__);
    } else {
        ret = mApi->startSession(options);
    }
    return ret;
}

Return<void> GnssBatching::flush() {
    if (mApi == nullptr) {
        LOC_LOGE("%s]: mApi is nullptr", __FUNCTION__);
    } else {
        mApi->flushBatchedLocations();
    }
    return Void();
}

Return<bool> GnssBatching::stop() {
    bool ret = false;
    if (mApi == nullptr) {
        LOC_LOGE("%s]: mApi is nullptr", __FUNCTION__);
    } else {
        ret = mApi->stopSession();
    }
    return ret;
}

Return<void> GnssBatching::cleanup() {
    if (mApi != nullptr) {
        mApi->stopSession();
    }
    if (mGnssBatchingCbIface != nullptr) {
        mGnssBatchingCbIface->unlinkToDeath(mGnssBatchingDeathRecipient);
        mGnssBatchingCbIface = nullptr;
    }
    if (mGnssBatchingCbIface_2_0 != nullptr) {
        mGnssBatchingCbIface_2_0->unlinkToDeath(mGnssBatchingDeathRecipient);
        mGnssBatchingCbIface_2_0 = nullptr;
    }
    return Void();
}

// Methods from ::android::hardware::gnss::V2_0::IGnssBatching follow.
Return<bool> GnssBatching::init_2_0(const sp<V2_0::IGnssBatchingCallback>& callback) {
    if (mApi != nullptr) {
        LOC_LOGD("%s]: mApi is NOT nullptr, delete it first", __FUNCTION__);
        mApi->destroy();
        mApi = nullptr;
    }

    mApi = new BatchingAPIClient(callback);
    if (mApi == nullptr) {
        LOC_LOGE("%s]: failed to create mApi", __FUNCTION__);
        return false;
    }

    if (mGnssBatchingCbIface_2_0 != nullptr) {
        mGnssBatchingCbIface_2_0->unlinkToDeath(mGnssBatchingDeathRecipient);
    }
    mGnssBatchingCbIface_2_0 = callback;
    if (mGnssBatchingCbIface_2_0 != nullptr) {
        mGnssBatchingCbIface_2_0->linkToDeath(mGnssBatchingDeathRecipient, 0 /*cookie*/);
    }

    return true;
}

}  // namespace implementation
}  // namespace V2_1
}  // namespace gnss
}  // namespace hardware
}  // namespace android
