/*
 * Copyright (C) 2018 The Android Open Source Project
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

#define LOG_TAG "android.hardware.power.stats@1.0-service.pixel"

#include <android-base/properties.h>
#include <android/log.h>
#include <binder/IPCThreadState.h>
#include <binder/IServiceManager.h>
#include <binder/ProcessState.h>
#include <hidl/HidlTransportSupport.h>

#include <pixelpowerstats/AidlStateResidencyDataProvider.h>
#include <pixelpowerstats/GenericStateResidencyDataProvider.h>
#include <pixelpowerstats/PowerStats.h>
#include <pixelpowerstats/WlanStateResidencyDataProvider.h>

using android::OK;
using android::sp;
using android::status_t;

// libhwbinder:
using android::hardware::configureRpcThreadpool;
using android::hardware::joinRpcThreadpool;

// Generated HIDL files
using android::hardware::power::stats::V1_0::IPowerStats;
using android::hardware::power::stats::V1_0::PowerEntityInfo;
using android::hardware::power::stats::V1_0::PowerEntityStateSpace;
using android::hardware::power::stats::V1_0::PowerEntityType;
using android::hardware::power::stats::V1_0::implementation::PowerStats;

// Pixel specific
using android::hardware::google::pixel::powerstats::AidlStateResidencyDataProvider;
using android::hardware::google::pixel::powerstats::GenericStateResidencyDataProvider;
using android::hardware::google::pixel::powerstats::PowerEntityConfig;
using android::hardware::google::pixel::powerstats::StateResidencyConfig;
using android::hardware::google::pixel::powerstats::WlanStateResidencyDataProvider;

int main(int /* argc */, char ** /* argv */) {
    ALOGI("power.stats service 1.0 is starting.");

    bool isDebuggable = android::base::GetBoolProperty("ro.debuggable", false);

    PowerStats *service = new PowerStats();

    if (isDebuggable) {
        // Add power entities related to rpmh
        const uint64_t RPM_CLK = 19200;  // RPM runs at 19.2Mhz. Divide by 19200 for msec
        std::function<uint64_t(uint64_t)> rpmConvertToMs = [](uint64_t a) { return a / RPM_CLK; };
        std::vector<StateResidencyConfig> rpmStateResidencyConfigs = {
            {.name = "XO_shutdown",
             .entryCountSupported = true,
             .entryCountPrefix = "XO Count:",
             .totalTimeSupported = true,
             .totalTimePrefix = "Accumulated XO duration:",
             .totalTimeTransform = rpmConvertToMs,
             .lastEntrySupported = false}};

        sp<GenericStateResidencyDataProvider> rpmSdp =
                new GenericStateResidencyDataProvider("/d/system_stats");

        uint32_t apssId = service->addPowerEntity("APSS", PowerEntityType::SUBSYSTEM);
        rpmSdp->addEntity(apssId, PowerEntityConfig("APSS", rpmStateResidencyConfigs));

        uint32_t mpssId = service->addPowerEntity("MPSS", PowerEntityType::SUBSYSTEM);
        rpmSdp->addEntity(mpssId, PowerEntityConfig("MPSS", rpmStateResidencyConfigs));

        uint32_t adspId = service->addPowerEntity("ADSP", PowerEntityType::SUBSYSTEM);
        rpmSdp->addEntity(adspId, PowerEntityConfig("ADSP", rpmStateResidencyConfigs));

        uint32_t slpiId = service->addPowerEntity("SLPI", PowerEntityType::SUBSYSTEM);
        rpmSdp->addEntity(slpiId, PowerEntityConfig("SLPI", rpmStateResidencyConfigs));

        service->addStateResidencyDataProvider(rpmSdp);

        // Add SoC power entity
        std::vector<StateResidencyConfig> socStateResidencyConfigs = {
            {.name = "XO_shutdown",
             .header = "RPM Mode:vlow",
             .entryCountSupported = true,
             .entryCountPrefix = "count:",
             .totalTimeSupported = true,
             .totalTimePrefix = "actual last sleep(msec):",
             .lastEntrySupported = false},
            {.name = "VMIN",
             .header = "RPM Mode:vmin",
             .entryCountSupported = true,
             .entryCountPrefix = "count:",
             .totalTimeSupported = true,
             .totalTimePrefix = "actual last sleep(msec):",
             .lastEntrySupported = false}};

         sp<GenericStateResidencyDataProvider> socSdp =
                new GenericStateResidencyDataProvider("/d/system_stats");

        uint32_t socId = service->addPowerEntity("SoC", PowerEntityType::POWER_DOMAIN);
        socSdp->addEntity(socId, PowerEntityConfig(socStateResidencyConfigs));

        service->addStateResidencyDataProvider(socSdp);

        // Add WLAN power entity
        uint32_t wlanId = service->addPowerEntity("WLAN", PowerEntityType::SUBSYSTEM);
        sp<WlanStateResidencyDataProvider> wlanSdp =
                new WlanStateResidencyDataProvider(wlanId, "/d/wlan0/power_stats");
        service->addStateResidencyDataProvider(wlanSdp);
    }

    // Add Power Entities that require the Aidl data provider
    sp<AidlStateResidencyDataProvider> aidlSdp = new AidlStateResidencyDataProvider();
    // TODO(117585786): Add real power entities here
    // uint32_t mock1Id = service->addPowerEntity("Mock1", PowerEntityType::SUBSYSTEM);
    // aidlSdp->addEntity(mock1Id, "Mock1", {"state_a", "state_b"});
    // uint32_t mock2Id = service->addPowerEntity("Mock2", PowerEntityType::SUBSYSTEM);
    // aidlSdp->addEntity(mock2Id, "Mock2", {"state_c", "state_d"});

    auto serviceStatus = android::defaultServiceManager()->addService(
            android::String16("power.stats-vendor"), aidlSdp);
    if (serviceStatus != android::OK) {
        ALOGE("Unable to register power.stats-vendor service %d", serviceStatus);
        return 1;
    }
    sp<android::ProcessState> ps{android::ProcessState::self()};  // Create non-HW binder threadpool
    ps->startThreadPool();

    service->addStateResidencyDataProvider(aidlSdp);

    // Configure the threadpool
    configureRpcThreadpool(1, true /*callerWillJoin*/);

    status_t status = service->registerAsService();
    if (status != OK) {
        ALOGE("Could not register service for power.stats HAL Iface (%d), exiting.", status);
        return 1;
    }

    ALOGI("power.stats service is ready");
    joinRpcThreadpool();

    // In normal operation, we don't expect the thread pool to exit
    ALOGE("power.stats service is shutting down");
    return 1;
}
