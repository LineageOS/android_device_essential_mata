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
#include "LCAReportLoggerUtil.h"

namespace location_client {

LCAReportLoggerUtil::LCAReportLoggerUtil():
        mLogLocation(nullptr),
        mLogSv(nullptr),
        mLogNmea(nullptr),
        mLogMeas(nullptr) {
    const char* libname = "liblocdiagiface.so";
    void* libHandle = nullptr;
    mLogLocation = (LogGnssLocation)dlGetSymFromLib(
            libHandle, libname, "LogGnssLocation");
    mLogSv = (LogGnssSv)dlGetSymFromLib(
            libHandle, libname, "LogGnssSv");
    mLogNmea = (LogGnssNmea)dlGetSymFromLib(
            libHandle, libname, "LogGnssNmea");
    mLogMeas = (LogGnssMeas)dlGetSymFromLib(
            libHandle, libname, "LogGnssMeas");
}

void LCAReportLoggerUtil::log(const GnssLocation& gnssLocation,
            const LocationCapabilitiesMask& capMask) {
    if (mLogLocation != nullptr) {
        mLogLocation(gnssLocation, capMask);
    }
}

void LCAReportLoggerUtil::log(const std::vector<GnssSv>& gnssSvsVector) {
    if (mLogSv != nullptr) {
        mLogSv(gnssSvsVector);
    }
}

void LCAReportLoggerUtil::log(
        uint64_t timestamp, uint32_t length, const char* nmea) {
    if (mLogNmea != nullptr) {
        mLogNmea(timestamp, length, nmea);
    }
}

void LCAReportLoggerUtil::log(const GnssMeasurements& gnssMeasurements) {
    if (mLogMeas != nullptr) {
        mLogMeas(gnssMeasurements);
    }
}
}
