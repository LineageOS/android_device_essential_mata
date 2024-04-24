/* Copyright (c) 2018-2020, The Linux Foundation. All rights reserved.
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

#define LOG_TAG "LocSvc_LocationClientApi"

#include <loc_cfg.h>
#include <cmath>
#include <LocationDataTypes.h>
#include <LocationClientApi.h>
#include <LocationClientApiImpl.h>
#include <LocationApiMsg.h>
#include <log_util.h>
#include <loc_log.h>
#include <gps_extended_c.h>

using std::string;

namespace location_client {

bool Geofence::operator==(Geofence& other) {
    return mGeofenceImpl != nullptr && other.mGeofenceImpl != nullptr &&
            mGeofenceImpl == other.mGeofenceImpl;
}

/******************************************************************************
LocationClientApi
******************************************************************************/
LocationClientApi::LocationClientApi(CapabilitiesCb capabitiescb) :
        mApiImpl(new LocationClientApiImpl(capabitiescb)) {
}

LocationClientApi::~LocationClientApi() {
    if (mApiImpl) {
        // two steps processes due to asynchronous message processing
        mApiImpl->destroy();
        // deletion of mApiImpl will be done after messages in the queue are processed
    }
}

bool LocationClientApi::startPositionSession(
        uint32_t intervalInMs,
        uint32_t distanceInMeters,
        LocationCb locationCallback,
        ResponseCb responseCallback) {

    loc_boot_kpi_marker("L - LCA ST-SPS %s %d",
            program_invocation_short_name, intervalInMs);
    //Input parameter check
    if (!locationCallback) {
        LOC_LOGe ("NULL locationCallback");
        return false;
    }

    if (!mApiImpl) {
        LOC_LOGe ("NULL mApiImpl");
        return false;
    }

    // callback functions
    ClientCallbacks cbs = {0};
    cbs.responsecb = responseCallback;
    cbs.locationcb = locationCallback;
    mApiImpl->updateCallbackFunctions(cbs);

    // callback masks
    LocationCallbacks callbacksOption = {0};
    callbacksOption.responseCb = [](::LocationError err, uint32_t id) {};
    // only register for trackingCb if distance is not 0
    if (distanceInMeters != 0) {
        callbacksOption.trackingCb = [](::Location n) {};
    } else {
        // for time based, register gnss location cb
        callbacksOption.gnssLocationInfoCb = [](::GnssLocationInfoNotification n) {};
    }
    mApiImpl->updateCallbacks(callbacksOption);

    // options
    LocationOptions locationOption;
    TrackingOptions trackingOption;
    locationOption.size = sizeof(locationOption);
    locationOption.minInterval = intervalInMs;
    locationOption.minDistance = distanceInMeters;
    trackingOption.setLocationOptions(locationOption);
    mApiImpl->startTracking(trackingOption);
    return true;
}

bool LocationClientApi::startPositionSession(
        uint32_t intervalInMs,
        const GnssReportCbs& gnssReportCallbacks,
        ResponseCb responseCallback) {

    loc_boot_kpi_marker("L - LCA EX-SPS %s %d",
            program_invocation_short_name, intervalInMs);

    if (!mApiImpl) {
        LOC_LOGe ("NULL mApiImpl");
        return false;
    }

    // callback functions
    ClientCallbacks cbs = {0};
    cbs.responsecb = responseCallback;
    cbs.gnssreportcbs = gnssReportCallbacks;
    mApiImpl->updateCallbackFunctions(cbs, REPORT_CB_GNSS_INFO);

    // callback masks
    LocationCallbacks callbacksOption = {0};
    callbacksOption.responseCb = [](::LocationError err, uint32_t id) {};
    if (gnssReportCallbacks.gnssLocationCallback) {
        callbacksOption.gnssLocationInfoCb = [](::GnssLocationInfoNotification n) {};
    }
    if (gnssReportCallbacks.gnssSvCallback) {
        callbacksOption.gnssSvCb = [](::GnssSvNotification n) {};
    }
    if (gnssReportCallbacks.gnssNmeaCallback) {
        callbacksOption.gnssNmeaCb = [](::GnssNmeaNotification n) {};
    }
    if (gnssReportCallbacks.gnssDataCallback) {
       callbacksOption.gnssDataCb = [] (::GnssDataNotification n) {};
    }
    if (gnssReportCallbacks.gnssMeasurementsCallback) {
        callbacksOption.gnssMeasurementsCb = [](::GnssMeasurementsNotification n) {};
    }
    mApiImpl->updateCallbacks(callbacksOption);

    // options
    LocationOptions locationOption;
    TrackingOptions trackingOption;
    locationOption.size = sizeof(locationOption);
    locationOption.minInterval = intervalInMs;
    locationOption.minDistance = 0;
    trackingOption.setLocationOptions(locationOption);
    mApiImpl->startTracking(trackingOption);
    return true;
}

bool LocationClientApi::startPositionSession(
        uint32_t intervalInMs,
        LocReqEngineTypeMask locEngReqMask,
        const EngineReportCbs& engReportCallbacks,
        ResponseCb responseCallback) {

    loc_boot_kpi_marker("L - LCA Fused-SPS %s %d",
            program_invocation_short_name, intervalInMs);
    if (!mApiImpl) {
        LOC_LOGe ("NULL mApiImpl");
        return false;
    }

    // callback functions
    ClientCallbacks cbs = {0};
    cbs.responsecb = responseCallback;
    cbs.engreportcbs = engReportCallbacks;
    mApiImpl->updateCallbackFunctions(cbs, REPORT_CB_ENGINE_INFO);

    // callback masks
    LocationCallbacks callbacksOption = {0};
    callbacksOption.responseCb = [](::LocationError err, uint32_t id) {};

    if (engReportCallbacks.engLocationsCallback) {
        callbacksOption.engineLocationsInfoCb =
                [](uint32_t count, ::GnssLocationInfoNotification* locArr) {};
    }
    if (engReportCallbacks.gnssSvCallback) {
        callbacksOption.gnssSvCb = [](::GnssSvNotification n) {};
    }
    if (engReportCallbacks.gnssNmeaCallback) {
        callbacksOption.gnssNmeaCb = [](::GnssNmeaNotification n) {};
    }
    if (engReportCallbacks.gnssDataCallback) {
       callbacksOption.gnssDataCb = [] (::GnssDataNotification n) {};
    }
    if (engReportCallbacks.gnssMeasurementsCallback) {
        callbacksOption.gnssMeasurementsCb = [](::GnssMeasurementsNotification n) {};
    }
    mApiImpl->updateCallbacks(callbacksOption);

    // options
    LocationOptions locationOption;
    TrackingOptions trackingOption;
    locationOption.size = sizeof(locationOption);
    locationOption.minInterval = intervalInMs;
    locationOption.minDistance = 0;
    locationOption.locReqEngTypeMask =(::LocReqEngineTypeMask)locEngReqMask;
    trackingOption.setLocationOptions(locationOption);

    mApiImpl->startTracking(trackingOption);
    return true;
}

void LocationClientApi::stopPositionSession() {
    if (mApiImpl) {
        mApiImpl->stopTracking(0);
    }
}

bool LocationClientApi::startTripBatchingSession(uint32_t minInterval, uint32_t tripDistance,
        BatchingCb batchingCallback, ResponseCb responseCallback) {
    //Input parameter check
    if (!batchingCallback) {
        LOC_LOGe ("NULL batchingCallback");
        return false;
    }

    if (!mApiImpl) {
        LOC_LOGe ("NULL mApiImpl");
        return false;
    }
    // callback functions
    ClientCallbacks cbs = {0};
    cbs.responsecb = responseCallback;
    cbs.batchingcb = batchingCallback;
    mApiImpl->updateCallbackFunctions(cbs);

    // callback masks
    LocationCallbacks callbacksOption = {0};
    callbacksOption.responseCb = [](::LocationError err, uint32_t id) {};
    callbacksOption.batchingCb = [](size_t count, ::Location* location,
            BatchingOptions batchingOptions) {};
    callbacksOption.batchingStatusCb = [](BatchingStatusInfo batchingStatus,
            std::list<uint32_t>& listOfcompletedTrips) {};
    mApiImpl->updateCallbacks(callbacksOption);

    LocationOptions locOption = {};
    locOption.size = sizeof(locOption);
    locOption.minInterval = minInterval;
    locOption.minDistance = tripDistance;
    locOption.mode = GNSS_SUPL_MODE_STANDALONE;

    BatchingOptions     batchOption = {};
    batchOption.size = sizeof(batchOption);
    batchOption.batchingMode = BATCHING_MODE_TRIP;
    batchOption.setLocationOptions(locOption);
    mApiImpl->startBatching(batchOption);
    return true;
}

bool LocationClientApi::startRoutineBatchingSession(uint32_t minInterval, uint32_t minDistance,
        BatchingCb batchingCallback, ResponseCb responseCallback) {
    //Input parameter check
    if (!batchingCallback) {
        LOC_LOGe ("NULL batchingCallback");
        return false;
    }

    if (!mApiImpl) {
        LOC_LOGe ("NULL mApiImpl");
        return false;
    }
    // callback functions
    ClientCallbacks cbs = {0};
    cbs.responsecb = responseCallback;
    cbs.batchingcb = batchingCallback;
    mApiImpl->updateCallbackFunctions(cbs);

    // callback masks
    LocationCallbacks callbacksOption = {0};
    callbacksOption.responseCb = [](::LocationError err, uint32_t id) {};
    callbacksOption.batchingCb = [](size_t count, ::Location* location,
            BatchingOptions batchingOptions) {};
    mApiImpl->updateCallbacks(callbacksOption);

    LocationOptions locOption = {};
    locOption.size = sizeof(locOption);
    locOption.minInterval = minInterval;
    locOption.minDistance = minDistance;
    locOption.mode = GNSS_SUPL_MODE_STANDALONE;

    BatchingOptions     batchOption = {};
    batchOption.size = sizeof(batchOption);
    batchOption.batchingMode = BATCHING_MODE_ROUTINE;
    batchOption.setLocationOptions(locOption);
    mApiImpl->startBatching(batchOption);
    return true;
}

void LocationClientApi::stopBatchingSession() {
    if (mApiImpl) {
        mApiImpl->stopBatching(0);
    }
}

void LocationClientApi::addGeofences(std::vector<Geofence>& geofences,
        GeofenceBreachCb gfBreachCb,
        CollectiveResponseCb responseCallback) {
    //Input parameter check
    if (!gfBreachCb) {
        LOC_LOGe ("NULL GeofenceBreachCb");
        return;
    }
    if (!mApiImpl) {
        LOC_LOGe ("NULL mApiImpl");
        return;
    }
    // callback functions
    ClientCallbacks cbs = {0};
    cbs.collectivecb = responseCallback;
    cbs.gfbreachcb = gfBreachCb;
    mApiImpl->updateCallbackFunctions(cbs);

    // callback masks
    LocationCallbacks callbacksOption = {0};
    callbacksOption.responseCb = [](LocationError err, uint32_t id) {};
    callbacksOption.collectiveResponseCb = [](size_t, LocationError*, uint32_t*) {};
    callbacksOption.geofenceBreachCb = [](GeofenceBreachNotification geofenceBreachNotification)
            {};
    mApiImpl->updateCallbacks(callbacksOption);
    size_t count = geofences.size();
    mApiImpl->mLastAddedClientIds.clear();
    if (count > 0) {
        GeofenceOption* gfOptions = (GeofenceOption*)malloc(sizeof(GeofenceOption) * count);
        GeofenceInfo* gfInfos = (GeofenceInfo*)malloc(sizeof(GeofenceInfo) * count);

        for (int i=0; i<count; ++i) {
            if (geofences[i].mGeofenceImpl) {
                continue;
            }
            gfOptions[i].breachTypeMask = geofences[i].getBreachType();
            gfOptions[i].responsiveness = geofences[i].getResponsiveness();
            gfOptions[i].dwellTime = geofences[i].getDwellTime();
            gfOptions[i].size = sizeof(gfOptions[i]);
            gfInfos[i].latitude = geofences[i].getLatitude();
            gfInfos[i].longitude = geofences[i].getLongitude();
            gfInfos[i].radius = geofences[i].getRadius();
            gfInfos[i].size = sizeof(gfInfos[i]);
            std::shared_ptr<GeofenceImpl> gfImpl(new GeofenceImpl(&geofences[i]));
            gfImpl->bindGeofence(&geofences[i]);
            mApiImpl->mLastAddedClientIds.push_back(gfImpl->getClientId());
            LOC_LOGd("Geofence LastAddedClientId: %d", gfImpl->getClientId());
            mApiImpl->addGeofenceMap(mApiImpl->mLastAddedClientIds[i], geofences[i]);
        }

        mApiImpl->addGeofences(geofences.size(), reinterpret_cast<GeofenceOption*>(gfOptions),
                reinterpret_cast<GeofenceInfo*>(gfInfos));
    }
}
void LocationClientApi::removeGeofences(std::vector<Geofence>& geofences) {
    if (!mApiImpl) {
        LOC_LOGe ("NULL mApiImpl");
        return;
    }
    size_t count = geofences.size();
    if (count > 0) {
        uint32_t* gfIds = (uint32_t*)malloc(sizeof(uint32_t) * count);
        for (int i=0; i<count; ++i) {
            if (!geofences[i].mGeofenceImpl) {
                LOC_LOGe ("Geofence not added yet");
                free(gfIds);
                return;
            }
            gfIds[i] = geofences[i].mGeofenceImpl->getClientId();
            LOC_LOGd("removeGeofences id : %d", gfIds[i]);
        }
        if (!mApiImpl->checkGeofenceMap(geofences.size(), gfIds)) {
            LOC_LOGe ("Wrong geofence IDs");
            free(gfIds);
            return;
        }
        mApiImpl->removeGeofences(count, gfIds);
    }
}
void LocationClientApi::modifyGeofences(std::vector<Geofence>& geofences) {
    if (!mApiImpl) {
        LOC_LOGe ("NULL mApiImpl");
        return;
    }
    size_t count = geofences.size();
    if (count > 0) {
        GeofenceOption* gfOptions = (GeofenceOption*)malloc(sizeof(GeofenceOption) * count);
        uint32_t* gfIds = (uint32_t*)malloc(sizeof(uint32_t) * count);
        for (int i=0; i<count; ++i) {
            gfOptions[i].breachTypeMask = geofences[i].getBreachType();
            gfOptions[i].responsiveness = geofences[i].getResponsiveness();
            gfOptions[i].dwellTime = geofences[i].getDwellTime();
            gfOptions[i].size = sizeof(gfOptions[i]);
            if (!geofences[i].mGeofenceImpl) {
                LOC_LOGe ("Geofence not added yet");
                free(gfIds);
                free(gfOptions);
                return;
            }
            gfIds[i] = geofences[i].mGeofenceImpl->getClientId();
            LOC_LOGd("modifyGeofences id : %d", gfIds[i]);
        }
        if (!mApiImpl->checkGeofenceMap(geofences.size(), gfIds)) {
            LOC_LOGe ("Wrong geofence IDs");
            free(gfIds);
            free(gfOptions);
            return;
        }
        mApiImpl->modifyGeofences(geofences.size(), const_cast<uint32_t*>(gfIds),
                reinterpret_cast<GeofenceOption*>(gfOptions));
    }
}

void LocationClientApi::pauseGeofences(std::vector<Geofence>& geofences) {
    if (!mApiImpl) {
        LOC_LOGe ("NULL mApiImpl");
        return;
    }
    size_t count = geofences.size();
    if (count > 0) {
        uint32_t* gfIds = (uint32_t*)malloc(sizeof(uint32_t) * count);
        for (int i=0; i<count; ++i) {
            if (!geofences[i].mGeofenceImpl) {
                LOC_LOGe ("Geofence not added yet");
                free(gfIds);
                return;
            }
            gfIds[i] = geofences[i].mGeofenceImpl->getClientId();
            LOC_LOGd("pauseGeofences id : %d", gfIds[i]);
        }
        if (!mApiImpl->checkGeofenceMap(geofences.size(), gfIds)) {
            LOC_LOGe ("Wrong geofence IDs");
            free(gfIds);
            return;
        }
        mApiImpl->pauseGeofences(count, gfIds);
    }
}

void LocationClientApi::resumeGeofences(std::vector<Geofence>& geofences) {
    if (!mApiImpl) {
        LOC_LOGe ("NULL mApiImpl");
        return;
    }
    size_t count = geofences.size();
    if (count > 0) {
        uint32_t* gfIds = (uint32_t*)malloc(sizeof(uint32_t) * count);
        for (int i=0; i<count; ++i) {
            if (!geofences[i].mGeofenceImpl) {
                LOC_LOGe ("Geofence not added yet");
                free(gfIds);
                return;
            }
            gfIds[i] = geofences[i].mGeofenceImpl->getClientId();
            LOC_LOGd("resumeGeofences id : %d", gfIds[i]);
        }
        if (!mApiImpl->checkGeofenceMap(geofences.size(), gfIds)) {
            LOC_LOGe ("Wrong geofence IDs");
            free(gfIds);
            return;
        }
        mApiImpl->resumeGeofences(count, gfIds);
    }
}

void LocationClientApi::updateNetworkAvailability(bool available) {
    if (mApiImpl) {
        mApiImpl->updateNetworkAvailability(available);
    }
}

void LocationClientApi::getGnssEnergyConsumed(
        GnssEnergyConsumedCb gnssEnergyConsumedCallback,
        ResponseCb responseCallback) {

    if (!gnssEnergyConsumedCallback) {
        if (responseCallback) {
            responseCallback(LOCATION_RESPONSE_PARAM_INVALID);
        }
    } else if (mApiImpl) {
        mApiImpl->getGnssEnergyConsumed(gnssEnergyConsumedCallback,
                                        responseCallback);
    } else {
        LOC_LOGe ("NULL mApiImpl");
    }
}

void LocationClientApi::updateLocationSystemInfoListener(
    LocationSystemInfoCb locSystemInfoCallback,
    ResponseCb responseCallback) {

    if (mApiImpl) {
        mApiImpl->updateLocationSystemInfoListener(
            locSystemInfoCallback, responseCallback);
    } else {
        LOC_LOGe ("NULL mApiImpl");
    }
}

uint16_t LocationClientApi::getYearOfHw() {
    if (mApiImpl) {
        return mApiImpl->getYearOfHw();
    } else {
        LOC_LOGe ("NULL mApiImpl");
        return 0;
    }
}

void LocationClientApi::getSingleTerrestrialPosition(
        uint32_t timeoutMsec, TerrestrialTechnologyMask techMask,
        float horQoS, LocationCb terrestrialPositionCallback,
        ResponseCb responseCallback) {

    LOC_LOGd("timeout msec = %u, horQoS = %f,"
             "techMask = 0x%x", timeoutMsec, horQoS, techMask);

    if ((timeoutMsec == 0) || (techMask != TERRESTRIAL_TECH_GTP_WWAN) ||
        (horQoS != 0.0)) {
        LOC_LOGe("invalid parameter: timeout %d, tech mask 0x%x, horQoS %f",
                 timeoutMsec, techMask, horQoS);
        if (responseCallback) {
            responseCallback(LOCATION_RESPONSE_PARAM_INVALID);
        }
    } else if (mApiImpl) {
        mApiImpl->getSingleTerrestrialPos(timeoutMsec, ::TERRESTRIAL_TECH_GTP_WWAN, horQoS,
                                          terrestrialPositionCallback, responseCallback);
    } else {
        LOC_LOGe ("NULL mApiImpl");
    }
}

// ============ below Section implements toString() methods of data structs ==============
static string maskToVals(uint64_t mask, int64_t baseNum) {
    string out;
    while (mask > 0) {
        baseNum += log2(loc_get_least_bit(mask));
        out += baseNum + " ";
    }
    return out;
}

// LocationCapabilitiesMask
DECLARE_TBL(LocationCapabilitiesMask) = {
    {LOCATION_CAPS_TIME_BASED_TRACKING_BIT, "TIME_BASED_TRACKING"},
    {LOCATION_CAPS_TIME_BASED_BATCHING_BIT, "TIME_BASED_BATCHING"},
    {LOCATION_CAPS_DISTANCE_BASED_TRACKING_BIT, "DIST_BASED_TRACKING"},
    {LOCATION_CAPS_DISTANCE_BASED_BATCHING_BIT, "DIST_BASED_BATCHING"},
    {LOCATION_CAPS_GEOFENCE_BIT, "GEOFENCE"},
    {LOCATION_CAPS_OUTDOOR_TRIP_BATCHING_BIT, "OUTDOOR_TRIP_BATCHING"},
    {LOCATION_CAPS_GNSS_MEASUREMENTS_BIT, "GNSS_MEASUREMENTS"},
    {LOCATION_CAPS_CONSTELLATION_ENABLEMENT_BIT, "CONSTELLATION_ENABLE"},
    {LOCATION_CAPS_CARRIER_PHASE_BIT, "CARRIER_PHASE"},
    {LOCATION_CAPS_SV_POLYNOMIAL_BIT, "SV_POLY"},
    {LOCATION_CAPS_QWES_GNSS_SINGLE_FREQUENCY, "GNSS_SINGLE_FREQ"},
    {LOCATION_CAPS_QWES_GNSS_MULTI_FREQUENCY, "GNSS_MULTI_FREQ"},
    {LOCATION_CAPS_QWES_VPE, "VPE"},
    {LOCATION_CAPS_QWES_CV2X_LOCATION_BASIC, "CV2X_LOC_BASIC"},
    {LOCATION_CAPS_QWES_CV2X_LOCATION_PREMIUM, "CV2X_LOC_PREMIUM"},
    {LOCATION_CAPS_QWES_PPE, "PPE"},
    {LOCATION_CAPS_QWES_QDR2, "QDR2"},
    {LOCATION_CAPS_QWES_QDR3, "QDR3"}
};
// GnssSvOptionsMask
DECLARE_TBL(GnssSvOptionsMask) = {
    {GNSS_SV_OPTIONS_HAS_EPHEMER_BIT, "EPH"},
    {GNSS_SV_OPTIONS_HAS_ALMANAC_BIT, "ALM"},
    {GNSS_SV_OPTIONS_USED_IN_FIX_BIT, "USED_IN_FIX"},
    {GNSS_SV_OPTIONS_HAS_CARRIER_FREQUENCY_BIT, "CARRIER_FREQ"},
    {GNSS_SV_OPTIONS_HAS_GNSS_SIGNAL_TYPE_BIT, "SIG_TYPES"}
};
// LocationFlagsMask
DECLARE_TBL(LocationFlagsMask) = {
    {LOCATION_HAS_LAT_LONG_BIT, "LAT_LON"},
    {LOCATION_HAS_ALTITUDE_BIT, "ALT"},
    {LOCATION_HAS_SPEED_BIT, "SPEED"},
    {LOCATION_HAS_BEARING_BIT, "FEARING"},
    {LOCATION_HAS_ACCURACY_BIT, "ACCURACY"},
    {LOCATION_HAS_VERTICAL_ACCURACY_BIT, "VERT_ACCURACY"},
    {LOCATION_HAS_SPEED_ACCURACY_BIT, "SPEED_ACCURACY"},
    {LOCATION_HAS_BEARING_ACCURACY_BIT, "BEARING_ACCURACY"},
    {LOCATION_HAS_BEARING_ACCURACY_BIT, "TS"}
};
// LocationTechnologyMask
DECLARE_TBL(LocationTechnologyMask) = {
    {LOCATION_TECHNOLOGY_GNSS_BIT, "GNSS"},
    {LOCATION_TECHNOLOGY_CELL_BIT, "CELL"},
    {LOCATION_TECHNOLOGY_WIFI_BIT, "WIFI"},
    {LOCATION_TECHNOLOGY_SENSORS_BIT, "SENSOR"},
    {LOCATION_TECHNOLOGY_REFERENCE_LOCATION_BIT, "REF_LOC"},
    {LOCATION_TECHNOLOGY_INJECTED_COARSE_POSITION_BIT, "CPI"},
    {LOCATION_TECHNOLOGY_AFLT_BIT, "AFLT"},
    {LOCATION_TECHNOLOGY_HYBRID_BIT, "HYBRID"},
    {LOCATION_TECHNOLOGY_PPE_BIT, "PPE"}
};
// GnssLocationNavSolutionMask
DECLARE_TBL(GnssLocationNavSolutionMask) = {
    {LOCATION_SBAS_CORRECTION_IONO_BIT, "SBAS_CORR_IONO"},
    {LOCATION_SBAS_CORRECTION_FAST_BIT, "SBAS_CORR_FAST"},
    {LOCATION_SBAS_CORRECTION_LONG_BIT, "SBAS_CORR_LON"},
    {LOCATION_SBAS_INTEGRITY_BIT, "SBAS_INTEGRITY"},
    {LOCATION_NAV_CORRECTION_DGNSS_BIT, "NAV_CORR_DGNSS"},
    {LOCATION_NAV_CORRECTION_RTK_BIT, "NAV_CORR_RTK"},
    {LOCATION_NAV_CORRECTION_PPP_BIT, "NAV_CORR_PPP"},
    {LOCATION_NAV_CORRECTION_RTK_FIXED_BIT, "NAV_CORR_RTK_FIXED"},
    {LOCATION_NAV_CORRECTION_ONLY_SBAS_CORRECTED_SV_USED_BIT,
            "NAV_CORR_ONLY_SBAS_CORRECTED_SV_USED"}
};
// GnssLocationPosDataMask
DECLARE_TBL(GnssLocationPosDataMask) = {
    {LOCATION_NAV_DATA_HAS_LONG_ACCEL_BIT, "LONG_ACCEL"},
    {LOCATION_NAV_DATA_HAS_LAT_ACCEL_BIT, "LAT_ACCEL"},
    {LOCATION_NAV_DATA_HAS_VERT_ACCEL_BIT, "VERT_ACCEL"},
    {LOCATION_NAV_DATA_HAS_YAW_RATE_BIT, "YAW_RATE"},
    {LOCATION_NAV_DATA_HAS_PITCH_BIT, "PITCH"},
    {LOCATION_NAV_DATA_HAS_LONG_ACCEL_UNC_BIT, "LONG_ACCEL_UNC"},
    {LOCATION_NAV_DATA_HAS_LAT_ACCEL_UNC_BIT, "LAT_ACCEL_UNC"},
    {LOCATION_NAV_DATA_HAS_VERT_ACCEL_UNC_BIT, "VERT_ACCEL_UNC"},
    {LOCATION_NAV_DATA_HAS_YAW_RATE_UNC_BIT, "YAW_RATE_UNC"},
    {LOCATION_NAV_DATA_HAS_PITCH_UNC_BIT, "PITCH_UNC"}
};
// GnssSignalTypeMask
DECLARE_TBL(GnssSignalTypeMask) = {
    {GNSS_SIGNAL_GPS_L1CA_BIT, "GPS_L1CA"},
    {GNSS_SIGNAL_GPS_L1C_BIT, "GPS_L1C"},
    {GNSS_SIGNAL_GPS_L2_BIT, "GPS_L2"},
    {GNSS_SIGNAL_GPS_L5_BIT, "GPS_L5"},
    {GNSS_SIGNAL_GLONASS_G1_BIT, "GLO_G1"},
    {GNSS_SIGNAL_GLONASS_G2_BIT, "GLO_G2"},
    {GNSS_SIGNAL_GALILEO_E1_BIT, "GAL_E1"},
    {GNSS_SIGNAL_GALILEO_E5A_BIT, "GAL_E5A"},
    {GNSS_SIGNAL_GALILEO_E5B_BIT, "GAL_E5B"},
    {GNSS_SIGNAL_BEIDOU_B1_BIT, "BDS_B1"},
    {GNSS_SIGNAL_BEIDOU_B2_BIT, "BDS_B2"},
    {GNSS_SIGNAL_QZSS_L1CA_BIT, "QZSS_L1CA"},
    {GNSS_SIGNAL_QZSS_L1S_BIT, "QZSS_L1S"},
    {GNSS_SIGNAL_QZSS_L2_BIT, "QZSS_L2"},
    {GNSS_SIGNAL_QZSS_L2_BIT, "QZSS_L5"},
    {GNSS_SIGNAL_SBAS_L1_BIT, "SBAS_L1"},
    {GNSS_SIGNAL_BEIDOU_B1I_BIT, "BDS_B1I"},
    {GNSS_SIGNAL_BEIDOU_B1C_BIT, "BDS_B1C"},
    {GNSS_SIGNAL_BEIDOU_B2I_BIT, "BDS_B2I"},
    {GNSS_SIGNAL_BEIDOU_B2AI_BIT, "BDS_B2AI"},
    {GNSS_SIGNAL_NAVIC_L5_BIT, "NAVIC_L5"},
    {GNSS_SIGNAL_BEIDOU_B2AQ_BIT, "BDS_B2AQ"}
};
// GnssSignalTypes
DECLARE_TBL(GnssSignalTypes) = {
    {GNSS_SIGNAL_TYPE_GPS_L1CA, "GPS_L1CA"},
    {GNSS_SIGNAL_TYPE_GPS_L1C, "GPS_L1C"},
    {GNSS_SIGNAL_TYPE_GPS_L2C_L, "GPS_L2_L"},
    {GNSS_SIGNAL_TYPE_GPS_L5_Q, "GPS_L5_Q"},
    {GNSS_SIGNAL_TYPE_GLONASS_G1, "GLO_G1"},
    {GNSS_SIGNAL_TYPE_GLONASS_G2, "GLO_G2"},
    {GNSS_SIGNAL_TYPE_GALILEO_E1_C, "GAL_E1_C"},
    {GNSS_SIGNAL_TYPE_GALILEO_E5A_Q, "GAL_E5A_Q"},
    {GNSS_SIGNAL_TYPE_GALILEO_E5B_Q, "GAL_E5B_Q"},
    {GNSS_SIGNAL_TYPE_BEIDOU_B1_I, "BDS_B1_I"},
    {GNSS_SIGNAL_TYPE_BEIDOU_B1C, "BDS_B1C"},
    {GNSS_SIGNAL_TYPE_BEIDOU_B1C, "BDS_B2I"},
    {GNSS_SIGNAL_TYPE_BEIDOU_B2A_I, "BDS_B2AI"},
    {GNSS_SIGNAL_TYPE_QZSS_L1CA, "QZSS_L1CA"},
    {GNSS_SIGNAL_TYPE_QZSS_L1S, "QZSS_L1S"},
    {GNSS_SIGNAL_TYPE_QZSS_L2C_L, "QZSS_L2C_L"},
    {GNSS_SIGNAL_TYPE_QZSS_L5_Q, "QZSS_L5"},
    {GNSS_SIGNAL_TYPE_SBAS_L1_CA, "SBAS_L1_CA"},
    {GNSS_SIGNAL_TYPE_NAVIC_L5, "NAVIC_L5"},
    {GNSS_SIGNAL_TYPE_BEIDOU_B2A_Q, "BDS_B2AQ"}
};
// GnssSvType
DECLARE_TBL(GnssSvType) = {
    {GNSS_SV_TYPE_UNKNOWN, "UNKNOWN"},
    {GNSS_SV_TYPE_GPS, "GPS"},
    {GNSS_SV_TYPE_SBAS, "SBAS"},
    {GNSS_SV_TYPE_GLONASS, "GLO"},
    {GNSS_SV_TYPE_QZSS, "QZSS"},
    {GNSS_SV_TYPE_BEIDOU, "BDS"},
    {GNSS_SV_TYPE_GALILEO, "GAL"},
    {GNSS_SV_TYPE_NAVIC, "NAVIC"}
};
// Gnss_LocSvSystemEnumType
DECLARE_TBL(Gnss_LocSvSystemEnumType) = {
    {GNSS_LOC_SV_SYSTEM_GPS,     "GPS"},
    {GNSS_LOC_SV_SYSTEM_GALILEO, "GAL"},
    {GNSS_LOC_SV_SYSTEM_SBAS,    "SBAS"},
    {GNSS_LOC_SV_SYSTEM_GLONASS, "GLO"},
    {GNSS_LOC_SV_SYSTEM_BDS,     "BDS"},
    {GNSS_LOC_SV_SYSTEM_QZSS,    "QZSS"},
    {GNSS_LOC_SV_SYSTEM_NAVIC,   "NAVIC"}
};
// GnssLocationInfoFlagMask
DECLARE_TBL(GnssLocationInfoFlagMask) = {
    {GNSS_LOCATION_INFO_ALTITUDE_MEAN_SEA_LEVEL_BIT, "ALT_SEA_LEVEL"},
    {GNSS_LOCATION_INFO_ALTITUDE_MEAN_SEA_LEVEL_BIT, "DOP"},
    {GNSS_LOCATION_INFO_MAGNETIC_DEVIATION_BIT, "MAG_DEV"},
    {GNSS_LOCATION_INFO_HOR_RELIABILITY_BIT, "HOR_RELIAB"},
    {GNSS_LOCATION_INFO_VER_RELIABILITY_BIT, "VER_RELIAB"},
    {GNSS_LOCATION_INFO_HOR_ACCURACY_ELIP_SEMI_MAJOR_BIT, "HOR_ACCU_ELIP_SEMI_MAJOR"},
    {GNSS_LOCATION_INFO_HOR_ACCURACY_ELIP_SEMI_MINOR_BIT, "HOR_ACCU_ELIP_SEMI_MINOR"},
    {GNSS_LOCATION_INFO_HOR_ACCURACY_ELIP_AZIMUTH_BIT, "HOR_ACCU_ELIP_AZIMUTH"},
    {GNSS_LOCATION_INFO_GNSS_SV_USED_DATA_BIT, "GNSS_SV_USED"},
    {GNSS_LOCATION_INFO_NAV_SOLUTION_MASK_BIT, "NAV_SOLUTION"},
    {GNSS_LOCATION_INFO_POS_TECH_MASK_BIT, "POS_TECH"},
    {GNSS_LOCATION_INFO_SV_SOURCE_INFO_BIT, "SV_SOURCE"},
    {GNSS_LOCATION_INFO_POS_DYNAMICS_DATA_BIT, "POS_DYNAMICS"},
    {GNSS_LOCATION_INFO_EXT_DOP_BIT, "EXT_DOP"},
    {GNSS_LOCATION_INFO_NORTH_STD_DEV_BIT, "NORTH_STD_DEV"},
    {GNSS_LOCATION_INFO_EAST_STD_DEV_BIT, "EAST_STD_DEV"},
    {GNSS_LOCATION_INFO_EAST_STD_DEV_BIT, "NORTH_VEL"},
    {GNSS_LOCATION_INFO_EAST_VEL_BIT, "EAST_VEL"},
    {GNSS_LOCATION_INFO_UP_VEL_BIT, "UP_VEL"},
    {GNSS_LOCATION_INFO_NORTH_VEL_UNC_BIT, "NORTH_VEL_UNC"},
    {GNSS_LOCATION_INFO_EAST_VEL_UNC_BIT, "EAST_VEL_UNC"},
    {GNSS_LOCATION_INFO_UP_VEL_UNC_BIT, "UP_VEL_UNC"},
    {GNSS_LOCATION_INFO_LEAP_SECONDS_BIT, "LEAP_SECONDS"},
    {GNSS_LOCATION_INFO_TIME_UNC_BIT, "TIME_UNC"},
    {GNSS_LOCATION_INFO_NUM_SV_USED_IN_POSITION_BIT, "NUM_SV_USED_IN_FIX"},
    {GNSS_LOCATION_INFO_CALIBRATION_CONFIDENCE_PERCENT_BIT, "CAL_CONF_PRECENT"},
    {GNSS_LOCATION_INFO_CALIBRATION_STATUS_BIT, "CAL_STATUS"},
    {GNSS_LOCATION_INFO_OUTPUT_ENG_TYPE_BIT, "OUTPUT_ENG_TYPE"},
    {GNSS_LOCATION_INFO_OUTPUT_ENG_MASK_BIT, "OUTPUT_ENG_MASK"},
    {GNSS_LOCATION_INFO_CONFORMITY_INDEX_BIT, "CONFORMITY_INDEX"}
};
// LocationReliability
DECLARE_TBL(LocationReliability) = {
    {LOCATION_RELIABILITY_NOT_SET, "NOT_SET"},
    {LOCATION_RELIABILITY_VERY_LOW, "VERY_LOW"},
    {LOCATION_RELIABILITY_LOW, "LOW"},
    {LOCATION_RELIABILITY_MEDIUM, "MED"},
    {LOCATION_RELIABILITY_HIGH, "HI"}
};
// GnssSystemTimeStructTypeFlags
DECLARE_TBL(GnssSystemTimeStructTypeFlags) = {
    {GNSS_SYSTEM_TIME_WEEK_VALID, "WEEK"},
    {GNSS_SYSTEM_TIME_WEEK_MS_VALID, "WEEK_MS"},
    {GNSS_SYSTEM_CLK_TIME_BIAS_VALID, "CLK_BIAS"},
    {GNSS_SYSTEM_CLK_TIME_BIAS_UNC_VALID, "CLK_BIAS_UNC"},
    {GNSS_SYSTEM_REF_FCOUNT_VALID, "REF_COUNT"},
    {GNSS_SYSTEM_NUM_CLOCK_RESETS_VALID, "CLK_RESET"}
};
// GnssGloTimeStructTypeFlags
DECLARE_TBL(GnssGloTimeStructTypeFlags) = {
    {GNSS_CLO_DAYS_VALID, "DAYS"},
    {GNSS_GLO_MSEC_VALID, "MS"},
    {GNSS_GLO_CLK_TIME_BIAS_VALID, "CLK_BIAS"},
    {GNSS_GLO_CLK_TIME_BIAS_UNC_VALID, "CLK_BIAS_UNC"},
    {GNSS_GLO_REF_FCOUNT_VALID, "REF_COUNT"},
    {GNSS_GLO_NUM_CLOCK_RESETS_VALID, "CLK_RESET"},
    {GNSS_GLO_FOUR_YEAR_VALID, "YEAR"}
};
// DrCalibrationStatusMask
DECLARE_TBL(DrCalibrationStatusMask) = {
    {DR_ROLL_CALIBRATION_NEEDED, "ROLL"},
    {DR_PITCH_CALIBRATION_NEEDED, "PITCH"},
    {DR_YAW_CALIBRATION_NEEDED, "YAW"},
    {DR_ODO_CALIBRATION_NEEDED, "ODO"},
    {DR_GYRO_CALIBRATION_NEEDED, "GYRO"}
};
// LocReqEngineTypeMask
DECLARE_TBL(LocReqEngineTypeMask) = {
    {LOC_REQ_ENGINE_FUSED_BIT, "FUSED"},
    {LOC_REQ_ENGINE_SPE_BIT, "SPE"},
    {LOC_REQ_ENGINE_PPE_BIT, "PPE"},
    {LOC_REQ_ENGINE_VPE_BIT, "VPE"}
};
// LocOutputEngineType
DECLARE_TBL(LocOutputEngineType) = {
    {LOC_OUTPUT_ENGINE_FUSED, "FUSED"},
    {LOC_OUTPUT_ENGINE_SPE, "SPE"},
    {LOC_OUTPUT_ENGINE_PPE, "PPE"},
    {LOC_OUTPUT_ENGINE_VPE, "VPE"},
    {LOC_OUTPUT_ENGINE_COUNT, "COUNT"}
};
// PositioningEngineMask
DECLARE_TBL(PositioningEngineMask) = {
    {STANDARD_POSITIONING_ENGINE, "SPE"},
    {DEAD_RECKONING_ENGINE, "DRE"},
    {PRECISE_POSITIONING_ENGINE, "PPE"},
    {VP_POSITIONING_ENGINE, "VPE"}
};
// GnssDataMask
DECLARE_TBL(GnssDataMask) = {
    {GNSS_DATA_JAMMER_IND_BIT, "JAMMER"},
    {GNSS_DATA_AGC_BIT, "AGC"}
};
// GnssMeasurementsDataFlagsMask
DECLARE_TBL(GnssMeasurementsDataFlagsMask) = {
    {GNSS_MEASUREMENTS_DATA_SV_ID_BIT, "svId"},
    {GNSS_MEASUREMENTS_DATA_SV_TYPE_BIT, "svType"},
    {GNSS_MEASUREMENTS_DATA_STATE_BIT, "stateMask"},
    {GNSS_MEASUREMENTS_DATA_RECEIVED_SV_TIME_BIT, "receivedSvTimeNs"},
    {GNSS_MEASUREMENTS_DATA_RECEIVED_SV_TIME_UNCERTAINTY_BIT, "receivedSvTimeUncertaintyNs"},
    {GNSS_MEASUREMENTS_DATA_CARRIER_TO_NOISE_BIT, "carrierToNoiseDbHz"},
    {GNSS_MEASUREMENTS_DATA_PSEUDORANGE_RATE_BIT, "pseudorangeRateMps"},
    {GNSS_MEASUREMENTS_DATA_PSEUDORANGE_RATE_UNCERTAINTY_BIT, "pseudorangeRateUncertaintyMps"},
    {GNSS_MEASUREMENTS_DATA_ADR_STATE_BIT, "adrStateMask"},
    {GNSS_MEASUREMENTS_DATA_ADR_BIT, "adrMeters"},
    {GNSS_MEASUREMENTS_DATA_ADR_UNCERTAINTY_BIT, "adrUncertaintyMeters"},
    {GNSS_MEASUREMENTS_DATA_CARRIER_FREQUENCY_BIT, "carrierFrequencyHz"},
    {GNSS_MEASUREMENTS_DATA_CARRIER_CYCLES_BIT, "carrierCycles"},
    {GNSS_MEASUREMENTS_DATA_CARRIER_PHASE_BIT, "carrierPhase"},
    {GNSS_MEASUREMENTS_DATA_CARRIER_PHASE_UNCERTAINTY_BIT, "carrierPhaseUncertainty"},
    {GNSS_MEASUREMENTS_DATA_MULTIPATH_INDICATOR_BIT, "multipathIndicator"},
    {GNSS_MEASUREMENTS_DATA_SIGNAL_TO_NOISE_RATIO_BIT, "signalToNoiseRatioDb"},
    {GNSS_MEASUREMENTS_DATA_AUTOMATIC_GAIN_CONTROL_BIT, "agcLevelDb"},
    {GNSS_MEASUREMENTS_DATA_FULL_ISB_BIT, "interSignalBiasNs"},
    {GNSS_MEASUREMENTS_DATA_FULL_ISB_UNCERTAINTY_BIT, "interSignalBiasUncertaintyNs"},
    {GNSS_MEASUREMENTS_DATA_CYCLE_SLIP_COUNT_BIT, "cycleSlipCount"}
};
// GnssMeasurementsStateMask
DECLARE_TBL(GnssMeasurementsStateMask) = {
    {GNSS_MEASUREMENTS_STATE_CODE_LOCK_BIT, "CODE_LOCK"},
    {GNSS_MEASUREMENTS_STATE_BIT_SYNC_BIT, "BIT_SYNC"},
    {GNSS_MEASUREMENTS_STATE_SUBFRAME_SYNC_BIT, "SUBFRAME_SYNC"},
    {GNSS_MEASUREMENTS_STATE_TOW_DECODED_BIT, "TOW_DECODED"},
    {GNSS_MEASUREMENTS_STATE_MSEC_AMBIGUOUS_BIT, "MSEC_AMBIGUOUS"},
    {GNSS_MEASUREMENTS_STATE_SYMBOL_SYNC_BIT, "SYMBOL_SYNC"},
    {GNSS_MEASUREMENTS_STATE_GLO_STRING_SYNC_BIT, "GLO_STRING_SYNC"},
    {GNSS_MEASUREMENTS_STATE_GLO_TOD_DECODED_BIT, "GLO_TOD_DECODED"},
    {GNSS_MEASUREMENTS_STATE_BDS_D2_BIT_SYNC_BIT, "BDS_D2_BIT_SYNC"},
    {GNSS_MEASUREMENTS_STATE_BDS_D2_SUBFRAME_SYNC_BIT, "BDS_D2_SUBFRAME_SYNC"},
    {GNSS_MEASUREMENTS_STATE_GAL_E1BC_CODE_LOCK_BIT, "GAL_E1BC_CODE_LOCK"},
    {GNSS_MEASUREMENTS_STATE_GAL_E1C_2ND_CODE_LOCK_BIT, "GAL_E1C_2ND_CODE_LOCK"},
    {GNSS_MEASUREMENTS_STATE_GAL_E1B_PAGE_SYNC_BIT, "GAL_E1B_PAGE_SYNC"},
    {GNSS_MEASUREMENTS_STATE_SBAS_SYNC_BIT, "SBAS_SYNC"}
};
// GnssMeasurementsAdrStateMask
DECLARE_TBL(GnssMeasurementsAdrStateMask) = {
    {GNSS_MEASUREMENTS_ACCUMULATED_DELTA_RANGE_STATE_VALID_BIT, "VALID"},
    {GNSS_MEASUREMENTS_ACCUMULATED_DELTA_RANGE_STATE_RESET_BIT, "RESET"},
    {GNSS_MEASUREMENTS_ACCUMULATED_DELTA_RANGE_STATE_CYCLE_SLIP_BIT, "CYCLE_SLIP"}
};
// GnssMeasurementsMultipathIndicator
DECLARE_TBL(GnssMeasurementsMultipathIndicator) = {
    {GNSS_MEASUREMENTS_MULTIPATH_INDICATOR_UNKNOWN, "UNKNOWN"},
    {GNSS_MEASUREMENTS_MULTIPATH_INDICATOR_PRESENT, "PRESENT"},
    {GNSS_MEASUREMENTS_MULTIPATH_INDICATOR_NOT_PRESENT, "NOT_PRESENT"}
};
// GnssMeasurementsClockFlagsMask
DECLARE_TBL(GnssMeasurementsClockFlagsMask) = {
    {GNSS_MEASUREMENTS_CLOCK_FLAGS_LEAP_SECOND_BIT, "LEAP_SECOND"},
    {GNSS_MEASUREMENTS_CLOCK_FLAGS_TIME_BIT, "TIME"},
    {GNSS_MEASUREMENTS_CLOCK_FLAGS_TIME_UNCERTAINTY_BIT, "TIME_UNC"},
    {GNSS_MEASUREMENTS_CLOCK_FLAGS_FULL_BIAS_BIT, "FULL_BIAS"},
    {GNSS_MEASUREMENTS_CLOCK_FLAGS_BIAS_BIT, "BIAS"},
    {GNSS_MEASUREMENTS_CLOCK_FLAGS_BIAS_BIT, "BIAS_UNC"},
    {GNSS_MEASUREMENTS_CLOCK_FLAGS_DRIFT_BIT, "DRIFT"},
    {GNSS_MEASUREMENTS_CLOCK_FLAGS_DRIFT_UNCERTAINTY_BIT, "DRIFT_UNC"},
    {GNSS_MEASUREMENTS_CLOCK_FLAGS_HW_CLOCK_DISCONTINUITY_COUNT_BIT, "HW_CLK_DISCONTINUITY_CNT"}
};
// LeapSecondSysInfoMask
DECLARE_TBL(LeapSecondSysInfoMask) = {
    {LEAP_SECOND_SYS_INFO_CURRENT_LEAP_SECONDS_BIT, "CUR_LEAP_SEC"},
    {LEAP_SECOND_SYS_INFO_LEAP_SECOND_CHANGE_BIT, "LEAP_SEC_CHANGE"}
};
// LocationSystemInfoMask
DECLARE_TBL(LocationSystemInfoMask) = {
    {LOC_SYS_INFO_LEAP_SECOND, "LEAP_SEC"}
};

// LocationSystemInfoMask
DECLARE_TBL(DrSolutionStatusMask) = {
    {DR_SOLUTION_STATUS_VEHICLE_SENSOR_SPEED_INPUT_DETECTED, "VEHICLE_SENSOR_SPEED_INPUT_DETECTED"},
    {DR_SOLUTION_STATUS_VEHICLE_SENSOR_SPEED_INPUT_USED, "VEHICLE_SENSOR_SPEED_INPUT_USED"}
};

string LocationClientApi::capabilitiesToString(LocationCapabilitiesMask capabMask) {
    string out;
    out.reserve(256);

    out += FIELDVAL_MASK(capabMask, LocationCapabilitiesMask_tbl);

    return out;
}

// LocSessionStatus
DECLARE_TBL(LocSessionStatus) = {
    {LOC_SESS_SUCCESS, "LOC_SESS_SUCCESS"},
    {LOC_SESS_INTERMEDIATE, "LOC_SESS_INTERMEDIATE"},
    {LOC_SESS_FAILURE, "LOC_SESS_FAILURE" }
};

string GnssLocationSvUsedInPosition::toString() const {
    string out;
    out.reserve(256);

    // gpsSvUsedIdsMask
    out += FIELDVAL_HEX(gpsSvUsedIdsMask);
    out += loc_parenthesize(maskToVals(gpsSvUsedIdsMask, GPS_SV_PRN_MIN)) + "\n";

    out += FIELDVAL_HEX(gloSvUsedIdsMask);
    out += loc_parenthesize(maskToVals(gloSvUsedIdsMask, GLO_SV_PRN_MIN)) + "\n";

    out += FIELDVAL_HEX(galSvUsedIdsMask);
    out += loc_parenthesize(maskToVals(galSvUsedIdsMask, GAL_SV_PRN_MIN)) + "\n";

    out += FIELDVAL_HEX(bdsSvUsedIdsMask);
    out += loc_parenthesize(maskToVals(bdsSvUsedIdsMask, BDS_SV_PRN_MIN)) + "\n";

    out += FIELDVAL_HEX(qzssSvUsedIdsMask);
    out += loc_parenthesize(maskToVals(qzssSvUsedIdsMask, QZSS_SV_PRN_MIN)) + "\n";

    out += FIELDVAL_HEX(navicSvUsedIdsMask);
    out += loc_parenthesize(maskToVals(navicSvUsedIdsMask, NAVIC_SV_PRN_MIN)) + "\n";

    return out;
}

string GnssMeasUsageInfo::toString() const {
    string out;
    out.reserve(256);

    out += FIELDVAL_ENUM(gnssConstellation, Gnss_LocSvSystemEnumType_tbl);
    out += FIELDVAL_DEC(gnssSvId);
    out += FIELDVAL_MASK(gnssSignalType, GnssSignalTypeMask_tbl);

    return out;
}

string GnssLocationPositionDynamics::toString() const {
    string out;
    out.reserve(256);

    out += FIELDVAL_MASK(bodyFrameDataMask, GnssLocationPosDataMask_tbl);
    out += FIELDVAL_DEC(longAccel);
    out += FIELDVAL_DEC(latAccel);
    out += FIELDVAL_DEC(vertAccel);
    out += FIELDVAL_DEC(longAccelUnc);
    out += FIELDVAL_DEC(latAccelUnc);
    out += FIELDVAL_DEC(vertAccelUnc);
    out += FIELDVAL_DEC(pitch);
    out += FIELDVAL_DEC(pitchUnc);
    out += FIELDVAL_DEC(pitchRate);
    out += FIELDVAL_DEC(pitchRateUnc);
    out += FIELDVAL_DEC(roll);
    out += FIELDVAL_DEC(rollUnc);
    out += FIELDVAL_DEC(rollRate);
    out += FIELDVAL_DEC(rollRateUnc);
    out += FIELDVAL_DEC(yaw);
    out += FIELDVAL_DEC(yawUnc);
    out += FIELDVAL_DEC(yawRate);
    out += FIELDVAL_DEC(yawRateUnc);

    return out;
}

string GnssSystemTimeStructType::toString() const {
    string out;
    out.reserve(256);

    out += FIELDVAL_MASK(validityMask, GnssSystemTimeStructTypeFlags_tbl);
    out += FIELDVAL_DEC(systemWeek);
    out += FIELDVAL_DEC(systemMsec);
    out += FIELDVAL_DEC(systemClkTimeBias);
    out += FIELDVAL_DEC(systemClkTimeUncMs);
    out += FIELDVAL_DEC(refFCount);
    out += FIELDVAL_DEC(numClockResets);

    return out;
}

string GnssGloTimeStructType::toString() const {
    string out;
    out.reserve(256);

    out += FIELDVAL_MASK(validityMask, GnssGloTimeStructTypeFlags_tbl);
    out += FIELDVAL_DEC(gloFourYear);
    out += FIELDVAL_DEC(gloDays);
    out += FIELDVAL_DEC(gloMsec);
    out += FIELDVAL_DEC(gloClkTimeBias);
    out += FIELDVAL_DEC(gloClkTimeUncMs);
    out += FIELDVAL_DEC(refFCount);
    out += FIELDVAL_DEC(numClockResets);

    return out;
}

string GnssSystemTime::toString() const {
    switch (gnssSystemTimeSrc) {
    case GNSS_LOC_SV_SYSTEM_GPS:
        return u.gpsSystemTime.toString();
    case GNSS_LOC_SV_SYSTEM_GALILEO:
        return u.galSystemTime.toString();
    case GNSS_LOC_SV_SYSTEM_GLONASS:
        return u.gloSystemTime.toString();
    case GNSS_LOC_SV_SYSTEM_BDS:
        return u.bdsSystemTime.toString();
    case GNSS_LOC_SV_SYSTEM_QZSS:
        return u.qzssSystemTime.toString();
    case GNSS_LOC_SV_SYSTEM_NAVIC:
        return u.navicSystemTime.toString();
    default:
        return "Unknown System ID: " + gnssSystemTimeSrc;
    }
}

string LLAInfo::toString() const {
    string out;
    out.reserve(256);
    out +=  "VRP based " + FIELDVAL_DEC(latitude);
    out +=  "VRP based " + FIELDVAL_DEC(longitude);
    out +=  "VRP based " + FIELDVAL_DEC(altitude);
    return out;
}

string Location::toString() const {
    string out;
    out.reserve(256);

    out += FIELDVAL_MASK(flags, LocationFlagsMask_tbl);
    out += FIELDVAL_DEC(timestamp);
    out += FIELDVAL_DEC(latitude);
    out += FIELDVAL_DEC(longitude);
    out += FIELDVAL_DEC(altitude);
    out += FIELDVAL_DEC(speed);
    out += FIELDVAL_DEC(bearing);
    out += FIELDVAL_DEC(horizontalAccuracy);
    out += FIELDVAL_DEC(verticalAccuracy);
    out += FIELDVAL_DEC(speedAccuracy);
    out += FIELDVAL_DEC(bearingAccuracy);
    out += FIELDVAL_MASK(techMask, LocationTechnologyMask_tbl);

    return out;
}

string GnssLocation::toString() const {
    string out;
    out.reserve(8096);

    out += Location::toString();
    out += FIELDVAL_MASK(gnssInfoFlags, GnssLocationInfoFlagMask_tbl);
    out += FIELDVAL_DEC(altitudeMeanSeaLevel);
    out += FIELDVAL_DEC(pdop);
    out += FIELDVAL_DEC(hdop);
    out += FIELDVAL_DEC(vdop);
    out += FIELDVAL_DEC(gdop);
    out += FIELDVAL_DEC(tdop);
    out += FIELDVAL_DEC(magneticDeviation);
    out += FIELDVAL_ENUM(horReliability, LocationReliability_tbl);
    out += FIELDVAL_ENUM(verReliability, LocationReliability_tbl);
    out += FIELDVAL_DEC(horUncEllipseSemiMajor);
    out += FIELDVAL_DEC(horUncEllipseSemiMinor);
    out += FIELDVAL_DEC(horUncEllipseOrientAzimuth);
    out += FIELDVAL_DEC(northStdDeviation);
    out += FIELDVAL_DEC(eastStdDeviation);
    out += FIELDVAL_DEC(northVelocity);
    out += FIELDVAL_DEC(eastVelocity);
    out += FIELDVAL_DEC(upVelocity);
    out += FIELDVAL_DEC(northVelocityStdDeviation);
    out += FIELDVAL_DEC(eastVelocityStdDeviation);
    out += FIELDVAL_DEC(upVelocityStdDeviation);
    out += FIELDVAL_DEC(numSvUsedInPosition);
    out += svUsedInPosition.toString();
    out += FIELDVAL_MASK(navSolutionMask, GnssLocationNavSolutionMask_tbl);
    out += bodyFrameData.toString();
    out += gnssSystemTime.toString();

    uint32_t ind = 0;
    for (auto measUsage : measUsageInfo) {
        out += "measUsageInfo[";
        out += to_string(ind);
        out += "]: ";
        out += measUsage.toString();
        ind++;
    }

    out += FIELDVAL_DEC(leapSeconds);
    out += FIELDVAL_DEC(timeUncMs);
    out += FIELDVAL_DEC(calibrationConfidencePercent);
    out += FIELDVAL_MASK(calibrationStatus, DrCalibrationStatusMask_tbl);
    out += FIELDVAL_ENUM(locOutputEngType, LocOutputEngineType_tbl);
    out += FIELDVAL_MASK(locOutputEngMask, PositioningEngineMask_tbl);
    out += FIELDVAL_DEC(conformityIndex);
    out += llaVRPBased.toString();
    out += FIELDVAL_DEC(enuVelocityVRPBased[0]);
    out += FIELDVAL_DEC(enuVelocityVRPBased[1]);
    out += FIELDVAL_DEC(enuVelocityVRPBased[2]);
    out += FIELDVAL_MASK(drSolutionStatusMask, DrSolutionStatusMask_tbl);
    out += FIELDVAL_MASK(sessionStatus, LocSessionStatus_tbl);

    return out;
}

string GnssSv::toString() const {
    string out;
    out.reserve(256);

    out += FIELDVAL_DEC(svId);
    out += FIELDVAL_ENUM(type, GnssSvType_tbl);
    out += FIELDVAL_DEC(cN0Dbhz);
    out += FIELDVAL_DEC(elevation);
    out += FIELDVAL_DEC(azimuth);
    out += FIELDVAL_MASK(gnssSvOptionsMask, GnssSvOptionsMask_tbl);
    out += FIELDVAL_DEC(carrierFrequencyHz);
    out += FIELDVAL_MASK(gnssSignalTypeMask, GnssSignalTypeMask_tbl);
    out += FIELDVAL_DEC(basebandCarrierToNoiseDbHz);
    out += FIELDVAL_DEC(gloFrequency);

    return out;
}

string GnssData::toString() const {
    string out;
    out.reserve(4096);

    for (int i = 0; i < GNSS_MAX_NUMBER_OF_SIGNAL_TYPES; i++) {
        out += FIELDVAL_MASK(i, GnssSignalTypes_tbl);
        out += FIELDVAL_MASK(gnssDataMask[i], GnssDataMask_tbl);
        out += FIELDVAL_DEC(jammerInd[i]);
        out += FIELDVAL_DEC(agc[i]);
    }

    return out;
}

string GnssMeasurementsData::toString() const {
    string out;
    out.reserve(256);

    out += FIELDVAL_MASK(flags, GnssMeasurementsDataFlagsMask_tbl);
    out += FIELDVAL_DEC(svId);
    out += FIELDVAL_ENUM(svType, GnssSvType_tbl);
    out += FIELDVAL_DEC(timeOffsetNs);
    out += FIELDVAL_MASK(stateMask, GnssMeasurementsStateMask_tbl);
    out += FIELDVAL_DEC(receivedSvTimeNs);
    out += FIELDVAL_DEC(receivedSvTimeUncertaintyNs);
    out += FIELDVAL_DEC(carrierToNoiseDbHz);
    out += FIELDVAL_DEC(pseudorangeRateMps);
    out += FIELDVAL_DEC(pseudorangeRateUncertaintyMps);
    out += FIELDVAL_MASK(adrStateMask, GnssMeasurementsAdrStateMask_tbl);
    out += FIELDVAL_DEC(adrMeters);
    out += FIELDVAL_DEC(adrUncertaintyMeters);
    out += FIELDVAL_DEC(carrierFrequencyHz);
    out += FIELDVAL_DEC(carrierCycles);
    out += FIELDVAL_DEC(carrierPhase);
    out += FIELDVAL_DEC(carrierPhaseUncertainty);
    out += FIELDVAL_ENUM(multipathIndicator, GnssMeasurementsMultipathIndicator_tbl);
    out += FIELDVAL_DEC(signalToNoiseRatioDb);
    out += FIELDVAL_DEC(agcLevelDb);
    out += FIELDVAL_DEC(basebandCarrierToNoiseDbHz);
    out += FIELDVAL_MASK(gnssSignalType, GnssSignalTypeMask_tbl);
    out += FIELDVAL_DEC(interSignalBiasNs);
    out += FIELDVAL_DEC(interSignalBiasUncertaintyNs);
    out += FIELDVAL_DEC(cycleSlipCount);

    return out;
}

string GnssMeasurementsClock::toString() const {
    string out;
    out.reserve(256);

    out += FIELDVAL_MASK(flags, GnssMeasurementsClockFlagsMask_tbl);
    out += FIELDVAL_DEC(leapSecond);
    out += FIELDVAL_DEC(timeNs);
    out += FIELDVAL_DEC(timeUncertaintyNs);
    out += FIELDVAL_DEC(fullBiasNs);
    out += FIELDVAL_DEC(biasNs);
    out += FIELDVAL_DEC(biasUncertaintyNs);
    out += FIELDVAL_DEC(driftNsps);
    out += FIELDVAL_DEC(driftUncertaintyNsps);
    out += FIELDVAL_DEC(hwClockDiscontinuityCount);

    return out;
}

string GnssMeasurements::toString() const {
    string out;
    // (number of GnssMeasurementsData in the vector + GnssMeasurementsClock) * 256
    out.reserve((measurements.size() + 1) << 8);

    out += clock.toString();
    for (auto meas : measurements) {
        out += meas.toString();
    }

    return out;
}

string LeapSecondChangeInfo::toString() const {
    string out;
    out.reserve(256);

    out += gpsTimestampLsChange.toString();
    out += FIELDVAL_DEC(leapSecondsBeforeChange);
    out += FIELDVAL_DEC(leapSecondsAfterChange);

    return out;
}

string LeapSecondSystemInfo::toString() const {
    string out;
    out.reserve(256);

    out += FIELDVAL_MASK(leapSecondInfoMask, LeapSecondSysInfoMask_tbl);
    out += FIELDVAL_DEC(leapSecondCurrent);

    return out;
}

string LocationSystemInfo::toString() const {
    string out;
    out.reserve(256);

    out += FIELDVAL_MASK(systemInfoMask, LocationSystemInfoMask_tbl);
    out += leapSecondSysInfo.toString();

    return out;
}

} // namespace location_client
