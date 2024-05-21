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

#define LOG_TAG "LocSvc__AGnssRilInterface"

#include <log_util.h>
#include <dlfcn.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sstream>
#include <string>
#include "Gnss.h"
#include "AGnssRil.h"
#include <DataItemConcreteTypesBase.h>

typedef void* (getLocationInterface)();

namespace android {
namespace hardware {
namespace gnss {
namespace V2_0 {
namespace implementation {


AGnssRil::AGnssRil(Gnss* gnss) : mGnss(gnss) {
    ENTRY_LOG_CALLFLOW();
}

AGnssRil::~AGnssRil() {
    ENTRY_LOG_CALLFLOW();
}

Return<bool> AGnssRil::updateNetworkState(bool connected, NetworkType type, bool /*roaming*/) {
    ENTRY_LOG_CALLFLOW();
    // Extra NetworkTypes not available in IAgnssRil enums
    const int NetworkType_BLUETOOTH = 7;
    const int NetworkType_ETHERNET = 9;
    const int NetworkType_PROXY = 16;
    std::string apn("");

    // for XTRA
    if (nullptr != mGnss && ( nullptr != mGnss->getGnssInterface() )) {
        int8_t typeout = loc_core::TYPE_UNKNOWN;
        switch(type)
        {
            case IAGnssRil::NetworkType::MOBILE:
                typeout = loc_core::TYPE_MOBILE;
                break;
            case IAGnssRil::NetworkType::WIFI:
                typeout = loc_core::TYPE_WIFI;
                break;
            case IAGnssRil::NetworkType::MMS:
                typeout = loc_core::TYPE_MMS;
                break;
            case IAGnssRil::NetworkType::SUPL:
                typeout = loc_core::TYPE_SUPL;
                break;
            case IAGnssRil::NetworkType::DUN:
                typeout = loc_core::TYPE_DUN;
                break;
            case IAGnssRil::NetworkType::HIPRI:
                typeout = loc_core::TYPE_HIPRI;
                break;
            case IAGnssRil::NetworkType::WIMAX:
                typeout = loc_core::TYPE_WIMAX;
                break;
            default:
                {
                    int networkType = (int) type;
                    // Handling network types not available in IAgnssRil
                    switch(networkType)
                    {
                        case NetworkType_BLUETOOTH:
                            typeout = loc_core::TYPE_BLUETOOTH;
                            break;
                        case NetworkType_ETHERNET:
                            typeout = loc_core::TYPE_ETHERNET;
                            break;
                        case NetworkType_PROXY:
                            typeout = loc_core::TYPE_PROXY;
                            break;
                        default:
                            typeout = loc_core::TYPE_UNKNOWN;
                    }
                }
                break;
        }
        mGnss->getGnssInterface()->updateConnectionStatus(connected, typeout, false, 0, apn);
    }
    return true;
}
Return<bool> AGnssRil::updateNetworkState_2_0(const V2_0::IAGnssRil::NetworkAttributes& attributes) {
    ENTRY_LOG_CALLFLOW();
    std::string apn = attributes.apn;
    if (nullptr != mGnss && (nullptr != mGnss->getGnssInterface())) {
        int8_t typeout = loc_core::TYPE_UNKNOWN;
        bool roaming = false;
        if (attributes.capabilities & IAGnssRil::NetworkCapability::NOT_METERED) {
            typeout = loc_core::TYPE_WIFI;
        } else {
            typeout = loc_core::TYPE_MOBILE;
        }
        if (attributes.capabilities & IAGnssRil::NetworkCapability::NOT_ROAMING) {
            roaming = false;
        }
        LOC_LOGd("apn string received is: %s", apn.c_str());
        mGnss->getGnssInterface()->updateConnectionStatus(attributes.isConnected,
                typeout, roaming, (NetworkHandle) attributes.networkHandle, apn);
    }
    return true;
}

}  // namespace implementation
}  // namespace V2_0
}  // namespace gnss
}  // namespace hardware
}  // namespace android
