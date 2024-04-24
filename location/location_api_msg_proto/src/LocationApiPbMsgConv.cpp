/* Copyright (c) 2020 The Linux Foundation. All rights reserved.
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
#define LOG_TAG "LocSvc_LocationApiPbMsgConv"

#include <inttypes.h>
#include <dirent.h>

#include <loc_pla.h>
#include <log_util.h>
#include <loc_misc_utils.h>
#include <gps_extended.h>
#include <LocIpc.h>
#include <LocTimer.h>
#include <loc_cfg.h>

#include <LocationApiPbMsgConv.h>

using namespace loc_util;
using std::min;

// ********************
// LocationApiPbMsgConv
// ********************
LocationApiPbMsgConv::LocationApiPbMsgConv() {
    mPbDebugLogEnabled = false;
    mPbVerboseLogEnabled = false;
    // Logtag mechanism for Protobuf conv util log
    // Hidden configs to enable debug logs at runtime for printing many protobuf
    // conversion output (encode and decode). This will help for debugging. By
    // default the config is disabled, so these logs wont be printed.
    // LOCAPIPB_DEBUG_LOG - enable hidden debug logs
    // LOCAPIPB_VERBOSE_LOG - enable hidden verbose logs
    loc_param_s_type gpsConfParamTable[] = {
        {"LOCAPIPB_DEBUG_LOG",       &mPbDebugLogEnabled,      nullptr, 'n'},
        {"LOCAPIPB_VERBOSE_LOG",       &mPbVerboseLogEnabled,      nullptr, 'n'}
    };
    UTIL_READ_CONF(LOC_PATH_GPS_CONF, gpsConfParamTable);
    LOC_LOGd("LocApiPB logs - Debug log: %d, Verbose log:%d",
            mPbDebugLogEnabled, mPbVerboseLogEnabled);
    LOC_LOGi("LocationApiMsg version  Major:%d, Minor:%d",
            LOCAPI_MSG_VER_MAJOR, LOCAPI_MSG_VER_MINOR);
    LOC_LOGi("LocationApiDataTypes version  Major:%d, Minor:%d",
            LOCAPI_DATA_TYPES_VER_MAJOR, LOCAPI_DATA_TYPES_VER_MINOR);
}

// **** helper function for enum conversion from protobuf enums to normal format.
ELocMsgID LocationApiPbMsgConv::getEnumForPBELocMsgID(const PBELocMsgID &pbLocMsgId) const {
    ELocMsgID eLocMsgId = E_LOCAPI_UNDEFINED_MSG_ID;
    switch (pbLocMsgId) {
        case PB_E_LOCAPI_UNDEFINED_MSG_ID:
            eLocMsgId = E_LOCAPI_UNDEFINED_MSG_ID;
            break;
        case PB_E_LOCAPI_CLIENT_REGISTER_MSG_ID:
            eLocMsgId = E_LOCAPI_CLIENT_REGISTER_MSG_ID;
            break;
        case PB_E_LOCAPI_CLIENT_DEREGISTER_MSG_ID:
            eLocMsgId = E_LOCAPI_CLIENT_DEREGISTER_MSG_ID;
            break;
        case PB_E_LOCAPI_CAPABILILTIES_MSG_ID:
            eLocMsgId = E_LOCAPI_CAPABILILTIES_MSG_ID;
            break;
        case PB_E_LOCAPI_HAL_READY_MSG_ID:
            eLocMsgId = E_LOCAPI_HAL_READY_MSG_ID;
            break;
        case PB_E_LOCAPI_START_TRACKING_MSG_ID:
            eLocMsgId = E_LOCAPI_START_TRACKING_MSG_ID;
            break;
        case PB_E_LOCAPI_STOP_TRACKING_MSG_ID:
            eLocMsgId = E_LOCAPI_STOP_TRACKING_MSG_ID;
            break;
        case PB_E_LOCAPI_UPDATE_CALLBACKS_MSG_ID:
            eLocMsgId = E_LOCAPI_UPDATE_CALLBACKS_MSG_ID;
            break;
        case PB_E_LOCAPI_UPDATE_TRACKING_OPTIONS_MSG_ID:
            eLocMsgId = E_LOCAPI_UPDATE_TRACKING_OPTIONS_MSG_ID;
            break;
        case PB_E_LOCAPI_CONTROL_UPDATE_CONFIG_MSG_ID:
            eLocMsgId = E_LOCAPI_CONTROL_UPDATE_CONFIG_MSG_ID;
            break;
        case PB_E_LOCAPI_CONTROL_DELETE_AIDING_DATA_MSG_ID:
            eLocMsgId = E_LOCAPI_CONTROL_DELETE_AIDING_DATA_MSG_ID;
            break;
        case PB_E_LOCAPI_CONTROL_UPDATE_NETWORK_AVAILABILITY_MSG_ID:
            eLocMsgId = E_LOCAPI_CONTROL_UPDATE_NETWORK_AVAILABILITY_MSG_ID;
            break;
        case PB_E_LOCAPI_LOCATION_MSG_ID:
            eLocMsgId = E_LOCAPI_LOCATION_MSG_ID;
            break;
        case PB_E_LOCAPI_LOCATION_INFO_MSG_ID:
            eLocMsgId = E_LOCAPI_LOCATION_INFO_MSG_ID;
            break;
        case PB_E_LOCAPI_SATELLITE_VEHICLE_MSG_ID:
            eLocMsgId = E_LOCAPI_SATELLITE_VEHICLE_MSG_ID;
            break;
        case PB_E_LOCAPI_NMEA_MSG_ID:
            eLocMsgId = E_LOCAPI_NMEA_MSG_ID;
            break;
        case PB_E_LOCAPI_DATA_MSG_ID:
            eLocMsgId = E_LOCAPI_DATA_MSG_ID;
            break;
        case PB_E_LOCAPI_GET_GNSS_ENGERY_CONSUMED_MSG_ID:
            eLocMsgId = E_LOCAPI_GET_GNSS_ENGERY_CONSUMED_MSG_ID;
            break;
        case PB_E_LOCAPI_LOCATION_SYSTEM_INFO_MSG_ID:
            eLocMsgId = E_LOCAPI_LOCATION_SYSTEM_INFO_MSG_ID;
            break;
        case PB_E_LOCAPI_ENGINE_LOCATIONS_INFO_MSG_ID:
            eLocMsgId = E_LOCAPI_ENGINE_LOCATIONS_INFO_MSG_ID;
            break;
        case PB_E_LOCAPI_START_BATCHING_MSG_ID:
            eLocMsgId = E_LOCAPI_START_BATCHING_MSG_ID;
            break;
        case PB_E_LOCAPI_STOP_BATCHING_MSG_ID:
            eLocMsgId = E_LOCAPI_STOP_BATCHING_MSG_ID;
            break;
        case PB_E_LOCAPI_UPDATE_BATCHING_OPTIONS_MSG_ID:
            eLocMsgId = E_LOCAPI_UPDATE_BATCHING_OPTIONS_MSG_ID;
            break;
        case PB_E_LOCAPI_BATCHING_MSG_ID:
            eLocMsgId = E_LOCAPI_BATCHING_MSG_ID;
            break;
        case PB_E_LOCAPI_ADD_GEOFENCES_MSG_ID:
            eLocMsgId = E_LOCAPI_ADD_GEOFENCES_MSG_ID;
            break;
        case PB_E_LOCAPI_REMOVE_GEOFENCES_MSG_ID:
            eLocMsgId = E_LOCAPI_REMOVE_GEOFENCES_MSG_ID;
            break;
        case PB_E_LOCAPI_MODIFY_GEOFENCES_MSG_ID:
            eLocMsgId = E_LOCAPI_MODIFY_GEOFENCES_MSG_ID;
            break;
        case PB_E_LOCAPI_PAUSE_GEOFENCES_MSG_ID:
            eLocMsgId = E_LOCAPI_PAUSE_GEOFENCES_MSG_ID;
            break;
        case PB_E_LOCAPI_RESUME_GEOFENCES_MSG_ID:
            eLocMsgId = E_LOCAPI_RESUME_GEOFENCES_MSG_ID;
            break;
        case PB_E_LOCAPI_GEOFENCE_BREACH_MSG_ID:
            eLocMsgId = E_LOCAPI_GEOFENCE_BREACH_MSG_ID;
            break;
        case PB_E_LOCAPI_MEAS_MSG_ID:
            eLocMsgId = E_LOCAPI_MEAS_MSG_ID;
            break;
        case PB_E_LOCAPI_GET_SINGLE_TERRESTRIAL_POS_REQ_MSG_ID:
            eLocMsgId = E_LOCAPI_GET_SINGLE_TERRESTRIAL_POS_REQ_MSG_ID;
            break;
        case PB_E_LOCAPI_GET_SINGLE_TERRESTRIAL_POS_RESP_MSG_ID:
            eLocMsgId = E_LOCAPI_GET_SINGLE_TERRESTRIAL_POS_RESP_MSG_ID;
            break;
        case PB_E_LOCAPI_PINGTEST_MSG_ID:
            eLocMsgId = E_LOCAPI_PINGTEST_MSG_ID;
            break;
        case PB_E_INTAPI_CONFIG_CONSTRAINTED_TUNC_MSG_ID:
            eLocMsgId = E_INTAPI_CONFIG_CONSTRAINTED_TUNC_MSG_ID;
            break;
        case PB_E_INTAPI_CONFIG_POSITION_ASSISTED_CLOCK_ESTIMATOR_MSG_ID:
            eLocMsgId = E_INTAPI_CONFIG_POSITION_ASSISTED_CLOCK_ESTIMATOR_MSG_ID;
            break;
        case PB_E_INTAPI_CONFIG_SV_CONSTELLATION_MSG_ID:
            eLocMsgId = E_INTAPI_CONFIG_SV_CONSTELLATION_MSG_ID;
            break;
        case PB_E_INTAPI_CONFIG_AIDING_DATA_DELETION_MSG_ID:
            eLocMsgId = E_INTAPI_CONFIG_AIDING_DATA_DELETION_MSG_ID;
            break;
        case PB_E_INTAPI_CONFIG_LEVER_ARM_MSG_ID:
            eLocMsgId = E_INTAPI_CONFIG_LEVER_ARM_MSG_ID;
            break;
        case PB_E_INTAPI_CONFIG_ROBUST_LOCATION_MSG_ID:
            eLocMsgId = E_INTAPI_CONFIG_ROBUST_LOCATION_MSG_ID;
            break;
        case PB_E_INTAPI_CONFIG_MIN_GPS_WEEK_MSG_ID:
            eLocMsgId = E_INTAPI_CONFIG_MIN_GPS_WEEK_MSG_ID;
            break;
        case PB_E_INTAPI_CONFIG_DEAD_RECKONING_ENGINE_MSG_ID:
            eLocMsgId = E_INTAPI_CONFIG_DEAD_RECKONING_ENGINE_MSG_ID;
            break;
        case PB_E_INTAPI_CONFIG_MIN_SV_ELEVATION_MSG_ID:
            eLocMsgId = E_INTAPI_CONFIG_MIN_SV_ELEVATION_MSG_ID;
            break;
        case PB_E_INTAPI_CONFIG_CONSTELLATION_SECONDARY_BAND_MSG_ID:
            eLocMsgId = E_INTAPI_CONFIG_CONSTELLATION_SECONDARY_BAND_MSG_ID;
            break;
        case PB_E_INTAPI_CONFIG_ENGINE_RUN_STATE_MSG_ID:
            eLocMsgId = E_INTAPI_CONFIG_ENGINE_RUN_STATE_MSG_ID;
            break;
        case PB_E_INTAPI_CONFIG_USER_CONSENT_TERRESTRIAL_POSITIONING_MSG_ID:
            eLocMsgId = E_INTAPI_CONFIG_USER_CONSENT_TERRESTRIAL_POSITIONING_MSG_ID;
            break;
        case PB_E_INTAPI_GET_ROBUST_LOCATION_CONFIG_REQ_MSG_ID:
            eLocMsgId = E_INTAPI_GET_ROBUST_LOCATION_CONFIG_REQ_MSG_ID;
            break;
        case PB_E_INTAPI_GET_ROBUST_LOCATION_CONFIG_RESP_MSG_ID:
            eLocMsgId = E_INTAPI_GET_ROBUST_LOCATION_CONFIG_RESP_MSG_ID;
            break;
        case PB_E_INTAPI_GET_MIN_GPS_WEEK_REQ_MSG_ID:
            eLocMsgId = E_INTAPI_GET_MIN_GPS_WEEK_REQ_MSG_ID;
            break;
        case PB_E_INTAPI_GET_MIN_GPS_WEEK_RESP_MSG_ID:
            eLocMsgId = E_INTAPI_GET_MIN_GPS_WEEK_RESP_MSG_ID;
            break;
        case PB_E_INTAPI_GET_MIN_SV_ELEVATION_REQ_MSG_ID:
            eLocMsgId = E_INTAPI_GET_MIN_SV_ELEVATION_REQ_MSG_ID;
            break;
        case PB_E_INTAPI_GET_MIN_SV_ELEVATION_RESP_MSG_ID:
            eLocMsgId = E_INTAPI_GET_MIN_SV_ELEVATION_RESP_MSG_ID;
            break;
        case PB_E_INTAPI_GET_CONSTELLATION_SECONDARY_BAND_CONFIG_REQ_MSG_ID:
            eLocMsgId = E_INTAPI_GET_CONSTELLATION_SECONDARY_BAND_CONFIG_REQ_MSG_ID;
            break;
        case PB_E_INTAPI_GET_CONSTELLATION_SECONDARY_BAND_CONFIG_RESP_MSG_ID:
            eLocMsgId = E_INTAPI_GET_CONSTELLATION_SECONDARY_BAND_CONFIG_RESP_MSG_ID;
            break;
        default:
            break;
    }
    LocApiPb_LOGv("LocApiPB: pbLocMsgId:%d, eLocMsgId:%d", pbLocMsgId, eLocMsgId);
    return eLocMsgId;
}

LocationError LocationApiPbMsgConv::getEnumForPBLocationError(
        const PBLocationError &pbLocErr) const {
    LocationError locErr = LOCATION_ERROR_GENERAL_FAILURE;
    if (PB_LOCATION_ERROR_SUCCESS == pbLocErr) {
        locErr = LOCATION_ERROR_SUCCESS;
    } else if (PB_LOCATION_ERROR_INVALID_PARAMETER == pbLocErr) {
        locErr = LOCATION_ERROR_INVALID_PARAMETER;
    } else if (PB_LOCATION_ERROR_NOT_SUPPORTED  == pbLocErr) {
        locErr = LOCATION_ERROR_NOT_SUPPORTED;
    } else if (PB_LOCATION_ERROR_TIMEOUT == pbLocErr) {
        locErr = LOCATION_ERROR_TIMEOUT;
    }
    return locErr;
}

ClientType LocationApiPbMsgConv::getEnumForPBClientType(const PBClientType &pbClntTyp) const {
    ClientType clntTyp = LOCATION_CLIENT_API;
    if (PB_LOCATION_INTEGRATION_API == pbClntTyp) {
        clntTyp = LOCATION_INTEGRATION_API;
    }
    LocApiPb_LOGv("LocApiPB: pbClntTyp:%d, clntTyp:%d", pbClntTyp, clntTyp);
    return clntTyp;
}

BatchingMode LocationApiPbMsgConv::getEnumForPBBatchingMode(
        const PBBatchingMode &pbBatchMode) const {
    BatchingMode batchMode = BATCHING_MODE_ROUTINE;
    switch (pbBatchMode) {
        case PB_BATCHING_MODE_TRIP:
            batchMode = BATCHING_MODE_TRIP;
            break;
        case PB_BATCHING_MODE_NO_AUTO_REPORT:
            batchMode = BATCHING_MODE_NO_AUTO_REPORT;
            break;
        default:
            break;
    }
    LocApiPb_LOGv("LocApiPB: pbBatchMode:%d, batchMode:%d", pbBatchMode, batchMode);
    return batchMode;
}

GnssSuplMode LocationApiPbMsgConv::getEnumForPBGnssSuplMode(
        const PBGnssSuplMode &pbGnssSuplMode) const {
    GnssSuplMode gnssSuplMode = GNSS_SUPL_MODE_STANDALONE;
    switch (pbGnssSuplMode) {
        case PB_GNSS_SUPL_MODE_MSB:
            gnssSuplMode = GNSS_SUPL_MODE_MSB;
            break;
        case PB_GNSS_SUPL_MODE_MSA:
            gnssSuplMode = GNSS_SUPL_MODE_MSA;
            break;
        default:
            break;
    }
    LocApiPb_LOGv("LocApiPB: pbGnssSuplMode:%d, gnssSuplMode:%d", pbGnssSuplMode, gnssSuplMode);
    return gnssSuplMode;
}

BatchingStatus LocationApiPbMsgConv::getEnumForPBBatchingStatus(
        const PBBatchingStatus &pbBatchStat) const {
    BatchingStatus batchStat = BATCHING_STATUS_POSITION_UNAVAILABLE;
    switch (pbBatchStat) {
        case PB_BATCHING_STATUS_TRIP_COMPLETED:
            batchStat = BATCHING_STATUS_TRIP_COMPLETED;
            break;
        case PB_BATCHING_STATUS_POSITION_AVAILABE:
            batchStat = BATCHING_STATUS_POSITION_AVAILABE;
            break;
        case PB_BATCHING_STATUS_POSITION_UNAVAILABLE:
            batchStat = BATCHING_STATUS_POSITION_UNAVAILABLE;
            break;
        default:
            break;
    }
    LocApiPb_LOGv("LocApiPB: pbBatchStat:%d, batchStat:%d", pbBatchStat, batchStat);
    return batchStat;
}

GnssMeasurementsMultipathIndicator LocationApiPbMsgConv::getEnumForPBGnssMeasMultipathIndic(
        const PBGnssMeasurementsMultipathIndicator &pbGnssMeasMultipathIndic) const {
    GnssMeasurementsMultipathIndicator gnssMeasMultipathIndic =
            GNSS_MEASUREMENTS_MULTIPATH_INDICATOR_UNKNOWN;
    switch (pbGnssMeasMultipathIndic) {
        case PB_GNSS_MEASUREMENTS_MULTIPATH_INDICATOR_UNKNOWN:
            gnssMeasMultipathIndic = GNSS_MEASUREMENTS_MULTIPATH_INDICATOR_UNKNOWN;
            break;
        case PB_GNSS_MEASUREMENTS_MULTIPATH_INDICATOR_PRESENT:
            gnssMeasMultipathIndic = GNSS_MEASUREMENTS_MULTIPATH_INDICATOR_PRESENT;
            break;
        case PB_GNSS_MEASUREMENTS_MULTIPATH_INDICATOR_NOT_PRESENT:
            gnssMeasMultipathIndic = GNSS_MEASUREMENTS_MULTIPATH_INDICATOR_NOT_PRESENT;
            break;
        default:
            break;
    }
    LocApiPb_LOGv("LocApiPB: pbGnssMeasMultipathIndic:%d, gnssMeasMultipathIndic:%d",
            pbGnssMeasMultipathIndic, gnssMeasMultipathIndic);
    return gnssMeasMultipathIndic;
}

LocationReliability LocationApiPbMsgConv::getEnumForPBLocReliability(
        const PBLocationReliability &pbLocReliability) const {
    LocationReliability locReliability = LOCATION_RELIABILITY_NOT_SET;
    switch (pbLocReliability) {
        case PB_LOCATION_RELIABILITY_NOT_SET:
            locReliability = LOCATION_RELIABILITY_NOT_SET;
            break;
        case PB_LOCATION_RELIABILITY_VERY_LOW:
            locReliability = LOCATION_RELIABILITY_VERY_LOW;
            break;
        case PB_LOCATION_RELIABILITY_LOW:
            locReliability = LOCATION_RELIABILITY_LOW;
            break;
        case PB_LOCATION_RELIABILITY_MEDIUM:
            locReliability = LOCATION_RELIABILITY_MEDIUM;
            break;
        case PB_LOCATION_RELIABILITY_HIGH:
            locReliability = LOCATION_RELIABILITY_HIGH;
            break;
        default:
            break;
    }
    LocApiPb_LOGv("LocApiPB: pbLocReliability:%d, locReliability:%d", pbLocReliability,
            locReliability);
    return locReliability;
}

Gnss_LocSvSystemEnumType LocationApiPbMsgConv::getEnumForPBGnssLocSvSystem(
            const PBLocApiGnss_LocSvSystemEnumType &pbGnssLocSvSysEnumType) const {
    Gnss_LocSvSystemEnumType gnssLocSvSysEnumType = GNSS_LOC_SV_SYSTEM_UNKNOWN;
    switch (pbGnssLocSvSysEnumType) {
        case PB_GNSS_LOC_SV_SYSTEM_INVALID:
            gnssLocSvSysEnumType = GNSS_LOC_SV_SYSTEM_UNKNOWN;
            break;
        case PB_GNSS_LOC_SV_SYSTEM_GPS:
            gnssLocSvSysEnumType = GNSS_LOC_SV_SYSTEM_GPS;
            break;
        case PB_GNSS_LOC_SV_SYSTEM_GALILEO:
            gnssLocSvSysEnumType = GNSS_LOC_SV_SYSTEM_GALILEO;
            break;
        case PB_GNSS_LOC_SV_SYSTEM_SBAS:
            gnssLocSvSysEnumType = GNSS_LOC_SV_SYSTEM_SBAS;
            break;
        case PB_GNSS_LOC_SV_SYSTEM_GLONASS:
            gnssLocSvSysEnumType = GNSS_LOC_SV_SYSTEM_GLONASS;
            break;
        case PB_GNSS_LOC_SV_SYSTEM_BDS:
            gnssLocSvSysEnumType = GNSS_LOC_SV_SYSTEM_BDS;
            break;
        case PB_GNSS_LOC_SV_SYSTEM_QZSS:
            gnssLocSvSysEnumType = GNSS_LOC_SV_SYSTEM_QZSS;
            break;
        case PB_GNSS_LOC_SV_SYSTEM_NAVIC:
            gnssLocSvSysEnumType = GNSS_LOC_SV_SYSTEM_NAVIC;
            break;
        default:
            break;
    }
    LocApiPb_LOGv("LocApiPB: pbGnssLocSvSysEnumType:%d, gnssLocSvSysEnumType:%d",
            pbGnssLocSvSysEnumType, gnssLocSvSysEnumType);
    return gnssLocSvSysEnumType;
}

LocOutputEngineType LocationApiPbMsgConv::getEnumForPBLocOutputEngineType(
            const PBLocApiOutputEngineType &pbLocOpEngType) const {
    LocOutputEngineType locOpEngType = LOC_OUTPUT_ENGINE_FUSED;
    switch (pbLocOpEngType) {
        case PB_LOC_OUTPUT_ENGINE_FUSED:
            locOpEngType = LOC_OUTPUT_ENGINE_FUSED;
            break;
        case PB_LOC_OUTPUT_ENGINE_SPE:
            locOpEngType = LOC_OUTPUT_ENGINE_SPE;
            break;
        case PB_LOC_OUTPUT_ENGINE_PPE:
            locOpEngType = LOC_OUTPUT_ENGINE_PPE;
            break;
        default:
            break;
    }
    LocApiPb_LOGv("LocApiPB: pbLocOpEngType:%d, locOpEngType:%d",
            pbLocOpEngType, locOpEngType);
    return locOpEngType;
}

// HAL position mask from PB position engine mask
LocEngineRunState LocationApiPbMsgConv::getEnumForPBLocEngineRunState(
            const PBLocEngineRunState &pbLocEngRunState) const {
    LocEngineRunState locEngRunState = (LocEngineRunState) 0;

    if (pbLocEngRunState == PB_LOC_ENGINE_RUN_STATE_PAUSE) {
        locEngRunState = LOC_ENGINE_RUN_STATE_PAUSE;
    } else if (pbLocEngRunState == PB_LOC_ENGINE_RUN_STATE_RESUME) {
        locEngRunState = LOC_ENGINE_RUN_STATE_RESUME;
    }

    LocApiPb_LOGv("LocApiPB: pbEngineRunState:%d, llocEngRunState:%d",
            pbLocEngRunState, locEngRunState);
    return locEngRunState;
}

// HAL position mask to PB position engine mask
uint32_t LocationApiPbMsgConv::getPBEnumForLocEngineRunState(
        const LocEngineRunState& locEngineRunState) const {

    PBLocEngineRunState pbEngineRunState = PB_LOC_ENGINE_RUN_STATE_INVALID;;

    if (locEngineRunState == LOC_ENGINE_RUN_STATE_PAUSE) {
        pbEngineRunState = PB_LOC_ENGINE_RUN_STATE_PAUSE;
    } else if (locEngineRunState ==LOC_ENGINE_RUN_STATE_RESUME) {
        pbEngineRunState = PB_LOC_ENGINE_RUN_STATE_RESUME;
    }
    LocApiPb_LOGd("LocApiPB: locEngineRunState: %d, pbEngineRunState: %d",
                  locEngineRunState, pbEngineRunState);
    return pbEngineRunState;
}

// HAL TerrestrialTechMask from PB format
uint32_t LocationApiPbMsgConv::getTerrestrialTechMaskFromPB(
        const uint32_t &pbTerrestrialTechMask) const {
    uint32_t terrestrialTechMask = 0;

    if (pbTerrestrialTechMask & PB_TERRESTRIAL_TECH_GTP_WWAN) {
        terrestrialTechMask |= TERRESTRIAL_TECH_GTP_WWAN;
    }

    return terrestrialTechMask;
}

// PB TerrestrialTechMask from HAL format
uint32_t LocationApiPbMsgConv::getPBMaskForTerrestrialTechMask(
        const uint32_t& terrestrialTechMask) const {
    uint32_t pbTerrestrialTechMask = PB_TERRESTRIAL_TECH_INVALID;

    if (terrestrialTechMask & TERRESTRIAL_TECH_GTP_WWAN) {
        pbTerrestrialTechMask |= PB_TERRESTRIAL_TECH_GTP_WWAN;
    }

    return pbTerrestrialTechMask;
}


// PBLocApiGnss_LocSvSystemEnumType to GnssSvType
GnssSvType LocationApiPbMsgConv::getGnssSvTypeFromPBGnssLocSvSystemEnumType(
            const PBLocApiGnss_LocSvSystemEnumType &pbGnssLocSvSysEnum) const {
    GnssSvType gnssSvType = GNSS_SV_TYPE_UNKNOWN;
    switch (pbGnssLocSvSysEnum) {
        case PB_GNSS_LOC_SV_SYSTEM_GPS:
            gnssSvType = GNSS_SV_TYPE_GPS;
            break;
        case PB_GNSS_LOC_SV_SYSTEM_GALILEO:
            gnssSvType = GNSS_SV_TYPE_GALILEO;
            break;
        case PB_GNSS_LOC_SV_SYSTEM_SBAS:
            gnssSvType = GNSS_SV_TYPE_SBAS;
            break;
        case PB_GNSS_LOC_SV_SYSTEM_GLONASS:
            gnssSvType = GNSS_SV_TYPE_GLONASS;
            break;
        case PB_GNSS_LOC_SV_SYSTEM_BDS:
            gnssSvType = GNSS_SV_TYPE_BEIDOU;
            break;
        case PB_GNSS_LOC_SV_SYSTEM_QZSS:
            gnssSvType = GNSS_SV_TYPE_QZSS;
            break;
        case PB_GNSS_LOC_SV_SYSTEM_NAVIC:
            gnssSvType = GNSS_SV_TYPE_NAVIC;
            break;
        default:
            break;
    }
    LocApiPb_LOGv("LocApiPB: pbGnssLocSvSysEnum:%d, gnssSvType:%d", pbGnssLocSvSysEnum,
            gnssSvType);
    return gnssSvType;
}

// **** helper function for enum conversion to protobuf enums
PBELocMsgID LocationApiPbMsgConv::getPBEnumForELocMsgID(const ELocMsgID &eLocMsgId) const {
    PBELocMsgID pbLocMsgId = PB_E_LOCAPI_UNDEFINED_MSG_ID;
    switch (eLocMsgId) {
        case E_LOCAPI_CLIENT_REGISTER_MSG_ID:
            pbLocMsgId = PB_E_LOCAPI_CLIENT_REGISTER_MSG_ID;
            break;
        case E_LOCAPI_CLIENT_DEREGISTER_MSG_ID:
            pbLocMsgId = PB_E_LOCAPI_CLIENT_DEREGISTER_MSG_ID;
            break;
        case E_LOCAPI_CAPABILILTIES_MSG_ID:
            pbLocMsgId = PB_E_LOCAPI_CAPABILILTIES_MSG_ID;
            break;
        case E_LOCAPI_HAL_READY_MSG_ID:
            pbLocMsgId = PB_E_LOCAPI_HAL_READY_MSG_ID;
            break;
        case E_LOCAPI_START_TRACKING_MSG_ID:
            pbLocMsgId = PB_E_LOCAPI_START_TRACKING_MSG_ID;
            break;
        case E_LOCAPI_STOP_TRACKING_MSG_ID:
            pbLocMsgId = PB_E_LOCAPI_STOP_TRACKING_MSG_ID;
            break;
        case E_LOCAPI_UPDATE_CALLBACKS_MSG_ID:
            pbLocMsgId = PB_E_LOCAPI_UPDATE_CALLBACKS_MSG_ID;
            break;
        case E_LOCAPI_UPDATE_TRACKING_OPTIONS_MSG_ID:
            pbLocMsgId = PB_E_LOCAPI_UPDATE_TRACKING_OPTIONS_MSG_ID;
            break;
        case E_LOCAPI_CONTROL_UPDATE_CONFIG_MSG_ID:
            pbLocMsgId = PB_E_LOCAPI_CONTROL_UPDATE_CONFIG_MSG_ID;
            break;
        case E_LOCAPI_CONTROL_DELETE_AIDING_DATA_MSG_ID:
            pbLocMsgId = PB_E_LOCAPI_CONTROL_DELETE_AIDING_DATA_MSG_ID;
            break;
        case E_LOCAPI_CONTROL_UPDATE_NETWORK_AVAILABILITY_MSG_ID:
            pbLocMsgId = PB_E_LOCAPI_CONTROL_UPDATE_NETWORK_AVAILABILITY_MSG_ID;
            break;
        case E_LOCAPI_LOCATION_MSG_ID:
            pbLocMsgId = PB_E_LOCAPI_LOCATION_MSG_ID;
            break;
        case E_LOCAPI_LOCATION_INFO_MSG_ID:
            pbLocMsgId = PB_E_LOCAPI_LOCATION_INFO_MSG_ID;
            break;
        case E_LOCAPI_SATELLITE_VEHICLE_MSG_ID:
            pbLocMsgId = PB_E_LOCAPI_SATELLITE_VEHICLE_MSG_ID;
            break;
        case E_LOCAPI_NMEA_MSG_ID:
            pbLocMsgId = PB_E_LOCAPI_NMEA_MSG_ID;
            break;
        case E_LOCAPI_DATA_MSG_ID:
            pbLocMsgId = PB_E_LOCAPI_DATA_MSG_ID;
            break;
        case E_LOCAPI_GET_GNSS_ENGERY_CONSUMED_MSG_ID:
            pbLocMsgId = PB_E_LOCAPI_GET_GNSS_ENGERY_CONSUMED_MSG_ID;
            break;
        case E_LOCAPI_LOCATION_SYSTEM_INFO_MSG_ID:
            pbLocMsgId = PB_E_LOCAPI_LOCATION_SYSTEM_INFO_MSG_ID;
            break;
        case E_LOCAPI_ENGINE_LOCATIONS_INFO_MSG_ID:
            pbLocMsgId = PB_E_LOCAPI_ENGINE_LOCATIONS_INFO_MSG_ID;
            break;
        case E_LOCAPI_START_BATCHING_MSG_ID:
            pbLocMsgId = PB_E_LOCAPI_START_BATCHING_MSG_ID;
            break;
        case E_LOCAPI_STOP_BATCHING_MSG_ID:
            pbLocMsgId = PB_E_LOCAPI_STOP_BATCHING_MSG_ID;
            break;
        case E_LOCAPI_UPDATE_BATCHING_OPTIONS_MSG_ID:
            pbLocMsgId = PB_E_LOCAPI_UPDATE_BATCHING_OPTIONS_MSG_ID;
            break;
        case E_LOCAPI_BATCHING_MSG_ID:
            pbLocMsgId = PB_E_LOCAPI_BATCHING_MSG_ID;
            break;
        case E_LOCAPI_ADD_GEOFENCES_MSG_ID:
            pbLocMsgId = PB_E_LOCAPI_ADD_GEOFENCES_MSG_ID;
            break;
        case E_LOCAPI_REMOVE_GEOFENCES_MSG_ID:
            pbLocMsgId = PB_E_LOCAPI_REMOVE_GEOFENCES_MSG_ID;
            break;
        case E_LOCAPI_MODIFY_GEOFENCES_MSG_ID:
            pbLocMsgId = PB_E_LOCAPI_MODIFY_GEOFENCES_MSG_ID;
            break;
        case E_LOCAPI_PAUSE_GEOFENCES_MSG_ID:
            pbLocMsgId = PB_E_LOCAPI_PAUSE_GEOFENCES_MSG_ID;
            break;
        case E_LOCAPI_RESUME_GEOFENCES_MSG_ID:
            pbLocMsgId = PB_E_LOCAPI_RESUME_GEOFENCES_MSG_ID;
            break;
        case E_LOCAPI_GEOFENCE_BREACH_MSG_ID:
            pbLocMsgId = PB_E_LOCAPI_GEOFENCE_BREACH_MSG_ID;
            break;
        case E_LOCAPI_MEAS_MSG_ID:
            pbLocMsgId = PB_E_LOCAPI_MEAS_MSG_ID;
            break;
        case E_LOCAPI_GET_SINGLE_TERRESTRIAL_POS_REQ_MSG_ID:
            pbLocMsgId = PB_E_LOCAPI_GET_SINGLE_TERRESTRIAL_POS_REQ_MSG_ID;
            break;
        case E_LOCAPI_GET_SINGLE_TERRESTRIAL_POS_RESP_MSG_ID:
            pbLocMsgId = PB_E_LOCAPI_GET_SINGLE_TERRESTRIAL_POS_RESP_MSG_ID;
            break;
        case E_LOCAPI_PINGTEST_MSG_ID:
            pbLocMsgId = PB_E_LOCAPI_PINGTEST_MSG_ID;
            break;
        case E_INTAPI_CONFIG_CONSTRAINTED_TUNC_MSG_ID:
            pbLocMsgId = PB_E_INTAPI_CONFIG_CONSTRAINTED_TUNC_MSG_ID;
            break;
        case E_INTAPI_CONFIG_POSITION_ASSISTED_CLOCK_ESTIMATOR_MSG_ID:
            pbLocMsgId = PB_E_INTAPI_CONFIG_POSITION_ASSISTED_CLOCK_ESTIMATOR_MSG_ID;
            break;
        case E_INTAPI_CONFIG_SV_CONSTELLATION_MSG_ID:
            pbLocMsgId = PB_E_INTAPI_CONFIG_SV_CONSTELLATION_MSG_ID;
            break;
        case E_INTAPI_CONFIG_AIDING_DATA_DELETION_MSG_ID:
            pbLocMsgId = PB_E_INTAPI_CONFIG_AIDING_DATA_DELETION_MSG_ID;
            break;
        case E_INTAPI_CONFIG_LEVER_ARM_MSG_ID:
            pbLocMsgId = PB_E_INTAPI_CONFIG_LEVER_ARM_MSG_ID;
            break;
        case E_INTAPI_CONFIG_ROBUST_LOCATION_MSG_ID:
            pbLocMsgId = PB_E_INTAPI_CONFIG_ROBUST_LOCATION_MSG_ID;
            break;
        case E_INTAPI_CONFIG_MIN_GPS_WEEK_MSG_ID:
            pbLocMsgId = PB_E_INTAPI_CONFIG_MIN_GPS_WEEK_MSG_ID;
            break;
        case E_INTAPI_CONFIG_DEAD_RECKONING_ENGINE_MSG_ID:
            pbLocMsgId = PB_E_INTAPI_CONFIG_DEAD_RECKONING_ENGINE_MSG_ID;
            break;
        case E_INTAPI_CONFIG_MIN_SV_ELEVATION_MSG_ID:
            pbLocMsgId = PB_E_INTAPI_CONFIG_MIN_SV_ELEVATION_MSG_ID;
            break;
        case E_INTAPI_CONFIG_CONSTELLATION_SECONDARY_BAND_MSG_ID:
            pbLocMsgId = PB_E_INTAPI_CONFIG_CONSTELLATION_SECONDARY_BAND_MSG_ID;
            break;
        case E_INTAPI_CONFIG_ENGINE_RUN_STATE_MSG_ID:
            pbLocMsgId = PB_E_INTAPI_CONFIG_ENGINE_RUN_STATE_MSG_ID;
            break;
        case E_INTAPI_CONFIG_USER_CONSENT_TERRESTRIAL_POSITIONING_MSG_ID:
            pbLocMsgId = PB_E_INTAPI_CONFIG_USER_CONSENT_TERRESTRIAL_POSITIONING_MSG_ID;
            break;
        case E_INTAPI_GET_ROBUST_LOCATION_CONFIG_REQ_MSG_ID:
            pbLocMsgId = PB_E_INTAPI_GET_ROBUST_LOCATION_CONFIG_REQ_MSG_ID;
            break;
        case E_INTAPI_GET_ROBUST_LOCATION_CONFIG_RESP_MSG_ID:
            pbLocMsgId = PB_E_INTAPI_GET_ROBUST_LOCATION_CONFIG_RESP_MSG_ID;
            break;
        case E_INTAPI_GET_MIN_GPS_WEEK_REQ_MSG_ID:
            pbLocMsgId = PB_E_INTAPI_GET_MIN_GPS_WEEK_REQ_MSG_ID;
            break;
        case E_INTAPI_GET_MIN_GPS_WEEK_RESP_MSG_ID:
            pbLocMsgId = PB_E_INTAPI_GET_MIN_GPS_WEEK_RESP_MSG_ID;
            break;
        case E_INTAPI_GET_MIN_SV_ELEVATION_REQ_MSG_ID:
            pbLocMsgId = PB_E_INTAPI_GET_MIN_SV_ELEVATION_REQ_MSG_ID;
            break;
        case E_INTAPI_GET_MIN_SV_ELEVATION_RESP_MSG_ID:
            pbLocMsgId = PB_E_INTAPI_GET_MIN_SV_ELEVATION_RESP_MSG_ID;
            break;
        case E_INTAPI_GET_CONSTELLATION_SECONDARY_BAND_CONFIG_REQ_MSG_ID:
            pbLocMsgId = PB_E_INTAPI_GET_CONSTELLATION_SECONDARY_BAND_CONFIG_REQ_MSG_ID;
            break;
        case E_INTAPI_GET_CONSTELLATION_SECONDARY_BAND_CONFIG_RESP_MSG_ID:
            pbLocMsgId = PB_E_INTAPI_GET_CONSTELLATION_SECONDARY_BAND_CONFIG_RESP_MSG_ID;
            break;
        default:
            break;
    }
    LocApiPb_LOGv("LocApiPB: eLocMsgId:%d, pbLocMsgId:%d", eLocMsgId, pbLocMsgId);
    return pbLocMsgId;
}

PBClientType LocationApiPbMsgConv::getPBEnumForClientType(const ClientType &clientTyp) const {
    PBClientType pbClntTyp = PB_LOCATION_CLIENT_TYPE_INVALID;
    if (LOCATION_CLIENT_API == clientTyp) {
        pbClntTyp = PB_LOCATION_CLIENT_API;
    } else {
        // LOCATION_INTEGRATION_API
        pbClntTyp = PB_LOCATION_INTEGRATION_API;
    }
    return pbClntTyp;
}

PBBatchingMode LocationApiPbMsgConv::getPBEnumForBatchingMode(
        const BatchingMode &batchMode) const {
    PBBatchingMode pbBatchMode = PB_BATCHING_MODE_ROUTINE;
    switch (batchMode) {
        case BATCHING_MODE_ROUTINE:
            pbBatchMode = PB_BATCHING_MODE_ROUTINE;
            break;
        case BATCHING_MODE_TRIP:
            pbBatchMode = PB_BATCHING_MODE_TRIP;
            break;
        case BATCHING_MODE_NO_AUTO_REPORT:
            pbBatchMode = PB_BATCHING_MODE_NO_AUTO_REPORT;
            break;
        default:
            break;
    }
    LocApiPb_LOGv("LocApiPB: batchMode:%d, pbBatchMode:%d", batchMode, pbBatchMode);
    return pbBatchMode;
}

PBLocationError LocationApiPbMsgConv::getPBEnumForLocationError(
        const LocationError &locErr) const {
    PBLocationError pbLocErr = PB_LOCATION_ERROR_GENERAL_FAILURE;
    switch (locErr) {
        case LOCATION_ERROR_SUCCESS:
            pbLocErr = PB_LOCATION_ERROR_SUCCESS;
            break;
        case LOCATION_ERROR_INVALID_PARAMETER:
            pbLocErr = PB_LOCATION_ERROR_INVALID_PARAMETER;
            break;
        case LOCATION_ERROR_NOT_SUPPORTED:
            pbLocErr = PB_LOCATION_ERROR_NOT_SUPPORTED;
            break;
        case LOCATION_ERROR_TIMEOUT:
            pbLocErr = PB_LOCATION_ERROR_TIMEOUT;
            break;
        default:
            break;
    }
    return pbLocErr;
}

PBGnssSuplMode LocationApiPbMsgConv::getPBEnumForGnssSuplMode(
        const GnssSuplMode &gnssSuplMode) const {
    PBGnssSuplMode pbGnssSuplMode = PB_GNSS_SUPL_MODE_STANDALONE;
    switch (gnssSuplMode) {
        case GNSS_SUPL_MODE_STANDALONE:
            pbGnssSuplMode = PB_GNSS_SUPL_MODE_STANDALONE;
            break;
        case GNSS_SUPL_MODE_MSB:
            pbGnssSuplMode = PB_GNSS_SUPL_MODE_MSB;
            break;
        case GNSS_SUPL_MODE_MSA:
            pbGnssSuplMode = PB_GNSS_SUPL_MODE_MSA;
            break;
        default:
            break;
    }
    LocApiPb_LOGv("LocApiPB: gnssSuplMode:%d, pbGnssSuplMode:%d", gnssSuplMode, pbGnssSuplMode);
    return pbGnssSuplMode;
}

PBBatchingStatus LocationApiPbMsgConv::getPBEnumForBatchingStatus(
        const BatchingStatus &batchStatus) const {
    PBBatchingStatus pbBatchStatus = PB_BATCHING_STATUS_POSITION_UNAVAILABLE;
    switch (batchStatus) {
        case BATCHING_STATUS_TRIP_COMPLETED:
            pbBatchStatus = PB_BATCHING_STATUS_TRIP_COMPLETED;
            break;
        case BATCHING_STATUS_POSITION_AVAILABE:
            pbBatchStatus = PB_BATCHING_STATUS_POSITION_AVAILABE;
            break;
        case BATCHING_STATUS_POSITION_UNAVAILABLE:
            pbBatchStatus = PB_BATCHING_STATUS_POSITION_UNAVAILABLE;
            break;
        default:
            break;
    }
    LocApiPb_LOGv("LocApiPB: batchStatus:%d, pbGnssSuplMode:%d", batchStatus, pbBatchStatus);
    return pbBatchStatus;
}

PBLocationReliability LocationApiPbMsgConv::getPBEnumForLocationReliability(
        const LocationReliability &locReliab) const {
    PBLocationReliability pbLocReliab = PB_LOCATION_RELIABILITY_NOT_SET;
    switch (locReliab) {
        case LOCATION_RELIABILITY_NOT_SET:
            pbLocReliab = PB_LOCATION_RELIABILITY_NOT_SET;
            break;
        case LOCATION_RELIABILITY_VERY_LOW:
            pbLocReliab = PB_LOCATION_RELIABILITY_VERY_LOW;
            break;
        case LOCATION_RELIABILITY_LOW:
            pbLocReliab = PB_LOCATION_RELIABILITY_LOW;
            break;
        case LOCATION_RELIABILITY_MEDIUM:
            pbLocReliab = PB_LOCATION_RELIABILITY_MEDIUM;
            break;
        case LOCATION_RELIABILITY_HIGH:
            pbLocReliab = PB_LOCATION_RELIABILITY_HIGH;
            break;
        default:
            break;
    }
    LocApiPb_LOGv("LocApiPB: locReliab:%d, pbLocReliab:%d", locReliab, pbLocReliab);
    return pbLocReliab;
}

PBLocApiOutputEngineType LocationApiPbMsgConv::getPBEnumForLocOutputEngineType(
        const LocOutputEngineType &locOpEng) const {
    PBLocApiOutputEngineType pbLocOpEng = PB_LOC_OUTPUT_ENGINE_FUSED;
    switch (locOpEng) {
        case LOC_OUTPUT_ENGINE_FUSED:
            pbLocOpEng = PB_LOC_OUTPUT_ENGINE_FUSED;
            break;
        case LOC_OUTPUT_ENGINE_SPE:
            pbLocOpEng = PB_LOC_OUTPUT_ENGINE_SPE;
            break;
        case LOC_OUTPUT_ENGINE_PPE:
            pbLocOpEng = PB_LOC_OUTPUT_ENGINE_PPE;
            break;
        default:
            break;
    }
    LocApiPb_LOGv("LocApiPB: locOpEng:%d, pbLocOpEng:%d", locOpEng, pbLocOpEng);
    return pbLocOpEng;
}

PBGnssMeasurementsMultipathIndicator LocationApiPbMsgConv::getPBEnumForGnssMeasMultiPathIndic(
            const GnssMeasurementsMultipathIndicator &gnssMeasMultiPathIndic) const {
    PBGnssMeasurementsMultipathIndicator pbGnssMeasMultiPathIndic =
            PB_GNSS_MEASUREMENTS_MULTIPATH_INDICATOR_UNKNOWN;
    switch (gnssMeasMultiPathIndic) {
        case GNSS_MEASUREMENTS_MULTIPATH_INDICATOR_UNKNOWN:
            pbGnssMeasMultiPathIndic = PB_GNSS_MEASUREMENTS_MULTIPATH_INDICATOR_UNKNOWN;
            break;
        case GNSS_MEASUREMENTS_MULTIPATH_INDICATOR_PRESENT:
            pbGnssMeasMultiPathIndic = PB_GNSS_MEASUREMENTS_MULTIPATH_INDICATOR_PRESENT;
            break;
        case GNSS_MEASUREMENTS_MULTIPATH_INDICATOR_NOT_PRESENT:
            pbGnssMeasMultiPathIndic = PB_GNSS_MEASUREMENTS_MULTIPATH_INDICATOR_NOT_PRESENT;
            break;
        default:
            break;
    }
    LocApiPb_LOGv("LocApiPB: gnssMeasMultiPathIndic:%d, pbGnssMeasMultiPathIndic:%d",
            gnssMeasMultiPathIndic, pbGnssMeasMultiPathIndic);
    return pbGnssMeasMultiPathIndic;
}

PBLocApiGnss_LocSvSystemEnumType LocationApiPbMsgConv::getPBEnumForGnssLocSvSystem(
            const Gnss_LocSvSystemEnumType &gnssLocSvSysEnumType) const {
    PBLocApiGnss_LocSvSystemEnumType pbGnssLocSvSysEnumType = PB_GNSS_LOC_SV_SYSTEM_INVALID;
    switch (gnssLocSvSysEnumType) {
        case GNSS_LOC_SV_SYSTEM_GPS:
            pbGnssLocSvSysEnumType = PB_GNSS_LOC_SV_SYSTEM_GPS;
            break;
        case GNSS_LOC_SV_SYSTEM_GALILEO:
            pbGnssLocSvSysEnumType = PB_GNSS_LOC_SV_SYSTEM_GALILEO;
            break;
        case GNSS_LOC_SV_SYSTEM_SBAS:
            pbGnssLocSvSysEnumType = PB_GNSS_LOC_SV_SYSTEM_SBAS;
            break;
        case GNSS_LOC_SV_SYSTEM_GLONASS:
            pbGnssLocSvSysEnumType = PB_GNSS_LOC_SV_SYSTEM_GLONASS;
            break;
        case GNSS_LOC_SV_SYSTEM_BDS:
            pbGnssLocSvSysEnumType = PB_GNSS_LOC_SV_SYSTEM_BDS;
            break;
        case GNSS_LOC_SV_SYSTEM_QZSS:
            pbGnssLocSvSysEnumType = PB_GNSS_LOC_SV_SYSTEM_QZSS;
            break;
        case GNSS_LOC_SV_SYSTEM_NAVIC:
            pbGnssLocSvSysEnumType = PB_GNSS_LOC_SV_SYSTEM_NAVIC;
            break;
        default:
            break;
    }
    LocApiPb_LOGv("LocApiPB: gnssLocSvSysEnumType:%d, pbGnssLocSvSysEnumType:%d",
            gnssLocSvSysEnumType, pbGnssLocSvSysEnumType);
    return pbGnssLocSvSysEnumType;
}

// GnssSvType to PBLocApiGnss_LocSvSystemEnumType
PBLocApiGnss_LocSvSystemEnumType LocationApiPbMsgConv::getPBGnssLocSvSysEnumFromGnssSvType(
        const GnssSvType &gnssSvType) const {
    PBLocApiGnss_LocSvSystemEnumType pbGnssLocSvSysEnum = PB_GNSS_LOC_SV_SYSTEM_INVALID;
    switch (gnssSvType) {
        case GNSS_SV_TYPE_UNKNOWN:
            pbGnssLocSvSysEnum = PB_GNSS_LOC_SV_SYSTEM_INVALID;
            break;
        case GNSS_SV_TYPE_GPS:
            pbGnssLocSvSysEnum = PB_GNSS_LOC_SV_SYSTEM_GPS;
            break;
        case GNSS_SV_TYPE_SBAS:
            pbGnssLocSvSysEnum = PB_GNSS_LOC_SV_SYSTEM_SBAS;
            break;
        case GNSS_SV_TYPE_GLONASS:
            pbGnssLocSvSysEnum = PB_GNSS_LOC_SV_SYSTEM_GLONASS;
            break;
        case GNSS_SV_TYPE_QZSS:
            pbGnssLocSvSysEnum = PB_GNSS_LOC_SV_SYSTEM_QZSS;
            break;
        case GNSS_SV_TYPE_BEIDOU:
            pbGnssLocSvSysEnum = PB_GNSS_LOC_SV_SYSTEM_BDS;
            break;
        case GNSS_SV_TYPE_GALILEO:
            pbGnssLocSvSysEnum = PB_GNSS_LOC_SV_SYSTEM_GALILEO;
            break;
        case GNSS_SV_TYPE_NAVIC:
            pbGnssLocSvSysEnum = PB_GNSS_LOC_SV_SYSTEM_NAVIC;
            break;
        default:
            break;
    }
    LocApiPb_LOGv("LocApiPB: gnssSvType:%d, pbGnssLocSvSysEnum:%d", gnssSvType,
            pbGnssLocSvSysEnum);
    return pbGnssLocSvSysEnum;
}

// **** helper function for mask conversion to protobuf masks
uint32_t LocationApiPbMsgConv::getPBMaskForLocationCallbacksMask(const uint32_t &locCbMask) const {
    uint32_t pbLocCbMask = 0;
    if (locCbMask & E_LOC_CB_DISTANCE_BASED_TRACKING_BIT) {
        pbLocCbMask |= PB_E_LOC_CB_DISTANCE_BASED_TRACKING_BIT;
    }
    if (locCbMask & E_LOC_CB_GNSS_LOCATION_INFO_BIT) {
        pbLocCbMask |= PB_E_LOC_CB_GNSS_LOCATION_INFO_BIT;
    }
    if (locCbMask & E_LOC_CB_GNSS_SV_BIT) {
        pbLocCbMask |= PB_E_LOC_CB_GNSS_SV_BIT;
    }
    if (locCbMask & E_LOC_CB_GNSS_NMEA_BIT) {
        pbLocCbMask |= PB_E_LOC_CB_GNSS_NMEA_BIT;
    }
    if (locCbMask & E_LOC_CB_GNSS_DATA_BIT) {
        pbLocCbMask |= PB_E_LOC_CB_GNSS_DATA_BIT;
    }
    if (locCbMask & E_LOC_CB_SYSTEM_INFO_BIT) {
        pbLocCbMask |= PB_E_LOC_CB_SYSTEM_INFO_BIT;
    }
    if (locCbMask & E_LOC_CB_BATCHING_BIT) {
        pbLocCbMask |= PB_E_LOC_CB_BATCHING_BIT;
    }
    if (locCbMask & E_LOC_CB_BATCHING_STATUS_BIT) {
        pbLocCbMask |= PB_E_LOC_CB_BATCHING_STATUS_BIT;
    }
    if (locCbMask & E_LOC_CB_GEOFENCE_BREACH_BIT) {
        pbLocCbMask |= PB_E_LOC_CB_GEOFENCE_BREACH_BIT;
    }
    if (locCbMask & E_LOC_CB_ENGINE_LOCATIONS_INFO_BIT) {
        pbLocCbMask |= PB_E_LOC_CB_ENGINE_LOCATIONS_INFO_BIT;
    }
    if (locCbMask & E_LOC_CB_SIMPLE_LOCATION_INFO_BIT) {
        pbLocCbMask |= PB_E_LOC_CB_SIMPLE_LOCATION_INFO_BIT;
    }
    if (locCbMask & E_LOC_CB_GNSS_MEAS_BIT) {
        pbLocCbMask |= PB_E_LOC_CB_GNSS_MEAS_BIT;
    }
    LocApiPb_LOGv("LocApiPB: locCbMask:%x, pbLocCbMask:%x", locCbMask, pbLocCbMask);
    return pbLocCbMask;
}

uint64_t LocationApiPbMsgConv::getPBMaskForLocationCapabilitiesMask(
        const uint64_t &locCapabMask) const {
    uint64_t pbLocCapabMask = 0;
    if (locCapabMask & LOCATION_CAPABILITIES_TIME_BASED_TRACKING_BIT) {
        pbLocCapabMask |= PB_LOCATION_CAPS_TIME_BASED_TRACKING_BIT;
    }
    if (locCapabMask & LOCATION_CAPABILITIES_TIME_BASED_BATCHING_BIT) {
        pbLocCapabMask |= PB_LOCATION_CAPS_TIME_BASED_BATCHING_BIT;
    }
    if (locCapabMask & LOCATION_CAPABILITIES_DISTANCE_BASED_TRACKING_BIT) {
        pbLocCapabMask |= PB_LOCATION_CAPS_DISTANCE_BASED_TRACKING_BIT;
    }
    if (locCapabMask & LOCATION_CAPABILITIES_DISTANCE_BASED_BATCHING_BIT) {
        pbLocCapabMask |= PB_LOCATION_CAPS_DISTANCE_BASED_BATCHING_BIT;
    }
    if (locCapabMask & LOCATION_CAPABILITIES_GEOFENCE_BIT) {
        pbLocCapabMask |= PB_LOCATION_CAPS_GEOFENCE_BIT;
    }
    if (locCapabMask & LOCATION_CAPABILITIES_OUTDOOR_TRIP_BATCHING_BIT) {
        pbLocCapabMask |= PB_LOCATION_CAPS_OUTDOOR_TRIP_BATCHING_BIT;
    }
    if (locCapabMask & LOCATION_CAPABILITIES_GNSS_MEASUREMENTS_BIT) {
        pbLocCapabMask |= PB_LOCATION_CAPS_GNSS_MEASUREMENTS_BIT;
    }
    if (locCapabMask & LOCATION_CAPABILITIES_CONSTELLATION_ENABLEMENT_BIT) {
        pbLocCapabMask |= PB_LOCATION_CAPS_CONSTELLATION_ENABLEMENT_BIT;
    }
    if (locCapabMask & LOCATION_CAPABILITIES_QWES_CARRIER_PHASE_BIT) {
        pbLocCapabMask |= PB_LOCATION_CAPS_QWES_CARRIER_PHASE_BIT;
    }
    if (locCapabMask & LOCATION_CAPABILITIES_QWES_SV_POLYNOMIAL_BIT) {
        pbLocCapabMask |= PB_LOCATION_CAPS_QWES_SV_POLYNOMIAL_BIT;
    }
    if (locCapabMask & LOCATION_CAPABILITIES_QWES_GNSS_SINGLE_FREQUENCY) {
        pbLocCapabMask |= PB_LOCATION_CAPS_QWES_GNSS_SINGLE_FREQUENCY;
    }
    if (locCapabMask & LOCATION_CAPABILITIES_QWES_GNSS_MULTI_FREQUENCY) {
        pbLocCapabMask |= PB_LOCATION_CAPS_QWES_GNSS_MULTI_FREQUENCY;
    }
    if (locCapabMask & LOCATION_CAPABILITIES_QWES_VPE) {
        pbLocCapabMask |= PB_LOCATION_CAPS_QWES_VPE;
    }
    if (locCapabMask & LOCATION_CAPABILITIES_QWES_CV2X_LOCATION_BASIC) {
        pbLocCapabMask |= PB_LOCATION_CAPS_QWES_CV2X_LOCATION_BASIC;
    }
    if (locCapabMask & LOCATION_CAPABILITIES_QWES_CV2X_LOCATION_PREMIUM) {
        pbLocCapabMask |= PB_LOCATION_CAPS_QWES_CV2X_LOCATION_PREMIUM;
    }
    if (locCapabMask & LOCATION_CAPABILITIES_QWES_PPE) {
        pbLocCapabMask |= PB_LOCATION_CAPS_QWES_PPE;
    }
    if (locCapabMask & LOCATION_CAPABILITIES_QWES_QDR2) {
        pbLocCapabMask |= PB_LOCATION_CAPS_QWES_QDR2;
    }
    if (locCapabMask & LOCATION_CAPABILITIES_QWES_QDR3) {
        pbLocCapabMask |= PB_LOCATION_CAPS_QWES_QDR3;
    }
    LOC_LOGi("LocApiPB: locCapabMask:0x%" PRIx64", pbLocCapabMask:0x%" PRIx64,
            locCapabMask, pbLocCapabMask);
    return pbLocCapabMask;
}

uint32_t LocationApiPbMsgConv::getPBMaskForPositioningEngineMask(
        const uint32_t &posEngMask) const {
    uint32_t pbPosEngMask = 0;
    if (posEngMask & STANDARD_POSITIONING_ENGINE) {
        pbPosEngMask |= PB_STANDARD_POSITIONING_ENGINE;
    }
    if (posEngMask & DEAD_RECKONING_ENGINE) {
        pbPosEngMask |= PB_DEAD_RECKONING_ENGINE;
    }
    if (posEngMask & PRECISE_POSITIONING_ENGINE) {
        pbPosEngMask |= PB_PRECISE_POSITIONING_ENGINE;
    }
    LocApiPb_LOGv("LocApiPB: posEngMask:%x, pbPosEngMask:%x", posEngMask, pbPosEngMask);
    return pbPosEngMask;
}

uint32_t LocationApiPbMsgConv::getPBMaskForLeverArmTypeMask(
        const uint32_t &leverArmTypeMask) const {
    uint32_t pbLeverArmTypeMask = 0;
    if (leverArmTypeMask & LEVER_ARM_TYPE_GNSS_TO_VRP_BIT) {
        pbLeverArmTypeMask |= PB_LEVER_ARM_TYPE_GNSS_TO_VRP_BIT;
    }
    if (leverArmTypeMask & LEVER_ARM_TYPE_DR_IMU_TO_GNSS_BIT) {
        pbLeverArmTypeMask |= PB_LEVER_ARM_TYPE_DR_IMU_TO_GNSS_BIT;
    }
    if (leverArmTypeMask & LEVER_ARM_TYPE_VEPP_IMU_TO_GNSS_BIT) {
        pbLeverArmTypeMask |= PB_LEVER_ARM_TYPE_VEPP_IMU_TO_GNSS_BIT;
    }
    LocApiPb_LOGv("LocApiPB: leverArmTypeMask:%x, pbLeverArmTypeMask:%x",
            leverArmTypeMask, pbLeverArmTypeMask);
    return pbLeverArmTypeMask;
}

uint32_t LocationApiPbMsgConv::getPBMaskForLocReqEngineTypeMask(
        const uint32_t &locReqEngTypeMask) const {
    uint32_t pbLocReqEngTypeMask = 0;
    if (locReqEngTypeMask & LOC_REQ_ENGINE_FUSED_BIT) {
        pbLocReqEngTypeMask |= PB_LOC_REQ_ENGINE_FUSED_BIT;
    }
    if (locReqEngTypeMask & LOC_REQ_ENGINE_SPE_BIT) {
        pbLocReqEngTypeMask |= PB_LOC_REQ_ENGINE_SPE_BIT;
    }
    if (locReqEngTypeMask & LOC_REQ_ENGINE_PPE_BIT) {
        pbLocReqEngTypeMask |= PB_LOC_REQ_ENGINE_PPE_BIT;
    }
    LocApiPb_LOGv("LocApiPB: locReqEngTypeMask:%x, pbLocReqEngTypeMask:%x",
            locReqEngTypeMask, pbLocReqEngTypeMask);
    return pbLocReqEngTypeMask;
}

uint32_t LocationApiPbMsgConv::getPBMaskForGnssCfgRobustLocValidMask(
        const uint32_t &gnssCfgRobustLocValidMask) const {
    uint32_t pbGnssCfgRobustLocValidMask = 0;
    if (gnssCfgRobustLocValidMask & GNSS_CONFIG_ROBUST_LOCATION_ENABLED_VALID_BIT) {
        pbGnssCfgRobustLocValidMask |= PB_GNSS_CONFIG_ROBUST_LOCATION_ENABLED_VALID_BIT;
    }
    if (gnssCfgRobustLocValidMask & GNSS_CONFIG_ROBUST_LOCATION_ENABLED_FOR_E911_VALID_BIT) {
        pbGnssCfgRobustLocValidMask |= PB_GNSS_CONFIG_ROBUST_LOCATION_ENABLED_FOR_E911_VALID_BIT;
    }
    if (gnssCfgRobustLocValidMask & GNSS_CONFIG_ROBUST_LOCATION_VERSION_VALID_BIT) {
        pbGnssCfgRobustLocValidMask |= PB_GNSS_CONFIG_ROBUST_LOCATION_VERSION_VALID_BIT;
    }
    LocApiPb_LOGv("LocApiPB: gnssCfgRobustLocValidMask:%x, pbGnssCfgRobustLocValidMask:%x",
            gnssCfgRobustLocValidMask, pbGnssCfgRobustLocValidMask);
    return pbGnssCfgRobustLocValidMask;
}

uint32_t LocationApiPbMsgConv::getPBMaskForLocSysInfoMask(const uint32_t &locSysInfoMask) const {
    uint32_t pbLocSysInfoMask = 0;
    if (locSysInfoMask & LOCATION_SYS_INFO_LEAP_SECOND) {
        pbLocSysInfoMask |= PB_LOCATION_SYS_INFO_LEAP_SECOND;
    }
    LocApiPb_LOGv("LocApiPB: locSysInfoMask:%x, pbLocSysInfoMask:%x", locSysInfoMask,
            pbLocSysInfoMask);
    return pbLocSysInfoMask;
}

uint64_t LocationApiPbMsgConv::getPBMaskForGnssDataMask(const uint64_t &gnssDataMask) const {
    uint64_t pbGnssDataMask = 0;
    if (gnssDataMask & GNSS_LOC_DATA_JAMMER_IND_BIT) {
        pbGnssDataMask |= PB_GNSS_LOC_DATA_JAMMER_IND_BIT;
    }
    if (gnssDataMask & GNSS_LOC_DATA_AGC_BIT) {
        pbGnssDataMask |= PB_GNSS_LOC_DATA_AGC_BIT;
    }
    LocApiPb_LOGv("LocApiPB: locSysInfoMask:%" PRIu64", pbGnssDataMask:%" PRIu64,
            gnssDataMask, pbGnssDataMask);
    return pbGnssDataMask;
}

uint32_t LocationApiPbMsgConv::getPBMaskForGnssAidingDataSvMask(
        const uint32_t &gnssAidDataSvMask) const {
    uint32_t pbGnssAidDataSvMask = 0;
    if (gnssAidDataSvMask & GNSS_AIDING_DATA_SV_EPHEMERIS_BIT) {
        pbGnssAidDataSvMask |= PB_AIDING_DATA_SV_EPHEMERIS_BIT;
    }
    LocApiPb_LOGv("LocApiPB: gnssAidDataSvMask:%x, pbGnssAidDataSvMask:%x",
            gnssAidDataSvMask, pbGnssAidDataSvMask);
    return pbGnssAidDataSvMask;
}

uint32_t LocationApiPbMsgConv::getPBMaskForLeapSecondSysInfoMask(
        const uint32_t &leapSecSysInfoMask) const {
    uint32_t pbLeapSecSysInfoMask = 0;
    if (leapSecSysInfoMask & LEAP_SECOND_SYS_INFO_CURRENT_LEAP_SECONDS_BIT) {
        pbLeapSecSysInfoMask |= PB_LEAP_SECOND_SYS_INFO_CURRENT_LEAP_SECONDS_BIT;
    }
    if (leapSecSysInfoMask & LEAP_SECOND_SYS_INFO_LEAP_SECOND_CHANGE_BIT) {
        pbLeapSecSysInfoMask |= PB_LEAP_SECOND_SYS_INFO_LEAP_SECOND_CHANGE_BIT;
    }

    LocApiPb_LOGv("LocApiPB: leapSecSysInfoMask:%x, pbLeapSecSysInfoMask:%x",
            leapSecSysInfoMask, pbLeapSecSysInfoMask);
    return pbLeapSecSysInfoMask;
}

uint32_t LocationApiPbMsgConv::getPBMaskForGnssSystemTimeStructTypeFlags(
        const uint32_t &gnssSysTimeStructTypeFlg) const {
    uint32_t pbGnssSysTimeStructTypeFlg = 0;
    if (gnssSysTimeStructTypeFlg & GNSS_SYSTEM_TIME_WEEK_VALID) {
        pbGnssSysTimeStructTypeFlg |= PB_GNSS_SYSTEM_TIME_WEEK_VALID;
    }
    if (gnssSysTimeStructTypeFlg & GNSS_SYSTEM_TIME_WEEK_MS_VALID) {
        pbGnssSysTimeStructTypeFlg |= PB_GNSS_SYSTEM_TIME_WEEK_MS_VALID;
    }
    if (gnssSysTimeStructTypeFlg & GNSS_SYSTEM_CLK_TIME_BIAS_VALID) {
        pbGnssSysTimeStructTypeFlg |= PB_GNSS_SYSTEM_CLK_TIME_BIAS_VALID;
    }
    if (gnssSysTimeStructTypeFlg & GNSS_SYSTEM_CLK_TIME_BIAS_UNC_VALID) {
        pbGnssSysTimeStructTypeFlg |= PB_GNSS_SYSTEM_CLK_TIME_BIAS_UNC_VALID;
    }
    if (gnssSysTimeStructTypeFlg & GNSS_SYSTEM_REF_FCOUNT_VALID) {
        pbGnssSysTimeStructTypeFlg |= PB_GNSS_SYSTEM_REF_FCOUNT_VALID;
    }
    if (gnssSysTimeStructTypeFlg & GNSS_SYSTEM_NUM_CLOCK_RESETS_VALID) {
        pbGnssSysTimeStructTypeFlg |= PB_GNSS_SYSTEM_NUM_CLOCK_RESETS_VALID;
    }
    LocApiPb_LOGv("LocApiPB: gnssSysTimeStructTypeFlg:%x, pbGnssSysTimeStructTypeFlg:%x",
            gnssSysTimeStructTypeFlg, pbGnssSysTimeStructTypeFlg);
    return pbGnssSysTimeStructTypeFlg;
}

uint32_t LocationApiPbMsgConv::getPBMaskForLocationFlagsMask(const uint32_t &locFlagsMask) const {
    uint32_t pbLocFlagsMask = 0;
    if (locFlagsMask & LOCATION_HAS_LAT_LONG_BIT) {
        pbLocFlagsMask |= PB_LOCATION_HAS_LAT_LONG_BIT;
    }
    if (locFlagsMask & LOCATION_HAS_ALTITUDE_BIT) {
        pbLocFlagsMask |= PB_LOCATION_HAS_ALTITUDE_BIT;
    }
    if (locFlagsMask & LOCATION_HAS_SPEED_BIT) {
        pbLocFlagsMask |= PB_LOCATION_HAS_SPEED_BIT;
    }
    if (locFlagsMask & LOCATION_HAS_BEARING_BIT) {
        pbLocFlagsMask |= PB_LOCATION_HAS_BEARING_BIT;
    }
    if (locFlagsMask & LOCATION_HAS_ACCURACY_BIT) {
        pbLocFlagsMask |= PB_LOCATION_HAS_ACCURACY_BIT;
    }
    if (locFlagsMask & LOCATION_HAS_VERTICAL_ACCURACY_BIT) {
        pbLocFlagsMask |= PB_LOCATION_HAS_VERTICAL_ACCURACY_BIT;
    }
    if (locFlagsMask & LOCATION_HAS_SPEED_ACCURACY_BIT) {
        pbLocFlagsMask |= PB_LOCATION_HAS_SPEED_ACCURACY_BIT;
    }
    if (locFlagsMask & LOCATION_HAS_BEARING_ACCURACY_BIT) {
        pbLocFlagsMask |= PB_LOCATION_HAS_BEARING_ACCURACY_BIT;
    }
    LocApiPb_LOGv("LocApiPB: locFlagsMask:%x, pbLocFlagsMask:%x", locFlagsMask, pbLocFlagsMask);
    return pbLocFlagsMask;
}

uint32_t LocationApiPbMsgConv::getPBMaskForLocationTechnologyMask(
        const uint32_t &locTechMask) const {
    uint32_t pbLocTechMask = 0;
    if (locTechMask & LOCATION_TECHNOLOGY_GNSS_BIT) {
        pbLocTechMask |= PB_LOCATION_TECHNOLOGY_GNSS_BIT;
    }
    if (locTechMask & LOCATION_TECHNOLOGY_CELL_BIT) {
        pbLocTechMask |= PB_LOCATION_TECHNOLOGY_CELL_BIT;
    }
    if (locTechMask & LOCATION_TECHNOLOGY_WIFI_BIT) {
        pbLocTechMask |= PB_LOCATION_TECHNOLOGY_WIFI_BIT;
    }
    if (locTechMask & LOCATION_TECHNOLOGY_SENSORS_BIT) {
        pbLocTechMask |= PB_LOCATION_TECHNOLOGY_SENSORS_BIT;
    }
    if (locTechMask & LOCATION_TECHNOLOGY_REFERENCE_LOCATION_BIT) {
        pbLocTechMask |= PB_LOCATION_TECHNOLOGY_REFERENCE_LOCATION_BIT;
    }
    if (locTechMask & LOCATION_TECHNOLOGY_INJECTED_COARSE_POSITION_BIT) {
        pbLocTechMask |= PB_LOCATION_TECHNOLOGY_INJECTED_COARSE_POSITION_BIT;
    }
    if (locTechMask & LOCATION_TECHNOLOGY_AFLT_BIT) {
        pbLocTechMask |= PB_LOCATION_TECHNOLOGY_AFLT_BIT;
    }
    if (locTechMask & LOCATION_TECHNOLOGY_HYBRID_BIT) {
        pbLocTechMask |= PB_LOCATION_TECHNOLOGY_HYBRID_BIT;
    }
    if (locTechMask & LOCATION_TECHNOLOGY_PPE_BIT) {
        pbLocTechMask |= PB_LOCATION_TECHNOLOGY_PPE_BIT;
    }
    if (locTechMask & LOCATION_TECHNOLOGY_VEH_BIT) {
        pbLocTechMask |= PB_LOCATION_TECHNOLOGY_VEH_BIT;
    }
    if (locTechMask & LOCATION_TECHNOLOGY_VIS_BIT) {
        pbLocTechMask |= PB_LOCATION_TECHNOLOGY_VIS_BIT;
    }
    LocApiPb_LOGv("LocApiPB: locTechMask:%x, pbLocTechMask:%x", locTechMask, pbLocTechMask);
    return pbLocTechMask;
}

uint32_t LocationApiPbMsgConv::getPBMaskForGnssLocationInfoFlagMask(
        const uint64_t &gnssLocInfoFlagMask) const {
    uint32_t pbGnssLocInfoFlagMask = 0;
    if (gnssLocInfoFlagMask & GNSS_LOCATION_INFO_ALTITUDE_MEAN_SEA_LEVEL_BIT) {
        pbGnssLocInfoFlagMask |= PB_GNSS_LOCATION_INFO_ALTITUDE_MEAN_SEA_LEVEL_BIT;
    }
    if (gnssLocInfoFlagMask & GNSS_LOCATION_INFO_DOP_BIT) {
        pbGnssLocInfoFlagMask |= PB_GNSS_LOCATION_INFO_DOP_BIT;
    }
    if (gnssLocInfoFlagMask & GNSS_LOCATION_INFO_MAGNETIC_DEVIATION_BIT) {
        pbGnssLocInfoFlagMask |= PB_GNSS_LOCATION_INFO_MAGNETIC_DEVIATION_BIT;
    }
    if (gnssLocInfoFlagMask & GNSS_LOCATION_INFO_HOR_RELIABILITY_BIT) {
        pbGnssLocInfoFlagMask |= PB_GNSS_LOCATION_INFO_HOR_RELIABILITY_BIT;
    }
    if (gnssLocInfoFlagMask & GNSS_LOCATION_INFO_VER_RELIABILITY_BIT) {
        pbGnssLocInfoFlagMask |= PB_GNSS_LOCATION_INFO_VER_RELIABILITY_BIT;
    }
    if (gnssLocInfoFlagMask & GNSS_LOCATION_INFO_HOR_ACCURACY_ELIP_SEMI_MAJOR_BIT) {
        pbGnssLocInfoFlagMask |= PB_GNSS_LOCATION_INFO_HOR_ACCURACY_ELIP_SEMI_MAJOR_BIT;
    }
    if (gnssLocInfoFlagMask & GNSS_LOCATION_INFO_HOR_ACCURACY_ELIP_SEMI_MINOR_BIT) {
        pbGnssLocInfoFlagMask |= PB_GNSS_LOCATION_INFO_HOR_ACCURACY_ELIP_SEMI_MINOR_BIT;
    }
    if (gnssLocInfoFlagMask & GNSS_LOCATION_INFO_HOR_ACCURACY_ELIP_AZIMUTH_BIT) {
        pbGnssLocInfoFlagMask |= PB_GNSS_LOCATION_INFO_HOR_ACCURACY_ELIP_AZIMUTH_BIT;
    }
    if (gnssLocInfoFlagMask & GNSS_LOCATION_INFO_GNSS_SV_USED_DATA_BIT) {
        pbGnssLocInfoFlagMask |= PB_GNSS_LOCATION_INFO_GNSS_SV_USED_DATA_BIT;
    }
    if (gnssLocInfoFlagMask & GNSS_LOCATION_INFO_NAV_SOLUTION_MASK_BIT) {
        pbGnssLocInfoFlagMask |= PB_GNSS_LOCATION_INFO_NAV_SOLUTION_MASK_BIT;
    }
    // GNSS_LOCATION_INFO_SV_SOURCE_INFO_BIT field is deprecated.
    if (gnssLocInfoFlagMask & GNSS_LOCATION_INFO_POS_DYNAMICS_DATA_BIT) {
        pbGnssLocInfoFlagMask |= PB_GNSS_LOCATION_INFO_POS_DYNAMICS_DATA_BIT;
    }
    if (gnssLocInfoFlagMask & GNSS_LOCATION_INFO_EXT_DOP_BIT) {
        pbGnssLocInfoFlagMask |= PB_GNSS_LOCATION_INFO_EXT_DOP_BIT;
    }
    if (gnssLocInfoFlagMask & GNSS_LOCATION_INFO_NORTH_STD_DEV_BIT) {
        pbGnssLocInfoFlagMask |= PB_GNSS_LOCATION_INFO_NORTH_STD_DEV_BIT;
    }
    if (gnssLocInfoFlagMask & GNSS_LOCATION_INFO_EAST_STD_DEV_BIT) {
        pbGnssLocInfoFlagMask |= PB_GNSS_LOCATION_INFO_EAST_STD_DEV_BIT;
    }
    if (gnssLocInfoFlagMask & GNSS_LOCATION_INFO_NORTH_VEL_BIT) {
        pbGnssLocInfoFlagMask |= PB_GNSS_LOCATION_INFO_NORTH_VEL_BIT;
    }
    if (gnssLocInfoFlagMask & GNSS_LOCATION_INFO_EAST_VEL_BIT) {
        pbGnssLocInfoFlagMask |= PB_GNSS_LOCATION_INFO_EAST_VEL_BIT;
    }
    if (gnssLocInfoFlagMask & GNSS_LOCATION_INFO_UP_VEL_BIT) {
        pbGnssLocInfoFlagMask |= PB_GNSS_LOCATION_INFO_UP_VEL_BIT;
    }
    if (gnssLocInfoFlagMask & GNSS_LOCATION_INFO_NORTH_VEL_UNC_BIT) {
        pbGnssLocInfoFlagMask |= PB_GNSS_LOCATION_INFO_NORTH_VEL_UNC_BIT;
    }
    if (gnssLocInfoFlagMask & GNSS_LOCATION_INFO_EAST_VEL_UNC_BIT) {
        pbGnssLocInfoFlagMask |= PB_GNSS_LOCATION_INFO_EAST_VEL_UNC_BIT;
    }
    if (gnssLocInfoFlagMask & GNSS_LOCATION_INFO_UP_VEL_UNC_BIT) {
        pbGnssLocInfoFlagMask |= PB_GNSS_LOCATION_INFO_UP_VEL_UNC_BIT;
    }
    if (gnssLocInfoFlagMask & GNSS_LOCATION_INFO_LEAP_SECONDS_BIT) {
        pbGnssLocInfoFlagMask |= PB_GNSS_LOCATION_INFO_LEAP_SECONDS_BIT;
    }
    if (gnssLocInfoFlagMask & GNSS_LOCATION_INFO_TIME_UNC_BIT) {
        pbGnssLocInfoFlagMask |= PB_GNSS_LOCATION_INFO_TIME_UNC_BIT;
    }
    if (gnssLocInfoFlagMask & GNSS_LOCATION_INFO_NUM_SV_USED_IN_POSITION_BIT) {
        pbGnssLocInfoFlagMask |= PB_GNSS_LOCATION_INFO_NUM_SV_USED_IN_POSITION_BIT;
    }
    if (gnssLocInfoFlagMask & GNSS_LOCATION_INFO_CALIBRATION_CONFIDENCE_BIT) {
        pbGnssLocInfoFlagMask |= PB_GNSS_LOCATION_INFO_CALIBRATION_CONFIDENCE_PERCENT_BIT;
    }
    if (gnssLocInfoFlagMask & GNSS_LOCATION_INFO_CALIBRATION_STATUS_BIT) {
        pbGnssLocInfoFlagMask |= PB_GNSS_LOCATION_INFO_CALIBRATION_STATUS_BIT;
    }
    if (gnssLocInfoFlagMask & GNSS_LOCATION_INFO_OUTPUT_ENG_TYPE_BIT) {
        pbGnssLocInfoFlagMask |= PB_GNSS_LOCATION_INFO_OUTPUT_ENG_TYPE_BIT;
    }
    if (gnssLocInfoFlagMask & GNSS_LOCATION_INFO_OUTPUT_ENG_MASK_BIT) {
        pbGnssLocInfoFlagMask |= PB_GNSS_LOCATION_INFO_OUTPUT_ENG_MASK_BIT;
    }
    if (gnssLocInfoFlagMask & GNSS_LOCATION_INFO_CONFORMITY_INDEX_BIT) {
        pbGnssLocInfoFlagMask |= PB_GNSS_LOCATION_INFO_CONFORMITY_INDEX_BIT;
    }
    if (gnssLocInfoFlagMask & GNSS_LOCATION_INFO_LLA_VRP_BASED_BIT) {
        pbGnssLocInfoFlagMask |= PB_GNSS_LOCATION_INFO_LLA_VRP_BASED_BIT;
    }
    if (gnssLocInfoFlagMask & GNSS_LOCATION_INFO_ENU_VELOCITY_VRP_BASED_BIT) {
        pbGnssLocInfoFlagMask |= PB_GNSS_LOCATION_INFO_ENU_VELOCITY_VRP_BASED_BIT;
    }
    if (gnssLocInfoFlagMask & GNSS_LOCATION_INFO_DR_SOLUTION_STATUS_MASK_BIT) {
        pbGnssLocInfoFlagMask |= PB_GNSS_LOCATION_INFO_DR_SOLUTION_STATUS_MASK_BIT;
    }
    LocApiPb_LOGv("LocApiPB: gnssLocInfoFlagMask:%x, pbGnssLocInfoFlagMask:%x",
            gnssLocInfoFlagMask, pbGnssLocInfoFlagMask);
    return pbGnssLocInfoFlagMask;
}

uint32_t LocationApiPbMsgConv::getPBMaskForGnssLocationInfoExtFlagMask(
        const uint64_t &gnssLocInfoFlagMask) const {

    uint32_t pbGnssLocInfoFlagMask = 0;
    if (gnssLocInfoFlagMask & GNSS_LOCATION_INFO_ALTITUDE_ASSUMED_BIT) {
        pbGnssLocInfoFlagMask |= PB_GNSS_LOCATION_INFO_ALTITUDE_ASSUMED_BIT;
    }

    if (gnssLocInfoFlagMask & GNSS_LOCATION_INFO_SESSION_STATUS_BIT) {
        pbGnssLocInfoFlagMask |= PB_GNSS_LOCATION_INFO_SESSION_STATUS_BIT;
    }

    return pbGnssLocInfoFlagMask;
}

uint32_t LocationApiPbMsgConv::getPBMaskForGnssLocationNavSolutionMask(
        const uint32_t &gnssLocNavSolnMask) const {
    uint32_t pbGnssLocNavSolnMask = 0;
    if (gnssLocNavSolnMask & LOCATION_SBAS_CORRECTION_IONO_BIT) {
        pbGnssLocNavSolnMask |= PB_LOCATION_SBAS_CORRECTION_IONO_BIT;
    }
    if (gnssLocNavSolnMask & LOCATION_SBAS_CORRECTION_FAST_BIT) {
        pbGnssLocNavSolnMask |= PB_LOCATION_SBAS_CORRECTION_FAST_BIT;
    }
    if (gnssLocNavSolnMask & LOCATION_SBAS_CORRECTION_LONG_BIT) {
        pbGnssLocNavSolnMask |= PB_LOCATION_SBAS_CORRECTION_LONG_BIT;
    }
    if (gnssLocNavSolnMask & LOCATION_SBAS_INTEGRITY_BIT) {
        pbGnssLocNavSolnMask |= PB_LOCATION_SBAS_INTEGRITY_BIT;
    }
    if (gnssLocNavSolnMask & LOCATION_NAV_CORRECTION_DGNSS_BIT) {
        pbGnssLocNavSolnMask |= PB_LOCATION_NAV_CORRECTION_DGNSS_BIT;
    }
    if (gnssLocNavSolnMask & LOCATION_NAV_CORRECTION_RTK_BIT) {
        pbGnssLocNavSolnMask |= PB_LOCATION_NAV_CORRECTION_RTK_BIT;
    }
    if (gnssLocNavSolnMask & LOCATION_NAV_CORRECTION_PPP_BIT) {
        pbGnssLocNavSolnMask |= PB_LOCATION_NAV_CORRECTION_PPP_BIT;
    }
    if (gnssLocNavSolnMask & LOCATION_NAV_CORRECTION_RTK_FIXED_BIT) {
        pbGnssLocNavSolnMask |= PB_LOCATION_NAV_CORRECTION_RTK_FIXED_BIT;
    }
    if (gnssLocNavSolnMask & LOCATION_NAV_CORRECTION_ONLY_SBAS_CORRECTED_SV_USED_BIT) {
        pbGnssLocNavSolnMask |= PB_LOCATION_NAV_CORRECTION_ONLY_SBAS_CORRECTED_SV_USED_BIT;
    }
    LocApiPb_LOGv("LocApiPB: gnssLocNavSolnMask:%x, pbGnssLocNavSolnMask:%x",
            gnssLocNavSolnMask, pbGnssLocNavSolnMask);
    return pbGnssLocNavSolnMask;
}

uint32_t LocationApiPbMsgConv::getPBMaskForDrCalibrationStatusMask(
        const uint32_t &drCalibStatusMask) const {
    uint32_t pbDrCalibStatusMask = 0;
    if (drCalibStatusMask & DR_ROLL_CALIBRATION_NEEDED) {
        pbDrCalibStatusMask |= PB_DR_ROLL_CALIBRATION_NEEDED;
    }
    if (drCalibStatusMask & DR_PITCH_CALIBRATION_NEEDED) {
        pbDrCalibStatusMask |= PB_DR_PITCH_CALIBRATION_NEEDED;
    }
    if (drCalibStatusMask & DR_YAW_CALIBRATION_NEEDED) {
        pbDrCalibStatusMask |= PB_DR_YAW_CALIBRATION_NEEDED;
    }
    if (drCalibStatusMask & DR_ODO_CALIBRATION_NEEDED) {
        pbDrCalibStatusMask |= PB_DR_ODO_CALIBRATION_NEEDED;
    }
    if (drCalibStatusMask & DR_GYRO_CALIBRATION_NEEDED) {
        pbDrCalibStatusMask |= PB_DR_GYRO_CALIBRATION_NEEDED;
    }
    LocApiPb_LOGv("LocApiPB: drCalibStatusMask:%x, pbDrCalibStatusMask:%x",
            drCalibStatusMask, pbDrCalibStatusMask);
    return pbDrCalibStatusMask;
}

uint32_t LocationApiPbMsgConv::getPBMaskForGnssMeasurementsDataFlagsMask(
        const uint32_t &gnssMeasDataFlagsMask) const {
    uint32_t pbGnssMeasDataFlagsMask = 0;
    if (gnssMeasDataFlagsMask & GNSS_MEASUREMENTS_DATA_SV_ID_BIT) {
        pbGnssMeasDataFlagsMask |= PB_GNSS_MEASUREMENTS_DATA_SV_ID_BIT;
    }
    if (gnssMeasDataFlagsMask & GNSS_MEASUREMENTS_DATA_SV_TYPE_BIT) {
        pbGnssMeasDataFlagsMask |= PB_GNSS_MEASUREMENTS_DATA_SV_TYPE_BIT;
    }
    if (gnssMeasDataFlagsMask & GNSS_MEASUREMENTS_DATA_STATE_BIT) {
        pbGnssMeasDataFlagsMask |= PB_GNSS_MEASUREMENTS_DATA_STATE_BIT;
    }
    if (gnssMeasDataFlagsMask & GNSS_MEASUREMENTS_DATA_RECEIVED_SV_TIME_BIT) {
        pbGnssMeasDataFlagsMask |= PB_GNSS_MEASUREMENTS_DATA_RECEIVED_SV_TIME_BIT;
    }
    if (gnssMeasDataFlagsMask & GNSS_MEASUREMENTS_DATA_RECEIVED_SV_TIME_UNCERTAINTY_BIT) {
        pbGnssMeasDataFlagsMask |= PB_GNSS_MEASUREMENTS_DATA_RECEIVED_SV_TIME_UNCERTAINTY_BIT;
    }
    if (gnssMeasDataFlagsMask & GNSS_MEASUREMENTS_DATA_CARRIER_TO_NOISE_BIT) {
        pbGnssMeasDataFlagsMask |= PB_GNSS_MEASUREMENTS_DATA_CARRIER_TO_NOISE_BIT;
    }
    if (gnssMeasDataFlagsMask & GNSS_MEASUREMENTS_DATA_PSEUDORANGE_RATE_BIT) {
        pbGnssMeasDataFlagsMask |= PB_GNSS_MEASUREMENTS_DATA_PSEUDORANGE_RATE_BIT;
    }
    if (gnssMeasDataFlagsMask & GNSS_MEASUREMENTS_DATA_PSEUDORANGE_RATE_UNCERTAINTY_BIT) {
        pbGnssMeasDataFlagsMask |= PB_GNSS_MEASUREMENTS_DATA_PSEUDORANGE_RATE_UNCERTAINTY_BIT;
    }
    if (gnssMeasDataFlagsMask & GNSS_MEASUREMENTS_DATA_ADR_STATE_BIT) {
        pbGnssMeasDataFlagsMask |= PB_GNSS_MEASUREMENTS_DATA_ADR_STATE_BIT;
    }
    if (gnssMeasDataFlagsMask & GNSS_MEASUREMENTS_DATA_ADR_BIT) {
        pbGnssMeasDataFlagsMask |= PB_GNSS_MEASUREMENTS_DATA_ADR_BIT;
    }
    if (gnssMeasDataFlagsMask & GNSS_MEASUREMENTS_DATA_ADR_UNCERTAINTY_BIT) {
        pbGnssMeasDataFlagsMask |= PB_GNSS_MEASUREMENTS_DATA_ADR_UNCERTAINTY_BIT;
    }
    if (gnssMeasDataFlagsMask & GNSS_MEASUREMENTS_DATA_CARRIER_FREQUENCY_BIT) {
        pbGnssMeasDataFlagsMask |= PB_GNSS_MEASUREMENTS_DATA_CARRIER_FREQUENCY_BIT;
    }
    if (gnssMeasDataFlagsMask & GNSS_MEASUREMENTS_DATA_CARRIER_CYCLES_BIT) {
        pbGnssMeasDataFlagsMask |= PB_GNSS_MEASUREMENTS_DATA_CARRIER_CYCLES_BIT;
    }
    if (gnssMeasDataFlagsMask & GNSS_MEASUREMENTS_DATA_CARRIER_PHASE_BIT) {
        pbGnssMeasDataFlagsMask |= PB_GNSS_MEASUREMENTS_DATA_CARRIER_PHASE_BIT;
    }
    if (gnssMeasDataFlagsMask & GNSS_MEASUREMENTS_DATA_CARRIER_PHASE_UNCERTAINTY_BIT) {
        pbGnssMeasDataFlagsMask |= PB_GNSS_MEASUREMENTS_DATA_CARRIER_PHASE_UNCERTAINTY_BIT;
    }
    if (gnssMeasDataFlagsMask & GNSS_MEASUREMENTS_DATA_MULTIPATH_INDICATOR_BIT) {
        pbGnssMeasDataFlagsMask |= PB_GNSS_MEASUREMENTS_DATA_MULTIPATH_INDICATOR_BIT;
    }
    if (gnssMeasDataFlagsMask & GNSS_MEASUREMENTS_DATA_SIGNAL_TO_NOISE_RATIO_BIT) {
        pbGnssMeasDataFlagsMask |= PB_GNSS_MEASUREMENTS_DATA_SIGNAL_TO_NOISE_RATIO_BIT;
    }
    if (gnssMeasDataFlagsMask & GNSS_MEASUREMENTS_DATA_AUTOMATIC_GAIN_CONTROL_BIT) {
        pbGnssMeasDataFlagsMask |= PB_GNSS_MEASUREMENTS_DATA_AUTOMATIC_GAIN_CONTROL_BIT;
    }
    if (gnssMeasDataFlagsMask & GNSS_MEASUREMENTS_DATA_FULL_ISB_BIT) {
        pbGnssMeasDataFlagsMask |= PB_GNSS_MEASUREMENTS_DATA_FULL_ISB_BIT;
    }
    if (gnssMeasDataFlagsMask & GNSS_MEASUREMENTS_DATA_FULL_ISB_UNCERTAINTY_BIT) {
        pbGnssMeasDataFlagsMask |= PB_GNSS_MEASUREMENTS_DATA_FULL_ISB_UNCERTAINTY_BIT;
    }
    if (gnssMeasDataFlagsMask & GNSS_MEASUREMENTS_DATA_CYCLE_SLIP_COUNT_BIT) {
        pbGnssMeasDataFlagsMask |= PB_GNSS_MEASUREMENTS_DATA_CYCLE_SLIP_COUNT_BIT;
    }
    LocApiPb_LOGv("LocApiPB: gnssMeasDataFlagsMask:%x, pbGnssMeasDataFlagsMask:%x",
            gnssMeasDataFlagsMask, pbGnssMeasDataFlagsMask);
    return pbGnssMeasDataFlagsMask;
}

uint32_t LocationApiPbMsgConv::getPBMaskForGnssMeasurementsStateMask(
        const uint32_t &gnssMeasStateMask) const {
    uint32_t pbGnssMeasStateMask = 0;
    if (gnssMeasStateMask & GNSS_MEASUREMENTS_STATE_UNKNOWN_BIT) {
        pbGnssMeasStateMask |= PB_GNSS_MEASUREMENTS_STATE_UNKNOWN_BIT;
    }
    if (gnssMeasStateMask & GNSS_MEASUREMENTS_STATE_CODE_LOCK_BIT) {
        pbGnssMeasStateMask |= PB_GNSS_MEASUREMENTS_STATE_CODE_LOCK_BIT;
    }
    if (gnssMeasStateMask & GNSS_MEASUREMENTS_STATE_BIT_SYNC_BIT) {
        pbGnssMeasStateMask |= PB_GNSS_MEASUREMENTS_STATE_BIT_SYNC_BIT;
    }
    if (gnssMeasStateMask & GNSS_MEASUREMENTS_STATE_SUBFRAME_SYNC_BIT) {
        pbGnssMeasStateMask |= PB_GNSS_MEASUREMENTS_STATE_SUBFRAME_SYNC_BIT;
    }
    if (gnssMeasStateMask & GNSS_MEASUREMENTS_STATE_TOW_DECODED_BIT) {
        pbGnssMeasStateMask |= PB_GNSS_MEASUREMENTS_STATE_TOW_DECODED_BIT;
    }
    if (gnssMeasStateMask & GNSS_MEASUREMENTS_STATE_MSEC_AMBIGUOUS_BIT) {
        pbGnssMeasStateMask |= PB_GNSS_MEASUREMENTS_STATE_MSEC_AMBIGUOUS_BIT;
    }
    if (gnssMeasStateMask & GNSS_MEASUREMENTS_STATE_SYMBOL_SYNC_BIT) {
        pbGnssMeasStateMask |= PB_GNSS_MEASUREMENTS_STATE_SYMBOL_SYNC_BIT;
    }
    if (gnssMeasStateMask & GNSS_MEASUREMENTS_STATE_GLO_STRING_SYNC_BIT) {
        pbGnssMeasStateMask |= PB_GNSS_MEASUREMENTS_STATE_GLO_STRING_SYNC_BIT;
    }
    if (gnssMeasStateMask & GNSS_MEASUREMENTS_STATE_GLO_TOD_DECODED_BIT) {
        pbGnssMeasStateMask |= PB_GNSS_MEASUREMENTS_STATE_GLO_TOD_DECODED_BIT;
    }
    if (gnssMeasStateMask & GNSS_MEASUREMENTS_STATE_BDS_D2_BIT_SYNC_BIT) {
        pbGnssMeasStateMask |= PB_GNSS_MEASUREMENTS_STATE_BDS_D2_BIT_SYNC_BIT;
    }
    if (gnssMeasStateMask & GNSS_MEASUREMENTS_STATE_BDS_D2_SUBFRAME_SYNC_BIT) {
        pbGnssMeasStateMask |= PB_GNSS_MEASUREMENTS_STATE_BDS_D2_SUBFRAME_SYNC_BIT;
    }
    if (gnssMeasStateMask & GNSS_MEASUREMENTS_STATE_GAL_E1BC_CODE_LOCK_BIT) {
        pbGnssMeasStateMask |= PB_GNSS_MEASUREMENTS_STATE_GAL_E1BC_CODE_LOCK_BIT;
    }
    if (gnssMeasStateMask & GNSS_MEASUREMENTS_STATE_GAL_E1C_2ND_CODE_LOCK_BIT) {
        pbGnssMeasStateMask |= PB_GNSS_MEASUREMENTS_STATE_GAL_E1C_2ND_CODE_LOCK_BIT;
    }
    if (gnssMeasStateMask & GNSS_MEASUREMENTS_STATE_GAL_E1B_PAGE_SYNC_BIT) {
        pbGnssMeasStateMask |= PB_GNSS_MEASUREMENTS_STATE_GAL_E1B_PAGE_SYNC_BIT;
    }
    if (gnssMeasStateMask & GNSS_MEASUREMENTS_STATE_SBAS_SYNC_BIT) {
        pbGnssMeasStateMask |= PB_GNSS_MEASUREMENTS_STATE_SBAS_SYNC_BIT;
    }
    LocApiPb_LOGv("LocApiPB: gnssMeasStateMask:%x, pbGnssMeasStateMask:%x",
            gnssMeasStateMask, pbGnssMeasStateMask);
    return pbGnssMeasStateMask;
}


uint32_t LocationApiPbMsgConv::getPBMaskForGnssMeasurementsAdrStateMask(
        const uint32_t &gnssMeasAdrStateMask) const {
    uint32_t pbGnssMeasAdrStateMask = 0;
    if (gnssMeasAdrStateMask & GNSS_MEASUREMENTS_ACCUMULATED_DELTA_RANGE_STATE_UNKNOWN) {
        pbGnssMeasAdrStateMask |= PB_GNSS_MEASUREMENTS_ACCUMULATED_DELTA_RANGE_STATE_UNKNOWN;
    }
    if (gnssMeasAdrStateMask & GNSS_MEASUREMENTS_ACCUMULATED_DELTA_RANGE_STATE_VALID_BIT) {
        pbGnssMeasAdrStateMask |= PB_GNSS_MEASUREMENTS_ACCUMULATED_DELTA_RANGE_STATE_VALID_BIT;
    }
    if (gnssMeasAdrStateMask & GNSS_MEASUREMENTS_ACCUMULATED_DELTA_RANGE_STATE_RESET_BIT) {
        pbGnssMeasAdrStateMask |= PB_GNSS_MEASUREMENTS_ACCUMULATED_DELTA_RANGE_STATE_RESET_BIT;
    }
    if (gnssMeasAdrStateMask & GNSS_MEASUREMENTS_ACCUMULATED_DELTA_RANGE_STATE_CYCLE_SLIP_BIT) {
       pbGnssMeasAdrStateMask |= PB_GNSS_MEASUREMENTS_ACCUMULATED_DELTA_RANGE_STATE_CYCLE_SLIP_BIT;
    }
    LocApiPb_LOGv("LocApiPB: gnssMeasAdrStateMask:%x, pbGnssMeasAdrStateMask:%x",
            gnssMeasAdrStateMask, pbGnssMeasAdrStateMask);
    return pbGnssMeasAdrStateMask;
}

uint32_t LocationApiPbMsgConv::getPBMaskForGnssLocationPosDataMask(
        const uint32_t &gnssLocPosDataMask, const uint32_t &gnssLocPosDataMaskExt) const {
    uint32_t pbGnssLocPosDataMask = 0;
    if (gnssLocPosDataMask & LOCATION_NAV_DATA_HAS_LONG_ACCEL_BIT) {
        pbGnssLocPosDataMask |= PB_LOCATION_NAV_DATA_HAS_LONG_ACCEL_BIT;
    }
    if (gnssLocPosDataMask & LOCATION_NAV_DATA_HAS_LAT_ACCEL_BIT) {
        pbGnssLocPosDataMask |= PB_LOCATION_NAV_DATA_HAS_LAT_ACCEL_BIT;
    }
    if (gnssLocPosDataMask & LOCATION_NAV_DATA_HAS_VERT_ACCEL_BIT) {
        pbGnssLocPosDataMask |= PB_LOCATION_NAV_DATA_HAS_VERT_ACCEL_BIT;
    }
    if (gnssLocPosDataMask & LOCATION_NAV_DATA_HAS_YAW_RATE_BIT) {
        pbGnssLocPosDataMask |= PB_LOCATION_NAV_DATA_HAS_YAW_RATE_BIT;
    }
    if (gnssLocPosDataMask & LOCATION_NAV_DATA_HAS_PITCH_BIT) {
        pbGnssLocPosDataMask |= PB_LOCATION_NAV_DATA_HAS_PITCH_BIT;
    }
    if (gnssLocPosDataMask & LOCATION_NAV_DATA_HAS_LONG_ACCEL_UNC_BIT) {
        pbGnssLocPosDataMask |= PB_LOCATION_NAV_DATA_HAS_LONG_ACCEL_UNC_BIT;
    }
    if (gnssLocPosDataMask & LOCATION_NAV_DATA_HAS_LAT_ACCEL_UNC_BIT) {
        pbGnssLocPosDataMask |= PB_LOCATION_NAV_DATA_HAS_LAT_ACCEL_UNC_BIT;
    }
    if (gnssLocPosDataMask & LOCATION_NAV_DATA_HAS_VERT_ACCEL_UNC_BIT) {
        pbGnssLocPosDataMask |= PB_LOCATION_NAV_DATA_HAS_VERT_ACCEL_UNC_BIT;
    }
    if (gnssLocPosDataMask & LOCATION_NAV_DATA_HAS_YAW_RATE_UNC_BIT) {
        pbGnssLocPosDataMask |= PB_LOCATION_NAV_DATA_HAS_YAW_RATE_UNC_BIT;
    }
    if (gnssLocPosDataMask & LOCATION_NAV_DATA_HAS_PITCH_UNC_BIT) {
        pbGnssLocPosDataMask |= PB_LOCATION_NAV_DATA_HAS_PITCH_UNC_BIT;
    }
    if (gnssLocPosDataMaskExt & LOCATION_NAV_DATA_HAS_PITCH_RATE_BIT) {
        pbGnssLocPosDataMask |= PB_LOCATION_NAV_DATA_HAS_PITCH_RATE_BIT;
    }
    if (gnssLocPosDataMaskExt & LOCATION_NAV_DATA_HAS_PITCH_RATE_UNC_BIT) {
        pbGnssLocPosDataMask |= PB_LOCATION_NAV_DATA_HAS_PITCH_RATE_UNC_BIT;
    }
    if (gnssLocPosDataMaskExt & LOCATION_NAV_DATA_HAS_ROLL_BIT) {
        pbGnssLocPosDataMask |= PB_LOCATION_NAV_DATA_HAS_ROLL_BIT;
    }
    if (gnssLocPosDataMaskExt & LOCATION_NAV_DATA_HAS_ROLL_UNC_BIT) {
        pbGnssLocPosDataMask |= PB_LOCATION_NAV_DATA_HAS_ROLL_UNC_BIT;
    }
    if (gnssLocPosDataMaskExt & LOCATION_NAV_DATA_HAS_ROLL_RATE_BIT) {
        pbGnssLocPosDataMask |= PB_LOCATION_NAV_DATA_HAS_ROLL_RATE_BIT;
    }
    if (gnssLocPosDataMaskExt & LOCATION_NAV_DATA_HAS_ROLL_RATE_UNC_BIT) {
        pbGnssLocPosDataMask |= PB_LOCATION_NAV_DATA_HAS_ROLL_RATE_UNC_BIT;
    }
    if (gnssLocPosDataMaskExt & LOCATION_NAV_DATA_HAS_YAW_BIT) {
        pbGnssLocPosDataMask |= PB_LOCATION_NAV_DATA_HAS_YAW_BIT;
    }
    if (gnssLocPosDataMaskExt & LOCATION_NAV_DATA_HAS_YAW_UNC_BIT) {
        pbGnssLocPosDataMask |= PB_LOCATION_NAV_DATA_HAS_YAW_UNC_BIT;
    }
    LocApiPb_LOGv("LocApiPB: Mask - gnssLocPosData:%x, gnssLocPosDataExt:%x, pbGnssLocPosData:%x",
            gnssLocPosDataMask, gnssLocPosDataMaskExt, pbGnssLocPosDataMask);
    return pbGnssLocPosDataMask;
}

uint32_t LocationApiPbMsgConv::getPBMaskForGnssMeasurementsClockFlagsMask(
        const uint32_t &gnssMeasClockFlagsMask) const {
    uint32_t pbGnssMeasClockFlagsMask = 0;
    if (gnssMeasClockFlagsMask & GNSS_MEASUREMENTS_CLOCK_FLAGS_LEAP_SECOND_BIT) {
        pbGnssMeasClockFlagsMask |= PB_GNSS_MEASUREMENTS_CLOCK_FLAGS_LEAP_SECOND_BIT;
    }
    if (gnssMeasClockFlagsMask & GNSS_MEASUREMENTS_CLOCK_FLAGS_TIME_BIT) {
        pbGnssMeasClockFlagsMask |= PB_GNSS_MEASUREMENTS_CLOCK_FLAGS_TIME_BIT;
    }
    if (gnssMeasClockFlagsMask & GNSS_MEASUREMENTS_CLOCK_FLAGS_TIME_UNCERTAINTY_BIT) {
        pbGnssMeasClockFlagsMask |= PB_GNSS_MEASUREMENTS_CLOCK_FLAGS_TIME_UNCERTAINTY_BIT;
    }
    if (gnssMeasClockFlagsMask & GNSS_MEASUREMENTS_CLOCK_FLAGS_FULL_BIAS_BIT) {
        pbGnssMeasClockFlagsMask |= PB_GNSS_MEASUREMENTS_CLOCK_FLAGS_FULL_BIAS_BIT;
    }
    if (gnssMeasClockFlagsMask & GNSS_MEASUREMENTS_CLOCK_FLAGS_BIAS_BIT) {
        pbGnssMeasClockFlagsMask |= PB_GNSS_MEASUREMENTS_CLOCK_FLAGS_BIAS_BIT;
    }
    if (gnssMeasClockFlagsMask & GNSS_MEASUREMENTS_CLOCK_FLAGS_BIAS_UNCERTAINTY_BIT) {
        pbGnssMeasClockFlagsMask |= PB_GNSS_MEASUREMENTS_CLOCK_FLAGS_BIAS_UNCERTAINTY_BIT;
    }
    if (gnssMeasClockFlagsMask & GNSS_MEASUREMENTS_CLOCK_FLAGS_DRIFT_BIT) {
        pbGnssMeasClockFlagsMask |= PB_GNSS_MEASUREMENTS_CLOCK_FLAGS_DRIFT_BIT;
    }
    if (gnssMeasClockFlagsMask & GNSS_MEASUREMENTS_CLOCK_FLAGS_DRIFT_UNCERTAINTY_BIT) {
        pbGnssMeasClockFlagsMask |= PB_GNSS_MEASUREMENTS_CLOCK_FLAGS_DRIFT_UNCERTAINTY_BIT;
    }
    if (gnssMeasClockFlagsMask & GNSS_MEASUREMENTS_CLOCK_FLAGS_HW_CLOCK_DISCONTINUITY_COUNT_BIT) {
        pbGnssMeasClockFlagsMask |=
                PB_GNSS_MEASUREMENTS_CLOCK_FLAGS_HW_CLOCK_DISCONTINUITY_COUNT_BIT;
    }
    LocApiPb_LOGv("LocApiPB: gnssMeasClockFlagsMask:%x, pbGnssMeasClockFlagsMask:%x",
            gnssMeasClockFlagsMask, pbGnssMeasClockFlagsMask);
    return pbGnssMeasClockFlagsMask;
}

uint32_t LocationApiPbMsgConv::getPBMaskForGnssGloTimeStructTypeFlags(
        const uint32_t &gnssGloTimeStructTypeFlags) const {
    uint32_t pbGnssGloTimeStructTypeFlags = 0;
    if (gnssGloTimeStructTypeFlags & GNSS_CLO_DAYS_VALID) {
        pbGnssGloTimeStructTypeFlags |= PB_GNSS_CLO_DAYS_VALID;
    }
    if (gnssGloTimeStructTypeFlags & GNSS_GLO_MSEC_VALID) {
        pbGnssGloTimeStructTypeFlags |= PB_GNSS_GLO_MSEC_VALID;
    }
    if (gnssGloTimeStructTypeFlags & GNSS_GLO_CLK_TIME_BIAS_VALID) {
        pbGnssGloTimeStructTypeFlags |= PB_GNSS_GLO_CLK_TIME_BIAS_VALID;
    }
    if (gnssGloTimeStructTypeFlags & GNSS_GLO_CLK_TIME_BIAS_UNC_VALID) {
        pbGnssGloTimeStructTypeFlags |= PB_GNSS_GLO_CLK_TIME_BIAS_UNC_VALID;
    }
    if (gnssGloTimeStructTypeFlags & GNSS_GLO_REF_FCOUNT_VALID) {
        pbGnssGloTimeStructTypeFlags |= PB_GNSS_GLO_REF_FCOUNT_VALID;
    }
    if (gnssGloTimeStructTypeFlags & GNSS_GLO_NUM_CLOCK_RESETS_VALID) {
        pbGnssGloTimeStructTypeFlags |= PB_GNSS_GLO_NUM_CLOCK_RESETS_VALID;
    }
    if (gnssGloTimeStructTypeFlags & GNSS_GLO_FOUR_YEAR_VALID) {
        pbGnssGloTimeStructTypeFlags |= PB_GNSS_GLO_FOUR_YEAR_VALID;
    }
    LocApiPb_LOGv("LocApiPB: gnssGloTimeStructTypeFlags:%x, pbGnssGloTimeStructTypeFlags:%x",
            gnssGloTimeStructTypeFlags, pbGnssGloTimeStructTypeFlags);
    return pbGnssGloTimeStructTypeFlags;
}

uint32_t LocationApiPbMsgConv::getPBMaskForGnssSvOptionsMask(const uint32_t &gnssSvOptMask) const {
    uint32_t pbGnssSvOptMask = 0;
    if (gnssSvOptMask & GNSS_SV_OPTIONS_HAS_EPHEMER_BIT) {
        pbGnssSvOptMask |= PB_GNSS_SV_OPTIONS_HAS_EPHEMER_BIT;
    }
    if (gnssSvOptMask & GNSS_SV_OPTIONS_HAS_ALMANAC_BIT) {
        pbGnssSvOptMask |= PB_GNSS_SV_OPTIONS_HAS_ALMANAC_BIT;
    }
    if (gnssSvOptMask & GNSS_SV_OPTIONS_USED_IN_FIX_BIT) {
        pbGnssSvOptMask |= PB_GNSS_SV_OPTIONS_USED_IN_FIX_BIT;
    }
    if (gnssSvOptMask & GNSS_SV_OPTIONS_HAS_CARRIER_FREQUENCY_BIT) {
        pbGnssSvOptMask |= PB_GNSS_SV_OPTIONS_HAS_CARRIER_FREQUENCY_BIT;
    }
    if (gnssSvOptMask & GNSS_SV_OPTIONS_HAS_GNSS_SIGNAL_TYPE_BIT) {
        pbGnssSvOptMask |= PB_GNSS_SV_OPTIONS_HAS_GNSS_SIGNAL_TYPE_BIT;
    }
    LocApiPb_LOGv("LocApiPB: gnssSvOptMask:%x, pbGnssSvOptMask:%x", gnssSvOptMask,
            pbGnssSvOptMask);
    return pbGnssSvOptMask;
}

uint32_t LocationApiPbMsgConv::getPBMaskForGnssSignalTypeMask(
        const uint32_t &gnssSignalTypeMask) const {
    uint32_t pbGnssSignalTypeMask = 0;
    if (gnssSignalTypeMask & GNSS_SIGNAL_GPS_L1CA) {
        pbGnssSignalTypeMask |= PB_GNSS_SIGNAL_GPS_L1CA_BIT;
    }
    if (gnssSignalTypeMask & GNSS_SIGNAL_GPS_L1C) {
        pbGnssSignalTypeMask |= PB_GNSS_SIGNAL_GPS_L1C_BIT;
    }
    if (gnssSignalTypeMask & GNSS_SIGNAL_GPS_L2) {
        pbGnssSignalTypeMask |= PB_GNSS_SIGNAL_GPS_L2_BIT;
    }
    if (gnssSignalTypeMask & GNSS_SIGNAL_GPS_L5) {
        pbGnssSignalTypeMask |= PB_GNSS_SIGNAL_GPS_L5_BIT;
    }
    if (gnssSignalTypeMask & GNSS_SIGNAL_GLONASS_G1) {
        pbGnssSignalTypeMask |= PB_GNSS_SIGNAL_GLONASS_G1_BIT;
    }
    if (gnssSignalTypeMask & GNSS_SIGNAL_GLONASS_G2) {
        pbGnssSignalTypeMask |= PB_GNSS_SIGNAL_GLONASS_G2_BIT;
    }
    if (gnssSignalTypeMask & GNSS_SIGNAL_GALILEO_E1) {
        pbGnssSignalTypeMask |= PB_GNSS_SIGNAL_GALILEO_E1_BIT;
    }
    if (gnssSignalTypeMask & GNSS_SIGNAL_GALILEO_E5A) {
        pbGnssSignalTypeMask |= PB_GNSS_SIGNAL_GALILEO_E5A_BIT;
    }
    if (gnssSignalTypeMask & GNSS_SIGNAL_GALILEO_E5B) {
        pbGnssSignalTypeMask |= PB_GNSS_SIGNAL_GALILEO_E5B_BIT;
    }
    if (gnssSignalTypeMask & GNSS_SIGNAL_BEIDOU_B1) {
        pbGnssSignalTypeMask |= PB_GNSS_SIGNAL_BEIDOU_B1_BIT;
    }
    if (gnssSignalTypeMask & GNSS_SIGNAL_BEIDOU_B2) {
        pbGnssSignalTypeMask |= PB_GNSS_SIGNAL_BEIDOU_B2_BIT;
    }
    if (gnssSignalTypeMask & GNSS_SIGNAL_QZSS_L1CA) {
        pbGnssSignalTypeMask |= PB_GNSS_SIGNAL_QZSS_L1CA_BIT;
    }
    if (gnssSignalTypeMask & GNSS_SIGNAL_QZSS_L1S) {
        pbGnssSignalTypeMask |= PB_GNSS_SIGNAL_QZSS_L1S_BIT;
    }
    if (gnssSignalTypeMask & GNSS_SIGNAL_QZSS_L2) {
        pbGnssSignalTypeMask |= PB_GNSS_SIGNAL_QZSS_L2_BIT;
    }
    if (gnssSignalTypeMask & GNSS_SIGNAL_QZSS_L5) {
        pbGnssSignalTypeMask |= PB_GNSS_SIGNAL_QZSS_L5_BIT;
    }
    if (gnssSignalTypeMask & GNSS_SIGNAL_SBAS_L1) {
        pbGnssSignalTypeMask |= PB_GNSS_SIGNAL_SBAS_L1_BIT;
    }
    if (gnssSignalTypeMask & GNSS_SIGNAL_BEIDOU_B1I) {
        pbGnssSignalTypeMask |= PB_GNSS_SIGNAL_BEIDOU_B1I_BIT;
    }
    if (gnssSignalTypeMask & GNSS_SIGNAL_BEIDOU_B1C) {
        pbGnssSignalTypeMask |= PB_GNSS_SIGNAL_BEIDOU_B1C_BIT;
    }
    if (gnssSignalTypeMask & GNSS_SIGNAL_BEIDOU_B2I) {
        pbGnssSignalTypeMask |= PB_GNSS_SIGNAL_BEIDOU_B2I_BIT;
    }
    if (gnssSignalTypeMask & GNSS_SIGNAL_BEIDOU_B2AI) {
        pbGnssSignalTypeMask |= PB_GNSS_SIGNAL_BEIDOU_B2AI_BIT;
    }
    if (gnssSignalTypeMask & GNSS_SIGNAL_NAVIC_L5) {
        pbGnssSignalTypeMask |= PB_GNSS_SIGNAL_NAVIC_L5_BIT;
    }
    if (gnssSignalTypeMask & GNSS_SIGNAL_BEIDOU_B2AQ) {
        pbGnssSignalTypeMask |= PB_GNSS_SIGNAL_BEIDOU_B2AQ_BIT;
    }
    LocApiPb_LOGv("LocApiPB: gnssSignalTypeMask:%x, pbGnssSignalTypeMask:%x",
            gnssSignalTypeMask, pbGnssSignalTypeMask);
    return pbGnssSignalTypeMask;
}

uint32_t LocationApiPbMsgConv::getPBMaskForGeofenceBreachTypeMask(
        const uint32_t &gfBreachTypeMask) const {
    uint32_t pbGfBreachTypeMask = 0;
    if (gfBreachTypeMask & GEOFENCE_BREACH_ENTER_BIT) {
        pbGfBreachTypeMask |= PB_GEOFENCE_BREACH_ENTER_BIT;
    }
    if (gfBreachTypeMask & GEOFENCE_BREACH_EXIT_BIT) {
        pbGfBreachTypeMask |= PB_GEOFENCE_BREACH_EXIT_BIT;
    }
    if (gfBreachTypeMask & GEOFENCE_BREACH_DWELL_IN_BIT) {
        pbGfBreachTypeMask |= PB_GEOFENCE_BREACH_DWELL_IN_BIT;
    }
    if (gfBreachTypeMask & GEOFENCE_BREACH_DWELL_OUT_BIT) {
        pbGfBreachTypeMask |= PB_GEOFENCE_BREACH_DWELL_OUT_BIT;
    }
    LocApiPb_LOGv("LocApiPB: gfBreachTypeMask:%x, pbGfBreachTypeMask:%x",
            gfBreachTypeMask, pbGfBreachTypeMask);
    return pbGfBreachTypeMask;
}

// DeadReckoningEngineConfigValidMask to PBDeadReckoningEngineConfigValidMask
uint64_t LocationApiPbMsgConv::getPBMaskForDeadReckoningEngineConfigValidMask(
            const uint64_t &drEngCfgValidMask) const {
    uint64_t pbDrEngCfgVldMask = 0;
    if (drEngCfgValidMask & BODY_TO_SENSOR_MOUNT_PARAMS_BIT) {
        pbDrEngCfgVldMask |= PB_BODY_TO_SENSOR_MOUNT_PARAMS_BIT;
    }
    if (drEngCfgValidMask & VEHICLE_SPEED_SCALE_FACTOR_BIT) {
        pbDrEngCfgVldMask |= PB_VEHICLE_SPEED_SCALE_FACTOR_BIT;
    }
    if (drEngCfgValidMask & VEHICLE_SPEED_SCALE_FACTOR_UNC_BIT) {
        pbDrEngCfgVldMask |= PB_VEHICLE_SPEED_SCALE_FACTOR_UNC_BIT;
    }
    if (drEngCfgValidMask & GYRO_SCALE_FACTOR_BIT) {
        pbDrEngCfgVldMask |= PB_GYRO_SCALE_FACTOR_BIT;
    }
    if (drEngCfgValidMask & GYRO_SCALE_FACTOR_UNC_BIT) {
        pbDrEngCfgVldMask |= PB_GYRO_SCALE_FACTOR_UNC_BIT;
    }
    LocApiPb_LOGv("LocApiPB: drEngCfgValidMask:%" PRIu64", pbDrEngCfgVldMask:%" PRIu64,
            drEngCfgValidMask, pbDrEngCfgVldMask);
    return pbDrEngCfgVldMask;
}


uint32_t LocationApiPbMsgConv::getPBMaskForDrEngineAidingDataMask(
        const uint32_t &drEngAidDataMask) const {
    uint32_t pbDrEngAidDataMask = 0;
    if (drEngAidDataMask & DR_ENGINE_AIDING_DATA_CALIBRATION_BIT) {
        pbDrEngAidDataMask |= PB_DR_ENGINE_AIDING_DATA_CALIBRATION_BIT;
    }
    LocApiPb_LOGv("LocApiPB: drEngAidDataMask:%x, pbDrEngAidDataMask:%x",
            drEngAidDataMask, pbDrEngAidDataMask);
    return pbDrEngAidDataMask;
}

uint32_t LocationApiPbMsgConv::getPBMaskForDrSolutionStatusMask(
        const uint32_t &drSolnStatusMask) const {
    uint32_t pbDrSolnStatusMask = 0;
    if (drSolnStatusMask & VEHICLE_SENSOR_SPEED_INPUT_DETECTED) {
        pbDrSolnStatusMask |= PB_VEHICLE_SENSOR_SPEED_INPUT_DETECTED;
    }
    if (drSolnStatusMask & VEHICLE_SENSOR_SPEED_INPUT_USED) {
        pbDrSolnStatusMask |= PB_VEHICLE_SENSOR_SPEED_INPUT_USED;
    }
    LocApiPb_LOGv("LocApiPB: drSolnStatusMask:%x, pbDrSolnStatusMask:%x",
            drSolnStatusMask, pbDrSolnStatusMask);
    return pbDrSolnStatusMask;
}

// **** helper function for mask conversion from protobuf masks to normal rigid values
uint64_t LocationApiPbMsgConv::getLocationCapabilitiesMaskFromPB(
        const uint64_t &pbLocCapabMask) const {
    uint64_t locCapabMask = 0;
    if (pbLocCapabMask & PB_LOCATION_CAPS_TIME_BASED_TRACKING_BIT) {
        locCapabMask |= LOCATION_CAPABILITIES_TIME_BASED_TRACKING_BIT;
    }
    if (pbLocCapabMask & PB_LOCATION_CAPS_TIME_BASED_BATCHING_BIT) {
        locCapabMask |= LOCATION_CAPABILITIES_TIME_BASED_BATCHING_BIT;
    }
    if (pbLocCapabMask & PB_LOCATION_CAPS_DISTANCE_BASED_TRACKING_BIT) {
        locCapabMask |= LOCATION_CAPABILITIES_DISTANCE_BASED_TRACKING_BIT;
    }
    if (pbLocCapabMask & PB_LOCATION_CAPS_DISTANCE_BASED_BATCHING_BIT) {
        locCapabMask |= LOCATION_CAPABILITIES_DISTANCE_BASED_BATCHING_BIT;
    }
    if (pbLocCapabMask & PB_LOCATION_CAPS_GEOFENCE_BIT) {
        locCapabMask |= LOCATION_CAPABILITIES_GEOFENCE_BIT;
    }
    if (pbLocCapabMask & PB_LOCATION_CAPS_OUTDOOR_TRIP_BATCHING_BIT) {
        locCapabMask |= LOCATION_CAPABILITIES_OUTDOOR_TRIP_BATCHING_BIT;
    }
    if (pbLocCapabMask & PB_LOCATION_CAPS_GNSS_MEASUREMENTS_BIT) {
        locCapabMask |= LOCATION_CAPABILITIES_GNSS_MEASUREMENTS_BIT;
    }
    if (pbLocCapabMask & PB_LOCATION_CAPS_CONSTELLATION_ENABLEMENT_BIT) {
        locCapabMask |= LOCATION_CAPABILITIES_CONSTELLATION_ENABLEMENT_BIT;
    }
    if (pbLocCapabMask & PB_LOCATION_CAPS_QWES_CARRIER_PHASE_BIT) {
        locCapabMask |= LOCATION_CAPABILITIES_QWES_CARRIER_PHASE_BIT;
    }
    if (pbLocCapabMask & PB_LOCATION_CAPS_QWES_SV_POLYNOMIAL_BIT) {
        locCapabMask |= LOCATION_CAPABILITIES_QWES_SV_POLYNOMIAL_BIT;
    }
    if (pbLocCapabMask & PB_LOCATION_CAPS_QWES_GNSS_SINGLE_FREQUENCY) {
        locCapabMask |= LOCATION_CAPABILITIES_QWES_GNSS_SINGLE_FREQUENCY;
    }
    if (pbLocCapabMask & PB_LOCATION_CAPS_QWES_GNSS_MULTI_FREQUENCY) {
        locCapabMask |= LOCATION_CAPABILITIES_QWES_GNSS_MULTI_FREQUENCY;
    }
    if (pbLocCapabMask & PB_LOCATION_CAPS_QWES_VPE) {
        locCapabMask |= LOCATION_CAPABILITIES_QWES_VPE;
    }
    if (pbLocCapabMask & PB_LOCATION_CAPS_QWES_CV2X_LOCATION_BASIC) {
        locCapabMask |= LOCATION_CAPABILITIES_QWES_CV2X_LOCATION_BASIC;
    }
    if (pbLocCapabMask & PB_LOCATION_CAPS_QWES_CV2X_LOCATION_PREMIUM) {
        locCapabMask |= LOCATION_CAPABILITIES_QWES_CV2X_LOCATION_PREMIUM;
    }
    if (pbLocCapabMask & PB_LOCATION_CAPS_QWES_PPE) {
        locCapabMask |= LOCATION_CAPABILITIES_QWES_PPE;
    }
    if (pbLocCapabMask & PB_LOCATION_CAPS_QWES_QDR2) {
        locCapabMask |= LOCATION_CAPABILITIES_QWES_QDR2;
    }
    if (pbLocCapabMask & PB_LOCATION_CAPS_QWES_QDR3) {
        locCapabMask |= LOCATION_CAPABILITIES_QWES_QDR3;
    }
    LOC_LOGi("LocApiPB: pbLocCapabMask:0x%" PRIx64", locCapabMask:0x%" PRIx64,
            pbLocCapabMask, locCapabMask);
    return locCapabMask;
}

uint32_t LocationApiPbMsgConv::getLocationCallbacksMaskFromPB(const uint32_t &pbLocCbMask) const {
    uint32_t locCbMask = 0;
    if (pbLocCbMask & PB_E_LOC_CB_DISTANCE_BASED_TRACKING_BIT) {
        locCbMask |= E_LOC_CB_DISTANCE_BASED_TRACKING_BIT;
    }
    if (pbLocCbMask & PB_E_LOC_CB_GNSS_LOCATION_INFO_BIT) {
        locCbMask |= E_LOC_CB_GNSS_LOCATION_INFO_BIT;
    }
    if (pbLocCbMask & PB_E_LOC_CB_GNSS_SV_BIT) {
        locCbMask |= E_LOC_CB_GNSS_SV_BIT;
    }
    if (pbLocCbMask & PB_E_LOC_CB_GNSS_NMEA_BIT) {
        locCbMask |= E_LOC_CB_GNSS_NMEA_BIT;
    }
    if (pbLocCbMask & PB_E_LOC_CB_GNSS_DATA_BIT) {
        locCbMask |= E_LOC_CB_GNSS_DATA_BIT;
    }
    if (pbLocCbMask & PB_E_LOC_CB_SYSTEM_INFO_BIT) {
        locCbMask |= E_LOC_CB_SYSTEM_INFO_BIT;
    }
    if (pbLocCbMask & PB_E_LOC_CB_BATCHING_BIT) {
        locCbMask |= E_LOC_CB_BATCHING_BIT;
    }
    if (pbLocCbMask & PB_E_LOC_CB_BATCHING_STATUS_BIT) {
        locCbMask |= E_LOC_CB_BATCHING_STATUS_BIT;
    }
    if (pbLocCbMask & PB_E_LOC_CB_GEOFENCE_BREACH_BIT) {
        locCbMask |= E_LOC_CB_GEOFENCE_BREACH_BIT;
    }
    if (pbLocCbMask & PB_E_LOC_CB_ENGINE_LOCATIONS_INFO_BIT) {
        locCbMask |= E_LOC_CB_ENGINE_LOCATIONS_INFO_BIT;
    }
    if (pbLocCbMask & PB_E_LOC_CB_SIMPLE_LOCATION_INFO_BIT) {
        locCbMask |= E_LOC_CB_SIMPLE_LOCATION_INFO_BIT;
    }
    if (pbLocCbMask & PB_E_LOC_CB_GNSS_MEAS_BIT) {
        locCbMask |= E_LOC_CB_GNSS_MEAS_BIT;
    }
    LocApiPb_LOGv("LocApiPB: pbLocCbMask:%x, locCbMask:%x", pbLocCbMask, locCbMask);
    return locCbMask;
}

uint32_t LocationApiPbMsgConv::getLeverArmTypeMaskFromPB(const uint32_t &pbLeverTypeMask) const {
    uint32_t leverYypeMask = 0;
    if (pbLeverTypeMask & PB_LEVER_ARM_TYPE_GNSS_TO_VRP_BIT) {
        leverYypeMask |= LEVER_ARM_TYPE_GNSS_TO_VRP_BIT;
    }
    if (pbLeverTypeMask & PB_LEVER_ARM_TYPE_DR_IMU_TO_GNSS_BIT) {
        leverYypeMask |= LEVER_ARM_TYPE_DR_IMU_TO_GNSS_BIT;
    }
    if (pbLeverTypeMask & PB_LEVER_ARM_TYPE_VEPP_IMU_TO_GNSS_BIT) {
        leverYypeMask |= LEVER_ARM_TYPE_VEPP_IMU_TO_GNSS_BIT;
    }
    LocApiPb_LOGv("LocApiPB: pbLeverTypeMask:%x, leverYypeMask:%x", pbLeverTypeMask,
            leverYypeMask);
    return leverYypeMask;
}

uint32_t LocationApiPbMsgConv::getEnumForPBPositioningEngineMask(
        const uint32_t &pbPosEngMask) const {
    uint32_t posEngMask = 0;
    if (pbPosEngMask & PB_STANDARD_POSITIONING_ENGINE) {
        posEngMask |= STANDARD_POSITIONING_ENGINE;
    }
    if (pbPosEngMask & PB_PRECISE_POSITIONING_ENGINE) {
        posEngMask |= PRECISE_POSITIONING_ENGINE;
    }
    if (pbPosEngMask & PB_DEAD_RECKONING_ENGINE) {
        posEngMask |= DEAD_RECKONING_ENGINE;
    }
    LocApiPb_LOGv("LocApiPB: pbPosEngMask:%x, posEngMask:%x", pbPosEngMask, posEngMask);
    return posEngMask;
}

uint64_t LocationApiPbMsgConv::getGnssSvTypesMaskFromPB(const uint64_t &pbGnssSvTypesMask) const {
    uint64_t gnssSvTypesMask = 0;
    if (pbGnssSvTypesMask & PB_GNSS_SV_TYPES_MASK_GLO_BIT) {
        gnssSvTypesMask |= GNSS_SV_TYPES_MASK_GLO_BIT;
    }
    if (pbGnssSvTypesMask & PB_GNSS_SV_TYPES_MASK_BDS_BIT) {
        gnssSvTypesMask |= GNSS_SV_TYPES_MASK_BDS_BIT;
    }
    if (pbGnssSvTypesMask & PB_GNSS_SV_TYPES_MASK_QZSS_BIT) {
        gnssSvTypesMask |= GNSS_SV_TYPES_MASK_QZSS_BIT;
    }
    if (pbGnssSvTypesMask & PB_GNSS_SV_TYPES_MASK_GAL_BIT) {
        gnssSvTypesMask |= GNSS_SV_TYPES_MASK_GAL_BIT;
    }
    if (pbGnssSvTypesMask & PB_GNSS_SV_TYPES_MASK_NAVIC_BIT) {
        gnssSvTypesMask |= GNSS_SV_TYPES_MASK_NAVIC_BIT;
    }
    if (pbGnssSvTypesMask & PB_GNSS_SV_TYPES_MASK_GPS_BIT) {
        gnssSvTypesMask |= GNSS_SV_TYPES_MASK_GPS_BIT;
    }
    LocApiPb_LOGv("LocApiPB: pbGnssSvTypesMask:%" PRIu64", gnssSvTypesMask:%" PRIu64,
            pbGnssSvTypesMask, gnssSvTypesMask);
    return gnssSvTypesMask;
}

uint32_t LocationApiPbMsgConv::getGnssAidingDataSvMaskFromPB(
        const uint32_t &pbGnssAidDataSvMask) const {
    uint32_t gnssAidDataSvMask = 0;
    if (pbGnssAidDataSvMask & PB_AIDING_DATA_SV_EPHEMERIS_BIT) {
        gnssAidDataSvMask |= GNSS_AIDING_DATA_SV_EPHEMERIS_BIT;
    }
    LocApiPb_LOGv("LocApiPB: pbGnssAidDataSvMask:%x, gnssAidDataSvMask:%x",
            pbGnssAidDataSvMask, gnssAidDataSvMask);
    return gnssAidDataSvMask;
}

uint32_t LocationApiPbMsgConv::getLocReqEngineTypeMaskFromPB(
        const uint32_t &pbLocReqEngTypeMask) const {
    uint32_t locReqEngTypeMask = 0;
    if (pbLocReqEngTypeMask & PB_LOC_REQ_ENGINE_FUSED_BIT) {
        locReqEngTypeMask |= LOC_REQ_ENGINE_FUSED_BIT;
    }
    if (pbLocReqEngTypeMask & PB_LOC_REQ_ENGINE_SPE_BIT) {
        locReqEngTypeMask |= LOC_REQ_ENGINE_SPE_BIT;
    }
    if (pbLocReqEngTypeMask & PB_LOC_REQ_ENGINE_PPE_BIT) {
        locReqEngTypeMask |= LOC_REQ_ENGINE_PPE_BIT;
    }
    LocApiPb_LOGv("LocApiPB: pbLocReqEngTypeMask:%x, locReqEngTypeMask:%x",
            pbLocReqEngTypeMask, locReqEngTypeMask);
    return locReqEngTypeMask;
}

uint32_t LocationApiPbMsgConv::getLocationSystemInfoMaskFromPB(
        const uint32_t &pbLocSysInfoMask) const {
    uint32_t locSysInfoMask = PB_LOCATION_SYS_INFO_INVALID;
    if (pbLocSysInfoMask & PB_LOCATION_SYS_INFO_LEAP_SECOND) {
        locSysInfoMask |= LOCATION_SYS_INFO_LEAP_SECOND;
    }
    LocApiPb_LOGv("LocApiPB: pbLocSysInfoMask:%x, locSysInfoMask:%x", pbLocSysInfoMask,
        locSysInfoMask);
    return locSysInfoMask;
}

uint64_t LocationApiPbMsgConv::getGnssDataMaskFromPB(const uint64_t &pbGnssDataMask) const {
    uint64_t gnssDataMask = 0;
    if (pbGnssDataMask & PB_GNSS_LOC_DATA_JAMMER_IND_BIT) {
        gnssDataMask |= GNSS_LOC_DATA_JAMMER_IND_BIT;
    }
    if (pbGnssDataMask & PB_GNSS_LOC_DATA_AGC_BIT) {
        gnssDataMask |= GNSS_LOC_DATA_AGC_BIT;
    }
    LocApiPb_LOGv("LocApiPB: pbGnssDataMask:%" PRIu64", gnssDataMask:%" PRIu64,
            pbGnssDataMask, gnssDataMask);
    return gnssDataMask;
}

uint32_t LocationApiPbMsgConv::getLeapSecSysInfoMaskFromPB(
        const uint32_t &pbLeapSecSysInfoMask) const {
    uint32_t leapSecSysInfoMask = 0;
    if (pbLeapSecSysInfoMask & PB_LEAP_SECOND_SYS_INFO_CURRENT_LEAP_SECONDS_BIT) {
        leapSecSysInfoMask |= LEAP_SECOND_SYS_INFO_CURRENT_LEAP_SECONDS_BIT;
    }
    if (pbLeapSecSysInfoMask & PB_LEAP_SECOND_SYS_INFO_LEAP_SECOND_CHANGE_BIT) {
        leapSecSysInfoMask |= LEAP_SECOND_SYS_INFO_LEAP_SECOND_CHANGE_BIT;
    }
    LocApiPb_LOGv("LocApiPB: pbLeapSecSysInfoMask:%x, leapSecSysInfoMask:%x",
            pbLeapSecSysInfoMask, leapSecSysInfoMask);
    return leapSecSysInfoMask;
}

uint32_t LocationApiPbMsgConv::getGnssSystemTimeStructTypeFlagsFromPB(
        const uint32_t &pbGnssSysTimeStrctType) const {
    uint32_t gnssSysTimeStrctTypeMsk = 0;
    if (pbGnssSysTimeStrctType & PB_GNSS_SYSTEM_TIME_WEEK_VALID) {
        gnssSysTimeStrctTypeMsk |= GNSS_SYSTEM_TIME_WEEK_VALID;
    }
    if (pbGnssSysTimeStrctType & PB_GNSS_SYSTEM_TIME_WEEK_MS_VALID) {
        gnssSysTimeStrctTypeMsk |= GNSS_SYSTEM_TIME_WEEK_MS_VALID;
    }
    if (pbGnssSysTimeStrctType & PB_GNSS_SYSTEM_CLK_TIME_BIAS_VALID) {
        gnssSysTimeStrctTypeMsk |= GNSS_SYSTEM_CLK_TIME_BIAS_VALID;
    }
    if (pbGnssSysTimeStrctType & PB_GNSS_SYSTEM_CLK_TIME_BIAS_UNC_VALID) {
        gnssSysTimeStrctTypeMsk |= GNSS_SYSTEM_CLK_TIME_BIAS_UNC_VALID;
    }
    if (pbGnssSysTimeStrctType & PB_GNSS_SYSTEM_REF_FCOUNT_VALID) {
        gnssSysTimeStrctTypeMsk |= GNSS_SYSTEM_REF_FCOUNT_VALID;
    }
    if (pbGnssSysTimeStrctType & PB_GNSS_SYSTEM_NUM_CLOCK_RESETS_VALID) {
        gnssSysTimeStrctTypeMsk |= GNSS_SYSTEM_NUM_CLOCK_RESETS_VALID;
    }
    LocApiPb_LOGv("LocApiPB: pbGnssSysTimeStrctType:%x, gnssSysTimeStrctTypeMsk:%x",
            pbGnssSysTimeStrctType, gnssSysTimeStrctTypeMsk);
    return gnssSysTimeStrctTypeMsk;
}

uint32_t LocationApiPbMsgConv::getGnssSignalTypeMaskFromPB(
        const uint32_t &pbGnssSignalTypeMask) const {
    uint32_t gnssSignalTypeMask = 0;
    if (pbGnssSignalTypeMask & PB_GNSS_SIGNAL_GPS_L1CA_BIT) {
        gnssSignalTypeMask |= GNSS_SIGNAL_GPS_L1CA;
    }
    if (pbGnssSignalTypeMask & PB_GNSS_SIGNAL_GPS_L1C_BIT) {
        gnssSignalTypeMask |= GNSS_SIGNAL_GPS_L1C;
    }
    if (pbGnssSignalTypeMask & PB_GNSS_SIGNAL_GPS_L2_BIT) {
        gnssSignalTypeMask |= GNSS_SIGNAL_GPS_L2;
    }
    if (pbGnssSignalTypeMask & PB_GNSS_SIGNAL_GPS_L5_BIT) {
        gnssSignalTypeMask |= GNSS_SIGNAL_GPS_L5;
    }
    if (pbGnssSignalTypeMask & PB_GNSS_SIGNAL_GLONASS_G1_BIT) {
        gnssSignalTypeMask |= GNSS_SIGNAL_GLONASS_G1;
    }
    if (pbGnssSignalTypeMask & PB_GNSS_SIGNAL_GLONASS_G2_BIT) {
        gnssSignalTypeMask |= GNSS_SIGNAL_GLONASS_G2;
    }
    if (pbGnssSignalTypeMask & PB_GNSS_SIGNAL_GALILEO_E1_BIT) {
        gnssSignalTypeMask |= GNSS_SIGNAL_GALILEO_E1;
    }
    if (pbGnssSignalTypeMask & PB_GNSS_SIGNAL_GALILEO_E5A_BIT) {
        gnssSignalTypeMask |= GNSS_SIGNAL_GALILEO_E5A;
    }
    if (pbGnssSignalTypeMask & PB_GNSS_SIGNAL_GALILEO_E5B_BIT) {
        gnssSignalTypeMask |= GNSS_SIGNAL_GALILEO_E5B;
    }
    if (pbGnssSignalTypeMask & PB_GNSS_SIGNAL_BEIDOU_B1_BIT) {
        gnssSignalTypeMask |= GNSS_SIGNAL_BEIDOU_B1;
    }
    if (pbGnssSignalTypeMask & PB_GNSS_SIGNAL_BEIDOU_B2_BIT) {
        gnssSignalTypeMask |= GNSS_SIGNAL_BEIDOU_B2;
    }
    if (pbGnssSignalTypeMask & PB_GNSS_SIGNAL_QZSS_L1CA_BIT) {
        gnssSignalTypeMask |= GNSS_SIGNAL_QZSS_L1CA;
    }
    if (pbGnssSignalTypeMask & PB_GNSS_SIGNAL_QZSS_L1S_BIT) {
        gnssSignalTypeMask |= GNSS_SIGNAL_QZSS_L1S;
    }
    if (pbGnssSignalTypeMask & PB_GNSS_SIGNAL_QZSS_L2_BIT) {
        gnssSignalTypeMask |= GNSS_SIGNAL_QZSS_L2;
    }
    if (pbGnssSignalTypeMask & PB_GNSS_SIGNAL_QZSS_L5_BIT) {
        gnssSignalTypeMask |= GNSS_SIGNAL_QZSS_L5;
    }
    if (pbGnssSignalTypeMask & PB_GNSS_SIGNAL_SBAS_L1_BIT) {
        gnssSignalTypeMask |= GNSS_SIGNAL_SBAS_L1;
    }
    if (pbGnssSignalTypeMask & PB_GNSS_SIGNAL_BEIDOU_B1I_BIT) {
        gnssSignalTypeMask |= GNSS_SIGNAL_BEIDOU_B1I;
    }
    if (pbGnssSignalTypeMask & PB_GNSS_SIGNAL_BEIDOU_B1C_BIT) {
        gnssSignalTypeMask |= GNSS_SIGNAL_BEIDOU_B1C;
    }
    if (pbGnssSignalTypeMask & PB_GNSS_SIGNAL_BEIDOU_B2I_BIT) {
        gnssSignalTypeMask |= GNSS_SIGNAL_BEIDOU_B2I;
    }
    if (pbGnssSignalTypeMask & PB_GNSS_SIGNAL_BEIDOU_B2AI_BIT) {
        gnssSignalTypeMask |= GNSS_SIGNAL_BEIDOU_B2AI;
    }
    if (pbGnssSignalTypeMask & PB_GNSS_SIGNAL_NAVIC_L5_BIT) {
        gnssSignalTypeMask |= GNSS_SIGNAL_NAVIC_L5;
    }
    if (pbGnssSignalTypeMask & PB_GNSS_SIGNAL_BEIDOU_B2AQ_BIT) {
        gnssSignalTypeMask |= GNSS_SIGNAL_BEIDOU_B2AQ;
    }
    LocApiPb_LOGv("LocApiPB: pbGnssSignalTypeMask:%x, gnssSignalTypeMask:%x",
            pbGnssSignalTypeMask, gnssSignalTypeMask);
    return gnssSignalTypeMask;
}

uint32_t LocationApiPbMsgConv::getGnssSvOptionsMaskFromPB(
        const uint32_t &pbGnssSvOptMask) const {
    uint32_t gnssSvOptMask = 0;
    if (pbGnssSvOptMask & PB_GNSS_SV_OPTIONS_HAS_EPHEMER_BIT) {
        gnssSvOptMask |= GNSS_SV_OPTIONS_HAS_EPHEMER_BIT;
    }
    if (pbGnssSvOptMask & PB_GNSS_SV_OPTIONS_HAS_ALMANAC_BIT) {
        gnssSvOptMask |= GNSS_SV_OPTIONS_HAS_ALMANAC_BIT;
    }
    if (pbGnssSvOptMask & PB_GNSS_SV_OPTIONS_USED_IN_FIX_BIT) {
        gnssSvOptMask |= GNSS_SV_OPTIONS_USED_IN_FIX_BIT;
    }
    if (pbGnssSvOptMask & PB_GNSS_SV_OPTIONS_HAS_CARRIER_FREQUENCY_BIT) {
        gnssSvOptMask |= GNSS_SV_OPTIONS_HAS_CARRIER_FREQUENCY_BIT;
    }
    if (pbGnssSvOptMask & PB_GNSS_SV_OPTIONS_HAS_GNSS_SIGNAL_TYPE_BIT) {
        gnssSvOptMask |= GNSS_SV_OPTIONS_HAS_GNSS_SIGNAL_TYPE_BIT;
    }
    LocApiPb_LOGv("LocApiPB: pbGnssSvOptMask:%x, gnssSvOptMask:%x", pbGnssSvOptMask,
            gnssSvOptMask);
    return gnssSvOptMask;
}

uint32_t LocationApiPbMsgConv::getGfBreachTypeMaskFromPB(const uint32_t &pbGfBreackTypMask) const {
    uint32_t gfBreachTypMask = 0;
    if (pbGfBreackTypMask & PB_GEOFENCE_BREACH_ENTER_BIT) {
        gfBreachTypMask |= GEOFENCE_BREACH_ENTER_BIT;
    }
    if (pbGfBreackTypMask & PB_GEOFENCE_BREACH_EXIT_BIT) {
        gfBreachTypMask |= GEOFENCE_BREACH_EXIT_BIT;
    }
    if (pbGfBreackTypMask & PB_GEOFENCE_BREACH_DWELL_IN_BIT) {
        gfBreachTypMask |= GEOFENCE_BREACH_DWELL_IN_BIT;
    }
    if (pbGfBreackTypMask & PB_GEOFENCE_BREACH_DWELL_OUT_BIT) {
        gfBreachTypMask |= GEOFENCE_BREACH_DWELL_OUT_BIT;
    }
    LocApiPb_LOGv("LocApiPB: pbGfBreackTypMask:%x, gfBreachTypMask:%x", pbGfBreackTypMask,
            gfBreachTypMask);
    return gfBreachTypMask;
}

uint32_t LocationApiPbMsgConv::getLocationFlagsMaskFromPB(const uint32_t &pbLocFlagsMask) const {
    uint32_t locFlagsMask = 0;
    if (pbLocFlagsMask & PB_LOCATION_HAS_LAT_LONG_BIT) {
        locFlagsMask |= LOCATION_HAS_LAT_LONG_BIT;
    }
    if (pbLocFlagsMask & PB_LOCATION_HAS_ALTITUDE_BIT) {
        locFlagsMask |= LOCATION_HAS_ALTITUDE_BIT;
    }
    if (pbLocFlagsMask & PB_LOCATION_HAS_SPEED_BIT) {
        locFlagsMask |= LOCATION_HAS_SPEED_BIT;
    }
    if (pbLocFlagsMask & PB_LOCATION_HAS_BEARING_BIT) {
        locFlagsMask |= LOCATION_HAS_BEARING_BIT;
    }
    if (pbLocFlagsMask & PB_LOCATION_HAS_ACCURACY_BIT) {
        locFlagsMask |= LOCATION_HAS_ACCURACY_BIT;
    }
    if (pbLocFlagsMask & PB_LOCATION_HAS_VERTICAL_ACCURACY_BIT) {
        locFlagsMask |= LOCATION_HAS_VERTICAL_ACCURACY_BIT;
    }
    if (pbLocFlagsMask & PB_LOCATION_HAS_SPEED_ACCURACY_BIT) {
        locFlagsMask |= LOCATION_HAS_SPEED_ACCURACY_BIT;
    }
    if (pbLocFlagsMask & PB_LOCATION_HAS_BEARING_ACCURACY_BIT) {
        locFlagsMask |= LOCATION_HAS_BEARING_ACCURACY_BIT;
    }
    LocApiPb_LOGv("LocApiPB: pbLocFlagsMask:%x, locFlagsMask:%x", pbLocFlagsMask, locFlagsMask);
    return locFlagsMask;
}

uint32_t LocationApiPbMsgConv::getLocationTechnologyMaskFromPB(
        const uint32_t &pbLocTechMask) const {
    uint32_t locTechMask = 0;
    if (pbLocTechMask & PB_LOCATION_TECHNOLOGY_GNSS_BIT) {
        locTechMask |= LOCATION_TECHNOLOGY_GNSS_BIT;
    }
    if (pbLocTechMask & PB_LOCATION_TECHNOLOGY_CELL_BIT) {
        locTechMask |= LOCATION_TECHNOLOGY_CELL_BIT;
    }
    if (pbLocTechMask & PB_LOCATION_TECHNOLOGY_WIFI_BIT) {
        locTechMask |= LOCATION_TECHNOLOGY_WIFI_BIT;
    }
    if (pbLocTechMask & PB_LOCATION_TECHNOLOGY_SENSORS_BIT) {
        locTechMask |= LOCATION_TECHNOLOGY_SENSORS_BIT;
    }
    if (pbLocTechMask & PB_LOCATION_TECHNOLOGY_REFERENCE_LOCATION_BIT) {
        locTechMask |= LOCATION_TECHNOLOGY_REFERENCE_LOCATION_BIT;
    }
    if (pbLocTechMask & PB_LOCATION_TECHNOLOGY_INJECTED_COARSE_POSITION_BIT) {
        locTechMask |= LOCATION_TECHNOLOGY_INJECTED_COARSE_POSITION_BIT;
    }
    if (pbLocTechMask & PB_LOCATION_TECHNOLOGY_AFLT_BIT) {
        locTechMask |= LOCATION_TECHNOLOGY_AFLT_BIT;
    }
    if (pbLocTechMask & PB_LOCATION_TECHNOLOGY_HYBRID_BIT) {
        locTechMask |= LOCATION_TECHNOLOGY_HYBRID_BIT;
    }
    if (pbLocTechMask & PB_LOCATION_TECHNOLOGY_PPE_BIT) {
        locTechMask |= LOCATION_TECHNOLOGY_PPE_BIT;
    }
    if (pbLocTechMask & PB_LOCATION_TECHNOLOGY_VEH_BIT) {
        locTechMask |= LOCATION_TECHNOLOGY_VEH_BIT;
    }
    if (pbLocTechMask & PB_LOCATION_TECHNOLOGY_VIS_BIT) {
        locTechMask |= LOCATION_TECHNOLOGY_VIS_BIT;
    }
    LocApiPb_LOGv("LocApiPB: pbLocTechMask:%x, locTechMask:%x", pbLocTechMask, locTechMask);
    return locTechMask;
}

uint32_t LocationApiPbMsgConv::getGnssMeasurementsClockFlagsMaskFromPB(
        const uint32_t &pbGnssMeasClockFlgMask) const {
    uint32_t gnssMeasClockFlgMask = 0;
    if (pbGnssMeasClockFlgMask & PB_GNSS_MEASUREMENTS_CLOCK_FLAGS_LEAP_SECOND_BIT) {
        gnssMeasClockFlgMask |= GNSS_MEASUREMENTS_CLOCK_FLAGS_LEAP_SECOND_BIT;
    }
    if (pbGnssMeasClockFlgMask & PB_GNSS_MEASUREMENTS_CLOCK_FLAGS_TIME_BIT) {
        gnssMeasClockFlgMask |= GNSS_MEASUREMENTS_CLOCK_FLAGS_TIME_BIT;
    }
    if (pbGnssMeasClockFlgMask & PB_GNSS_MEASUREMENTS_CLOCK_FLAGS_TIME_UNCERTAINTY_BIT) {
        gnssMeasClockFlgMask |= GNSS_MEASUREMENTS_CLOCK_FLAGS_TIME_UNCERTAINTY_BIT;
    }
    if (pbGnssMeasClockFlgMask & PB_GNSS_MEASUREMENTS_CLOCK_FLAGS_FULL_BIAS_BIT) {
        gnssMeasClockFlgMask |= GNSS_MEASUREMENTS_CLOCK_FLAGS_FULL_BIAS_BIT;
    }
    if (pbGnssMeasClockFlgMask & PB_GNSS_MEASUREMENTS_CLOCK_FLAGS_BIAS_BIT) {
        gnssMeasClockFlgMask |= GNSS_MEASUREMENTS_CLOCK_FLAGS_BIAS_BIT;
    }
    if (pbGnssMeasClockFlgMask & PB_GNSS_MEASUREMENTS_CLOCK_FLAGS_BIAS_UNCERTAINTY_BIT) {
        gnssMeasClockFlgMask |= GNSS_MEASUREMENTS_CLOCK_FLAGS_BIAS_UNCERTAINTY_BIT;
    }
    if (pbGnssMeasClockFlgMask & PB_GNSS_MEASUREMENTS_CLOCK_FLAGS_DRIFT_BIT) {
        gnssMeasClockFlgMask |= GNSS_MEASUREMENTS_CLOCK_FLAGS_DRIFT_BIT;
    }
    if (pbGnssMeasClockFlgMask & PB_GNSS_MEASUREMENTS_CLOCK_FLAGS_DRIFT_UNCERTAINTY_BIT) {
        gnssMeasClockFlgMask |= GNSS_MEASUREMENTS_CLOCK_FLAGS_DRIFT_UNCERTAINTY_BIT;
    }
    if (pbGnssMeasClockFlgMask &
            PB_GNSS_MEASUREMENTS_CLOCK_FLAGS_HW_CLOCK_DISCONTINUITY_COUNT_BIT) {
        gnssMeasClockFlgMask |= GNSS_MEASUREMENTS_CLOCK_FLAGS_HW_CLOCK_DISCONTINUITY_COUNT_BIT;
    }
    LocApiPb_LOGv("LocApiPB: pbGnssMeasClockFlgMask:%x, gnssMeasClockFlgMask:%x",
            pbGnssMeasClockFlgMask, gnssMeasClockFlgMask);
    return gnssMeasClockFlgMask;
}

uint32_t LocationApiPbMsgConv::getGnssMeasurementsDataFlagsMaskFromPB(
        const uint32_t &pbGnssMeasDataFlgMask) const {
    uint32_t gnssMeasDataFlgMask = 0;
    if (pbGnssMeasDataFlgMask & PB_GNSS_MEASUREMENTS_DATA_SV_ID_BIT) {
        gnssMeasDataFlgMask |= GNSS_MEASUREMENTS_DATA_SV_ID_BIT;
    }
    if (pbGnssMeasDataFlgMask & PB_GNSS_MEASUREMENTS_DATA_SV_TYPE_BIT) {
        gnssMeasDataFlgMask |= GNSS_MEASUREMENTS_DATA_SV_TYPE_BIT;
    }
    if (pbGnssMeasDataFlgMask & PB_GNSS_MEASUREMENTS_DATA_STATE_BIT) {
        gnssMeasDataFlgMask |= GNSS_MEASUREMENTS_DATA_STATE_BIT;
    }
    if (pbGnssMeasDataFlgMask & PB_GNSS_MEASUREMENTS_DATA_RECEIVED_SV_TIME_BIT) {
        gnssMeasDataFlgMask |= GNSS_MEASUREMENTS_DATA_RECEIVED_SV_TIME_BIT;
    }
    if (pbGnssMeasDataFlgMask & PB_GNSS_MEASUREMENTS_DATA_RECEIVED_SV_TIME_UNCERTAINTY_BIT) {
        gnssMeasDataFlgMask |= GNSS_MEASUREMENTS_DATA_RECEIVED_SV_TIME_UNCERTAINTY_BIT;
    }
    if (pbGnssMeasDataFlgMask & PB_GNSS_MEASUREMENTS_DATA_CARRIER_TO_NOISE_BIT) {
        gnssMeasDataFlgMask |= GNSS_MEASUREMENTS_DATA_CARRIER_TO_NOISE_BIT;
    }
    if (pbGnssMeasDataFlgMask & PB_GNSS_MEASUREMENTS_DATA_PSEUDORANGE_RATE_BIT) {
        gnssMeasDataFlgMask |= GNSS_MEASUREMENTS_DATA_PSEUDORANGE_RATE_BIT;
    }
    if (pbGnssMeasDataFlgMask & PB_GNSS_MEASUREMENTS_DATA_PSEUDORANGE_RATE_UNCERTAINTY_BIT) {
        gnssMeasDataFlgMask |= GNSS_MEASUREMENTS_DATA_PSEUDORANGE_RATE_UNCERTAINTY_BIT;
    }
    if (pbGnssMeasDataFlgMask & PB_GNSS_MEASUREMENTS_DATA_ADR_STATE_BIT) {
        gnssMeasDataFlgMask |= GNSS_MEASUREMENTS_DATA_ADR_STATE_BIT;
    }
    if (pbGnssMeasDataFlgMask & PB_GNSS_MEASUREMENTS_DATA_ADR_BIT) {
        gnssMeasDataFlgMask |= GNSS_MEASUREMENTS_DATA_ADR_BIT;
    }
    if (pbGnssMeasDataFlgMask & PB_GNSS_MEASUREMENTS_DATA_ADR_UNCERTAINTY_BIT) {
        gnssMeasDataFlgMask |= GNSS_MEASUREMENTS_DATA_ADR_UNCERTAINTY_BIT;
    }
    if (pbGnssMeasDataFlgMask & PB_GNSS_MEASUREMENTS_DATA_CARRIER_FREQUENCY_BIT) {
        gnssMeasDataFlgMask |= GNSS_MEASUREMENTS_DATA_CARRIER_FREQUENCY_BIT;
    }
    if (pbGnssMeasDataFlgMask & PB_GNSS_MEASUREMENTS_DATA_CARRIER_CYCLES_BIT) {
        gnssMeasDataFlgMask |= GNSS_MEASUREMENTS_DATA_CARRIER_CYCLES_BIT;
    }
    if (pbGnssMeasDataFlgMask & PB_GNSS_MEASUREMENTS_DATA_CARRIER_PHASE_BIT) {
        gnssMeasDataFlgMask |= GNSS_MEASUREMENTS_DATA_CARRIER_PHASE_BIT;
    }
    if (pbGnssMeasDataFlgMask & PB_GNSS_MEASUREMENTS_DATA_CARRIER_PHASE_UNCERTAINTY_BIT) {
        gnssMeasDataFlgMask |= GNSS_MEASUREMENTS_DATA_CARRIER_PHASE_UNCERTAINTY_BIT;
    }
    if (pbGnssMeasDataFlgMask & PB_GNSS_MEASUREMENTS_DATA_MULTIPATH_INDICATOR_BIT) {
        gnssMeasDataFlgMask |= GNSS_MEASUREMENTS_DATA_MULTIPATH_INDICATOR_BIT;
    }
    if (pbGnssMeasDataFlgMask & PB_GNSS_MEASUREMENTS_DATA_SIGNAL_TO_NOISE_RATIO_BIT) {
        gnssMeasDataFlgMask |= GNSS_MEASUREMENTS_DATA_SIGNAL_TO_NOISE_RATIO_BIT;
    }
    if (pbGnssMeasDataFlgMask & PB_GNSS_MEASUREMENTS_DATA_AUTOMATIC_GAIN_CONTROL_BIT) {
        gnssMeasDataFlgMask |= GNSS_MEASUREMENTS_DATA_AUTOMATIC_GAIN_CONTROL_BIT;
    }
    if (pbGnssMeasDataFlgMask & PB_GNSS_MEASUREMENTS_DATA_FULL_ISB_BIT) {
        gnssMeasDataFlgMask |= GNSS_MEASUREMENTS_DATA_FULL_ISB_BIT;
    }
    if (pbGnssMeasDataFlgMask & PB_GNSS_MEASUREMENTS_DATA_FULL_ISB_UNCERTAINTY_BIT) {
        gnssMeasDataFlgMask |= GNSS_MEASUREMENTS_DATA_FULL_ISB_UNCERTAINTY_BIT;
    }
    if (pbGnssMeasDataFlgMask & PB_GNSS_MEASUREMENTS_DATA_CYCLE_SLIP_COUNT_BIT) {
        gnssMeasDataFlgMask |= GNSS_MEASUREMENTS_DATA_CYCLE_SLIP_COUNT_BIT;
    }
    LocApiPb_LOGv("LocApiPB: pbGnssMeasDataFlgMask:%x, gnssMeasDataFlgMask:%x",
            pbGnssMeasDataFlgMask, gnssMeasDataFlgMask);
    return gnssMeasDataFlgMask;
}

uint32_t LocationApiPbMsgConv::getGnssMeasurementsAdrStateMaskFromPB(
        const uint32_t &pbGnssMeasAdrStateMask) const {
    uint32_t gnssMeasAdrStateMask = 0;
    if (pbGnssMeasAdrStateMask & PB_GNSS_MEASUREMENTS_ACCUMULATED_DELTA_RANGE_STATE_VALID_BIT) {
        gnssMeasAdrStateMask |= GNSS_MEASUREMENTS_ACCUMULATED_DELTA_RANGE_STATE_VALID_BIT;
    }
    if (pbGnssMeasAdrStateMask & PB_GNSS_MEASUREMENTS_ACCUMULATED_DELTA_RANGE_STATE_RESET_BIT) {
        gnssMeasAdrStateMask |= GNSS_MEASUREMENTS_ACCUMULATED_DELTA_RANGE_STATE_RESET_BIT;
    }
    if (pbGnssMeasAdrStateMask &
            PB_GNSS_MEASUREMENTS_ACCUMULATED_DELTA_RANGE_STATE_CYCLE_SLIP_BIT) {
        gnssMeasAdrStateMask |= GNSS_MEASUREMENTS_ACCUMULATED_DELTA_RANGE_STATE_CYCLE_SLIP_BIT;
    }
    LocApiPb_LOGv("LocApiPB: pbGnssMeasAdrStateMask:%x, gnssMeasAdrStateMask:%x",
            pbGnssMeasAdrStateMask, gnssMeasAdrStateMask);
    return gnssMeasAdrStateMask;
}

uint64_t LocationApiPbMsgConv::getGnssLocationInfoFlagMaskFromPB(
        const uint32_t &pbGnssLocInfoFlagMask,
        const uint32_t &pbGnssLocInfoExtFlagMask) const {

    uint64_t gnssLocInfoFlagMask = 0;
    if (pbGnssLocInfoFlagMask & PB_GNSS_LOCATION_INFO_ALTITUDE_MEAN_SEA_LEVEL_BIT) {
        gnssLocInfoFlagMask |= GNSS_LOCATION_INFO_ALTITUDE_MEAN_SEA_LEVEL_BIT;
    }
    if (pbGnssLocInfoFlagMask & PB_GNSS_LOCATION_INFO_DOP_BIT) {
        gnssLocInfoFlagMask |= PB_GNSS_LOCATION_INFO_DOP_BIT;
    }
    if (pbGnssLocInfoFlagMask & PB_GNSS_LOCATION_INFO_MAGNETIC_DEVIATION_BIT) {
        gnssLocInfoFlagMask |= GNSS_LOCATION_INFO_MAGNETIC_DEVIATION_BIT;
    }
    if (pbGnssLocInfoFlagMask & PB_GNSS_LOCATION_INFO_HOR_RELIABILITY_BIT) {
        gnssLocInfoFlagMask |= GNSS_LOCATION_INFO_HOR_RELIABILITY_BIT;
    }
    if (pbGnssLocInfoFlagMask & PB_GNSS_LOCATION_INFO_VER_RELIABILITY_BIT) {
        gnssLocInfoFlagMask |= GNSS_LOCATION_INFO_VER_RELIABILITY_BIT;
    }
    if (pbGnssLocInfoFlagMask & PB_GNSS_LOCATION_INFO_HOR_ACCURACY_ELIP_SEMI_MAJOR_BIT) {
        gnssLocInfoFlagMask |= GNSS_LOCATION_INFO_HOR_ACCURACY_ELIP_SEMI_MAJOR_BIT;
    }
    if (pbGnssLocInfoFlagMask & PB_GNSS_LOCATION_INFO_HOR_ACCURACY_ELIP_SEMI_MINOR_BIT) {
        gnssLocInfoFlagMask |= GNSS_LOCATION_INFO_HOR_ACCURACY_ELIP_SEMI_MINOR_BIT;
    }
    if (pbGnssLocInfoFlagMask & PB_GNSS_LOCATION_INFO_HOR_ACCURACY_ELIP_AZIMUTH_BIT) {
        gnssLocInfoFlagMask |= GNSS_LOCATION_INFO_HOR_ACCURACY_ELIP_AZIMUTH_BIT;
    }
    if (pbGnssLocInfoFlagMask & PB_GNSS_LOCATION_INFO_GNSS_SV_USED_DATA_BIT) {
        gnssLocInfoFlagMask |= GNSS_LOCATION_INFO_GNSS_SV_USED_DATA_BIT;
    }
    if (pbGnssLocInfoFlagMask & PB_GNSS_LOCATION_INFO_NAV_SOLUTION_MASK_BIT) {
        gnssLocInfoFlagMask |= GNSS_LOCATION_INFO_NAV_SOLUTION_MASK_BIT;
    }
    if (pbGnssLocInfoFlagMask & PB_GNSS_LOCATION_INFO_POS_DYNAMICS_DATA_BIT) {
        gnssLocInfoFlagMask |= GNSS_LOCATION_INFO_POS_DYNAMICS_DATA_BIT;
    }
    if (pbGnssLocInfoFlagMask & PB_GNSS_LOCATION_INFO_EXT_DOP_BIT) {
        gnssLocInfoFlagMask |= GNSS_LOCATION_INFO_EXT_DOP_BIT;
    }
    if (pbGnssLocInfoFlagMask & PB_GNSS_LOCATION_INFO_NORTH_STD_DEV_BIT) {
        gnssLocInfoFlagMask |= GNSS_LOCATION_INFO_NORTH_STD_DEV_BIT;
    }
    if (pbGnssLocInfoFlagMask & PB_GNSS_LOCATION_INFO_EAST_STD_DEV_BIT) {
        gnssLocInfoFlagMask |= GNSS_LOCATION_INFO_EAST_STD_DEV_BIT;
    }
    if (pbGnssLocInfoFlagMask & PB_GNSS_LOCATION_INFO_NORTH_VEL_BIT) {
        gnssLocInfoFlagMask |= GNSS_LOCATION_INFO_NORTH_VEL_BIT;
    }
    if (pbGnssLocInfoFlagMask & PB_GNSS_LOCATION_INFO_EAST_VEL_BIT) {
        gnssLocInfoFlagMask |= GNSS_LOCATION_INFO_EAST_VEL_BIT;
    }
    if (pbGnssLocInfoFlagMask & PB_GNSS_LOCATION_INFO_UP_VEL_BIT) {
        gnssLocInfoFlagMask |= GNSS_LOCATION_INFO_UP_VEL_BIT;
    }
    if (pbGnssLocInfoFlagMask & PB_GNSS_LOCATION_INFO_NORTH_VEL_UNC_BIT) {
        gnssLocInfoFlagMask |= GNSS_LOCATION_INFO_NORTH_VEL_UNC_BIT;
    }
    if (pbGnssLocInfoFlagMask & PB_GNSS_LOCATION_INFO_EAST_VEL_UNC_BIT) {
        gnssLocInfoFlagMask |= GNSS_LOCATION_INFO_EAST_VEL_UNC_BIT;
    }
    if (pbGnssLocInfoFlagMask & PB_GNSS_LOCATION_INFO_UP_VEL_UNC_BIT) {
        gnssLocInfoFlagMask |= GNSS_LOCATION_INFO_UP_VEL_UNC_BIT;
    }
    if (pbGnssLocInfoFlagMask & PB_GNSS_LOCATION_INFO_LEAP_SECONDS_BIT) {
        gnssLocInfoFlagMask |= GNSS_LOCATION_INFO_LEAP_SECONDS_BIT;
    }
    if (pbGnssLocInfoFlagMask & PB_GNSS_LOCATION_INFO_TIME_UNC_BIT) {
        gnssLocInfoFlagMask |= GNSS_LOCATION_INFO_TIME_UNC_BIT;
    }
    if (pbGnssLocInfoFlagMask & PB_GNSS_LOCATION_INFO_NUM_SV_USED_IN_POSITION_BIT) {
        gnssLocInfoFlagMask |= GNSS_LOCATION_INFO_NUM_SV_USED_IN_POSITION_BIT;
    }
    if (pbGnssLocInfoFlagMask & PB_GNSS_LOCATION_INFO_CALIBRATION_CONFIDENCE_PERCENT_BIT) {
        gnssLocInfoFlagMask |= GNSS_LOCATION_INFO_CALIBRATION_CONFIDENCE_BIT;
    }
    if (pbGnssLocInfoFlagMask & PB_GNSS_LOCATION_INFO_CALIBRATION_STATUS_BIT) {
        gnssLocInfoFlagMask |= GNSS_LOCATION_INFO_CALIBRATION_STATUS_BIT;
    }
    if (pbGnssLocInfoFlagMask & PB_GNSS_LOCATION_INFO_OUTPUT_ENG_TYPE_BIT) {
        gnssLocInfoFlagMask |= GNSS_LOCATION_INFO_OUTPUT_ENG_TYPE_BIT;
    }
    if (pbGnssLocInfoFlagMask & PB_GNSS_LOCATION_INFO_OUTPUT_ENG_MASK_BIT) {
        gnssLocInfoFlagMask |= GNSS_LOCATION_INFO_OUTPUT_ENG_MASK_BIT;
    }
    if (pbGnssLocInfoFlagMask & PB_GNSS_LOCATION_INFO_CONFORMITY_INDEX_BIT) {
        gnssLocInfoFlagMask |= GNSS_LOCATION_INFO_CONFORMITY_INDEX_BIT;
    }
    if (pbGnssLocInfoFlagMask & PB_GNSS_LOCATION_INFO_LLA_VRP_BASED_BIT) {
        gnssLocInfoFlagMask |= GNSS_LOCATION_INFO_LLA_VRP_BASED_BIT;
    }
    if (pbGnssLocInfoFlagMask & PB_GNSS_LOCATION_INFO_ENU_VELOCITY_VRP_BASED_BIT) {
        gnssLocInfoFlagMask |= GNSS_LOCATION_INFO_ENU_VELOCITY_VRP_BASED_BIT;
    }
    if (pbGnssLocInfoFlagMask & PB_GNSS_LOCATION_INFO_DR_SOLUTION_STATUS_MASK_BIT) {
        gnssLocInfoFlagMask |= GNSS_LOCATION_INFO_DR_SOLUTION_STATUS_MASK_BIT;
    }
    if (pbGnssLocInfoExtFlagMask & PB_GNSS_LOCATION_INFO_ALTITUDE_ASSUMED_BIT) {
        gnssLocInfoFlagMask |= GNSS_LOCATION_INFO_ALTITUDE_ASSUMED_BIT;
    }
    if (pbGnssLocInfoExtFlagMask & PB_GNSS_LOCATION_INFO_SESSION_STATUS_BIT) {
        gnssLocInfoFlagMask |= GNSS_LOCATION_INFO_SESSION_STATUS_BIT;
    }

    LocApiPb_LOGv("LocApiPB: pbGnssLocInfoFlagMask:0x%x, pbGnssLocInfoExtFlagMask:0x%x, "
                  "gnssLocInfoFlagMask:0x%" PRIu64"", pbGnssLocInfoFlagMask,
                  pbGnssLocInfoExtFlagMask, gnssLocInfoFlagMask);

    return gnssLocInfoFlagMask;
}


uint32_t LocationApiPbMsgConv::getGnssLocationNavSolutionMaskFromPB(
        const uint32_t &pbGnssLocNavSoln) const {
    uint32_t gnssLocNavSoln = 0;
    if (pbGnssLocNavSoln & PB_LOCATION_SBAS_CORRECTION_IONO_BIT) {
        gnssLocNavSoln |= LOCATION_SBAS_CORRECTION_IONO_BIT;
    }
    if (pbGnssLocNavSoln & PB_LOCATION_SBAS_CORRECTION_FAST_BIT) {
        gnssLocNavSoln |= LOCATION_SBAS_CORRECTION_FAST_BIT;
    }
    if (pbGnssLocNavSoln & PB_LOCATION_SBAS_CORRECTION_LONG_BIT) {
        gnssLocNavSoln |= LOCATION_SBAS_CORRECTION_LONG_BIT;
    }
    if (pbGnssLocNavSoln & PB_LOCATION_SBAS_INTEGRITY_BIT) {
        gnssLocNavSoln |= LOCATION_SBAS_INTEGRITY_BIT;
    }
    if (pbGnssLocNavSoln & PB_LOCATION_NAV_CORRECTION_DGNSS_BIT) {
        gnssLocNavSoln |= LOCATION_NAV_CORRECTION_DGNSS_BIT;
    }
    if (pbGnssLocNavSoln & PB_LOCATION_NAV_CORRECTION_RTK_BIT) {
        gnssLocNavSoln |= LOCATION_NAV_CORRECTION_RTK_BIT;
    }
    if (pbGnssLocNavSoln & PB_LOCATION_NAV_CORRECTION_PPP_BIT) {
        gnssLocNavSoln |= LOCATION_NAV_CORRECTION_PPP_BIT;
    }
    if (pbGnssLocNavSoln & PB_LOCATION_NAV_CORRECTION_RTK_FIXED_BIT) {
        gnssLocNavSoln |= LOCATION_NAV_CORRECTION_RTK_FIXED_BIT;
    }
    if (pbGnssLocNavSoln & PB_LOCATION_NAV_CORRECTION_ONLY_SBAS_CORRECTED_SV_USED_BIT) {
        gnssLocNavSoln |= LOCATION_NAV_CORRECTION_ONLY_SBAS_CORRECTED_SV_USED_BIT;
    }
    LocApiPb_LOGv("LocApiPB: pbGnssLocNavSoln:%x, gnssLocNavSoln:%x", pbGnssLocNavSoln,
            gnssLocNavSoln);
    return gnssLocNavSoln;
}

uint32_t LocationApiPbMsgConv::getDrCalibrationStatusMaskFromPB(
        const uint32_t &pbDrCalibStatus) const {
    uint32_t drCalibStatus = 0;
    if (pbDrCalibStatus & PB_DR_ROLL_CALIBRATION_NEEDED) {
        drCalibStatus |= DR_ROLL_CALIBRATION_NEEDED;
    }
    if (pbDrCalibStatus & PB_DR_PITCH_CALIBRATION_NEEDED) {
        drCalibStatus |= DR_PITCH_CALIBRATION_NEEDED;
    }
    if (pbDrCalibStatus & PB_DR_YAW_CALIBRATION_NEEDED) {
        drCalibStatus |= DR_YAW_CALIBRATION_NEEDED;
    }
    if (pbDrCalibStatus & PB_DR_ODO_CALIBRATION_NEEDED) {
        drCalibStatus |= DR_ODO_CALIBRATION_NEEDED;
    }
    if (pbDrCalibStatus & PB_DR_GYRO_CALIBRATION_NEEDED) {
        drCalibStatus |= DR_GYRO_CALIBRATION_NEEDED;
    }
    LocApiPb_LOGv("LocApiPB: pbDrCalibStatus:%x, drCalibStatus:%x", pbDrCalibStatus,
            drCalibStatus);
    return drCalibStatus;
}

uint32_t LocationApiPbMsgConv::getGnssLocationPosDataMaskFromPB(
        const uint32_t &pbGnssLocPosDataMask) const {
    uint32_t gnssLocPosDataMask = 0;
    if (pbGnssLocPosDataMask & PB_LOCATION_NAV_DATA_HAS_LONG_ACCEL_BIT) {
        gnssLocPosDataMask |= LOCATION_NAV_DATA_HAS_LONG_ACCEL_BIT;
    }
    if (pbGnssLocPosDataMask & PB_LOCATION_NAV_DATA_HAS_LAT_ACCEL_BIT) {
        gnssLocPosDataMask |= LOCATION_NAV_DATA_HAS_LAT_ACCEL_BIT;
    }
    if (pbGnssLocPosDataMask & PB_LOCATION_NAV_DATA_HAS_VERT_ACCEL_BIT) {
        gnssLocPosDataMask |= LOCATION_NAV_DATA_HAS_VERT_ACCEL_BIT;
    }
    if (pbGnssLocPosDataMask & PB_LOCATION_NAV_DATA_HAS_YAW_RATE_BIT) {
        gnssLocPosDataMask |= LOCATION_NAV_DATA_HAS_YAW_RATE_BIT;
    }
    if (pbGnssLocPosDataMask & PB_LOCATION_NAV_DATA_HAS_PITCH_BIT) {
        gnssLocPosDataMask |= LOCATION_NAV_DATA_HAS_PITCH_BIT;
    }
    if (pbGnssLocPosDataMask & PB_LOCATION_NAV_DATA_HAS_LONG_ACCEL_UNC_BIT) {
        gnssLocPosDataMask |= LOCATION_NAV_DATA_HAS_LONG_ACCEL_UNC_BIT;
    }
    if (pbGnssLocPosDataMask & PB_LOCATION_NAV_DATA_HAS_LAT_ACCEL_UNC_BIT) {
        gnssLocPosDataMask |= LOCATION_NAV_DATA_HAS_LAT_ACCEL_UNC_BIT;
    }
    if (pbGnssLocPosDataMask & PB_LOCATION_NAV_DATA_HAS_VERT_ACCEL_UNC_BIT) {
        gnssLocPosDataMask |= LOCATION_NAV_DATA_HAS_VERT_ACCEL_UNC_BIT;
    }
    if (pbGnssLocPosDataMask & PB_LOCATION_NAV_DATA_HAS_YAW_RATE_UNC_BIT) {
        gnssLocPosDataMask |= LOCATION_NAV_DATA_HAS_YAW_RATE_UNC_BIT;
    }
    if (pbGnssLocPosDataMask & PB_LOCATION_NAV_DATA_HAS_PITCH_UNC_BIT) {
        gnssLocPosDataMask |= LOCATION_NAV_DATA_HAS_PITCH_UNC_BIT;
    }
    LocApiPb_LOGv("LocApiPB: pbGnssLocPosDataMask:%x, gnssLocPosDataMask:%x",
            pbGnssLocPosDataMask, gnssLocPosDataMask);
    return gnssLocPosDataMask;
}

uint32_t LocationApiPbMsgConv::getGnssLocationPosDataMaskExtFromPB(
        const uint32_t &pbGnssLocPosDataMask) const {
    uint32_t gnssLocPosDataMaskExt = 0;
    if (pbGnssLocPosDataMask & PB_LOCATION_NAV_DATA_HAS_PITCH_RATE_BIT) {
        gnssLocPosDataMaskExt |= LOCATION_NAV_DATA_HAS_PITCH_RATE_BIT;
    }
    if (pbGnssLocPosDataMask & PB_LOCATION_NAV_DATA_HAS_PITCH_RATE_UNC_BIT) {
        gnssLocPosDataMaskExt |= LOCATION_NAV_DATA_HAS_PITCH_RATE_UNC_BIT;
    }
    if (pbGnssLocPosDataMask & PB_LOCATION_NAV_DATA_HAS_ROLL_BIT) {
        gnssLocPosDataMaskExt |= LOCATION_NAV_DATA_HAS_ROLL_BIT;
    }
    if (pbGnssLocPosDataMask & PB_LOCATION_NAV_DATA_HAS_ROLL_UNC_BIT) {
        gnssLocPosDataMaskExt |= LOCATION_NAV_DATA_HAS_ROLL_UNC_BIT;
    }
    if (pbGnssLocPosDataMask & PB_LOCATION_NAV_DATA_HAS_ROLL_RATE_BIT) {
        gnssLocPosDataMaskExt |= LOCATION_NAV_DATA_HAS_ROLL_RATE_BIT;
    }
    if (pbGnssLocPosDataMask & PB_LOCATION_NAV_DATA_HAS_ROLL_RATE_UNC_BIT) {
        gnssLocPosDataMaskExt |= LOCATION_NAV_DATA_HAS_ROLL_RATE_UNC_BIT;
    }
    if (pbGnssLocPosDataMask & PB_LOCATION_NAV_DATA_HAS_YAW_BIT) {
        gnssLocPosDataMaskExt |= LOCATION_NAV_DATA_HAS_YAW_BIT;
    }
    if (pbGnssLocPosDataMask & PB_LOCATION_NAV_DATA_HAS_YAW_UNC_BIT) {
        gnssLocPosDataMaskExt |= LOCATION_NAV_DATA_HAS_YAW_UNC_BIT;
    }
    LocApiPb_LOGv("LocApiPB: pbGnssLocPosDataMask:%x, gnssLocPosDataMaskExt:%x",
            pbGnssLocPosDataMask, gnssLocPosDataMaskExt);
    return gnssLocPosDataMaskExt;
}

uint32_t LocationApiPbMsgConv::getGnssGloTimeStructTypeFlagsFromPB(
        const uint32_t &pbGnssGloTimeStruct) const {
    uint32_t gnssGloTimeStruct = 0;
    if (pbGnssGloTimeStruct & PB_GNSS_CLO_DAYS_VALID) {
        gnssGloTimeStruct |= GNSS_CLO_DAYS_VALID;
    }
    if (pbGnssGloTimeStruct & PB_GNSS_GLO_MSEC_VALID) {
        gnssGloTimeStruct |= GNSS_GLO_MSEC_VALID;
    }
    if (pbGnssGloTimeStruct & PB_GNSS_GLO_CLK_TIME_BIAS_VALID) {
        gnssGloTimeStruct |= GNSS_GLO_CLK_TIME_BIAS_VALID;
    }
    if (pbGnssGloTimeStruct & PB_GNSS_GLO_CLK_TIME_BIAS_UNC_VALID) {
        gnssGloTimeStruct |= GNSS_GLO_CLK_TIME_BIAS_UNC_VALID;
    }
    if (pbGnssGloTimeStruct & PB_GNSS_GLO_REF_FCOUNT_VALID) {
        gnssGloTimeStruct |= GNSS_GLO_REF_FCOUNT_VALID;
    }
    if (pbGnssGloTimeStruct & PB_GNSS_GLO_NUM_CLOCK_RESETS_VALID) {
        gnssGloTimeStruct |= GNSS_GLO_NUM_CLOCK_RESETS_VALID;
    }
    if (pbGnssGloTimeStruct & PB_GNSS_GLO_FOUR_YEAR_VALID) {
        gnssGloTimeStruct |= GNSS_GLO_FOUR_YEAR_VALID;
    }
    LocApiPb_LOGv("LocApiPB: pbGnssGloTimeStruct:%x, gnssGloTimeStruct:%x",
            pbGnssGloTimeStruct, gnssGloTimeStruct);
    return gnssGloTimeStruct;
}

uint32_t LocationApiPbMsgConv::getGnssConfigRobustLocationValidMaskFromPB(
        const uint32_t &pbGnssCfgRobstLocValidMask) const {
    uint32_t gnssCfgRobstLocValidMask = 0;
    if (pbGnssCfgRobstLocValidMask & PB_GNSS_CONFIG_ROBUST_LOCATION_ENABLED_VALID_BIT) {
        gnssCfgRobstLocValidMask |= GNSS_CONFIG_ROBUST_LOCATION_ENABLED_VALID_BIT;
    }
    if (pbGnssCfgRobstLocValidMask & PB_GNSS_CONFIG_ROBUST_LOCATION_ENABLED_FOR_E911_VALID_BIT) {
        gnssCfgRobstLocValidMask |=  GNSS_CONFIG_ROBUST_LOCATION_ENABLED_FOR_E911_VALID_BIT;
    }
    if (pbGnssCfgRobstLocValidMask & PB_GNSS_CONFIG_ROBUST_LOCATION_VERSION_VALID_BIT) {
        gnssCfgRobstLocValidMask |= GNSS_CONFIG_ROBUST_LOCATION_VERSION_VALID_BIT;
    }

    LocApiPb_LOGv("LocApiPB: pbGnssCfgRobstLocValidMask:%x, gnssCfgRobstLocValidMask:%x",
            pbGnssCfgRobstLocValidMask, gnssCfgRobstLocValidMask);
    return gnssCfgRobstLocValidMask;
}

uint64_t LocationApiPbMsgConv::getDeadReckoningEngineConfigValidMaskFromPB(
        const uint64_t &pbDrEngCfgVldMask) const {
    uint64_t drEngCfgValidMask = 0;
    if (pbDrEngCfgVldMask & PB_BODY_TO_SENSOR_MOUNT_PARAMS_BIT) {
        drEngCfgValidMask |= BODY_TO_SENSOR_MOUNT_PARAMS_BIT;
    }
    if (pbDrEngCfgVldMask & PB_VEHICLE_SPEED_SCALE_FACTOR_BIT) {
        drEngCfgValidMask |= VEHICLE_SPEED_SCALE_FACTOR_BIT;
    }
    if (pbDrEngCfgVldMask & PB_VEHICLE_SPEED_SCALE_FACTOR_UNC_BIT) {
        drEngCfgValidMask |= VEHICLE_SPEED_SCALE_FACTOR_UNC_BIT;
    }
    if (pbDrEngCfgVldMask & PB_GYRO_SCALE_FACTOR_BIT) {
        drEngCfgValidMask |= GYRO_SCALE_FACTOR_BIT;
    }
    if (pbDrEngCfgVldMask & PB_GYRO_SCALE_FACTOR_UNC_BIT) {
        drEngCfgValidMask |= GYRO_SCALE_FACTOR_UNC_BIT;
    }
    LocApiPb_LOGv("LocApiPB: pbDrEngCfgVldMask:%" PRIu64", drEngCfgValidMask:%" PRIu64,
            pbDrEngCfgVldMask, drEngCfgValidMask);
    return drEngCfgValidMask;
}

uint32_t LocationApiPbMsgConv::getDrEngineAidingDataMaskFromPB(
        const uint32_t &pbDrEngAidDataMask) const {
    uint32_t drEngAidDataMask = 0;
    if (pbDrEngAidDataMask & PB_DR_ENGINE_AIDING_DATA_CALIBRATION_BIT) {
        drEngAidDataMask |= DR_ENGINE_AIDING_DATA_CALIBRATION_BIT;
    }
    LocApiPb_LOGv("LocApiPB: pbDrEngAidDataMask:%x, drEngAidDataMask:%x",
            pbDrEngAidDataMask, drEngAidDataMask);
    return drEngAidDataMask;
}

uint32_t LocationApiPbMsgConv::getDrSolutionStatusMaskFromPB(
        const uint32_t &pbDrSolnStatusMask) const {
    uint32_t drSolnStatusMask = 0;
    if (pbDrSolnStatusMask & PB_VEHICLE_SENSOR_SPEED_INPUT_DETECTED) {
        drSolnStatusMask |= VEHICLE_SENSOR_SPEED_INPUT_DETECTED;
    }
    if (pbDrSolnStatusMask & PB_VEHICLE_SENSOR_SPEED_INPUT_USED) {
        drSolnStatusMask |= VEHICLE_SENSOR_SPEED_INPUT_USED;
    }
    LocApiPb_LOGv("LocApiPB: pbDrSolnStatusMask:%x, drSolnStatusMask:%x",
            pbDrSolnStatusMask, drSolnStatusMask);
    return drSolnStatusMask;
}

PBLocationSessionStatus LocationApiPbMsgConv::getPBEnumForLocSessionStatus(
        const loc_sess_status& status) const {
    PBLocationSessionStatus pbStatus = PB_LOCATION_SESS_FAILURE;

    if (status == LOC_SESS_SUCCESS) {
        pbStatus = PB_LOCATION_SESS_SUCCESS;
    } else if (status == LOC_SESS_INTERMEDIATE) {
        pbStatus = PB_LOCATION_SESS_INTERMEDIATE;
    }
    return pbStatus;
}

loc_sess_status LocationApiPbMsgConv::getLocSessionStatusFromPB(
        const PBLocationSessionStatus& pbStatus) const {

    loc_sess_status status = LOC_SESS_FAILURE;
    if (pbStatus == PB_LOCATION_SESS_SUCCESS) {
        status = LOC_SESS_SUCCESS;
    } else if (pbStatus == PB_LOCATION_SESS_INTERMEDIATE) {
        status = LOC_SESS_INTERMEDIATE;
    }
    return status;
}

// **** helper function for structure conversion to protobuf format
int LocationApiPbMsgConv::convertGnssSvTypeConfigToPB(const GnssSvTypeConfig &gnssSvTypeCfg,
        PBGnssSvTypeConfig *pbGnssSvTypeCfg) const {
    if (nullptr == pbGnssSvTypeCfg) {
        LOC_LOGe("pbGnssSvTypeCfg is NULL!, return");
        return 1;
    }
    // uint64 enabledSvTypesMask = 1;
    pbGnssSvTypeCfg->set_enabledsvtypesmask(gnssSvTypeCfg.enabledSvTypesMask);

    // uint64 blacklistedSvTypesMask = 2;
    pbGnssSvTypeCfg->set_blacklistedsvtypesmask(gnssSvTypeCfg.blacklistedSvTypesMask);

    LocApiPb_LOGd("LocApiPB: gnssSvTypeCfg - Enable Sv types: %" PRIu64 \
            " Blacklist Sv Types: %" PRIu64, gnssSvTypeCfg.enabledSvTypesMask,
            gnssSvTypeCfg.blacklistedSvTypesMask);
    return 0;
}

int LocationApiPbMsgConv::convertGnssSvIdConfigToPB(const GnssSvIdConfig &gnssSvIdCfg,
        PBGnssSvIdConfig *pbGnssSvIdCfg) const {
    if (nullptr == pbGnssSvIdCfg) {
        LOC_LOGe("pbGnssSvIdCfg is NULL!, return");
        return 1;
    }
    // uint64 gloBlacklistSvMask = 1;
    pbGnssSvIdCfg->set_globlacklistsvmask(gnssSvIdCfg.gloBlacklistSvMask);

    // uint64 bdsBlacklistSvMask = 2;
    pbGnssSvIdCfg->set_bdsblacklistsvmask(gnssSvIdCfg.bdsBlacklistSvMask);

    // uint64 qzssBlacklistSvMask = 3;
    pbGnssSvIdCfg->set_qzssblacklistsvmask(gnssSvIdCfg.qzssBlacklistSvMask);

    // uint64 galBlacklistSvMask = 4;
    pbGnssSvIdCfg->set_galblacklistsvmask(gnssSvIdCfg.galBlacklistSvMask);

    // uint64 sbasBlacklistSvMask = 5;
    pbGnssSvIdCfg->set_sbasblacklistsvmask(gnssSvIdCfg.sbasBlacklistSvMask);

    // uint64_t navicBlacklistSvMask = 6;
    pbGnssSvIdCfg->set_navicblacklistsvmask(gnssSvIdCfg.navicBlacklistSvMask);

    LocApiPb_LOGd("LocApiPB: gnssSvIdCfg - Glo: %" PRIu64 ",Bds: %" PRIu64 ",Qzss: %" PRIu64 \
            ",Gal: %" PRIu64 ",Sbas: %" PRIu64",Nav: %" PRIu64, gnssSvIdCfg.gloBlacklistSvMask,
            gnssSvIdCfg.bdsBlacklistSvMask, gnssSvIdCfg.qzssBlacklistSvMask,
            gnssSvIdCfg.galBlacklistSvMask, gnssSvIdCfg.sbasBlacklistSvMask,
            gnssSvIdCfg.navicBlacklistSvMask);
    return 0;
}

int LocationApiPbMsgConv::convertGnssAidingDataToPB(const GnssAidingData &gnssAidData,
        PBAidingData *pbGnssAidData) const {
    if (nullptr == pbGnssAidData) {
        LOC_LOGe("pbGnssAidData is NULL!, return");
        return 1;
    }

    //bool deleteAll  = 1;
    pbGnssAidData->set_deleteall(gnssAidData.deleteAll);

    // uint32 gnssAidingDataSvMask = 2; - bitwise OR of PBLocApiGnssAidingDataSvMask
    pbGnssAidData->set_gnssaidingdatasvmask(
        getPBMaskForGnssAidingDataSvMask(gnssAidData.sv.svMask));

    // uint32 dreAidingDataMask = 3;- PBDrEngineAidingDataMask
    pbGnssAidData->set_dreaidingdatamask(
        getPBMaskForDrEngineAidingDataMask(gnssAidData.dreAidingDataMask));

    // Masks from - PBLocApiPositioningEngineMask
    // uint32 posEngineMask = 4;
    pbGnssAidData->set_posenginemask(
            getPBMaskForPositioningEngineMask(gnssAidData.posEngineMask));

    LocApiPb_LOGd("LocApiPB: gnssAidData deleteAll:%d, svMask:%x, PosEngMask:%x",
            gnssAidData.deleteAll, gnssAidData.sv.svMask, gnssAidData.posEngineMask);
    return 0;
}

int LocationApiPbMsgConv::convertLeverArmConfigInfoToPB(const LeverArmConfigInfo &leverArmCfgInfo,
        PBLIALeverArmConfigInfo *pbLeverArmCfgInfo) const {
    if (nullptr == pbLeverArmCfgInfo) {
        LOC_LOGe("pbLeverArmCfgInfo is NULL!, return");
        return 1;
    }

    // uint32 leverArmValidMask = 1; - PBLIALeverArmTypeMask
    pbLeverArmCfgInfo->set_leverarmvalidmask(
            getPBMaskForLeverArmTypeMask(leverArmCfgInfo.leverArmValidMask));

    // PBLIALeverArmParams   gnssToVRP = 2;
    PBLIALeverArmParams* gnssToVRP = pbLeverArmCfgInfo->mutable_gnsstovrp();
    if (nullptr != gnssToVRP) {
        if (convertLeverArmParamsToPB(leverArmCfgInfo.gnssToVRP, gnssToVRP)) {
            LOC_LOGe("convertLeverArmParamsToPB failed");
            free(gnssToVRP);
            return 1;
        }
    } else {
        LOC_LOGe("mutable_drimutognss failed");
        return 1;
    }

    // PBLIALeverArmParams   drImuToGnss = 3;
    PBLIALeverArmParams* drImuToGnss = pbLeverArmCfgInfo->mutable_drimutognss();
    if (nullptr != drImuToGnss) {
        if (convertLeverArmParamsToPB(leverArmCfgInfo.drImuToGnss, drImuToGnss)) {
            LOC_LOGe("convertLeverArmParamsToPB failed");
            free(drImuToGnss);
            return 1;
        }
    } else {
        LOC_LOGe("mutable_drimutognss failed");
        return 1;
    }

    // PBLIALeverArmParams   veppImuToGnss = 4;
    PBLIALeverArmParams* veppImuToGnss = pbLeverArmCfgInfo->mutable_veppimutognss();
    if (nullptr != veppImuToGnss) {
        if (convertLeverArmParamsToPB(leverArmCfgInfo.veppImuToGnss, veppImuToGnss)) {
            LOC_LOGe("convertLeverArmParamsToPB failed");
            free(veppImuToGnss);
            return 1;
        }
    } else {
        LOC_LOGe("mutable_veppimutognss failed");
        return 1;
    }

    LocApiPb_LOGd("LocApiPB: leverArmCfgInfo - leverArmValidMask: %x",
            leverArmCfgInfo.leverArmValidMask);
    return 0;
}

int LocationApiPbMsgConv::convertBodyToSensorMountParamsToPB(
        const BodyToSensorMountParams &bodyToSensorMntParams,
        PBLIABodyToSensorMountParams *pbBodyToSensorMntParams) const {
    if (nullptr == pbBodyToSensorMntParams) {
        LOC_LOGe("pbBodyToSensorMntParams is NULL!, return");
        return 1;
    }
    // float rollOffset = 1;
    pbBodyToSensorMntParams->set_rolloffset(bodyToSensorMntParams.rollOffset);
    // float yawOffset = 2;
    pbBodyToSensorMntParams->set_yawoffset(bodyToSensorMntParams.yawOffset);
    // float pitchOffset = 3;
    pbBodyToSensorMntParams->set_pitchoffset(bodyToSensorMntParams.pitchOffset);
    // float offsetUnc = 4;
    pbBodyToSensorMntParams->set_offsetunc(bodyToSensorMntParams.offsetUnc);
    LocApiPb_LOGd("LocApiPB: bodyToSensorMntParams: Offset - Roll:%f, Yaw:%f, Pitch:%f, Unc:%f",
            bodyToSensorMntParams.rollOffset, bodyToSensorMntParams.yawOffset,
            bodyToSensorMntParams.pitchOffset, bodyToSensorMntParams.offsetUnc);
    return 0;
}

int LocationApiPbMsgConv::convertDeadReckoningEngineConfigToPB(
        const DeadReckoningEngineConfig &drEngConfig,
        PBDeadReckoningEngineConfig *pbDrEngConfig) const {
    if (nullptr == pbDrEngConfig) {
        LOC_LOGe("pbDrEngConfig is NULL!, return");
        return 1;
    }
    // uint64    validMask = 1; - Bitwise OR mask of PBDeadReckoningEngineConfigValidMask
    pbDrEngConfig->set_validmask(
            getPBMaskForDeadReckoningEngineConfigValidMask(drEngConfig.validMask));

    // PBLIABodyToSensorMountParams bodyToSensorMountParams = 2;
    PBLIABodyToSensorMountParams* b2SMountParams = pbDrEngConfig->mutable_bodytosensormountparams();
    if (nullptr != b2SMountParams) {
        if (convertBodyToSensorMountParamsToPB(drEngConfig.bodyToSensorMountParams,
                b2SMountParams)) {
            LOC_LOGe("convertBodyToSensorMountParamsToPB failed");
            free(b2SMountParams);
            return 1;
        }
    } else {
        LOC_LOGe("mutable_bodytosensormountparams failed");
        return 1;
    }

    // float vehicleSpeedScaleFactor = 3;
    pbDrEngConfig->set_vehiclespeedscalefactor(drEngConfig.vehicleSpeedScaleFactor);
    // float vehicleSpeedScaleFactorUnc = 4;
    pbDrEngConfig->set_vehiclespeedscalefactorunc(drEngConfig.vehicleSpeedScaleFactorUnc);
    // float gyroScaleFactor = 5;
    pbDrEngConfig->set_gyroscalefactor(drEngConfig.gyroScaleFactor);
    // float gyroScaleFactorUnc = 6;
    pbDrEngConfig->set_gyroscalefactorunc(drEngConfig.gyroScaleFactorUnc);

    LOC_LOGd("LocApiPB: drEngConfig - DrEngConfigValidMask:%"  PRIu64", VehSpeedScale: %f"
            " VehSpeedScaleUnc: %f, GyroScale: %f, GyroScaleUnc: %f", drEngConfig.validMask,
            drEngConfig.vehicleSpeedScaleFactor, drEngConfig.vehicleSpeedScaleFactorUnc,
            drEngConfig.gyroScaleFactor, drEngConfig.gyroScaleFactorUnc);
    return 0;
}

int LocationApiPbMsgConv::convertLocationOptionsToPB(const LocationOptions &locOpt,
        PBLocationOptions *pbLocOpt) const {
    if (nullptr == pbLocOpt) {
        LOC_LOGe("pbLocOpt is NULL!, return");
        return 1;
    }
    // uint32 minInterval = 1;
    pbLocOpt->set_mininterval(locOpt.minInterval);

    // uint32 minDistance = 2;
    pbLocOpt->set_mindistance(locOpt.minDistance);

    // PBGnssSuplMode mode = 3;
    pbLocOpt->set_mode(getPBEnumForGnssSuplMode(locOpt.mode));

    // uint32 locReqEngTypeMask = 4; - bitwise OR of PBLocReqEngineTypeMask
    pbLocOpt->set_locreqengtypemask(getPBMaskForLocReqEngineTypeMask(locOpt.locReqEngTypeMask));

    LocApiPb_LOGd("LocApiPB: locOpt - MinInterval: %u, MinDistance:%u, GnssSuplMode:%d, "\
            "LocReqEngineTypeMask:%x", locOpt.minInterval, locOpt.minDistance, locOpt.mode,
            locOpt.locReqEngTypeMask);
    return 0;
}

int LocationApiPbMsgConv::convertGfAddedReqPayloadToPB(
        const GeofencesAddedReqPayload &gfAddReqPayload,
        PBGeofencesAddedReqPayload *pbGfAddReqPayload) const {
    if (nullptr == pbGfAddReqPayload) {
        LOC_LOGe("pbGfAddReqPayload is NULL!, return");
        return 1;
    }
    // repeated PBGeofencePayload gfPayload = 1;
    LOC_LOGd("LocApiPB: gfPayload count:%d", gfAddReqPayload.count);
    for (uint32_t i=0; i < gfAddReqPayload.count; i++) {
        PBGeofencePayload* gfPload = pbGfAddReqPayload->add_gfpayload();
        if (nullptr != gfPload) {
            // uint32 gfClientId = 1;
            gfPload->set_gfclientid(gfAddReqPayload.gfPayload[i].gfClientId);
            LocApiPb_LOGv("LocApiPB: gfPayload[%u] client Id:%u", i, gfAddReqPayload.count);

            // PBGeofenceOption gfOption = 2;
            PBGeofenceOption *pbGfOpt = gfPload->mutable_gfoption();
            if (nullptr != pbGfOpt) {
                if (convertGeofenceOptionToPB(gfAddReqPayload.gfPayload[i].gfOption, pbGfOpt)) {
                    LOC_LOGe("convertGeofenceOptionToPB failed");
                    free(pbGfOpt);
                    return 1;
                }
            } else {
                LOC_LOGe("mutable_gfoption failed");
                return 1;
            }

            // PBGeofenceInfo gfInfo = 3;
            PBGeofenceInfo* pbGfInfo = gfPload->mutable_gfinfo();
            if (nullptr != pbGfInfo) {
                if (convertGeofenceInfoToPB(gfAddReqPayload.gfPayload[i].gfInfo, pbGfInfo)) {
                    LOC_LOGe("convertGeofenceInfoToPB failed");
                    free(pbGfInfo);
                    return 1;
                }
            } else {
                LOC_LOGe("mutable_gfinfo failed");
                return 1;
            }
        } else {
            LOC_LOGe("add_gfpayload is NULL");
            return 1;
        }
    }

    return 0;
}

int LocationApiPbMsgConv::convertGfReqClientIdPayloadToPB(
        const GeofencesReqClientIdPayload &gfReqClntIdPayload,
        PBGeofencesReqClientIdPayload *pbGfReqClntIdPayload) const {
    if (nullptr == pbGfReqClntIdPayload) {
        LOC_LOGe("pbGfReqClntIdPayload is NULL!, return");
        return 1;
    }
    // repeated uint32 gfIds = 1;
    uint32_t gfCount = gfReqClntIdPayload.count;
    LocApiPb_LOGd("LocApiPB: gfReqClntIdPayload - count %u", gfCount);
    for (uint32_t i = 0; i < gfCount; i++) {
        LocApiPb_LOGv("LocApiPB: gfReqClntIdPayload gfIds[%u] - %u", i,
                gfReqClntIdPayload.gfIds[i]);
        pbGfReqClntIdPayload->add_gfids(gfReqClntIdPayload.gfIds[i]);
    }
    return 0;
}

int LocationApiPbMsgConv::convertCollectiveResPayloadToPB(
        const CollectiveResPayload &collctResPload,
        PBCollectiveResPayload *pbCollctResPload) const {
    if (nullptr == pbCollctResPload) {
        LOC_LOGe("pbCollctResPload is NULL!, return");
        return 1;
    }

    // repeated PBGeofenceResponse resp = 1;
    LocApiPb_LOGd("LocApiPB: collctResPload count:%d", collctResPload.count);
    for (int i=0; i < collctResPload.count; i++) {
        LocApiPb_LOGv("LocApiPB: collctResPload clientId:%u, error:%d",
                collctResPload.resp[i].clientId, collctResPload.resp[i].error);

        PBGeofenceResponse *gfResp = pbCollctResPload->add_resp();
        if (nullptr != gfResp) {
            // uint32 clientId = 1;
            gfResp->set_clientid(collctResPload.resp[i].clientId);
            // PBLocationError error = 2;
            gfResp->set_error(getPBEnumForLocationError(collctResPload.resp[i].error));
        } else {
            LOC_LOGe("add_resp is NULL");
            return 1;
        }
    }
    return 0;
}

int LocationApiPbMsgConv::convertGnssConfigRobustLocationToPB(
        const GnssConfigRobustLocation &gnssCfgRbstLoc,
        PBGnssConfigRobustLocation *pbGnssCfgRbstLoc) const {
    if (nullptr == pbGnssCfgRbstLoc) {
        LOC_LOGe("pbGnssCfgRbstLoc is NULL!, return");
        return 1;
    }

    // uint32 validMask = 1; - bitwise OR of PBGnssConfigRobustLocationValidMask
    pbGnssCfgRbstLoc->set_validmask(
        getPBMaskForGnssCfgRobustLocValidMask(gnssCfgRbstLoc.validMask));
    // bool enabled = 2;
    pbGnssCfgRbstLoc->set_enabled(gnssCfgRbstLoc.enabled);
    // bool enabledForE911 = 3;
    pbGnssCfgRbstLoc->set_enabledfore911(gnssCfgRbstLoc.enabledForE911);

    // PBGnssConfigRobustLocationVersion version = 4;
    PBGnssConfigRobustLocationVersion *cfRbstLocVer = pbGnssCfgRbstLoc->mutable_version();
    if (nullptr != cfRbstLocVer) {
        // uint32 major = 1;
        cfRbstLocVer->set_major(gnssCfgRbstLoc.version.major);
        // uint32 minor = 2;
        cfRbstLocVer->set_minor(gnssCfgRbstLoc.version.minor);
    } else {
        LOC_LOGe("cfRbstLocVer is NULL");
        return 1;
    }

    LOC_LOGd("LocApiPB: gnssCfgRbstLoc - CfgRobustLocMask:%x, Enabled:%d, EnabForE911:%d, "\
        "MajorVer:%d, MinorVer:%d", gnssCfgRbstLoc.validMask, gnssCfgRbstLoc.enabled,
        gnssCfgRbstLoc.enabledForE911, gnssCfgRbstLoc.version.major, gnssCfgRbstLoc.version.minor);
    return 0;
}

int LocationApiPbMsgConv::convertLocationToPB(const Location &location,
        PBLocation *pbLocation) const {
    if (nullptr == pbLocation) {
        LOC_LOGe("pbLocation is NULL!, return");
        return 1;
    }
    // uint32 flags = 1; - PBLocationFlagsMask
    pbLocation->set_flags(getPBMaskForLocationFlagsMask(location.flags));

    // uint64 timestamp = 2;
    pbLocation->set_timestamp(location.timestamp);

    // double latitude = 3;
    pbLocation->set_latitude(location.latitude);

    // double longitude = 4;
    pbLocation->set_longitude(location.longitude);

    // double altitude = 5;
    pbLocation->set_altitude(location.altitude);

    // float speed = 6;
    pbLocation->set_speed(location.speed);

    // float bearing = 7;
    pbLocation->set_bearing(location.bearing);

    // float horizontalAccuracy = 8;
    pbLocation->set_horizontalaccuracy(location.accuracy);

    // float verticalAccuracy = 9;
    pbLocation->set_verticalaccuracy(location.verticalAccuracy);

    // float speedAccuracy = 10;
    pbLocation->set_speedaccuracy(location.speedAccuracy);

    // float bearingAccuracy = 11;
    pbLocation->set_bearingaccuracy(location.bearingAccuracy);

    // uint32 techMask = 12; - PBLocationTechnologyMask
    pbLocation->set_techmask(getPBMaskForLocationTechnologyMask(location.techMask));

    LOC_LOGd("LocApiPB: location - Timestamp: %" PRIu64" Lat:%lf, Lon:%lf, Alt:%lf, TechMask:%x",
            location.timestamp, location.latitude, location.longitude, location.altitude,
            location.techMask);
    LocApiPb_LOGd("LocApiPB: location - speed:%f, bear:%f, HorzAcc:%f, VertAcc:%f, SpeedAcc:%f, "
            "BearAcc:%f", location.speed, location.bearing, location.accuracy,
            location.verticalAccuracy, location.speedAccuracy, location.bearingAccuracy);
    return 0;
}

int LocationApiPbMsgConv::convertLocAPIBatchingNotifMsgToPB(
        const LocAPIBatchNotification &locApiBatchNotifMsg,
        PBLocAPIBatchNotification *pbLocApiBatchNotifMsg) const {
    if (nullptr == pbLocApiBatchNotifMsg) {
        LOC_LOGe("pbLocApiBatchNotifMsg is NULL!, return");
        return 1;
    }
    LOC_LOGd("LocApiPB: locApiBatchNotifMsg - BatchStat: %d, Loc count:%u",
            locApiBatchNotifMsg.status, locApiBatchNotifMsg.count);
    // PBBatchingStatus status = 1;
    pbLocApiBatchNotifMsg->set_status(
            getPBEnumForBatchingStatus(locApiBatchNotifMsg.status));
    // repeated PBLocation location = 2;
    uint32_t count = locApiBatchNotifMsg.count;
    for (int i=0; i < count; i++) {
        PBLocation* location = pbLocApiBatchNotifMsg->add_location();
        if (nullptr != location) {
            if (convertLocationToPB(locApiBatchNotifMsg.location[i], location)) {
                LOC_LOGe("convertLocationToPB failed");
                free(location);
                return 1;
            }
        } else {
            LOC_LOGe("add_location is NULL");
            return 1;
        }
    }
    return 0;
}

int LocationApiPbMsgConv::convertLocAPIGfBreachNotifToPB(
        const LocAPIGeofenceBreachNotification &locApiGfBreachNotif,
        PBLocAPIGeofenceBreachNotification *pbLocApiGfBreachNotif) const {
    if (nullptr == pbLocApiGfBreachNotif) {
        LOC_LOGe("pbLocApiGfBreachNotif is NULL!, return");
        return 1;
    }
    LOC_LOGd("LocApiPB: locApiGfBreachNotif - BreachTypMask: %x, timestamp: %" PRIu64 \
            "count:%d", locApiGfBreachNotif.type, locApiGfBreachNotif.timestamp,
            locApiGfBreachNotif.count);

    // uint64 timestamp = 1;
    pbLocApiGfBreachNotif->set_timestamp(locApiGfBreachNotif.timestamp);

    // uint32 breachtype = 2; - PBGeofenceBreachTypeMask
    pbLocApiGfBreachNotif->set_breachtype(locApiGfBreachNotif.type);

    // PBLocation location = 3;
    PBLocation* location = pbLocApiGfBreachNotif->mutable_location();
    if (nullptr != location) {
        if (convertLocationToPB(locApiGfBreachNotif.location, location)) {
            LOC_LOGe("convertLocationToPB failed");
            free(location);
            return 1;
        }
    } else {
        LOC_LOGe("mutable_locationnotification failed");
        return 1;
    }

    // repeated uint32 id = 4;
    for (int i = 0; i < locApiGfBreachNotif.count; i++) {
        pbLocApiGfBreachNotif->add_id(locApiGfBreachNotif.id[i]);
    }
    return 0;
}

int LocationApiPbMsgConv::convertGnssLocInfoNotifToPB(
        const GnssLocationInfoNotification &gnssLocInfoNotif,
        PBGnssLocationInfoNotification *pbGnssLocInfoNotif) const {
    if (nullptr == pbGnssLocInfoNotif) {
        LOC_LOGe("pbGnssLocInfoNotif is NULL!, return");
        return 1;
    }
    // PBLocation location = 1;
    PBLocation* location = pbGnssLocInfoNotif->mutable_location();
    if (nullptr != location) {
        if (convertLocationToPB(gnssLocInfoNotif.location, location)) {
            LOC_LOGe("convertLocationToPB failed");
            free(location);
            return 1;
        }
    } else {
        LOC_LOGe("mutable_location failed");
        return 1;
    }

    // uint32 flags = 2; - bitwise OR of PBGnssLocationInfoFlagMask for param validity
    pbGnssLocInfoNotif->set_flags(getPBMaskForGnssLocationInfoFlagMask(gnssLocInfoNotif.flags));

    // float altitudeMeanSeaLevel = 3;
    pbGnssLocInfoNotif->set_altitudemeansealevel(gnssLocInfoNotif.altitudeMeanSeaLevel);

    // float pdop = 4;
    // float hdop = 5;
    // float vdop = 6;
    // float gdop = 7;
    // float tdop = 8;
    // float magneticDeviation = 9;
    pbGnssLocInfoNotif->set_pdop(gnssLocInfoNotif.pdop);
    pbGnssLocInfoNotif->set_hdop(gnssLocInfoNotif.hdop);
    pbGnssLocInfoNotif->set_vdop(gnssLocInfoNotif.vdop);
    pbGnssLocInfoNotif->set_gdop(gnssLocInfoNotif.gdop);
    pbGnssLocInfoNotif->set_tdop(gnssLocInfoNotif.tdop);
    pbGnssLocInfoNotif->set_magneticdeviation(gnssLocInfoNotif.magneticDeviation);

    // PBLocationReliability horReliability = 10;
    // PBLocationReliability verReliability = 11;
    pbGnssLocInfoNotif->set_horreliability(
            getPBEnumForLocationReliability(gnssLocInfoNotif.horReliability));
    pbGnssLocInfoNotif->set_verreliability(
            getPBEnumForLocationReliability(gnssLocInfoNotif.verReliability));

    // float horUncEllipseSemiMajor = 12;
    // float horUncEllipseSemiMinor = 13;
    // float horUncEllipseOrientAzimuth = 14;
    pbGnssLocInfoNotif->set_horuncellipsesemimajor(gnssLocInfoNotif.horUncEllipseSemiMajor);
    pbGnssLocInfoNotif->set_horuncellipsesemiminor(gnssLocInfoNotif.horUncEllipseSemiMinor);
    pbGnssLocInfoNotif->set_horuncellipseorientazimuth(
            gnssLocInfoNotif.horUncEllipseOrientAzimuth);

    // float northStdDeviation = 15;
    // float eastStdDeviation = 16;
    // float northVelocity = 17;
    // float eastVelocity = 18;
    // float upVelocity = 19;
    // float northVelocityStdDeviation = 20;
    // float eastVelocityStdDeviation = 21;
    // float upVelocityStdDeviation = 22;
    pbGnssLocInfoNotif->set_northstddeviation(gnssLocInfoNotif.northStdDeviation);
    pbGnssLocInfoNotif->set_eaststddeviation(gnssLocInfoNotif.eastStdDeviation);
    pbGnssLocInfoNotif->set_northvelocity(gnssLocInfoNotif.northVelocity);
    pbGnssLocInfoNotif->set_eastvelocity(gnssLocInfoNotif.eastVelocity);
    pbGnssLocInfoNotif->set_upvelocity(gnssLocInfoNotif.upVelocity);
    pbGnssLocInfoNotif->set_northvelocitystddeviation(gnssLocInfoNotif.northVelocityStdDeviation);
    pbGnssLocInfoNotif->set_eastvelocitystddeviation(gnssLocInfoNotif.eastVelocityStdDeviation);
    pbGnssLocInfoNotif->set_upvelocitystddeviation(gnssLocInfoNotif.upVelocityStdDeviation);

    // uint32 numSvUsedInPosition = 23;
    pbGnssLocInfoNotif->set_numsvusedinposition(gnssLocInfoNotif.numSvUsedInPosition);

    // PBGnssLocationSvUsedInPosition svUsedInPosition = 24;
    PBGnssLocationSvUsedInPosition* gnssLocSvUsedInPos =
            pbGnssLocInfoNotif->mutable_svusedinposition();
    if (nullptr != gnssLocSvUsedInPos) {
        if (convertGnssLocSvUsedInPosToPB(gnssLocInfoNotif.svUsedInPosition, gnssLocSvUsedInPos)) {
            LOC_LOGe("convertLocationToPB failed");
            free(gnssLocSvUsedInPos);
            return 1;
        }
    } else {
        LOC_LOGe("mutable_svusedinposition failed");
        return 1;
    }

    // uint32 navSolutionMask = 25;  - bitwise OR of PBGnssLocationNavSolutionMask
    pbGnssLocInfoNotif->set_navsolutionmask(
            getPBMaskForGnssLocationNavSolutionMask(gnssLocInfoNotif.navSolutionMask));

    // PBLocApiGnssLocationPositionDynamics bodyFrameData = 26;
    // GnssLocationPositionDynamicsExt bodyFrameDataExt - Additional Body Frame Dynamics.
    PBLocApiGnssLocationPositionDynamics* gnssLocPosDyn =
            pbGnssLocInfoNotif->mutable_bodyframedata();
    if (nullptr != gnssLocPosDyn) {
        if (convertGnssLocationPositionDynamicsToPB(gnssLocInfoNotif.bodyFrameData,
                gnssLocInfoNotif.bodyFrameDataExt, gnssLocPosDyn)) {
            LOC_LOGe("convertGnssLocationPositionDynamicsToPB failed");
            free(gnssLocPosDyn);
            return 1;
        }
    } else {
        LOC_LOGe("mutable_bodyFrameData failed");
        return 1;
    }

    // PBLocApiGnssSystemTime gnssSystemTime = 27;
    PBLocApiGnssSystemTime* gnssSysTime = pbGnssLocInfoNotif->mutable_gnsssystemtime();
    if (nullptr != gnssSysTime) {
        if (convertGnssSystemTimeToPB(gnssLocInfoNotif.gnssSystemTime, gnssSysTime)) {
            LOC_LOGe("convertGnssSystemTimeToPB failed");
            free(gnssSysTime);
            return 1;
        }
    } else {
        LOC_LOGe("mutable_gnsssystemtime failed");
        return 1;
    }

    // uint32 numOfMeasReceived = 28;
    pbGnssLocInfoNotif->set_numofmeasreceived(gnssLocInfoNotif.numOfMeasReceived);

    // repeated PBGnssMeasUsageInfo measUsageInfo = 29; (Max array len - GNSS_SV_MAX)
    uint8_t count = gnssLocInfoNotif.numOfMeasReceived;
    LOC_LOGd("LocApiPB: gnssLocInfoNotif numOfMeasReceived : %u", count);
    for (uint8_t iter = 0; iter < count; iter++) {
        PBGnssMeasUsageInfo *gnssMeasUsageInfo = pbGnssLocInfoNotif->add_measusageinfo();
        if (nullptr != gnssMeasUsageInfo) {
            if (convertGnssMeasUsageInfoToPB(gnssLocInfoNotif.measUsageInfo[iter],
                    gnssMeasUsageInfo)) {
                LOC_LOGe("convertGnssLocInfoNotifToPB failed");
                free(gnssMeasUsageInfo);
                return 1;
            }
        } else {
            LOC_LOGe("add_measusageinfo is NULL");
            return 1;
        }
    }

    // uint32 leapSeconds = 30;
    pbGnssLocInfoNotif->set_leapseconds(gnssLocInfoNotif.leapSeconds);

    // float timeUncMs = 31;
    pbGnssLocInfoNotif->set_timeuncms(gnssLocInfoNotif.timeUncMs);

    // uint32 calibrationConfidence = 32;
    pbGnssLocInfoNotif->set_calibrationconfidence(gnssLocInfoNotif.calibrationConfidence);

    // uint32 calibrationStatus = 33; - PBLocApiDrCalibrationStatusMask
    pbGnssLocInfoNotif->set_calibrationstatus(
            getPBMaskForDrCalibrationStatusMask(gnssLocInfoNotif.calibrationStatus));

    // PBLocApiOutputEngineType locOutputEngType = 34;
    pbGnssLocInfoNotif->set_locoutputengtype(
            getPBEnumForLocOutputEngineType(gnssLocInfoNotif.locOutputEngType));

    // uint32 locOutputEngMask = 35; - bitwise OR of PBLocApiPositioningEngineMask
    pbGnssLocInfoNotif->set_locoutputengmask(
            getPBMaskForPositioningEngineMask(gnssLocInfoNotif.locOutputEngMask));

    // float conformityIndex = 36;
    pbGnssLocInfoNotif->set_conformityindex(gnssLocInfoNotif.conformityIndex);

    // PBLLAInfo llaVRPBased = 37;
    PBLLAInfo* llaInfo = pbGnssLocInfoNotif->mutable_llavrpbased();
    if (nullptr != llaInfo) {
        if (convertLLAInfoToPB(gnssLocInfoNotif.llaVRPBased, llaInfo)) {
            LOC_LOGe("convertLLAInfoToPB failed");
            free(llaInfo);
            return 1;
        }
    } else {
        LOC_LOGe("mutable_llavrpbased failed");
        return 1;
    }

    // repeated float enuVelocityVRPBased = 38; - Max array length 3
    for (int i = 0; i < 3; i++) {
        LOC_LOGd("LocApiPB: gnssLocInfoNotif - jammerInd: %lf",
                gnssLocInfoNotif.enuVelocityVRPBased[i]);
        pbGnssLocInfoNotif->add_enuvelocityvrpbased(gnssLocInfoNotif.enuVelocityVRPBased[i]);
    }

    // uint32 drSolutionStatusMask = 39; - PBDrSolutionStatusMask
    pbGnssLocInfoNotif->set_drsolutionstatusmask(getPBMaskForDrSolutionStatusMask(
            (uint32_t)gnssLocInfoNotif.drSolutionStatusMask));

    // uint32 extFlags = 40;
    // bitwise OR of PBGnssLocationInfoExtFlagMask for fields 41 and onwards
    pbGnssLocInfoNotif->set_extflags(
            getPBMaskForGnssLocationInfoExtFlagMask(gnssLocInfoNotif.flags));

    // bool altitudeAssumed = 41;
    pbGnssLocInfoNotif->set_altitudeassumed(gnssLocInfoNotif.altitudeAssumed);

    // bool sessionStatus = 42
    pbGnssLocInfoNotif->set_sessionstatus(
            getPBEnumForLocSessionStatus(gnssLocInfoNotif.sessionStatus));

    LocApiPb_LOGd("LocApiPB: gnssLocInfoNotif - GLocInfoFlgMask:%u, pdop:%f, hdop:%f, vdop:%f",
            gnssLocInfoNotif.flags, gnssLocInfoNotif.pdop, gnssLocInfoNotif.hdop,
            gnssLocInfoNotif.vdop);
    LocApiPb_LOGd("LocApiPB: gnssLocInfoNotif - HorReliab:%d, VerReliab:%d, HorUnc-SemiMajor:%f "
            "SemiMinor:%f",
            gnssLocInfoNotif.horReliability, gnssLocInfoNotif.verReliability,
            gnssLocInfoNotif.horUncEllipseSemiMajor, gnssLocInfoNotif.horUncEllipseSemiMinor);
    LOC_LOGd("LocApiPB: gnssLocInfoNotif - NavSolMask:%x, NumMeasRcvd:%u, "\
            "LocOpEngType:%d, PosEngMask:%x,  NumSvUsedInPos:%u",
            gnssLocInfoNotif.navSolutionMask, gnssLocInfoNotif.numOfMeasReceived,
            gnssLocInfoNotif.locOutputEngType, gnssLocInfoNotif.locOutputEngMask,
            gnssLocInfoNotif.numSvUsedInPosition);
    return 0;
}

int LocationApiPbMsgConv::convertLocSysInfoToPB(const LocationSystemInfo &locSysInfo,
        PBLocationSystemInfo *pbLocSysInfo) const {
    if (nullptr == pbLocSysInfo) {
        LOC_LOGe("pbLocSysInfo is NULL!, return");
        return 1;
    }
    LOC_LOGd("LocApiPB: locSysInfo - sysInfoMask: %x", locSysInfo.systemInfoMask);
    // uint32 systemInfoMask = 1; - bitwise OR of PBLocationSystemInfoMask
    pbLocSysInfo->set_systeminfomask(getPBMaskForLocSysInfoMask(locSysInfo.systemInfoMask));

    // PBLeapSecondSystemInfo  leapSecondSysInfo = 2;
    PBLeapSecondSystemInfo* leapSecSysInfo = pbLocSysInfo->mutable_leapsecondsysinfo();
    if (nullptr != leapSecSysInfo) {
        if (convertLeapSecondSystemInfoToPB(locSysInfo.leapSecondSysInfo, leapSecSysInfo)) {
            LOC_LOGe("convertLeapSecondSystemInfoToPB failed");
            free(leapSecSysInfo);
            return 1;
        }
    } else {
        LOC_LOGe("mutable_leapsecondsysinfo failed");
        return 1;
    }
    return 0;
}

int LocationApiPbMsgConv::convertGnssMeasNotifToPB(
        const GnssMeasurementsNotification &gnssMeasNotif,
        PBGnssMeasurementsNotification *pbGnssMeasNotif) const {
    if (nullptr == pbGnssMeasNotif) {
        LOC_LOGe("pbGnssMeasNotif is NULL!, return");
        return 1;
    }

    // repeated PBGnssMeasurementsData measurements = 1; Max array len - GNSS_MEASUREMENTS_MAX
    uint32_t count = gnssMeasNotif.count;
    LOC_LOGd("LocApiPB: gnssMeasNotif - MeasNotif count:%d", count);
    for (int i=0; i < count; i++) {
        PBGnssMeasurementsData* gnssMeasData = pbGnssMeasNotif->add_measurements();
        if (nullptr != gnssMeasData) {
            if (convertGnssMeasDataToPB(gnssMeasNotif.measurements[i], gnssMeasData)) {
                LOC_LOGe("convertGnssMeasDataToPB failed");
                free(gnssMeasData);
                return 1;
            }
        } else {
            LOC_LOGe("add_measurements failed");
            return 1;
        }
    }

    // PBGnssMeasurementsClock clock = 2;
    PBGnssMeasurementsClock* gnssMeasClock = pbGnssMeasNotif->mutable_clock();
    if (nullptr != gnssMeasClock) {
        if (convertGnssMeasClockToPB(gnssMeasNotif.clock, gnssMeasClock)) {
            LOC_LOGe("convertGnssMeasClockToPB failed");
            free(gnssMeasClock);
            return 1;
        }
    } else {
        LOC_LOGe("mutable_clock failed");
        return 1;
    }

    return 0;
}

int LocationApiPbMsgConv::convertGnssDataNotifToPB(const GnssDataNotification &gnssDataNotif,
        PBGnssDataNotification *pbGnssDataNotif) const {
    if (nullptr == pbGnssDataNotif) {
        LOC_LOGe("pbGnssDataNotif is NULL!, return");
        return 1;
    }
    // uint32 numberSignalTypes = 1
    pbGnssDataNotif->set_numbersignaltypes((uint32_t)GNSS_LOC_MAX_NUMBER_OF_SIGNAL_TYPES);

    int i=0;
    // bitwise OR of GnssDataMask. Max array len - GNSS_LOC_MAX_NUMBER_OF_SIGNAL_TYPES
    // repeated uint64  gnssDataMask = 2;
    // repeated double     jammerInd = 3;
    // repeated double     agc = 4;
    for (i = 0; i < GNSS_LOC_MAX_NUMBER_OF_SIGNAL_TYPES; i++) {
        LocApiPb_LOGd("LocApiPB: gnssDataNotif - jammerInd: %lf, agc: %lf, gnssDataMask: %" PRIu64,
                gnssDataNotif.jammerInd[i], gnssDataNotif.agc[i], gnssDataNotif.gnssDataMask[i]);
        pbGnssDataNotif->add_gnssdatamask(gnssDataNotif.gnssDataMask[i]);
        pbGnssDataNotif->add_jammerind(gnssDataNotif.jammerInd[i]);
        pbGnssDataNotif->add_agc(gnssDataNotif.agc[i]);
    }
    return 0;
}

int LocationApiPbMsgConv::convertLocAPINmeaSerializedPayloadToPB(
        const LocAPINmeaSerializedPayload &locAPINmeaSerPload,
        PBLocAPINmeaSerializedPayload *pbLocAPINmeaSerPload) const {
    if (nullptr == pbLocAPINmeaSerPload) {
        LOC_LOGe("pbLocAPINmeaSerPload is NULL!, return");
        return 1;
    }
    // uint64 timestamp = 1;
    pbLocAPINmeaSerPload->set_timestamp(locAPINmeaSerPload.timestamp);
    // string nmea = 2;
    pbLocAPINmeaSerPload->set_nmea(locAPINmeaSerPload.nmea);

    LocApiPb_LOGv("LocApiPB: locAPINmeaSerPload - Timestamp: %" PRIu64,
            locAPINmeaSerPload.timestamp);
    return 0;
}

int LocationApiPbMsgConv::convertGnssSvNotifToPB(const GnssSvNotification &gnssSvNotif,
        PBLocApiGnssSvNotification *pbGnssSvNotif) const {
    if (nullptr == pbGnssSvNotif) {
        LOC_LOGe("pbGnssSvNotif is NULL!, return");
        return 1;
    }

    // bool gnssSignalTypeMaskValid = 1;
    pbGnssSvNotif->set_gnsssignaltypemaskvalid(gnssSvNotif.gnssSignalTypeMaskValid);

    // repeated PBLocApiGnssSv gnssSvs = 2; (max - GNSS_SV_MAX)
    uint32_t count = gnssSvNotif.count;
    LOC_LOGd("LocApiPB: gnssSvNotif - SvNotif count:%d", count);
    for (int i=0; i < count; i++) {
        PBLocApiGnssSv* gnssSv = pbGnssSvNotif->add_gnsssvs();
        if (nullptr != gnssSv) {
            if (convertGnssSvToPB(gnssSvNotif.gnssSvs[i], gnssSv)) {
                LOC_LOGe("convertGnssSvToPB failed");
                free(gnssSv);
                return 1;
            }
        } else {
            LOC_LOGe("add_gnsssvs failed");
            return 1;
        }
    }
    return 0;
}

int LocationApiPbMsgConv::convertLeverArmParamsToPB(const LeverArmParams &leverArmParams,
        PBLIALeverArmParams *pbLeverArmParams) const {
    if (nullptr == pbLeverArmParams) {
        LOC_LOGe("pbLeverArmParams is NULL!, return");
        return 1;
    }
    // float forwardOffsetMeters = 1;
    pbLeverArmParams->set_forwardoffsetmeters(leverArmParams.forwardOffsetMeters);

    // float sidewaysOffsetMeters = 2;
    pbLeverArmParams->set_sidewaysoffsetmeters(leverArmParams.sidewaysOffsetMeters);

    // float upOffsetMeters = 3;
    pbLeverArmParams->set_upoffsetmeters(leverArmParams.upOffsetMeters);

    LOC_LOGd("LocApiPB: leverArmParams: Offset - Fwd: %f, Side: %f, Up: %f",
            leverArmParams.forwardOffsetMeters, leverArmParams.sidewaysOffsetMeters,
            leverArmParams.upOffsetMeters);
    return 0;
}

int LocationApiPbMsgConv::convertLeapSecondSystemInfoToPB(
        const LeapSecondSystemInfo &leapSecSysInfo,
        PBLeapSecondSystemInfo *pbLeapSecSysInfo) const {
    if (nullptr == pbLeapSecSysInfo) {
        LOC_LOGe("pbLeapSecSysInfo is NULL!, return");
        return 1;
    }

    // uint32  leapSecondInfoMask = 1; - bitwise OR of PBLeapSecondSysInfoMask
    pbLeapSecSysInfo->set_leapsecondinfomask(
            getPBMaskForLeapSecondSysInfoMask(leapSecSysInfo.leapSecondInfoMask));

    // uint32  leapSecondCurrent = 2;
    pbLeapSecSysInfo->set_leapsecondcurrent(leapSecSysInfo.leapSecondCurrent);

    // PBLeapSecondChangeInfo  leapSecondChangeInfo = 3;
    PBLeapSecondChangeInfo* leapSecChngInfo = pbLeapSecSysInfo->mutable_leapsecondchangeinfo();
    if (nullptr != leapSecChngInfo) {
        if (convertLeapSecChgInfoToPB(leapSecSysInfo.leapSecondChangeInfo, leapSecChngInfo)) {
            LOC_LOGe("convertLeapSecChgInfoToPB failed");
            free(leapSecChngInfo);
            return 1;
        }
    } else {
        LOC_LOGe("mutable_leapsecondchangeinfo failed");
        return 1;
    }

    LOC_LOGd("LocApiPB: leapSecSysInfo - LeapSecondInfoMask:%x, LeapSecCurr: %u",
            leapSecSysInfo.leapSecondInfoMask, leapSecSysInfo.leapSecondCurrent);
    return 0;
}

int LocationApiPbMsgConv::convertLeapSecChgInfoToPB(const LeapSecondChangeInfo &leapSecChngInfo,
        PBLeapSecondChangeInfo *pbLeapSecChngInfo) const {
    if (nullptr == pbLeapSecChngInfo) {
        LOC_LOGe("pbLeapSecChngInfo is NULL!, return");
        return 1;
    }
    // PBLocApiGnssSystemTimeStructType gpsTimestampLsChange = 1;
    PBLocApiGnssSystemTimeStructType* pLeapSecChngInfo =
            pbLeapSecChngInfo->mutable_gpstimestamplschange();
    if (nullptr != pLeapSecChngInfo) {
        if (convertGnssSystemTimeStructTypeToPB(leapSecChngInfo.gpsTimestampLsChange,
                pLeapSecChngInfo)) {
            LOC_LOGe("convertGnssSystemTimeStructTypeToPB failed");
            free(pLeapSecChngInfo);
            return 1;
        }
    } else {
        LOC_LOGe("mutable_gpstimestamplschange failed");
        return 1;
    }

    // uint32 leapSecondsBeforeChange = 2;
    pbLeapSecChngInfo->set_leapsecondsbeforechange(leapSecChngInfo.leapSecondsBeforeChange);

    // uint32 leapSecondsAfterChange = 3;
    pbLeapSecChngInfo->set_leapsecondsafterchange(leapSecChngInfo.leapSecondsAfterChange);

    LocApiPb_LOGd("LocApiPB: leapSecChngInfo - LeapSecs Before: %u, After:%u",
            leapSecChngInfo.leapSecondsBeforeChange, leapSecChngInfo.leapSecondsAfterChange);
    return 0;
}

int LocationApiPbMsgConv::convertGnssSystemTimeStructTypeToPB(
        const GnssSystemTimeStructType &gnssSysTimeStructType,
        PBLocApiGnssSystemTimeStructType *pbGnssSysTimeStructType) const {
    if (nullptr == pbGnssSysTimeStructType) {
        LOC_LOGe("pbGnssSysTimeStructType is NULL!, return");
        return 1;
    }
    // uint32 validityMask = 1; - PBLocApiGnssSystemTimeStructTypeFlags
    pbGnssSysTimeStructType->set_validitymask(
            getPBMaskForGnssSystemTimeStructTypeFlags(gnssSysTimeStructType.validityMask));

    // uint32 systemWeek = 2;
    pbGnssSysTimeStructType->set_systemweek(gnssSysTimeStructType.systemWeek);

    // uint32 systemMsec = 3;
    pbGnssSysTimeStructType->set_systemmsec(gnssSysTimeStructType.systemMsec);

    // float systemClkTimeBiasMs = 4;
    pbGnssSysTimeStructType->set_systemclktimebiasms(gnssSysTimeStructType.systemClkTimeBias);

    // float systemClkTimeBiasUncMs = 5;
    pbGnssSysTimeStructType->set_systemclktimebiasuncms(gnssSysTimeStructType.systemClkTimeUncMs);

    // uint32 refFCount = 6;
    pbGnssSysTimeStructType->set_reffcount(gnssSysTimeStructType.refFCount);

    // uint32 numClockResets = 7;
    pbGnssSysTimeStructType->set_numclockresets(gnssSysTimeStructType.numClockResets);

    LocApiPb_LOGd("LocApiPB: gnssSysTimeStruct - ValidityMsk: %x, SysWeek: %u, SysMsec: %u, "\
            "SysClkTimeBias: %f", gnssSysTimeStructType.validityMask,
            gnssSysTimeStructType.systemWeek, gnssSysTimeStructType.systemMsec,
            gnssSysTimeStructType.systemClkTimeBias);
    LocApiPb_LOGd("LocApiPB: gnssSysTimeStruct - SysClkTimeUnc: %f, RefCnt: %u, NumClkReset:%u",
            gnssSysTimeStructType.systemClkTimeUncMs, gnssSysTimeStructType.refFCount,
            gnssSysTimeStructType.numClockResets);
    return 0;
}

int LocationApiPbMsgConv::convertGnssGloTimeStructTypeToPB(
        const GnssGloTimeStructType &gnssGloTime,
        PBLocApiGnssGloTimeStructType *pbgnssGloTime) const {
    if (nullptr == pbgnssGloTime) {
        LOC_LOGe("pbgnssGloTime is NULL!, return");
        return 1;
    }
    // uint32 validityMask = 1; - Bitwise OR of PBGnssGloTimeStructTypeFlags
    pbgnssGloTime->set_validitymask(
            getPBMaskForGnssGloTimeStructTypeFlags(gnssGloTime.validityMask));

    // uint32 gloFourYear = 2;
    pbgnssGloTime->set_glofouryear(gnssGloTime.gloFourYear);

    // uint32 gloDays = 3;
    pbgnssGloTime->set_glodays(gnssGloTime.gloDays);

    // uint32 gloMsec = 4;
    pbgnssGloTime->set_glomsec(gnssGloTime.gloMsec);

    // float gloClkTimeBias = 5;
    pbgnssGloTime->set_gloclktimebias(gnssGloTime.gloClkTimeBias);

    // float gloClkTimeUncMs = 6;
    pbgnssGloTime->set_gloclktimeuncms(gnssGloTime.gloClkTimeUncMs);

    // uint32  refFCount = 7;
    pbgnssGloTime->set_reffcount(gnssGloTime.refFCount);

    // uint32 numClockResets = 8;
    pbgnssGloTime->set_numclockresets(gnssGloTime.numClockResets);

    LOC_LOGd("LocApiPB: gnssGloTime - GloValidityMsk: %x, GloFourYear: %u, GloDays: %u, "\
            "GloMsec:%u", gnssGloTime.validityMask, gnssGloTime.gloFourYear, gnssGloTime.gloDays,
             gnssGloTime.gloMsec);
    LOC_LOGd("LocApiPB: gnssGloTime - GloClkTimeBias: %f, GloClkTimeUnc: %f, RefFCnt: %u, "\
            "NumClkReset:%u", gnssGloTime.gloClkTimeBias, gnssGloTime.gloClkTimeUncMs,
            gnssGloTime.refFCount, gnssGloTime.numClockResets);
    return 0;
}

int LocationApiPbMsgConv::convertGnssMeasDataToPB(const GnssMeasurementsData &gnssMeasData,
        PBGnssMeasurementsData *pbGnssMeasData) const {
    if (nullptr == pbGnssMeasData) {
        LOC_LOGe("pbGnssMeasData is NULL!, return");
        return 1;
    }
    // uint32 flags = 1; - bitwise OR of PBGnssMeasurementsDataFlagsMask
    pbGnssMeasData->set_flags(getPBMaskForGnssMeasurementsDataFlagsMask(gnssMeasData.flags));

    // int32 svId = 2;
    pbGnssMeasData->set_svid(gnssMeasData.svId);

    // Use Gnss_LocSvSystemEnumType instead of GnssSvType
    // PBLocApiGnss_LocSvSystemEnumType svType = 3;
    pbGnssMeasData->set_svtype(getPBGnssLocSvSysEnumFromGnssSvType(gnssMeasData.svType));

    // double timeOffsetNs = 4;
    pbGnssMeasData->set_timeoffsetns(gnssMeasData.timeOffsetNs);

    // uint32 stateMask = 5; - bitwise OR of PBGnssMeasurementsStateMask
    pbGnssMeasData->set_statemask(getPBMaskForGnssMeasurementsStateMask(gnssMeasData.stateMask));

    // int64 receivedSvTimeNs = 6;
    pbGnssMeasData->set_receivedsvtimens(gnssMeasData.receivedSvTimeNs);

    // int64 receivedSvTimeUncertaintyNs = 7;
    pbGnssMeasData->set_receivedsvtimeuncertaintyns(gnssMeasData.receivedSvTimeUncertaintyNs);

    // double carrierToNoiseDbHz = 8;
    pbGnssMeasData->set_carriertonoisedbhz(gnssMeasData.carrierToNoiseDbHz);

    // double pseudorangeRateMps = 9;
    pbGnssMeasData->set_pseudorangeratemps(gnssMeasData.pseudorangeRateMps);

    // double pseudorangeRateUncertaintyMps = 10;
    pbGnssMeasData->set_pseudorangerateuncertaintymps(gnssMeasData.pseudorangeRateUncertaintyMps);

    // uint32 adrStateMask = 11; - bitwise OR of PBGnssMeasurementsAdrStateMask
    pbGnssMeasData->set_adrstatemask(
            getPBMaskForGnssMeasurementsAdrStateMask(gnssMeasData.adrStateMask));

    // double adrMeters = 12;
    pbGnssMeasData->set_adrmeters(gnssMeasData.adrMeters);

    // double adrUncertaintyMeters = 13;
    pbGnssMeasData->set_adruncertaintymeters(gnssMeasData.adrUncertaintyMeters);

    // float carrierFrequencyHz = 14;
    pbGnssMeasData->set_carrierfrequencyhz(gnssMeasData.carrierFrequencyHz);

    // int64 carrierCycles = 15;
    pbGnssMeasData->set_carriercycles(gnssMeasData.carrierCycles);

    // double carrierPhase = 16;
    pbGnssMeasData->set_carrierphase(gnssMeasData.carrierPhase);

    // double carrierPhaseUncertainty = 17;
    pbGnssMeasData->set_carrierphaseuncertainty(gnssMeasData.carrierPhaseUncertainty);

    // PBGnssMeasurementsMultipathIndicator multipathIndicator = 18;
    pbGnssMeasData->set_multipathindicator(
            getPBEnumForGnssMeasMultiPathIndic(gnssMeasData.multipathIndicator));

    // double signalToNoiseRatioDb = 19;
    pbGnssMeasData->set_signaltonoiseratiodb(gnssMeasData.signalToNoiseRatioDb);

    // double agcLevelDb = 20;
    pbGnssMeasData->set_agcleveldb(gnssMeasData.agcLevelDb);

    // double basebandCarrierToNoiseDbHz = 21;
    pbGnssMeasData->set_basebandcarriertonoisedbhz(gnssMeasData.basebandCarrierToNoiseDbHz);

    // uint32 gnssSignalType = 22; - bitwise OR of  PBGnssSignalTypeMask
    pbGnssMeasData->set_gnsssignaltype(
            getPBMaskForGnssSignalTypeMask(gnssMeasData.gnssSignalType));

    // double fullInterSignalBiasNs = 23;
    pbGnssMeasData->set_fullintersignalbiasns(gnssMeasData.fullInterSignalBiasNs);

    // double fullInterSignalBiasUncertaintyNs = 24;
    pbGnssMeasData->set_fullintersignalbiasuncertaintyns(
            gnssMeasData.fullInterSignalBiasUncertaintyNs);

    // uint32 cycleSlipCount = 25;
    pbGnssMeasData->set_cycleslipcount(gnssMeasData.cycleSlipCount);

    LOC_LOGd("LocApiPB: gnssMeasData - GnssMeasDataFlags:%x, Svid:%d, SvType:%d, StateMsk:%x, "\
            "RcvSvTime:%"  PRIu64", RcvSvTimeUnc:%" PRIu64", CNoDb:%lf", gnssMeasData.flags,
            gnssMeasData.svId, gnssMeasData.svType, gnssMeasData.stateMask,
            gnssMeasData.receivedSvTimeNs, gnssMeasData.receivedSvTimeUncertaintyNs,
            gnssMeasData.carrierToNoiseDbHz);

    LocApiPb_LOGd("LocApiPB: gnssMeasData - TimeOffset:%lf, PseuRngRt:%lf, PseuRngRtUnc:%lf,"\
            "AdrStateMask:%x, AdrMeters:%lf, AdrUncMeters:%lf, CarierFreq:%f, CarierCyc:%" PRIu64,
            gnssMeasData.timeOffsetNs, gnssMeasData.pseudorangeRateMps,
            gnssMeasData.pseudorangeRateUncertaintyMps, gnssMeasData.adrStateMask,
            gnssMeasData.adrMeters, gnssMeasData.adrUncertaintyMeters,
            gnssMeasData.carrierFrequencyHz, gnssMeasData.carrierCycles);

    LocApiPb_LOGd("LocApiPB: gnssMeasData - CarierPhase:%lf, CarierPhaseUnc:%lf, MultiPathInd:%d"\
            "CNoRatio:%lf, AgcLevel:%lf, BasebandCno:%lf", gnssMeasData.carrierPhase,
            gnssMeasData.carrierPhaseUncertainty, gnssMeasData.multipathIndicator,
            gnssMeasData.signalToNoiseRatioDb, gnssMeasData.agcLevelDb,
            gnssMeasData.basebandCarrierToNoiseDbHz);

    LocApiPb_LOGd("LocApiPB: gnssMeasData - GnssSignalType:%x, InterSigBiasNs:%lf, "\
            "InterSigBiasUncNs:%lf, cycleSlipCount:%d",
            gnssMeasData.gnssSignalType, gnssMeasData.fullInterSignalBiasNs,
            gnssMeasData.fullInterSignalBiasUncertaintyNs, gnssMeasData.cycleSlipCount);

    return 0;
}

int LocationApiPbMsgConv::convertGnssMeasClockToPB(const GnssMeasurementsClock &gnssMeasClock,
        PBGnssMeasurementsClock *pbGnssMeasClock) const {
    if (nullptr == pbGnssMeasClock) {
        LOC_LOGe("pbGnssMeasClock is NULL!, return");
        return 1;
    }
    // uint32 flags = 1; - bitwise OR of PBGnssMeasurementsClockFlagsMask
    pbGnssMeasClock->set_flags(getPBMaskForGnssMeasurementsClockFlagsMask(gnssMeasClock.flags));
    // int32 leapSecond = 2;
    pbGnssMeasClock->set_leapsecond(gnssMeasClock.leapSecond);
    // int64 timeNs = 3;
    pbGnssMeasClock->set_timens(gnssMeasClock.timeNs);
    // double timeUncertaintyNs = 4;
    pbGnssMeasClock->set_timeuncertaintyns(gnssMeasClock.timeUncertaintyNs);
    // int64 fullBiasNs = 5;
    pbGnssMeasClock->set_fullbiasns(gnssMeasClock.fullBiasNs);
    // double biasNs = 6;
    pbGnssMeasClock->set_biasns(gnssMeasClock.biasNs);
    // double biasUncertaintyNs = 7;
    pbGnssMeasClock->set_biasuncertaintyns(gnssMeasClock.biasUncertaintyNs);
    // double driftNsps = 8;
    pbGnssMeasClock->set_driftnsps(gnssMeasClock.driftNsps);
    // double driftUncertaintyNsps = 9;
    pbGnssMeasClock->set_driftuncertaintynsps(gnssMeasClock.driftUncertaintyNsps);
    // uint32 hwClockDiscontinuityCount= 10;
    pbGnssMeasClock->set_hwclockdiscontinuitycount(gnssMeasClock.hwClockDiscontinuityCount);

    LOC_LOGd("LocApiPB: gnssMeasClock - GnssMeasClockFlags:%x, leapSecond:%u, TimeNs:%" PRIu64\
        "TimeUnc:%lf FullBiasNs:%" PRIu64" BiasNs:%lf, BiasUncNs:%lf, DriftNs:%lf, DriftUncNs:%lf"
        "HwDiscCnt:%u",
        gnssMeasClock.flags, gnssMeasClock.leapSecond, gnssMeasClock.timeNs,
        gnssMeasClock.timeUncertaintyNs, gnssMeasClock.fullBiasNs, gnssMeasClock.biasNs,
        gnssMeasClock.biasUncertaintyNs, gnssMeasClock.driftNsps,
        gnssMeasClock.driftUncertaintyNsps, gnssMeasClock.hwClockDiscontinuityCount);
    return 0;
}

int LocationApiPbMsgConv::convertGnssSvToPB(const GnssSv &gnssSv,
        PBLocApiGnssSv *pbGnssSv) const {
    if (nullptr == pbGnssSv) {
        LOC_LOGe("pbGnssSv is NULL!, return");
        return 1;
    }
    // uint32 svId = 1;
    pbGnssSv->set_svid(gnssSv.svId);

    // PBLocApiGnss_LocSvSystemEnumType type = 2; - Use Gnss_LocSvSystemEnumType
    // instead of GnssSvType
    pbGnssSv->set_type(getPBGnssLocSvSysEnumFromGnssSvType(gnssSv.type));

    // float cN0Dbhz = 3;
    pbGnssSv->set_cn0dbhz(gnssSv.cN0Dbhz);

    // float elevation = 4;
    pbGnssSv->set_elevation(gnssSv.elevation);

    // float azimuth = 5;
    pbGnssSv->set_azimuth(gnssSv.azimuth);

    // uint32 gnssSvOptionsMask = 6; - Bitwise OR of PBLocApiGnssSvOptionsMask
    pbGnssSv->set_gnsssvoptionsmask(getPBMaskForGnssSvOptionsMask(gnssSv.gnssSvOptionsMask));

    // float carrierFrequencyHz = 7;
    pbGnssSv->set_carrierfrequencyhz(gnssSv.carrierFrequencyHz);

    // uint32 gnssSignalTypeMask = 8;  - Bitwise OR of PBGnssSignalTypeMask
    pbGnssSv->set_gnsssignaltypemask(getPBMaskForGnssSignalTypeMask(gnssSv.gnssSignalTypeMask));

    // double basebandCarrierToNoiseDbHz = 9;
    pbGnssSv->set_basebandcarriertonoisedbhz(gnssSv.basebandCarrierToNoiseDbHz);

    // uint32 gloFrequency = 10;
    pbGnssSv->set_glofrequency(gnssSv.gloFrequency);

    LocApiPb_LOGd("LocApiPB: gnssSv - SvId:%d, SvType:%d, CNo:%f, Elev:%f, Azi:%f, SvOptMask:%x, "\
            "CarrierFreq:%f, SignalTypeMask:%x, BseBandCno:%lf gloFrequency:%d",
            gnssSv.svId, gnssSv.type, gnssSv.cN0Dbhz, gnssSv.elevation,
            gnssSv.azimuth, gnssSv.gnssSvOptionsMask,
            gnssSv.carrierFrequencyHz, gnssSv.gnssSignalTypeMask,
            gnssSv.basebandCarrierToNoiseDbHz, gnssSv.gloFrequency);
    return 0;
}

int LocationApiPbMsgConv::convertGnssLocSvUsedInPosToPB(
        const GnssLocationSvUsedInPosition &gnssLocSvUsedInPos,
        PBGnssLocationSvUsedInPosition *pbGnssLocSvUsedInPos) const {
    if (nullptr == pbGnssLocSvUsedInPos) {
        LOC_LOGe("pbGnssLocSvUsedInPos is NULL!, return");
        return 1;
    }
    // uint64 gpsSvUsedIdsMask = 1;
    pbGnssLocSvUsedInPos->set_gpssvusedidsmask(gnssLocSvUsedInPos.gpsSvUsedIdsMask);

    // uint64 gloSvUsedIdsMask = 2;
    pbGnssLocSvUsedInPos->set_glosvusedidsmask(gnssLocSvUsedInPos.gloSvUsedIdsMask);

    // uint64 galSvUsedIdsMask = 3;
    pbGnssLocSvUsedInPos->set_galsvusedidsmask(gnssLocSvUsedInPos.galSvUsedIdsMask);

    // uint64 bdsSvUsedIdsMask = 4;
    pbGnssLocSvUsedInPos->set_bdssvusedidsmask(gnssLocSvUsedInPos.bdsSvUsedIdsMask);

    // uint64 qzssSvUsedIdsMask = 5;
    pbGnssLocSvUsedInPos->set_qzsssvusedidsmask(gnssLocSvUsedInPos.qzssSvUsedIdsMask);

    // uint64 navicSvUsedIdsMask = 6;
    pbGnssLocSvUsedInPos->set_navicsvusedidsmask(gnssLocSvUsedInPos.navicSvUsedIdsMask);

    LOC_LOGd("LocApiPB: gnssLocSvUsedInPos - Gps:%" PRIu64", Glo:%" PRIu64", Gal:%" PRIu64\
            ", Bds:%" PRIu64", Qzss:%" PRIu64", Navic:%" PRIu64,
            gnssLocSvUsedInPos.gpsSvUsedIdsMask, gnssLocSvUsedInPos.gloSvUsedIdsMask,
            gnssLocSvUsedInPos.galSvUsedIdsMask, gnssLocSvUsedInPos.bdsSvUsedIdsMask,
            gnssLocSvUsedInPos.qzssSvUsedIdsMask, gnssLocSvUsedInPos.navicSvUsedIdsMask);
    return 0;
}

int LocationApiPbMsgConv::convertGnssSystemTimeToPB(const GnssSystemTime &gnssSysTime,
        PBLocApiGnssSystemTime *pbGnssSysTime) const {
    if (nullptr == pbGnssSysTime) {
        LOC_LOGe("pbGnssSysTime is NULL!, return");
        return 1;
    }
    // PBLocApiGnss_LocSvSystemEnumType gnssSystemTimeSrc = 1;
    pbGnssSysTime->set_gnsssystemtimesrc(
            getPBEnumForGnssLocSvSystem(gnssSysTime.gnssSystemTimeSrc));

    // PBLocApiSystemTimeStructUnion u = 2;
    PBLocApiSystemTimeStructUnion* sysTimeStructUnion = pbGnssSysTime->mutable_u();
    if (nullptr != sysTimeStructUnion) {
        if (convertSystemTimeStructUnionToPB(gnssSysTime.gnssSystemTimeSrc, gnssSysTime.u,
                sysTimeStructUnion)) {
            LOC_LOGe("convertSystemTimeStructUnionToPB failed");
            free(sysTimeStructUnion);
            return 1;
        }
    } else {
        LOC_LOGe("mutable_u failed");
        return 1;
    }

    LocApiPb_LOGd("LocApiPB: gnssSysTime - SysTimeSrc:%d", gnssSysTime.gnssSystemTimeSrc);
    return 0;
}

int LocationApiPbMsgConv::convertGnssLocationPositionDynamicsToPB(
        const GnssLocationPositionDynamics &gnssLocPosDyn,
        const GnssLocationPositionDynamicsExt &gnssLocPosDynExt,
        PBLocApiGnssLocationPositionDynamics *pbGnssLocPosDyn) const {
    if (nullptr == pbGnssLocPosDyn) {
        LOC_LOGe("pbGnssLocPosDyn is NULL!, return");
        return 1;
    }
    // uint32  bodyFrameDataMask = 1; - Bitwise OR of PBLocApiGnssLocationPosDataMask
    pbGnssLocPosDyn->set_bodyframedatamask(
            getPBMaskForGnssLocationPosDataMask(
                gnssLocPosDyn.bodyFrameDataMask,
                gnssLocPosDynExt.bodyFrameDataMask));

    // float           longAccel = 2;
    // float           latAccel = 3;
    // float           vertAccel = 4;
    // float           longAccelUnc = 5;
    // float           latAccelUnc = 6;
    // float           vertAccelUnc = 7;
    pbGnssLocPosDyn->set_longaccel(gnssLocPosDyn.longAccel);
    pbGnssLocPosDyn->set_lataccel(gnssLocPosDyn.latAccel);
    pbGnssLocPosDyn->set_vertaccel(gnssLocPosDyn.vertAccel);
    pbGnssLocPosDyn->set_longaccelunc(gnssLocPosDyn.longAccelUnc);
    pbGnssLocPosDyn->set_lataccelunc(gnssLocPosDyn.latAccelUnc);
    pbGnssLocPosDyn->set_vertaccelunc(gnssLocPosDyn.vertAccelUnc);

    // float           pitch = 8;
    // float           pitchUnc = 9;
    // float           pitchRate = 10;
    // float           pitchRateUnc = 11;
    // float           roll = 12;
    // float           rollUnc = 13;
    // float           rollRate = 14;
    // float           rollRateUnc = 15;
    // float           yaw = 16;
    // float           yawUnc = 17;
    // float           yawRate = 18;
    // float           yawRateUnc = 19;
    pbGnssLocPosDyn->set_pitch(gnssLocPosDyn.pitch);
    pbGnssLocPosDyn->set_pitchunc(gnssLocPosDyn.pitchUnc);
    pbGnssLocPosDyn->set_pitchrate(gnssLocPosDynExt.pitchRate);
    pbGnssLocPosDyn->set_pitchrateunc(gnssLocPosDynExt.pitchRateUnc);
    pbGnssLocPosDyn->set_roll(gnssLocPosDynExt.roll);
    pbGnssLocPosDyn->set_rollunc(gnssLocPosDynExt.rollUnc);
    pbGnssLocPosDyn->set_rollrate(gnssLocPosDynExt.rollRate);
    pbGnssLocPosDyn->set_rollrateunc(gnssLocPosDynExt.rollRateUnc);
    pbGnssLocPosDyn->set_yaw(gnssLocPosDynExt.yaw);
    pbGnssLocPosDyn->set_yawunc(gnssLocPosDynExt.yawUnc);
    pbGnssLocPosDyn->set_yawrate(gnssLocPosDyn.yawRate);
    pbGnssLocPosDyn->set_yawrateunc(gnssLocPosDyn.yawRateUnc);

    LOC_LOGd("LocApiPB: gnssLocPosDyn - Mask:%x, MaskExt:%x, Accel-Long:%f Lat:%f Vert:%f",
            gnssLocPosDyn.bodyFrameDataMask, gnssLocPosDynExt.bodyFrameDataMask,
            gnssLocPosDyn.longAccel, gnssLocPosDyn.latAccel, gnssLocPosDyn.vertAccel);
    LocApiPb_LOGd("LocApiPB: Pitch:%f, PitchRate:%f, Roll:%f, RollRate:%f, Yaw:%f, YawRate:%f",
            gnssLocPosDyn.pitch, gnssLocPosDynExt.pitchRate, gnssLocPosDynExt.roll,
            gnssLocPosDynExt.rollRate, gnssLocPosDynExt.yaw, gnssLocPosDyn.yawRate);
    return 0;
}

int LocationApiPbMsgConv::convertLLAInfoToPB(const LLAInfo &llaInfo,
        PBLLAInfo *pbLlaInfo) const {
    if (nullptr == pbLlaInfo) {
        LOC_LOGe("pbLlaInfo is NULL!, return");
        return 1;
    }
    // double latitude = 1;
    pbLlaInfo->set_latitude(llaInfo.latitude);
    // double longitude = 2;
    pbLlaInfo->set_longitude(llaInfo.longitude);
    // float altitude = 3;
    pbLlaInfo->set_altitude(llaInfo.altitude);
    LOC_LOGd("LocApiPB: llaInfo - Lat:%lf, Lon:%lf, Alt:%f", llaInfo.latitude,
            llaInfo.longitude, llaInfo.altitude);
    return 0;
}

int LocationApiPbMsgConv::convertGnssMeasUsageInfoToPB(const GnssMeasUsageInfo &gnssMeasUsageInfo,
        PBGnssMeasUsageInfo *pbGnssMeasUsageInfo) const {
    if (nullptr == pbGnssMeasUsageInfo) {
        LOC_LOGe("pbGnssMeasUsageInfo is NULL!, return");
        return 1;
    }
    // PBLocApiGnss_LocSvSystemEnumType gnssConstellation = 1;
    pbGnssMeasUsageInfo->set_gnssconstellation(
            getPBEnumForGnssLocSvSystem(gnssMeasUsageInfo.gnssConstellation));

    // uint32 gnssSvId = 2;
    pbGnssMeasUsageInfo->set_gnsssvid(gnssMeasUsageInfo.gnssSvId);

    // uint32 gnssSignalType = 3; - bitwise OR of PBGnssSignalTypeMask
    pbGnssMeasUsageInfo->set_gnsssignaltype(
            getPBMaskForGnssSignalTypeMask(gnssMeasUsageInfo.gnssSignalType));

    LocApiPb_LOGd("LocApiPB: gnssMeasUsageInfo - Constl:%d, SvId:%d, SignalTypeMask:%x",
            gnssMeasUsageInfo.gnssConstellation, gnssMeasUsageInfo.gnssSvId,
            gnssMeasUsageInfo.gnssSignalType);
    return 0;
}

int LocationApiPbMsgConv::convertSystemTimeStructUnionToPB(
        const Gnss_LocSvSystemEnumType &gnssLocSvSysEnumType,
        const SystemTimeStructUnion &sysTimeStructUnion,
        PBLocApiSystemTimeStructUnion *pbSysTimeStructUnion) const {
    uint32_t retVal = 0;
    if (nullptr == pbSysTimeStructUnion) {
        LOC_LOGe("pbSysTimeStructUnion is NULL!, return");
        return 1;
    }

    switch (gnssLocSvSysEnumType) {
        case GNSS_LOC_SV_SYSTEM_GPS:
            {
                // PBLocApiGnssSystemTimeStructType gpsSystemTime = 1;
                PBLocApiGnssSystemTimeStructType* gpsSysTime =
                        pbSysTimeStructUnion->mutable_gpssystemtime();
                if (nullptr != gpsSysTime) {
                    if (convertGnssSystemTimeStructTypeToPB(sysTimeStructUnion.gpsSystemTime,
                            gpsSysTime)) {
                        LOC_LOGe("convertGnssSystemTimeStructTypeToPB failed");
                        free(gpsSysTime);
                        retVal = 1;
                    }
                } else {
                    LOC_LOGe("mutable_gpssystemtime failed");
                    retVal = 1;
                }
            }
            break;
        case GNSS_LOC_SV_SYSTEM_GALILEO:
            {
                // PBLocApiGnssSystemTimeStructType galSystemTime = 2;
                PBLocApiGnssSystemTimeStructType* galSysTime =
                        pbSysTimeStructUnion->mutable_galsystemtime();
                if (nullptr != galSysTime) {
                    if (convertGnssSystemTimeStructTypeToPB(sysTimeStructUnion.galSystemTime,
                            galSysTime)) {
                        LOC_LOGe("convertGnssSystemTimeStructTypeToPB failed");
                        free(galSysTime);
                        retVal = 1;
                    }
                } else {
                    LOC_LOGe("mutable_galsystemtime failed");
                    retVal = 1;
                }
            }
            break;
        case GNSS_LOC_SV_SYSTEM_BDS:
            {
                // PBLocApiGnssSystemTimeStructType bdsSystemTime = 3;
                PBLocApiGnssSystemTimeStructType* bdsSysTime =
                        pbSysTimeStructUnion->mutable_bdssystemtime();
                if (nullptr != bdsSysTime) {
                    if (convertGnssSystemTimeStructTypeToPB(sysTimeStructUnion.bdsSystemTime,
                            bdsSysTime)) {
                        LOC_LOGe("convertGnssSystemTimeStructTypeToPB failed");
                        free(bdsSysTime);
                        retVal = 1;
                    }
                } else {
                    LOC_LOGe("mutable_bdssystemtime failed");
                    retVal = 1;
                }
            }
            break;
        case GNSS_LOC_SV_SYSTEM_QZSS:
            {
                // PBLocApiGnssSystemTimeStructType qzssSystemTime = 4;
                PBLocApiGnssSystemTimeStructType* qzssSysTime =
                        pbSysTimeStructUnion->mutable_qzsssystemtime();
                if (nullptr != qzssSysTime) {
                    if (convertGnssSystemTimeStructTypeToPB(sysTimeStructUnion.qzssSystemTime,
                            qzssSysTime)) {
                        LOC_LOGe("convertGnssSystemTimeStructTypeToPB failed");
                        free(qzssSysTime);
                        retVal = 1;
                    }
                } else {
                    LOC_LOGe("mutable_qzsssystemtime failed");
                    retVal = 1;
                }
            }
            break;
        case GNSS_LOC_SV_SYSTEM_GLONASS:
            {
                // PBLocApiGnssGloTimeStructType gloSystemTime = 5;
                PBLocApiGnssGloTimeStructType* gloSysTime =
                        pbSysTimeStructUnion->mutable_glosystemtime();
                if (nullptr != gloSysTime) {
                    if (convertGnssGloTimeStructTypeToPB(sysTimeStructUnion.gloSystemTime,
                            gloSysTime)) {
                        LOC_LOGe("convertGnssGloTimeStructTypeToPB failed");
                        free(gloSysTime);
                        retVal = 1;
                    }
                } else {
                    LOC_LOGe("mutable_glosystemtime failed");
                    retVal = 1;
                }
            }
            break;
        case GNSS_LOC_SV_SYSTEM_NAVIC:
            {
                // PBLocApiGnssSystemTimeStructType navicSystemTime = 6;
                PBLocApiGnssSystemTimeStructType* navicSysTime =
                        pbSysTimeStructUnion->mutable_navicsystemtime();
                if (nullptr != navicSysTime) {
                    if (convertGnssSystemTimeStructTypeToPB(sysTimeStructUnion.navicSystemTime,
                            navicSysTime)) {
                        LOC_LOGe("convertGnssSystemTimeStructTypeToPB failed");
                        free(navicSysTime);
                        retVal = 1;
                    }
                } else {
                    LOC_LOGe("mutable_navicsystemtime failed");
                    retVal = 1;
                }
            }
            break;
        default:
            retVal = 0;
            break;
    }
    LocApiPb_LOGd("LocApiPB: sysTimeStructUnion - gnssLocSvSysEnumType:%d, return %d",
            gnssLocSvSysEnumType, retVal);
    return retVal;
}

int LocationApiPbMsgConv::convertGeofenceInfoToPB(const GeofenceInfo &gfInfo,
        PBGeofenceInfo *pbGfInfo) const {
    if (nullptr == pbGfInfo) {
        LOC_LOGe("pbGfInfo is NULL!, return");
        return 1;
    }
    // double latitude = 1;
    pbGfInfo->set_latitude(gfInfo.latitude);
    // double longitude = 2;
    pbGfInfo->set_longitude(gfInfo.longitude);
    // double radius = 3;
    pbGfInfo->set_radius(gfInfo.radius);

    LOC_LOGd("LocApiPB: gfInfo - Lat:%lf, Lon:%lf, Rad:%lf", gfInfo.latitude, gfInfo.longitude,
            gfInfo.radius);
    return 0;
}

int LocationApiPbMsgConv::convertGeofenceOptionToPB(const GeofenceOption &gfOpt,
        PBGeofenceOption *pbGfOpt) const {
    if (nullptr == pbGfOpt) {
        LOC_LOGe("pbGfOpt is NULL!, return");
        return 1;
    }
    // bitwise OR of PBGeofenceBreachTypeMask
    // uint32 breachTypeMask = 1;
    pbGfOpt->set_breachtypemask(getPBMaskForGeofenceBreachTypeMask(gfOpt.breachTypeMask));
    // uint32 responsiveness = 2
    pbGfOpt->set_responsiveness(gfOpt.responsiveness);
    // uint32 dwellTime = 3;
    pbGfOpt->set_dwelltime(gfOpt.dwellTime);

    LOC_LOGd("LocApiPB: gfOpt - BreachTypeMask:%x Resp:%u, DwellTime:%u",
            gfOpt.breachTypeMask, gfOpt.responsiveness, gfOpt.dwellTime);
    return 0;
}

// **** helper function  to convert from protobuf struct to normal struct.
int LocationApiPbMsgConv::pbConvertToGnssConfigRobustLocation(
        const PBGnssConfigRobustLocation &pbGnssCfgRobLoc,
        GnssConfigRobustLocation &gnssCfgRobLoc) const {
    // uint32 validMask = 1; - bitwise OR of PBGnssConfigRobustLocationValidMask
    gnssCfgRobLoc.validMask = (GnssConfigRobustLocationValidMask)
            getGnssConfigRobustLocationValidMaskFromPB(pbGnssCfgRobLoc.validmask());

    // bool enabled = 2;
    gnssCfgRobLoc.enabled = pbGnssCfgRobLoc.enabled();

    // bool enabledForE911 = 3;
    gnssCfgRobLoc.enabledForE911 = pbGnssCfgRobLoc.enabledfore911();

    // PBGnssConfigRobustLocationVersion version = 4;
    // >> uint32 major = 1;
    // >> uint32 minor = 2;
    gnssCfgRobLoc.version.major = pbGnssCfgRobLoc.version().major();
    gnssCfgRobLoc.version.minor = pbGnssCfgRobLoc.version().minor();

    LOC_LOGd("LocApiPB: pbGnssCfgRobLoc - CfgRobustLocMask:%x, Enabled:%d, EnabForE911:%d, "\
           "MajorVer:%d, MinorVer:%d", gnssCfgRobLoc.validMask, gnssCfgRobLoc.enabled,
           gnssCfgRobLoc.enabledForE911, gnssCfgRobLoc.version.major, gnssCfgRobLoc.version.minor);
    return 0;
}

int LocationApiPbMsgConv::pbConvertToCollectiveResPayload(
        const PBCollectiveResPayload &pbClctResPayload,
        CollectiveResPayload &clctResPayload) const {
    // repeated PBGeofenceResponse resp = 1;
    uint32_t count = pbClctResPayload.resp_size();
    clctResPayload.count = count;
    for (int i=0; i < count; i++) {
        clctResPayload.resp[i].clientId = pbClctResPayload.resp(i).clientid();
        clctResPayload.resp[i].error = getEnumForPBLocationError(pbClctResPayload.resp(i).error());
        LocApiPb_LOGv("LocApiPB: pbClctResPayload clientId:%u, error:%d",
                clctResPayload.resp[i].clientId, clctResPayload.resp[i].error);
    }
    LocApiPb_LOGd("LocApiPB: pbClctResPayload count:%d", clctResPayload.count);
    return 0;
}

int LocationApiPbMsgConv::pbConvertToLocation(const PBLocation &pbLoc, Location &loc) const {
    loc.size = sizeof(Location);

    // uint32 flags = 1; - Bitwise OR of PBLocationFlagsMask
    loc.flags = getLocationFlagsMaskFromPB(pbLoc.flags());

    // uint64 timestamp = 2;
    loc.timestamp = pbLoc.timestamp();

    // double latitude = 3;
    loc.latitude = pbLoc.latitude();

    // double longitude = 4;
    loc.longitude = pbLoc.longitude();

    // double altitude = 5;
    loc.altitude = pbLoc.altitude();

    // float speed = 6;
    loc.speed = pbLoc.speed();

    // float bearing = 7;
    loc.bearing = pbLoc.bearing();

    // float horizontalAccuracy = 8;
    loc.accuracy = pbLoc.horizontalaccuracy();

    // float verticalAccuracy = 9;
    loc.verticalAccuracy = pbLoc.verticalaccuracy();

    // float speedAccuracy = 10;
    loc.speedAccuracy = pbLoc.speedaccuracy();

    // float bearingAccuracy = 11;
    loc.bearingAccuracy = pbLoc.bearingaccuracy();

    // uint32 techMask = 12; - bitwise OR of PBLocationTechnologyMask
    loc.techMask = getLocationTechnologyMaskFromPB(pbLoc.techmask());

    LOC_LOGd("LocApiPB: pbLoc - Timestamp: %" PRIu64" Lat:%lf, Lon:%lf, Alt:%lf, TechMask:%x",
            loc.timestamp, loc.latitude, loc.longitude, loc.altitude, loc.techMask);
    LocApiPb_LOGd("LocApiPB: pbLoc - speed:%f, bearing:%f, HorzAcc:%f, VertAcc:%f, SpeedAcc:%f, "\
            "BearAcc:%f", loc.speed, loc.bearing, loc.accuracy, loc.verticalAccuracy,
            loc.speedAccuracy, loc.bearingAccuracy);
    return 0;
}

int LocationApiPbMsgConv::pbConvertToLocAPIBatchNotification(
        const PBLocAPIBatchNotification &pbLocBatchNotif,
        LocAPIBatchNotification &locBatchNotif) const {
    // PBBatchingStatus status = 1;
    locBatchNotif.status = getEnumForPBBatchingStatus(pbLocBatchNotif.status());

    // repeated PBLocation location = 2;
    uint32_t count = pbLocBatchNotif.location_size();
    locBatchNotif.count = count;
    for (int i=0; i < count; i++) {
        pbConvertToLocation(pbLocBatchNotif.location(i), locBatchNotif.location[i]);
    }

    LOC_LOGd("LocApiPB: pbLocBatchNotif - BatchStat: %d, Loc count:%u",
            locBatchNotif.status, count);
    return 0;
}

int LocationApiPbMsgConv::pbConvertToLocAPIGfBreachNotification(
            const PBLocAPIGeofenceBreachNotification &pbLocApiGfBreachNotif,
            LocAPIGeofenceBreachNotification &locApiGfBreachNotif) const {
    // uint64 timestamp = 1;
    locApiGfBreachNotif.timestamp = pbLocApiGfBreachNotif.timestamp();

    // uint32 breachtype = 2; - PBGeofenceBreachTypeMask
    locApiGfBreachNotif.type = getGfBreachTypeMaskFromPB(pbLocApiGfBreachNotif.breachtype());

    // PBLocation location = 3;
    pbConvertToLocation(pbLocApiGfBreachNotif.location(), locApiGfBreachNotif.location);

    // repeated uint32 id = 4;  --- uint32_t id[1];
    locApiGfBreachNotif.id[0] = pbLocApiGfBreachNotif.id(0);

    LOC_LOGd("LocApiPB: pbLocApiGfBreachNotif - BreachTypMask: %x, timestamp: %" PRIu64,
            locApiGfBreachNotif.type, locApiGfBreachNotif.timestamp);
    return 0;
}

int LocationApiPbMsgConv::pbConvertToGnssLocInfoNotif(
        const PBGnssLocationInfoNotification &pbGnssLocInfoNotif,
        GnssLocationInfoNotification &gnssLocInfoNotif) const {
    // PBLocation location = 1;
    pbConvertToLocation(pbGnssLocInfoNotif.location(), gnssLocInfoNotif.location);

    // uint64 flags = 2; -  bitwise OR of PBGnssLocationInfoFlagMask
    gnssLocInfoNotif.flags = getGnssLocationInfoFlagMaskFromPB(
            pbGnssLocInfoNotif.flags(), pbGnssLocInfoNotif.extflags());

    // float altitudeMeanSeaLevel = 3;
    gnssLocInfoNotif.altitudeMeanSeaLevel = pbGnssLocInfoNotif.altitudemeansealevel();

    // float pdop = 4;
    gnssLocInfoNotif.pdop = pbGnssLocInfoNotif.pdop();
    // float hdop = 5;
    gnssLocInfoNotif.hdop = pbGnssLocInfoNotif.hdop();
    // float vdop = 6;
    gnssLocInfoNotif.vdop = pbGnssLocInfoNotif.vdop();
    // float gdop = 7;
    gnssLocInfoNotif.gdop = pbGnssLocInfoNotif.gdop();
    // float tdop = 8;
    gnssLocInfoNotif.tdop = pbGnssLocInfoNotif.tdop();

    // float magneticDeviation = 9;
    gnssLocInfoNotif.magneticDeviation = pbGnssLocInfoNotif.magneticdeviation();

    // PBLocationReliability horReliability = 10;
    gnssLocInfoNotif.horReliability =
            getEnumForPBLocReliability(pbGnssLocInfoNotif.horreliability());
    // PBLocationReliability verReliability = 11;
    gnssLocInfoNotif.verReliability =
            getEnumForPBLocReliability(pbGnssLocInfoNotif.verreliability());

    // float horUncEllipseSemiMajor = 12;
    gnssLocInfoNotif.horUncEllipseSemiMajor = pbGnssLocInfoNotif.horuncellipsesemimajor();
    // float horUncEllipseSemiMinor = 13;
    gnssLocInfoNotif.horUncEllipseSemiMinor = pbGnssLocInfoNotif.horuncellipsesemiminor();
    // float horUncEllipseOrientAzimuth = 14;
    gnssLocInfoNotif.horUncEllipseOrientAzimuth = pbGnssLocInfoNotif.horuncellipseorientazimuth();

    // float northStdDeviation = 15;
    gnssLocInfoNotif.northStdDeviation = pbGnssLocInfoNotif.northstddeviation();
    // float eastStdDeviation = 16;
    gnssLocInfoNotif.eastStdDeviation = pbGnssLocInfoNotif.eaststddeviation();

    // float northVelocity = 17;
    gnssLocInfoNotif.northVelocity = pbGnssLocInfoNotif.northvelocity();
    // float eastVelocity = 18;
    gnssLocInfoNotif.eastVelocity = pbGnssLocInfoNotif.eastvelocity();
    // float upVelocity = 19;
    gnssLocInfoNotif.upVelocity = pbGnssLocInfoNotif.upvelocity();
    // float northVelocityStdDeviation = 20;
    gnssLocInfoNotif.northVelocityStdDeviation = pbGnssLocInfoNotif.northvelocitystddeviation();
    // float eastVelocityStdDeviation = 21;
    gnssLocInfoNotif.eastVelocityStdDeviation = pbGnssLocInfoNotif.eastvelocitystddeviation();
    // float upVelocityStdDeviation = 22;
    gnssLocInfoNotif.upVelocityStdDeviation = pbGnssLocInfoNotif.upvelocitystddeviation();

    // uint32 numSvUsedInPosition = 23;
    gnssLocInfoNotif.numSvUsedInPosition = pbGnssLocInfoNotif.numsvusedinposition();

    // PBGnssLocationSvUsedInPosition svUsedInPosition = 24;
    pbConvertToGnssLocationSvUsedInPosition(pbGnssLocInfoNotif.svusedinposition(),
            gnssLocInfoNotif.svUsedInPosition);

    // uint32 navSolutionMask = 25; - bitwise OR of PBGnssLocationNavSolutionMask
    gnssLocInfoNotif.navSolutionMask =
            getGnssLocationNavSolutionMaskFromPB(pbGnssLocInfoNotif.navsolutionmask());

    // PBLocApiGnssLocationPositionDynamics bodyFrameData = 26;
    // bodyFrameDataExt - Additional Body Frame Dynamics. Added to this too.
    pbConvertToGnssLocationPositionDynamics(pbGnssLocInfoNotif.bodyframedata(),
            gnssLocInfoNotif.bodyFrameData, gnssLocInfoNotif.bodyFrameDataExt);

    // PBLocApiGnssSystemTime gnssSystemTime = 27;
    pbConvertToGnssSystemTime(pbGnssLocInfoNotif.gnsssystemtime(),
            gnssLocInfoNotif.gnssSystemTime);

    // uint32 numOfMeasReceived = 28;
    gnssLocInfoNotif.numOfMeasReceived = min(pbGnssLocInfoNotif.numofmeasreceived(),
            (uint32_t)GNSS_SV_MAX);

    // repeated PBGnssMeasUsageInfo measUsageInfo = 29; Max array len - GNSS_SV_MAX
    for (uint32_t iter=0; iter < gnssLocInfoNotif.numOfMeasReceived; iter++) {
        pbConvertToGnssMeasUsageInfo(pbGnssLocInfoNotif.measusageinfo(iter),
                gnssLocInfoNotif.measUsageInfo[iter]);
    }

    // uint32 leapSeconds = 30;
    gnssLocInfoNotif.leapSeconds = pbGnssLocInfoNotif.leapseconds();

    // float timeUncMs = 31;
    gnssLocInfoNotif.timeUncMs = pbGnssLocInfoNotif.timeuncms();

    // uint32 calibrationConfidence = 32;
    gnssLocInfoNotif.calibrationConfidence = pbGnssLocInfoNotif.calibrationconfidence();

    // uint32 calibrationStatus = 33; -  PBLocApiDrCalibrationStatusMask
    gnssLocInfoNotif.calibrationStatus =
            getDrCalibrationStatusMaskFromPB(pbGnssLocInfoNotif.calibrationstatus());

    // PBLocApiOutputEngineType locOutputEngType = 34;
    gnssLocInfoNotif.locOutputEngType =
            getEnumForPBLocOutputEngineType(pbGnssLocInfoNotif.locoutputengtype());

    // uint32 locOutputEngMask = 35; - bitwise OR of PBLocApiPositioningEngineMask
    gnssLocInfoNotif.locOutputEngMask =
            getEnumForPBPositioningEngineMask(pbGnssLocInfoNotif.locoutputengmask());

    // float conformityIndex = 36;
    gnssLocInfoNotif.conformityIndex = pbGnssLocInfoNotif.conformityindex();

    // PBLLAInfo llaVRPBased = 37;
    pbConvertToLLAInfo(pbGnssLocInfoNotif.llavrpbased(), gnssLocInfoNotif.llaVRPBased);

    // repeated float enuVelocityVRPBased = 38; - Max array length 3
    for (int i=0; i < 3; i++) {
        gnssLocInfoNotif.enuVelocityVRPBased[i] = pbGnssLocInfoNotif.enuvelocityvrpbased(i);
        LocApiPb_LOGd("LocApiPB: enuVelocityVRPBased[%d]:%f", i,
                gnssLocInfoNotif.enuVelocityVRPBased[i]);
    }

    // uint32 drSolutionStatusMask = 39; - PBDrSolutionStatusMask
    gnssLocInfoNotif.drSolutionStatusMask =
            (DrSolutionStatusMask)getDrSolutionStatusMaskFromPB(
            pbGnssLocInfoNotif.drsolutionstatusmask());

    // bool altitudeAssumed = 41;
    gnssLocInfoNotif.altitudeAssumed= pbGnssLocInfoNotif.altitudeassumed();

    // bool sessionStatus = 42;
    gnssLocInfoNotif.sessionStatus = getLocSessionStatusFromPB(
            pbGnssLocInfoNotif.sessionstatus());

    LOC_LOGd("LocApiPB: pbGnssLocInfoNotif -GLocInfoFlgMask:%u, pdop:%f, hdop:%f, vdop:%f",
            gnssLocInfoNotif.flags, gnssLocInfoNotif.pdop, gnssLocInfoNotif.hdop,
            gnssLocInfoNotif.vdop);
    LOC_LOGd("HorReliab:%d, VerReliab:%d, HorUncElps-SemiMajor:%f SemiMinor:%f, NumSvUsedInPos:%u",
            gnssLocInfoNotif.horReliability, gnssLocInfoNotif.verReliability,
            gnssLocInfoNotif.horUncEllipseSemiMajor, gnssLocInfoNotif.horUncEllipseSemiMinor,
            gnssLocInfoNotif.numSvUsedInPosition);
    LOC_LOGd("NavSolMask:%x, NumMeasRcvd:%u, LocOpEngType:%d, PosEngMask:%x",
            gnssLocInfoNotif.navSolutionMask, gnssLocInfoNotif.numOfMeasReceived,
            gnssLocInfoNotif.locOutputEngType, gnssLocInfoNotif.locOutputEngMask);
    return 0;
}

int LocationApiPbMsgConv::pbConvertToGnssSvNotif(const PBLocApiGnssSvNotification &pbGnssSvNotif,
            GnssSvNotification &gnssSvNotif) const {
    gnssSvNotif.size = sizeof(GnssSvNotification);

    // bool gnssSignalTypeMaskValid = 1;
    gnssSvNotif.gnssSignalTypeMaskValid = pbGnssSvNotif.gnsssignaltypemaskvalid();

    // information on a number of SVs (max - GNSS_SV_MAX)
    // repeated PBLocApiGnssSv gnssSvs = 2;
    gnssSvNotif.count = min(pbGnssSvNotif.gnsssvs_size(), (int)GNSS_SV_MAX);
    LOC_LOGd("LocApiPB: pbGnssSvNotif- num svs %d", gnssSvNotif.count);
    for (int i=0; i < gnssSvNotif.count; i++) {
        PBLocApiGnssSv pPbGnssSv = pbGnssSvNotif.gnsssvs(i);
        gnssSvNotif.gnssSvs[i].size = sizeof(GnssSv);
        // uint32 svId = 1;
        gnssSvNotif.gnssSvs[i].svId = pPbGnssSv.svid();

        // Use Gnss_LocSvSystemEnumType instead of GnssSvType
        // PBLocApiGnss_LocSvSystemEnumType type = 2;
        gnssSvNotif.gnssSvs[i].type =
                getGnssSvTypeFromPBGnssLocSvSystemEnumType(pPbGnssSv.type());

        // float cN0Dbhz = 3;
        gnssSvNotif.gnssSvs[i].cN0Dbhz = pPbGnssSv.cn0dbhz();

        // float elevation = 4;
        gnssSvNotif.gnssSvs[i].elevation = pPbGnssSv.elevation();

        // float azimuth = 5;
        gnssSvNotif.gnssSvs[i].azimuth = pPbGnssSv.azimuth();

        // Bitwise OR of PBLocApiGnssSvOptionsMask
        // uint32 gnssSvOptionsMask = 6;
        gnssSvNotif.gnssSvs[i].gnssSvOptionsMask =
                getGnssSvOptionsMaskFromPB(pPbGnssSv.gnsssvoptionsmask());

        // float carrierFrequencyHz = 7;
        gnssSvNotif.gnssSvs[i].carrierFrequencyHz = pPbGnssSv.carrierfrequencyhz();

        // uint32 gnssSignalTypeMask = 8; - PBGnssSignalTypeMask
        gnssSvNotif.gnssSvs[i].gnssSignalTypeMask =
                getGnssSignalTypeMaskFromPB(pPbGnssSv.gnsssignaltypemask());

        // double basebandCarrierToNoiseDbHz = 9;
        gnssSvNotif.gnssSvs[i].basebandCarrierToNoiseDbHz = pPbGnssSv.basebandcarriertonoisedbhz();

        // uint32 gloFrequency = 10;
        gnssSvNotif.gnssSvs[i].gloFrequency = pPbGnssSv.glofrequency();
        LocApiPb_LOGd("LocApiPB: gnssSv[%d] - SvId:%d, CNo:%f, SvOptMask:%x, SignalTypeMask:%x, "\
                "BseBandCno:%lf, gloFrequency:%d",
                i, gnssSvNotif.gnssSvs[i].svId, gnssSvNotif.gnssSvs[i].cN0Dbhz,
                gnssSvNotif.gnssSvs[i].gnssSvOptionsMask,
                gnssSvNotif.gnssSvs[i].gnssSignalTypeMask,
                gnssSvNotif.gnssSvs[i].basebandCarrierToNoiseDbHz,
                gnssSvNotif.gnssSvs[i].gloFrequency);
    }
    return 0;
}

int LocationApiPbMsgConv::pbConvertToLocAPINmeaSerializedPayload(
            const PBLocAPINmeaSerializedPayload &pbLocApiNmeaSerPayload,
            LocAPINmeaSerializedPayload &locApiNmeaSerPayload) const {
    // uint64 timestamp = 1;
    locApiNmeaSerPayload.timestamp = pbLocApiNmeaSerPayload.timestamp();
    // string nmea = 2;
    locApiNmeaSerPayload.nmea = pbLocApiNmeaSerPayload.nmea();
    LocApiPb_LOGv("LocApiPB: pbLocApiNmeaSerPayload %" PRIu64, locApiNmeaSerPayload.timestamp);
    return 0;
}

int LocationApiPbMsgConv::pbConvertToGnssDataNotification(
        const PBGnssDataNotification &pbGnssDataNotif,
        GnssDataNotification &gnssDataNotif) const {
    gnssDataNotif.size = sizeof(GnssDataNotification);

    // uint32 numberSignalTypes = 1
    uint32_t i = 0;
    uint32_t arrMaxLen = min(pbGnssDataNotif.numbersignaltypes(),
            (uint32_t)GNSS_LOC_MAX_NUMBER_OF_SIGNAL_TYPES);

    // repeated uint32  gnssDataMask = 2;
    // repeated double     jammerInd = 3;
    // repeated double     agc = 4;
    for (i = 0; i < arrMaxLen; i++) {
        gnssDataNotif.gnssDataMask[i] = getGnssDataMaskFromPB(pbGnssDataNotif.gnssdatamask(i));
        gnssDataNotif.jammerInd[i] = pbGnssDataNotif.jammerind(i);
        gnssDataNotif.agc[i] = pbGnssDataNotif.agc(i);
        LocApiPb_LOGd("LocApiPB: pbGnssDataNotif - jammerInd:%lf, agc:%lf, gnssDataMask: %" PRIu64,
                gnssDataNotif.jammerInd[i], gnssDataNotif.agc[i], gnssDataNotif.gnssDataMask[i]);
    }
    return 0;
}

int LocationApiPbMsgConv::pbConvertToGnssMeasNotification(
        const PBGnssMeasurementsNotification &pbGnssMeasNotif,
        GnssMeasurementsNotification &gnssMeasNotif) const {
    gnssMeasNotif.size = sizeof(GnssMeasurementsNotification);
    // repeated PBGnssMeasurementsData measurements = 1; - Max array len - GNSS_MEASUREMENTS_MAX
    uint32_t count = pbGnssMeasNotif.measurements_size();
    gnssMeasNotif.count = count;
    for (int i=0; i < count; i++) {
        pbConvertToGnssMeasurementsData(pbGnssMeasNotif.measurements(i),
                gnssMeasNotif.measurements[i]);
    }

    // PBGnssMeasurementsClock clock = 2;
    pbConvertToGnssMeasurementsClock(pbGnssMeasNotif.clock(), gnssMeasNotif.clock);

    LOC_LOGd("LocApiPB: pbGnssMeasNotif - count:%u", count);

    return 0;
}

int LocationApiPbMsgConv::pbConvertToLocationSystemInfo(const PBLocationSystemInfo &pbLocSysInfo,
            LocationSystemInfo &locSysInfo) const {
    // bitwise OR of PBLocationSystemInfoMask
    // uint32 systemInfoMask = 1;
    locSysInfo.systemInfoMask = getLocationSystemInfoMaskFromPB(pbLocSysInfo.systeminfomask());

    // PBLeapSecondSystemInfo   leapSecondSysInfo = 2;
    pbConvertToLeapSecSysInfo(pbLocSysInfo.leapsecondsysinfo(), locSysInfo.leapSecondSysInfo);

    LOC_LOGd("LocApiPB: pbLocSysInfo - sysInfoMask: %x", locSysInfo.systemInfoMask);
    return 0;
}

int LocationApiPbMsgConv::pbConvertToLocationOptions(const PBLocationOptions &pbLocOpt,
        LocationOptions &locOpt) const {
    locOpt.size = sizeof(LocationOptions);
    // uint32 minInterval = 1;
    locOpt.minInterval = pbLocOpt.mininterval();

    // uint32 minDistance = 2;
    locOpt.minDistance = pbLocOpt.mindistance();

    // PBGnssSuplMode mode = 3;
    locOpt.mode = getEnumForPBGnssSuplMode(pbLocOpt.mode());

    // bitwise OR of PBLocReqEngineTypeMask
    // uint32 locReqEngTypeMask = 4;
    locOpt.locReqEngTypeMask = (LocReqEngineTypeMask)getLocReqEngineTypeMaskFromPB(
            pbLocOpt.locreqengtypemask());

    LocApiPb_LOGd("LocApiPB: pbLocOpt - MinInterval: %u, MinDistance:%u, GnssSuplMode:%d, "\
            "LocReqEngineTypeMask:%x", locOpt.minInterval, locOpt.minDistance, locOpt.mode,
            locOpt.locReqEngTypeMask);
    return 0;
}

int LocationApiPbMsgConv::pbConvertToGfAddReqPayload(
        const PBGeofencesAddedReqPayload &pbGfAddReqPload,
        GeofencesAddedReqPayload &gfAddReqPload) const {
    // repeated PBGeofencePayload gfPayload = 1;
    uint32_t gfCount = pbGfAddReqPload.gfpayload_size();
    gfAddReqPload.count = gfCount;
    LOC_LOGd("LocApiPB: pbGfAddReqPload- count %d", gfCount);
    for (int i=0; i < gfCount; i++) {
        PBGeofencePayload pbGfPayload = pbGfAddReqPload.gfpayload(i);
        // uint32 gfClientId = 1;
        gfAddReqPload.gfPayload[i].gfClientId = pbGfPayload.gfclientid();
        LocApiPb_LOGv("LocApiPB: gfPayload[%d] gfClientId - %u", i,
                gfAddReqPload.gfPayload[i].gfClientId);
        // PBGeofenceOption gfOption = 2;
        pbConvertToGeofenceOption(pbGfPayload.gfoption(), gfAddReqPload.gfPayload[i].gfOption);
        // PBGeofenceInfo gfInfo = 3;
        pbConvertToGeofenceInfo(pbGfPayload.gfinfo(), gfAddReqPload.gfPayload[i].gfInfo);
    }
    return 0;
}

int LocationApiPbMsgConv::pbConvertToGfReqClientIdPayload(
        const PBGeofencesReqClientIdPayload &pbGfReqClientIdPload,
        GeofencesReqClientIdPayload &gfReqClientIdPload) const {
    // repeated uint32 gfIds = 1;
    gfReqClientIdPload.count = pbGfReqClientIdPload.gfids_size();
    uint32_t gfCount = gfReqClientIdPload.count;
    LOC_LOGd("LocApiPB: gfReqClientIdPload count %u", gfCount);
    for (uint32_t i=0; i < gfCount; i++) {
        gfReqClientIdPload.gfIds[i] = pbGfReqClientIdPload.gfids(i);
        LocApiPb_LOGv("LocApiPB: gfReqClientIdPload gfIds[%u]: %u", i,
                gfReqClientIdPload.gfIds[i]);
    }
    return 0;
}

int LocationApiPbMsgConv::pbConvertToGnssSvTypeConfig(const PBGnssSvTypeConfig &pbGnssSvTypeConfig,
        GnssSvTypeConfig &gnssSvTypeConfig) const {
    gnssSvTypeConfig.size = sizeof(GnssSvTypeConfig);
    // Enabled Constellations - bitwise OR of PBGnssSvTypesMask
    // uint64 enabledSvTypesMask = 1;
    gnssSvTypeConfig.enabledSvTypesMask = getGnssSvTypesMaskFromPB(
            pbGnssSvTypeConfig.enabledsvtypesmask());

    // Disabled Constellations - bitwise OR of PBGnssSvTypesMask
    // uint64 blacklistedSvTypesMask = 2;
    gnssSvTypeConfig.blacklistedSvTypesMask = getGnssSvTypesMaskFromPB(
            pbGnssSvTypeConfig.blacklistedsvtypesmask());

    LOC_LOGd("LocApiPB: pbGnssSvTypesMask - Enable Sv types: %" PRIu64 \
            " Blacklist Sv Types: %" PRIu64, gnssSvTypeConfig.enabledSvTypesMask,
            gnssSvTypeConfig.blacklistedSvTypesMask);
    return 0;
}

int LocationApiPbMsgConv::pbConvertToGnssSvIdConfig(const PBGnssSvIdConfig &pbGnssSvIdConfig,
        GnssSvIdConfig &gnssSvIdConfig) const {
    gnssSvIdConfig.size = sizeof(GnssSvIdConfig);
    // uint64 gloBlacklistSvMask = 1;
    gnssSvIdConfig.gloBlacklistSvMask = pbGnssSvIdConfig.globlacklistsvmask();

    // uint64 bdsBlacklistSvMask = 2;
    gnssSvIdConfig.bdsBlacklistSvMask = pbGnssSvIdConfig.bdsblacklistsvmask();

    // uint64 qzssBlacklistSvMask = 3;
    gnssSvIdConfig.qzssBlacklistSvMask = pbGnssSvIdConfig.qzssblacklistsvmask();

    // uint64 galBlacklistSvMask = 4;
    gnssSvIdConfig.galBlacklistSvMask = pbGnssSvIdConfig.galblacklistsvmask();

    // uint64 sbasBlacklistSvMask = 5;
    gnssSvIdConfig.sbasBlacklistSvMask = pbGnssSvIdConfig.sbasblacklistsvmask();

    // uint64_t navicBlacklistSvMask = 6;
    gnssSvIdConfig.navicBlacklistSvMask = pbGnssSvIdConfig.navicblacklistsvmask();

    LOC_LOGd("LocApiPB: BlackListSvMask - Glo: %" PRIu64 ",Bds: %" PRIu64 ",Qzss: %" PRIu64 \
            ",Gal: %" PRIu64 ",Sbas: %" PRIu64",Nav: %" PRIu64, gnssSvIdConfig.gloBlacklistSvMask,
            gnssSvIdConfig.bdsBlacklistSvMask, gnssSvIdConfig.qzssBlacklistSvMask,
            gnssSvIdConfig.galBlacklistSvMask, gnssSvIdConfig.sbasBlacklistSvMask,
            gnssSvIdConfig.navicBlacklistSvMask);
    return 0;
}

int LocationApiPbMsgConv::pbConvertToGnssAidingData(const PBAidingData &pbGnssAidData,
        GnssAidingData &gnssAidData) const {
    // bool deleteAll  = 1;
    gnssAidData.deleteAll = pbGnssAidData.deleteall();

    // uint32 gnssAidingDataSvMask = 2; - bitwise OR of PBLocApiGnssAidingDataSvMask
    gnssAidData.sv.svMask = getGnssAidingDataSvMaskFromPB(pbGnssAidData.gnssaidingdatasvmask());

    // uint32 dreAidingDataMask = 3;- PBDrEngineAidingDataMask
    gnssAidData.dreAidingDataMask = getDrEngineAidingDataMaskFromPB(
            pbGnssAidData.dreaidingdatamask());

    // Masks from - PBLocApiPositioningEngineMask
    // uint32 posEngineMask = 4;
    gnssAidData.posEngineMask = getEnumForPBPositioningEngineMask(pbGnssAidData.posenginemask());

    LOC_LOGd("LocApiPB: pbGnssAidData deleteAll:%d, svMask: %x, PosEngMask:%x",
            gnssAidData.deleteAll, gnssAidData.sv.svMask, gnssAidData.posEngineMask);
    return 0;
}

int LocationApiPbMsgConv::pbConvertToLeverArmConfigInfo(
        const PBLIALeverArmConfigInfo &pbLeverArmCfgInfo,
        LeverArmConfigInfo &leverArmCfgInfo) const {
    // uint32 leverArmValidMask = 1;
    // PBLIALeverArmTypeMask
    leverArmCfgInfo.leverArmValidMask = getLeverArmTypeMaskFromPB(
            pbLeverArmCfgInfo.leverarmvalidmask());

    // PBLIALeverArmParams   gnssToVRP = 2;
    pbConvertToLeverArmParams(pbLeverArmCfgInfo.gnsstovrp(), leverArmCfgInfo.gnssToVRP);

    // PBLIALeverArmParams   drImuToGnss = 3;
    pbConvertToLeverArmParams(pbLeverArmCfgInfo.drimutognss(), leverArmCfgInfo.drImuToGnss);

    // PBLIALeverArmParams   veppImuToGnss = 4;
    pbConvertToLeverArmParams(pbLeverArmCfgInfo.veppimutognss(), leverArmCfgInfo.veppImuToGnss);

    LOC_LOGd("LocApiPB: pbLeverArmCfgInfo - leverArmValidMask: %x",
            leverArmCfgInfo.leverArmValidMask);
    return 0;
}

int LocationApiPbMsgConv::pbConvertToBodyToSensorMountParams(
        const PBLIABodyToSensorMountParams &pbBody2SensorMntParam,
        BodyToSensorMountParams &body2SensorMntParam) const {
    // float rollOffset = 1;
    body2SensorMntParam.rollOffset = pbBody2SensorMntParam.rolloffset();
    // float yawOffset = 2;
    body2SensorMntParam.yawOffset = pbBody2SensorMntParam.yawoffset();
    // float pitchOffset = 3;
    body2SensorMntParam.pitchOffset = pbBody2SensorMntParam.pitchoffset();
    // float offsetUnc = 4;
    body2SensorMntParam.offsetUnc = pbBody2SensorMntParam.offsetunc();

    LOC_LOGd("LocApiPB: pbBody2SensorMntParam: Offset - Roll:%f, Yaw:%f, Pitch:%f, Unc:%f",
            body2SensorMntParam.rollOffset, body2SensorMntParam.yawOffset,
            body2SensorMntParam.pitchOffset, body2SensorMntParam.offsetUnc);
    return 0;
}

int LocationApiPbMsgConv::pbConvertToDeadReckoningEngineConfig(
        const PBDeadReckoningEngineConfig &pbDrEngConfig,
        DeadReckoningEngineConfig &drEngConfig) const {
    // uint64    validMask = 1; - Bitwise OR mask of PBDeadReckoningEngineConfigValidMask
    drEngConfig.validMask = getDeadReckoningEngineConfigValidMaskFromPB(pbDrEngConfig.validmask());

    // PBLIABodyToSensorMountParams bodyToSensorMountParams = 2;
    pbConvertToBodyToSensorMountParams(pbDrEngConfig.bodytosensormountparams(),
            drEngConfig.bodyToSensorMountParams);

    // float vehicleSpeedScaleFactor = 3;
    drEngConfig.vehicleSpeedScaleFactor = pbDrEngConfig.vehiclespeedscalefactor();
    // float vehicleSpeedScaleFactorUnc = 4;
    drEngConfig.vehicleSpeedScaleFactorUnc = pbDrEngConfig.vehiclespeedscalefactorunc();
    // float gyroScaleFactor = 5;
    drEngConfig.gyroScaleFactor = pbDrEngConfig.gyroscalefactor();
    // float gyroScaleFactorUnc = 6;
    drEngConfig.gyroScaleFactorUnc = pbDrEngConfig.gyroscalefactorunc();

    LOC_LOGd("LocApiPB: pbDrEngConfig - DrEngConfigValidMask:%"  PRIu64", VehSpeedScale: %f"
            " VehSpeedScaleUnc: %f, GyroScale: %f, GyroScaleUnc: %f", drEngConfig.validMask,
            drEngConfig.vehicleSpeedScaleFactor, drEngConfig.vehicleSpeedScaleFactorUnc,
            drEngConfig.gyroScaleFactor, drEngConfig.gyroScaleFactorUnc);
    return 0;
}

int LocationApiPbMsgConv::pbConvertToLeverArmParams(const PBLIALeverArmParams &pbLeverArmParams,
            LeverArmParams &leverArmParams) const {
    // float forwardOffsetMeters = 1;
    leverArmParams.forwardOffsetMeters = pbLeverArmParams.forwardoffsetmeters();
    // float sidewaysOffsetMeters = 2;
    leverArmParams.sidewaysOffsetMeters = pbLeverArmParams.sidewaysoffsetmeters();
    // float upOffsetMeters = 3;
    leverArmParams.upOffsetMeters = pbLeverArmParams.upoffsetmeters();

    LOC_LOGd("LocApiPB: pbLeverArmParams: Offset - Fwd: %f, Side: %f, Up: %f",
            leverArmParams.forwardOffsetMeters, leverArmParams.sidewaysOffsetMeters,
            leverArmParams.upOffsetMeters);
    return 0;
}

int LocationApiPbMsgConv::pbConvertToLeapSecSysInfo(const PBLeapSecondSystemInfo &pbLeapSecSysInfo,
        LeapSecondSystemInfo &leapSecSysInfo) const {
    // bitwise OR of PBLeapSecondSysInfoMask
    // uint32  leapSecondInfoMask = 1;
    leapSecSysInfo.leapSecondInfoMask = getLeapSecSysInfoMaskFromPB(
            pbLeapSecSysInfo.leapsecondinfomask());

    // uint32  leapSecondCurrent = 2;
    leapSecSysInfo.leapSecondCurrent = pbLeapSecSysInfo.leapsecondcurrent();

    // PBLeapSecondChangeInfo  leapSecondChangeInfo = 3;
    pbConvertToLeapSecChgInfo(pbLeapSecSysInfo.leapsecondchangeinfo(),
            leapSecSysInfo.leapSecondChangeInfo);

    LOC_LOGd("LocApiPB: pbLeapSecSysInfo - LeapSecondInfoMask:%x, LeapSecCurr: %u",
            leapSecSysInfo.leapSecondInfoMask, leapSecSysInfo.leapSecondCurrent);
    return 0;
}

int LocationApiPbMsgConv::pbConvertToLeapSecChgInfo(const PBLeapSecondChangeInfo &pbLeapSecChgInfo,
            LeapSecondChangeInfo &leapSecChgInfo) const {
    // PBLocApiGnssSystemTimeStructType gpsTimestampLsChange = 1;
    pbConvertToGnssSystemTimeStructType(pbLeapSecChgInfo.gpstimestamplschange(),
            leapSecChgInfo.gpsTimestampLsChange);

    // uint32 leapSecondsBeforeChange = 2;
    leapSecChgInfo.leapSecondsBeforeChange = pbLeapSecChgInfo.leapsecondsbeforechange();

    // uint32 leapSecondsAfterChange = 3;
    leapSecChgInfo.leapSecondsAfterChange = pbLeapSecChgInfo.leapsecondsafterchange();

    LOC_LOGd("LocApiPB: pbLeapSecChgInfo - LeapSecs Before: %u, After:%u",
            leapSecChgInfo.leapSecondsBeforeChange, leapSecChgInfo.leapSecondsAfterChange);
    return 0;
}

int LocationApiPbMsgConv::pbConvertToGnssSystemTimeStructType(
        const PBLocApiGnssSystemTimeStructType &pbGnssSysTimeStrct,
        GnssSystemTimeStructType &gnssSysTimeStrct) const {
    // Validity mask for below fields PBLocApiGnssSystemTimeStructTypeFlags
    // uint32 validityMask = 1;
    gnssSysTimeStrct.validityMask = getGnssSystemTimeStructTypeFlagsFromPB(
            pbGnssSysTimeStrct.validitymask());

    // uint32 systemWeek = 2;
    gnssSysTimeStrct.systemWeek = pbGnssSysTimeStrct.systemweek();

    // uint32 systemMsec = 3;
    gnssSysTimeStrct.systemMsec = pbGnssSysTimeStrct.systemmsec();

    // float systemClkTimeBiasMs = 4;
    gnssSysTimeStrct.systemClkTimeBias = pbGnssSysTimeStrct.systemclktimebiasms();

    // float systemClkTimeBiasUncMs = 5;
    gnssSysTimeStrct.systemClkTimeUncMs = pbGnssSysTimeStrct.systemclktimebiasuncms();

    // uint32 refFCount = 6;
    gnssSysTimeStrct.refFCount = pbGnssSysTimeStrct.reffcount();

    // uint32 numClockResets = 7;
    gnssSysTimeStrct.numClockResets = pbGnssSysTimeStrct.numclockresets();

    LOC_LOGd("LocApiPB: pbGnssSysTimeStrct - ValidityMsk: %x, SysWeek: %u, SysMsec: %u, "\
            "SysClkTimeBias: %f", gnssSysTimeStrct.validityMask, gnssSysTimeStrct.systemWeek,
            gnssSysTimeStrct.systemMsec, gnssSysTimeStrct.systemClkTimeBias);
    LocApiPb_LOGd("LocApiPB: pbGnssSysTimeStrct - SysClkTimeUnc: %f, RefCnt: %u, NumClkReset:%u",
            gnssSysTimeStrct.systemClkTimeUncMs, gnssSysTimeStrct.refFCount,
            gnssSysTimeStrct.numClockResets);
    return 0;
}

// PBGnssMeasurementsData to GnssMeasurementsData
int LocationApiPbMsgConv::pbConvertToGnssMeasurementsData(
        const PBGnssMeasurementsData &pbGnssMeasData,
        GnssMeasurementsData &gnssMeasData) const {
    gnssMeasData.size = sizeof(GnssMeasurementsData);
    // uint32 flags = 1; - bitwise OR of PBGnssMeasurementsDataFlagsMask
    gnssMeasData.flags = getGnssMeasurementsDataFlagsMaskFromPB(pbGnssMeasData.flags());

    // int32 svId = 2;
    gnssMeasData.svId = pbGnssMeasData.svid();

    // Use Gnss_LocSvSystemEnumType instead of GnssSvType
    // PBLocApiGnss_LocSvSystemEnumType svType = 3;
    gnssMeasData.svType = getGnssSvTypeFromPBGnssLocSvSystemEnumType(pbGnssMeasData.svtype());

    // double timeOffsetNs = 4;
    gnssMeasData.timeOffsetNs = pbGnssMeasData.timeoffsetns();

    // uint32 stateMask = 5; - bitwise OR of PBGnssMeasurementsStateMask
    gnssMeasData.stateMask = pbGnssMeasData.statemask();

    // int64 receivedSvTimeNs = 6;
    gnssMeasData.receivedSvTimeNs = pbGnssMeasData.receivedsvtimens();

    // int64 receivedSvTimeUncertaintyNs = 7;
    gnssMeasData.receivedSvTimeUncertaintyNs = pbGnssMeasData.receivedsvtimeuncertaintyns();

    // double carrierToNoiseDbHz = 8;
    gnssMeasData.carrierToNoiseDbHz = pbGnssMeasData.carriertonoisedbhz();

    // double pseudorangeRateMps = 9;
    gnssMeasData.pseudorangeRateMps = pbGnssMeasData.pseudorangeratemps();

    // double pseudorangeRateUncertaintyMps = 10;
    gnssMeasData.pseudorangeRateUncertaintyMps = pbGnssMeasData.pseudorangerateuncertaintymps();

    // uint32 adrStateMask = 11; - bitwise OR of PBGnssMeasurementsAdrStateMask
    gnssMeasData.adrStateMask = getGnssMeasurementsAdrStateMaskFromPB(
            pbGnssMeasData.adrstatemask());

    // double adrMeters = 12;
    gnssMeasData.adrMeters = pbGnssMeasData.adrmeters();

    // double adrUncertaintyMeters = 13;
    gnssMeasData.adrUncertaintyMeters = pbGnssMeasData.adruncertaintymeters();

    // float carrierFrequencyHz = 14;
    gnssMeasData.carrierFrequencyHz = pbGnssMeasData.carrierfrequencyhz();

    // int64 carrierCycles = 15;
    gnssMeasData.carrierCycles = pbGnssMeasData.carriercycles();

    // double carrierPhase = 16;
    gnssMeasData.carrierPhase = pbGnssMeasData.carrierphase();

    // double carrierPhaseUncertainty = 17;
    gnssMeasData.carrierPhaseUncertainty = pbGnssMeasData.carrierphaseuncertainty();

    // PBGnssMeasurementsMultipathIndicator multipathIndicator = 18;
    gnssMeasData.multipathIndicator = getEnumForPBGnssMeasMultipathIndic(
            pbGnssMeasData.multipathindicator());

    // double signalToNoiseRatioDb = 19;
    gnssMeasData.signalToNoiseRatioDb = pbGnssMeasData.signaltonoiseratiodb();

    // double agcLevelDb = 20;
    gnssMeasData.agcLevelDb = pbGnssMeasData.agcleveldb();

    // double basebandCarrierToNoiseDbHz = 21;
    gnssMeasData.basebandCarrierToNoiseDbHz = pbGnssMeasData.basebandcarriertonoisedbhz();

    // uint32 gnssSignalType = 22; - bitwise OR of  PBGnssSignalTypeMask
    gnssMeasData.gnssSignalType =  getGnssSignalTypeMaskFromPB(pbGnssMeasData.gnsssignaltype());

    // double fullInterSignalBiasNs = 23;
    gnssMeasData.fullInterSignalBiasNs = pbGnssMeasData.fullintersignalbiasns();

    // double fullInterSignalBiasUncertaintyNs = 24;
    gnssMeasData.fullInterSignalBiasUncertaintyNs =
            pbGnssMeasData.fullintersignalbiasuncertaintyns();

    // uint32 cycleSlipCount = 25;
    gnssMeasData.cycleSlipCount = pbGnssMeasData.cycleslipcount();

    LOC_LOGd("LocApiPB: pbGnssMeasData - GnssMeasDataFlags:%x, Svid:%d, SvType:%d, StateMsk:%x, "\
            "RcvSvTime:%"  PRIu64", RcvSvTimeUnc:%" PRIu64", CNoDb:%lf", gnssMeasData.flags,
            gnssMeasData.svId, gnssMeasData.svType, gnssMeasData.stateMask,
            gnssMeasData.receivedSvTimeNs, gnssMeasData.receivedSvTimeUncertaintyNs,
            gnssMeasData.carrierToNoiseDbHz);

    LocApiPb_LOGd("LocApiPB: pbGnssMeasData - TimeOffset:%lf, PseuRngRt:%lf, PseuRngRtUnc:%lf,"\
            "AdrStateMask:%x, AdrMeters:%lf, AdrUncMeters:%lf, CarierFreq:%f, CarierCyc:%" PRIu64,
            gnssMeasData.timeOffsetNs, gnssMeasData.pseudorangeRateMps,
            gnssMeasData.pseudorangeRateUncertaintyMps, gnssMeasData.adrStateMask,
            gnssMeasData.adrMeters, gnssMeasData.adrUncertaintyMeters,
            gnssMeasData.carrierFrequencyHz, gnssMeasData.carrierCycles);

    LocApiPb_LOGd("LocApiPB: pbGnssMeasData- CarierPhase:%lf, CarierPhaseUnc:%lf, MultiPathInd:%d"\
            "CNoRatio:%lf, AgcLevel:%lf, BasebandCno:%lf", gnssMeasData.carrierPhase,
            gnssMeasData.carrierPhaseUncertainty, gnssMeasData.multipathIndicator,
            gnssMeasData.signalToNoiseRatioDb, gnssMeasData.agcLevelDb,
            gnssMeasData.basebandCarrierToNoiseDbHz);

    LocApiPb_LOGd("LocApiPB: gnssMeasData - GnssSignalType:%x, InterSigBiasNs:%lf, "\
            "InterSigBiasUncNs:%lf, cycleSlipCount:%d",
            gnssMeasData.gnssSignalType, gnssMeasData.fullInterSignalBiasNs,
            gnssMeasData.fullInterSignalBiasUncertaintyNs, gnssMeasData.cycleSlipCount);

    return 0;
}

int LocationApiPbMsgConv::pbConvertToGnssMeasurementsClock(
        const PBGnssMeasurementsClock &pbGnssMeasClock,
        GnssMeasurementsClock &gnssMeasClock) const {
    gnssMeasClock.size = sizeof(GnssMeasurementsClock);
    // uint32 flags = 1; - bitwise OR of PBGnssMeasurementsClockFlagsMask
    gnssMeasClock.flags = getGnssMeasurementsClockFlagsMaskFromPB(pbGnssMeasClock.flags());
    // int32 leapSecond = 2;
    gnssMeasClock.leapSecond = pbGnssMeasClock.leapsecond();
    // int64 timeNs = 3;
    gnssMeasClock.timeNs = pbGnssMeasClock.timens();
    // double timeUncertaintyNs = 4;
    gnssMeasClock.timeUncertaintyNs = pbGnssMeasClock.timeuncertaintyns();
    // int64 fullBiasNs = 5;
    gnssMeasClock.fullBiasNs = pbGnssMeasClock.fullbiasns();
    // double biasNs = 6;
    gnssMeasClock.biasNs = pbGnssMeasClock.biasns();
    // double biasUncertaintyNs = 7;
    gnssMeasClock.biasUncertaintyNs = pbGnssMeasClock.biasuncertaintyns();
    // double driftNsps = 8;
    gnssMeasClock.driftNsps = pbGnssMeasClock.driftnsps();
    // double driftUncertaintyNsps = 9;
    gnssMeasClock.driftUncertaintyNsps = pbGnssMeasClock.driftuncertaintynsps();
    // uint32 hwClockDiscontinuityCount= 10;
    gnssMeasClock.hwClockDiscontinuityCount = pbGnssMeasClock.hwclockdiscontinuitycount();

    LOC_LOGd("LocApiPB: pbGnssMeasClock - GnssMeasClockFlags:%x, leapSecond:%u, TimeNs:%" PRIu64\
        "TimeUnc:%lf FullBiasNs:%" PRIu64" BiasNs:%lf, BiasUncNs:%lf, DriftNs:%lf, DriftUncNs:%lf"
        "HwDiscCnt:%u",
        gnssMeasClock.flags, gnssMeasClock.leapSecond, gnssMeasClock.timeNs,
        gnssMeasClock.timeUncertaintyNs, gnssMeasClock.fullBiasNs, gnssMeasClock.biasNs,
        gnssMeasClock.biasUncertaintyNs, gnssMeasClock.driftNsps,
        gnssMeasClock.driftUncertaintyNsps, gnssMeasClock.hwClockDiscontinuityCount);

    return 0;
}

int LocationApiPbMsgConv::pbConvertToGnssLocationSvUsedInPosition(
        const PBGnssLocationSvUsedInPosition &pbGnssLocSvUsedInPos,
        GnssLocationSvUsedInPosition &gnssLocSvUsedInPos) const {
    // uint64 gpsSvUsedIdsMask = 1;
    gnssLocSvUsedInPos.gpsSvUsedIdsMask = pbGnssLocSvUsedInPos.gpssvusedidsmask();
    // uint64 gloSvUsedIdsMask = 2;
    gnssLocSvUsedInPos.gloSvUsedIdsMask = pbGnssLocSvUsedInPos.glosvusedidsmask();
    // uint64 galSvUsedIdsMask = 3;
    gnssLocSvUsedInPos.galSvUsedIdsMask = pbGnssLocSvUsedInPos.galsvusedidsmask();
    // uint64 bdsSvUsedIdsMask = 4;
    gnssLocSvUsedInPos.bdsSvUsedIdsMask = pbGnssLocSvUsedInPos.bdssvusedidsmask();
    // uint64 qzssSvUsedIdsMask = 5;
    gnssLocSvUsedInPos.qzssSvUsedIdsMask = pbGnssLocSvUsedInPos.qzsssvusedidsmask();
    // uint64 navicSvUsedIdsMask = 6;
    gnssLocSvUsedInPos.navicSvUsedIdsMask = pbGnssLocSvUsedInPos.navicsvusedidsmask();
    LOC_LOGd("LocApiPB: pbGnssLocSvUsedInPos - Gps:%" PRIu64", Glo:%" PRIu64", Gal:%" PRIu64\
            ", Bds:%" PRIu64", Qzss:%" PRIu64", Navic:%" PRIu64,
            gnssLocSvUsedInPos.gpsSvUsedIdsMask, gnssLocSvUsedInPos.gloSvUsedIdsMask,
            gnssLocSvUsedInPos.galSvUsedIdsMask, gnssLocSvUsedInPos.bdsSvUsedIdsMask,
            gnssLocSvUsedInPos.qzssSvUsedIdsMask, gnssLocSvUsedInPos.navicSvUsedIdsMask);
    return 0;
}

int LocationApiPbMsgConv::pbConvertToLLAInfo(const PBLLAInfo &pbLlaInfo, LLAInfo &llaInfo) const {
    // double latitude = 1;
    llaInfo.latitude = pbLlaInfo.latitude();

    // double longitude = 2;
    llaInfo.longitude = pbLlaInfo.longitude();

    // float altitude = 3;
    llaInfo.altitude = pbLlaInfo.altitude();

    LOC_LOGd("LocApiPB: pbLlaInfo - Lat:%lf, Lon:%lf, Alt:%f", llaInfo.latitude,
            llaInfo.longitude, llaInfo.altitude);
    return 0;
}

int LocationApiPbMsgConv::pbConvertToGnssLocationPositionDynamics(
        const PBLocApiGnssLocationPositionDynamics &pbGnssLocPosDyn,
        GnssLocationPositionDynamics &gnssLocPosDyn,
        GnssLocationPositionDynamicsExt &gnssLocPosDynExt) const {

    // uint32  bodyFrameDataMask = 1; - Bitwise OR of PBLocApiGnssLocationPosDataMask
    gnssLocPosDyn.bodyFrameDataMask =
            getGnssLocationPosDataMaskFromPB(pbGnssLocPosDyn.bodyframedatamask());
    gnssLocPosDynExt.bodyFrameDataMask =
            getGnssLocationPosDataMaskExtFromPB(pbGnssLocPosDyn.bodyframedatamask());

    // float           longAccel = 2;
    // float           latAccel = 3;
    // float           vertAccel = 4;
    // float           longAccelUnc = 5;
    // float           latAccelUnc = 6;
    // float           vertAccelUnc = 7;
    gnssLocPosDyn.longAccel = pbGnssLocPosDyn.longaccel();
    gnssLocPosDyn.latAccel = pbGnssLocPosDyn.lataccel();
    gnssLocPosDyn.vertAccel = pbGnssLocPosDyn.vertaccel();
    gnssLocPosDyn.longAccelUnc = pbGnssLocPosDyn.longaccelunc();
    gnssLocPosDyn.latAccelUnc = pbGnssLocPosDyn.lataccelunc();
    gnssLocPosDyn.vertAccelUnc = pbGnssLocPosDyn.vertaccelunc();

    // float           pitch = 8;
    // float           pitchUnc = 9;
    // float           pitchRate = 10;
    // float           pitchRateUnc = 11;
    gnssLocPosDyn.pitch = pbGnssLocPosDyn.pitch();
    gnssLocPosDyn.pitchUnc = pbGnssLocPosDyn.pitchunc();
    gnssLocPosDynExt.pitchRate = pbGnssLocPosDyn.pitchrate();
    gnssLocPosDynExt.pitchRateUnc = pbGnssLocPosDyn.pitchrateunc();

    // float           roll = 12;
    // float           rollUnc = 13;
    // float           rollRate = 14;
    // float           rollRateUnc = 15;
    gnssLocPosDynExt.roll = pbGnssLocPosDyn.roll();
    gnssLocPosDynExt.rollUnc = pbGnssLocPosDyn.rollunc();
    gnssLocPosDynExt.rollRate = pbGnssLocPosDyn.rollrate();
    gnssLocPosDynExt.rollRateUnc = pbGnssLocPosDyn.rollrateunc();

    // float           yaw = 16;
    // float           yawUnc = 17;
    // float           yawRate = 18;
    // float           yawRateUnc = 19;
    gnssLocPosDynExt.yaw = pbGnssLocPosDyn.yaw();
    gnssLocPosDynExt.yawUnc = pbGnssLocPosDyn.yawunc();
    gnssLocPosDyn.yawRate = pbGnssLocPosDyn.yawrate();
    gnssLocPosDyn.yawRateUnc = pbGnssLocPosDyn.yawrateunc();

    LOC_LOGd("LocApiPB: pbGnssLocPosDyn - Mask:%x, MaskExt:%x, Accel-Long:%f Lat:%f Vert:%f",
            gnssLocPosDyn.bodyFrameDataMask, gnssLocPosDynExt.bodyFrameDataMask,
            gnssLocPosDyn.longAccel, gnssLocPosDyn.latAccel, gnssLocPosDyn.vertAccel);
    LocApiPb_LOGd("LocApiPB: Pitch:%f, PitchRate:%f, Roll:%f, RollRate:%f, Yaw:%f, YawRate:%f",
            gnssLocPosDyn.pitch, gnssLocPosDynExt.pitchRate, gnssLocPosDynExt.roll,
            gnssLocPosDynExt.rollRate, gnssLocPosDynExt.yaw, gnssLocPosDyn.yawRate);
    return 0;
}

int LocationApiPbMsgConv::pbConvertToGnssSystemTime(const PBLocApiGnssSystemTime &pbGnssSysTime,
        GnssSystemTime &gnssSysTime) const {
    // PBLocApiGnss_LocSvSystemEnumType gnssSystemTimeSrc = 1;
    gnssSysTime.gnssSystemTimeSrc =
            getEnumForPBGnssLocSvSystem(pbGnssSysTime.gnsssystemtimesrc());
    // PBLocApiSystemTimeStructUnion u = 2;
    pbConvertToSystemTimeStructUnion(gnssSysTime.gnssSystemTimeSrc,
            pbGnssSysTime.u(), gnssSysTime.u);
    LOC_LOGd("LocApiPB: pbGnssSysTime - GnssLocSvSystem %d", gnssSysTime.gnssSystemTimeSrc);
    return 0;
}

int LocationApiPbMsgConv::pbConvertToGnssMeasUsageInfo(
        const PBGnssMeasUsageInfo &pbGnssMeasUsageInfo,
        GnssMeasUsageInfo &gnssMeasUsageInfo) const {
    // PBLocApiGnss_LocSvSystemEnumType gnssConstellation = 1;
    gnssMeasUsageInfo.gnssConstellation =
            getEnumForPBGnssLocSvSystem(pbGnssMeasUsageInfo.gnssconstellation());

    // uint32 gnssSvId = 2;
    gnssMeasUsageInfo.gnssSvId = pbGnssMeasUsageInfo.gnsssvid();

    // uint32 gnssSignalType = 3; - bitwise OR of PBGnssSignalTypeMask
    gnssMeasUsageInfo.gnssSignalType =
        getGnssSignalTypeMaskFromPB(pbGnssMeasUsageInfo.gnsssignaltype());

    LocApiPb_LOGd("LocApiPB: pbGnssMeasUsageInfo - Constl:%d, SvId:%d, SignalTypeMask:%x",
            gnssMeasUsageInfo.gnssConstellation, gnssMeasUsageInfo.gnssSvId,
            gnssMeasUsageInfo.gnssSignalType);
    return 0;
}

int LocationApiPbMsgConv::pbConvertToSystemTimeStructUnion(
        const Gnss_LocSvSystemEnumType &gnssLocSvSysEnumType,
        const PBLocApiSystemTimeStructUnion &pbSysTimeStructUnion,
        SystemTimeStructUnion &sysTimeStructUnion) const {
    LOC_LOGd("LocApiPB: pbSysTimeStructUnion - gnssLocSvSysEnumType:%d", gnssLocSvSysEnumType);
    switch (gnssLocSvSysEnumType) {
        case GNSS_LOC_SV_SYSTEM_GPS:
            // PBLocApiGnssSystemTimeStructType gpsSystemTime = 1;
            pbConvertToGnssSystemTimeStructType(pbSysTimeStructUnion.gpssystemtime(),
                    sysTimeStructUnion.gpsSystemTime);
            break;
        case GNSS_LOC_SV_SYSTEM_GALILEO:
            // PBLocApiGnssSystemTimeStructType galSystemTime = 2;
            pbConvertToGnssSystemTimeStructType(pbSysTimeStructUnion.galsystemtime(),
                    sysTimeStructUnion.galSystemTime);
            break;
        case GNSS_LOC_SV_SYSTEM_BDS:
            // PBLocApiGnssSystemTimeStructType bdsSystemTime = 3;
            pbConvertToGnssSystemTimeStructType(pbSysTimeStructUnion.bdssystemtime(),
                    sysTimeStructUnion.bdsSystemTime);
            break;
        case GNSS_LOC_SV_SYSTEM_QZSS:
            // PBLocApiGnssSystemTimeStructType qzssSystemTime = 4;
            pbConvertToGnssSystemTimeStructType(pbSysTimeStructUnion.qzsssystemtime(),
                    sysTimeStructUnion.qzssSystemTime);
            break;
        case GNSS_LOC_SV_SYSTEM_GLONASS:
            // PBLocApiGnssGloTimeStructType gloSystemTime = 5;
            pbConvertToGnssGloTimeStructType(pbSysTimeStructUnion.glosystemtime(),
                    sysTimeStructUnion.gloSystemTime);
            break;
        case GNSS_LOC_SV_SYSTEM_NAVIC:
            // PBLocApiGnssSystemTimeStructType navicSystemTime = 6;
            pbConvertToGnssSystemTimeStructType(pbSysTimeStructUnion.navicsystemtime(),
                    sysTimeStructUnion.navicSystemTime);
            break;
        default:
            break;
    }
    return 0;
}

int LocationApiPbMsgConv::pbConvertToGnssGloTimeStructType(
        const PBLocApiGnssGloTimeStructType &pbGnssGloTime,
        GnssGloTimeStructType &gnssGloTime) const {
    // uint32 validityMask = 1; - Bitwise OR of PBGnssGloTimeStructTypeFlags
    gnssGloTime.validityMask = getGnssGloTimeStructTypeFlagsFromPB(pbGnssGloTime.validitymask());

    // uint32 gloFourYear = 2;
    gnssGloTime.gloFourYear = pbGnssGloTime.glofouryear();

    // uint32 gloDays = 3;
    gnssGloTime.gloDays = pbGnssGloTime.glodays();

    // uint32 gloMsec = 4;
    gnssGloTime.gloMsec = pbGnssGloTime.glomsec();

    // float gloClkTimeBias = 5;
    gnssGloTime.gloClkTimeBias = pbGnssGloTime.gloclktimebias();

    // float gloClkTimeUncMs = 6;
    gnssGloTime.gloClkTimeUncMs = pbGnssGloTime.gloclktimeuncms();

    // uint32  refFCount = 7;
    gnssGloTime.refFCount = pbGnssGloTime.reffcount();

    // uint32 numClockResets = 8;
    gnssGloTime.numClockResets = pbGnssGloTime.numclockresets();

    LOC_LOGd("LocApiPB: pbGnssGloTime - GloValidityMsk: %x, GloFourYear: %u, GloDays: %u, "\
            "GloMsec:%u", gnssGloTime.validityMask, gnssGloTime.gloFourYear, gnssGloTime.gloDays,
             gnssGloTime.gloMsec);
    LocApiPb_LOGd("LocApiPB: pbGnssGloTime - GloClkTimeBias: %f, GloClkTimeUnc: %f, RefFCnt: %u, "\
            "NumClkReset:%u", gnssGloTime.gloClkTimeBias, gnssGloTime.gloClkTimeUncMs,
            gnssGloTime.refFCount, gnssGloTime.numClockResets);
    return 0;
}

int LocationApiPbMsgConv::pbConvertToGeofenceOption(const PBGeofenceOption &pbGfOpt,
        GeofenceOption &gfOpt) const {
    gfOpt.size = sizeof(GeofenceOption);
    // bitwise OR of PBGeofenceBreachTypeMask
    // uint32 breachTypeMask = 1;
    gfOpt.breachTypeMask = getGfBreachTypeMaskFromPB(pbGfOpt.breachtypemask());
    // uint32 responsiveness = 2;
    gfOpt.responsiveness = pbGfOpt.responsiveness();
    // uint32 dwellTime = 3;
    gfOpt.dwellTime = pbGfOpt.dwelltime();
    LOC_LOGd("LocApiPB: pbGfOpt - BreachTypeMask:%x Resp:%u, DwellTime:%u", gfOpt.breachTypeMask,
            gfOpt.responsiveness, gfOpt.dwellTime);
    return 0;
}

int LocationApiPbMsgConv::pbConvertToGeofenceInfo(const PBGeofenceInfo &pbGfInfo,
        GeofenceInfo &gfInfo) const {
    gfInfo.size = sizeof(GeofenceInfo);
    // double latitude = 1;
    gfInfo.latitude = pbGfInfo.latitude();
    // double longitude = 2;
    gfInfo.longitude = pbGfInfo.longitude();
    // double radius = 3;
    gfInfo.radius = pbGfInfo.radius();
    LOC_LOGd("LocApiPB: pbGfInfo - Lat:%lf, Lon:%lf, Rad:%lf", gfInfo.latitude, gfInfo.longitude,
            gfInfo.radius);
    return 0;
}
