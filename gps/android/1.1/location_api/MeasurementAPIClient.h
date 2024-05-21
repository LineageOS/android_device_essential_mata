/* Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation, nor the names of its
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
 *
 */

#ifndef MEASUREMENT_API_CLINET_H
#define MEASUREMENT_API_CLINET_H

#include <mutex>
#include <android/hardware/gnss/1.1/IGnssMeasurement.h>
#include <android/hardware/gnss/1.1/IGnssMeasurementCallback.h>
#include <LocationAPIClientBase.h>
#include <hidl/Status.h>
#include <gps_extended_c.h>

namespace android {
namespace hardware {
namespace gnss {
namespace V1_1 {
namespace implementation {

using ::android::sp;

class MeasurementAPIClient : public LocationAPIClientBase
{
public:
    MeasurementAPIClient();
    MeasurementAPIClient(const MeasurementAPIClient&) = delete;
    MeasurementAPIClient& operator=(const MeasurementAPIClient&) = delete;

    // for GpsMeasurementInterface
    Return<V1_0::IGnssMeasurement::GnssMeasurementStatus> measurementSetCallback(
            const sp<V1_0::IGnssMeasurementCallback>& callback);
    Return<V1_0::IGnssMeasurement::GnssMeasurementStatus> measurementSetCallback_1_1(
            const sp<IGnssMeasurementCallback>& callback,
            GnssPowerMode powerMode = GNSS_POWER_MODE_INVALID,
            uint32_t timeBetweenMeasurement = GPS_DEFAULT_FIX_INTERVAL_MS);
    void measurementClose();
    Return<IGnssMeasurement::GnssMeasurementStatus> startTracking(
            GnssPowerMode powerMode = GNSS_POWER_MODE_INVALID,
            uint32_t timeBetweenMeasurement = GPS_DEFAULT_FIX_INTERVAL_MS);

    // callbacks we are interested in
    void onGnssMeasurementsCb(GnssMeasurementsNotification gnssMeasurementsNotification) final;

private:
    virtual ~MeasurementAPIClient();

    std::mutex mMutex;
    sp<V1_0::IGnssMeasurementCallback> mGnssMeasurementCbIface;
    sp<IGnssMeasurementCallback> mGnssMeasurementCbIface_1_1;

    bool mTracking;
};

}  // namespace implementation
}  // namespace V1_1
}  // namespace gnss
}  // namespace hardware
}  // namespace android
#endif // MEASUREMENT_API_CLINET_H
