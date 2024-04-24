/* Copyright (c) 2020, The Linux Foundation. All rights reserved.
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
#ifndef LOCATION_CLIENT_API_DIAG_BASE
#define LOCATION_CLIENT_API_DIAG_BASE

#include "LocLoggerBase.h"
#include "LocationClientApi.h"
#include <loc_misc_utils.h>

using namespace loc_util;

namespace location_client {

class LCAReportLoggerUtil {
public:
    typedef void (*LogGnssLocation)(const GnssLocation& gnssLocation,
            const LocationCapabilitiesMask& capMask);
    typedef void (*LogGnssSv)(const std::vector<GnssSv>& gnssSvsVector);
    typedef void (*LogGnssNmea)(uint64_t timestamp, uint32_t length, const char* nmea);
    typedef void (*LogGnssMeas)(const GnssMeasurements& gnssMeasurements);

    LCAReportLoggerUtil();
    void log(const GnssLocation& gnssLocation, const LocationCapabilitiesMask& capMask);
    void log(const std::vector<GnssSv>& gnssSvsVector);
    void log(uint64_t timestamp, uint32_t length, const char* nmea);
    void log(const GnssMeasurements& gnssMeasurements);
private:
    LogGnssLocation mLogLocation;
    LogGnssSv mLogSv;
    LogGnssNmea mLogNmea;
    LogGnssMeas mLogMeas;
};


}

#endif
