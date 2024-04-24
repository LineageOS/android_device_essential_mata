/* Copyright (c) 2011-2020, The Linux Foundation. All rights reserved.
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
 *     * Neither the name of The Linux Foundatoin, nor the names of its
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
#define LOG_NDEBUG 0
#define LOG_TAG "LocSvc_ApiV02"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sstream>
#include <math.h>
#include <dlfcn.h>
#include <algorithm>

#include <LocApiV02.h>
#include <loc_api_v02_log.h>
#include <loc_api_sync_req.h>
#include <loc_api_v02_client.h>
#include <loc_util_log.h>
#include <gps_extended.h>
#include "loc_pla.h"
#include <loc_cfg.h>
#include <LocContext.h>

using namespace std;
using namespace loc_core;

/* Doppler Conversion from M/S to NS/S */
#define MPS_TO_NSPS         (1.0/0.299792458)

/* Default session id ; TBD needs incrementing for each */
#define LOC_API_V02_DEF_SESSION_ID (1)

/* UMTS CP Address key*/
#define LOC_NI_NOTIF_KEY_ADDRESS           "Address"

/* GPS SV Id offset */
#define GPS_SV_ID_OFFSET        (1)

/* GLONASS SV Id offset */
#define GLONASS_SV_ID_OFFSET    (65)

/* SV ID range */
#define SV_ID_RANGE             (32)

#define BDS_SV_ID_OFFSET         (201)

/* BeiDou SV ID RANGE*/
#define BDS_SV_ID_RANGE          QMI_LOC_DELETE_MAX_BDS_SV_INFO_LENGTH_V02

/* GPS week unknown*/
#define C_GPS_WEEK_UNKNOWN      (65535)

/* seconds per week*/
#define WEEK_MSECS              (60*60*24*7*1000LL)
#define DAY_MSECS               (60*60*24*1000LL)
/* Num days elapsed since GLONASS started from GPS start day 1980->1996 */
#define GPS_GLONASS_DAYS_DIFF    5838
/* number of glonass days in a 4-year interval */
#define GLONASS_DAYS_IN_4YEARS   1461
/* glonass and UTC offset: 3 hours */
#define GLONASS_UTC_OFFSET_HOURS 3

/* speed of light */
#define SPEED_OF_LIGHT          299792458.0

#define MAX_SV_CNT_SUPPORTED_IN_ONE_CONSTELLATION 64

/* number of QMI_LOC messages that need to be checked*/
#define NUMBER_OF_MSG_TO_BE_CHECKED        (3)

/* the time, in seconds, to wait for user response for NI  */
#define LOC_NI_NO_RESPONSE_TIME 20

#define LAT_LONG_TO_RADIANS .000005364418
#define GF_RESPONSIVENESS_THRESHOLD_MSEC_HIGH   120000 //2 mins
#define GF_RESPONSIVENESS_THRESHOLD_MSEC_MEDIUM 900000 //15 mins

#define FLP_BATCHING_MINIMUN_INTERVAL           (1000) // in msec
#define FLP_BATCHING_MIN_TRIP_DISTANCE           1 // 1 meter

template struct loc_core::LocApiResponseData<LocApiBatchData>;
template struct loc_core::LocApiResponseData<LocApiGeofenceData>;
template struct loc_core::LocApiResponseData<LocGpsLocation>;

const float CarrierFrequencies[] = {
    0.0,                                // UNKNOWN
    GPS_L1C_CARRIER_FREQUENCY,          // L1C
    SBAS_L1_CA_CARRIER_FREQUENCY,       // SBAS_L1
    GLONASS_G1_CARRIER_FREQUENCY,       // GLONASS_G1
    QZSS_L1CA_CARRIER_FREQUENCY,        // QZSS_L1CA
    BEIDOU_B1_I_CARRIER_FREQUENCY,      // BEIDOU_B1
    GALILEO_E1_C_CARRIER_FREQUENCY,     // GALILEO_E1
    NAVIC_L5_CARRIER_FREQUENCY };       // NAVIC_L5

/* Gaussian 2D scaling table - scale from x% to 68% confidence */
struct conf_scaler_to_68_pair {
    uint8_t confidence;
    float scaler_to_68;
};
/* length of confScalers array */
#define CONF_SCALER_ARRAY_MAX   (3)
const struct conf_scaler_to_68_pair confScalers[CONF_SCALER_ARRAY_MAX] = {
    {39, 1.517}, // 0 - 39 . Index 0
    {50, 1.287}, // 40 - 50. Index 1
    {63, 1.072}, // 51 - 63. Index 2
};

/*fixed timestamp uncertainty 10 milli second */
static int ap_timestamp_uncertainty = 0;

typedef enum {
    RF_LOSS_GPS_CONF        = 0,
    RF_LOSS_GPS_L5_CONF     = 1,
    RF_LOSS_GLO_LEFT_CONF   = 2,
    RF_LOSS_GLO_CENTER_CONF = 3,
    RF_LOSS_GLO_RIGHT_CONF  = 4,
    RF_LOSS_BDS_CONF        = 5,
    RF_LOSS_BDS_B2A_CONF    = 6,
    RF_LOSS_GAL_CONF        = 7,
    RF_LOSS_GAL_E5_CONF     = 8,
    RF_LOSS_NAVIC_CONF      = 9,
    RF_LOSS_MAX_CONF        = 10
} rfLossConf;

static uint32_t rfLossNV[RF_LOSS_MAX_CONF] = { 0 };

static loc_param_s_type gps_conf_param_table[] =
{
    { "AP_TIMESTAMP_UNCERTAINTY",   &ap_timestamp_uncertainty,          NULL, 'n' },
    { "RF_LOSS_GPS",                &rfLossNV[RF_LOSS_GPS_CONF],        NULL, 'n' },
    { "RF_LOSS_GPS_L5",             &rfLossNV[RF_LOSS_GPS_L5_CONF],     NULL, 'n' },
    { "RF_LOSS_GLO_LEFT",           &rfLossNV[RF_LOSS_GLO_LEFT_CONF],   NULL, 'n' },
    { "RF_LOSS_GLO_CENTER",         &rfLossNV[RF_LOSS_GLO_CENTER_CONF], NULL, 'n' },
    { "RF_LOSS_GLO_RIGHT",          &rfLossNV[RF_LOSS_GLO_RIGHT_CONF],  NULL, 'n' },
    { "RF_LOSS_BDS",                &rfLossNV[RF_LOSS_BDS_CONF],        NULL, 'n' },
    { "RF_LOSS_BDS_B2A",            &rfLossNV[RF_LOSS_BDS_B2A_CONF],    NULL, 'n' },
    { "RF_LOSS_GAL",                &rfLossNV[RF_LOSS_GAL_CONF],        NULL, 'n' },
    { "RF_LOSS_GAL_E5",             &rfLossNV[RF_LOSS_GAL_E5_CONF],     NULL, 'n' },
    { "RF_LOSS_NAVIC",              &rfLossNV[RF_LOSS_NAVIC_CONF],      NULL, 'n' },
};

/* static event callbacks that call the LocApiV02 callbacks*/

/* global event callback, call the eventCb function in loc api adapter v02
   instance */
static void globalEventCb(locClientHandleType clientHandle,
                          uint32_t eventId,
                          const locClientEventIndUnionType eventPayload,
                          void*  pClientCookie)
{
    switch (eventId) {
    case QMI_LOC_EVENT_POSITION_REPORT_IND_V02:
    case QMI_LOC_EVENT_UNPROPAGATED_POSITION_REPORT_IND_V02:
    case QMI_LOC_EVENT_NMEA_IND_V02:
    case QMI_LOC_EVENT_GNSS_SV_INFO_IND_V02:
    case QMI_LOC_EVENT_GNSS_MEASUREMENT_REPORT_IND_V02:
    case QMI_LOC_EVENT_SV_POLYNOMIAL_REPORT_IND_V02:
    case QMI_LOC_EVENT_GPS_EPHEMERIS_REPORT_IND_V02:
    case QMI_LOC_EVENT_GLONASS_EPHEMERIS_REPORT_IND_V02:
    case QMI_LOC_EVENT_BDS_EPHEMERIS_REPORT_IND_V02:
    case QMI_LOC_EVENT_GALILEO_EPHEMERIS_REPORT_IND_V02:
    case QMI_LOC_EVENT_QZSS_EPHEMERIS_REPORT_IND_V02:
    case QMI_LOC_LATENCY_INFORMATION_IND_V02:
        MODEM_LOG_CALLFLOW_DEBUG(%s, loc_get_v02_event_name(eventId));
        break;
    default:
        MODEM_LOG_CALLFLOW(%s, loc_get_v02_event_name(eventId));
        break;
    }

    LocApiV02 *locApiV02Instance = (LocApiV02 *)pClientCookie;

    LOC_LOGv ("client = %p, event id = 0x%X, client cookie ptr = %p",
              clientHandle, eventId, pClientCookie);

    // return if null is passed
    if (NULL == locApiV02Instance)
    {
        LOC_LOGe ("NULL object passed : client = %p, event id = 0x%X",
                  clientHandle, eventId);
        return;
    }
    locApiV02Instance->eventCb(clientHandle, eventId, eventPayload);
}

/* global response callback, it calls the sync request process
   indication function to unblock the request that is waiting on this
   response indication*/
static void globalRespCb(locClientHandleType clientHandle,
                         uint32_t respId,
                         const locClientRespIndUnionType respPayload,
                         uint32_t respPayloadSize,
                         void*  pClientCookie)
{
  MODEM_LOG_CALLFLOW(%s, loc_get_v02_event_name(respId));
  LocApiV02 *locApiV02Instance =
        (LocApiV02 *)pClientCookie;

  LOC_LOGV ("%s:%d] client = %p, resp id = %d, client cookie ptr = %p\n",
                  __func__,  __LINE__,  clientHandle, respId, pClientCookie);

  if( NULL == locApiV02Instance)
  {
    LOC_LOGE ("%s:%d] NULL object passed : client = %p, resp id = %d\n",
                  __func__,  __LINE__,  clientHandle, respId);
    return;
  }

  switch(respId)
  {
    case QMI_LOC_GET_AVAILABLE_WWAN_POSITION_IND_V02:
      if (respPayload.pGetAvailWwanPositionInd != NULL) {
          locApiV02Instance->handleWwanZppFixIndication(*respPayload.pGetAvailWwanPositionInd);
      }
      break;
    case QMI_LOC_GET_BEST_AVAILABLE_POSITION_IND_V02:
        if (respPayload.pGetBestAvailablePositionInd != NULL) {
            locApiV02Instance->handleZppBestAvailableFixIndication(
                    *respPayload.pGetBestAvailablePositionInd);
        }
        [[fallthrough]];
       // Call loc_sync_process_ind below also
    default:
      // process the sync call
      // use pDeleteAssistDataInd as a dummy pointer
      loc_sync_process_ind(clientHandle, respId,
          (void *)respPayload.pDeleteAssistDataInd, respPayloadSize);
      break;
  }
}

/* global error callback, it will call the handle service down
   function in the loc api adapter instance. */
static void globalErrorCb (locClientHandleType clientHandle,
                           locClientErrorEnumType errorId,
                           void *pClientCookie)
{
  LocApiV02 *locApiV02Instance =
          (LocApiV02 *)pClientCookie;

  LOC_LOGV ("%s:%d] client = %p, error id = %d\n, client cookie ptr = %p\n",
                  __func__,  __LINE__,  clientHandle, errorId, pClientCookie);
  if( NULL == locApiV02Instance)
  {
    LOC_LOGE ("%s:%d] NULL object passed : client = %p, error id = %d\n",
                  __func__,  __LINE__,  clientHandle, errorId);
    return;
  }
  locApiV02Instance->errorCb(clientHandle, errorId);
}

/* global structure containing the callbacks */
locClientCallbacksType globalCallbacks =
{
    sizeof(locClientCallbacksType),
    globalEventCb,
    globalRespCb,
    globalErrorCb
};

static void getInterSystemTimeBias(const char* interSystem,
                                   Gnss_InterSystemBiasStructType &interSystemBias,
                                   const qmiLocInterSystemBiasStructT_v02* pInterSysBias)
{
    LOC_LOGd("%s] Mask:%d, TimeBias:%f, TimeBiasUnc:%f,\n",
             interSystem, pInterSysBias->validMask, pInterSysBias->timeBias,
             pInterSysBias->timeBiasUnc);

    interSystemBias.validMask    = pInterSysBias->validMask;
    interSystemBias.timeBias     = pInterSysBias->timeBias;
    interSystemBias.timeBiasUnc  = pInterSysBias->timeBiasUnc;
}

/* Constructor for LocApiV02 */
LocApiV02 :: LocApiV02(LOC_API_ADAPTER_EVENT_MASK_T exMask,
                       ContextBase* context):
    LocApiBase(exMask, context),
    clientHandle(LOC_CLIENT_INVALID_HANDLE_VALUE),
    mQmiMask(0), mInSession(false), mPowerMode(GNSS_POWER_MODE_INVALID),
    mEngineOn(false), mMeasurementsStarted(false),
    mMasterRegisterNotSupported(false),
    mCounter(0), mMinInterval(1000),
    mGnssMeasurements(nullptr),
    mBatchSize(0), mDesiredBatchSize(0),
    mTripBatchSize(0), mDesiredTripBatchSize(0),
    mIsFirstFinalFixReported(false),
    mIsFirstStartFixReq(false),
    mHlosQtimer1(0),
    mHlosQtimer2(0),
    mRefFCount(0),
    mMeasElapsedRealTimeCal(600000000),
    mPositionElapsedRealTimeCal(30000000)
{
  // initialize loc_sync_req interface
  loc_sync_req_init();
  mADRdata.clear();

  UTIL_READ_CONF(LOC_PATH_GPS_CONF, gps_conf_param_table);
}

/* Destructor for LocApiV02 */
LocApiV02 :: ~LocApiV02()
{
    close();
}

LocApiBase* getLocApi(LOC_API_ADAPTER_EVENT_MASK_T exMask,
                      ContextBase* context)
{
    return (LocApiBase*)LocApiV02::createLocApiV02(exMask, context);
}

LocApiBase* LocApiV02::createLocApiV02(LOC_API_ADAPTER_EVENT_MASK_T exMask,
                                       ContextBase* context)
{
    LOC_LOGD("%s:%d]: Creating new LocApiV02", __func__, __LINE__);
    return new LocApiV02(exMask, context);
}

/* Initialize a loc api v02 client AND
   check which loc message are supported by modem */
enum loc_api_adapter_err
LocApiV02 :: open(LOC_API_ADAPTER_EVENT_MASK_T mask)
{
  enum loc_api_adapter_err rtv = LOC_API_ADAPTER_ERR_SUCCESS;
  locClientStatusEnumType status = eLOC_CLIENT_SUCCESS;

  LOC_API_ADAPTER_EVENT_MASK_T newMask = mask & ~mExcludedMask;

  LOC_LOGd("%p Enter mMask: 0x%" PRIx64 "  mQmiMask: 0x%" PRIx64 " mExcludedMask: 0x%" PRIx64 "",
           clientHandle, mMask, mQmiMask, mExcludedMask);

  /* If the client is already open close it first */
  if(LOC_CLIENT_INVALID_HANDLE_VALUE == clientHandle)
  {
    LOC_LOGV ("%s:%d]: reference to this = %p passed in \n",
              __func__, __LINE__, this);
    /* initialize the loc api v02 interface, note that
       the locClientOpen() function will block if the
       service is unavailable for a fixed time out */

    // it is important to cap the mask here, because not all LocApi's
    // can enable the same bits, e.g. foreground and bckground.
    status = locClientOpen(0, &globalCallbacks, &clientHandle, (void *)this);
    if (eLOC_CLIENT_SUCCESS != status ||
        clientHandle == LOC_CLIENT_INVALID_HANDLE_VALUE )
    {
      mMask = 0;
      mNmeaMask = 0;
      mQmiMask = 0;
      LOC_LOGE ("%s:%d]: locClientOpen failed, status = %s\n", __func__,
                __LINE__, loc_get_v02_client_status_name(status));
      rtv = LOC_API_ADAPTER_ERR_FAILURE;
    } else {
        uint64_t supportedMsgList = 0;
        bool gnssMeasurementSupported = false;
        const uint32_t msgArray[NUMBER_OF_MSG_TO_BE_CHECKED] =
        {
            // For - LOC_API_ADAPTER_MESSAGE_LOCATION_BATCHING
            QMI_LOC_GET_BATCH_SIZE_REQ_V02,

            // For - LOC_API_ADAPTER_MESSAGE_BATCHED_GENFENCE_BREACH
            QMI_LOC_EVENT_GEOFENCE_BATCHED_BREACH_NOTIFICATION_IND_V02,

            // For - LOC_API_ADAPTER_MESSAGE_DISTANCE_BASE_TRACKING
            QMI_LOC_START_DBT_REQ_V02
        };

        if (isMaster()) {
            registerMasterClient();
            gnssMeasurementSupported = cacheGnssMeasurementSupport();
            LocContext::injectFeatureConfig(mContext);
        }

        // check the modem
        status = locClientSupportMsgCheck(clientHandle,
                                          msgArray,
                                          NUMBER_OF_MSG_TO_BE_CHECKED,
                                          &supportedMsgList);
        if (eLOC_CLIENT_SUCCESS != status) {
            LOC_LOGE("%s:%d]: Failed to checking QMI_LOC message supported. \n",
                     __func__, __LINE__);
        }

        /** if batching is supported , check if the adaptive batching or
            distance-based batching is supported. */
        uint32_t messageChecker = 1 << LOC_API_ADAPTER_MESSAGE_LOCATION_BATCHING;
        if ((messageChecker & supportedMsgList) == messageChecker) {
            locClientReqUnionType req_union;
            locClientStatusEnumType status = eLOC_CLIENT_SUCCESS;
            qmiLocQueryAonConfigReqMsgT_v02 queryAonConfigReq;
            qmiLocQueryAonConfigIndMsgT_v02 queryAonConfigInd;

            memset(&queryAonConfigReq, 0, sizeof(queryAonConfigReq));
            memset(&queryAonConfigInd, 0, sizeof(queryAonConfigInd));
            queryAonConfigReq.transactionId = LOC_API_V02_DEF_SESSION_ID;

            req_union.pQueryAonConfigReq = &queryAonConfigReq;
            status = locSyncSendReq(QMI_LOC_QUERY_AON_CONFIG_REQ_V02,
                                    req_union,
                                    LOC_ENGINE_SYNC_REQUEST_TIMEOUT,
                                    QMI_LOC_QUERY_AON_CONFIG_IND_V02,
                                    &queryAonConfigInd);

            if (status == eLOC_CLIENT_FAILURE_UNSUPPORTED) {
                LOC_LOGE("%s:%d]: Query AON config is not supported.\n", __func__, __LINE__);
            } else {
                if (status != eLOC_CLIENT_SUCCESS ||
                    queryAonConfigInd.status != eQMI_LOC_SUCCESS_V02) {
                    LOC_LOGE("%s:%d]: Query AON config failed."
                             " status: %s, ind status:%s\n",
                             __func__, __LINE__,
                             loc_get_v02_client_status_name(status),
                             loc_get_v02_qmi_status_name(queryAonConfigInd.status));
                } else {
                    LOC_LOGD("%s:%d]: Query AON config succeeded. aonCapability is %d.\n",
                             __func__, __LINE__, queryAonConfigInd.aonCapability);
                    if (queryAonConfigInd.aonCapability_valid) {
                        if (queryAonConfigInd.aonCapability |
                            QMI_LOC_MASK_AON_TIME_BASED_BATCHING_SUPPORTED_V02) {
                            LOC_LOGD("%s:%d]: LB 1.0 is supported.\n", __func__, __LINE__);
                        }
                        if (queryAonConfigInd.aonCapability |
                            QMI_LOC_MASK_AON_AUTO_BATCHING_SUPPORTED_V02) {
                            LOC_LOGD("%s:%d]: LB 1.5 is supported.\n", __func__, __LINE__);
                            supportedMsgList |=
                                (1 << LOC_API_ADAPTER_MESSAGE_ADAPTIVE_LOCATION_BATCHING);
                        }
                        if (queryAonConfigInd.aonCapability |
                            QMI_LOC_MASK_AON_DISTANCE_BASED_BATCHING_SUPPORTED_V02) {
                            LOC_LOGD("%s:%d]: LB 2.0 is supported.\n", __func__, __LINE__);
                            supportedMsgList |=
                                (1 << LOC_API_ADAPTER_MESSAGE_DISTANCE_BASE_LOCATION_BATCHING);
                        }
                        if (queryAonConfigInd.aonCapability |
                            QMI_LOC_MASK_AON_DISTANCE_BASED_TRACKING_SUPPORTED_V02) {
                            LOC_LOGD("%s:%d]: DBT 2.0 is supported.\n", __func__, __LINE__);
                        }
                        if (queryAonConfigInd.aonCapability |
                            QMI_LOC_MASK_AON_UPDATE_TBF_SUPPORTED_V02) {
                            LOC_LOGD("%s:%d]: Updating tracking TBF on the fly is supported.\n",
                                     __func__, __LINE__);
                            supportedMsgList |=
                                (1 << LOC_API_ADAPTER_MESSAGE_UPDATE_TBF_ON_THE_FLY);
                        }
                        if (queryAonConfigInd.aonCapability |
                            QMI_LOC_MASK_AON_OUTDOOR_TRIP_BATCHING_SUPPORTED_V02) {
                            LOC_LOGD("%s:%d]: OTB is supported.\n",
                                     __func__, __LINE__);
                            supportedMsgList |=
                                (1 << LOC_API_ADAPTER_MESSAGE_OUTDOOR_TRIP_BATCHING);
                        }
                    } else {
                        LOC_LOGE("%s:%d]: AON capability is invalid.\n", __func__, __LINE__);
                    }
                }
            }
        }
        LOC_LOGV("%s:%d]: supportedMsgList is %" PRIu64 ". \n",
                 __func__, __LINE__, supportedMsgList);

        // Query for supported feature list
        locClientReqUnionType req_union;
        locClientStatusEnumType status = eLOC_CLIENT_SUCCESS;
        qmiLocGetSupportedFeatureReqMsgT_v02 getSupportedFeatureList_req;
        qmiLocGetSupportedFeatureIndMsgT_v02 getSupportedFeatureList_ind;

        memset(&getSupportedFeatureList_req, 0, sizeof(getSupportedFeatureList_req));
        memset(&getSupportedFeatureList_ind, 0, sizeof(getSupportedFeatureList_ind));

        req_union.pGetSupportedFeatureReq = &getSupportedFeatureList_req;
        status = locSyncSendReq(QMI_LOC_GET_SUPPORTED_FEATURE_REQ_V02,
                                req_union,
                                LOC_ENGINE_SYNC_REQUEST_TIMEOUT,
                                QMI_LOC_GET_SUPPORTED_FEATURE_IND_V02,
                                &getSupportedFeatureList_ind);
        if (eLOC_CLIENT_SUCCESS != status) {
            LOC_LOGE("%s:%d:%d]: Failed to get features supported from "
                     "QMI_LOC_GET_SUPPORTED_FEATURE_REQ_V02. \n", __func__, __LINE__, status);
        } else {
            LOC_LOGD("%s:%d:%d]: Got list of features supported of length:%d ",
                     __func__, __LINE__, status, getSupportedFeatureList_ind.feature_len);
            for (uint32_t i = 0; i < getSupportedFeatureList_ind.feature_len; i++) {
                LOC_LOGD("Bit-mask of supported features at index:%d is %d",i,
                         getSupportedFeatureList_ind.feature[i]);
            }
        }

        // cache the mpss engine capabilities
       mContext->setEngineCapabilities(supportedMsgList,
            (getSupportedFeatureList_ind.feature_len != 0 ? getSupportedFeatureList_ind.feature:
            NULL), gnssMeasurementSupported);

        getEngineLockStateSync();
    }
  }

  if ((eLOC_CLIENT_SUCCESS == status) && (LOC_CLIENT_INVALID_HANDLE_VALUE != clientHandle)) {
    registerEventMask(newMask);
  }

  LOC_LOGd("clientHandle = %p Exit mMask: 0x%" PRIx64 " mQmiMask: 0x%" PRIx64 "",
           clientHandle, mMask, mQmiMask);

  return rtv;
}

void LocApiV02 :: registerEventMask(LOC_API_ADAPTER_EVENT_MASK_T adapterMask)
{
  locClientEventMaskType qmiMask = adjustMaskIfNoSessionOrEngineOff(convertMask(adapterMask));

  LOC_LOGd("clientHandle = %p; mMask: 0x%" PRIx64 "; newMask: 0x%" PRIx64 "; mQmiMask: 0x%" PRIx64
           "; qmiMask: 0x%" PRIx64 "", clientHandle, mMask, adapterMask, mQmiMask, qmiMask);

  if ((qmiMask != mQmiMask) &&
      (locClientRegisterEventMask(clientHandle, qmiMask, isMaster()))) {
    locClientEventMaskType maskDiff = qmiMask ^ mQmiMask;
    if (maskDiff & qmiMask & QMI_LOC_EVENT_MASK_WIFI_REQ_V02) {
      wifiStatusInformSync();
    }

    if (isMaster()) {
      /* Set the SV Measurement Constellation when Measurement Report or Polynomial report is set */
      /* Check if either measurement report or sv polynomial report bit is different in the new
         mask compared to the old mask. If yes then turn that report on or off as requested */
      /* Later change: we need to set the SV Measurement Constellation whenever measurements
         are supported, and that is because other clients (e.g. CHRE need to have measurements
         enabled and those clients cannot set the SV Measurement Constellation since they are
         not master */
      locClientEventMaskType measOrSvPoly = QMI_LOC_EVENT_MASK_GNSS_SV_POLYNOMIAL_REPORT_V02;
      LOC_LOGd("clientHandle: %p isMaster: %d measOrSvPoly: 0x%" PRIx64 " maskDiff: 0x%" PRIx64 "",
               clientHandle, isMaster(), measOrSvPoly, maskDiff);
      if (((maskDiff & measOrSvPoly) != 0)) {
        if (mContext->gnssConstellationConfig()) {
          setSvMeasurementConstellation(qmiMask | QMI_LOC_EVENT_MASK_GNSS_MEASUREMENT_REPORT_V02);
        } else {
          setSvMeasurementConstellation(qmiMask & ~QMI_LOC_EVENT_MASK_GNSS_MEASUREMENT_REPORT_V02);
        }
      }
    }
    mQmiMask = qmiMask;
  }
  mMask = adapterMask;
}

bool LocApiV02::sendRequestForAidingData(locClientEventMaskType qmiMask) {

    qmiLocSetGNSSConstRepConfigReqMsgT_v02 aidingDataReq;
    qmiLocSetGNSSConstRepConfigIndMsgT_v02 aidingDataReqInd;
    locClientStatusEnumType status;
    locClientReqUnionType req_union;

    LOC_LOGd("qmiMask = 0x%" PRIx64 "\n", qmiMask);

    memset(&aidingDataReq, 0, sizeof(aidingDataReq));

    if (qmiMask & QMI_LOC_EVENT_MASK_GNSS_SV_POLYNOMIAL_REPORT_V02) {
        aidingDataReq.reportFullSvPolyDb_valid = true;
        aidingDataReq.reportFullSvPolyDb = true;
    }

    if (qmiMask & QMI_LOC_EVENT_MASK_EPHEMERIS_REPORT_V02) {
        aidingDataReq.reportFullEphemerisDb_valid = true;
        aidingDataReq.reportFullEphemerisDb = true;
    }

    /* Note: Requesting for full KlobucharIonoModel report is based on
       QMI_LOC_EVENT_MASK_GNSS_EVENT_REPORT_V02 bit as there is no separate QMI subscription bit for
       iono model.*/
    if (qmiMask & QMI_LOC_EVENT_MASK_GNSS_EVENT_REPORT_V02) {
        aidingDataReq.reportFullIonoDb_valid = true;
        aidingDataReq.reportFullIonoDb = true;
    }

    req_union.pSetGNSSConstRepConfigReq = &aidingDataReq;
    memset(&aidingDataReqInd, 0, sizeof(aidingDataReqInd));

    status = locSyncSendReq(QMI_LOC_SET_GNSS_CONSTELL_REPORT_CONFIG_V02,
        req_union,
        LOC_ENGINE_SYNC_REQUEST_TIMEOUT,
        QMI_LOC_SET_GNSS_CONSTELL_REPORT_CONFIG_IND_V02,
        &aidingDataReqInd);

    if (status != eLOC_CLIENT_SUCCESS ||
        (aidingDataReqInd.status != eQMI_LOC_SUCCESS_V02 &&
            aidingDataReqInd.status != eQMI_LOC_ENGINE_BUSY_V02)) {
        LOC_LOGe("Request for aiding data failed status: %s, ind status:%s",
            loc_get_v02_client_status_name(status),
            loc_get_v02_qmi_status_name(aidingDataReqInd.status));
    }
    else {
        LOC_LOGd("Request for aiding data succeeded");
    }

    return true;

}

locClientEventMaskType LocApiV02 :: adjustMaskIfNoSessionOrEngineOff(locClientEventMaskType qmiMask)
{
    locClientEventMaskType oldQmiMask = qmiMask;
    if (!mInSession) {
        locClientEventMaskType clearMask = QMI_LOC_EVENT_MASK_POSITION_REPORT_V02 |
                                           QMI_LOC_EVENT_MASK_UNPROPAGATED_POSITION_REPORT_V02 |
                                           QMI_LOC_EVENT_MASK_GNSS_SV_INFO_V02 |
                                           QMI_LOC_EVENT_MASK_NMEA_V02 |
                                           QMI_LOC_EVENT_MASK_ENGINE_STATE_V02 |
                                           QMI_LOC_EVENT_MASK_GNSS_MEASUREMENT_REPORT_V02 |
                                           QMI_LOC_EVENT_MASK_GNSS_NHZ_MEASUREMENT_REPORT_V02 |
                                           QMI_LOC_EVENT_MASK_GNSS_SV_POLYNOMIAL_REPORT_V02 |
                                           QMI_LOC_EVENT_MASK_EPHEMERIS_REPORT_V02 |
                                           QMI_LOC_EVENT_MASK_GNSS_EVENT_REPORT_V02 |
                                           QMI_LOC_EVENT_MASK_NEXT_LS_INFO_REPORT_V02 |
                                           QMI_LOC_EVENT_MASK_LATENCY_INFORMATION_REPORT_V02;
        qmiMask = qmiMask & ~clearMask;
    } else if (!mEngineOn) {
        locClientEventMaskType clearMask = QMI_LOC_EVENT_MASK_NMEA_V02;
        qmiMask = qmiMask & ~clearMask;
    }
    LOC_LOGd("oldQmiMask=%" PRIx64 " qmiMask=%" PRIx64 " mInSession: %d mEngineOn: %d",
            oldQmiMask, qmiMask, mInSession, mEngineOn);
    return qmiMask;
}

enum loc_api_adapter_err LocApiV02 :: close()
{
  enum loc_api_adapter_err rtv =
      // success if either client is already invalid, or
      // we successfully close the handle
      (LOC_CLIENT_INVALID_HANDLE_VALUE == clientHandle ||
       eLOC_CLIENT_SUCCESS == locClientClose(&clientHandle)) ?
      LOC_API_ADAPTER_ERR_SUCCESS : LOC_API_ADAPTER_ERR_FAILURE;

  mMask = 0;
  mQmiMask = 0;
  mInSession = false;
  clientHandle = LOC_CLIENT_INVALID_HANDLE_VALUE;

  return rtv;
}


/* start positioning session */
void LocApiV02 :: startFix(const LocPosMode& fixCriteria, LocApiResponse *adapterResponse)
{
  sendMsg(new LocApiMsg([this, fixCriteria, adapterResponse] () {

  locClientStatusEnumType status;
  locClientReqUnionType req_union;

  qmiLocStartReqMsgT_v02 start_msg;

  qmiLocSetOperationModeReqMsgT_v02 set_mode_msg;
  qmiLocSetOperationModeIndMsgT_v02 set_mode_ind;

    // clear all fields, validity masks
  memset (&start_msg, 0, sizeof(start_msg));
  memset (&set_mode_msg, 0, sizeof(set_mode_msg));
  memset (&set_mode_ind, 0, sizeof(set_mode_ind));

  LOC_LOGV("%s:%d]: start \n", __func__, __LINE__);
  // BOOT KPI marker, print only once for a session
  if (false == mInSession) {
      loc_boot_kpi_marker("L - LocApiV02 startFix, tbf %d", fixCriteria.min_interval);
  }
  fixCriteria.logv();

  mInSession = true;
  mMeasurementsStarted = true;
  registerEventMask(mMask);

  // fill in the start request
  switch(fixCriteria.mode)
  {
    case LOC_POSITION_MODE_MS_BASED:
      set_mode_msg.operationMode = eQMI_LOC_OPER_MODE_MSB_V02;
      break;

    case LOC_POSITION_MODE_MS_ASSISTED:
      set_mode_msg.operationMode = eQMI_LOC_OPER_MODE_MSA_V02;
      break;

    case LOC_POSITION_MODE_RESERVED_4:
      set_mode_msg.operationMode = eQMI_LOC_OPER_MODE_CELL_ID_V02;
        break;

    case LOC_POSITION_MODE_RESERVED_5:
      set_mode_msg.operationMode = eQMI_LOC_OPER_MODE_WWAN_V02;
        break;

    default:
      set_mode_msg.operationMode = eQMI_LOC_OPER_MODE_STANDALONE_V02;
      break;
  }

  req_union.pSetOperationModeReq = &set_mode_msg;

  // send the mode first, before the start message.
  status = locSyncSendReq(QMI_LOC_SET_OPERATION_MODE_REQ_V02,
                          req_union, LOC_ENGINE_SYNC_REQUEST_TIMEOUT,
                          QMI_LOC_SET_OPERATION_MODE_IND_V02,
                          &set_mode_ind); // NULL?
   //When locSyncSendReq status is time out, more likely the response was lost.
   //startFix will continue as though it is succeeded.
  if ((status != eLOC_CLIENT_SUCCESS && status != eLOC_CLIENT_FAILURE_TIMEOUT) ||
       eQMI_LOC_SUCCESS_V02 != set_mode_ind.status)
  {
    LOC_LOGE ("%s:%d]: set opertion mode failed status = %s, "
                   "ind..status = %s\n", __func__, __LINE__,
              loc_get_v02_client_status_name(status),
              loc_get_v02_qmi_status_name(set_mode_ind.status));
  } else {
      if (status == eLOC_CLIENT_FAILURE_TIMEOUT)
      {
          LOC_LOGE ("%s:%d]: set operation mode timed out\n", __func__, __LINE__);
      }
      start_msg.minInterval_valid = 1;
      start_msg.minInterval = fixCriteria.min_interval;
      mMinInterval = start_msg.minInterval;

      start_msg.horizontalAccuracyLevel_valid = 1;

      if (fixCriteria.preferred_accuracy <= 100)
      {
          // fix needs high accuracy
          start_msg.horizontalAccuracyLevel =  eQMI_LOC_ACCURACY_HIGH_V02;
      }
      else if (fixCriteria.preferred_accuracy <= 1000)
      {
          //fix needs med accuracy
          start_msg.horizontalAccuracyLevel =  eQMI_LOC_ACCURACY_MED_V02;
      }
      else
      {
          //fix needs low accuracy
          start_msg.horizontalAccuracyLevel =  eQMI_LOC_ACCURACY_LOW_V02;
          // limit the scanning max time to 1 min and TBF to 10 min
          // this is to control the power cost for gps for LOW accuracy
          start_msg.positionReportTimeout_valid = 1;
          start_msg.positionReportTimeout = 60000;
          if (start_msg.minInterval < 600000) {
              start_msg.minInterval = 600000;
          }
      }

      start_msg.fixRecurrence_valid = 1;
      if(LOC_GPS_POSITION_RECURRENCE_SINGLE == fixCriteria.recurrence)
      {
          start_msg.fixRecurrence = eQMI_LOC_RECURRENCE_SINGLE_V02;
      }
      else
      {
          start_msg.fixRecurrence = eQMI_LOC_RECURRENCE_PERIODIC_V02;
      }

      //dummy session id
      // TBD: store session ID, check for session id in pos reports.
      start_msg.sessionId = LOC_API_V02_DEF_SESSION_ID;

      //Set whether position report can be shared with other LOC clients
      start_msg.sharePosition_valid = 1;
      start_msg.sharePosition = fixCriteria.share_position;

      if (fixCriteria.credentials[0] != 0) {
          int size1 = sizeof(start_msg.applicationId.applicationName);
          int size2 = sizeof(fixCriteria.credentials);
          int len = ((size1 < size2) ? size1 : size2) - 1;
          memcpy(start_msg.applicationId.applicationName,
                 fixCriteria.credentials,
                 len);

          size1 = sizeof(start_msg.applicationId.applicationProvider);
          size2 = sizeof(fixCriteria.provider);
          len = ((size1 < size2) ? size1 : size2) - 1;
          memcpy(start_msg.applicationId.applicationProvider,
                 fixCriteria.provider,
                 len);

          start_msg.applicationId_valid = 1;
      }

      // config Altitude Assumed
      start_msg.configAltitudeAssumed_valid = 1;
      start_msg.configAltitudeAssumed = eQMI_LOC_ALTITUDE_ASSUMED_IN_GNSS_SV_INFO_DISABLED_V02;

      // set power mode details
      mPowerMode = fixCriteria.powerMode;
      if (GNSS_POWER_MODE_INVALID != fixCriteria.powerMode) {
          start_msg.powerMode_valid = 1;
          start_msg.powerMode.powerMode = convertPowerMode(fixCriteria.powerMode);
          start_msg.powerMode.timeBetweenMeasurement =
                  fixCriteria.timeBetweenMeasurements;
          // Force low accuracy for background power modes
          if (GNSS_POWER_MODE_M3 == fixCriteria.powerMode ||
                  GNSS_POWER_MODE_M4 == fixCriteria.powerMode ||
                  GNSS_POWER_MODE_M5 == fixCriteria.powerMode) {
              start_msg.horizontalAccuracyLevel =  eQMI_LOC_ACCURACY_LOW_V02;
          }
          // Force TBM = TBF for power mode M4
          if (GNSS_POWER_MODE_M4 == fixCriteria.powerMode) {
              start_msg.powerMode.timeBetweenMeasurement = start_msg.minInterval;
          }
      }

      req_union.pStartReq = &start_msg;

      status = locClientSendReq(QMI_LOC_START_REQ_V02, req_union);
  }

  LocationError err = LOCATION_ERROR_GENERAL_FAILURE;
  if (eLOC_CLIENT_SUCCESS == status) {
      err = LOCATION_ERROR_SUCCESS;
  }

  if (adapterResponse != NULL) {
      adapterResponse->returnToSender(err);
  }
  }));

}

/* stop a positioning session */
void LocApiV02 :: stopFix(LocApiResponse *adapterResponse)
{
  sendMsg(new LocApiMsg([this, adapterResponse] () {

  locClientStatusEnumType status;
  locClientReqUnionType req_union;

  qmiLocStopReqMsgT_v02 stop_msg;

  LOC_LOGD(" %s:%d]: stop called \n", __func__, __LINE__);
  loc_boot_kpi_marker("L - LocApiV02 stop Fix session");

  memset(&stop_msg, 0, sizeof(stop_msg));

  // dummy session id
  stop_msg.sessionId = LOC_API_V02_DEF_SESSION_ID;

  req_union.pStopReq = &stop_msg;

  status = locClientSendReq(QMI_LOC_STOP_REQ_V02, req_union);

  mIsFirstFinalFixReported = false;
  mInSession = false;
  mPowerMode = GNSS_POWER_MODE_INVALID;

  // if engine on never happend, deregister events
  // without waiting for Engine Off
  if (!mEngineOn) {
      registerEventMask(mMask);
  }

  // free the memory used to assemble SV measurement from
  // different constellations and bands
  if (!mGnssMeasurements) {
      free(mGnssMeasurements);
      mGnssMeasurements = nullptr;
  }

  if( eLOC_CLIENT_SUCCESS != status)
  {
      LOC_LOGE("%s:%d]: error = %s\n",__func__, __LINE__,
               loc_get_v02_client_status_name(status));
  }

  LocationError err = LOCATION_ERROR_GENERAL_FAILURE;
  if (eLOC_CLIENT_SUCCESS == status) {
      err = LOCATION_ERROR_SUCCESS;
  }

  if (adapterResponse != NULL) {
      adapterResponse->returnToSender(err);
  }
  }));
}

/* inject time into the position engine */
void LocApiV02 ::
    setTime(LocGpsUtcTime time, int64_t timeReference, int uncertainty)
{
  sendMsg(new LocApiMsg([this, time, timeReference, uncertainty] () {

  locClientReqUnionType req_union;
  locClientStatusEnumType status;
  qmiLocInjectUtcTimeReqMsgT_v02  inject_time_msg;
  qmiLocInjectUtcTimeIndMsgT_v02 inject_time_ind;

  memset(&inject_time_msg, 0, sizeof(inject_time_msg));

  inject_time_ind.status = eQMI_LOC_GENERAL_FAILURE_V02;

  inject_time_msg.timeUtc = time;

  inject_time_msg.timeUtc += (int64_t)(elapsedRealtime() - timeReference);

  inject_time_msg.timeUnc = uncertainty;

  req_union.pInjectUtcTimeReq = &inject_time_msg;

  LOC_LOGV ("%s:%d]: uncertainty = %d\n", __func__, __LINE__,
                 uncertainty);

  status = locSyncSendReq(QMI_LOC_INJECT_UTC_TIME_REQ_V02,
                          req_union, LOC_ENGINE_SYNC_REQUEST_TIMEOUT,
                          QMI_LOC_INJECT_UTC_TIME_IND_V02,
                          &inject_time_ind);

  if (status != eLOC_CLIENT_SUCCESS ||
      eQMI_LOC_SUCCESS_V02 != inject_time_ind.status)
  {
    LOC_LOGE ("%s:%d] status = %s, ind..status = %s\n", __func__,  __LINE__,
              loc_get_v02_client_status_name(status),
              loc_get_v02_qmi_status_name(inject_time_ind.status));
  }

  }));
}

/* inject position into the position engine */
void LocApiV02 ::
    injectPosition(double latitude, double longitude, float accuracy, bool onDemandCpi)
{
    Location location = {};

    location.flags |= LOCATION_HAS_LAT_LONG_BIT;
    location.latitude = latitude;
    location.longitude = longitude;

    location.flags |= LOCATION_HAS_ACCURACY_BIT;
    location.accuracy = accuracy;

    struct timespec time_info_current;
    if(clock_gettime(CLOCK_REALTIME,&time_info_current) == 0) //success
    {
        location.timestamp = (time_info_current.tv_sec)*1e3 +
            (time_info_current.tv_nsec)/1e6;
    }

    //Use this bit to indicate the injected position source is NLP
    location.techMask |= LOCATION_TECHNOLOGY_WIFI_BIT;
    injectPosition(location, onDemandCpi);
}

void LocApiV02::injectPosition(const Location& location, bool onDemandCpi)
{
    sendMsg(new LocApiMsg([this, location, onDemandCpi] () {

    qmiLocInjectPositionReqMsgT_v02 injectPositionReq;
    memset(&injectPositionReq, 0, sizeof(injectPositionReq));

    if (location.timestamp > 0) {
        injectPositionReq.timestampUtc_valid = 1;
        injectPositionReq.timestampUtc = location.timestamp;
    }

    if (LOCATION_HAS_LAT_LONG_BIT & location.flags) {
        injectPositionReq.latitude_valid = 1;
        injectPositionReq.longitude_valid = 1;
        injectPositionReq.latitude = location.latitude;
        injectPositionReq.longitude = location.longitude;
    }

    if (LOCATION_HAS_ACCURACY_BIT & location.flags) {
        injectPositionReq.horUncCircular_valid = 1;
        injectPositionReq.horUncCircular = location.accuracy;
        injectPositionReq.horConfidence_valid = 1;
        injectPositionReq.horConfidence = 68;
        injectPositionReq.rawHorUncCircular_valid = 1;
        injectPositionReq.rawHorUncCircular = location.accuracy;
        injectPositionReq.rawHorConfidence_valid = 1;
        injectPositionReq.rawHorConfidence = 68;

        // We don't wish to advertise accuracy better than 1000 meters to Modem
        if (injectPositionReq.horUncCircular < 1000) {
            injectPositionReq.horUncCircular = 1000;
        }
    }

    if (LOCATION_HAS_ALTITUDE_BIT & location.flags) {
        injectPositionReq.altitudeWrtEllipsoid_valid = 1;
        injectPositionReq.altitudeWrtEllipsoid = location.altitude;
        injectPositionReq.altSourceInfo_valid = 1;
        injectPositionReq.altSourceInfo.source = eQMI_LOC_ALT_SRC_OTHER_V02;
        injectPositionReq.altSourceInfo.linkage = eQMI_LOC_ALT_SRC_LINKAGE_FULLY_INTERDEPENDENT_V02;
    }

    if (LOCATION_HAS_VERTICAL_ACCURACY_BIT & location.flags) {
        injectPositionReq.vertUnc_valid = 1;
        injectPositionReq.vertUnc = location.verticalAccuracy;
        injectPositionReq.vertConfidence_valid = 1;
        injectPositionReq.vertConfidence = 68;
    }

    injectPositionReq.positionSrc_valid = 1;
    if (LOCATION_TECHNOLOGY_WIFI_BIT & location.techMask) {
        injectPositionReq.positionSrc = eQMI_LOC_POSITION_SRC_WIFI_V02;
    } else {
        injectPositionReq.positionSrc = eQMI_LOC_POSITION_SRC_OTHER_V02;
    }

    if (onDemandCpi) {
        injectPositionReq.onDemandCpi_valid = 1;
        injectPositionReq.onDemandCpi = 1;
    }

    LOC_LOGv("Lat=%lf, Lon=%lf, Acc=%.2lf rawAcc=%.2lf horConfidence=%d"
             "rawHorConfidence=%d onDemandCpi=%d",
             injectPositionReq.latitude, injectPositionReq.longitude,
             injectPositionReq.horUncCircular, injectPositionReq.rawHorUncCircular,
             injectPositionReq.horConfidence, injectPositionReq.rawHorConfidence,
             injectPositionReq.onDemandCpi);

    LOC_SEND_SYNC_REQ(InjectPosition, INJECT_POSITION, injectPositionReq);

    }));
}

void LocApiV02::injectPosition(const GnssLocationInfoNotification &locationInfo, bool onDemandCpi)
{
    sendMsg(new LocApiMsg([this, locationInfo, onDemandCpi] () {

    qmiLocInjectPositionReqMsgT_v02 injectPositionReq;
    memset(&injectPositionReq, 0, sizeof(injectPositionReq));
    const Location &location = locationInfo.location;

    if (location.timestamp > 0) {
        injectPositionReq.timestampUtc_valid = 1;
        injectPositionReq.timestampUtc = location.timestamp;
    }

    if (LOCATION_HAS_LAT_LONG_BIT & location.flags) {
        injectPositionReq.latitude_valid = 1;
        injectPositionReq.longitude_valid = 1;
        injectPositionReq.latitude = location.latitude;
        injectPositionReq.longitude = location.longitude;
    }

    if (LOCATION_HAS_ACCURACY_BIT & location.flags) {
        injectPositionReq.horUncCircular_valid = 1;
        injectPositionReq.horUncCircular = location.accuracy;
        injectPositionReq.horConfidence_valid = 1;
        injectPositionReq.horConfidence = 68;
        injectPositionReq.rawHorUncCircular_valid = 1;
        injectPositionReq.rawHorUncCircular = location.accuracy;
        injectPositionReq.rawHorConfidence_valid = 1;
        injectPositionReq.rawHorConfidence = 68;

        // We don't wish to advertise accuracy better than 1000 meters to Modem
        if (injectPositionReq.horUncCircular < 1000) {
            injectPositionReq.horUncCircular = 1000;
        }
    }

    if (LOCATION_HAS_ALTITUDE_BIT & location.flags) {
        injectPositionReq.altitudeWrtEllipsoid_valid = 1;
        injectPositionReq.altitudeWrtEllipsoid = location.altitude;
    }

    if (LOCATION_HAS_VERTICAL_ACCURACY_BIT & location.flags) {
        injectPositionReq.vertUnc_valid = 1;
        injectPositionReq.vertUnc = location.verticalAccuracy;
        injectPositionReq.vertConfidence_valid = 1;
        injectPositionReq.vertConfidence = 68;
    }

    // GPS time
    if (locationInfo.gnssSystemTime.gnssSystemTimeSrc == GNSS_LOC_SV_SYSTEM_GPS) {
        injectPositionReq.gpsTime_valid = 1;
        injectPositionReq.gpsTime.gpsWeek =
                locationInfo.gnssSystemTime.u.gpsSystemTime.systemWeek;
        injectPositionReq.gpsTime.gpsTimeOfWeekMs =
                locationInfo.gnssSystemTime.u.gpsSystemTime.systemMsec;
    } else if (locationInfo.gnssSystemTime.gnssSystemTimeSrc == GNSS_LOC_SV_SYSTEM_GALILEO) {
        injectPositionReq.gpsTime_valid = 1;
        injectPositionReq.gpsTime.gpsWeek =
                locationInfo.gnssSystemTime.u.galSystemTime.systemWeek;
        injectPositionReq.gpsTime.gpsTimeOfWeekMs =
                locationInfo.gnssSystemTime.u.gpsSystemTime.systemMsec;
    } else if (locationInfo.gnssSystemTime.gnssSystemTimeSrc == GNSS_LOC_SV_SYSTEM_BDS) {
        injectPositionReq.gpsTime_valid = 1;
        injectPositionReq.gpsTime.gpsWeek =
                locationInfo.gnssSystemTime.u.bdsSystemTime.systemWeek;
        injectPositionReq.gpsTime.gpsTimeOfWeekMs =
                locationInfo.gnssSystemTime.u.bdsSystemTime.systemMsec;
    } else if (locationInfo.gnssSystemTime.gnssSystemTimeSrc == GNSS_LOC_SV_SYSTEM_QZSS) {
        injectPositionReq.gpsTime_valid = 1;
        injectPositionReq.gpsTime.gpsWeek =
                locationInfo.gnssSystemTime.u.qzssSystemTime.systemWeek;
        injectPositionReq.gpsTime.gpsTimeOfWeekMs =
                locationInfo.gnssSystemTime.u.qzssSystemTime.systemMsec;
     } else if (locationInfo.gnssSystemTime.gnssSystemTimeSrc == GNSS_LOC_SV_SYSTEM_GLONASS) {
         if (GNSS_LOCATION_INFO_LEAP_SECONDS_BIT & locationInfo.flags) {
             const GnssGloTimeStructType &gloSystemTime =
                     locationInfo.gnssSystemTime.u.gloSystemTime;
             unsigned long long msecTotal =
                     gloSystemTime.gloFourYear * GLONASS_DAYS_IN_4YEARS * DAY_MSECS +
                     gloSystemTime.gloDays * DAY_MSECS +
                     gloSystemTime.gloMsec;
             // compensate the glonnass msec with difference between GPS time
             msecTotal += (GPS_GLONASS_DAYS_DIFF * DAY_MSECS -
                           GLONASS_UTC_OFFSET_HOURS * 3600 * 1000 +
                           locationInfo.leapSeconds * 1000);
             injectPositionReq.gpsTime_valid = 1;
             injectPositionReq.gpsTime.gpsWeek = msecTotal / WEEK_MSECS;
             injectPositionReq.gpsTime.gpsTimeOfWeekMs = msecTotal % WEEK_MSECS;
         }
    } else if (locationInfo.gnssSystemTime.gnssSystemTimeSrc == GNSS_LOC_SV_SYSTEM_NAVIC) {
        injectPositionReq.gpsTime_valid = 1;
        injectPositionReq.gpsTime.gpsWeek =
                locationInfo.gnssSystemTime.u.navicSystemTime.systemWeek;
        injectPositionReq.gpsTime.gpsTimeOfWeekMs =
                locationInfo.gnssSystemTime.u.navicSystemTime.systemMsec;
    }

    // velocity enu
    if ((GNSS_LOCATION_INFO_EAST_VEL_BIT & locationInfo.flags) &&
        (GNSS_LOCATION_INFO_NORTH_VEL_BIT & locationInfo.flags) &&
        (GNSS_LOCATION_INFO_UP_VEL_BIT & locationInfo.flags)) {
        injectPositionReq.velEnu_valid = 1;
        injectPositionReq.velEnu[0] = locationInfo.eastVelocity;
        injectPositionReq.velEnu[1] = locationInfo.northVelocity;
        injectPositionReq.velEnu[2] = locationInfo.upVelocity;
    }

    // velocity uncertainty enu
    if ((GNSS_LOCATION_INFO_EAST_VEL_UNC_BIT & locationInfo.flags) &&
        (GNSS_LOCATION_INFO_NORTH_VEL_UNC_BIT & locationInfo.flags) &&
        (GNSS_LOCATION_INFO_UP_VEL_UNC_BIT & locationInfo.flags)) {
        injectPositionReq.velUncEnu_valid = 1;
        injectPositionReq.velUncEnu[0] = locationInfo.eastVelocityStdDeviation;
        injectPositionReq.velUncEnu[1] = locationInfo.northVelocityStdDeviation;
        injectPositionReq.velUncEnu[2]  = locationInfo.upVelocityStdDeviation;
    }

    // time uncertainty
    if (GNSS_LOCATION_INFO_TIME_UNC_BIT & locationInfo.flags) {
        injectPositionReq.timeUnc_valid = 1;
        injectPositionReq.timeUnc = locationInfo.timeUncMs;
    }

    // number of SV used info, this replaces expandedGnssSvUsedList as the payload
    // for expandedGnssSvUsedList is too large
    if (GNSS_LOCATION_INFO_NUM_SV_USED_IN_POSITION_BIT & locationInfo.flags) {
        injectPositionReq.numSvInFix = locationInfo.numSvUsedInPosition;
        injectPositionReq.numSvInFix_valid = 1;
    }

    // mark fix as from DRE for modem to distinguish from others
    if (LOCATION_TECHNOLOGY_SENSORS_BIT & locationInfo.location.techMask) {
        injectPositionReq.positionSrc_valid = 1;
        injectPositionReq.positionSrc = eQMI_LOC_POSITION_SRC_DRE_V02;
    }

    if (onDemandCpi) {
        injectPositionReq.onDemandCpi_valid = 1;
        injectPositionReq.onDemandCpi = 1;
    }

    LOC_LOGv("Lat=%lf, Lon=%lf, Acc=%.2lf rawAcc=%.2lf horConfidence=%d"
             "rawHorConfidence=%d onDemandCpi=%d, position src=%d, numSVs=%d",
             injectPositionReq.latitude, injectPositionReq.longitude,
             injectPositionReq.horUncCircular, injectPositionReq.rawHorUncCircular,
             injectPositionReq.horConfidence, injectPositionReq.rawHorConfidence,
             injectPositionReq.onDemandCpi, injectPositionReq.positionSrc,
             injectPositionReq.numSvInFix);

    LOC_SEND_SYNC_REQ(InjectPosition, INJECT_POSITION, injectPositionReq);

    }));
}

/* delete assistance date */
void
LocApiV02::deleteAidingData(const GnssAidingData& data, LocApiResponse *adapterResponse)
{
  sendMsg(new LocApiMsg([this, data, adapterResponse] () {

  static bool isNewApiSupported = true;
  locClientReqUnionType req_union;
  locClientStatusEnumType status = eLOC_CLIENT_FAILURE_UNSUPPORTED;
  LocationError err = LOCATION_ERROR_SUCCESS;

  // Use the new API first
  qmiLocDeleteGNSSServiceDataReqMsgT_v02 delete_gnss_req;
  qmiLocDeleteGNSSServiceDataIndMsgT_v02 delete_gnss_resp;

  memset(&delete_gnss_req, 0, sizeof(delete_gnss_req));
  memset(&delete_gnss_resp, 0, sizeof(delete_gnss_resp));

  if (isNewApiSupported) {
      if (data.deleteAll) {
          delete_gnss_req.deleteAllFlag = true;
      } else {
          if (GNSS_AIDING_DATA_SV_EPHEMERIS_BIT & data.sv.svMask) {
              delete_gnss_req.deleteSatelliteData_valid = 1;
              delete_gnss_req.deleteSatelliteData.deleteSatelliteDataMask |=
                  QMI_LOC_DELETE_DATA_MASK_EPHEMERIS_V02;
          }
          if (GNSS_AIDING_DATA_SV_ALMANAC_BIT & data.sv.svMask) {
              delete_gnss_req.deleteSatelliteData_valid = 1;
              delete_gnss_req.deleteSatelliteData.deleteSatelliteDataMask |=
                  QMI_LOC_DELETE_DATA_MASK_ALMANAC_V02;
          }
          if (GNSS_AIDING_DATA_SV_HEALTH_BIT & data.sv.svMask) {
              delete_gnss_req.deleteSatelliteData_valid = 1;
              delete_gnss_req.deleteSatelliteData.deleteSatelliteDataMask |=
                  QMI_LOC_DELETE_DATA_MASK_SVHEALTH_V02;
          }
          if (GNSS_AIDING_DATA_SV_DIRECTION_BIT & data.sv.svMask) {
              delete_gnss_req.deleteSatelliteData_valid = 1;
              delete_gnss_req.deleteSatelliteData.deleteSatelliteDataMask |=
                  QMI_LOC_DELETE_DATA_MASK_SVDIR_V02;
          }
          if (GNSS_AIDING_DATA_SV_STEER_BIT & data.sv.svMask) {
              delete_gnss_req.deleteSatelliteData_valid = 1;
              delete_gnss_req.deleteSatelliteData.deleteSatelliteDataMask |=
                  QMI_LOC_DELETE_DATA_MASK_SVSTEER_V02;
          }
          if (GNSS_AIDING_DATA_SV_ALMANAC_CORR_BIT & data.sv.svMask) {
              delete_gnss_req.deleteSatelliteData_valid = 1;
              delete_gnss_req.deleteSatelliteData.deleteSatelliteDataMask |=
                  QMI_LOC_DELETE_DATA_MASK_ALM_CORR_V02;
          }
          if (GNSS_AIDING_DATA_SV_BLACKLIST_BIT & data.sv.svMask) {
              delete_gnss_req.deleteSatelliteData_valid = 1;
              delete_gnss_req.deleteSatelliteData.deleteSatelliteDataMask |=
                  QMI_LOC_DELETE_DATA_MASK_BLACKLIST_V02;
          }
          if (GNSS_AIDING_DATA_SV_SA_DATA_BIT & data.sv.svMask) {
              delete_gnss_req.deleteSatelliteData_valid = 1;
              delete_gnss_req.deleteSatelliteData.deleteSatelliteDataMask |=
                  QMI_LOC_DELETE_DATA_MASK_SA_DATA_V02;
          }
          if (GNSS_AIDING_DATA_SV_NO_EXIST_BIT & data.sv.svMask) {
              delete_gnss_req.deleteSatelliteData_valid = 1;
              delete_gnss_req.deleteSatelliteData.deleteSatelliteDataMask |=
                  QMI_LOC_DELETE_DATA_MASK_SV_NO_EXIST_V02;
          }
          if (GNSS_AIDING_DATA_SV_IONOSPHERE_BIT & data.sv.svMask) {
              delete_gnss_req.deleteSatelliteData_valid = 1;
              delete_gnss_req.deleteSatelliteData.deleteSatelliteDataMask |=
                  QMI_LOC_DELETE_DATA_MASK_IONO_V02;
          }
          if (GNSS_AIDING_DATA_SV_TIME_BIT & data.sv.svMask) {
              delete_gnss_req.deleteSatelliteData_valid = 1;
              delete_gnss_req.deleteSatelliteData.deleteSatelliteDataMask |=
                  QMI_LOC_DELETE_DATA_MASK_TIME_V02;
          }
          if (GNSS_AIDING_DATA_SV_MB_DATA & data.sv.svMask) {
              delete_gnss_req.deleteSatelliteData_valid = 1;
              delete_gnss_req.deleteSatelliteData.deleteSatelliteDataMask |=
                  QMI_LOC_DELETE_DATA_MASK_MB_DATA_V02;
          }
          if (delete_gnss_req.deleteSatelliteData_valid) {
              if (GNSS_AIDING_DATA_SV_TYPE_GPS_BIT & data.sv.svTypeMask) {
                  delete_gnss_req.deleteSatelliteData.system |= QMI_LOC_SYSTEM_GPS_V02;
              }
              if (GNSS_AIDING_DATA_SV_TYPE_GLONASS_BIT & data.sv.svTypeMask) {
                  delete_gnss_req.deleteSatelliteData.system |= QMI_LOC_SYSTEM_GLO_V02;
              }
              if (GNSS_AIDING_DATA_SV_TYPE_QZSS_BIT & data.sv.svTypeMask) {
                  delete_gnss_req.deleteSatelliteData.system |= QMI_LOC_SYSTEM_BDS_V02;
              }
              if (GNSS_AIDING_DATA_SV_TYPE_BEIDOU_BIT & data.sv.svTypeMask) {
                  delete_gnss_req.deleteSatelliteData.system |= QMI_LOC_SYSTEM_GAL_V02;
              }
              if (GNSS_AIDING_DATA_SV_TYPE_GALILEO_BIT & data.sv.svTypeMask) {
                  delete_gnss_req.deleteSatelliteData.system |= QMI_LOC_SYSTEM_QZSS_V02;
              }
              if (GNSS_AIDING_DATA_SV_TYPE_NAVIC_BIT & data.sv.svTypeMask) {
                  delete_gnss_req.deleteSatelliteData.system |= QMI_LOC_SYSTEM_NAVIC_V02;
              }
          }

          if (GNSS_AIDING_DATA_COMMON_POSITION_BIT & data.common.mask) {
              delete_gnss_req.deleteCommonDataMask_valid = 1;
              delete_gnss_req.deleteCommonDataMask |= QMI_LOC_DELETE_COMMON_MASK_POS_V02;
          }
          if (GNSS_AIDING_DATA_COMMON_TIME_BIT & data.common.mask) {
              delete_gnss_req.deleteCommonDataMask_valid = 1;
              delete_gnss_req.deleteCommonDataMask |= QMI_LOC_DELETE_COMMON_MASK_TIME_V02;
          }
          if (GNSS_AIDING_DATA_COMMON_UTC_BIT & data.common.mask) {
              delete_gnss_req.deleteCommonDataMask_valid = 1;
              delete_gnss_req.deleteCommonDataMask |= QMI_LOC_DELETE_COMMON_MASK_UTC_V02;
          }
          if (GNSS_AIDING_DATA_COMMON_RTI_BIT & data.common.mask) {
              delete_gnss_req.deleteCommonDataMask_valid = 1;
              delete_gnss_req.deleteCommonDataMask |= QMI_LOC_DELETE_COMMON_MASK_RTI_V02;
          }
          if (GNSS_AIDING_DATA_COMMON_FREQ_BIAS_EST_BIT & data.common.mask) {
              delete_gnss_req.deleteCommonDataMask_valid = 1;
              delete_gnss_req.deleteCommonDataMask |= QMI_LOC_DELETE_COMMON_MASK_FREQ_BIAS_EST_V02;
          }
          if (GNSS_AIDING_DATA_COMMON_CELLDB_BIT & data.common.mask) {
              delete_gnss_req.deleteCellDbDataMask_valid = 1;
              delete_gnss_req.deleteCellDbDataMask =
                  (QMI_LOC_MASK_DELETE_CELLDB_POS_V02 |
                      QMI_LOC_MASK_DELETE_CELLDB_LATEST_GPS_POS_V02 |
                      QMI_LOC_MASK_DELETE_CELLDB_OTA_POS_V02 |
                      QMI_LOC_MASK_DELETE_CELLDB_EXT_REF_POS_V02 |
                      QMI_LOC_MASK_DELETE_CELLDB_TIMETAG_V02 |
                      QMI_LOC_MASK_DELETE_CELLDB_CELLID_V02 |
                      QMI_LOC_MASK_DELETE_CELLDB_CACHED_CELLID_V02 |
                      QMI_LOC_MASK_DELETE_CELLDB_LAST_SRV_CELL_V02 |
                      QMI_LOC_MASK_DELETE_CELLDB_CUR_SRV_CELL_V02 |
                      QMI_LOC_MASK_DELETE_CELLDB_NEIGHBOR_INFO_V02);
          }
      }

      req_union.pDeleteGNSSServiceDataReq = &delete_gnss_req;

      status = locSyncSendReq(QMI_LOC_DELETE_GNSS_SERVICE_DATA_REQ_V02,
                              req_union, LOC_ENGINE_SYNC_REQUEST_TIMEOUT,
                              QMI_LOC_DELETE_GNSS_SERVICE_DATA_IND_V02,
                              &delete_gnss_resp);

      if (status != eLOC_CLIENT_SUCCESS ||
          eQMI_LOC_SUCCESS_V02 != delete_gnss_resp.status)
      {
          LOC_LOGE("%s:%d]: error! status = %s, delete_resp.status = %s\n",
              __func__, __LINE__,
              loc_get_v02_client_status_name(status),
              loc_get_v02_qmi_status_name(delete_gnss_resp.status));
      }
  }

  if (eLOC_CLIENT_FAILURE_UNSUPPORTED == status ||
      eLOC_CLIENT_FAILURE_INTERNAL == status) {
      // If the new API is not supported we fall back on the old one
      // The error could be eLOC_CLIENT_FAILURE_INTERNAL if
      // QMI_LOC_DELETE_GNSS_SERVICE_DATA_REQ_V02 is not in the .idl file
      LOC_LOGD("%s:%d]: QMI_LOC_DELETE_GNSS_SERVICE_DATA_REQ_V02 not supported"
          "We use QMI_LOC_DELETE_ASSIST_DATA_REQ_V02\n",
          __func__, __LINE__);
      isNewApiSupported = false;

      qmiLocDeleteAssistDataReqMsgT_v02 delete_req;
      qmiLocDeleteAssistDataIndMsgT_v02 delete_resp;

      memset(&delete_req, 0, sizeof(delete_req));
      memset(&delete_resp, 0, sizeof(delete_resp));

      if (data.deleteAll) {
          delete_req.deleteAllFlag = true;
      } else {
          /* to keep track of svInfoList for GPS and GLO*/
          uint32_t curr_sv_len = 0;
          uint32_t curr_sv_idx = 0;
          uint32_t sv_id = 0;

          if ((GNSS_AIDING_DATA_SV_EPHEMERIS_BIT & data.sv.svMask ||
              GNSS_AIDING_DATA_SV_ALMANAC_BIT & data.sv.svMask) &&
              GNSS_AIDING_DATA_SV_TYPE_GPS_BIT & data.sv.svTypeMask) {

              /* do delete for all GPS SV's */
              curr_sv_len += SV_ID_RANGE;

              sv_id = GPS_SV_ID_OFFSET;

              delete_req.deleteSvInfoList_valid = 1;

              delete_req.deleteSvInfoList_len = curr_sv_len;

              LOC_LOGV("%s:%d]: Delete GPS SV info for index %d to %d"
                  "and sv id %d to %d \n",
                  __func__, __LINE__, curr_sv_idx, curr_sv_len - 1,
                  sv_id, sv_id + SV_ID_RANGE - 1);

              for (uint32_t i = curr_sv_idx; i < curr_sv_len; i++, sv_id++) {
                  delete_req.deleteSvInfoList[i].gnssSvId = sv_id;

                  delete_req.deleteSvInfoList[i].system = eQMI_LOC_SV_SYSTEM_GPS_V02;

                  if (GNSS_AIDING_DATA_SV_EPHEMERIS_BIT & data.sv.svMask) {
                      // set ephemeris mask for all GPS SV's
                      delete_req.deleteSvInfoList[i].deleteSvInfoMask |=
                          QMI_LOC_MASK_DELETE_EPHEMERIS_V02;
                  }

                  if (GNSS_AIDING_DATA_SV_ALMANAC_BIT & data.sv.svMask) {
                      delete_req.deleteSvInfoList[i].deleteSvInfoMask |=
                          QMI_LOC_MASK_DELETE_ALMANAC_V02;
                  }
              }
              // increment the current index
              curr_sv_idx += SV_ID_RANGE;

          }

          if (GNSS_AIDING_DATA_COMMON_POSITION_BIT & data.common.mask) {
              delete_req.deleteGnssDataMask_valid = 1;
              delete_req.deleteGnssDataMask |= QMI_LOC_MASK_DELETE_POSITION_V02;
          }
          if (GNSS_AIDING_DATA_COMMON_TIME_BIT & data.common.mask) {
              delete_req.deleteGnssDataMask_valid = 1;
              delete_req.deleteGnssDataMask |= QMI_LOC_MASK_DELETE_TIME_V02;
          }
          if ((GNSS_AIDING_DATA_SV_IONOSPHERE_BIT & data.sv.svMask) &&
              (GNSS_AIDING_DATA_SV_TYPE_GPS_BIT & data.sv.svTypeMask)) {
              delete_req.deleteGnssDataMask_valid = 1;
              delete_req.deleteGnssDataMask |= QMI_LOC_MASK_DELETE_IONO_V02;
          }
          if (GNSS_AIDING_DATA_COMMON_UTC_BIT & data.common.mask)
          {
              delete_req.deleteGnssDataMask_valid = 1;
              delete_req.deleteGnssDataMask |= QMI_LOC_MASK_DELETE_UTC_V02;
          }
          if ((GNSS_AIDING_DATA_SV_HEALTH_BIT & data.sv.svMask) &&
              (GNSS_AIDING_DATA_SV_TYPE_GPS_BIT & data.sv.svTypeMask)) {
              delete_req.deleteGnssDataMask_valid = 1;
              delete_req.deleteGnssDataMask |= QMI_LOC_MASK_DELETE_HEALTH_V02;
          }
          if ((GNSS_AIDING_DATA_SV_DIRECTION_BIT & data.sv.svMask) &&
              (GNSS_AIDING_DATA_SV_TYPE_GPS_BIT & data.sv.svTypeMask)) {
              delete_req.deleteGnssDataMask_valid = 1;
              delete_req.deleteGnssDataMask |= QMI_LOC_MASK_DELETE_GPS_SVDIR_V02;
          }
          if ((GNSS_AIDING_DATA_SV_SA_DATA_BIT & data.sv.svMask) &&
              (GNSS_AIDING_DATA_SV_TYPE_GPS_BIT & data.sv.svTypeMask)) {
              delete_req.deleteGnssDataMask_valid = 1;
              delete_req.deleteGnssDataMask |= QMI_LOC_MASK_DELETE_SADATA_V02;
          }
          if (GNSS_AIDING_DATA_COMMON_RTI_BIT & data.common.mask) {
              delete_req.deleteGnssDataMask_valid = 1;
              delete_req.deleteGnssDataMask |= QMI_LOC_MASK_DELETE_RTI_V02;
          }
          if (GNSS_AIDING_DATA_COMMON_CELLDB_BIT & data.common.mask) {
              delete_req.deleteCellDbDataMask_valid = 1;
              delete_req.deleteCellDbDataMask =
                  (QMI_LOC_MASK_DELETE_CELLDB_POS_V02 |
                      QMI_LOC_MASK_DELETE_CELLDB_LATEST_GPS_POS_V02 |
                      QMI_LOC_MASK_DELETE_CELLDB_OTA_POS_V02 |
                      QMI_LOC_MASK_DELETE_CELLDB_EXT_REF_POS_V02 |
                      QMI_LOC_MASK_DELETE_CELLDB_TIMETAG_V02 |
                      QMI_LOC_MASK_DELETE_CELLDB_CELLID_V02 |
                      QMI_LOC_MASK_DELETE_CELLDB_CACHED_CELLID_V02 |
                      QMI_LOC_MASK_DELETE_CELLDB_LAST_SRV_CELL_V02 |
                      QMI_LOC_MASK_DELETE_CELLDB_CUR_SRV_CELL_V02 |
                      QMI_LOC_MASK_DELETE_CELLDB_NEIGHBOR_INFO_V02);

          }
      }

      req_union.pDeleteAssistDataReq = &delete_req;

      status = locSyncSendReq(QMI_LOC_DELETE_ASSIST_DATA_REQ_V02,
                              req_union, LOC_ENGINE_SYNC_REQUEST_TIMEOUT,
                              QMI_LOC_DELETE_ASSIST_DATA_IND_V02,
                              &delete_resp);

      if (status != eLOC_CLIENT_SUCCESS ||
          eQMI_LOC_SUCCESS_V02 != delete_resp.status)
      {
          LOC_LOGE("%s:%d]: error! status = %s, delete_resp.status = %s\n",
              __func__, __LINE__,
              loc_get_v02_client_status_name(status),
              loc_get_v02_qmi_status_name(delete_resp.status));
          err = LOCATION_ERROR_GENERAL_FAILURE;
      }
  }

  if (adapterResponse != NULL) {
      adapterResponse->returnToSender(err);
  }
  }));
}

/* send NI user repsonse to the engine */
void
LocApiV02::informNiResponse(GnssNiResponse userResponse, const void* passThroughData)
{
    sendMsg(new LocApiMsg([this, userResponse, passThroughData] () {

        LocationError err = LOCATION_ERROR_SUCCESS;
        locClientReqUnionType req_union;
        locClientStatusEnumType status;
        qmiLocNiUserRespReqMsgT_v02 ni_resp;
        qmiLocNiUserRespIndMsgT_v02 ni_resp_ind;

        qmiLocEventNiNotifyVerifyReqIndMsgT_v02 *request_pass_back =
            (qmiLocEventNiNotifyVerifyReqIndMsgT_v02 *)passThroughData;

        memset(&ni_resp,0, sizeof(ni_resp));

        memset(&ni_resp_ind,0, sizeof(ni_resp_ind));

        switch (userResponse) {
        case GNSS_NI_RESPONSE_ACCEPT:
            ni_resp.userResp = eQMI_LOC_NI_LCS_NOTIFY_VERIFY_ACCEPT_V02;
            break;
        case GNSS_NI_RESPONSE_DENY:
            ni_resp.userResp = eQMI_LOC_NI_LCS_NOTIFY_VERIFY_DENY_V02;
            break;
        case GNSS_NI_RESPONSE_NO_RESPONSE:
            ni_resp.userResp = eQMI_LOC_NI_LCS_NOTIFY_VERIFY_NORESP_V02;
            break;
        default:
            err = LOCATION_ERROR_INVALID_PARAMETER;
            free((void *)passThroughData);
            return;
        }

        LOC_LOGv("NI response: %d", ni_resp.userResp);

        ni_resp.notificationType = request_pass_back->notificationType;

        // copy SUPL payload from request
        if (1 == request_pass_back->NiSuplInd_valid) {
            ni_resp.NiSuplPayload_valid = 1;
            memcpy(&(ni_resp.NiSuplPayload),
                   &(request_pass_back->NiSuplInd),
                   sizeof(qmiLocNiSuplNotifyVerifyStructT_v02));
        }
        // should this be an "else if"?? we don't need to decide

        // copy UMTS-CP payload from request
        if (1 == request_pass_back->NiUmtsCpInd_valid) {
            ni_resp.NiUmtsCpPayload_valid = 1;
            memcpy(&(ni_resp.NiUmtsCpPayload),
                   &(request_pass_back->NiUmtsCpInd),
                   sizeof(qmiLocNiUmtsCpNotifyVerifyStructT_v02));
        }

        //copy Vx payload from the request
        if (1 == request_pass_back->NiVxInd_valid) {
            ni_resp.NiVxPayload_valid = 1;
            memcpy(&(ni_resp.NiVxPayload),
                   &(request_pass_back->NiVxInd),
                   sizeof(qmiLocNiVxNotifyVerifyStructT_v02));
        }

        // copy Vx service interaction payload from the request
        if (1 == request_pass_back->NiVxServiceInteractionInd_valid) {
            ni_resp.NiVxServiceInteractionPayload_valid = 1;
            memcpy(&(ni_resp.NiVxServiceInteractionPayload),
                   &(request_pass_back->NiVxServiceInteractionInd),
                   sizeof(qmiLocNiVxServiceInteractionStructT_v02));
        }

        // copy Network Initiated SUPL Version 2 Extension
        if (1 == request_pass_back->NiSuplVer2ExtInd_valid) {
            ni_resp.NiSuplVer2ExtPayload_valid = 1;
            memcpy(&(ni_resp.NiSuplVer2ExtPayload),
                   &(request_pass_back->NiSuplVer2ExtInd),
                   sizeof(qmiLocNiSuplVer2ExtStructT_v02));
        }

        // copy SUPL Emergency Notification
        if (request_pass_back->suplEmergencyNotification_valid) {
            ni_resp.suplEmergencyNotification_valid = 1;
            memcpy(&(ni_resp.suplEmergencyNotification),
                   &(request_pass_back->suplEmergencyNotification),
                   sizeof(qmiLocEmergencyNotificationStructT_v02));
        }

        req_union.pNiUserRespReq = &ni_resp;

        status = locSyncSendReq(QMI_LOC_NI_USER_RESPONSE_REQ_V02,
                                req_union, LOC_ENGINE_SYNC_REQUEST_TIMEOUT,
                                QMI_LOC_NI_USER_RESPONSE_IND_V02,
                                &ni_resp_ind);

        if (status != eLOC_CLIENT_SUCCESS ||
            eQMI_LOC_SUCCESS_V02 != ni_resp_ind.status) {

            LOC_LOGe("error! status = %s, ni_resp_ind.status = %s",
                     loc_get_v02_client_status_name(status),
                     loc_get_v02_qmi_status_name(ni_resp_ind.status));
            err = LOCATION_ERROR_GENERAL_FAILURE;
        }

        free((void *)passThroughData);
    }));
}

void
LocApiV02::registerMasterClient()
{
  LocationError err = LOCATION_ERROR_SUCCESS;
  locClientReqUnionType req_union;
  locClientStatusEnumType status;
  qmiLocRegisterMasterClientReqMsgT_v02 reg_master_client_req;
  qmiLocRegisterMasterClientIndMsgT_v02 reg_master_client_ind;

  memset(&reg_master_client_req, 0, sizeof(reg_master_client_req));
  memset(&reg_master_client_ind, 0, sizeof(reg_master_client_ind));

  reg_master_client_req.key = 0xBAABCDEF;

  req_union.pRegisterMasterClientReq = &reg_master_client_req;

  status = locSyncSendReq(QMI_LOC_REGISTER_MASTER_CLIENT_REQ_V02,
                          req_union, LOC_ENGINE_SYNC_REQUEST_TIMEOUT,
                          QMI_LOC_REGISTER_MASTER_CLIENT_IND_V02,
                          &reg_master_client_ind);

  if (eLOC_CLIENT_SUCCESS != status ||
      eQMI_LOC_REGISTER_MASTER_CLIENT_SUCCESS_V02 != reg_master_client_ind.status) {
    LOC_LOGw ("error status = %s, reg_master_client_ind.status = %s",
              loc_get_v02_client_status_name(status),
              loc_get_v02_qmi_reg_mk_status_name(reg_master_client_ind.status));
    if (eLOC_CLIENT_FAILURE_INVALID_MESSAGE_ID == status ||
        eLOC_CLIENT_FAILURE_UNSUPPORTED == status) {
      mMasterRegisterNotSupported = true;
    } else {
      mMasterRegisterNotSupported = false;
    }
    LOC_LOGv("mMasterRegisterNotSupported = %d", mMasterRegisterNotSupported);
    err = LOCATION_ERROR_GENERAL_FAILURE;
  }
}

/* Set UMTs SLP server URL */
LocationError
LocApiV02::setServerSync(const char* url, int len, LocServerType type)
{
  LocationError err = LOCATION_ERROR_SUCCESS;
  locClientReqUnionType req_union;
  locClientStatusEnumType status;
  qmiLocSetServerReqMsgT_v02 set_server_req;
  qmiLocSetServerIndMsgT_v02 set_server_ind;

  if(len < 0 || (size_t)len > sizeof(set_server_req.urlAddr))
  {
    LOC_LOGe("len = %d greater than max allowed url length", len);

    return LOCATION_ERROR_INVALID_PARAMETER;
  }

  memset(&set_server_req, 0, sizeof(set_server_req));
  memset(&set_server_ind, 0, sizeof(set_server_ind));

  LOC_LOGd("url = %s, len = %d type=%d", url, len, type);

  if (LOC_AGPS_MO_SUPL_SERVER == type) {
      set_server_req.serverType = eQMI_LOC_SERVER_TYPE_CUSTOM_SLP_V02;
  } else {
      set_server_req.serverType = eQMI_LOC_SERVER_TYPE_UMTS_SLP_V02;
  }

  set_server_req.urlAddr_valid = 1;

  strlcpy(set_server_req.urlAddr, url, sizeof(set_server_req.urlAddr));

  req_union.pSetServerReq = &set_server_req;

  status = locSyncSendReq(QMI_LOC_SET_SERVER_REQ_V02,
                          req_union, LOC_ENGINE_SYNC_REQUEST_LONG_TIMEOUT,
                          QMI_LOC_SET_SERVER_IND_V02,
                          &set_server_ind);

  if (status != eLOC_CLIENT_SUCCESS ||
         eQMI_LOC_SUCCESS_V02 != set_server_ind.status)
  {
    LOC_LOGe ("error status = %s, set_server_ind.status = %s",
              loc_get_v02_client_status_name(status),
              loc_get_v02_qmi_status_name(set_server_ind.status));
    err = LOCATION_ERROR_GENERAL_FAILURE;
  }

  return err;
}

LocationError
LocApiV02::setServerSync(unsigned int ip, int port, LocServerType type)
{
  LocationError err = LOCATION_ERROR_SUCCESS;
  locClientReqUnionType req_union;
  locClientStatusEnumType status;
  qmiLocSetServerReqMsgT_v02 set_server_req;
  qmiLocSetServerIndMsgT_v02 set_server_ind;
  qmiLocServerTypeEnumT_v02 set_server_cmd;

  switch (type) {
  case LOC_AGPS_MPC_SERVER:
      set_server_cmd = eQMI_LOC_SERVER_TYPE_CDMA_MPC_V02;
      break;
  case LOC_AGPS_CUSTOM_PDE_SERVER:
      set_server_cmd = eQMI_LOC_SERVER_TYPE_CUSTOM_PDE_V02;
      break;
  default:
      set_server_cmd = eQMI_LOC_SERVER_TYPE_CDMA_PDE_V02;
      break;
  }

  memset(&set_server_req, 0, sizeof(set_server_req));
  memset(&set_server_ind, 0, sizeof(set_server_ind));

  LOC_LOGD("%s:%d]:, ip = %u, port = %d\n", __func__, __LINE__, ip, port);

  set_server_req.serverType = set_server_cmd;
  set_server_req.ipv4Addr_valid = 1;
  set_server_req.ipv4Addr.addr = ip;
  set_server_req.ipv4Addr.port = port;

  req_union.pSetServerReq = &set_server_req;

  status = locSyncSendReq(QMI_LOC_SET_SERVER_REQ_V02,
                          req_union, LOC_ENGINE_SYNC_REQUEST_LONG_TIMEOUT,
                          QMI_LOC_SET_SERVER_IND_V02,
                          &set_server_ind);

  if (status != eLOC_CLIENT_SUCCESS ||
      eQMI_LOC_SUCCESS_V02 != set_server_ind.status)
  {
    LOC_LOGE ("%s:%d]: error status = %s, set_server_ind.status = %s\n",
              __func__,__LINE__,
              loc_get_v02_client_status_name(status),
              loc_get_v02_qmi_status_name(set_server_ind.status));
    err = LOCATION_ERROR_GENERAL_FAILURE;
  }

  return err;
}

void LocApiV02 :: atlOpenStatus(
  int handle, int is_succ, char* apn, uint32_t apnLen, AGpsBearerType bear,
  LocAGpsType /*agpsType*/, LocApnTypeMask apnTypeMask)
{
  sendMsg(new LocApiMsg([this, handle, is_succ,
                        apnStr=std::string(apn, apnLen),
                        bear, apnTypeMask] () {

  locClientStatusEnumType result = eLOC_CLIENT_SUCCESS;
  locClientReqUnionType req_union;
  qmiLocInformLocationServerConnStatusReqMsgT_v02 conn_status_req;
  qmiLocInformLocationServerConnStatusIndMsgT_v02 conn_status_ind;

  LOC_LOGd("ATL open handle = %d, is_succ = %d, APN = [%s], bearer = %d, "
           "apnTypeMask 0x%X" PRIx32, handle, is_succ, apnStr.c_str(),
           bear, apnTypeMask);

  memset(&conn_status_req, 0, sizeof(conn_status_req));
  memset(&conn_status_ind, 0, sizeof(conn_status_ind));

        // Fill in data
  conn_status_req.connHandle = handle;

  conn_status_req.requestType = eQMI_LOC_SERVER_REQUEST_OPEN_V02;

  if(is_succ)
  {
    conn_status_req.statusType = eQMI_LOC_SERVER_REQ_STATUS_SUCCESS_V02;

    if(!apnStr.empty()) {
        strlcpy(conn_status_req.apnProfile.apnName, apnStr.c_str(),
                sizeof(conn_status_req.apnProfile.apnName) );
    }

    switch(bear)
    {
    case AGPS_APN_BEARER_IPV4:
        conn_status_req.apnProfile.pdnType =
            eQMI_LOC_APN_PROFILE_PDN_TYPE_IPV4_V02;
        conn_status_req.apnProfile_valid = 1;
        break;

    case AGPS_APN_BEARER_IPV6:
        conn_status_req.apnProfile.pdnType =
            eQMI_LOC_APN_PROFILE_PDN_TYPE_IPV6_V02;
        conn_status_req.apnProfile_valid = 1;
        break;

    case AGPS_APN_BEARER_IPV4V6:
        conn_status_req.apnProfile.pdnType =
            eQMI_LOC_APN_PROFILE_PDN_TYPE_IPV4V6_V02;
        conn_status_req.apnProfile_valid = 1;
        break;

    case AGPS_APN_BEARER_INVALID:
        conn_status_req.apnProfile_valid = 0;
        break;

    default:
        LOC_LOGE("%s:%d]:invalid bearer type\n",__func__,__LINE__);
        conn_status_req.apnProfile_valid = 0;
        return;
    }

    // Populate apnTypeMask
    if (0 != apnTypeMask) {
        conn_status_req.apnTypeMask_valid = true;
        conn_status_req.apnTypeMask = convertLocApnTypeMask(apnTypeMask);
    }

  }
  else
  {
    conn_status_req.statusType = eQMI_LOC_SERVER_REQ_STATUS_FAILURE_V02;
  }

  req_union.pInformLocationServerConnStatusReq = &conn_status_req;

  result = locSyncSendReq(QMI_LOC_INFORM_LOCATION_SERVER_CONN_STATUS_REQ_V02,
                          req_union, LOC_ENGINE_SYNC_REQUEST_TIMEOUT,
                          QMI_LOC_INFORM_LOCATION_SERVER_CONN_STATUS_IND_V02,
                          &conn_status_ind);

  if(result != eLOC_CLIENT_SUCCESS ||
     eQMI_LOC_SUCCESS_V02 != conn_status_ind.status)
  {
    LOC_LOGE ("%s:%d]: Error status = %s, ind..status = %s ",
              __func__, __LINE__,
              loc_get_v02_client_status_name(result),
              loc_get_v02_qmi_status_name(conn_status_ind.status));
  }

  }));
}


/* close atl connection */
void LocApiV02 :: atlCloseStatus(int handle, int is_succ)
{
  sendMsg(new LocApiMsg([this, handle, is_succ] () {

  locClientStatusEnumType result = eLOC_CLIENT_SUCCESS;
  locClientReqUnionType req_union;
  qmiLocInformLocationServerConnStatusReqMsgT_v02 conn_status_req;
  qmiLocInformLocationServerConnStatusIndMsgT_v02 conn_status_ind;

  LOC_LOGD("%s:%d]: ATL close handle = %d, is_succ = %d\n",
                 __func__, __LINE__,  handle, is_succ);

  memset(&conn_status_req, 0, sizeof(conn_status_req));
  memset(&conn_status_ind, 0, sizeof(conn_status_ind));

        // Fill in data
  conn_status_req.connHandle = handle;

  conn_status_req.requestType = eQMI_LOC_SERVER_REQUEST_CLOSE_V02;

  if(is_succ)
  {
    conn_status_req.statusType = eQMI_LOC_SERVER_REQ_STATUS_SUCCESS_V02;
  }
  else
  {
    conn_status_req.statusType = eQMI_LOC_SERVER_REQ_STATUS_FAILURE_V02;
  }

  req_union.pInformLocationServerConnStatusReq = &conn_status_req;

  result = locSyncSendReq(QMI_LOC_INFORM_LOCATION_SERVER_CONN_STATUS_REQ_V02,
                          req_union, LOC_ENGINE_SYNC_REQUEST_TIMEOUT,
                          QMI_LOC_INFORM_LOCATION_SERVER_CONN_STATUS_IND_V02,
                          &conn_status_ind);

  if(result != eLOC_CLIENT_SUCCESS ||
     eQMI_LOC_SUCCESS_V02 != conn_status_ind.status)
  {
    LOC_LOGE ("%s:%d]: Error status = %s, ind..status = %s ",
              __func__, __LINE__,
              loc_get_v02_client_status_name(result),
              loc_get_v02_qmi_status_name(conn_status_ind.status));
  }
  }));
}

/* set the SUPL version */
LocationError
LocApiV02::setSUPLVersionSync(GnssConfigSuplVersion version)
{
  LocationError err = LOCATION_ERROR_SUCCESS;
  locClientStatusEnumType result = eLOC_CLIENT_SUCCESS;
  locClientReqUnionType req_union;

  qmiLocSetProtocolConfigParametersReqMsgT_v02 supl_config_req;
  qmiLocSetProtocolConfigParametersIndMsgT_v02 supl_config_ind;

  LOC_LOGD("%s:%d]: supl version = %d\n",  __func__, __LINE__, version);


  memset(&supl_config_req, 0, sizeof(supl_config_req));
  memset(&supl_config_ind, 0, sizeof(supl_config_ind));

  supl_config_req.suplVersion_valid = 1;

  switch (version) {
    case GNSS_CONFIG_SUPL_VERSION_2_0_4:
      supl_config_req.suplVersion = eQMI_LOC_SUPL_VERSION_2_0_4_V02;
      break;
    case GNSS_CONFIG_SUPL_VERSION_2_0_2:
      supl_config_req.suplVersion = eQMI_LOC_SUPL_VERSION_2_0_2_V02;
      break;
    case GNSS_CONFIG_SUPL_VERSION_2_0_0:
      supl_config_req.suplVersion = eQMI_LOC_SUPL_VERSION_2_0_V02;
      break;
    case GNSS_CONFIG_SUPL_VERSION_1_0_0:
    default:
      supl_config_req.suplVersion =  eQMI_LOC_SUPL_VERSION_1_0_V02;
      break;
  }

  req_union.pSetProtocolConfigParametersReq = &supl_config_req;

  result = locSyncSendReq(QMI_LOC_SET_PROTOCOL_CONFIG_PARAMETERS_REQ_V02,
                          req_union, LOC_ENGINE_SYNC_REQUEST_TIMEOUT,
                          QMI_LOC_SET_PROTOCOL_CONFIG_PARAMETERS_IND_V02,
                          &supl_config_ind);

  if(result != eLOC_CLIENT_SUCCESS ||
     eQMI_LOC_SUCCESS_V02 != supl_config_ind.status)
  {
    LOC_LOGE ("%s:%d]: Error status = %s, ind..status = %s ",
              __func__, __LINE__,
              loc_get_v02_client_status_name(result),
              loc_get_v02_qmi_status_name(supl_config_ind.status));
     err = LOCATION_ERROR_GENERAL_FAILURE;
  }

  return err;
}

/* set the NMEA types mask */
enum loc_api_adapter_err LocApiV02 :: setNMEATypesSync(uint32_t typesMask)
{
  locClientStatusEnumType result = eLOC_CLIENT_SUCCESS;
  locClientReqUnionType req_union;

  qmiLocSetNmeaTypesReqMsgT_v02 setNmeaTypesReqMsg;
  qmiLocSetNmeaTypesIndMsgT_v02 setNmeaTypesIndMsg;

  LOC_LOGD(" %s:%d]: setNMEATypes, mask = 0x%X", __func__, __LINE__, typesMask);

  if (typesMask != mNmeaMask) {
      memset(&setNmeaTypesReqMsg, 0, sizeof(setNmeaTypesReqMsg));
      memset(&setNmeaTypesIndMsg, 0, sizeof(setNmeaTypesIndMsg));

      setNmeaTypesReqMsg.nmeaSentenceType = typesMask;

      req_union.pSetNmeaTypesReq = &setNmeaTypesReqMsg;

      LOC_LOGD(" %s:%d]: Setting mask = 0x%X", __func__, __LINE__, typesMask);
      result = locSyncSendReq(QMI_LOC_SET_NMEA_TYPES_REQ_V02,
                              req_union, LOC_ENGINE_SYNC_REQUEST_TIMEOUT,
                              QMI_LOC_SET_NMEA_TYPES_IND_V02,
                              &setNmeaTypesIndMsg);

      // if success
      if (result != eLOC_CLIENT_SUCCESS)
      {
          LOC_LOGE("%s:%d]: Error status = %s, ind..status = %s ",
                   __func__, __LINE__,
                   loc_get_v02_client_status_name(result),
                   loc_get_v02_qmi_status_name(setNmeaTypesIndMsg.status));
      }
      mNmeaMask = typesMask;
  }

  return convertErr(result);
}

/* set the configuration for LTE positioning profile (LPP) */
LocationError
LocApiV02::setLPPConfigSync(GnssConfigLppProfileMask profileMask)
{
  LocationError err = LOCATION_ERROR_SUCCESS;
  locClientStatusEnumType result = eLOC_CLIENT_SUCCESS;
  locClientReqUnionType req_union;
  qmiLocSetProtocolConfigParametersReqMsgT_v02 lpp_config_req;
  qmiLocSetProtocolConfigParametersIndMsgT_v02 lpp_config_ind;

  LOC_LOGD("%s:%d]: lpp profile = %u",  __func__, __LINE__, profileMask);

  memset(&lpp_config_req, 0, sizeof(lpp_config_req));
  memset(&lpp_config_ind, 0, sizeof(lpp_config_ind));

  lpp_config_req.lppConfig_valid = 1;
  if (profileMask & GNSS_CONFIG_LPP_PROFILE_USER_PLANE_BIT) {
      lpp_config_req.lppConfig |= QMI_LOC_LPP_CONFIG_ENABLE_USER_PLANE_V02;
  }
  if (profileMask & GNSS_CONFIG_LPP_PROFILE_CONTROL_PLANE_BIT) {
      lpp_config_req.lppConfig |= QMI_LOC_LPP_CONFIG_ENABLE_CONTROL_PLANE_V02;
  }
  if (profileMask & GNSS_CONFIG_LPP_PROFILE_USER_PLANE_OVER_NR5G_SA_BIT) {
      lpp_config_req.lppConfig |= QMI_LOC_LPP_CONFIG_ENABLE_USER_PLANE_OVER_NR5G_SA_V02;
  }
  if (profileMask & GNSS_CONFIG_LPP_PROFILE_CONTROL_PLANE_OVER_NR5G_SA_BIT) {
      lpp_config_req.lppConfig |= QMI_LOC_LPP_CONFIG_ENABLE_CONTROL_PLANE_OVER_NR5G_SA_V02;
  }

  req_union.pSetProtocolConfigParametersReq = &lpp_config_req;

  result = locSyncSendReq(QMI_LOC_SET_PROTOCOL_CONFIG_PARAMETERS_REQ_V02,
                          req_union, LOC_ENGINE_SYNC_REQUEST_LONG_TIMEOUT,
                          QMI_LOC_SET_PROTOCOL_CONFIG_PARAMETERS_IND_V02,
                          &lpp_config_ind);

  if(result != eLOC_CLIENT_SUCCESS ||
     eQMI_LOC_SUCCESS_V02 != lpp_config_ind.status)
  {
    LOC_LOGE ("%s:%d]: Error status = %s, ind..status = %s ",
              __func__, __LINE__,
              loc_get_v02_client_status_name(result),
              loc_get_v02_qmi_status_name(lpp_config_ind.status));
     err = LOCATION_ERROR_GENERAL_FAILURE;
  }

  return err;
}


/* set the Sensor Properties */
enum loc_api_adapter_err LocApiV02 :: setSensorPropertiesSync(
        bool gyroBiasVarianceRandomWalk_valid, float gyroBiasVarianceRandomWalk,
        bool accelBiasVarianceRandomWalk_valid, float accelBiasVarianceRandomWalk,
        bool angleBiasVarianceRandomWalk_valid, float angleBiasVarianceRandomWalk,
        bool rateBiasVarianceRandomWalk_valid, float rateBiasVarianceRandomWalk,
        bool velocityBiasVarianceRandomWalk_valid, float velocityBiasVarianceRandomWalk)
{
  locClientStatusEnumType result = eLOC_CLIENT_SUCCESS;
  locClientReqUnionType req_union;

  qmiLocSetSensorPropertiesReqMsgT_v02 sensor_prop_req;
  qmiLocSetSensorPropertiesIndMsgT_v02 sensor_prop_ind;

  LOC_LOGI("%s:%d]: sensors prop: gyroBiasRandomWalk = %f, accelRandomWalk = %f, "
           "angleRandomWalk = %f, rateRandomWalk = %f, velocityRandomWalk = %f\n",
                 __func__, __LINE__, gyroBiasVarianceRandomWalk, accelBiasVarianceRandomWalk,
           angleBiasVarianceRandomWalk, rateBiasVarianceRandomWalk, velocityBiasVarianceRandomWalk);

  memset(&sensor_prop_req, 0, sizeof(sensor_prop_req));
  memset(&sensor_prop_ind, 0, sizeof(sensor_prop_ind));

  /* Set the validity bit and value for each sensor property */
  sensor_prop_req.gyroBiasVarianceRandomWalk_valid = gyroBiasVarianceRandomWalk_valid;
  sensor_prop_req.gyroBiasVarianceRandomWalk = gyroBiasVarianceRandomWalk;

  sensor_prop_req.accelerationRandomWalkSpectralDensity_valid = accelBiasVarianceRandomWalk_valid;
  sensor_prop_req.accelerationRandomWalkSpectralDensity = accelBiasVarianceRandomWalk;

  sensor_prop_req.angleRandomWalkSpectralDensity_valid = angleBiasVarianceRandomWalk_valid;
  sensor_prop_req.angleRandomWalkSpectralDensity = angleBiasVarianceRandomWalk;

  sensor_prop_req.rateRandomWalkSpectralDensity_valid = rateBiasVarianceRandomWalk_valid;
  sensor_prop_req.rateRandomWalkSpectralDensity = rateBiasVarianceRandomWalk;

  sensor_prop_req.velocityRandomWalkSpectralDensity_valid = velocityBiasVarianceRandomWalk_valid;
  sensor_prop_req.velocityRandomWalkSpectralDensity = velocityBiasVarianceRandomWalk;

  req_union.pSetSensorPropertiesReq = &sensor_prop_req;

  result = locSyncSendReq(QMI_LOC_SET_SENSOR_PROPERTIES_REQ_V02,
                          req_union, LOC_ENGINE_SYNC_REQUEST_TIMEOUT,
                          QMI_LOC_SET_SENSOR_PROPERTIES_IND_V02,
                          &sensor_prop_ind);

  if(result != eLOC_CLIENT_SUCCESS ||
     eQMI_LOC_SUCCESS_V02 != sensor_prop_ind.status)
  {
    LOC_LOGE ("%s:%d]: Error status = %s, ind..status = %s ",
              __func__, __LINE__,
              loc_get_v02_client_status_name(result),
              loc_get_v02_qmi_status_name(sensor_prop_ind.status));
  }

  return convertErr(result);
}

/* set the Sensor Performance Config */
enum loc_api_adapter_err LocApiV02 :: setSensorPerfControlConfigSync(int controlMode,
        int accelSamplesPerBatch, int accelBatchesPerSec,
        int gyroSamplesPerBatch, int gyroBatchesPerSec,
        int accelSamplesPerBatchHigh, int accelBatchesPerSecHigh,
        int gyroSamplesPerBatchHigh, int gyroBatchesPerSecHigh,
        int algorithmConfig)
{
  locClientStatusEnumType result = eLOC_CLIENT_SUCCESS;
  locClientReqUnionType req_union;

  qmiLocSetSensorPerformanceControlConfigReqMsgT_v02 sensor_perf_config_req;
  qmiLocSetSensorPerformanceControlConfigIndMsgT_v02 sensor_perf_config_ind;

  LOC_LOGD("%s:%d]: Sensor Perf Control Config (performanceControlMode)(%u) "
                "accel(#smp,#batches) (%u,%u) gyro(#smp,#batches) (%u,%u) "
                "accel_high(#smp,#batches) (%u,%u) gyro_high(#smp,#batches) (%u,%u) "
                "algorithmConfig(%u)\n",
                __FUNCTION__,
                __LINE__,
                controlMode,
                accelSamplesPerBatch,
                accelBatchesPerSec,
                gyroSamplesPerBatch,
                gyroBatchesPerSec,
                accelSamplesPerBatchHigh,
                accelBatchesPerSecHigh,
                gyroSamplesPerBatchHigh,
                gyroBatchesPerSecHigh,
                algorithmConfig
                );

  memset(&sensor_perf_config_req, 0, sizeof(sensor_perf_config_req));
  memset(&sensor_perf_config_ind, 0, sizeof(sensor_perf_config_ind));

  sensor_perf_config_req.performanceControlMode_valid = (controlMode == 2) ? 0 : 1;
  sensor_perf_config_req.performanceControlMode = (qmiLocSensorPerformanceControlModeEnumT_v02)controlMode;
  sensor_perf_config_req.accelSamplingSpec_valid = 1;
  sensor_perf_config_req.accelSamplingSpec.batchesPerSecond = accelBatchesPerSec;
  sensor_perf_config_req.accelSamplingSpec.samplesPerBatch = accelSamplesPerBatch;
  sensor_perf_config_req.gyroSamplingSpec_valid = 1;
  sensor_perf_config_req.gyroSamplingSpec.batchesPerSecond = gyroBatchesPerSec;
  sensor_perf_config_req.gyroSamplingSpec.samplesPerBatch = gyroSamplesPerBatch;
  sensor_perf_config_req.accelSamplingSpecHigh_valid = 1;
  sensor_perf_config_req.accelSamplingSpecHigh.batchesPerSecond = accelBatchesPerSecHigh;
  sensor_perf_config_req.accelSamplingSpecHigh.samplesPerBatch = accelSamplesPerBatchHigh;
  sensor_perf_config_req.gyroSamplingSpecHigh_valid = 1;
  sensor_perf_config_req.gyroSamplingSpecHigh.batchesPerSecond = gyroBatchesPerSecHigh;
  sensor_perf_config_req.gyroSamplingSpecHigh.samplesPerBatch = gyroSamplesPerBatchHigh;
  sensor_perf_config_req.algorithmConfig_valid = 1;
  sensor_perf_config_req.algorithmConfig = algorithmConfig;

  req_union.pSetSensorPerformanceControlConfigReq = &sensor_perf_config_req;

  result = locSyncSendReq(QMI_LOC_SET_SENSOR_PERFORMANCE_CONTROL_CONFIGURATION_REQ_V02,
                          req_union, LOC_ENGINE_SYNC_REQUEST_TIMEOUT,
                          QMI_LOC_SET_SENSOR_PERFORMANCE_CONTROL_CONFIGURATION_IND_V02,
                          &sensor_perf_config_ind);

  if(result != eLOC_CLIENT_SUCCESS ||
     eQMI_LOC_SUCCESS_V02 != sensor_perf_config_ind.status)
  {
    LOC_LOGE ("%s:%d]: Error status = %s, ind..status = %s ",
              __func__, __LINE__,
              loc_get_v02_client_status_name(result),
              loc_get_v02_qmi_status_name(sensor_perf_config_ind.status));
  }

  return convertErr(result);
}

/* set the Positioning Protocol on A-GLONASS system */
LocationError
LocApiV02::setAGLONASSProtocolSync(GnssConfigAGlonassPositionProtocolMask aGlonassProtocol)
{
  LocationError err = LOCATION_ERROR_SUCCESS;
  locClientStatusEnumType result = eLOC_CLIENT_SUCCESS;
  locClientReqUnionType req_union;
  qmiLocSetProtocolConfigParametersReqMsgT_v02 aGlonassProtocol_req;
  qmiLocSetProtocolConfigParametersIndMsgT_v02 aGlonassProtocol_ind;

  memset(&aGlonassProtocol_req, 0, sizeof(aGlonassProtocol_req));
  memset(&aGlonassProtocol_ind, 0, sizeof(aGlonassProtocol_ind));

  aGlonassProtocol_req.assistedGlonassProtocolMask_valid = 1;
  if (GNSS_CONFIG_RRC_CONTROL_PLANE_BIT & aGlonassProtocol) {
      aGlonassProtocol_req.assistedGlonassProtocolMask |=
          QMI_LOC_ASSISTED_GLONASS_PROTOCOL_MASK_RRC_CP_V02 ;
  }
  if (GNSS_CONFIG_RRLP_USER_PLANE_BIT & aGlonassProtocol) {
      aGlonassProtocol_req.assistedGlonassProtocolMask |=
          QMI_LOC_ASSISTED_GLONASS_PROTOCOL_MASK_RRLP_UP_V02;
  }
  if (GNSS_CONFIG_LLP_USER_PLANE_BIT & aGlonassProtocol) {
      aGlonassProtocol_req.assistedGlonassProtocolMask |=
          QMI_LOC_ASSISTED_GLONASS_PROTOCOL_MASK_LPP_UP_V02;
  }
  if (GNSS_CONFIG_LLP_CONTROL_PLANE_BIT & aGlonassProtocol) {
      aGlonassProtocol_req.assistedGlonassProtocolMask |=
          QMI_LOC_ASSISTED_GLONASS_PROTOCOL_MASK_LPP_CP_V02;
  }

  req_union.pSetProtocolConfigParametersReq = &aGlonassProtocol_req;

  LOC_LOGD("%s:%d]: aGlonassProtocolMask = 0x%x",  __func__, __LINE__,
                             aGlonassProtocol_req.assistedGlonassProtocolMask);

  result = locSyncSendReq(QMI_LOC_SET_PROTOCOL_CONFIG_PARAMETERS_REQ_V02,
                          req_union, LOC_ENGINE_SYNC_REQUEST_LONG_TIMEOUT,
                          QMI_LOC_SET_PROTOCOL_CONFIG_PARAMETERS_IND_V02,
                          &aGlonassProtocol_ind);

  if(result != eLOC_CLIENT_SUCCESS ||
     eQMI_LOC_SUCCESS_V02 != aGlonassProtocol_ind.status)
  {
    LOC_LOGE ("%s:%d]: Error status = %s, ind..status = %s ",
              __func__, __LINE__,
              loc_get_v02_client_status_name(result),
              loc_get_v02_qmi_status_name(aGlonassProtocol_ind.status));
    err = LOCATION_ERROR_GENERAL_FAILURE;
  }

  return err;
}

LocationError
LocApiV02::setLPPeProtocolCpSync(GnssConfigLppeControlPlaneMask lppeCP)
{
  LocationError err = LOCATION_ERROR_SUCCESS;
  locClientStatusEnumType result = eLOC_CLIENT_SUCCESS;
  locClientReqUnionType req_union;
  qmiLocSetProtocolConfigParametersReqMsgT_v02 lppe_req;
  qmiLocSetProtocolConfigParametersIndMsgT_v02 lppe_ind;

  memset(&lppe_req, 0, sizeof(lppe_req));
  memset(&lppe_ind, 0, sizeof(lppe_ind));

  lppe_req.lppeCpConfig_valid = 1;
  if (GNSS_CONFIG_LPPE_CONTROL_PLANE_DBH_BIT & lppeCP) {
      lppe_req.lppeCpConfig |= QMI_LOC_LPPE_MASK_CP_DBH_V02;
  }
  if (GNSS_CONFIG_LPPE_CONTROL_PLANE_WLAN_AP_MEASUREMENTS_BIT & lppeCP) {
      lppe_req.lppeCpConfig |= QMI_LOC_LPPE_MASK_CP_AP_WIFI_MEASUREMENT_V02;
  }
  if (GNSS_CONFIG_LPPE_CONTROL_PLANE_SRN_AP_MEASUREMENTS_BIT & lppeCP) {
      lppe_req.lppeCpConfig |= QMI_LOC_LPPE_MASK_CP_AP_SRN_BTLE_MEASUREMENT_V02;
  }
  if (GNSS_CONFIG_LPPE_CONTROL_PLANE_SENSOR_BARO_MEASUREMENTS_BIT & lppeCP) {
      lppe_req.lppeCpConfig |= QMI_LOC_LPPE_MASK_CP_UBP_V02;
  }

  req_union.pSetProtocolConfigParametersReq = &lppe_req;

  LOC_LOGD("%s:%d]: lppeCpConfig = 0x%" PRIx64,  __func__, __LINE__,
           lppe_req.lppeCpConfig);

  result = locSyncSendReq(QMI_LOC_SET_PROTOCOL_CONFIG_PARAMETERS_REQ_V02,
                          req_union, LOC_ENGINE_SYNC_REQUEST_LONG_TIMEOUT,
                          QMI_LOC_SET_PROTOCOL_CONFIG_PARAMETERS_IND_V02,
                          &lppe_ind);

  if(result != eLOC_CLIENT_SUCCESS ||
     eQMI_LOC_SUCCESS_V02 != lppe_ind.status)
  {
    LOC_LOGE ("%s:%d]: Error status = %s, ind..status = %s ",
              __func__, __LINE__,
              loc_get_v02_client_status_name(result),
              loc_get_v02_qmi_status_name(lppe_ind.status));
    err = LOCATION_ERROR_GENERAL_FAILURE;
  }

  return err;
}

LocationError
LocApiV02::setLPPeProtocolUpSync(GnssConfigLppeUserPlaneMask lppeUP)
{
  LocationError err = LOCATION_ERROR_SUCCESS;
  locClientStatusEnumType result = eLOC_CLIENT_SUCCESS;
  locClientReqUnionType req_union;
  qmiLocSetProtocolConfigParametersReqMsgT_v02 lppe_req;
  qmiLocSetProtocolConfigParametersIndMsgT_v02 lppe_ind;

  memset(&lppe_req, 0, sizeof(lppe_req));
  memset(&lppe_ind, 0, sizeof(lppe_ind));
  memset(&req_union, 0, sizeof(req_union));

  lppe_req.lppeUpConfig_valid = 1;
  if (GNSS_CONFIG_LPPE_USER_PLANE_DBH_BIT & lppeUP) {
      lppe_req.lppeUpConfig |= QMI_LOC_LPPE_MASK_UP_DBH_V02;
  }
  if (GNSS_CONFIG_LPPE_USER_PLANE_WLAN_AP_MEASUREMENTS_BIT & lppeUP) {
      lppe_req.lppeUpConfig |= QMI_LOC_LPPE_MASK_UP_AP_WIFI_MEASUREMENT_V02;
  }
  if (GNSS_CONFIG_LPPE_USER_PLANE_SRN_AP_MEASUREMENTS_BIT & lppeUP) {
      lppe_req.lppeUpConfig |= QMI_LOC_LPPE_MASK_UP_AP_SRN_BTLE_MEASUREMENT_V02;
  }
  if (GNSS_CONFIG_LPPE_USER_PLANE_SENSOR_BARO_MEASUREMENTS_BIT & lppeUP) {
      lppe_req.lppeUpConfig |= QMI_LOC_LPPE_MASK_UP_UBP_V02;
  }

  req_union.pSetProtocolConfigParametersReq = &lppe_req;

  LOC_LOGD("%s:%d]: lppeUpConfig = 0x%" PRIx64,  __func__, __LINE__,
           lppe_req.lppeUpConfig);

  result = locSyncSendReq(QMI_LOC_SET_PROTOCOL_CONFIG_PARAMETERS_REQ_V02,
                          req_union, LOC_ENGINE_SYNC_REQUEST_LONG_TIMEOUT,
                          QMI_LOC_SET_PROTOCOL_CONFIG_PARAMETERS_IND_V02,
                          &lppe_ind);

  if(result != eLOC_CLIENT_SUCCESS ||
     eQMI_LOC_SUCCESS_V02 != lppe_ind.status)
  {
    LOC_LOGE ("%s:%d]: Error status = %s, ind..status = %s ",
              __func__, __LINE__,
              loc_get_v02_client_status_name(result),
              loc_get_v02_qmi_status_name(lppe_ind.status));
    err = LOCATION_ERROR_GENERAL_FAILURE;
  }

  return err;
}

/* Convert event mask from loc eng to loc_api_v02 format */
locClientEventMaskType LocApiV02 :: convertMask(
  LOC_API_ADAPTER_EVENT_MASK_T mask)
{
  locClientEventMaskType eventMask = 0;
  LOC_LOGd("adapter mask = 0x%" PRIx64, mask);

  if (mask & LOC_API_ADAPTER_BIT_PARSED_POSITION_REPORT)
      eventMask |= QMI_LOC_EVENT_MASK_POSITION_REPORT_V02;

  if (mask & LOC_API_ADAPTER_BIT_PARSED_UNPROPAGATED_POSITION_REPORT)
      eventMask |= QMI_LOC_EVENT_MASK_UNPROPAGATED_POSITION_REPORT_V02;

  if (mask & LOC_API_ADAPTER_BIT_SATELLITE_REPORT)
      eventMask |= QMI_LOC_EVENT_MASK_GNSS_SV_INFO_V02;

  /* treat NMEA_1Hz and NMEA_POSITION_REPORT the same*/
  if ((mask & LOC_API_ADAPTER_BIT_NMEA_POSITION_REPORT) ||
      (mask & LOC_API_ADAPTER_BIT_NMEA_1HZ_REPORT) ) {
      eventMask |= QMI_LOC_EVENT_MASK_NMEA_V02;
      // if registering for NMEA event, also register for ENGINE STATE event so that we can
      // register for NMEA event when Engine turns ON and deregister when Engine turns OFF
      eventMask |= QMI_LOC_EVENT_MASK_ENGINE_STATE_V02;
  }

  if (mask & LOC_API_ADAPTER_BIT_NI_NOTIFY_VERIFY_REQUEST)
      eventMask |= QMI_LOC_EVENT_MASK_NI_NOTIFY_VERIFY_REQ_V02;

  if (mask & LOC_API_ADAPTER_BIT_ASSISTANCE_DATA_REQUEST)
  {
    eventMask |= QMI_LOC_EVENT_MASK_INJECT_PREDICTED_ORBITS_REQ_V02;
    eventMask |= QMI_LOC_EVENT_MASK_INJECT_TIME_REQ_V02;
  }

  if (mask & LOC_API_ADAPTER_BIT_POSITION_INJECTION_REQUEST)
      eventMask |= QMI_LOC_EVENT_MASK_INJECT_POSITION_REQ_V02;

  if (mask & LOC_API_ADAPTER_BIT_STATUS_REPORT)
  {
      eventMask |= (QMI_LOC_EVENT_MASK_ENGINE_STATE_V02);
  }

  if (mask & LOC_API_ADAPTER_BIT_LOCATION_SERVER_REQUEST)
      eventMask |= QMI_LOC_EVENT_MASK_LOCATION_SERVER_CONNECTION_REQ_V02;

  if (mask & LOC_API_ADAPTER_BIT_REQUEST_WIFI)
      eventMask |= QMI_LOC_EVENT_MASK_WIFI_REQ_V02;

  if (mask & LOC_API_ADAPTER_BIT_SENSOR_STATUS)
      eventMask |= QMI_LOC_EVENT_MASK_SENSOR_STREAMING_READY_STATUS_V02;

  if (mask & LOC_API_ADAPTER_BIT_REQUEST_TIME_SYNC)
      eventMask |= QMI_LOC_EVENT_MASK_TIME_SYNC_REQ_V02;

  if (mask & LOC_API_ADAPTER_BIT_REPORT_SPI)
      eventMask |= QMI_LOC_EVENT_MASK_SET_SPI_STREAMING_REPORT_V02;

  if (mask & LOC_API_ADAPTER_BIT_REPORT_NI_GEOFENCE)
      eventMask |= QMI_LOC_EVENT_MASK_NI_GEOFENCE_NOTIFICATION_V02;

  if (mask & LOC_API_ADAPTER_BIT_GEOFENCE_GEN_ALERT)
      eventMask |= QMI_LOC_EVENT_MASK_GEOFENCE_GEN_ALERT_V02;

  if (mask & LOC_API_ADAPTER_BIT_REPORT_GENFENCE_BREACH)
      eventMask |= QMI_LOC_EVENT_MASK_GEOFENCE_BREACH_NOTIFICATION_V02;

  if (mask & LOC_API_ADAPTER_BIT_BATCHED_GENFENCE_BREACH_REPORT) {
      if (ContextBase::isMessageSupported(LOC_API_ADAPTER_MESSAGE_BATCHED_GENFENCE_BREACH)) {
          eventMask |= QMI_LOC_EVENT_MASK_GEOFENCE_BATCH_BREACH_NOTIFICATION_V02;
      } else {
          eventMask |= QMI_LOC_EVENT_MASK_GEOFENCE_BREACH_NOTIFICATION_V02;
      }
  }

  if (mask & LOC_API_ADAPTER_BIT_PEDOMETER_CTRL)
      eventMask |= QMI_LOC_EVENT_MASK_PEDOMETER_CONTROL_V02;

  if (mask & LOC_API_ADAPTER_BIT_REPORT_GENFENCE_DWELL)
      eventMask |= QMI_LOC_EVENT_MASK_GEOFENCE_BATCH_DWELL_NOTIFICATION_V02;

  if (mask & LOC_API_ADAPTER_BIT_MOTION_CTRL)
      eventMask |= QMI_LOC_EVENT_MASK_MOTION_DATA_CONTROL_V02;

  if (mask & LOC_API_ADAPTER_BIT_REQUEST_WIFI_AP_DATA)
      eventMask |= QMI_LOC_EVENT_MASK_INJECT_WIFI_AP_DATA_REQ_V02;

  if(mask & LOC_API_ADAPTER_BIT_BATCH_FULL)
      eventMask |= QMI_LOC_EVENT_MASK_BATCH_FULL_NOTIFICATION_V02;

  if(mask & LOC_API_ADAPTER_BIT_BATCH_STATUS)
      eventMask |= QMI_LOC_EVENT_MASK_BATCHING_STATUS_V02;

  if(mask & LOC_API_ADAPTER_BIT_BATCHED_POSITION_REPORT)
      eventMask |= QMI_LOC_EVENT_MASK_LIVE_BATCHED_POSITION_REPORT_V02;

  if(mask & LOC_API_ADAPTER_BIT_GNSS_MEASUREMENT_REPORT)
        eventMask |= QMI_LOC_EVENT_MASK_GNSS_MEASUREMENT_REPORT_V02;

  if(mask & LOC_API_ADAPTER_BIT_GNSS_SV_POLYNOMIAL_REPORT)
      eventMask |= QMI_LOC_EVENT_MASK_GNSS_SV_POLYNOMIAL_REPORT_V02;

  if(mask & LOC_API_ADAPTER_BIT_GNSS_SV_EPHEMERIS_REPORT)
        eventMask |= QMI_LOC_EVENT_MASK_EPHEMERIS_REPORT_V02;

  // for GDT
  if(mask & LOC_API_ADAPTER_BIT_GDT_UPLOAD_BEGIN_REQ)
      eventMask |= QMI_LOC_EVENT_MASK_GDT_UPLOAD_BEGIN_REQ_V02;

  if(mask & LOC_API_ADAPTER_BIT_GDT_UPLOAD_END_REQ)
      eventMask |= QMI_LOC_EVENT_MASK_GDT_UPLOAD_END_REQ_V02;

  if (mask & LOC_API_ADAPTER_BIT_GNSS_MEASUREMENT)
      eventMask |= QMI_LOC_EVENT_MASK_GNSS_MEASUREMENT_REPORT_V02;

  if(mask & LOC_API_ADAPTER_BIT_REQUEST_TIMEZONE)
      eventMask |= QMI_LOC_EVENT_MASK_GET_TIME_ZONE_REQ_V02;

  if(mask & LOC_API_ADAPTER_BIT_REQUEST_SRN_DATA)
      eventMask |= QMI_LOC_EVENT_MASK_INJECT_SRN_AP_DATA_REQ_V02 ;

  if (mask & LOC_API_ADAPTER_BIT_FDCL_SERVICE_REQ)
      eventMask |= QMI_LOC_EVENT_MASK_FDCL_SERVICE_REQ_V02;

  if (mask & LOC_API_ADAPTER_BIT_BS_OBS_DATA_SERVICE_REQ)
      eventMask |= QMI_LOC_EVENT_MASK_BS_OBS_DATA_SERVICE_REQ_V02;

  if (mask & LOC_API_ADAPTER_BIT_LOC_SYSTEM_INFO)
      eventMask |= QMI_LOC_EVENT_MASK_NEXT_LS_INFO_REPORT_V02;

  if (mask & LOC_API_ADAPTER_BIT_EVENT_REPORT_INFO)
      eventMask |= QMI_LOC_EVENT_MASK_GNSS_EVENT_REPORT_V02;

  if (mask & LOC_API_ADAPTER_BIT_GNSS_NHZ_MEASUREMENT) {
      eventMask |= QMI_LOC_EVENT_MASK_GNSS_NHZ_MEASUREMENT_REPORT_V02;
  }

  if (mask & LOC_API_ADAPTER_BIT_LATENCY_INFORMATION) {
      eventMask |= QMI_LOC_EVENT_MASK_LATENCY_INFORMATION_REPORT_V02;
  }

  return eventMask;
}

qmiLocLockEnumT_v02 LocApiV02 ::convertGpsLockFromAPItoQMI(GnssConfigGpsLock lock)
{
    switch (lock)
    {
      case GNSS_CONFIG_GPS_LOCK_MO_AND_NI:
        return eQMI_LOC_LOCK_ALL_V02;
      case GNSS_CONFIG_GPS_LOCK_MO:
        return eQMI_LOC_LOCK_MI_V02;
      case GNSS_CONFIG_GPS_LOCK_NI:
        return eQMI_LOC_LOCK_MT_V02;
      case GNSS_CONFIG_GPS_LOCK_NONE:
      default:
        return eQMI_LOC_LOCK_NONE_V02;
    }
}

GnssConfigGpsLock LocApiV02::convertGpsLockFromQMItoAPI(qmiLocLockEnumT_v02 lock)
{
    switch (lock) {
      case eQMI_LOC_LOCK_MI_V02:
        return GNSS_CONFIG_GPS_LOCK_MO;
      case eQMI_LOC_LOCK_MT_V02:
        return GNSS_CONFIG_GPS_LOCK_NI;
      case eQMI_LOC_LOCK_ALL_V02:
        return GNSS_CONFIG_GPS_LOCK_MO_AND_NI;
      case eQMI_LOC_LOCK_NONE_V02:
      default:
        return GNSS_CONFIG_GPS_LOCK_NONE;
    }
}

EngineLockState LocApiV02::convertEngineLockState(qmiLocEngineLockStateEnumT_v02 LockState)
{
    switch (LockState) {
      case eQMI_LOC_ENGINE_LOCK_STATE_ENABLED_V02:
        return ENGINE_LOCK_STATE_ENABLED;
      case eQMI_LOC_ENGINE_LOCK_STATE_DISABLED_V02:
        return ENGINE_LOCK_STATE_DISABLED;
      default:
        return ENGINE_LOCK_STATE_INVALID;
    }
}

/* Convert error from loc_api_v02 to loc eng format*/
enum loc_api_adapter_err LocApiV02 :: convertErr(
  locClientStatusEnumType status)
{
  switch( status)
  {
    case eLOC_CLIENT_SUCCESS:
      return LOC_API_ADAPTER_ERR_SUCCESS;

    case eLOC_CLIENT_FAILURE_GENERAL:
      return LOC_API_ADAPTER_ERR_GENERAL_FAILURE;

    case eLOC_CLIENT_FAILURE_UNSUPPORTED:
      return LOC_API_ADAPTER_ERR_UNSUPPORTED;

    case eLOC_CLIENT_FAILURE_INVALID_PARAMETER:
      return LOC_API_ADAPTER_ERR_INVALID_PARAMETER;

    case eLOC_CLIENT_FAILURE_ENGINE_BUSY:
      return LOC_API_ADAPTER_ERR_ENGINE_BUSY;

    case eLOC_CLIENT_FAILURE_PHONE_OFFLINE:
      return LOC_API_ADAPTER_ERR_PHONE_OFFLINE;

    case eLOC_CLIENT_FAILURE_TIMEOUT:
      return LOC_API_ADAPTER_ERR_TIMEOUT;

    case eLOC_CLIENT_FAILURE_INVALID_HANDLE:
      return LOC_API_ADAPTER_ERR_INVALID_HANDLE;

    case eLOC_CLIENT_FAILURE_SERVICE_NOT_PRESENT:
      return LOC_API_ADAPTER_ERR_SERVICE_NOT_PRESENT;

    case eLOC_CLIENT_FAILURE_INTERNAL:
      return LOC_API_ADAPTER_ERR_INTERNAL;

    default:
      return LOC_API_ADAPTER_ERR_FAILURE;
  }
}

/* convert position report to loc eng format and send the converted
   position to loc eng */

void LocApiV02 :: reportPosition (
  const qmiLocEventPositionReportIndMsgT_v02 *location_report_ptr,
  bool unpropagatedPosition)
{
    UlpLocation location;
    LocPosTechMask tech_Mask = LOC_POS_TECH_MASK_DEFAULT;
    LOC_LOGD("Reporting position from V2 Adapter\n");

    mHlosQtimer2 = getQTimerTickCount();
    LOC_LOGv("mHlosQtimer2=%" PRIi64 " ", mHlosQtimer2);

    memset(&location, 0, sizeof (UlpLocation));
    location.size = sizeof(location);
    location.unpropagatedPosition = unpropagatedPosition;
    GnssDataNotification dataNotify = {};
    int msInWeek = -1;

    GpsLocationExtended locationExtended;
    memset(&locationExtended, 0, sizeof (GpsLocationExtended));
    locationExtended.size = sizeof(locationExtended);

    locationExtended.flags |= GPS_LOCATION_EXTENDED_HAS_OUTPUT_ENG_TYPE;
    locationExtended.locOutputEngType = LOC_OUTPUT_ENGINE_SPE;
    locationExtended.flags |= GPS_LOCATION_EXTENDED_HAS_OUTPUT_ENG_MASK;
    locationExtended.locOutputEngMask = STANDARD_POSITIONING_ENGINE;

    struct timespec apTimestamp;
    if( clock_gettime( CLOCK_BOOTTIME, &apTimestamp)== 0)
    {
       locationExtended.timeStamp.apTimeStamp.tv_sec = apTimestamp.tv_sec;
       locationExtended.timeStamp.apTimeStamp.tv_nsec = apTimestamp.tv_nsec;
       locationExtended.timeStamp.apTimeStampUncertaintyMs = (float)ap_timestamp_uncertainty;
    }
    else
    {
       locationExtended.timeStamp.apTimeStampUncertaintyMs = FLT_MAX;
       LOC_LOGE("%s:%d Error in clock_gettime() ",__func__, __LINE__);
    }
    LOC_LOGd("QMI_PosPacketTime %" PRIu32 "(sec) %" PRIu32 "(nsec), QMI_spoofReportMask %" PRIu64,
                 locationExtended.timeStamp.apTimeStamp.tv_sec,
                 locationExtended.timeStamp.apTimeStamp.tv_nsec,
                 location_report_ptr->spoofReportMask);

    // Process the position from final and intermediate reports
    memset(&dataNotify, 0, sizeof(dataNotify));
    msInWeek = (int)location_report_ptr->gpsTime.gpsTimeOfWeekMs;

    if (location_report_ptr->jammerIndicatorList_valid) {
        LOC_LOGV("%s:%d jammerIndicator is present len=%d",
                 __func__, __LINE__,
                 location_report_ptr->jammerIndicatorList_len);
        /* msInWeek is used to indicate if jammer indicator is present in this message.
           The appropriate action will be taken in GnssAdapter
           There are three cases as follows:
           1. jammer is not present (old modem image that doesn't support this) then
              msInWeek is set above, has a value >=0 and it is used in GnssAdapter to
              validate when parsing jammer value from NMEA debug message
           2. jammer is present and at least one satellite has a valid value, then
              msInWeek is -1
           3. jammer is present, but the values for all satellites are invalid
              (GNSS_INVALID_JAMMER_IND) then msInWeek is -2 */
        msInWeek = -2;
        for (uint32_t i = 1; i < location_report_ptr->jammerIndicatorList_len; i++) {
            dataNotify.gnssDataMask[i-1] = 0;
            dataNotify.agc[i-1] = 0.0;
            dataNotify.jammerInd[i-1] = 0.0;
            if (GNSS_INVALID_JAMMER_IND !=
                location_report_ptr->jammerIndicatorList[i].agcMetricDb) {
                LOC_LOGv("agcMetricDb[%d]=0x%X",
                         i, location_report_ptr->jammerIndicatorList[i].agcMetricDb);
                dataNotify.gnssDataMask[i-1] |= GNSS_LOC_DATA_AGC_BIT;
                dataNotify.agc[i-1] =
                    (double)location_report_ptr->jammerIndicatorList[i].agcMetricDb / 100.0;
                msInWeek = -1;
            }
            if (GNSS_INVALID_JAMMER_IND !=
                location_report_ptr->jammerIndicatorList[i].bpMetricDb) {
                LOC_LOGv("bpMetricDb[%d]=0x%X",
                         i, location_report_ptr->jammerIndicatorList[i].bpMetricDb);
                dataNotify.gnssDataMask[i-1] |= GNSS_LOC_DATA_JAMMER_IND_BIT;
                dataNotify.jammerInd[i-1] =
                    (double)location_report_ptr->jammerIndicatorList[i].bpMetricDb / 100.0;
                msInWeek = -1;
            }
        }
    } else {
        LOC_LOGd("jammerIndicator is not present");
    }

    if ((false == mIsFirstFinalFixReported) &&
            (eQMI_LOC_SESS_STATUS_SUCCESS_V02 == location_report_ptr->sessionStatus)) {
        loc_boot_kpi_marker("L - LocApiV02 First Final Fix");
        mIsFirstFinalFixReported = true;
    }
    if((location_report_ptr->sessionStatus == eQMI_LOC_SESS_STATUS_SUCCESS_V02) ||
       (location_report_ptr->sessionStatus == eQMI_LOC_SESS_STATUS_IN_PROGRESS_V02)) {
        // Latitude & Longitude
        if ((1 == location_report_ptr->latitude_valid) &&
            (1 == location_report_ptr->longitude_valid))
        {
            location.gpsLocation.flags  |= LOC_GPS_LOCATION_HAS_LAT_LONG;
            location.gpsLocation.latitude  = location_report_ptr->latitude;
            location.gpsLocation.longitude = location_report_ptr->longitude;
            if (location_report_ptr->altitudeWrtEllipsoid_valid) {
                LocApiProxyBase* locApiProxyObj = getLocApiProxy();
                float geoidalSeparation = 0.0;
                if (nullptr != locApiProxyObj) {
                    geoidalSeparation = locApiProxyObj->getGeoidalSeparation(
                            location_report_ptr->latitude, location_report_ptr->longitude);
                    locationExtended.altitudeMeanSeaLevel =
                            location_report_ptr->altitudeWrtEllipsoid - geoidalSeparation;
                    locationExtended.flags |= GPS_LOCATION_EXTENDED_HAS_ALTITUDE_MEAN_SEA_LEVEL;
                }
            }
        } else {
            LocApiBase::reportData(dataNotify, msInWeek);
        }

        // Time stamp (UTC)
        if (location_report_ptr->timestampUtc_valid == 1)
        {
            location.gpsLocation.timestamp = location_report_ptr->timestampUtc;
        }

        // Altitude
        if (location_report_ptr->altitudeWrtEllipsoid_valid == 1  )
        {
            location.gpsLocation.flags  |= LOC_GPS_LOCATION_HAS_ALTITUDE;
            location.gpsLocation.altitude = location_report_ptr->altitudeWrtEllipsoid;
        }

        // Altitude assumed or calculated info
        if (location_report_ptr->altitudeAssumed_valid == 1) {
            locationExtended.flags |= GPS_LOCATION_EXTENDED_HAS_ALTITUDE_ASSUMED;
            locationExtended.altitudeAssumed = location_report_ptr->altitudeAssumed;
        }

        // Speed
        if (location_report_ptr->speedHorizontal_valid == 1)
        {
            location.gpsLocation.flags  |= LOC_GPS_LOCATION_HAS_SPEED;
            location.gpsLocation.speed = location_report_ptr->speedHorizontal;
        }

        // Heading
        if (location_report_ptr->heading_valid == 1)
        {
            location.gpsLocation.flags  |= LOC_GPS_LOCATION_HAS_BEARING;
            location.gpsLocation.bearing = location_report_ptr->heading;
        }

        // Uncertainty (circular)
        if (location_report_ptr->horUncCircular_valid) {
            location.gpsLocation.flags |= LOC_GPS_LOCATION_HAS_ACCURACY;
            location.gpsLocation.accuracy = location_report_ptr->horUncCircular;
        } else if (location_report_ptr->horUncEllipseSemiMinor_valid &&
                   location_report_ptr->horUncEllipseSemiMajor_valid) {
            location.gpsLocation.flags |= LOC_GPS_LOCATION_HAS_ACCURACY;
            location.gpsLocation.accuracy =
                sqrt((location_report_ptr->horUncEllipseSemiMinor *
                      location_report_ptr->horUncEllipseSemiMinor) +
                     (location_report_ptr->horUncEllipseSemiMajor *
                      location_report_ptr->horUncEllipseSemiMajor));
        }

        // If horConfidence_valid is true, and horConfidence value is less than 68%
        // then scale the accuracy value to 68% confidence.
        if (location_report_ptr->horConfidence_valid)
        {
            bool is_CircUnc = (location_report_ptr->horUncCircular_valid) ?
                                                                    true : false;
            scaleAccuracyTo68PercentConfidence(location_report_ptr->horConfidence,
                                               location.gpsLocation,
                                               is_CircUnc);
        }

        // Technology Mask
        tech_Mask |= location_report_ptr->technologyMask;
        locationExtended.flags |= GPS_LOCATION_EXTENDED_HAS_POS_TECH_MASK;
        locationExtended.tech_mask = convertPosTechMask(location_report_ptr->technologyMask);

        //Mark the location source as from GNSS
        location.gpsLocation.flags |= LOCATION_HAS_SOURCE_INFO;
        location.position_source = ULP_LOCATION_IS_FROM_GNSS;

        if (location_report_ptr->spoofReportMask_valid)
        {
            location.gpsLocation.flags |= LOC_GPS_LOCATION_HAS_SPOOF_MASK;
            location.gpsLocation.spoof_mask = (uint32_t)location_report_ptr->spoofReportMask;
        }

        if (location_report_ptr->magneticDeviation_valid)
        {
            locationExtended.flags |= GPS_LOCATION_EXTENDED_HAS_MAG_DEV;
            locationExtended.magneticDeviation = location_report_ptr->magneticDeviation;
        }

        if (location_report_ptr->conformityIndex_valid) {
            locationExtended.flags |= GPS_LOCATION_EXTENDED_HAS_CONFORMITY_INDEX;
            locationExtended.conformityIndex = location_report_ptr->conformityIndex;
        }

        if (location_report_ptr->DOP_valid)
        {
            locationExtended.flags |= GPS_LOCATION_EXTENDED_HAS_DOP;
            locationExtended.pdop = location_report_ptr->DOP.PDOP;
            locationExtended.hdop = location_report_ptr->DOP.HDOP;
            locationExtended.vdop = location_report_ptr->DOP.VDOP;
        }

        if (location_report_ptr->vertUnc_valid)
        {
           locationExtended.flags |= GPS_LOCATION_EXTENDED_HAS_VERT_UNC;
           locationExtended.vert_unc = location_report_ptr->vertUnc;
        }

        if (location_report_ptr->speedUnc_valid)
        {
           locationExtended.flags |= GPS_LOCATION_EXTENDED_HAS_SPEED_UNC;
           locationExtended.speed_unc = location_report_ptr->speedUnc;
        }
        if (location_report_ptr->headingUnc_valid)
        {
           locationExtended.flags |= GPS_LOCATION_EXTENDED_HAS_BEARING_UNC;
           locationExtended.bearing_unc = location_report_ptr->headingUnc;
        }
        if (location_report_ptr->horReliability_valid)
        {
           locationExtended.flags |= GPS_LOCATION_EXTENDED_HAS_HOR_RELIABILITY;
           switch (location_report_ptr->horReliability)
           {
              case eQMI_LOC_RELIABILITY_NOT_SET_V02 :
                locationExtended.horizontal_reliability = LOC_RELIABILITY_NOT_SET;
                break;
              case eQMI_LOC_RELIABILITY_VERY_LOW_V02 :
                locationExtended.horizontal_reliability = LOC_RELIABILITY_VERY_LOW;
                break;
              case eQMI_LOC_RELIABILITY_LOW_V02 :
                locationExtended.horizontal_reliability = LOC_RELIABILITY_LOW;
                break;
              case eQMI_LOC_RELIABILITY_MEDIUM_V02 :
                locationExtended.horizontal_reliability = LOC_RELIABILITY_MEDIUM;
                break;
              case eQMI_LOC_RELIABILITY_HIGH_V02 :
                locationExtended.horizontal_reliability = LOC_RELIABILITY_HIGH;
                break;
              default:
                locationExtended.horizontal_reliability = LOC_RELIABILITY_NOT_SET;
                break;
           }
        }
        if (location_report_ptr->vertReliability_valid)
        {
           locationExtended.flags |= GPS_LOCATION_EXTENDED_HAS_VERT_RELIABILITY;
           switch (location_report_ptr->vertReliability)
           {
              case eQMI_LOC_RELIABILITY_NOT_SET_V02 :
                locationExtended.vertical_reliability = LOC_RELIABILITY_NOT_SET;
                break;
              case eQMI_LOC_RELIABILITY_VERY_LOW_V02 :
                locationExtended.vertical_reliability = LOC_RELIABILITY_VERY_LOW;
                break;
              case eQMI_LOC_RELIABILITY_LOW_V02 :
                locationExtended.vertical_reliability = LOC_RELIABILITY_LOW;
                break;
              case eQMI_LOC_RELIABILITY_MEDIUM_V02 :
                locationExtended.vertical_reliability = LOC_RELIABILITY_MEDIUM;
                break;
              case eQMI_LOC_RELIABILITY_HIGH_V02 :
                locationExtended.vertical_reliability = LOC_RELIABILITY_HIGH;
                break;
              default:
                locationExtended.vertical_reliability = LOC_RELIABILITY_NOT_SET;
                break;
           }
        }

        if (location_report_ptr->horUncEllipseSemiMajor_valid)
        {
            locationExtended.flags |= GPS_LOCATION_EXTENDED_HAS_HOR_ELIP_UNC_MAJOR;
            locationExtended.horUncEllipseSemiMajor = location_report_ptr->horUncEllipseSemiMajor;
        }
        if (location_report_ptr->horUncEllipseSemiMinor_valid)
        {
            locationExtended.flags |= GPS_LOCATION_EXTENDED_HAS_HOR_ELIP_UNC_MINOR;
            locationExtended.horUncEllipseSemiMinor = location_report_ptr->horUncEllipseSemiMinor;
        }
        if (location_report_ptr->horUncEllipseOrientAzimuth_valid)
        {
            locationExtended.flags |= GPS_LOCATION_EXTENDED_HAS_HOR_ELIP_UNC_AZIMUTH;
            locationExtended.horUncEllipseOrientAzimuth =
                location_report_ptr->horUncEllipseOrientAzimuth;
        }

        // If the horizontal uncertainty ellipse info is available,
        // calculate the horizontal uncertainty along north and east
        if (location_report_ptr->horUncEllipseSemiMajor_valid &&
            location_report_ptr->horUncEllipseSemiMinor_valid &&
            location_report_ptr->horUncEllipseOrientAzimuth_valid)
        {
            double cosVal = cos((double)locationExtended.horUncEllipseOrientAzimuth);
            double sinVal = sin((double)locationExtended.horUncEllipseOrientAzimuth);
            double major = locationExtended.horUncEllipseSemiMajor;
            double minor = locationExtended.horUncEllipseSemiMinor;

            double northSquare = major*major * cosVal*cosVal + minor*minor * sinVal*sinVal;
            double eastSquare =  major*major * sinVal*sinVal + minor*minor * cosVal*cosVal;

            locationExtended.flags |= GPS_LOCATION_EXTENDED_HAS_NORTH_STD_DEV;
            locationExtended.northStdDeviation = sqrt(northSquare);

            locationExtended.flags |= GPS_LOCATION_EXTENDED_HAS_EAST_STD_DEV;
            locationExtended.eastStdDeviation  = sqrt(eastSquare);
        }

        if (((location_report_ptr->expandedGnssSvUsedList_valid) &&
                (location_report_ptr->expandedGnssSvUsedList_len != 0)) ||
                ((location_report_ptr->gnssSvUsedList_valid) &&
                (location_report_ptr->gnssSvUsedList_len != 0)))
        {
            uint32_t idx=0;
            uint32_t gnssSvUsedList_len = 0;
            uint16_t gnssSvIdUsed = 0;
            const uint16_t *svUsedList;
            bool multiBandTypesAvailable = false;

            if (location_report_ptr->expandedGnssSvUsedList_valid)
            {
                gnssSvUsedList_len = location_report_ptr->expandedGnssSvUsedList_len;
                svUsedList = location_report_ptr->expandedGnssSvUsedList;
            } else if (location_report_ptr->gnssSvUsedList_valid)
            {
                gnssSvUsedList_len = location_report_ptr->gnssSvUsedList_len;
                svUsedList = location_report_ptr->gnssSvUsedList;
            }

            // If multifreq data is not available then default to L1 for all constellations.
            if ((location_report_ptr->gnssSvUsedSignalTypeList_valid) &&
                    (location_report_ptr->gnssSvUsedSignalTypeList_len != 0)) {
                multiBandTypesAvailable = true;
            }

            LOC_LOGv("sv length %d ", gnssSvUsedList_len);

            locationExtended.numOfMeasReceived = gnssSvUsedList_len;
            memset(locationExtended.measUsageInfo, 0, sizeof(locationExtended.measUsageInfo));
            // Set of used_in_fix SV ID
            bool reported = LocApiBase::needReport(location,
                                                   (eQMI_LOC_SESS_STATUS_IN_PROGRESS_V02 ==
                                                   location_report_ptr->sessionStatus ?
                                                   LOC_SESS_INTERMEDIATE : LOC_SESS_SUCCESS),
                                                   tech_Mask);
            if (unpropagatedPosition || reported) {
                for (idx = 0; idx < gnssSvUsedList_len; idx++)
                {
                    gnssSvIdUsed = svUsedList[idx];
                    locationExtended.measUsageInfo[idx].gnssSvId = gnssSvIdUsed;
                    locationExtended.measUsageInfo[idx].carrierPhaseAmbiguityType =
                            CARRIER_PHASE_AMBIGUITY_RESOLUTION_NONE;

                    qmiLocGnssSignalTypeMaskT_v02 qmiGnssSignalType =
                            location_report_ptr->gnssSvUsedSignalTypeList[idx];
                    GnssSignalTypeMask gnssSignalTypeMask =
                            convertQmiGnssSignalType(qmiGnssSignalType);
                    LOC_LOGd("sv id %d, qmi signal type: 0x%" PRIx64 ", hal signal type: 0x%x",
                             gnssSvIdUsed, qmiGnssSignalType, gnssSignalTypeMask);

                    if (gnssSvIdUsed <= GPS_SV_PRN_MAX)
                    {
                        uint64_t bit = (1ULL << (gnssSvIdUsed - GPS_SV_PRN_MIN));
                        locationExtended.gnss_sv_used_ids.gps_sv_used_ids_mask |= bit;
                        locationExtended.measUsageInfo[idx].gnssConstellation =
                                GNSS_LOC_SV_SYSTEM_GPS;
                        if (multiBandTypesAvailable) {
                            locationExtended.measUsageInfo[idx].gnssSignalType =
                                    gnssSignalTypeMask;
                            if (locationExtended.measUsageInfo[idx].gnssSignalType &
                                    GNSS_SIGNAL_GPS_L1CA) {
                                locationExtended.gnss_mb_sv_used_ids.gps_l1ca_sv_used_ids_mask
                                        |= bit;
                            }
                            if (locationExtended.measUsageInfo[idx].gnssSignalType &
                                    GNSS_SIGNAL_GPS_L1C) {
                                locationExtended.gnss_mb_sv_used_ids.gps_l1c_sv_used_ids_mask
                                        |= bit;
                            }
                            if (locationExtended.measUsageInfo[idx].gnssSignalType &
                                    GNSS_SIGNAL_GPS_L2) {
                                locationExtended.gnss_mb_sv_used_ids.gps_l2_sv_used_ids_mask
                                        |= bit;
                            }
                            if (locationExtended.measUsageInfo[idx].gnssSignalType &
                                    GNSS_SIGNAL_GPS_L5) {
                                locationExtended.gnss_mb_sv_used_ids.gps_l5_sv_used_ids_mask
                                        |= bit;
                            }
                        } else {
                            locationExtended.measUsageInfo[idx].gnssSignalType =
                                    GNSS_SIGNAL_GPS_L1CA;
                        }
                    }
                    else if ((gnssSvIdUsed >= GLO_SV_PRN_MIN) &&
                             (gnssSvIdUsed <= GLO_SV_PRN_MAX))
                    {
                        uint64_t bit = (1ULL << (gnssSvIdUsed - GLO_SV_PRN_MIN));
                        locationExtended.gnss_sv_used_ids.glo_sv_used_ids_mask |= bit;
                        locationExtended.measUsageInfo[idx].gnssConstellation =
                                GNSS_LOC_SV_SYSTEM_GLONASS;
                        if (multiBandTypesAvailable) {
                            locationExtended.measUsageInfo[idx].gnssSignalType =
                                    gnssSignalTypeMask;

                            if (locationExtended.measUsageInfo[idx].gnssSignalType &
                                    GNSS_SIGNAL_GLONASS_G1) {
                                locationExtended.gnss_mb_sv_used_ids.glo_g1_sv_used_ids_mask
                                        |= bit;
                            }
                            if (locationExtended.measUsageInfo[idx].gnssSignalType &
                                    GNSS_SIGNAL_GLONASS_G2) {
                                locationExtended.gnss_mb_sv_used_ids.glo_g2_sv_used_ids_mask
                                        |= bit;
                            }
                        } else {
                            locationExtended.measUsageInfo[idx].gnssSignalType =
                                    GNSS_SIGNAL_GLONASS_G1;
                        }

                    }
                    else if ((gnssSvIdUsed >= BDS_SV_PRN_MIN) &&
                             (gnssSvIdUsed <= BDS_SV_PRN_MAX))
                    {
                        uint64_t bit = (1ULL << (gnssSvIdUsed - BDS_SV_PRN_MIN));
                        locationExtended.gnss_sv_used_ids.bds_sv_used_ids_mask |= bit;
                        locationExtended.measUsageInfo[idx].gnssConstellation =
                                GNSS_LOC_SV_SYSTEM_BDS;
                        if (multiBandTypesAvailable) {
                            locationExtended.measUsageInfo[idx].gnssSignalType =
                                    gnssSignalTypeMask;
                            if (locationExtended.measUsageInfo[idx].gnssSignalType &
                                    GNSS_SIGNAL_BEIDOU_B1I) {
                                locationExtended.gnss_mb_sv_used_ids.bds_b1i_sv_used_ids_mask
                                        |= bit;
                            }
                            if (locationExtended.measUsageInfo[idx].gnssSignalType &
                                    GNSS_SIGNAL_BEIDOU_B1C) {
                                locationExtended.gnss_mb_sv_used_ids.bds_b1c_sv_used_ids_mask
                                        |= bit;
                            }
                            if (locationExtended.measUsageInfo[idx].gnssSignalType &
                                    GNSS_SIGNAL_BEIDOU_B2I) {
                                locationExtended.gnss_mb_sv_used_ids.bds_b2i_sv_used_ids_mask
                                        |= bit;
                            }
                            if (locationExtended.measUsageInfo[idx].gnssSignalType &
                                    GNSS_SIGNAL_BEIDOU_B2AI) {
                                locationExtended.gnss_mb_sv_used_ids.bds_b2ai_sv_used_ids_mask
                                        |= bit;
                            }
                            if (locationExtended.measUsageInfo[idx].gnssSignalType &
                                    GNSS_SIGNAL_BEIDOU_B2AQ) {
                                locationExtended.gnss_mb_sv_used_ids.bds_b2aq_sv_used_ids_mask
                                        |= bit;
                            }
                        } else {
                            locationExtended.measUsageInfo[idx].gnssSignalType =
                                    GNSS_SIGNAL_BEIDOU_B1I;
                        }
                    }
                    else if ((gnssSvIdUsed >= GAL_SV_PRN_MIN) &&
                             (gnssSvIdUsed <= GAL_SV_PRN_MAX))
                    {
                        uint64_t bit = (1ULL << (gnssSvIdUsed - GAL_SV_PRN_MIN));
                        locationExtended.gnss_sv_used_ids.gal_sv_used_ids_mask |= bit;
                        locationExtended.measUsageInfo[idx].gnssConstellation =
                                GNSS_LOC_SV_SYSTEM_GALILEO;
                        if (multiBandTypesAvailable) {
                            locationExtended.measUsageInfo[idx].gnssSignalType =
                                    gnssSignalTypeMask;

                            if (locationExtended.measUsageInfo[idx].gnssSignalType &
                                    GNSS_SIGNAL_GALILEO_E1) {
                                locationExtended.gnss_mb_sv_used_ids.gal_e1_sv_used_ids_mask
                                        |= bit;
                            }
                            if (locationExtended.measUsageInfo[idx].gnssSignalType &
                                    GNSS_SIGNAL_GALILEO_E5A) {
                                locationExtended.gnss_mb_sv_used_ids.gal_e5a_sv_used_ids_mask
                                        |= bit;
                            }
                            if (locationExtended.measUsageInfo[idx].gnssSignalType &
                                    GNSS_SIGNAL_GALILEO_E5B) {
                                locationExtended.gnss_mb_sv_used_ids.gal_e5b_sv_used_ids_mask
                                        |= bit;
                            }
                        } else {
                            locationExtended.measUsageInfo[idx].gnssSignalType =
                                    GNSS_SIGNAL_GALILEO_E1;
                        }
                    }
                    else if ((gnssSvIdUsed >= QZSS_SV_PRN_MIN) &&
                             (gnssSvIdUsed <= QZSS_SV_PRN_MAX))
                    {
                        uint64_t bit = (1ULL << (gnssSvIdUsed - QZSS_SV_PRN_MIN));
                        locationExtended.gnss_sv_used_ids.qzss_sv_used_ids_mask |= bit;
                        locationExtended.measUsageInfo[idx].gnssConstellation =
                                GNSS_LOC_SV_SYSTEM_QZSS;
                        if (multiBandTypesAvailable) {
                            locationExtended.measUsageInfo[idx].gnssSignalType =
                                    gnssSignalTypeMask;

                            if (locationExtended.measUsageInfo[idx].gnssSignalType &
                                    GNSS_SIGNAL_QZSS_L1CA) {
                                locationExtended.gnss_mb_sv_used_ids.qzss_l1ca_sv_used_ids_mask
                                        |= bit;
                            }
                            if (locationExtended.measUsageInfo[idx].gnssSignalType &
                                    GNSS_SIGNAL_QZSS_L1S) {
                                locationExtended.gnss_mb_sv_used_ids.qzss_l1s_sv_used_ids_mask
                                        |= bit;
                            }
                            if (locationExtended.measUsageInfo[idx].gnssSignalType &
                                    GNSS_SIGNAL_QZSS_L2) {
                                locationExtended.gnss_mb_sv_used_ids.qzss_l2_sv_used_ids_mask
                                        |= bit;
                            }
                            if (locationExtended.measUsageInfo[idx].gnssSignalType &
                                    GNSS_SIGNAL_QZSS_L5) {
                                locationExtended.gnss_mb_sv_used_ids.qzss_l5_sv_used_ids_mask
                                        |= bit;
                            }
                        } else {
                                locationExtended.measUsageInfo[idx].gnssSignalType =
                                        GNSS_SIGNAL_QZSS_L1CA;
                        }
                    }
                    else if ((gnssSvIdUsed >= NAVIC_SV_PRN_MIN) &&
                             (gnssSvIdUsed <= NAVIC_SV_PRN_MAX))
                    {
                        locationExtended.gnss_sv_used_ids.navic_sv_used_ids_mask |=
                                (1ULL << (gnssSvIdUsed - NAVIC_SV_PRN_MIN));
                        locationExtended.measUsageInfo[idx].gnssConstellation =
                                GNSS_LOC_SV_SYSTEM_NAVIC;
                        locationExtended.measUsageInfo[idx].gnssSignalType =
                                (multiBandTypesAvailable ?
                                location_report_ptr->gnssSvUsedSignalTypeList[idx] :
                                GNSS_SIGNAL_NAVIC_L5);
                    }
                }
                locationExtended.flags |= GPS_LOCATION_EXTENDED_HAS_GNSS_SV_USED_DATA;
                if (multiBandTypesAvailable) {
                    locationExtended.flags |= GPS_LOCATION_EXTENDED_HAS_MULTIBAND;
                }
            }
        }

        if (location_report_ptr->navSolutionMask_valid)
        {
            locationExtended.flags |= GPS_LOCATION_EXTENDED_HAS_NAV_SOLUTION_MASK;
            locationExtended.navSolutionMask =
                convertNavSolutionMask(location_report_ptr->navSolutionMask);
        }

        if (location_report_ptr->gpsTime_valid)
        {
            locationExtended.flags |= GPS_LOCATION_EXTENDED_HAS_GPS_TIME;
            locationExtended.gpsTime.gpsWeek = location_report_ptr->gpsTime.gpsWeek;
            locationExtended.gpsTime.gpsTimeOfWeekMs = location_report_ptr->gpsTime.gpsTimeOfWeekMs;
        }

        if (location_report_ptr->extDOP_valid)
        {
            locationExtended.flags |= GPS_LOCATION_EXTENDED_HAS_EXT_DOP;
            locationExtended.extDOP.PDOP = location_report_ptr->extDOP.PDOP;
            locationExtended.extDOP.HDOP = location_report_ptr->extDOP.HDOP;
            locationExtended.extDOP.VDOP = location_report_ptr->extDOP.VDOP;
            locationExtended.extDOP.GDOP = location_report_ptr->extDOP.GDOP;
            locationExtended.extDOP.TDOP = location_report_ptr->extDOP.TDOP;
        }

        if (location_report_ptr->velEnu_valid)
        {
            locationExtended.flags |= GPS_LOCATION_EXTENDED_HAS_EAST_VEL;
            locationExtended.eastVelocity = location_report_ptr->velEnu[0];
            locationExtended.flags |= GPS_LOCATION_EXTENDED_HAS_NORTH_VEL;
            locationExtended.northVelocity = location_report_ptr->velEnu[1];
            locationExtended.flags |= GPS_LOCATION_EXTENDED_HAS_UP_VEL;
            locationExtended.upVelocity = location_report_ptr->velEnu[2];
        }

        if (location_report_ptr->velUncEnu_valid)
        {
            locationExtended.flags |= GPS_LOCATION_EXTENDED_HAS_EAST_VEL_UNC;
            locationExtended.eastVelocityStdDeviation = location_report_ptr->velUncEnu[0];
            locationExtended.flags |= GPS_LOCATION_EXTENDED_HAS_NORTH_VEL_UNC;
            locationExtended.northVelocityStdDeviation = location_report_ptr->velUncEnu[1];
            locationExtended.flags |= GPS_LOCATION_EXTENDED_HAS_UP_VEL_UNC;
            locationExtended.upVelocityStdDeviation = location_report_ptr->velUncEnu[2];
        }
        // fill in GnssSystemTime based on gps timestamp and time uncertainty
        locationExtended.gnssSystemTime.gnssSystemTimeSrc = (Gnss_LocSvSystemEnumType)0;
        if (location_report_ptr->gpsTime_valid)
        {
            locationExtended.gnssSystemTime.gnssSystemTimeSrc = GNSS_LOC_SV_SYSTEM_GPS;
            locationExtended.gnssSystemTime.u.gpsSystemTime.validityMask = 0x0;

            locationExtended.gnssSystemTime.u.gpsSystemTime.systemWeek =
                    locationExtended.gpsTime.gpsWeek;
            locationExtended.gnssSystemTime.u.gpsSystemTime.validityMask |=
                    GNSS_SYSTEM_TIME_WEEK_VALID;

            locationExtended.gnssSystemTime.u.gpsSystemTime.systemMsec =
                    locationExtended.gpsTime.gpsTimeOfWeekMs;
            locationExtended.gnssSystemTime.u.gpsSystemTime.validityMask |=
                    GNSS_SYSTEM_TIME_WEEK_MS_VALID;

            if (location_report_ptr->systemClkTimeBias_valid) {
                locationExtended.gnssSystemTime.u.gpsSystemTime.systemClkTimeBias =
                        location_report_ptr->systemClkTimeBias;
                locationExtended.gnssSystemTime.u.gpsSystemTime.validityMask |=
                        GNSS_SYSTEM_CLK_TIME_BIAS_VALID;
            } else {
                locationExtended.gnssSystemTime.u.gpsSystemTime.systemClkTimeBias = 0.0f;
                locationExtended.gnssSystemTime.u.gpsSystemTime.validityMask |=
                        GNSS_SYSTEM_CLK_TIME_BIAS_VALID;
            }

            if (location_report_ptr->systemClkTimeBiasUnc_valid) {
                locationExtended.gnssSystemTime.u.gpsSystemTime.systemClkTimeUncMs =
                        location_report_ptr->systemClkTimeBiasUnc;
                locationExtended.gnssSystemTime.u.gpsSystemTime.validityMask |=
                        GNSS_SYSTEM_CLK_TIME_BIAS_UNC_VALID;
            } else if (location_report_ptr->timeUnc_valid) {
                locationExtended.gnssSystemTime.u.gpsSystemTime.systemClkTimeUncMs =
                        location_report_ptr->timeUnc;
                locationExtended.gnssSystemTime.u.gpsSystemTime.validityMask |=
                        GNSS_SYSTEM_CLK_TIME_BIAS_UNC_VALID;
            }
        }

        if (location_report_ptr->timeUnc_valid)
        {
            locationExtended.flags |= GPS_LOCATION_EXTENDED_HAS_TIME_UNC;
            locationExtended.timeUncMs = location_report_ptr->timeUnc;
        }

        if (location_report_ptr->leapSeconds_valid)
        {
            locationExtended.flags |= GPS_LOCATION_EXTENDED_HAS_LEAP_SECONDS;
            locationExtended.leapSeconds = location_report_ptr->leapSeconds;
        }

        if (location_report_ptr->dgnssCorrectionSource_valid) {
            locationExtended.flags |=
                    GPS_LOCATION_EXTENDED_HAS_DGNSS_CORRECTION_SOURCE_TYPE;
            locationExtended.dgnssCorrectionSourceType = (LocDgnssCorrectionSourceType)
                    location_report_ptr->dgnssCorrectionSource;
        }

        if (location_report_ptr->dgnssCorrectionSourceID_valid) {
            locationExtended.flags |=
                    GPS_LOCATION_EXTENDED_HAS_DGNSS_CORRECTION_SOURCE_ID;
            locationExtended.dgnssCorrectionSourceID =
                    location_report_ptr->dgnssCorrectionSourceID;
        }

        if (location_report_ptr->dgnssConstellationUsage_valid) {
            locationExtended.flags |=
                    GPS_LOCATION_EXTENDED_HAS_DGNSS_CONSTELLATION_USAGE;
            convertGnssConestellationMask(location_report_ptr->dgnssConstellationUsage,
                                          locationExtended.dgnssConstellationUsage);
        }

        if (location_report_ptr->dgnssRefStationId_valid) {
            locationExtended.flags |=
                    GPS_LOCATION_EXTENDED_HAS_DGNSS_REF_STATION_ID;
            locationExtended.dgnssRefStationId =
                    location_report_ptr->dgnssRefStationId;
        }

        if (location_report_ptr->dgnssDataAgeMsec_valid) {
            locationExtended.flags |=
                    GPS_LOCATION_EXTENDED_HAS_DGNSS_DATA_AGE;
            locationExtended.dgnssDataAgeMsec =
                    location_report_ptr->dgnssDataAgeMsec;
        }

        int64_t elapsedRealTime = -1;
        int64_t unc = -1;
        if (location_report_ptr->systemTick_valid &&
            location_report_ptr->systemTickUnc_valid) {
            LOC_LOGD("Report position to the upper layer");
            /* deal with Qtimer for ElapsedRealTimeNanos */
            elapsedRealTime = ElapsedRealtimeEstimator::getElapsedRealtimeQtimer(
                    location_report_ptr->systemTick);

            /* Uncertainty on HLOS time is 0, so the uncertainty of the difference
               is the uncertainty of the Qtimer in the modem
               Note that location_report_ptr->systemTickUnc is in msec */
            unc = (int64_t)location_report_ptr->systemTickUnc * 1000000;
        } else if (location_report_ptr->timestampUtc_valid == 1) {
            //If Qtimer isn't valid, estimate the elapsedRealTime
            int64_t locationTimeNanos = (int64_t)(location_report_ptr->timestampUtc) * 1000000;
            bool isCurDataTimeTrustable =
                    (locationTimeNanos % ((int64_t)mMinInterval * 1000000) == 0);
            elapsedRealTime = mPositionElapsedRealTimeCal.
                    getElapsedRealtimeEstimateNanos(locationTimeNanos, isCurDataTimeTrustable,
                                                    (int64_t)mMinInterval * 1000000);
            unc = mPositionElapsedRealTimeCal.getElapsedRealtimeUncNanos();
        }

        if (elapsedRealTime != -1) {
            location.gpsLocation.flags |= LOC_GPS_LOCATION_HAS_ELAPSED_REAL_TIME;
            location.gpsLocation.elapsedRealTime = elapsedRealTime;
            location.gpsLocation.elapsedRealTimeUnc = unc;
        }

        LOC_LOGd("Position elapsedRealtime: %" PRIi64 " uncertainty: %" PRIi64 "",
               location.gpsLocation.elapsedRealTime,
               location.gpsLocation.elapsedRealTimeUnc);

        LOC_LOGv("report position mask: 0x%" PRIx64 ", dgnss info: 0x%x %d %d %d %d,",
                 locationExtended.flags,
                 locationExtended.dgnssConstellationUsage,
                 locationExtended.dgnssCorrectionSourceType,
                 locationExtended.dgnssCorrectionSourceID,
                 locationExtended.dgnssDataAgeMsec,
                 locationExtended.dgnssRefStationId);
        LocApiBase::reportPosition(location,
                                   locationExtended,
                                   (location_report_ptr->sessionStatus ==
                                    eQMI_LOC_SESS_STATUS_IN_PROGRESS_V02 ?
                                    LOC_SESS_INTERMEDIATE : LOC_SESS_SUCCESS),
                                   tech_Mask, &dataNotify, msInWeek);
    }
    else
    {
        LocApiBase::reportPosition(location,
                                   locationExtended,
                                   LOC_SESS_FAILURE,
                                   LOC_POS_TECH_MASK_DEFAULT,
                                   &dataNotify, msInWeek);

        LOC_LOGD("%s:%d]: Ignoring position report with sess status = %d, "
                      "fix id = %u\n", __func__, __LINE__,
                      location_report_ptr->sessionStatus,
                      location_report_ptr->fixId );
    }
}

/*convert signal type to carrier frequency*/
float LocApiV02::convertSignalTypeToCarrierFrequency(
    qmiLocGnssSignalTypeMaskT_v02 signalType,
    uint8_t gloFrequency)
{
    float carrierFrequency = 0.0;

    LOC_LOGv("signalType = 0x%" PRIx64 , signalType);
    switch (signalType) {
    case QMI_LOC_MASK_GNSS_SIGNAL_TYPE_GPS_L1CA_V02:
        carrierFrequency = GPS_L1CA_CARRIER_FREQUENCY;
        break;

    case QMI_LOC_MASK_GNSS_SIGNAL_TYPE_GPS_L1C_V02:
        carrierFrequency = GPS_L1C_CARRIER_FREQUENCY;
        break;

    case QMI_LOC_MASK_GNSS_SIGNAL_TYPE_GPS_L2C_L_V02:
        carrierFrequency = GPS_L2C_L_CARRIER_FREQUENCY;
        break;

    case QMI_LOC_MASK_GNSS_SIGNAL_TYPE_GPS_L5_Q_V02:
        carrierFrequency = GPS_L5_Q_CARRIER_FREQUENCY;
        break;

    case QMI_LOC_MASK_GNSS_SIGNAL_TYPE_GLONASS_G1_V02:
        carrierFrequency = GLONASS_G1_CARRIER_FREQUENCY;
        if ((gloFrequency >= 1 && gloFrequency <= 14)) {
            carrierFrequency += ((gloFrequency - 8) * 562500);
        }
        LOC_LOGv("GLO carFreq after conversion = %f", carrierFrequency);
        break;

    case QMI_LOC_MASK_GNSS_SIGNAL_TYPE_GLONASS_G2_V02:
        carrierFrequency = GLONASS_G2_CARRIER_FREQUENCY;
        if ((gloFrequency >= 1 && gloFrequency <= 14)) {
            carrierFrequency += ((gloFrequency - 8) * 437500);
        }
        LOC_LOGv("GLO carFreq after conversion = %f", carrierFrequency);
        break;

    case QMI_LOC_MASK_GNSS_SIGNAL_TYPE_GALILEO_E1_C_V02:
        carrierFrequency = GALILEO_E1_C_CARRIER_FREQUENCY;
        break;

    case QMI_LOC_MASK_GNSS_SIGNAL_TYPE_GALILEO_E5A_Q_V02:
        carrierFrequency = GALILEO_E5A_Q_CARRIER_FREQUENCY;
        break;

    case QMI_LOC_MASK_GNSS_SIGNAL_TYPE_GALILEO_E5B_Q_V02:
        carrierFrequency = GALILEO_E5B_Q_CARRIER_FREQUENCY;
        break;

    case QMI_LOC_MASK_GNSS_SIGNAL_TYPE_BEIDOU_B1_I_V02:
        carrierFrequency = BEIDOU_B1_I_CARRIER_FREQUENCY;
        break;

    case QMI_LOC_MASK_GNSS_SIGNAL_TYPE_BEIDOU_B1C_V02:
        carrierFrequency = BEIDOU_B1C_CARRIER_FREQUENCY;
        break;

    case QMI_LOC_MASK_GNSS_SIGNAL_TYPE_BEIDOU_B2_I_V02:
        carrierFrequency = BEIDOU_B2_I_CARRIER_FREQUENCY;
        break;

    case QMI_LOC_MASK_GNSS_SIGNAL_TYPE_BEIDOU_B2A_I_V02:
        carrierFrequency = BEIDOU_B2A_I_CARRIER_FREQUENCY;
        break;

    case QMI_LOC_MASK_GNSS_SIGNAL_TYPE_QZSS_L1CA_V02:
        carrierFrequency = QZSS_L1CA_CARRIER_FREQUENCY;
        break;

    case QMI_LOC_MASK_GNSS_SIGNAL_TYPE_QZSS_L1S_V02:
        carrierFrequency = QZSS_L1S_CARRIER_FREQUENCY;
        break;

    case QMI_LOC_MASK_GNSS_SIGNAL_TYPE_QZSS_L2C_L_V02:
        carrierFrequency = QZSS_L2C_L_CARRIER_FREQUENCY;
        break;

    case QMI_LOC_MASK_GNSS_SIGNAL_TYPE_QZSS_L5_Q_V02:
        carrierFrequency = QZSS_L5_Q_CARRIER_FREQUENCY;
        break;

    case QMI_LOC_MASK_GNSS_SIGNAL_TYPE_SBAS_L1_CA_V02:
        carrierFrequency = SBAS_L1_CA_CARRIER_FREQUENCY;
        break;

    case QMI_LOC_MASK_GNSS_SIGNAL_TYPE_BEIDOU_B2A_Q_V02:
        carrierFrequency = BEIDOU_B2A_Q_CARRIER_FREQUENCY;
        break;

    case QMI_LOC_MASK_GNSS_SIGNAL_TYPE_NAVIC_L5_V02:
        carrierFrequency = NAVIC_L5_CARRIER_FREQUENCY;
        break;

    default:
        break;
    }
    return carrierFrequency;
}

static GnssSignalTypeMask getDefaultGnssSignalTypeMask(qmiLocSvSystemEnumT_v02 qmiSvSystemType)
{
    GnssSignalTypeMask gnssSignalType = 0;

    switch (qmiSvSystemType) {
    case eQMI_LOC_SV_SYSTEM_GPS_V02:
        gnssSignalType = GNSS_SIGNAL_GPS_L1CA;
        break;
    case eQMI_LOC_SV_SYSTEM_GALILEO_V02:
        gnssSignalType = GNSS_SIGNAL_GALILEO_E1;
        break;
    case eQMI_LOC_SV_SYSTEM_SBAS_V02:
        gnssSignalType = GNSS_SIGNAL_SBAS_L1;
        break;
    case eQMI_LOC_SV_SYSTEM_GLONASS_V02:
        gnssSignalType = GNSS_SIGNAL_GLONASS_G1;
        break;
    case eQMI_LOC_SV_SYSTEM_BDS_V02:
    case eQMI_LOC_SV_SYSTEM_COMPASS_V02:
        gnssSignalType = GNSS_SIGNAL_BEIDOU_B1I;
        break;
    case eQMI_LOC_SV_SYSTEM_QZSS_V02:
        gnssSignalType = GNSS_SIGNAL_QZSS_L1CA;
        break;
    default:
        break;
    }

    return gnssSignalType;
}

/* convert satellite report to location api format and send the converted
   report to base */
void  LocApiV02 :: reportSv (
    const qmiLocEventGnssSvInfoIndMsgT_v02 *gnss_report_ptr)
{
    GnssSvNotification SvNotify = {};
    int              num_svs_max, i;
    const qmiLocSvInfoStructT_v02 *sv_info_ptr;
    uint8_t gloFrequency = 0;

    num_svs_max = 0;
    if (1 == gnss_report_ptr->expandedSvList_valid) {
        num_svs_max = std::min((uint32_t)QMI_LOC_EXPANDED_SV_INFO_LIST_MAX_SIZE_V02,
                                gnss_report_ptr->expandedSvList_len);
    }
    else if (1 == gnss_report_ptr->svList_valid) {
        num_svs_max = std::min((uint32_t)QMI_LOC_MAX_SV_USED_LIST_LENGTH_V02,
                                gnss_report_ptr->svList_len);
    }
    num_svs_max = std::min(num_svs_max, GNSS_SV_MAX);

    SvNotify.size = sizeof(GnssSvNotification);
    if (gnss_report_ptr->gnssSignalTypeList_valid) {
        SvNotify.gnssSignalTypeMaskValid = true;
    }

    if (1 == gnss_report_ptr->svList_valid ||
        1 == gnss_report_ptr->expandedSvList_valid) {
        SvNotify.count = 0;
        if (0 == gnss_report_ptr->expandedSvList_valid ||
            0 == gnss_report_ptr->rfLoss_valid) {
            /*  For modems that don's send rfLoss in the QMI LOC message
                we need to read this info from gps.conf, this is done
                in the constructor */
            for (int i = RF_LOSS_GPS_CONF; i < RF_LOSS_MAX_CONF; i++) {
                if (0 == rfLossNV[i]) {
                    LOC_LOGw("At least one RF_LOSS is 0 in gps.conf, please configure it");
                    break;
                }
            }
        }
        for(i = 0; i < num_svs_max; i++) {
            if (1 == gnss_report_ptr->expandedSvList_valid) {
                sv_info_ptr = &(gnss_report_ptr->expandedSvList[i].svInfo);
            }
            else {
                sv_info_ptr = &(gnss_report_ptr->svList[i]);
            }
            if((sv_info_ptr->validMask & QMI_LOC_SV_INFO_MASK_VALID_SYSTEM_V02) &&
               (sv_info_ptr->validMask & QMI_LOC_SV_INFO_MASK_VALID_GNSS_SVID_V02)
                && (sv_info_ptr->gnssSvId != 0 ))
            {
                GnssSvOptionsMask mask = 0;

                LOC_LOGv("i:%d sv-id:%d count:%d sys:%d en:0x%" PRIx64,
                    i, sv_info_ptr->gnssSvId, SvNotify.count, sv_info_ptr->system,
                    gnss_report_ptr->gnssSignalTypeList[i]);

                GnssSv &gnssSv_ref = SvNotify.gnssSvs[SvNotify.count];
                bool bSvIdIsValid = false;

                gnssSv_ref.size = sizeof(GnssSv);
                gnssSv_ref.svId = sv_info_ptr->gnssSvId;
                if (1 == gnss_report_ptr->expandedSvList_valid) {
                    gnssSv_ref.gloFrequency = gnss_report_ptr->expandedSvList[i].gloFrequency;
                }

                switch (sv_info_ptr->system) {
                case eQMI_LOC_SV_SYSTEM_GPS_V02:
                    gnssSv_ref.type = GNSS_SV_TYPE_GPS;
                    bSvIdIsValid = isValInRangeInclusive(gnssSv_ref.svId,
                                                         GPS_SV_PRN_MIN, GPS_SV_PRN_MAX);
                    break;

                case eQMI_LOC_SV_SYSTEM_GALILEO_V02:
                    gnssSv_ref.type = GNSS_SV_TYPE_GALILEO;
                    bSvIdIsValid = isValInRangeInclusive(gnssSv_ref.svId,
                                                         GAL_SV_PRN_MIN, GAL_SV_PRN_MAX);
                    break;

                case eQMI_LOC_SV_SYSTEM_SBAS_V02:
                    gnssSv_ref.type = GNSS_SV_TYPE_SBAS;
                    bSvIdIsValid = isValInRangeInclusive(gnssSv_ref.svId,
                                                         SBAS_SV_PRN_MIN, SBAS_SV_PRN_MAX);
                    break;

                // Glonass in SV report comes in range of [1, 32] or 255,
                // convert to [65, 96] or 255
                case eQMI_LOC_SV_SYSTEM_GLONASS_V02:
                    gnssSv_ref.type = GNSS_SV_TYPE_GLONASS;
                    if (!isGloSlotUnknown(sv_info_ptr->gnssSvId)) {
                        gnssSv_ref.svId = sv_info_ptr->gnssSvId + GLO_SV_PRN_MIN - 1;
                    }
                    bSvIdIsValid = isValInRangeInclusive(gnssSv_ref.svId, GLO_SV_PRN_MIN,
                                                         GLO_SV_PRN_MAX) ||
                                   isGloSlotUnknown(gnssSv_ref.svId);
                    break;

                case eQMI_LOC_SV_SYSTEM_BDS_V02:
                case eQMI_LOC_SV_SYSTEM_COMPASS_V02:
                    gnssSv_ref.type = GNSS_SV_TYPE_BEIDOU;
                    bSvIdIsValid = isValInRangeInclusive(gnssSv_ref.svId,
                                                         BDS_SV_PRN_MIN, BDS_SV_PRN_MAX);
                    break;

                case eQMI_LOC_SV_SYSTEM_QZSS_V02:
                    gnssSv_ref.type = GNSS_SV_TYPE_QZSS;
                    bSvIdIsValid = isValInRangeInclusive(gnssSv_ref.svId,
                                                         QZSS_SV_PRN_MIN, QZSS_SV_PRN_MAX);
                    break;

                case eQMI_LOC_SV_SYSTEM_NAVIC_V02:
                    gnssSv_ref.type = GNSS_SV_TYPE_NAVIC;
                    bSvIdIsValid = isValInRangeInclusive(gnssSv_ref.svId,
                                                         NAVIC_SV_PRN_MIN, NAVIC_SV_PRN_MAX);
                    break;

                default:
                    gnssSv_ref.type = GNSS_SV_TYPE_UNKNOWN;
                    break;
                }
                if (!bSvIdIsValid) {
                    memset(&gnssSv_ref, 0, sizeof(gnssSv_ref));
                    continue;
                }

                if (sv_info_ptr->validMask & QMI_LOC_SV_INFO_MASK_VALID_ELEVATION_V02) {
                    gnssSv_ref.elevation = sv_info_ptr->elevation;
                }

                if (sv_info_ptr->validMask & QMI_LOC_SV_INFO_MASK_VALID_AZIMUTH_V02) {
                    gnssSv_ref.azimuth = sv_info_ptr->azimuth;
                }

                if (sv_info_ptr->validMask &
                    QMI_LOC_SV_INFO_MASK_VALID_SVINFO_MASK_V02) {
                    if (sv_info_ptr->svInfoMask &
                        QMI_LOC_SVINFO_MASK_HAS_EPHEMERIS_V02) {
                        mask |= GNSS_SV_OPTIONS_HAS_EPHEMER_BIT;
                    }
                    if (sv_info_ptr->svInfoMask &
                        QMI_LOC_SVINFO_MASK_HAS_ALMANAC_V02) {
                        mask |= GNSS_SV_OPTIONS_HAS_ALMANAC_BIT;
                    }
                }

                mask |= GNSS_SV_OPTIONS_HAS_GNSS_SIGNAL_TYPE_BIT;
                if (gnss_report_ptr->gnssSignalTypeList_valid) {
                    if (SvNotify.count > gnss_report_ptr->gnssSignalTypeList_len - 1) {
                        LOC_LOGv("Frequency not available for this SV");
                    }
                    else {
                        if (1 == gnss_report_ptr->expandedSvList_valid) {
                            gloFrequency = gnss_report_ptr->expandedSvList[i].gloFrequency;
                            LOC_LOGv("gloFrequency = 0x%X", gloFrequency);
                        }

                        if (gnss_report_ptr->gnssSignalTypeList[i] != 0) {
                            gnssSv_ref.carrierFrequencyHz =
                                    convertSignalTypeToCarrierFrequency(
                                        gnss_report_ptr->gnssSignalTypeList[i],
                                        gloFrequency);
                            mask |= GNSS_SV_OPTIONS_HAS_CARRIER_FREQUENCY_BIT;
                            gnssSv_ref.gnssSignalTypeMask = convertQmiGnssSignalType(
                                    gnss_report_ptr->gnssSignalTypeList[i]);
                            LOC_LOGd("sv id %d, qmi signal type: 0x%" PRIx64 ", "
                                     "hal signal type: 0x%x", gnssSv_ref.svId,
                                     gnss_report_ptr->gnssSignalTypeList[i],
                                     gnssSv_ref.gnssSignalTypeMask);
                        }
                    }
                } else {
                    gnssSv_ref.gnssSignalTypeMask =
                            getDefaultGnssSignalTypeMask(sv_info_ptr->system);
                    mask |= GNSS_SV_OPTIONS_HAS_CARRIER_FREQUENCY_BIT;
                    gnssSv_ref.carrierFrequencyHz = CarrierFrequencies[gnssSv_ref.type];
                }

                if (sv_info_ptr->validMask & QMI_LOC_SV_INFO_MASK_VALID_SNR_V02) {
                    gnssSv_ref.cN0Dbhz = sv_info_ptr->snr;
                    double rfLoss = gnss_report_ptr->rfLoss[i];
                    if ((1 != gnss_report_ptr->expandedSvList_valid) ||
                        (1 != gnss_report_ptr->rfLoss_valid)) {
                        switch (gnssSv_ref.gnssSignalTypeMask) {
                        case GNSS_SIGNAL_GPS_L1CA:
                        case GNSS_SIGNAL_GPS_L1C:
                        case GNSS_SIGNAL_QZSS_L1CA:
                            rfLoss = rfLossNV[RF_LOSS_GPS_CONF]/10.0;
                            break;
                        case GNSS_SIGNAL_GPS_L5:
                        case GNSS_SIGNAL_QZSS_L5:
                            rfLoss = rfLossNV[RF_LOSS_GPS_L5_CONF]/10.0;
                            break;
                        case GNSS_SIGNAL_BEIDOU_B1I:
                            rfLoss = rfLossNV[RF_LOSS_BDS_CONF]/10.0;
                            break;
                        case GNSS_SIGNAL_BEIDOU_B2AI:
                            rfLoss = rfLossNV[RF_LOSS_BDS_B2A_CONF]/10.0;
                            break;
                        case GNSS_SIGNAL_GLONASS_G1:
                        case GNSS_SIGNAL_GLONASS_G2:
                            {
                                /* compute rfLoss based on rfLossNV[RF_LOSS_GLO_LEFT_CONF],
                                   rfLossNV[RF_LOSS_GLO_CENTER_CONF],
                                   rfLossNV[RF_LOSS_GLO_RIGHT_CONF] and gloFrequency */
                                LocApiProxyBase* locApiProxyObj = getLocApiProxy();
                                if (nullptr != locApiProxyObj &&
                                    ((gloFrequency >= 1 && gloFrequency <= 14))) {
                                    rfLoss = locApiProxyObj->getGloRfLoss(
                                                    rfLossNV[RF_LOSS_GLO_LEFT_CONF],
                                                    rfLossNV[RF_LOSS_GLO_CENTER_CONF],
                                                    rfLossNV[RF_LOSS_GLO_RIGHT_CONF],
                                                    gloFrequency);
                                }
                            }
                            break;
                        case GNSS_SIGNAL_GALILEO_E1:
                            rfLoss = rfLossNV[RF_LOSS_GAL_CONF]/10.0;
                            break;
                        case GNSS_SIGNAL_GALILEO_E5A:
                        case GNSS_SIGNAL_GALILEO_E5B:
                            rfLoss = rfLossNV[RF_LOSS_GAL_E5_CONF]/10.0;
                            break;
                        case GNSS_SIGNAL_NAVIC_L5:
                            rfLoss = rfLossNV[RF_LOSS_NAVIC_CONF]/10.0;
                            break;
                        default:
                            break;
                        }
                    }
                    LOC_LOGv("cN0Dbhz=%f rfLoss=%f gloFrequency=%d",
                             gnssSv_ref.cN0Dbhz, rfLoss, gloFrequency);
                    if (gnssSv_ref.cN0Dbhz > rfLoss) {
                        gnssSv_ref.basebandCarrierToNoiseDbHz = gnssSv_ref.cN0Dbhz - rfLoss;
                    }
                }

                gnssSv_ref.gnssSvOptionsMask = mask;
                SvNotify.count++;
            }
        }
    }

    LocApiBase::reportSv(SvNotify);
}

static Gnss_LocSvSystemEnumType getLocApiSvSystemType (qmiLocSvSystemEnumT_v02 qmiSvSystemType) {
    Gnss_LocSvSystemEnumType locSvSystemType = GNSS_LOC_SV_SYSTEM_UNKNOWN;

    switch (qmiSvSystemType) {
    case eQMI_LOC_SV_SYSTEM_GPS_V02:
        locSvSystemType = GNSS_LOC_SV_SYSTEM_GPS;
        break;

    case eQMI_LOC_SV_SYSTEM_GALILEO_V02:
        locSvSystemType = GNSS_LOC_SV_SYSTEM_GALILEO;
        break;

    case eQMI_LOC_SV_SYSTEM_SBAS_V02:
        locSvSystemType = GNSS_LOC_SV_SYSTEM_SBAS;
        break;

    case eQMI_LOC_SV_SYSTEM_GLONASS_V02:
        locSvSystemType = GNSS_LOC_SV_SYSTEM_GLONASS;
        break;

    case eQMI_LOC_SV_SYSTEM_BDS_V02:
    case eQMI_LOC_SV_SYSTEM_COMPASS_V02:
        locSvSystemType = GNSS_LOC_SV_SYSTEM_BDS;
        break;

    case eQMI_LOC_SV_SYSTEM_QZSS_V02:
        locSvSystemType = GNSS_LOC_SV_SYSTEM_QZSS;
        break;

    case eQMI_LOC_SV_SYSTEM_NAVIC_V02:
        locSvSystemType = GNSS_LOC_SV_SYSTEM_NAVIC;
        break;

    default:
        break;
    }

    return locSvSystemType;
}

/* convert satellite polynomial to loc eng format and  send the converted
   report to loc eng */
void  LocApiV02 :: reportSvPolynomial (
  const qmiLocEventGnssSvPolyIndMsgT_v02 *gnss_sv_poly_ptr)
{
  GnssSvPolynomial  svPolynomial;

  memset(&svPolynomial, 0, sizeof(GnssSvPolynomial));
  svPolynomial.size = sizeof(GnssSvPolynomial);
  svPolynomial.is_valid = 0;

  if(0 != gnss_sv_poly_ptr->gnssSvId)
  {
    svPolynomial.gnssSvId       = gnss_sv_poly_ptr->gnssSvId;
    svPolynomial.T0             = gnss_sv_poly_ptr->T0;

    if(1 == gnss_sv_poly_ptr->gloFrequency_valid)
    {
      svPolynomial.is_valid  |= ULP_GNSS_SV_POLY_BIT_GLO_FREQ;
      svPolynomial.freqNum   = gnss_sv_poly_ptr->gloFrequency;
    }
    if(1 == gnss_sv_poly_ptr->IODE_valid)
    {
      svPolynomial.is_valid  |= ULP_GNSS_SV_POLY_BIT_IODE;
      svPolynomial.iode       = gnss_sv_poly_ptr->IODE;
     }

    if(1 == gnss_sv_poly_ptr->svPosUnc_valid)
    {
      svPolynomial.is_valid |= ULP_GNSS_SV_POLY_BIT_SV_POSUNC;
      svPolynomial.svPosUnc = gnss_sv_poly_ptr->svPosUnc;
    }

    if(0 != gnss_sv_poly_ptr->svPolyFlagValid)
    {
      svPolynomial.is_valid |= ULP_GNSS_SV_POLY_BIT_FLAG;
      svPolynomial.svPolyStatusMaskValidity = gnss_sv_poly_ptr->svPolyFlagValid;
      svPolynomial.svPolyStatusMask = gnss_sv_poly_ptr->svPolyFlags;
    }

    if(1 == gnss_sv_poly_ptr->polyCoeffXYZ0_valid)
    {
      svPolynomial.is_valid |= ULP_GNSS_SV_POLY_BIT_POLYCOEFF_XYZ0;
      for(int i=0;i<GNSS_SV_POLY_XYZ_0_TH_ORDER_COEFF_MAX_SIZE;i++)
      {
        svPolynomial.polyCoeffXYZ0[i] = gnss_sv_poly_ptr->polyCoeffXYZ0[i];
      }
    }

    if(1 == gnss_sv_poly_ptr->polyCoefXYZN_valid)
    {
      svPolynomial.is_valid |= ULP_GNSS_SV_POLY_BIT_POLYCOEFF_XYZN;
      for(int i=0;i<GNSS_SV_POLY_XYZ_N_TH_ORDER_COEFF_MAX_SIZE;i++)
      {
        svPolynomial.polyCoefXYZN[i] = gnss_sv_poly_ptr->polyCoefXYZN[i];
      }
    }

    if(1 == gnss_sv_poly_ptr->polyCoefClockBias_valid)
    {

      svPolynomial.is_valid |= ULP_GNSS_SV_POLY_BIT_POLYCOEFF_OTHER;
      for(int i=0;i<GNSS_SV_POLY_SV_CLKBIAS_COEFF_MAX_SIZE;i++)
      {
        svPolynomial.polyCoefOther[i] = gnss_sv_poly_ptr->polyCoefClockBias[i];
      }
    }

    if(1 == gnss_sv_poly_ptr->ionoDot_valid)
    {
      svPolynomial.is_valid |= ULP_GNSS_SV_POLY_BIT_IONODOT;
      svPolynomial.ionoDot = gnss_sv_poly_ptr->ionoDot;
    }
    if(1 == gnss_sv_poly_ptr->ionoDelay_valid)
    {
      svPolynomial.is_valid |= ULP_GNSS_SV_POLY_BIT_IONODELAY;
      svPolynomial.ionoDelay = gnss_sv_poly_ptr->ionoDelay;
    }

    if(1 == gnss_sv_poly_ptr->sbasIonoDot_valid)
    {
      svPolynomial.is_valid |= ULP_GNSS_SV_POLY_BIT_SBAS_IONODOT;
      svPolynomial.sbasIonoDot = gnss_sv_poly_ptr->sbasIonoDot;
    }
    if(1 == gnss_sv_poly_ptr->sbasIonoDelay_valid)
    {
      svPolynomial.is_valid |= ULP_GNSS_SV_POLY_BIT_SBAS_IONODELAY;
      svPolynomial.sbasIonoDelay = gnss_sv_poly_ptr->sbasIonoDelay;
    }
    if(1 == gnss_sv_poly_ptr->tropoDelay_valid)
    {
      svPolynomial.is_valid |= ULP_GNSS_SV_POLY_BIT_TROPODELAY;
      svPolynomial.tropoDelay = gnss_sv_poly_ptr->tropoDelay;
    }
    if(1 == gnss_sv_poly_ptr->elevation_valid)
    {
      svPolynomial.is_valid |= ULP_GNSS_SV_POLY_BIT_ELEVATION;
      svPolynomial.elevation = gnss_sv_poly_ptr->elevation;
    }
    if(1 == gnss_sv_poly_ptr->elevationDot_valid)
    {
      svPolynomial.is_valid |= ULP_GNSS_SV_POLY_BIT_ELEVATIONDOT;
      svPolynomial.elevationDot = gnss_sv_poly_ptr->elevationDot;
    }
    if(1 == gnss_sv_poly_ptr->elenationUnc_valid)
    {
      svPolynomial.is_valid |= ULP_GNSS_SV_POLY_BIT_ELEVATIONUNC;
      svPolynomial.elevationUnc = gnss_sv_poly_ptr->elenationUnc;
    }
    if(1 == gnss_sv_poly_ptr->velCoef_valid)
    {
      svPolynomial.is_valid |= ULP_GNSS_SV_POLY_BIT_VELO_COEFF;
      for(int i=0;i<GNSS_SV_POLY_VELOCITY_COEF_MAX_SIZE;i++)
      {
        svPolynomial.velCoef[i] = gnss_sv_poly_ptr->velCoef[i];
      }
    }
    if(1 == gnss_sv_poly_ptr->enhancedIOD_valid)
    {
      svPolynomial.is_valid |= ULP_GNSS_SV_POLY_BIT_ENHANCED_IOD;
      svPolynomial.enhancedIOD = gnss_sv_poly_ptr->enhancedIOD;
    }

    if(1 == gnss_sv_poly_ptr->gpsIscL1ca_valid)
    {
        svPolynomial.is_valid |= ULP_GNSS_SV_POLY_BIT_GPS_ISC_L1CA;
        svPolynomial.gpsIscL1ca = gnss_sv_poly_ptr->gpsIscL1ca;
    }

    if(1 == gnss_sv_poly_ptr->gpsIscL2c_valid)
    {
        svPolynomial.is_valid |= ULP_GNSS_SV_POLY_BIT_GPS_ISC_L2C;
        svPolynomial.gpsIscL2c = gnss_sv_poly_ptr->gpsIscL2c;
    }

    if(1 == gnss_sv_poly_ptr->gpsIscL5I5_valid)
    {
        svPolynomial.is_valid |= ULP_GNSS_SV_POLY_BIT_GPS_ISC_L5I5;
        svPolynomial.gpsIscL5I5 = gnss_sv_poly_ptr->gpsIscL5I5;
    }

    if(1 == gnss_sv_poly_ptr->gpsIscL5Q5_valid)
    {
        svPolynomial.is_valid |= ULP_GNSS_SV_POLY_BIT_GPS_ISC_L5Q5;
        svPolynomial.gpsIscL5Q5 = gnss_sv_poly_ptr->gpsIscL5Q5;
    }

    if(1 == gnss_sv_poly_ptr->gpsTgd_valid)
    {
        svPolynomial.is_valid |= ULP_GNSS_SV_POLY_BIT_GPS_TGD;
        svPolynomial.gpsTgd = gnss_sv_poly_ptr->gpsTgd;
    }

    if(1 == gnss_sv_poly_ptr->gloTgdG1G2_valid)
    {
        svPolynomial.is_valid |= ULP_GNSS_SV_POLY_BIT_GLO_TGD_G1G2;
        svPolynomial.gloTgdG1G2 = gnss_sv_poly_ptr->gloTgdG1G2;
    }

    if(1 == gnss_sv_poly_ptr->bdsTgdB1_valid)
    {
        svPolynomial.is_valid |= ULP_GNSS_SV_POLY_BIT_BDS_TGD_B1;
        svPolynomial.bdsTgdB1 = gnss_sv_poly_ptr->bdsTgdB1;
    }

    if(1 == gnss_sv_poly_ptr->bdsTgdB2_valid)
    {
        svPolynomial.is_valid |= ULP_GNSS_SV_POLY_BIT_BDS_TGD_B2;
        svPolynomial.bdsTgdB2= gnss_sv_poly_ptr->bdsTgdB2;
    }

    if(1 == gnss_sv_poly_ptr->bdsTgdB2a_valid)
    {
        svPolynomial.is_valid |= ULP_GNSS_SV_POLY_BIT_BDS_TGD_B2A;
        svPolynomial.bdsTgdB2a = gnss_sv_poly_ptr->bdsTgdB2a;
    }

    if(1 == gnss_sv_poly_ptr->bdsIscB2a_valid)
    {
        svPolynomial.is_valid |= ULP_GNSS_SV_POLY_BIT_BDS_ISC_B2A;
        svPolynomial.bdsIscB2a = gnss_sv_poly_ptr->bdsIscB2a;
    }

    if(1 == gnss_sv_poly_ptr->galBgdE1E5a_valid)
    {
        svPolynomial.is_valid |= ULP_GNSS_SV_POLY_BIT_GAL_BGD_E1E5A;
        svPolynomial.galBgdE1E5a = gnss_sv_poly_ptr->galBgdE1E5a;
    }

    if(1 == gnss_sv_poly_ptr->galBgdE1E5b_valid)
    {
        svPolynomial.is_valid |= ULP_GNSS_SV_POLY_BIT_GAL_BGD_E1E5B;
        svPolynomial.galBgdE1E5b = gnss_sv_poly_ptr->galBgdE1E5b;
    }

    if(1 == gnss_sv_poly_ptr->navicTgdL5_valid)
    {
        svPolynomial.is_valid |= ULP_GNSS_SV_POLY_BIT_NAVIC_TGD_L5;
        svPolynomial.navicTgdL5 = gnss_sv_poly_ptr->navicTgdL5;
    }

    LocApiBase::reportSvPolynomial(svPolynomial);

    LOC_LOGV("[SV_POLY_QMI] SV-Id:%d\n", svPolynomial.gnssSvId);
  }
  else
  {
     LOC_LOGV("[SV_POLY]  INVALID SV-Id:%d", svPolynomial.gnssSvId);
  }
} //reportSvPolynomial

void LocApiV02::reportLocEvent(const qmiLocEventReportIndMsgT_v02 *event_report_ptr)
{
    GnssAidingData aidingData;
    memset(&aidingData, 0, sizeof(aidingData));
    LOC_LOGe("Loc event report: %" PRIu64 " KlobucharIonoMode_valid:%d: leapSec_valid:%d: "
             "tauC_valid:%d featureStatusReport_valid: %d featureStatusReport: %" PRIu64 "",
            event_report_ptr->eventReport, event_report_ptr->klobucharIonoModel_valid,
            event_report_ptr->leapSec_valid, event_report_ptr->tauC_valid,
            event_report_ptr->featureStatusReport_valid, event_report_ptr->featureStatusReport);

    if (event_report_ptr->eventReport & QMI_LOC_DELETE_GPS_EPHEMERIS_ALL_V02) {
        aidingData.sv.svMask |= GNSS_AIDING_DATA_SV_EPHEMERIS_BIT;
        aidingData.sv.svTypeMask |= GNSS_AIDING_DATA_SV_TYPE_GPS_BIT;
    }

    if (event_report_ptr->eventReport & QMI_LOC_DELETE_GLO_EPHEMERIS_ALL_V02) {
        aidingData.sv.svMask |= GNSS_AIDING_DATA_SV_EPHEMERIS_BIT;
        aidingData.sv.svTypeMask |= GNSS_AIDING_DATA_SV_TYPE_GLONASS_BIT;
    }

    if (event_report_ptr->eventReport & QMI_LOC_DELETE_BDS_EPHEMERIS_ALL_V02) {
        aidingData.sv.svMask |= GNSS_AIDING_DATA_SV_EPHEMERIS_BIT;
        aidingData.sv.svTypeMask |= GNSS_AIDING_DATA_SV_TYPE_BEIDOU_BIT;
    }

    if (event_report_ptr->eventReport & QMI_LOC_DELETE_GAL_EPHEMERIS_ALL_V02) {
        aidingData.sv.svMask |= GNSS_AIDING_DATA_SV_EPHEMERIS_BIT;
        aidingData.sv.svTypeMask |= GNSS_AIDING_DATA_SV_TYPE_GALILEO_BIT;
    }

    if (event_report_ptr->eventReport & QMI_LOC_DELETE_QZSS_EPHEMERIS_ALL_V02) {
        aidingData.sv.svMask |= GNSS_AIDING_DATA_SV_EPHEMERIS_BIT;
        aidingData.sv.svTypeMask |= GNSS_AIDING_DATA_SV_TYPE_QZSS_BIT;
    }

    if (event_report_ptr->eventReport & QMI_LOC_DELETE_GPS_SV_POLY_ALL_V02) {
        aidingData.sv.svMask |= GNSS_AIDING_DATA_SV_POLY_BIT;
        aidingData.sv.svTypeMask |= GNSS_AIDING_DATA_SV_TYPE_GPS_BIT;
    }

    if (event_report_ptr->eventReport & QMI_LOC_DELETE_GLO_SV_POLY_ALL_V02) {
        aidingData.sv.svMask |= GNSS_AIDING_DATA_SV_POLY_BIT;
        aidingData.sv.svTypeMask |= GNSS_AIDING_DATA_SV_TYPE_GLONASS_BIT;
    }

    if (event_report_ptr->eventReport & QMI_LOC_DELETE_BDS_SV_POLY_ALL_V02) {
        aidingData.sv.svMask |= GNSS_AIDING_DATA_SV_POLY_BIT;
        aidingData.sv.svTypeMask |= GNSS_AIDING_DATA_SV_TYPE_BEIDOU_BIT;
    }

    if (event_report_ptr->eventReport & QMI_LOC_DELETE_GAL_SV_POLY_ALL_V02) {
        aidingData.sv.svMask |= GNSS_AIDING_DATA_SV_POLY_BIT;
        aidingData.sv.svTypeMask |= GNSS_AIDING_DATA_SV_TYPE_GALILEO_BIT;
    }

    if (event_report_ptr->eventReport & QMI_LOC_DELETE_QZSS_SV_POLY_ALL_V02) {
        aidingData.sv.svMask |= GNSS_AIDING_DATA_SV_POLY_BIT;
        aidingData.sv.svTypeMask |= GNSS_AIDING_DATA_SV_TYPE_QZSS_BIT;
    }

    if (event_report_ptr->eventReport & QMI_LOC_DELETE_GPS_IONO_ALL_V02) {
        aidingData.sv.svMask |= GNSS_AIDING_DATA_SV_IONOSPHERE_BIT;
        aidingData.sv.svTypeMask |= GNSS_AIDING_DATA_SV_TYPE_GPS_BIT;
    }

    if (event_report_ptr->eventReport & QMI_LOC_DELETE_GLO_IONO_ALL_V02) {
        aidingData.sv.svMask |= GNSS_AIDING_DATA_SV_IONOSPHERE_BIT;
        aidingData.sv.svTypeMask |= GNSS_AIDING_DATA_SV_TYPE_GLONASS_BIT;
    }

    if (event_report_ptr->eventReport & QMI_LOC_DELETE_BDS_IONO_ALL_V02) {
        aidingData.sv.svMask |= GNSS_AIDING_DATA_SV_IONOSPHERE_BIT;
        aidingData.sv.svTypeMask |= GNSS_AIDING_DATA_SV_TYPE_BEIDOU_BIT;
    }

    if (event_report_ptr->eventReport & QMI_LOC_DELETE_GAL_IONO_ALL_V02) {
        aidingData.sv.svMask |= GNSS_AIDING_DATA_SV_IONOSPHERE_BIT;
        aidingData.sv.svTypeMask |= GNSS_AIDING_DATA_SV_TYPE_GALILEO_BIT;
    }

    if (event_report_ptr->eventReport & QMI_LOC_DELETE_QZSS_IONO_ALL_V02) {
        aidingData.sv.svMask |= GNSS_AIDING_DATA_SV_IONOSPHERE_BIT;
        aidingData.sv.svTypeMask |= GNSS_AIDING_DATA_SV_TYPE_QZSS_BIT;
    }

    if ((aidingData.sv.svMask != 0) && (aidingData.sv.svTypeMask != 0)) {
        LocApiBase::reportDeleteAidingDataEvent(aidingData);
    }

    // report Klobuchar IonoModel
    if (event_report_ptr->klobucharIonoModel_valid) {
        GnssKlobucharIonoModel klobucharIonoModel;
        memset(&klobucharIonoModel, 0, sizeof(klobucharIonoModel));

        if (event_report_ptr->gpsSystemTime_valid) {
            klobucharIonoModel.isSystemTimeValid = true;
            populateGpsTimeOfReport(event_report_ptr->gpsSystemTime, klobucharIonoModel.systemTime);
        }

        switch(event_report_ptr->klobucharIonoModel.dataSource) {
            case eQMI_LOC_SV_SYSTEM_GPS_V02:
                klobucharIonoModel.gnssConstellation = GNSS_LOC_SV_SYSTEM_GPS;
                break;
            case eQMI_LOC_SV_SYSTEM_GALILEO_V02:
                klobucharIonoModel.gnssConstellation = GNSS_LOC_SV_SYSTEM_GALILEO;
                break;
            case eQMI_LOC_SV_SYSTEM_SBAS_V02:
                klobucharIonoModel.gnssConstellation = GNSS_LOC_SV_SYSTEM_SBAS;
                break;
            case eQMI_LOC_SV_SYSTEM_GLONASS_V02:
                klobucharIonoModel.gnssConstellation = GNSS_LOC_SV_SYSTEM_GLONASS;
                break;
            case eQMI_LOC_SV_SYSTEM_BDS_V02:
            case eQMI_LOC_SV_SYSTEM_COMPASS_V02:
                klobucharIonoModel.gnssConstellation = GNSS_LOC_SV_SYSTEM_BDS;
                break;
            case eQMI_LOC_SV_SYSTEM_QZSS_V02:
                klobucharIonoModel.gnssConstellation = GNSS_LOC_SV_SYSTEM_QZSS;
                break;
        }

        klobucharIonoModel.alpha0 = event_report_ptr->klobucharIonoModel.alpha0;
        klobucharIonoModel.alpha1 = event_report_ptr->klobucharIonoModel.alpha1;
        klobucharIonoModel.alpha2 = event_report_ptr->klobucharIonoModel.alpha2;
        klobucharIonoModel.alpha3 = event_report_ptr->klobucharIonoModel.alpha3;
        klobucharIonoModel.beta0  = event_report_ptr->klobucharIonoModel.beta0;
        klobucharIonoModel.beta1 = event_report_ptr->klobucharIonoModel.beta1;
        klobucharIonoModel.beta2 = event_report_ptr->klobucharIonoModel.beta2;
        klobucharIonoModel.beta3 = event_report_ptr->klobucharIonoModel.beta3;
        LOC_LOGd("iono model: %d:%f:%f:%f:%f:%f:%f:%f:%f:%d",
            klobucharIonoModel.gnssConstellation,  klobucharIonoModel.alpha0,
            klobucharIonoModel.alpha1, klobucharIonoModel.alpha2, klobucharIonoModel.alpha3,
            klobucharIonoModel.beta0, klobucharIonoModel.beta1, klobucharIonoModel.beta2,
            klobucharIonoModel.beta3, klobucharIonoModel.isSystemTimeValid);

        LocApiBase::reportKlobucharIonoModel(klobucharIonoModel);
    }

    // report gnss additional system info
    GnssAdditionalSystemInfo additionalSystemInfo;
    memset(&additionalSystemInfo, 0, sizeof(additionalSystemInfo));
    if (event_report_ptr->leapSec_valid) {
        additionalSystemInfo.validityMask |= GNSS_ADDITIONAL_SYSTEMINFO_HAS_LEAP_SEC;
        additionalSystemInfo.leapSec = event_report_ptr->leapSec;
        LOC_LOGd("LeapSec: %d", event_report_ptr->leapSec);
    }
    if (event_report_ptr->tauC_valid) {
        additionalSystemInfo.validityMask |= GNSS_ADDITIONAL_SYSTEMINFO_HAS_TAUC;
        additionalSystemInfo.tauC = event_report_ptr->tauC;
        LOC_LOGd("tauC: %lf", event_report_ptr->tauC);
    }

    if (additionalSystemInfo.validityMask && event_report_ptr->gpsSystemTime_valid) {
        additionalSystemInfo.isSystemTimeValid = true;
        populateGpsTimeOfReport(event_report_ptr->gpsSystemTime, additionalSystemInfo.systemTime);
        LocApiBase::reportGnssAdditionalSystemInfo(additionalSystemInfo);
    }
    if (event_report_ptr->featureStatusReport_valid) {
        std::unordered_map<LocationQwesFeatureType, bool> featureMap;
        populateFeatureStatusReport(event_report_ptr->featureStatusReport, featureMap);
        LocApiBase::reportQwesCapabilities(featureMap);
    }
}

void LocApiV02::populateFeatureStatusReport
(
        const qmiLocFeaturesStatusMaskT_v02 &featureStatusReport,
        std::unordered_map<LocationQwesFeatureType, bool> &featureMap
)
{
    if (featureStatusReport & QMI_LOC_FEATURE_STATUS_CARRIER_PHASE_V02) {
        featureMap[LOCATION_QWES_FEATURE_TYPE_CARRIER_PHASE] = true;
    } else {
        featureMap[LOCATION_QWES_FEATURE_TYPE_CARRIER_PHASE] = false;
    }
    if (featureStatusReport & QMI_LOC_FEATURE_STATUS_SV_POLYNOMIALS_V02) {
        featureMap[LOCATION_QWES_FEATURE_TYPE_SV_POLYNOMIAL] = true;
    } else {
        featureMap[LOCATION_QWES_FEATURE_TYPE_SV_POLYNOMIAL] = false;
    }
    if (featureStatusReport & QMI_LOC_FEATURE_STATUS_SV_EPHEMERIS_V02) {
        featureMap[LOCATION_QWES_FEATURE_TYPE_SV_EPH] = true;
    } else {
        featureMap[LOCATION_QWES_FEATURE_TYPE_SV_EPH] = false;
    }
    if (featureStatusReport & QMI_LOC_FEATURE_STATUS_SINGLE_FREQUENCY_V02) {
        featureMap[LOCATION_QWES_FEATURE_TYPE_GNSS_SINGLE_FREQUENCY] = true;
    } else {
        featureMap[LOCATION_QWES_FEATURE_TYPE_GNSS_SINGLE_FREQUENCY] = false;
    }
    if (featureStatusReport & QMI_LOC_FEATURE_STATUS_MULTI_FREQUENCY_V02) {
        featureMap[LOCATION_QWES_FEATURE_TYPE_GNSS_MULTI_FREQUENCY] = true;
    } else {
        featureMap[LOCATION_QWES_FEATURE_TYPE_GNSS_MULTI_FREQUENCY] = false;
    }
    if (featureStatusReport & QMI_LOC_FEATURE_STATUS_TIME_FREQUENCY_V02) {
        featureMap[LOCATION_QWES_FEATURE_TYPE_TIME_FREQUENCY] = true;
    } else {
        featureMap[LOCATION_QWES_FEATURE_TYPE_TIME_FREQUENCY] = false;
    }
    if (featureStatusReport & QMI_LOC_FEATURE_STATUS_TIME_UNCERTAINTY_V02) {
        featureMap[LOCATION_QWES_FEATURE_TYPE_TIME_UNCERTAINTY] = true;
    } else {
        featureMap[LOCATION_QWES_FEATURE_TYPE_TIME_UNCERTAINTY] = false;
    }
    if (featureStatusReport & QMI_LOC_FEATURE_STATUS_CLOCK_ESTIMATE_V02) {
        featureMap[LOCATION_QWES_FEATURE_TYPE_CLOCK_ESTIMATE] = true;
    } else {
        featureMap[LOCATION_QWES_FEATURE_TYPE_CLOCK_ESTIMATE] = false;
    }

}

void LocApiV02::reportSvEphemeris (
        uint32_t eventId, const locClientEventIndUnionType &eventPayload) {

    GnssSvEphemerisReport svEphemeris;
    memset(&svEphemeris, 0, sizeof(svEphemeris));

    switch (eventId)
    {
        case QMI_LOC_EVENT_GPS_EPHEMERIS_REPORT_IND_V02:
            svEphemeris.gnssConstellation = GNSS_LOC_SV_SYSTEM_GPS;
            populateGpsEphemeris(eventPayload.pGpsEphemerisReportEvent, svEphemeris);
            break;
        case QMI_LOC_EVENT_GLONASS_EPHEMERIS_REPORT_IND_V02:
            svEphemeris.gnssConstellation = GNSS_LOC_SV_SYSTEM_GLONASS;
            populateGlonassEphemeris(eventPayload.pGloEphemerisReportEvent, svEphemeris);
            break;
        case QMI_LOC_EVENT_BDS_EPHEMERIS_REPORT_IND_V02:
            svEphemeris.gnssConstellation = GNSS_LOC_SV_SYSTEM_BDS;
            populateBdsEphemeris(eventPayload.pBdsEphemerisReportEvent, svEphemeris);
            break;
        case QMI_LOC_EVENT_GALILEO_EPHEMERIS_REPORT_IND_V02:
            svEphemeris.gnssConstellation = GNSS_LOC_SV_SYSTEM_GALILEO;
            populateGalEphemeris(eventPayload.pGalEphemerisReportEvent, svEphemeris);
            break;
        case QMI_LOC_EVENT_QZSS_EPHEMERIS_REPORT_IND_V02:
            svEphemeris.gnssConstellation = GNSS_LOC_SV_SYSTEM_QZSS;
            populateQzssEphemeris(eventPayload.pQzssEphemerisReportEvent, svEphemeris);
    }

    LocApiBase::reportSvEphemeris(svEphemeris);
}

void LocApiV02::populateGpsTimeOfReport(const qmiLocGnssTimeStructT_v02 &inGpsSystemTime,
        GnssSystemTimeStructType& outGpsSystemTime) {

    outGpsSystemTime.validityMask = GNSS_SYSTEM_TIME_WEEK_VALID |  GNSS_SYSTEM_TIME_WEEK_MS_VALID |
            GNSS_SYSTEM_CLK_TIME_BIAS_VALID | GNSS_SYSTEM_CLK_TIME_BIAS_UNC_VALID;

    outGpsSystemTime.systemWeek = inGpsSystemTime.systemWeek;
    outGpsSystemTime.systemMsec = inGpsSystemTime.systemMsec;
    outGpsSystemTime.systemClkTimeBias = inGpsSystemTime.systemClkTimeBias;
    outGpsSystemTime.systemClkTimeUncMs = inGpsSystemTime.systemClkTimeUncMs;
}

void LocApiV02::populateCommonEphemeris(const qmiLocEphGnssDataStructT_v02 &receivedEph,
        GnssEphCommon &ephToFill)
{
    LOC_LOGv("Eph received for sv-id: %d action:%d", receivedEph.gnssSvId,
            receivedEph.updateAction);

    ephToFill.gnssSvId = receivedEph.gnssSvId;
    switch(receivedEph.updateAction) {
        case eQMI_LOC_UPDATE_EPH_SRC_UNKNOWN_V02:
            ephToFill.updateAction = GNSS_EPH_ACTION_UPDATE_SRC_UNKNOWN_V02;
            break;
        case eQMI_LOC_UPDATE_EPH_SRC_OTA_V02:
            ephToFill.updateAction = GNSS_EPH_ACTION_UPDATE_SRC_OTA_V02;
            break;
        case eQMI_LOC_UPDATE_EPH_SRC_NETWORK_V02:
            ephToFill.updateAction = GNSS_EPH_ACTION_UPDATE_SRC_NETWORK_V02;
            break;
        case eQMI_LOC_DELETE_EPH_SRC_UNKNOWN_V02:
            ephToFill.updateAction = GNSS_EPH_ACTION_DELETE_SRC_UNKNOWN_V02;
            break;
        case eQMI_LOC_DELETE_EPH_SRC_NETWORK_V02:
            ephToFill.updateAction = GNSS_EPH_ACTION_DELETE_SRC_NETWORK_V02;
            break;
        case eQMI_LOC_DELETE_EPH_SRC_OTA_V02:
            ephToFill.updateAction = GNSS_EPH_ACTION_DELETE_SRC_OTA_V02;
            break;
    }

    ephToFill.IODE = receivedEph.IODE;
    ephToFill.aSqrt = receivedEph.aSqrt;
    ephToFill.deltaN = receivedEph.deltaN;
    ephToFill.m0 = receivedEph.m0;
    ephToFill.eccentricity = receivedEph.eccentricity;
    ephToFill.omega0 = receivedEph.omega0;
    ephToFill.i0 = receivedEph.i0;
    ephToFill.omega = receivedEph.omega;
    ephToFill.omegaDot = receivedEph.omegaDot;
    ephToFill.iDot = receivedEph.iDot;
    ephToFill.cUc = receivedEph.cUc;
    ephToFill.cUs = receivedEph.cUs;
    ephToFill.cRc = receivedEph.cRc;
    ephToFill.cRs = receivedEph.cRs;
    ephToFill.cIc = receivedEph.cIc;
    ephToFill.cIs = receivedEph.cIs;
    ephToFill.toe = receivedEph.toe;
    ephToFill.toc = receivedEph.toc;
    ephToFill.af0 = receivedEph.af0;
    ephToFill.af1 = receivedEph.af1;
    ephToFill.af2 = receivedEph.af2;
}


void LocApiV02::populateGpsEphemeris(
        const qmiLocGpsEphemerisReportIndMsgT_v02 *gpsEphemeris,
        GnssSvEphemerisReport &svEphemeris)
{
    LOC_LOGd("GPS Ephemeris Received: Len= %d: systemTime_valid%d",
            gpsEphemeris->gpsEphemerisList_len, gpsEphemeris->gpsSystemTime_valid);
    svEphemeris.ephInfo.gpsEphemeris.numOfEphemeris = gpsEphemeris->gpsEphemerisList_len;

    if (gpsEphemeris->gpsSystemTime_valid) {
        svEphemeris.isSystemTimeValid = true;
        populateGpsTimeOfReport(gpsEphemeris->gpsSystemTime, svEphemeris.systemTime);
    }

    for (uint32_t i =0; i < gpsEphemeris->gpsEphemerisList_len; i++) {
        const qmiLocGpsEphemerisT_v02 &receivedGpsEphemeris = gpsEphemeris->gpsEphemerisList[i];
        GpsEphemeris &gpsEphemerisToFill = svEphemeris.ephInfo.gpsEphemeris.gpsEphemerisData[i];

        populateCommonEphemeris(receivedGpsEphemeris.commonEphemerisData,
                gpsEphemerisToFill.commonEphemerisData);

        gpsEphemerisToFill.signalHealth = receivedGpsEphemeris.signalHealth;
        gpsEphemerisToFill.URAI = receivedGpsEphemeris.URAI;
        gpsEphemerisToFill.codeL2 = receivedGpsEphemeris.codeL2;
        gpsEphemerisToFill.dataFlagL2P = receivedGpsEphemeris.dataFlagL2P;
        gpsEphemerisToFill.tgd = receivedGpsEphemeris.tgd;
        gpsEphemerisToFill.fitInterval = receivedGpsEphemeris.fitInterval;
        gpsEphemerisToFill.IODC = receivedGpsEphemeris.IODC;
    }
}

void LocApiV02::populateGlonassEphemeris(const qmiLocGloEphemerisReportIndMsgT_v02 *gloEphemeris,
        GnssSvEphemerisReport &svEphemeris)
{
    LOC_LOGd("GLO Ephemeris Received: Len= %d: systemTime_valid%d",
            gloEphemeris->gloEphemerisList_len, gloEphemeris->gpsSystemTime_valid);
    svEphemeris.ephInfo.glonassEphemeris.numOfEphemeris = gloEphemeris->gloEphemerisList_len;

    if (gloEphemeris->gpsSystemTime_valid) {
        svEphemeris.isSystemTimeValid = true;
        populateGpsTimeOfReport(gloEphemeris->gpsSystemTime, svEphemeris.systemTime);
    }

    for (uint32_t i =0; i < gloEphemeris->gloEphemerisList_len; i++) {
        const qmiLocGloEphemerisT_v02 &receivedGloEphemeris = gloEphemeris->gloEphemerisList[i];
        GlonassEphemeris &gloEphemerisToFill = svEphemeris.ephInfo.glonassEphemeris.gloEphemerisData[i];

        LOC_LOGv("Eph received for sv-id: %d action:%d", receivedGloEphemeris.gnssSvId,
            receivedGloEphemeris.updateAction);

        gloEphemerisToFill.gnssSvId = receivedGloEphemeris.gnssSvId;
        switch(receivedGloEphemeris.updateAction) {
            case eQMI_LOC_UPDATE_EPH_SRC_UNKNOWN_V02:
                gloEphemerisToFill.updateAction = GNSS_EPH_ACTION_UPDATE_SRC_UNKNOWN_V02;
                break;
            case eQMI_LOC_UPDATE_EPH_SRC_OTA_V02:
                gloEphemerisToFill.updateAction = GNSS_EPH_ACTION_UPDATE_SRC_OTA_V02;
                break;
            case eQMI_LOC_UPDATE_EPH_SRC_NETWORK_V02:
                gloEphemerisToFill.updateAction = GNSS_EPH_ACTION_UPDATE_SRC_NETWORK_V02;
                break;
            case eQMI_LOC_DELETE_EPH_SRC_UNKNOWN_V02:
                gloEphemerisToFill.updateAction = GNSS_EPH_ACTION_DELETE_SRC_UNKNOWN_V02;
                break;
            case eQMI_LOC_DELETE_EPH_SRC_NETWORK_V02:
                gloEphemerisToFill.updateAction = GNSS_EPH_ACTION_DELETE_SRC_NETWORK_V02;
                break;
            case eQMI_LOC_DELETE_EPH_SRC_OTA_V02:
                gloEphemerisToFill.updateAction = GNSS_EPH_ACTION_DELETE_SRC_OTA_V02;
                break;
        }

        gloEphemerisToFill.bnHealth = receivedGloEphemeris.bnHealth;
        gloEphemerisToFill.lnHealth = receivedGloEphemeris.lnHealth;
        gloEphemerisToFill.tb = receivedGloEphemeris.tb;
        gloEphemerisToFill.ft = receivedGloEphemeris.ft;
        gloEphemerisToFill.gloM = receivedGloEphemeris.gloM;
        gloEphemerisToFill.enAge = receivedGloEphemeris.enAge;
        gloEphemerisToFill.gloFrequency = receivedGloEphemeris.gloFrequency;
        gloEphemerisToFill.p1 = receivedGloEphemeris.p1;
        gloEphemerisToFill.p2 = receivedGloEphemeris.p2;
        gloEphemerisToFill.deltaTau = receivedGloEphemeris.deltaTau;
        memcpy(gloEphemerisToFill.position, receivedGloEphemeris.position,
              sizeof(*receivedGloEphemeris.position) * 3);
        memcpy(gloEphemerisToFill.velocity, receivedGloEphemeris.velocity,
              sizeof(*receivedGloEphemeris.velocity) * 3);
        memcpy(gloEphemerisToFill.acceleration, receivedGloEphemeris.acceleration,
              sizeof(*receivedGloEphemeris.acceleration) * 3);
        gloEphemerisToFill.tauN = receivedGloEphemeris.tauN;
        gloEphemerisToFill.gamma = receivedGloEphemeris.gamma;
        gloEphemerisToFill.toe = receivedGloEphemeris.toe;
        gloEphemerisToFill.nt = receivedGloEphemeris.nt;
    }
}

void LocApiV02::populateBdsEphemeris(const qmiLocBdsEphemerisReportIndMsgT_v02 *bdsEphemeris,
        GnssSvEphemerisReport &svEphemeris)
{
    LOC_LOGd("BDS Ephemeris Received: Len= %d: systemTime_valid%d",
            bdsEphemeris->bdsEphemerisList_len, bdsEphemeris->gpsSystemTime_valid);
    svEphemeris.ephInfo.bdsEphemeris.numOfEphemeris = bdsEphemeris->bdsEphemerisList_len;

    if (bdsEphemeris->gpsSystemTime_valid) {
        svEphemeris.isSystemTimeValid = true;
        populateGpsTimeOfReport(bdsEphemeris->gpsSystemTime, svEphemeris.systemTime);
    }


    for (uint32_t i =0; i < bdsEphemeris->bdsEphemerisList_len; i++) {
        const qmiLocBdsEphemerisT_v02 &receivedBdsEphemeris = bdsEphemeris->bdsEphemerisList[i];
        BdsEphemeris &bdsEphemerisToFill = svEphemeris.ephInfo.bdsEphemeris.bdsEphemerisData[i];

        populateCommonEphemeris(receivedBdsEphemeris.commonEphemerisData,
                bdsEphemerisToFill.commonEphemerisData);

        bdsEphemerisToFill.svHealth = receivedBdsEphemeris.svHealth;
        bdsEphemerisToFill.AODC = receivedBdsEphemeris.AODC;
        bdsEphemerisToFill.tgd1 = receivedBdsEphemeris.tgd1;
        bdsEphemerisToFill.tgd2 = receivedBdsEphemeris.tgd2;
        bdsEphemerisToFill.URAI = receivedBdsEphemeris.URAI;
    }
}

void LocApiV02::populateGalEphemeris(const qmiLocGalEphemerisReportIndMsgT_v02 *galEphemeris,
        GnssSvEphemerisReport &svEphemeris)
{
    LOC_LOGd("GAL Ephemeris Received: Len= %d: systemTime_valid%d",
            galEphemeris->galEphemerisList_len, galEphemeris->gpsSystemTime_valid);
    svEphemeris.ephInfo.galileoEphemeris.numOfEphemeris = galEphemeris->galEphemerisList_len;

    if (galEphemeris->gpsSystemTime_valid) {
        svEphemeris.isSystemTimeValid = true;
        populateGpsTimeOfReport(galEphemeris->gpsSystemTime, svEphemeris.systemTime);
    }


    for (uint32_t i =0; i < galEphemeris->galEphemerisList_len; i++) {
        const qmiLocGalEphemerisT_v02 &receivedGalEphemeris = galEphemeris->galEphemerisList[i];
        GalileoEphemeris &galEphemerisToFill = svEphemeris.ephInfo.galileoEphemeris.galEphemerisData[i];

        populateCommonEphemeris(receivedGalEphemeris.commonEphemerisData,
                galEphemerisToFill.commonEphemerisData);

        switch (receivedGalEphemeris.dataSourceSignal) {
            case eQMI_LOC_GAL_EPH_SIGNAL_SRC_UNKNOWN_V02:
                galEphemerisToFill.dataSourceSignal = GAL_EPH_SIGNAL_SRC_UNKNOWN_V02;
                break;
            case eQMI_LOC_GAL_EPH_SIGNAL_SRC_E1B_V02:
                galEphemerisToFill.dataSourceSignal = GAL_EPH_SIGNAL_SRC_E1B_V02;
                break;
            case eQMI_LOC_GAL_EPH_SIGNAL_SRC_E5A_V02:
                galEphemerisToFill.dataSourceSignal = GAL_EPH_SIGNAL_SRC_E5A_V02;
                break;
            case eQMI_LOC_GAL_EPH_SIGNAL_SRC_E5B_V02:
                galEphemerisToFill.dataSourceSignal = GAL_EPH_SIGNAL_SRC_E5B_V02;
        }

        galEphemerisToFill.sisIndex = receivedGalEphemeris.sisIndex;
        galEphemerisToFill.bgdE1E5a = receivedGalEphemeris.bgdE1E5a;
        galEphemerisToFill.bgdE1E5b = receivedGalEphemeris.bgdE1E5b;
        galEphemerisToFill.svHealth = receivedGalEphemeris.svHealth;
    }
}

void LocApiV02::populateQzssEphemeris(const qmiLocQzssEphemerisReportIndMsgT_v02 *qzssEphemeris,
        GnssSvEphemerisReport &svEphemeris)
{
    LOC_LOGd("QZSS Ephemeris Received: Len= %d: systemTime_valid%d",
            qzssEphemeris->qzssEphemerisList_len, qzssEphemeris->gpsSystemTime_valid);
    svEphemeris.ephInfo.qzssEphemeris.numOfEphemeris = qzssEphemeris->qzssEphemerisList_len;

    if (qzssEphemeris->gpsSystemTime_valid) {
        svEphemeris.isSystemTimeValid = true;
        populateGpsTimeOfReport(qzssEphemeris->gpsSystemTime, svEphemeris.systemTime);
    }


    for (uint32_t i =0; i < qzssEphemeris->qzssEphemerisList_len; i++) {
        const qmiLocGpsEphemerisT_v02 &receivedQzssEphemeris = qzssEphemeris->qzssEphemerisList[i];
        GpsEphemeris &qzssEphemerisToFill = svEphemeris.ephInfo.qzssEphemeris.qzssEphemerisData[i];

        populateCommonEphemeris(receivedQzssEphemeris.commonEphemerisData,
                qzssEphemerisToFill.commonEphemerisData);

        qzssEphemerisToFill.signalHealth = receivedQzssEphemeris.signalHealth;
        qzssEphemerisToFill.URAI = receivedQzssEphemeris.URAI;
        qzssEphemerisToFill.codeL2 = receivedQzssEphemeris.codeL2;
        qzssEphemerisToFill.dataFlagL2P = receivedQzssEphemeris.dataFlagL2P;
        qzssEphemerisToFill.tgd = receivedQzssEphemeris.tgd;
        qzssEphemerisToFill.fitInterval = receivedQzssEphemeris.fitInterval;
        qzssEphemerisToFill.IODC = receivedQzssEphemeris.IODC;
    }
}

/* convert system info to GnssDataNotification format and dispatch
   to the registered adapter */
void  LocApiV02 :: reportSystemInfo(
    const qmiLocSystemInfoIndMsgT_v02* system_info_ptr){

    if (system_info_ptr == nullptr) {
        return;
    }

    LOC_LOGe("system info type: %d, leap second valid: %d "
             "current gps time:valid:%d, week: %d, msec: %d,"
             "current leap second:valid %d, seconds %d, "
             "next gps time: valid %d, week: %d, msec: %d,"
             "next leap second: valid %d, seconds %d",
             system_info_ptr->systemInfo,
             system_info_ptr->nextLeapSecondInfo_valid,
             system_info_ptr->nextLeapSecondInfo.gpsTimeCurrent_valid,
             system_info_ptr->nextLeapSecondInfo.gpsTimeCurrent.gpsWeek,
             system_info_ptr->nextLeapSecondInfo.gpsTimeCurrent.gpsTimeOfWeekMs,
             system_info_ptr->nextLeapSecondInfo.leapSecondsCurrent_valid,
             system_info_ptr->nextLeapSecondInfo.leapSecondsCurrent,
             system_info_ptr->nextLeapSecondInfo.gpsTimeNextLsEvent_valid,
             system_info_ptr->nextLeapSecondInfo.gpsTimeNextLsEvent.gpsWeek,
             system_info_ptr->nextLeapSecondInfo.gpsTimeNextLsEvent.gpsTimeOfWeekMs,
             system_info_ptr->nextLeapSecondInfo.leapSecondsNext_valid,
             system_info_ptr->nextLeapSecondInfo.leapSecondsNext);

    LocationSystemInfo systemInfo = {};
    if ((system_info_ptr->systemInfo == eQMI_LOC_NEXT_LEAP_SECOND_INFO_V02) &&
        (system_info_ptr->nextLeapSecondInfo_valid == 1)) {

        const qmiLocNextLeapSecondInfoStructT_v02 &nextLeapSecondInfo =
            system_info_ptr->nextLeapSecondInfo;

        if (nextLeapSecondInfo.gpsTimeNextLsEvent_valid &&
                nextLeapSecondInfo.leapSecondsCurrent_valid &&
                nextLeapSecondInfo.leapSecondsNext_valid) {

            systemInfo.systemInfoMask |= LOCATION_SYS_INFO_LEAP_SECOND;
            systemInfo.leapSecondSysInfo.leapSecondInfoMask |=
                    LEAP_SECOND_SYS_INFO_LEAP_SECOND_CHANGE_BIT;

            LeapSecondChangeInfo &leapSecondChangeInfo =
                    systemInfo.leapSecondSysInfo.leapSecondChangeInfo;
            leapSecondChangeInfo.gpsTimestampLsChange.systemWeek =
                    nextLeapSecondInfo.gpsTimeNextLsEvent.gpsWeek;
            leapSecondChangeInfo.gpsTimestampLsChange.systemMsec =
                    nextLeapSecondInfo.gpsTimeNextLsEvent.gpsTimeOfWeekMs;
            leapSecondChangeInfo.gpsTimestampLsChange.validityMask =
                    (GNSS_SYSTEM_TIME_WEEK_VALID | GNSS_SYSTEM_TIME_WEEK_MS_VALID);

            leapSecondChangeInfo.leapSecondsBeforeChange =
                    nextLeapSecondInfo.leapSecondsCurrent;
            leapSecondChangeInfo.leapSecondsAfterChange =
                    nextLeapSecondInfo.leapSecondsNext;
        }

        if (nextLeapSecondInfo.leapSecondsCurrent_valid) {
            systemInfo.systemInfoMask |= LOCATION_SYS_INFO_LEAP_SECOND;
            systemInfo.leapSecondSysInfo.leapSecondInfoMask |=
                    LEAP_SECOND_SYS_INFO_CURRENT_LEAP_SECONDS_BIT;
            systemInfo.leapSecondSysInfo.leapSecondCurrent =
                    nextLeapSecondInfo.leapSecondsCurrent;
        }
    }

    if (systemInfo.systemInfoMask) {
        LocApiBase::reportLocationSystemInfo(systemInfo);
    }
}

void LocApiV02::reportLocationRequestNotification(
    const qmiLocLocationRequestNotificationIndMsgT_v02* loc_req_notif)
{
    GnssNfwNotification notification = {};

    LOC_LOGv("IN: protocolStack=%d"
             " ,clientStrId_valid=%d"
             " ,clientStrId=%s"
             " ,requestor=%d"
             " ,requestorId=%s"
             " ,responseType=%d"
             " ,inEmergencyMode=%d"
             " ,isCachedLocation=%u",
             loc_req_notif->protocolStack,
             loc_req_notif->clientStrId_valid,
             loc_req_notif->clientStrId,
             loc_req_notif->requestor,
             loc_req_notif->requestorId,
             loc_req_notif->responseType,
             loc_req_notif->inEmergencyMode,
             loc_req_notif->isCachedLocation);

    switch (loc_req_notif->protocolStack) {
    case eQMI_LOC_CTRL_PLANE_V02:
        notification.protocolStack = GNSS_NFW_CTRL_PLANE;
        break;
    case eQMI_LOC_SUPL_V02:
        notification.protocolStack = GNSS_NFW_SUPL;
        break;
    case eQMI_LOC_IMS_V02:
        notification.protocolStack = GNSS_NFW_IMS;
        break;
    case eQMI_LOC_SIM_V02:
        notification.protocolStack = GNSS_NFW_SIM;
        break;
    case eQMI_LOC_MDT_V02:
    case eQMI_LOC_TLOC_V02:
    case eQMI_LOC_OTHER_V02:
    default:
        notification.protocolStack = GNSS_NFW_OTHER_PROTOCOL_STACK;
        if (loc_req_notif->clientStrId_valid) {
            strlcpy(notification.otherProtocolStackName,
                    loc_req_notif->clientStrId,
                    sizeof(notification.otherProtocolStackName));
        } else {
            strlcpy(notification.otherProtocolStackName,
                    "NFW Client",
                    sizeof(notification.otherProtocolStackName));
        }
        break;
    }
    switch (loc_req_notif->requestor) {
    case eQMI_LOC_REQUESTOR_CARRIER_V02:
        notification.requestor = GNSS_NFW_CARRIER;
        break;
    case eQMI_LOC_REQUESTOR_OEM_V02:
        notification.requestor = GNSS_NFW_OEM;
        break;
    case eQMI_LOC_REQUESTOR_MODEM_CHIPSET_VENDOR_V02:
        notification.requestor = GNSS_NFW_MODEM_CHIPSET_VENDOR;
        break;
    case eQMI_LOC_REQUESTOR_GNSS_CHIPSET_VENDOR_V02:
        notification.requestor = GNSS_NFW_GNSS_CHIPSET_VENDOR;
        break;
    case eQMI_LOC_REQUESTOR_OTHER_CHIPSET_VENDOR_V02:
        notification.requestor = GNSS_NFW_OTHER_CHIPSET_VENDOR;
        break;
    case eQMI_LOC_REQUESTOR_AUTOMOBILE_CLIENT_V02:
        notification.requestor = GNSS_NFW_AUTOMOBILE_CLIENT;
        break;
    case eQMI_LOC_REQUESTOR_OTHER_V02:
    default:
        notification.requestor = GNSS_NFW_OTHER_REQUESTOR;
        break;
    }
    switch (loc_req_notif->responseType) {
    case eQMI_LOC_REJECTED_V02:
        notification.responseType = GNSS_NFW_REJECTED;
        break;
    case eQMI_LOC_ACCEPTED_NO_LOCATION_PROVIDED_V02:
        notification.responseType = GNSS_NFW_ACCEPTED_NO_LOCATION_PROVIDED;
        break;
    case eQMI_LOC_ACCEPTED_LOCATION_PROVIDED_V02:
        notification.responseType = GNSS_NFW_ACCEPTED_LOCATION_PROVIDED;
        break;
    }
    notification.inEmergencyMode = (bool)loc_req_notif->inEmergencyMode;
    notification.isCachedLocation = (bool)loc_req_notif->isCachedLocation;

    bool isImpossibleScenario = false;
    // we need to reject various impossible scenarios
    if (notification.inEmergencyMode) {
        if (notification.protocolStack != GNSS_NFW_CTRL_PLANE &&
            notification.protocolStack != GNSS_NFW_SUPL) {
            isImpossibleScenario = true;
            LOC_LOGe("inEmergencyMode is true, but protocolStack=%d",
                notification.protocolStack);
        }
        if (GNSS_NFW_REJECTED == notification.responseType) {
            isImpossibleScenario = true;
            LOC_LOGe("inEmergencyMode is true, but responseType is REJECTED");
        }
    } else {
        // requestorId is "" for emergency
        strlcpy(notification.requestorId,
                loc_req_notif->requestorId,
                sizeof(notification.requestorId));
        // proxyAppPackageName is "" for emergency
        strlcpy(notification.proxyAppPackageName,
                ContextBase::mGps_conf.PROXY_APP_PACKAGE_NAME,
                sizeof(notification.proxyAppPackageName));
    }

    if (!isImpossibleScenario) {
        LOC_LOGv("OUT: proxyAppPackageName=%s"
                 " ,protocolStack=%d"
                 " ,otherProtocolStackName=%s"
                 " ,requestor=%d"
                 " ,requestorId=%s"
                 " ,responseType=%d"
                 " ,inEmergencyMode=%d"
                 " ,isCachedLocation=%u",
                 notification.proxyAppPackageName,
                 notification.protocolStack,
                 notification.otherProtocolStackName,
                 notification.requestor,
                 notification.requestorId,
                 notification.responseType,
                 notification.inEmergencyMode,
                 notification.isCachedLocation);

        LocApiBase::sendNfwNotification(notification);
    }
}

/* convert engine state report to loc eng format and send the converted
   report to loc eng */
void LocApiV02 :: reportEngineState (
    const qmiLocEventEngineStateIndMsgT_v02 *engine_state_ptr)
{

  LOC_LOGV("%s:%d]: state = %d\n", __func__, __LINE__,
                 engine_state_ptr->engineState);

  struct MsgUpdateEngineState : public LocMsg {
      LocApiV02* mpLocApiV02;
      bool mEngineOn;
      inline MsgUpdateEngineState(LocApiV02* pLocApiV02, bool engineOn) :
                 LocMsg(), mpLocApiV02(pLocApiV02), mEngineOn(engineOn) {}
      inline virtual void proc() const {

          // Call registerEventMask so that if qmi mask has changed,
          // it will register the new qmi mask to the modem
          mpLocApiV02->mEngineOn = mEngineOn;
          mpLocApiV02->registerEventMask(mpLocApiV02->mMask);

          if (!mEngineOn) {
              for (auto resender : mpLocApiV02->mResenders) {
                  LOC_LOGV("%s:%d]: resend failed command.", __func__, __LINE__);
                  resender();
              }
              mpLocApiV02->mResenders.clear();
          }
      }
  };

  if (eQMI_LOC_ENGINE_STATE_ON_V02 == engine_state_ptr->engineState) {
      sendMsg(new MsgUpdateEngineState(this, true));
  }
  else if (eQMI_LOC_ENGINE_STATE_OFF_V02 == engine_state_ptr->engineState) {
      sendMsg(new MsgUpdateEngineState(this, false));
  }

}

/* convert fix session state report to loc eng format and send the converted
   report to loc eng */
void LocApiV02 :: reportFixSessionState (
    const qmiLocEventFixSessionStateIndMsgT_v02 *fix_session_state_ptr)
{
  LocGpsStatusValue status;
  LOC_LOGD("%s:%d]: state = %d\n", __func__, __LINE__,
                fix_session_state_ptr->sessionState);

  status = LOC_GPS_STATUS_NONE;
  if (fix_session_state_ptr->sessionState == eQMI_LOC_FIX_SESSION_STARTED_V02)
  {
    status = LOC_GPS_STATUS_SESSION_BEGIN;
  }
  else if (fix_session_state_ptr->sessionState
           == eQMI_LOC_FIX_SESSION_FINISHED_V02)
  {
    status = LOC_GPS_STATUS_SESSION_END;
  }
  reportStatus(status);
}

/* convert NMEA report to loc eng format and send the converted
   report to loc eng */
void LocApiV02 :: reportNmea (
  const qmiLocEventNmeaIndMsgT_v02 *nmea_report_ptr)
{
    if (NULL == nmea_report_ptr) {
        return;
    }

    const char* p_nmea = NULL;
    uint32_t q_nmea_len = 0;

    if (nmea_report_ptr->expandedNmea_valid) {
        p_nmea = nmea_report_ptr->expandedNmea;
        q_nmea_len = strlen(nmea_report_ptr->expandedNmea);
        if (q_nmea_len > QMI_LOC_EXPANDED_NMEA_STRING_MAX_LENGTH_V02) {
            q_nmea_len = QMI_LOC_EXPANDED_NMEA_STRING_MAX_LENGTH_V02;
        }
    }
    else
    {
        p_nmea = nmea_report_ptr->nmea;
        q_nmea_len = strlen(nmea_report_ptr->nmea);
        if (q_nmea_len > QMI_LOC_NMEA_STRING_MAX_LENGTH_V02) {
            q_nmea_len = QMI_LOC_NMEA_STRING_MAX_LENGTH_V02;
        }
    }

    if ((NULL != p_nmea) && (q_nmea_len > 0)) {
        LocApiBase::reportNmea(p_nmea, q_nmea_len);
    }
}

/* convert and report an ATL request to loc engine */
void LocApiV02 :: reportAtlRequest(
  const qmiLocEventLocationServerConnectionReqIndMsgT_v02 * server_request_ptr)
{
  uint32_t connHandle = server_request_ptr->connHandle;

  if (server_request_ptr->requestType == eQMI_LOC_SERVER_REQUEST_OPEN_V02)
  {
    LocAGpsType agpsType = LOC_AGPS_TYPE_ANY;
    LocApnTypeMask apnTypeMask = 0;

    switch (server_request_ptr->wwanType)
    {
      case eQMI_LOC_WWAN_TYPE_INTERNET_V02:
        agpsType = LOC_AGPS_TYPE_WWAN_ANY;
        break;
      case eQMI_LOC_WWAN_TYPE_AGNSS_V02:
        agpsType = LOC_AGPS_TYPE_SUPL;
        break;
      case eQMI_LOC_WWAN_TYPE_AGNSS_EMERGENCY_V02:
        agpsType = LOC_AGPS_TYPE_SUPL_ES;
        break;
      default:
        agpsType = LOC_AGPS_TYPE_WWAN_ANY;
        break;
    }

    if (server_request_ptr->apnTypeMask_valid) {
        apnTypeMask = convertQmiLocApnTypeMask(server_request_ptr->apnTypeMask);
    }
    LOC_LOGd("handle=%d agpsType=0x%X apnTypeMask=0x%X",
        connHandle, agpsType, apnTypeMask);
    requestATL(connHandle, agpsType, apnTypeMask);
  }
  // service the ATL close request
  else if (server_request_ptr->requestType == eQMI_LOC_SERVER_REQUEST_CLOSE_V02)
  {
    releaseATL(connHandle);
  }
}

/* conver the NI report to loc eng format and send t loc engine */
void LocApiV02 :: reportNiRequest(
    const qmiLocEventNiNotifyVerifyReqIndMsgT_v02 *ni_req_ptr)
{
    GnssNiNotification notif = {};
    notif.messageEncoding = GNSS_NI_ENCODING_TYPE_NONE ;
    notif.requestorEncoding = GNSS_NI_ENCODING_TYPE_NONE;
    notif.timeoutResponse = GNSS_NI_RESPONSE_NO_RESPONSE;
    notif.timeout = LOC_NI_NO_RESPONSE_TIME;

    /* Handle Vx request */
    if (1 == ni_req_ptr->NiVxInd_valid) {
        const qmiLocNiVxNotifyVerifyStructT_v02 *vx_req = &(ni_req_ptr->NiVxInd);

        notif.type = GNSS_NI_TYPE_VOICE;

        // Requestor ID, the requestor id recieved is NULL terminated
        hexcode(notif.requestor, sizeof notif.requestor,
                (char *)vx_req->requestorId, vx_req->requestorId_len );
    }

    /* Handle UMTS CP request*/
    else if (1 == ni_req_ptr->NiUmtsCpInd_valid) {
        const qmiLocNiUmtsCpNotifyVerifyStructT_v02 *umts_cp_req =
            &ni_req_ptr->NiUmtsCpInd;

        notif.type = GNSS_NI_TYPE_CONTROL_PLANE;

        /* notificationText should always be a NULL terminated string */
        hexcode(notif.message, sizeof notif.message,
                (char *)umts_cp_req->notificationText,
                umts_cp_req->notificationText_len);

        /* Store requestor ID */
        hexcode(notif.requestor, sizeof(notif.requestor),
                (char *)umts_cp_req->requestorId.codedString,
                umts_cp_req->requestorId.codedString_len);

        /* convert encodings */
        notif.messageEncoding = convertNiEncoding(umts_cp_req->dataCodingScheme);

        notif.requestorEncoding =
            convertNiEncoding(umts_cp_req->requestorId.dataCodingScheme);

        /* LCS address (using extras field) */
        if (0 != umts_cp_req->clientAddress_len) {
            char lcs_addr[32]; // Decoded LCS address for UMTS CP NI

            // Copy LCS Address into notif.extras in the format: Address = 012345
            strlcat(notif.extras, LOC_NI_NOTIF_KEY_ADDRESS, sizeof (notif.extras));
            strlcat(notif.extras, " = ", sizeof notif.extras);
            int addr_len = 0;
            const char *address_source = NULL;
            address_source = (char *)umts_cp_req->clientAddress;
            // client Address is always NULL terminated
            addr_len = decodeAddress(lcs_addr, sizeof(lcs_addr), address_source,
                                     umts_cp_req->clientAddress_len);

            // The address is ASCII string
            if (addr_len) {
                strlcat(notif.extras, lcs_addr, sizeof notif.extras);
            }
        }
    } else if (1 == ni_req_ptr->NiSuplInd_valid) {
        const qmiLocNiSuplNotifyVerifyStructT_v02 *supl_req =
            &ni_req_ptr->NiSuplInd;

        notif.type = GNSS_NI_TYPE_SUPL;

        // Client name
        if (supl_req->valid_flags & QMI_LOC_SUPL_CLIENT_NAME_MASK_V02) {
            hexcode(notif.message, sizeof(notif.message),
                    (char *)supl_req->clientName.formattedString,
                    supl_req->clientName.formattedString_len);
            LOC_LOGv("SUPL NI: client_name: %s \n", notif.message);
        } else {
            LOC_LOGv("SUPL NI: client_name not present.");
        }

        // Requestor ID
        if (supl_req->valid_flags & QMI_LOC_SUPL_REQUESTOR_ID_MASK_V02) {
            hexcode(notif.requestor, sizeof notif.requestor,
                    (char*)supl_req->requestorId.formattedString,
                    supl_req->requestorId.formattedString_len);

            LOC_LOGv("SUPL NI: requestor: %s", notif.requestor);
        } else {
            LOC_LOGv("SUPL NI: requestor not present.");
        }

        // Encoding type
        if (supl_req->valid_flags & QMI_LOC_SUPL_DATA_CODING_SCHEME_MASK_V02) {
            notif.messageEncoding = convertNiEncoding(supl_req->dataCodingScheme);
            notif.requestorEncoding = convertNiEncoding(supl_req->dataCodingScheme);
        } else {
            notif.messageEncoding = notif.requestorEncoding = GNSS_NI_ENCODING_TYPE_NONE;
        }

        // ES SUPL
        if (1 == ni_req_ptr->suplEmergencyNotification_valid) {
            const qmiLocEmergencyNotificationStructT_v02 *supl_emergency_request =
                &ni_req_ptr->suplEmergencyNotification;

            notif.type = GNSS_NI_TYPE_EMERGENCY_SUPL;
        }
    } //ni_req_ptr->NiSuplInd_valid == 1
    else {
        LOC_LOGe("unknown request event");
        return;
    }

    // Set default_response & notify_flags
    convertNiNotifyVerifyType(&notif, ni_req_ptr->notificationType);

    qmiLocEventNiNotifyVerifyReqIndMsgT_v02 *ni_req_copy_ptr =
        (qmiLocEventNiNotifyVerifyReqIndMsgT_v02 *)malloc(sizeof(*ni_req_copy_ptr));

    LocInEmergency emergencyState = ni_req_ptr->isInEmergencySession_valid ?
            (ni_req_ptr->isInEmergencySession ? LOC_IN_EMERGENCY_SET : LOC_IN_EMERGENCY_NOT_SET) :
            LOC_IN_EMERGENCY_UNKNOWN;
    if (NULL != ni_req_copy_ptr) {
        memcpy(ni_req_copy_ptr, ni_req_ptr, sizeof(*ni_req_copy_ptr));
        requestNiNotify(notif, (const void*)ni_req_copy_ptr, emergencyState);
    } else {
        LOC_LOGe("Error copying NI request");
    }
}

/* If Confidence value is less than 68%, then scale the accuracy value to
   68%.confidence.*/
void LocApiV02 :: scaleAccuracyTo68PercentConfidence(
                                                const uint8_t confidenceValue,
                                                LocGpsLocation &gpsLocation,
                                                const bool isCircularUnc)
{
  if (confidenceValue < 68)
  {
    // Circular uncertainty is at 63%.confidence. Scale factor should be
    // 1.072(from 63% -> 68%)
    uint8_t realConfidence = (isCircularUnc) ? 63:confidenceValue;
    // get scaling value based on 2D% confidence scaling table
    for (uint8_t iter = 0; iter < CONF_SCALER_ARRAY_MAX; iter++)
    {
      if (realConfidence <= confScalers[iter].confidence)
      {
        LOC_LOGD("Confidence: %d, Scaler value:%f",
                realConfidence,confScalers[iter].scaler_to_68);
        gpsLocation.accuracy *= confScalers[iter].scaler_to_68;
        break;
      }
    }
  }
}

/* Report the Xtra Server Url from the modem to HAL*/
void LocApiV02 :: reportXtraServerUrl(
                const qmiLocEventInjectPredictedOrbitsReqIndMsgT_v02*
                server_request_ptr)
{

  if (server_request_ptr->serverList.serverList_len == 1)
  {
    reportXtraServer(server_request_ptr->serverList.serverList[0].serverUrl,
                     "",
                     "",
                     QMI_LOC_MAX_SERVER_ADDR_LENGTH_V02);
  }
  else if (server_request_ptr->serverList.serverList_len == 2)
  {
    reportXtraServer(server_request_ptr->serverList.serverList[0].serverUrl,
                     server_request_ptr->serverList.serverList[1].serverUrl,
                     "",
                     QMI_LOC_MAX_SERVER_ADDR_LENGTH_V02);
  }
  else
  {
    reportXtraServer(server_request_ptr->serverList.serverList[0].serverUrl,
                     server_request_ptr->serverList.serverList[1].serverUrl,
                     server_request_ptr->serverList.serverList[2].serverUrl,
                     QMI_LOC_MAX_SERVER_ADDR_LENGTH_V02);
  }

}

/* convert Ni Encoding type from QMI_LOC to loc eng format */
GnssNiEncodingType LocApiV02 ::convertNiEncoding(
  qmiLocNiDataCodingSchemeEnumT_v02 loc_encoding)
{
   GnssNiEncodingType enc = GNSS_NI_ENCODING_TYPE_NONE;

   switch (loc_encoding)
   {
     case eQMI_LOC_NI_SUPL_UTF8_V02:
       enc = GNSS_NI_ENCODING_TYPE_UTF8;
       break;
     case eQMI_LOC_NI_SUPL_UCS2_V02:
       enc = GNSS_NI_ENCODING_TYPE_UCS2;
       break;
     case eQMI_LOC_NI_SUPL_GSM_DEFAULT_V02:
       enc = GNSS_NI_ENCODING_TYPE_GSM_DEFAULT;
       break;
     case eQMI_LOC_NI_SS_LANGUAGE_UNSPEC_V02:
       enc = GNSS_NI_ENCODING_TYPE_GSM_DEFAULT; // SS_LANGUAGE_UNSPEC = GSM
       break;
     default:
       break;
   }

   return enc;
}

/*convert NI notify verify type from QMI LOC to loc eng format*/
bool LocApiV02 :: convertNiNotifyVerifyType (
  GnssNiNotification *notif,
  qmiLocNiNotifyVerifyEnumT_v02 notif_priv)
{
  switch (notif_priv)
   {
   case eQMI_LOC_NI_USER_NO_NOTIFY_NO_VERIFY_V02:
      notif->options = 0;
      break;

   case eQMI_LOC_NI_USER_NOTIFY_ONLY_V02:
      notif->options = GNSS_NI_OPTIONS_NOTIFICATION_BIT;
      break;

   case eQMI_LOC_NI_USER_NOTIFY_VERIFY_ALLOW_NO_RESP_V02:
      notif->options = GNSS_NI_OPTIONS_NOTIFICATION_BIT | GNSS_NI_OPTIONS_VERIFICATION_BIT;
      notif->timeoutResponse = GNSS_NI_RESPONSE_ACCEPT;
      break;

   case eQMI_LOC_NI_USER_NOTIFY_VERIFY_NOT_ALLOW_NO_RESP_V02:
      notif->options = GNSS_NI_OPTIONS_NOTIFICATION_BIT | GNSS_NI_OPTIONS_VERIFICATION_BIT;
      notif->timeoutResponse = GNSS_NI_RESPONSE_DENY;
      break;

   case eQMI_LOC_NI_USER_NOTIFY_VERIFY_PRIVACY_OVERRIDE_V02:
      notif->options = GNSS_NI_OPTIONS_PRIVACY_OVERRIDE_BIT;
      break;

   default:
      return false;
   }

   return true;
}

/* convert and report GNSS measurement data to loc eng */
void LocApiV02::reportGnssMeasurementData(
  const qmiLocEventGnssSvMeasInfoIndMsgT_v02& gnss_measurement_report_ptr)
{
    LOC_LOGv("entering");

    static uint32_t prevRefFCount = 0;
    static bool newMeasProcessed = false;
    uint8_t maxSubSeqNum = 0;
    uint8_t subSeqNum = 0;

    if (gnss_measurement_report_ptr.maxSubSeqNum_valid &&
        gnss_measurement_report_ptr.subSeqNum_valid) {
        maxSubSeqNum = gnss_measurement_report_ptr.maxSubSeqNum;
        subSeqNum = gnss_measurement_report_ptr.subSeqNum;
    } else {
        maxSubSeqNum = 0;
        subSeqNum = 0;
    }

    LOC_LOGd("[SvMeas] nHz (%d, %d), SeqNum: %d, MaxMsgNum: %d, "
             "SubSeqNum: %d, MaxSubMsgNum: %d, "
             "SvSystem: %d SignalType: %" PRIu64 " MeasValid: %d, #of SV: %d",
             gnss_measurement_report_ptr.nHzMeasurement_valid,
             gnss_measurement_report_ptr.nHzMeasurement,
             gnss_measurement_report_ptr.seqNum,
             gnss_measurement_report_ptr.maxMessageNum,
             subSeqNum,
             maxSubSeqNum,
             gnss_measurement_report_ptr.system,
             gnss_measurement_report_ptr.gnssSignalType,
             gnss_measurement_report_ptr.svMeasurement_valid,
             gnss_measurement_report_ptr.svMeasurement_len);

    if (!mGnssMeasurements) {
        mGnssMeasurements = (GnssMeasurements*)malloc(sizeof(GnssMeasurements));
        if (!mGnssMeasurements) {
            LOC_LOGe("Malloc failed to allocate heap memory for mGnssMeasurements");
            return;
        }
        resetSvMeasurementReport();
        newMeasProcessed = false;
    }

    if (gnss_measurement_report_ptr.seqNum > gnss_measurement_report_ptr.maxMessageNum ||
        subSeqNum > maxSubSeqNum) {
        LOC_LOGe("Invalid seqNum or subSeqNum, do not proceed");
        return;
    }

    // in case the measurement with seqNum of 1 is dropped, we will use ref count
    // to reset the measurement
    if ((gnss_measurement_report_ptr.seqNum == 1 && subSeqNum <= 1) ||
        (gnss_measurement_report_ptr.systemTimeExt_valid &&
         gnss_measurement_report_ptr.systemTimeExt.refFCount != prevRefFCount)) {
        // we have received some valid info since we last reported when
        // seq num matches with max seq num
        if (true == newMeasProcessed) {
            LOC_LOGe ("report due to seq number jump");
            reportSvMeasurementInternal();
            resetSvMeasurementReport();
            newMeasProcessed = false;
        }

        mHlosQtimer1 = getQTimerTickCount();
        mRefFCount = gnss_measurement_report_ptr.systemTimeExt.refFCount;
        LOC_LOGv("mHlosQtimer1=%" PRIi64 " mRefFCount=%d", mHlosQtimer1, mRefFCount);
        prevRefFCount = gnss_measurement_report_ptr.systemTimeExt.refFCount;
        if (gnss_measurement_report_ptr.nHzMeasurement_valid &&
            gnss_measurement_report_ptr.nHzMeasurement) {
            mGnssMeasurements->gnssSvMeasurementSet.isNhz = true;
        }
        mCounter++;
    }

    Gnss_LocSvSystemEnumType locSvSystemType =
        getLocApiSvSystemType(gnss_measurement_report_ptr.system);
    if (GNSS_LOC_SV_SYSTEM_UNKNOWN == locSvSystemType) {
        LOC_LOGi("Unknown sv system");
        return;
    }

    // set up indication that we have processed some new measurement
    newMeasProcessed = true;

    if (subSeqNum <= 1) {
        convertGnssMeasurementsHeader(locSvSystemType, gnss_measurement_report_ptr);
    }

    // check whether we have valid dgnss measurement
    bool validDgnssMeas = false;
    if ((gnss_measurement_report_ptr.dgnssSvMeasurement_valid) &&
        (gnss_measurement_report_ptr.dgnssSvMeasurement_len != 0)){
        uint32_t totalSvMeasLen = 0;
        if (gnss_measurement_report_ptr.svMeasurement_valid) {
            totalSvMeasLen = gnss_measurement_report_ptr.svMeasurement_len;
            if (gnss_measurement_report_ptr.extSvMeasurement_valid) {
                totalSvMeasLen += gnss_measurement_report_ptr.extSvMeasurement_len;
            }
        }
        if (totalSvMeasLen == gnss_measurement_report_ptr.dgnssSvMeasurement_len) {
            validDgnssMeas = true;
        }
    }

    // number of measurements
    if (gnss_measurement_report_ptr.svMeasurement_valid) {
        if (gnss_measurement_report_ptr.svMeasurement_len != 0 &&
            gnss_measurement_report_ptr.svMeasurement_len <= QMI_LOC_SV_MEAS_LIST_MAX_SIZE_V02) {
            // the array of measurements
            LOC_LOGv("Measurements received for GNSS system %d",
                     gnss_measurement_report_ptr.system);

            if (0 == mGnssMeasurements->gnssMeasNotification.count) {
                mAgcIsPresent = true;
            }
            for (uint32_t index = 0; index < gnss_measurement_report_ptr.svMeasurement_len &&
                    mGnssMeasurements->gnssMeasNotification.count < GNSS_MEASUREMENTS_MAX;
                    index++) {
                LOC_LOGv("index=%u count=%" PRIu32,
                    index, mGnssMeasurements->gnssMeasNotification.count);
                if ((gnss_measurement_report_ptr.svMeasurement[index].validMeasStatusMask &
                     QMI_LOC_MASK_MEAS_STATUS_GNSS_FRESH_MEAS_STAT_BIT_VALID_V02) &&
                    (gnss_measurement_report_ptr.svMeasurement[index].measurementStatus &
                     QMI_LOC_MASK_MEAS_STATUS_GNSS_FRESH_MEAS_VALID_V02)) {
                    mAgcIsPresent &= convertGnssMeasurements(
                        gnss_measurement_report_ptr,
                        index, false, validDgnssMeas);
                    mGnssMeasurements->gnssMeasNotification.count++;
                } else {
                    LOC_LOGv("Measurements are stale, do not report");
                }
            }
            LOC_LOGv("there are %d SV measurements now, total=%" PRIu32,
                     gnss_measurement_report_ptr.svMeasurement_len,
                     mGnssMeasurements->gnssMeasNotification.count);

            /* now check if more measurements are available (some constellations such
            as BDS have more measurements available in extSvMeasurement)
            */
            if (gnss_measurement_report_ptr.extSvMeasurement_valid &&
                gnss_measurement_report_ptr.extSvMeasurement_len != 0 &&
                gnss_measurement_report_ptr.extSvMeasurement_len <=
                    QMI_LOC_EXT_SV_MEAS_LIST_MAX_SIZE_V02) {
                // the array of measurements
                LOC_LOGv("More measurements received for GNSS system %d",
                         gnss_measurement_report_ptr.system);
                for (uint32_t index = 0; index < gnss_measurement_report_ptr.extSvMeasurement_len &&
                        mGnssMeasurements->gnssMeasNotification.count < GNSS_MEASUREMENTS_MAX;
                        index++) {
                    LOC_LOGv("index=%u count=%" PRIu32, index, mGnssMeasurements->gnssMeasNotification.count);
                    if ((gnss_measurement_report_ptr.extSvMeasurement[index].validMeasStatusMask &
                         QMI_LOC_MASK_MEAS_STATUS_GNSS_FRESH_MEAS_STAT_BIT_VALID_V02) &&
                        (gnss_measurement_report_ptr.extSvMeasurement[index].measurementStatus &
                         QMI_LOC_MASK_MEAS_STATUS_GNSS_FRESH_MEAS_VALID_V02)) {
                        mAgcIsPresent &= convertGnssMeasurements(
                            gnss_measurement_report_ptr,
                            index, true, validDgnssMeas);
                        mGnssMeasurements->gnssMeasNotification.count++;
                    }
                    else {
                        LOC_LOGv("Measurements are stale, do not report");
                    }
                }
                LOC_LOGv("there are %d SV measurements now, total=%" PRIu32,
                         gnss_measurement_report_ptr.extSvMeasurement_len,
                         mGnssMeasurements->gnssMeasNotification.count);
            }
        }
    } else {
        LOC_LOGv("there is no valid GNSS measurement for system %d, total=%" PRIu32,
                 gnss_measurement_report_ptr.system,
            mGnssMeasurements->gnssMeasNotification.count);
    }
    // the GPS clock time reading
    if (eQMI_LOC_SV_SYSTEM_GPS_V02 == gnss_measurement_report_ptr.system &&
        subSeqNum <= 1 &&
        false == mGPSreceived) {
        mGPSreceived = true;
        mMsInWeek = convertGnssClock(mGnssMeasurements->gnssMeasNotification.clock,
                                     gnss_measurement_report_ptr);
    }

    if (gnss_measurement_report_ptr.maxMessageNum == gnss_measurement_report_ptr.seqNum &&
        maxSubSeqNum == subSeqNum) {
        LOC_LOGD("Report the measurements to the upper layer");
        int64_t elapsedRealTime = -1;
        int64_t unc;
        if (gnss_measurement_report_ptr.refCountTicks_valid &&
            gnss_measurement_report_ptr.refCountTicksUnc_valid) {
            /* deal with Qtimer for ElapsedRealTimeNanos */
            mGnssMeasurements->gnssMeasNotification.clock.flags |=
                    GNSS_MEASUREMENTS_CLOCK_FLAGS_ELAPSED_REAL_TIME_BIT;

            elapsedRealTime = ElapsedRealtimeEstimator::getElapsedRealtimeQtimer(
                    gnss_measurement_report_ptr.refCountTicks);

            /* Uncertainty on HLOS time is 0, so the uncertainty of the difference
            is the uncertainty of the Qtimer in the modem
            Note that gnss_measurement_report_ptr.refCountTicksUncis in msec */
            unc = gnss_measurement_report_ptr.refCountTicksUnc * 1000000;
        } else {
            //If Qtimer isn't valid, estimate the elapsedRealTime
            GnssMeasurementsNotification& in = mGnssMeasurements->gnssMeasNotification;
            const uint32_t UTC_TO_GPS_SECONDS = 315964800;
            int64_t measTimeNanos = (int64_t)in.clock.timeNs - (int64_t)in.clock.fullBiasNs
                    - (int64_t)in.clock.biasNs - (int64_t)in.clock.leapSecond * 1000000000
                    + (int64_t)UTC_TO_GPS_SECONDS * 1000000000;
            bool isCurDataTimeTrustable =
                    in.clock.flags & GNSS_MEASUREMENTS_CLOCK_FLAGS_LEAP_SECOND_BIT &&
                    in.clock.flags & GNSS_MEASUREMENTS_CLOCK_FLAGS_FULL_BIAS_BIT &&
                    in.clock.flags & GNSS_MEASUREMENTS_CLOCK_FLAGS_BIAS_BIT &&
                    in.clock.flags & GNSS_MEASUREMENTS_CLOCK_FLAGS_BIAS_UNCERTAINTY_BIT;
            elapsedRealTime = mMeasElapsedRealTimeCal.
                    getElapsedRealtimeEstimateNanos(measTimeNanos, isCurDataTimeTrustable, 0);
            unc = mMeasElapsedRealTimeCal.getElapsedRealtimeUncNanos();
        }

        if (elapsedRealTime != -1) {
            mGnssMeasurements->gnssMeasNotification.clock.flags |=
                    GNSS_MEASUREMENTS_CLOCK_FLAGS_ELAPSED_REAL_TIME_BIT;
            mGnssMeasurements->gnssMeasNotification.clock.elapsedRealTime = elapsedRealTime;
            mGnssMeasurements->gnssMeasNotification.clock.elapsedRealTimeUnc = unc;
        }
        LOC_LOGd("Measurement elapsedRealtime: %" PRIi64 " uncertainty: %" PRIi64 "",
               mGnssMeasurements->gnssMeasNotification.clock.elapsedRealTime,
               mGnssMeasurements->gnssMeasNotification.clock.elapsedRealTimeUnc);

        reportSvMeasurementInternal();
        resetSvMeasurementReport();
        // set up flag to indicate that no new info in mGnssMeasurements
        newMeasProcessed = false;
    }
}

void LocApiV02::setGnssBiases() {
    GnssMeasurementsData* measData;
    uint64_t tempFlag, tempFlagUnc;

    for (uint32_t i = 0; i < mGnssMeasurements->gnssMeasNotification.count; i++) {
        measData = &mGnssMeasurements->gnssMeasNotification.measurements[i];
        switch (measData->gnssSignalType) {
        case GNSS_SIGNAL_GPS_L1CA:
            measData->fullInterSignalBiasNs = 0.0;
            measData->flags |= GNSS_MEASUREMENTS_DATA_FULL_ISB_BIT;
            measData->fullInterSignalBiasUncertaintyNs = 0.0;
            measData->flags |= GNSS_MEASUREMENTS_DATA_FULL_ISB_UNCERTAINTY_BIT;
            break;

        case GNSS_SIGNAL_GPS_L5:
        case GNSS_SIGNAL_QZSS_L5:
            if (mTimeBiases.flags & BIAS_GPSL1_GPSL5_VALID) {
                measData->fullInterSignalBiasNs = -mTimeBiases.gpsL1_gpsL5;
                measData->flags |= GNSS_MEASUREMENTS_DATA_FULL_ISB_BIT;
            }
            if (mTimeBiases.flags & BIAS_GPSL1_GPSL5_UNC_VALID) {
                measData->fullInterSignalBiasUncertaintyNs = mTimeBiases.gpsL1_gpsL5Unc;
                measData->flags |= GNSS_MEASUREMENTS_DATA_FULL_ISB_UNCERTAINTY_BIT;
            }
            break;

        case GNSS_SIGNAL_GLONASS_G1:
            if (mTimeBiases.flags & BIAS_GPSL1_GLOG1_VALID) {
                measData->fullInterSignalBiasNs = -mTimeBiases.gpsL1_gloG1;
                measData->flags |= GNSS_MEASUREMENTS_DATA_FULL_ISB_BIT;
            }
            if (mTimeBiases.flags & BIAS_GPSL1_GLOG1_UNC_VALID) {
                measData->fullInterSignalBiasUncertaintyNs = mTimeBiases.gpsL1_gloG1Unc;
                measData->flags |= GNSS_MEASUREMENTS_DATA_FULL_ISB_UNCERTAINTY_BIT;
            }
            break;

        case GNSS_SIGNAL_GALILEO_E1:
            if (mTimeBiases.flags & BIAS_GPSL1_GALE1_VALID) {
                measData->fullInterSignalBiasNs = -mTimeBiases.gpsL1_galE1;
                measData->flags |= GNSS_MEASUREMENTS_DATA_FULL_ISB_BIT;
            }
            if (mTimeBiases.flags & BIAS_GPSL1_GALE1_UNC_VALID) {
                measData->fullInterSignalBiasUncertaintyNs = mTimeBiases.gpsL1_galE1Unc;
                measData->flags |= GNSS_MEASUREMENTS_DATA_FULL_ISB_UNCERTAINTY_BIT;
            }
            break;

        case GNSS_SIGNAL_GALILEO_E5A:
            tempFlag = BIAS_GPSL1_VALID | BIAS_GALE1_VALID | BIAS_GALE1_GALE5A_VALID;
            tempFlagUnc =
                    BIAS_GPSL1_UNC_VALID | BIAS_GALE1_UNC_VALID | BIAS_GALE1_GALE5A_UNC_VALID;
            if (tempFlag == (mTimeBiases.flags & tempFlag)) {
                measData->fullInterSignalBiasNs =
                        -mTimeBiases.gpsL1 + mTimeBiases.galE1 - mTimeBiases.galE1_galE5a;
                measData->flags |= GNSS_MEASUREMENTS_DATA_FULL_ISB_BIT;
            }
            if (tempFlagUnc == (mTimeBiases.flags & tempFlagUnc)) {
                measData->fullInterSignalBiasUncertaintyNs =
                        mTimeBiases.gpsL1Unc + mTimeBiases.galE1Unc + mTimeBiases.galE1_galE5aUnc;
                measData->flags |= GNSS_MEASUREMENTS_DATA_FULL_ISB_UNCERTAINTY_BIT;
            }
            break;

        case GNSS_SIGNAL_BEIDOU_B1I:
            if (mTimeBiases.flags & BIAS_GPSL1_BDSB1_VALID) {
                measData->fullInterSignalBiasNs = -mTimeBiases.gpsL1_bdsB1;
                measData->flags |= GNSS_MEASUREMENTS_DATA_FULL_ISB_BIT;
            }
            if (mTimeBiases.flags & BIAS_GPSL1_BDSB1_UNC_VALID) {
                measData->fullInterSignalBiasUncertaintyNs = mTimeBiases.gpsL1_bdsB1Unc;
                measData->flags |= GNSS_MEASUREMENTS_DATA_FULL_ISB_UNCERTAINTY_BIT;
            }
            break;

        case GNSS_SIGNAL_QZSS_L1CA:
        case GNSS_SIGNAL_QZSS_L1S:
        case GNSS_SIGNAL_SBAS_L1:
            if (mTimeBiases.flags & BIAS_GPSL1_VALID) {
                measData->fullInterSignalBiasNs = 0;
                measData->flags |= GNSS_MEASUREMENTS_DATA_FULL_ISB_BIT;
            }
            if (mTimeBiases.flags & BIAS_GPSL1_UNC_VALID) {
                measData->fullInterSignalBiasUncertaintyNs = 0;
                measData->flags |= GNSS_MEASUREMENTS_DATA_FULL_ISB_UNCERTAINTY_BIT;
            }
            break;

        case GNSS_SIGNAL_BEIDOU_B1C:
            tempFlag = BIAS_GPSL1_VALID | BIAS_BDSB1_VALID | BIAS_BDSB1_BDSB1C_VALID;
            tempFlagUnc =
                    BIAS_GPSL1_UNC_VALID | BIAS_BDSB1_UNC_VALID | BIAS_BDSB1_BDSB1C_UNC_VALID;
            if (tempFlag == (mTimeBiases.flags & tempFlag)) {
                measData->fullInterSignalBiasNs =
                        -mTimeBiases.gpsL1 + mTimeBiases.bdsB1 - mTimeBiases.bdsB1_bdsB1c;
                measData->flags |= GNSS_MEASUREMENTS_DATA_FULL_ISB_BIT;
            }
            if (tempFlagUnc == (mTimeBiases.flags & tempFlagUnc)) {
                measData->fullInterSignalBiasUncertaintyNs =
                        mTimeBiases.gpsL1Unc + mTimeBiases.bdsB1Unc + mTimeBiases.bdsB1_bdsB1cUnc;
                measData->flags |= GNSS_MEASUREMENTS_DATA_FULL_ISB_UNCERTAINTY_BIT;
            }
            break;

        case GNSS_SIGNAL_NAVIC_L5:
            if (mTimeBiases.flags & BIAS_GPSL1_NAVIC_VALID) {
                measData->fullInterSignalBiasNs = -mTimeBiases.gpsL1_navic;
                measData->flags |= GNSS_MEASUREMENTS_DATA_FULL_ISB_BIT;
            }
            if (mTimeBiases.flags & BIAS_GPSL1_NAVIC_UNC_VALID) {
                measData->fullInterSignalBiasUncertaintyNs = mTimeBiases.gpsL1_navicUnc;
                measData->flags |= GNSS_MEASUREMENTS_DATA_FULL_ISB_UNCERTAINTY_BIT;
            }
            break;

        case GNSS_SIGNAL_BEIDOU_B2AQ:
            tempFlag = BIAS_GPSL1_VALID | BIAS_BDSB1_VALID | BIAS_BDSB1_BDSB2A_VALID;
            tempFlagUnc =
                    BIAS_GPSL1_UNC_VALID | BIAS_BDSB1_UNC_VALID | BIAS_BDSB1_BDSB2A_UNC_VALID;
            if (tempFlag == (mTimeBiases.flags & tempFlag)) {
                measData->fullInterSignalBiasNs =
                        -mTimeBiases.gpsL1 + mTimeBiases.bdsB1 - mTimeBiases.bdsB1_bdsB2a;
                measData->flags |= GNSS_MEASUREMENTS_DATA_FULL_ISB_BIT;
            }
            if (tempFlagUnc == (mTimeBiases.flags & tempFlagUnc)) {
                measData->fullInterSignalBiasUncertaintyNs =
                        mTimeBiases.gpsL1Unc + mTimeBiases.bdsB1Unc + mTimeBiases.bdsB1_bdsB2aUnc;
                measData->flags |= GNSS_MEASUREMENTS_DATA_FULL_ISB_UNCERTAINTY_BIT;
            }
            break;

        /* not supported */
        case GNSS_SIGNAL_GPS_L1C:
        case GNSS_SIGNAL_GPS_L2:
        case GNSS_SIGNAL_GLONASS_G2:
        case GNSS_SIGNAL_GALILEO_E5B:
        case GNSS_SIGNAL_QZSS_L2:
        case GNSS_SIGNAL_BEIDOU_B2I:
        case GNSS_SIGNAL_BEIDOU_B2AI:
        default:
            break;
        }
    }
}

void LocApiV02 ::reportSvMeasurementInternal() {

    if (mGnssMeasurements) {
        // calling the base
        if (mAgcIsPresent) {
            /* If we can get AGC from QMI LOC there is no need to get it from NMEA */
            mMsInWeek = -1;
        }
        // now remove all the elements in the vector which are not for current epoch
        if (mADRdata.size() > 0) {
            auto front = mADRdata.begin();
            for (auto back = mADRdata.end(); front != back;) {
                if (mCounter != front->counter) {
                    --back;
                    swap(*front, *back);
                } else {
                    front++;
                }
            }
            if (front != mADRdata.end()) {
                mADRdata.erase(front, mADRdata.end());
            }
        }

        mGnssMeasurements->gnssSvMeasurementSet.svMeasCount = mGnssMeasurements->gnssMeasNotification.count;
        GnssSvMeasurementHeader &svMeasSetHead =
            mGnssMeasurements->gnssSvMeasurementSet.svMeasSetHeader;

        // when we received the last sequence, timestamp the packet with AP time
        struct timespec apTimestamp;
        if (clock_gettime(CLOCK_BOOTTIME, &apTimestamp)== 0) {
            svMeasSetHead.apBootTimeStamp.apTimeStamp.tv_sec = apTimestamp.tv_sec;
            svMeasSetHead.apBootTimeStamp.apTimeStamp.tv_nsec = apTimestamp.tv_nsec;

            // use the time uncertainty configured in gps.conf
            svMeasSetHead.apBootTimeStamp.apTimeStampUncertaintyMs =
                    (float)ap_timestamp_uncertainty;
            svMeasSetHead.flags |= GNSS_SV_MEAS_HEADER_HAS_AP_TIMESTAMP;

            LOC_LOGD("%s:%d QMI_MeasPacketTime  %u (sec)  %u (nsec)",__func__,__LINE__,
                     svMeasSetHead.apBootTimeStamp.apTimeStamp.tv_sec,
                     svMeasSetHead.apBootTimeStamp.apTimeStamp.tv_nsec);
        }
        else {
            svMeasSetHead.apBootTimeStamp.apTimeStampUncertaintyMs = FLT_MAX;
            LOC_LOGE("%s:%d Error in clock_gettime() ",__func__, __LINE__);
        }

        setGnssBiases();
        LOC_LOGd("report %d sv in sv meas",
                 mGnssMeasurements->gnssMeasNotification.count);

        for (uint32_t i = 0; i < mGnssMeasurements->gnssMeasNotification.count; i++) {
           LOC_LOGv("measurements[%d].flags=0x%08x "
                    "measurements[%d].gnssSignalType=%d "
                    "measurements[%d].fullInterSignalBiasNs=%.2f "
                    "measurements[%d].fullInterSignalBiasUncertaintyNs=%.2f",
                    i, mGnssMeasurements->gnssMeasNotification.measurements[i].flags,
                    i, mGnssMeasurements->gnssMeasNotification.measurements[i].gnssSignalType,
                    i, mGnssMeasurements->gnssMeasNotification.
                            measurements[i].fullInterSignalBiasNs,
                    i, mGnssMeasurements->gnssMeasNotification.
                            measurements[i].fullInterSignalBiasUncertaintyNs);
        }
        LocApiBase::reportGnssMeasurements(*mGnssMeasurements, mMsInWeek);
    }
}

void LocApiV02::convertGnssMeasurementsHeader(const Gnss_LocSvSystemEnumType locSvSystemType,
    const qmiLocEventGnssSvMeasInfoIndMsgT_v02& gnss_measurement_info)
{
    GnssSvMeasurementHeader &svMeasSetHead =
        mGnssMeasurements->gnssSvMeasurementSet.svMeasSetHeader;

    // The refCountTicks for each constellation sent for one meas report is the same
    // always. It does not matter if it gets overwritten.
    if (gnss_measurement_info.refCountTicks_valid) {
        svMeasSetHead.flags |= GNSS_SV_MEAS_HEADER_HAS_REF_COUNT_TICKS;
        svMeasSetHead.refCountTicks = gnss_measurement_info.refCountTicks;
    }

    // clock frequency
    if (1 == gnss_measurement_info.rcvrClockFrequencyInfo_valid) {
        const qmiLocRcvrClockFrequencyInfoStructT_v02* rcvClockFreqInfo =
            &gnss_measurement_info.rcvrClockFrequencyInfo;

        svMeasSetHead.clockFreq.size = sizeof(Gnss_LocRcvrClockFrequencyInfoStructType);
        svMeasSetHead.clockFreq.clockDrift =
            gnss_measurement_info.rcvrClockFrequencyInfo.clockDrift;
        svMeasSetHead.clockFreq.clockDriftUnc =
            gnss_measurement_info.rcvrClockFrequencyInfo.clockDriftUnc;
        svMeasSetHead.clockFreq.sourceOfFreq = (Gnss_LocSourceofFreqEnumType)
            gnss_measurement_info.rcvrClockFrequencyInfo.sourceOfFreq;

        svMeasSetHead.flags |= GNSS_SV_MEAS_HEADER_HAS_CLOCK_FREQ;
        LOC_LOGv("FreqInfo:: Drift: %f, DriftUnc: %f, sourceoffreq: %d",
            svMeasSetHead.clockFreq.clockDrift, svMeasSetHead.clockFreq.clockDriftUnc,
            svMeasSetHead.clockFreq.sourceOfFreq);
    }

    if ((1 == gnss_measurement_info.leapSecondInfo_valid) &&
        (0 == gnss_measurement_info.leapSecondInfo.leapSecUnc)) {

        qmiLocLeapSecondInfoStructT_v02* leapSecond =
            (qmiLocLeapSecondInfoStructT_v02*)&gnss_measurement_info.leapSecondInfo;

        svMeasSetHead.leapSec.size = sizeof(Gnss_LeapSecondInfoStructType);
        svMeasSetHead.leapSec.leapSec = gnss_measurement_info.leapSecondInfo.leapSec;
        svMeasSetHead.leapSec.leapSecUnc = gnss_measurement_info.leapSecondInfo.leapSecUnc;

        svMeasSetHead.flags |= GNSS_SV_MEAS_HEADER_HAS_LEAP_SECOND;
        LOC_LOGV("leapSecondInfo:: leapSec: %d, leapSecUnc: %d",
            svMeasSetHead.leapSec.leapSec, svMeasSetHead.leapSec.leapSecUnc);
    }

    if (1 == gnss_measurement_info.gpsGloInterSystemBias_valid) {
        qmiLocInterSystemBiasStructT_v02* interSystemBias =
            (qmiLocInterSystemBiasStructT_v02*)&gnss_measurement_info.gpsGloInterSystemBias;

        getInterSystemTimeBias("gpsGloInterSystemBias",
            svMeasSetHead.gpsGloInterSystemBias, interSystemBias);
        svMeasSetHead.flags |= GNSS_SV_MEAS_HEADER_HAS_GPS_GLO_INTER_SYSTEM_BIAS;

        if (gnss_measurement_info.gpsGloInterSystemBias.validMask &
            QMI_LOC_SYS_TIME_BIAS_VALID_V02) {
            mTimeBiases.gpsL1_gloG1 =
                    -gnss_measurement_info.gpsGloInterSystemBias.timeBias * 1000000;
            mTimeBiases.flags |= BIAS_GPSL1_GLOG1_VALID;
        }
        if (gnss_measurement_info.gpsGloInterSystemBias.validMask &
            QMI_LOC_SYS_TIME_BIAS_UNC_VALID_V02) {
            mTimeBiases.gpsL1_gloG1Unc =
                    gnss_measurement_info.gpsGloInterSystemBias.timeBiasUnc * 1000000;
            mTimeBiases.flags |= BIAS_GPSL1_GLOG1_UNC_VALID;
        }
    }

    if (1 == gnss_measurement_info.gpsBdsInterSystemBias_valid) {
        qmiLocInterSystemBiasStructT_v02* interSystemBias =
                (qmiLocInterSystemBiasStructT_v02*)&gnss_measurement_info.gpsBdsInterSystemBias;

        getInterSystemTimeBias("gpsBdsInterSystemBias",
                                svMeasSetHead.gpsBdsInterSystemBias, interSystemBias);
        svMeasSetHead.flags |= GNSS_SV_MEAS_HEADER_HAS_GPS_BDS_INTER_SYSTEM_BIAS;

        if (gnss_measurement_info.gpsBdsInterSystemBias.validMask &
            QMI_LOC_SYS_TIME_BIAS_VALID_V02) {
            mTimeBiases.gpsL1_bdsB1 =
                    -gnss_measurement_info.gpsBdsInterSystemBias.timeBias * 1000000;
            mTimeBiases.flags |= BIAS_GPSL1_BDSB1_VALID;
        }
        if (gnss_measurement_info.gpsBdsInterSystemBias.validMask &
            QMI_LOC_SYS_TIME_BIAS_UNC_VALID_V02) {
            mTimeBiases.gpsL1_bdsB1Unc =
                    gnss_measurement_info.gpsBdsInterSystemBias.timeBiasUnc * 1000000;
            mTimeBiases.flags |= BIAS_GPSL1_BDSB1_UNC_VALID;
        }
    }

    if (1 == gnss_measurement_info.gpsGalInterSystemBias_valid) {
        qmiLocInterSystemBiasStructT_v02* interSystemBias =
                (qmiLocInterSystemBiasStructT_v02*)&gnss_measurement_info.gpsGalInterSystemBias;

        getInterSystemTimeBias("gpsGalInterSystemBias",
                               svMeasSetHead.gpsGalInterSystemBias, interSystemBias);
        svMeasSetHead.flags |= GNSS_SV_MEAS_HEADER_HAS_GPS_GAL_INTER_SYSTEM_BIAS;

        if (gnss_measurement_info.gpsGalInterSystemBias.validMask &
            QMI_LOC_SYS_TIME_BIAS_VALID_V02) {
            mTimeBiases.gpsL1_galE1 =
                    -gnss_measurement_info.gpsGalInterSystemBias.timeBias * 1000000;
            mTimeBiases.flags |= BIAS_GPSL1_GALE1_VALID;
        }
        if (gnss_measurement_info.gpsGalInterSystemBias.validMask &
            QMI_LOC_SYS_TIME_BIAS_UNC_VALID_V02) {
            mTimeBiases.gpsL1_galE1Unc =
                    gnss_measurement_info.gpsGalInterSystemBias.timeBiasUnc * 1000000;
            mTimeBiases.flags |= BIAS_GPSL1_GALE1_UNC_VALID;
        }
    }

    if (1 == gnss_measurement_info.bdsGloInterSystemBias_valid) {
        qmiLocInterSystemBiasStructT_v02* interSystemBias =
                (qmiLocInterSystemBiasStructT_v02*)&gnss_measurement_info.bdsGloInterSystemBias;

        getInterSystemTimeBias("bdsGloInterSystemBias",
                               svMeasSetHead.bdsGloInterSystemBias, interSystemBias);
        svMeasSetHead.flags |= GNSS_SV_MEAS_HEADER_HAS_BDS_GLO_INTER_SYSTEM_BIAS;
    }

    if (1 == gnss_measurement_info.galGloInterSystemBias_valid) {
        qmiLocInterSystemBiasStructT_v02* interSystemBias =
                (qmiLocInterSystemBiasStructT_v02*)&gnss_measurement_info.galGloInterSystemBias;

        getInterSystemTimeBias("galGloInterSystemBias",
                                svMeasSetHead.galGloInterSystemBias, interSystemBias);
        svMeasSetHead.flags |= GNSS_SV_MEAS_HEADER_HAS_GAL_GLO_INTER_SYSTEM_BIAS;
    }

    if (1 == gnss_measurement_info.galBdsInterSystemBias_valid) {
        qmiLocInterSystemBiasStructT_v02* interSystemBias =
                (qmiLocInterSystemBiasStructT_v02*)&gnss_measurement_info.galBdsInterSystemBias;

        getInterSystemTimeBias("galBdsInterSystemBias",
                                svMeasSetHead.galBdsInterSystemBias, interSystemBias);
        svMeasSetHead.flags |= GNSS_SV_MEAS_HEADER_HAS_GAL_BDS_INTER_SYSTEM_BIAS;
    }

    if (1 == gnss_measurement_info.gpsNavicInterSystemBias_valid) {
        qmiLocInterSystemBiasStructT_v02* interSystemBias =
                (qmiLocInterSystemBiasStructT_v02*)&gnss_measurement_info.gpsNavicInterSystemBias;

        getInterSystemTimeBias("gpsNavicInterSystemBias",
                                svMeasSetHead.gpsNavicInterSystemBias, interSystemBias);
        svMeasSetHead.flags |= GNSS_SV_MEAS_HEADER_HAS_GPS_NAVIC_INTER_SYSTEM_BIAS;

        if (gnss_measurement_info.gpsNavicInterSystemBias.validMask &
            QMI_LOC_SYS_TIME_BIAS_VALID_V02) {
            mTimeBiases.gpsL1_navic =
                    -gnss_measurement_info.gpsNavicInterSystemBias.timeBias * 1000000;
            mTimeBiases.flags |= BIAS_GPSL1_NAVIC_VALID;
        }
        if (gnss_measurement_info.gpsNavicInterSystemBias.validMask &
            QMI_LOC_SYS_TIME_BIAS_UNC_VALID_V02) {
            mTimeBiases.gpsL1_navicUnc =
                    gnss_measurement_info.gpsNavicInterSystemBias.timeBiasUnc * 1000000;
            mTimeBiases.flags |= BIAS_GPSL1_NAVIC_UNC_VALID;
        }
    }

    if (1 == gnss_measurement_info.galNavicInterSystemBias_valid) {
        qmiLocInterSystemBiasStructT_v02* interSystemBias =
                (qmiLocInterSystemBiasStructT_v02*)&gnss_measurement_info.galNavicInterSystemBias;

        getInterSystemTimeBias("galNavicInterSystemBias",
                                svMeasSetHead.galNavicInterSystemBias, interSystemBias);
        svMeasSetHead.flags |= GNSS_SV_MEAS_HEADER_HAS_GAL_NAVIC_INTER_SYSTEM_BIAS;
    }

    if (1 == gnss_measurement_info.gloNavicInterSystemBias_valid) {
        qmiLocInterSystemBiasStructT_v02* interSystemBias =
                (qmiLocInterSystemBiasStructT_v02*)&gnss_measurement_info.gloNavicInterSystemBias;

        getInterSystemTimeBias("gloNavicInterSystemBias",
                                svMeasSetHead.gloNavicInterSystemBias, interSystemBias);
        svMeasSetHead.flags |= GNSS_SV_MEAS_HEADER_HAS_GLO_NAVIC_INTER_SYSTEM_BIAS;
    }

    if (1 == gnss_measurement_info.bdsNavicInterSystemBias_valid) {
        qmiLocInterSystemBiasStructT_v02* interSystemBias =
                (qmiLocInterSystemBiasStructT_v02*)&gnss_measurement_info.bdsNavicInterSystemBias;

        getInterSystemTimeBias("bdsNavicInterSystemBias",
                                svMeasSetHead.bdsNavicInterSystemBias, interSystemBias);
        svMeasSetHead.flags |= GNSS_SV_MEAS_HEADER_HAS_BDS_NAVIC_INTER_SYSTEM_BIAS;
    }

    if (1 == gnss_measurement_info.GpsL1L5TimeBias_valid) {
        qmiLocInterSystemBiasStructT_v02* interSystemBias =
                (qmiLocInterSystemBiasStructT_v02*)&gnss_measurement_info.GpsL1L5TimeBias;

        getInterSystemTimeBias("gpsL1L5TimeBias",
                                svMeasSetHead.gpsL1L5TimeBias, interSystemBias);
        svMeasSetHead.flags |= GNSS_SV_MEAS_HEADER_HAS_GPSL1L5_TIME_BIAS;

        if (gnss_measurement_info.GpsL1L5TimeBias.validMask & QMI_LOC_SYS_TIME_BIAS_VALID_V02) {
            mTimeBiases.gpsL1_gpsL5 = -gnss_measurement_info.GpsL1L5TimeBias.timeBias * 1000000;
            mTimeBiases.flags |= BIAS_GPSL1_GPSL5_VALID;
        }
        if (gnss_measurement_info.GpsL1L5TimeBias.validMask &
            QMI_LOC_SYS_TIME_BIAS_UNC_VALID_V02) {
            mTimeBiases.gpsL1_gpsL5Unc =
                    gnss_measurement_info.GpsL1L5TimeBias.timeBiasUnc * 1000000;
            mTimeBiases.flags |= BIAS_GPSL1_GPSL5_UNC_VALID;
        }
    }

    if (1 == gnss_measurement_info.GalE1E5aTimeBias_valid) {
        qmiLocInterSystemBiasStructT_v02* interSystemBias =
                (qmiLocInterSystemBiasStructT_v02*)&gnss_measurement_info.GalE1E5aTimeBias;

        getInterSystemTimeBias("galE1E5aTimeBias",
                                svMeasSetHead.galE1E5aTimeBias, interSystemBias);
        svMeasSetHead.flags |= GNSS_SV_MEAS_HEADER_HAS_GALE1E5A_TIME_BIAS;

        if (gnss_measurement_info.GalE1E5aTimeBias.validMask & QMI_LOC_SYS_TIME_BIAS_VALID_V02) {
            mTimeBiases.galE1_galE5a = -gnss_measurement_info.GalE1E5aTimeBias.timeBias * 1000000;
            mTimeBiases.flags |= BIAS_GALE1_GALE5A_VALID;
        }
        if (gnss_measurement_info.GalE1E5aTimeBias.validMask &
            QMI_LOC_SYS_TIME_BIAS_UNC_VALID_V02) {
            mTimeBiases.galE1_galE5aUnc =
                    gnss_measurement_info.GalE1E5aTimeBias.timeBiasUnc * 1000000;
            mTimeBiases.flags |= BIAS_GALE1_GALE5A_UNC_VALID;
        }
    }

    if (1 == gnss_measurement_info.BdsB1iB2aTimeBias_valid) {
        qmiLocInterSystemBiasStructT_v02* interSystemBias =
                (qmiLocInterSystemBiasStructT_v02*)&gnss_measurement_info.BdsB1iB2aTimeBias;

        getInterSystemTimeBias("bdsB1iB2aTimeBias",
                               svMeasSetHead.bdsB1iB2aTimeBias, interSystemBias);
        svMeasSetHead.flags |= GNSS_SV_MEAS_HEADER_HAS_BDSB1IB2A_TIME_BIAS;

        if (gnss_measurement_info.BdsB1iB2aTimeBias.validMask & QMI_LOC_SYS_TIME_BIAS_VALID_V02) {
            mTimeBiases.bdsB1_bdsB2a = -gnss_measurement_info.BdsB1iB2aTimeBias.timeBias * 1000000;
            mTimeBiases.flags |= BIAS_BDSB1_BDSB2A_VALID;
        }
        if (gnss_measurement_info.BdsB1iB2aTimeBias.validMask &
            QMI_LOC_SYS_TIME_BIAS_UNC_VALID_V02) {
            mTimeBiases.bdsB1_bdsB2aUnc =
                    gnss_measurement_info.BdsB1iB2aTimeBias.timeBiasUnc * 1000000;
            mTimeBiases.flags |= BIAS_BDSB1_BDSB2A_UNC_VALID;
        }
    }

    if (1 == gnss_measurement_info.BdsB1iB1cTimeBias_valid) {
        if (gnss_measurement_info.BdsB1iB1cTimeBias.validMask & QMI_LOC_SYS_TIME_BIAS_VALID_V02) {
            mTimeBiases.bdsB1_bdsB1c = -gnss_measurement_info.BdsB1iB1cTimeBias.timeBias * 1000000;
            mTimeBiases.flags |= BIAS_BDSB1_BDSB1C_VALID;
        }
        if (gnss_measurement_info.BdsB1iB1cTimeBias.validMask &
            QMI_LOC_SYS_TIME_BIAS_UNC_VALID_V02) {
            mTimeBiases.bdsB1_bdsB1cUnc =
                    gnss_measurement_info.BdsB1iB1cTimeBias.timeBiasUnc * 1000000;
            mTimeBiases.flags |= BIAS_BDSB1_BDSB1C_UNC_VALID;
        }
    }

    if (1 == gnss_measurement_info.GpsL1L2cTimeBias_valid) {
        qmiLocInterSystemBiasStructT_v02* interSystemBias =
                (qmiLocInterSystemBiasStructT_v02*)&gnss_measurement_info.GpsL1L2cTimeBias;

        getInterSystemTimeBias("gpsL1L2cTimeBias",
                               svMeasSetHead.gpsL1L2cTimeBias, interSystemBias);
        svMeasSetHead.flags |= GNSS_SV_MEAS_HEADER_HAS_GPSL1L2C_TIME_BIAS;
    }

    if (1 == gnss_measurement_info.GloG1G2TimeBias_valid) {
        qmiLocInterSystemBiasStructT_v02* interSystemBias =
                (qmiLocInterSystemBiasStructT_v02*)&gnss_measurement_info.GloG1G2TimeBias;

        getInterSystemTimeBias("gloG1G2TimeBias",
                               svMeasSetHead.gloG1G2TimeBias, interSystemBias);
        svMeasSetHead.flags |= GNSS_SV_MEAS_HEADER_HAS_GLOG1G2_TIME_BIAS;
    }

    if (1 == gnss_measurement_info.BdsB1iB1cTimeBias_valid) {
        qmiLocInterSystemBiasStructT_v02* interSystemBias =
                (qmiLocInterSystemBiasStructT_v02*)&gnss_measurement_info.BdsB1iB1cTimeBias;

        getInterSystemTimeBias("bdsB1iB1cTimeBias",
                               svMeasSetHead.bdsB1iB1cTimeBias, interSystemBias);
        svMeasSetHead.flags |= GNSS_SV_MEAS_HEADER_HAS_BDSB1IB1C_TIME_BIAS;
    }

    if (1 == gnss_measurement_info.GalE1E5bTimeBias_valid) {
        qmiLocInterSystemBiasStructT_v02* interSystemBias =
                (qmiLocInterSystemBiasStructT_v02*)&gnss_measurement_info.GalE1E5bTimeBias;

        getInterSystemTimeBias("galE1E5bTimeBias",
                               svMeasSetHead.galE1E5bTimeBias, interSystemBias);
        svMeasSetHead.flags |= GNSS_SV_MEAS_HEADER_HAS_GALE1E5B_TIME_BIAS;
    }

    if (1 == gnss_measurement_info.gloTime_valid) {
        GnssGloTimeStructType & gloSystemTime = svMeasSetHead.gloSystemTime;

        gloSystemTime.gloFourYear = gnss_measurement_info.gloTime.gloFourYear;
        gloSystemTime.gloDays = gnss_measurement_info.gloTime.gloDays;
        gloSystemTime.gloMsec = gnss_measurement_info.gloTime.gloMsec;
        gloSystemTime.gloClkTimeBias = gnss_measurement_info.gloTime.gloClkTimeBias;
        gloSystemTime.gloClkTimeUncMs = gnss_measurement_info.gloTime.gloClkTimeUncMs;
        gloSystemTime.validityMask |= (GNSS_GLO_FOUR_YEAR_VALID |
            GNSS_CLO_DAYS_VALID |
            GNSS_GLO_MSEC_VALID |
            GNSS_GLO_CLK_TIME_BIAS_VALID |
            GNSS_GLO_CLK_TIME_BIAS_UNC_VALID);

        if (gnss_measurement_info.systemTimeExt_valid) {
            gloSystemTime.refFCount = gnss_measurement_info.systemTimeExt.refFCount;
            gloSystemTime.validityMask |= GNSS_GLO_REF_FCOUNT_VALID;
        }

        if (gnss_measurement_info.numClockResets_valid) {
            gloSystemTime.numClockResets = gnss_measurement_info.numClockResets;
            gloSystemTime.validityMask |= GNSS_GLO_NUM_CLOCK_RESETS_VALID;
        }

        svMeasSetHead.flags |= GNSS_SV_MEAS_HEADER_HAS_GLO_SYSTEM_TIME;
    }

    if ((1 == gnss_measurement_info.systemTime_valid) ||
        (1 == gnss_measurement_info.systemTimeExt_valid)) {

        GnssSystemTimeStructType* systemTimePtr = nullptr;
        Gnss_LocGnssTimeExtStructType* systemTimeExtPtr = nullptr;
        GpsSvMeasHeaderFlags systemTimeFlags = 0x0;
        GpsSvMeasHeaderFlags systemTimeExtFlags = 0x0;

        switch (locSvSystemType) {
        case GNSS_LOC_SV_SYSTEM_GPS:
            systemTimePtr = &svMeasSetHead.gpsSystemTime;
            systemTimeExtPtr = &svMeasSetHead.gpsSystemTimeExt;
            systemTimeFlags = GNSS_SV_MEAS_HEADER_HAS_GPS_SYSTEM_TIME;
            systemTimeExtFlags = GNSS_SV_MEAS_HEADER_HAS_GPS_SYSTEM_TIME_EXT;
            if (gnss_measurement_info.systemTime_valid) {
                mTimeBiases.gpsL1 = gnss_measurement_info.systemTime.systemClkTimeBias * 1000000;
                mTimeBiases.gpsL1Unc =
                        gnss_measurement_info.systemTime.systemClkTimeUncMs * 1000000;
                mTimeBiases.flags |= BIAS_GPSL1_VALID;
                mTimeBiases.flags |= BIAS_GPSL1_UNC_VALID;
            }
            break;
        case GNSS_LOC_SV_SYSTEM_GALILEO:
            systemTimePtr = &svMeasSetHead.galSystemTime;
            systemTimeExtPtr = &svMeasSetHead.galSystemTimeExt;
            systemTimeFlags = GNSS_SV_MEAS_HEADER_HAS_GAL_SYSTEM_TIME;
            systemTimeExtFlags = GNSS_SV_MEAS_HEADER_HAS_GAL_SYSTEM_TIME_EXT;
            if (gnss_measurement_info.systemTime_valid) {
                mTimeBiases.galE1 = gnss_measurement_info.systemTime.systemClkTimeBias * 1000000;
                mTimeBiases.galE1Unc =
                    gnss_measurement_info.systemTime.systemClkTimeUncMs * 1000000;
                mTimeBiases.flags |= BIAS_GALE1_VALID;
                mTimeBiases.flags |= BIAS_GALE1_UNC_VALID;
            }
            break;
        case GNSS_LOC_SV_SYSTEM_BDS:
            systemTimePtr = &svMeasSetHead.bdsSystemTime;
            systemTimeExtPtr = &svMeasSetHead.bdsSystemTimeExt;
            systemTimeFlags = GNSS_SV_MEAS_HEADER_HAS_BDS_SYSTEM_TIME;
            systemTimeExtFlags = GNSS_SV_MEAS_HEADER_HAS_BDS_SYSTEM_TIME_EXT;
            if (gnss_measurement_info.systemTime_valid) {
                mTimeBiases.bdsB1 = gnss_measurement_info.systemTime.systemClkTimeBias * 1000000;
                mTimeBiases.bdsB1Unc =
                    gnss_measurement_info.systemTime.systemClkTimeUncMs * 1000000;
                mTimeBiases.flags |= BIAS_BDSB1_VALID;
                mTimeBiases.flags |= BIAS_BDSB1_UNC_VALID;
            }
            break;
        case GNSS_LOC_SV_SYSTEM_QZSS:
            systemTimePtr = &svMeasSetHead.qzssSystemTime;
            systemTimeExtPtr = &svMeasSetHead.qzssSystemTimeExt;
            systemTimeFlags = GNSS_SV_MEAS_HEADER_HAS_QZSS_SYSTEM_TIME;
            systemTimeExtFlags = GNSS_SV_MEAS_HEADER_HAS_QZSS_SYSTEM_TIME_EXT;
            break;
        case GNSS_LOC_SV_SYSTEM_GLONASS:
            systemTimeExtPtr = &svMeasSetHead.gloSystemTimeExt;
            systemTimeExtFlags = GNSS_SV_MEAS_HEADER_HAS_GLO_SYSTEM_TIME_EXT;
            break;
        case GNSS_LOC_SV_SYSTEM_NAVIC:
            systemTimePtr = &svMeasSetHead.navicSystemTime;
            systemTimeExtPtr = &svMeasSetHead.navicSystemTimeExt;
            systemTimeFlags = GNSS_SV_MEAS_HEADER_HAS_NAVIC_SYSTEM_TIME;
            systemTimeExtFlags = GNSS_SV_MEAS_HEADER_HAS_NAVIC_SYSTEM_TIME_EXT;
            break;
        default:
            break;
        }

        if (systemTimePtr) {
            if (gnss_measurement_info.systemTime_valid) {
                systemTimePtr->systemWeek =
                    gnss_measurement_info.systemTime.systemWeek;
                systemTimePtr->systemMsec =
                    gnss_measurement_info.systemTime.systemMsec;
                systemTimePtr->systemClkTimeBias =
                    gnss_measurement_info.systemTime.systemClkTimeBias;
                systemTimePtr->systemClkTimeUncMs =
                    gnss_measurement_info.systemTime.systemClkTimeUncMs;
                systemTimePtr->validityMask |= (GNSS_SYSTEM_TIME_WEEK_VALID |
                    GNSS_SYSTEM_TIME_WEEK_MS_VALID |
                    GNSS_SYSTEM_CLK_TIME_BIAS_VALID |
                    GNSS_SYSTEM_CLK_TIME_BIAS_UNC_VALID);
                svMeasSetHead.flags |= systemTimeFlags;
            }

            if (gnss_measurement_info.systemTimeExt_valid) {
                systemTimePtr->refFCount = gnss_measurement_info.systemTimeExt.refFCount;
                systemTimePtr->validityMask |= GNSS_SYSTEM_REF_FCOUNT_VALID;
                svMeasSetHead.flags |= systemTimeFlags;
            }

            if (gnss_measurement_info.numClockResets_valid) {
                systemTimePtr->numClockResets = gnss_measurement_info.numClockResets;
                systemTimePtr->validityMask |= GNSS_SYSTEM_NUM_CLOCK_RESETS_VALID;
                svMeasSetHead.flags |= systemTimeFlags;
            }
        }

        if (systemTimeExtPtr) {
            if (gnss_measurement_info.systemTimeExt_valid) {
                systemTimeExtPtr->size = sizeof(Gnss_LocGnssTimeExtStructType);

                systemTimeExtPtr->systemRtc_valid =
                    gnss_measurement_info.systemTimeExt.systemRtc_valid;
                systemTimeExtPtr->systemRtcMs =
                    gnss_measurement_info.systemTimeExt.systemRtcMs;

                svMeasSetHead.flags |= systemTimeExtFlags;
            }
        }
    }

    if (gnss_measurement_info.dgnssCorrectionSourceT_valid) {
        svMeasSetHead.dgnssCorrectionSourceType =
                (LocDgnssCorrectionSourceType)gnss_measurement_info.dgnssCorrectionSourceT;
        svMeasSetHead.flags |= GNSS_SV_MEAS_HEADER_HAS_DGNSS_CORRECTION_SOURCE_TYPE;
    }
    if (gnss_measurement_info.dgnssCorrectionSourceID_valid) {
        svMeasSetHead.dgnssCorrectionSourceID =
                gnss_measurement_info.dgnssCorrectionSourceID;
        svMeasSetHead.flags |= GNSS_SV_MEAS_HEADER_HAS_DGNSS_CORRECTION_SOURCE_ID;

    }
    if (gnss_measurement_info.dgnssRefStationId_valid) {
        svMeasSetHead.dgnssRefStationId =
                gnss_measurement_info.dgnssRefStationId;
        svMeasSetHead.flags |= GNSS_SV_MEAS_HEADER_HAS_DGNSS_REF_STATION_ID;
    }
}

/* convert and report ODCPI request */
void LocApiV02::requestOdcpi(const qmiLocEventWifiReqIndMsgT_v02& qmiReq)
{
    LOC_LOGv("ODCPI Request: requestType %d", qmiReq.requestType);

    OdcpiRequestInfo req = {};
    req.size = sizeof(OdcpiRequestInfo);

    if (eQMI_LOC_WIFI_START_PERIODIC_HI_FREQ_FIXES_V02 == qmiReq.requestType ||
            eQMI_LOC_WIFI_START_PERIODIC_KEEP_WARM_V02 == qmiReq.requestType) {
        req.type = ODCPI_REQUEST_TYPE_START;
    } else if (eQMI_LOC_WIFI_STOP_PERIODIC_FIXES_V02 == qmiReq.requestType){
        req.type = ODCPI_REQUEST_TYPE_STOP;
    } else {
        LOC_LOGe("Invalid request type");
        return;
    }

    if (qmiReq.e911Mode_valid) {
        req.isEmergencyMode = qmiReq.e911Mode == 1 ? true : false;
    }

    if (qmiReq.tbfInMs_valid) {
        req.tbfMillis = qmiReq.tbfInMs;
    }

    LocApiBase::requestOdcpi(req);
}

void LocApiV02::wifiStatusInformSync()
{
    qmiLocNotifyWifiStatusReqMsgT_v02 wifiStatusReq;
    memset(&wifiStatusReq, 0, sizeof(wifiStatusReq));
    wifiStatusReq.wifiStatus = eQMI_LOC_WIFI_STATUS_AVAILABLE_V02;

    LOC_LOGv("Informing wifi status available.");
    LOC_SEND_SYNC_REQ(NotifyWifiStatus, NOTIFY_WIFI_STATUS, wifiStatusReq);
}

#define FIRST_BDS_D2_SV_PRN 1
#define LAST_BDS_D2_SV_PRN  5
#define IS_BDS_GEO_SV(svId, gnssType) ( ((gnssType == GNSS_SV_TYPE_BEIDOU) && \
                                        (svId <= LAST_BDS_D2_SV_PRN) && \
                                        (svId >= FIRST_BDS_D2_SV_PRN)) ? true : false )

/*convert GnssMeasurement type from QMI LOC to loc eng format*/
bool LocApiV02 :: convertGnssMeasurements(
    const qmiLocEventGnssSvMeasInfoIndMsgT_v02& gnss_measurement_report_ptr,
    int index, bool isExt, bool validDgnssSvMeas)
{
    bool bAgcIsPresent = false;
    qmiLocSVMeasurementStructT_v02 gnss_measurement_info;
    uint32_t count = mGnssMeasurements->gnssMeasNotification.count;

    LOC_LOGv("entering extMeas %d, qmi sv index %d, current sv count %d", isExt, index, count);
    if (isExt) {
        gnss_measurement_info = gnss_measurement_report_ptr.extSvMeasurement[index];
    } else {
        gnss_measurement_info = gnss_measurement_report_ptr.svMeasurement[index];
    }

    GnssMeasurementsData& measurementData =
        mGnssMeasurements->gnssMeasNotification.measurements[count];

    Gnss_SVMeasurementStructType& svMeas =
        mGnssMeasurements->gnssSvMeasurementSet.svMeas[count];

    svMeas.size = sizeof(Gnss_SVMeasurementStructType);
    svMeas.gnssSystem = getLocApiSvSystemType(gnss_measurement_report_ptr.system);
    svMeas.gnssSvId = gnss_measurement_info.gnssSvId;
    svMeas.gloFrequency = gnss_measurement_info.gloFrequency;
    if (gnss_measurement_report_ptr.gnssSignalType_valid) {
        svMeas.gnssSignalTypeMask =
                convertQmiGnssSignalType(gnss_measurement_report_ptr.gnssSignalType);
    } else {
        svMeas.gnssSignalTypeMask =
                getDefaultGnssSignalTypeMask(gnss_measurement_report_ptr.system);
    }
    if (gnss_measurement_info.validMask & QMI_LOC_SV_LOSSOFLOCK_VALID_V02) {
        svMeas.lossOfLock = (bool)gnss_measurement_info.lossOfLock;
    }

    svMeas.svStatus = (Gnss_LocSvSearchStatusEnumT)gnss_measurement_info.svStatus;

    if (gnss_measurement_info.validMask & QMI_LOC_SV_HEALTH_VALID_V02) {
        svMeas.healthStatus_valid = 1;
        svMeas.healthStatus = (uint8_t)gnss_measurement_info.healthStatus;
    }

    svMeas.svInfoMask = (Gnss_LocSvInfoMaskT)gnss_measurement_info.svInfoMask;
    svMeas.CNo = gnss_measurement_info.CNo;
    svMeas.gloRfLoss = gnss_measurement_info.gloRfLoss;
    svMeas.measLatency = gnss_measurement_info.measLatency;

    // SVTimeSpeed
    svMeas.svTimeSpeed.size = sizeof(Gnss_LocSVTimeSpeedStructType);
    svMeas.svTimeSpeed.svMs = gnss_measurement_info.svTimeSpeed.svTimeMs;
    svMeas.svTimeSpeed.svSubMs = gnss_measurement_info.svTimeSpeed.svTimeSubMs;
    svMeas.svTimeSpeed.svTimeUncMs = gnss_measurement_info.svTimeSpeed.svTimeUncMs;
    svMeas.svTimeSpeed.dopplerShift = gnss_measurement_info.svTimeSpeed.dopplerShift;
    svMeas.svTimeSpeed.dopplerShiftUnc = gnss_measurement_info.svTimeSpeed.dopplerShiftUnc;

    svMeas.validMeasStatusMask = gnss_measurement_info.validMeasStatusMask;
    qmiLocSvMeasStatusMaskT_v02 measStatus = gnss_measurement_info.measurementStatus;
    svMeas.measurementStatus = GNSS_LOC_MEAS_STATUS_NULL;
    // Convert qmiSvMeas.measurementStatus to svMeas.measurementStatus
    if (QMI_LOC_MASK_MEAS_STATUS_SM_VALID_V02 & measStatus) {
        svMeas.measurementStatus |= GNSS_LOC_MEAS_STATUS_SM_VALID;
    }
    if (QMI_LOC_MASK_MEAS_STATUS_SB_VALID_V02 & measStatus) {
        svMeas.measurementStatus |= GNSS_LOC_MEAS_STATUS_SB_VALID;
    }
    if (QMI_LOC_MASK_MEAS_STATUS_MS_VALID_V02 & measStatus) {
        svMeas.measurementStatus |= GNSS_LOC_MEAS_STATUS_MS_VALID;
    }
    if (QMI_LOC_MASK_MEAS_STATUS_BE_CONFIRM_V02 & measStatus) {
        svMeas.measurementStatus |= GNSS_LOC_MEAS_STATUS_BE_CONFIRM;
    }
    if (QMI_LOC_MASK_MEAS_STATUS_VELOCITY_VALID_V02 & measStatus) {
        svMeas.measurementStatus |= GNSS_LOC_MEAS_STATUS_VELOCITY_VALID;
    }
    if (QMI_LOC_MASK_MEAS_STATUS_VELOCITY_FINE_V02 & measStatus) {
        svMeas.measurementStatus |= GNSS_LOC_MEAS_STATUS_VELOCITY_FINE;
    }
    if (QMI_LOC_MASK_MEAS_STATUS_LP_VALID_V02 & measStatus) {
        svMeas.measurementStatus |= GNSS_LOC_MEAS_STATUS_LP_VALID;
    }
    if (QMI_LOC_MASK_MEAS_STATUS_LP_POS_VALID_V02 & measStatus) {
        svMeas.measurementStatus |= GNSS_LOC_MEAS_STATUS_LP_POS_VALID;
    }
    if (QMI_LOC_MASK_MEAS_STATUS_FROM_RNG_DIFF_V02 & measStatus) {
        svMeas.measurementStatus |= GNSS_LOC_MEAS_STATUS_FROM_RNG_DIFF;
    }
    if (QMI_LOC_MASK_MEAS_STATUS_FROM_VE_DIFF_V02 & measStatus) {
        svMeas.measurementStatus |= GNSS_LOC_MEAS_STATUS_FROM_VE_DIFF;
    }
    if (QMI_LOC_MASK_MEAS_STATUS_GNSS_FRESH_MEAS_VALID_V02 & measStatus) {
        svMeas.measurementStatus |= GNSS_LOC_MEAS_STATUS_GNSS_FRESH_MEAS;
    }
    if (gnss_measurement_info.validMask & QMI_LOC_SV_MULTIPATH_EST_VALID_V02) {
        svMeas.multipathEstValid = 1;
        svMeas.multipathEstimate = gnss_measurement_info.multipathEstimate;
    }

    if (gnss_measurement_info.validMask & QMI_LOC_SV_FINE_SPEED_VALID_V02) {
        svMeas.fineSpeedValid = 1;
        svMeas.fineSpeed = gnss_measurement_info.fineSpeed;
    }
    if (gnss_measurement_info.validMask & QMI_LOC_SV_FINE_SPEED_UNC_VALID_V02) {
        svMeas.fineSpeedUncValid = 1;
        svMeas.fineSpeedUnc = gnss_measurement_info.fineSpeedUnc;
    }
    if (gnss_measurement_info.validMask & QMI_LOC_SV_CARRIER_PHASE_VALID_V02) {
        svMeas.carrierPhaseValid = 1;
        svMeas.carrierPhase = gnss_measurement_info.carrierPhase;
    }
    if (gnss_measurement_info.validMask & QMI_LOC_SV_SV_DIRECTION_VALID_V02) {
        svMeas.svDirectionValid = 1;
        svMeas.svElevation = gnss_measurement_info.svElevation;
        svMeas.svAzimuth = gnss_measurement_info.svAzimuth;
    }
    if (gnss_measurement_info.validMask & QMI_LOC_SV_CYCLESLIP_COUNT_VALID_V02) {
        svMeas.cycleSlipCountValid = 1;
        svMeas.cycleSlipCount = gnss_measurement_info.cycleSlipCount;
    }

    if (validDgnssSvMeas) {
        int dgnssSvMeasIndex = index;
        // adjust the dgnss meas index if we are processing external SV measurement
        if (isExt) {
            dgnssSvMeasIndex += gnss_measurement_report_ptr.svMeasurement_len;
        }
        const qmiLocDgnssSVMeasurementStructT_v02* dgnss_sv_meas_ptr =
                &gnss_measurement_report_ptr.dgnssSvMeasurement[dgnssSvMeasIndex];
        svMeas.dgnssSvMeas.dgnssMeasStatus = (LocSvDgnssMeasStatusMask)
                dgnss_sv_meas_ptr->dgnssMeasStatus,
        svMeas.dgnssSvMeas.diffDataEpochTimeMsec =
                dgnss_sv_meas_ptr->diffDataEpochTimeMsec;
        svMeas.dgnssSvMeas.prCorrMeters =
                dgnss_sv_meas_ptr->prCorrMeters;
        svMeas.dgnssSvMeas.prrCorrMetersPerSec =
                dgnss_sv_meas_ptr->prrCorrMetersPerSec;
    }

    if (!isExt) {
        uint32_t svMeas_len = gnss_measurement_report_ptr.svMeasurement_len;
        uint32_t svCarPhUnc_len = gnss_measurement_report_ptr.svCarrierPhaseUncertainty_len;
        if ((1 == gnss_measurement_report_ptr.svCarrierPhaseUncertainty_valid) &&
            (svMeas_len == svCarPhUnc_len)) {
            svMeas.carrierPhaseUncValid = 1;
            svMeas.carrierPhaseUnc =
                    gnss_measurement_report_ptr.svCarrierPhaseUncertainty[index];
        }
    } else {
        uint32_t extSvMeas_len = gnss_measurement_report_ptr.extSvMeasurement_len;
        uint32_t extSvCarPhUnc_len = gnss_measurement_report_ptr.extSvCarrierPhaseUncertainty_len;
        if ((1 == gnss_measurement_report_ptr.extSvCarrierPhaseUncertainty_valid) &&
            (extSvMeas_len == extSvCarPhUnc_len)) {
            svMeas.carrierPhaseUncValid = 1;
            svMeas.carrierPhaseUnc =
                    gnss_measurement_report_ptr.extSvCarrierPhaseUncertainty[index];
        }
    }

    // size
    measurementData.size = sizeof(GnssMeasurementsData);

    // flag initiation
    measurementData.flags = 0;

    // svid
    measurementData.svId = gnss_measurement_info.gnssSvId;

    // constellation
    switch (gnss_measurement_report_ptr.system)
    {
        case eQMI_LOC_SV_SYSTEM_GPS_V02:
            measurementData.svType = GNSS_SV_TYPE_GPS;
            break;

        case eQMI_LOC_SV_SYSTEM_GALILEO_V02:
            measurementData.svType = GNSS_SV_TYPE_GALILEO;
            break;

        case eQMI_LOC_SV_SYSTEM_SBAS_V02:
            measurementData.svType = GNSS_SV_TYPE_SBAS;
            break;

        case eQMI_LOC_SV_SYSTEM_GLONASS_V02:
            measurementData.svType = GNSS_SV_TYPE_GLONASS;
            measurementData.gloFrequency = gnss_measurement_info.gloFrequency;
            break;

        case eQMI_LOC_SV_SYSTEM_BDS_V02:
            measurementData.svType = GNSS_SV_TYPE_BEIDOU;
            break;

        case eQMI_LOC_SV_SYSTEM_QZSS_V02:
            measurementData.svType = GNSS_SV_TYPE_QZSS;
            break;

        case eQMI_LOC_SV_SYSTEM_NAVIC_V02:
            measurementData.svType = GNSS_SV_TYPE_NAVIC;
            break;

        default:
            measurementData.svType = GNSS_SV_TYPE_UNKNOWN;
            break;
    }

    // time_offset_ns
    if (0 != gnss_measurement_info.measLatency)
    {
        LOC_LOGV("%s:%d]: measLatency is not 0\n", __func__, __LINE__);
    }
    measurementData.timeOffsetNs = 0.0;

    // stateMask & receivedSvTimeNs & received_gps_tow_uncertainty_ns
    uint64_t validMeasStatus = gnss_measurement_info.measurementStatus &
                               gnss_measurement_info.validMeasStatusMask;
    uint64_t bitSynMask = QMI_LOC_MASK_MEAS_STATUS_BE_CONFIRM_V02 |
                          QMI_LOC_MASK_MEAS_STATUS_SB_VALID_V02;
    double gpsTowUncNs = (double)gnss_measurement_info.svTimeSpeed.svTimeUncMs * 1e6;

    bool bBandNotAvailable = !gnss_measurement_report_ptr.gnssSignalType_valid;
    qmiLocGnssSignalTypeMaskT_v02 gnssBand;

    if (gnss_measurement_report_ptr.gnssSignalType_valid) {
        gnssBand = gnss_measurement_report_ptr.gnssSignalType;
    }

    measurementData.stateMask = GNSS_MEASUREMENTS_STATE_UNKNOWN_BIT;
    measurementData.receivedSvTimeNs = 0;
    measurementData.receivedSvTimeUncertaintyNs = 0;

    // Deal with Galileo first since it is special
    uint64_t galSVstateMask = 0;
    if (GNSS_SV_TYPE_GALILEO == measurementData.svType &&
        (bBandNotAvailable ||
        (QMI_LOC_MASK_GNSS_SIGNAL_TYPE_GALILEO_E1_C_V02 == gnssBand))) {

        galSVstateMask = GNSS_MEASUREMENTS_STATE_GAL_E1BC_CODE_LOCK_BIT;

        if (gnss_measurement_info.measurementStatus &
            QMI_LOC_MASK_MEAS_STATUS_100MS_STAT_BIT_VALID_V02) {
            galSVstateMask |= GNSS_MEASUREMENTS_STATE_GAL_E1C_2ND_CODE_LOCK_BIT;
        }

        if (gnss_measurement_info.measurementStatus &
            QMI_LOC_MASK_MEAS_STATUS_2S_STAT_BIT_VALID_V02) {
            galSVstateMask |= GNSS_MEASUREMENTS_STATE_GAL_E1B_PAGE_SYNC_BIT;
        }
    }
    // now deal with all constellations
    bool bIsL5orE5 = false;
    // L5
    if (GNSS_SV_TYPE_GPS == measurementData.svType &&
        (bBandNotAvailable ||
        (QMI_LOC_MASK_GNSS_SIGNAL_TYPE_GPS_L5_Q_V02 == gnssBand))) {
        bIsL5orE5 = true;
    }
    // E5
    if (GNSS_SV_TYPE_GALILEO == measurementData.svType &&
        (bBandNotAvailable ||
        (QMI_LOC_MASK_GNSS_SIGNAL_TYPE_GALILEO_E5A_Q_V02 == gnssBand))) {
        bIsL5orE5 = true;
    }

    if (validMeasStatus & QMI_LOC_MASK_MEAS_STATUS_MS_VALID_V02) {
        /* sub-frame decode & TOW decode */
        measurementData.stateMask |= (GNSS_MEASUREMENTS_STATE_SUBFRAME_SYNC_BIT |
                                      GNSS_MEASUREMENTS_STATE_TOW_DECODED_BIT |
                                      GNSS_MEASUREMENTS_STATE_TOW_KNOWN_BIT |
                                      GNSS_MEASUREMENTS_STATE_BIT_SYNC_BIT |
                                      GNSS_MEASUREMENTS_STATE_CODE_LOCK_BIT);
        // GLO
        if (GNSS_SV_TYPE_GLONASS == measurementData.svType &&
            (bBandNotAvailable ||
            (QMI_LOC_MASK_GNSS_SIGNAL_TYPE_GLONASS_G1_V02 == gnssBand))) {

            measurementData.stateMask |= (GNSS_MEASUREMENTS_STATE_GLO_TOD_KNOWN_BIT |
                                          GNSS_MEASUREMENTS_STATE_GLO_TOD_DECODED_BIT |
                                          GNSS_MEASUREMENTS_STATE_GLO_STRING_SYNC_BIT);
        }

        // GAL
        measurementData.stateMask |= galSVstateMask;

        // BDS
        if (IS_BDS_GEO_SV(measurementData.svId, measurementData.svType)) {
            /* BDS_GEO SV transmitting D2 signal */
            measurementData.stateMask |= (GNSS_MEASUREMENTS_STATE_BDS_D2_BIT_SYNC_BIT |
                                          GNSS_MEASUREMENTS_STATE_BDS_D2_SUBFRAME_SYNC_BIT);
        }

        // L5 or E5
        if (bIsL5orE5) {
            measurementData.stateMask |= GNSS_MEASUREMENTS_STATE_2ND_CODE_LOCK_BIT;
        }

        measurementData.receivedSvTimeNs =
            ((int64_t)gnss_measurement_info.svTimeSpeed.svTimeMs * 1000000) +
            (int64_t)(gnss_measurement_info.svTimeSpeed.svTimeSubMs * 1e6);

        measurementData.receivedSvTimeUncertaintyNs = (int64_t)gpsTowUncNs;

    } else if ((validMeasStatus & bitSynMask) == bitSynMask) {
        /* bit sync */
        measurementData.stateMask |= (GNSS_MEASUREMENTS_STATE_BIT_SYNC_BIT |
                                      GNSS_MEASUREMENTS_STATE_SYMBOL_SYNC_BIT |
                                      GNSS_MEASUREMENTS_STATE_CODE_LOCK_BIT);
        measurementData.stateMask |= galSVstateMask;

        // L5 or E5
        if (bIsL5orE5) {
            measurementData.stateMask |= GNSS_MEASUREMENTS_STATE_2ND_CODE_LOCK_BIT;
        }

        measurementData.receivedSvTimeNs =
            (((int64_t)gnss_measurement_info.svTimeSpeed.svTimeMs * 1000000) +
            (int64_t)(gnss_measurement_info.svTimeSpeed.svTimeSubMs * 1e6)) % (int64_t)20;

        measurementData.receivedSvTimeUncertaintyNs = (int64_t)gpsTowUncNs;

    } else if (validMeasStatus & QMI_LOC_MASK_MEAS_STATUS_SM_VALID_V02) {
        /* code lock */
        measurementData.stateMask |= GNSS_MEASUREMENTS_STATE_CODE_LOCK_BIT;
        measurementData.stateMask |= galSVstateMask;

        measurementData.receivedSvTimeNs =
            (int64_t)(gnss_measurement_info.svTimeSpeed.svTimeSubMs * 1e6);

        measurementData.receivedSvTimeUncertaintyNs = (int64_t)gpsTowUncNs;
    }

    // carrierToNoiseDbHz
    measurementData.carrierToNoiseDbHz = gnss_measurement_info.CNo/10.0;

    if (QMI_LOC_MASK_MEAS_STATUS_VELOCITY_FINE_V02 == (gnss_measurement_info.measurementStatus & QMI_LOC_MASK_MEAS_STATUS_VELOCITY_FINE_V02))
    {
        LOC_LOGV ("%s:%d]: FINE mS=0x%4" PRIX64 " fS=%f fSU=%f dS=%f dSU=%f\n", __func__, __LINE__,
        gnss_measurement_info.measurementStatus,
        gnss_measurement_info.fineSpeed, gnss_measurement_info.fineSpeedUnc,
        gnss_measurement_info.svTimeSpeed.dopplerShift, gnss_measurement_info.svTimeSpeed.dopplerShiftUnc);
        // pseudorangeRateMps
        measurementData.pseudorangeRateMps = gnss_measurement_info.fineSpeed;

        // pseudorangeRateUncertaintyMps
        measurementData.pseudorangeRateUncertaintyMps = gnss_measurement_info.fineSpeedUnc;
    }
    else
    {
        LOC_LOGV ("%s:%d]: COARSE mS=0x%4" PRIX64 " fS=%f fSU=%f dS=%f dSU=%f\n", __func__, __LINE__,
        gnss_measurement_info.measurementStatus,
        gnss_measurement_info.fineSpeed, gnss_measurement_info.fineSpeedUnc,
        gnss_measurement_info.svTimeSpeed.dopplerShift, gnss_measurement_info.svTimeSpeed.dopplerShiftUnc);
        // pseudorangeRateMps
        measurementData.pseudorangeRateMps = gnss_measurement_info.svTimeSpeed.dopplerShift;

        // pseudorangeRateUncertaintyMps
        measurementData.pseudorangeRateUncertaintyMps = gnss_measurement_info.svTimeSpeed.dopplerShiftUnc;
    }

    // carrier frequency
    if (gnss_measurement_report_ptr.gnssSignalType_valid) {
        LOC_LOGv("gloFrequency = 0x%X, sigType=%" PRIu64,
                 gnss_measurement_info.gloFrequency, gnss_measurement_report_ptr.gnssSignalType);
        measurementData.carrierFrequencyHz = convertSignalTypeToCarrierFrequency(
                gnss_measurement_report_ptr.gnssSignalType, gnss_measurement_info.gloFrequency);
        measurementData.flags |= GNSS_MEASUREMENTS_DATA_CARRIER_FREQUENCY_BIT;
    }
    else {
        measurementData.carrierFrequencyHz = 0;
        // GLONASS is FDMA system, so each channel has its own carrier frequency
        // The formula is f(k) = fc + k * 0.5625;
        // This is applicable for GLONASS G1 only, where fc = 1602MHz
        if ((gnss_measurement_info.gloFrequency >= 1 && gnss_measurement_info.gloFrequency <= 14)) {
            measurementData.carrierFrequencyHz += (gnss_measurement_info.gloFrequency - 8) * 562500;
        }
        measurementData.carrierFrequencyHz += CarrierFrequencies[measurementData.svType];
        measurementData.flags |= GNSS_MEASUREMENTS_DATA_CARRIER_FREQUENCY_BIT;

        LOC_LOGv("gnss_measurement_report_ptr.gnssSignalType_valid = 0");
    }

    // accumulatedDeltaRangeM
    if (gnss_measurement_info.validMask & QMI_LOC_SV_CARRIER_PHASE_VALID_V02) {
        measurementData.carrierPhase = gnss_measurement_info.carrierPhase;
        measurementData.flags |= GNSS_MEASUREMENTS_DATA_CARRIER_PHASE_BIT;
        if ((validMeasStatus & QMI_LOC_MASK_MEAS_STATUS_LP_VALID_V02) &&
            (validMeasStatus & QMI_LOC_MASK_MEAS_STATUS_LP_POS_VALID_V02)) {
            measurementData.carrierPhase += 0.5;
        }
        measurementData.adrMeters =
            (SPEED_OF_LIGHT / measurementData.carrierFrequencyHz) * measurementData.carrierPhase;
        LOC_LOGv("carrierPhase = %.2f adrMeters = %.2f",
                 measurementData.carrierPhase,
                 measurementData.adrMeters);
    } else {
        measurementData.adrMeters = 0.0;
    }

    // carrierPhaseUncertainty
    if (1 == svMeas.carrierPhaseUncValid) {
        measurementData.carrierPhaseUncertainty = svMeas.carrierPhaseUnc;
        measurementData.flags |= GNSS_MEASUREMENTS_DATA_CARRIER_PHASE_UNCERTAINTY_BIT;
    }

    // accumulatedDeltaRangeUncertaintyM
    if (!isExt) {
        if (gnss_measurement_report_ptr.svCarrierPhaseUncertainty_valid) {
            measurementData.adrUncertaintyMeters =
                (SPEED_OF_LIGHT / measurementData.carrierFrequencyHz) *
                gnss_measurement_report_ptr.svCarrierPhaseUncertainty[index];
            LOC_LOGv("carrierPhaseUnc = %.6f adrMetersUnc = %.6f",
                     gnss_measurement_report_ptr.svCarrierPhaseUncertainty[index],
                     measurementData.adrUncertaintyMeters);
        } else {
            measurementData.adrUncertaintyMeters = 0.0;
        }
    } else {
        if (gnss_measurement_report_ptr.extSvCarrierPhaseUncertainty_valid) {
            measurementData.adrUncertaintyMeters =
                (SPEED_OF_LIGHT / measurementData.carrierFrequencyHz) *
                gnss_measurement_report_ptr.extSvCarrierPhaseUncertainty[index];
            LOC_LOGv("extCarrierPhaseUnc = %.6f adrMetersUnc = %.6f",
                     gnss_measurement_report_ptr.extSvCarrierPhaseUncertainty[index],
                     measurementData.adrUncertaintyMeters);
        } else {
            measurementData.adrUncertaintyMeters = 0.0;
        }
    }

    // accumulatedDeltaRangeState
    measurementData.adrStateMask = GNSS_MEASUREMENTS_ACCUMULATED_DELTA_RANGE_STATE_UNKNOWN;
    if ((gnss_measurement_info.validMask & QMI_LOC_SV_CARRIER_PHASE_VALID_V02) &&
        (gnss_measurement_info.carrierPhase != 0.0)) {
        measurementData.adrStateMask = GNSS_MEASUREMENTS_ACCUMULATED_DELTA_RANGE_STATE_VALID_BIT;

        bool bFound = false;
        adrData tempAdrData;
        vector<adrData>::iterator it;
        // check the prior epoch
        // first see if info for this satellite exists in the vector (from prior epoch)
        for (it = mADRdata.begin(); it != mADRdata.end(); ++it) {
            tempAdrData = *it;
            if (gnss_measurement_report_ptr.system == tempAdrData.system &&
                ((gnss_measurement_report_ptr.gnssSignalType_valid &&
                (gnss_measurement_report_ptr.gnssSignalType == tempAdrData.gnssSignalType)) ||
                 (!gnss_measurement_report_ptr.gnssSignalType_valid &&
                    (0 == tempAdrData.gnssSignalType))) &&
                gnss_measurement_info.gnssSvId == tempAdrData.gnssSvId) {
                bFound = true;
                break;
            }
        }
        measurementData.adrStateMask |= GNSS_MEASUREMENTS_ACCUMULATED_DELTA_RANGE_STATE_RESET_BIT;
        if (bFound) {
            LOC_LOGv("Found the carrier phase for this satellite from last epoch");
            if (tempAdrData.validMask & QMI_LOC_SV_CARRIER_PHASE_VALID_V02) {
                LOC_LOGv("and it has valid carrier phase");
                // let's make sure this is prior measurement
                if (mMinInterval <= 1000 &&
                    (tempAdrData.counter == (mCounter - 1))) {
                    // at this point prior epoch does have carrier phase valid
                    measurementData.adrStateMask &=
                            ~GNSS_MEASUREMENTS_ACCUMULATED_DELTA_RANGE_STATE_RESET_BIT;
                    if (tempAdrData.validMask & QMI_LOC_SV_CYCLESLIP_COUNT_VALID_V02 &&
                        gnss_measurement_info.validMask & QMI_LOC_SV_CYCLESLIP_COUNT_VALID_V02) {
                        LOC_LOGv("cycle slip count is valid for both current and prior epochs");
                        if (tempAdrData.cycleSlipCount != gnss_measurement_info.cycleSlipCount) {
                            LOC_LOGv("cycle slip count for current epoch (%d)"
                                     " is different than the last epoch(%d)",
                                     gnss_measurement_info.cycleSlipCount,
                                     tempAdrData.cycleSlipCount);
                            measurementData.adrStateMask |=
                                    GNSS_MEASUREMENTS_ACCUMULATED_DELTA_RANGE_STATE_CYCLE_SLIP_BIT;
                        }
                    }
                }
            }
            // now update the current satellite info to the vector
            tempAdrData.counter = mCounter;
            tempAdrData.validMask = gnss_measurement_info.validMask;
            tempAdrData.cycleSlipCount = gnss_measurement_info.cycleSlipCount;
            *it = tempAdrData;
        } else {
            // now add the current satellite info to the vector
            tempAdrData.counter = mCounter;
            tempAdrData.system = gnss_measurement_report_ptr.system;
            if (gnss_measurement_report_ptr.gnssSignalType_valid) {
                tempAdrData.gnssSignalType = gnss_measurement_report_ptr.gnssSignalType;
            } else {
                tempAdrData.gnssSignalType = 0;
            }
            tempAdrData.gnssSvId = gnss_measurement_info.gnssSvId;
            tempAdrData.validMask = gnss_measurement_info.validMask;
            tempAdrData.cycleSlipCount = gnss_measurement_info.cycleSlipCount;
            mADRdata.push_back(tempAdrData);
        }

        if (validMeasStatus & QMI_LOC_MASK_MEAS_STATUS_LP_VALID_V02) {
            LOC_LOGv("measurement status has QMI_LOC_MASK_MEAS_STATUS_LP_VALID_V02 set");
            measurementData.adrStateMask |=
                    GNSS_MEASUREMENTS_ACCUMULATED_DELTA_RANGE_STATE_HALF_CYCLE_RESOLVED_BIT;
        }
        LOC_LOGv("adrStateMask = 0x%02x", measurementData.adrStateMask);
    }

    // cycleSlipCount
    if (1 == svMeas.cycleSlipCountValid) {
        measurementData.cycleSlipCount = svMeas.cycleSlipCount;
        measurementData.flags |= GNSS_MEASUREMENTS_DATA_CYCLE_SLIP_COUNT_BIT;
    }

    // multipath_indicator
    measurementData.multipathIndicator = GNSS_MEASUREMENTS_MULTIPATH_INDICATOR_UNKNOWN;

    // AGC
    if (gnss_measurement_report_ptr.jammerIndicator_valid) {
        if (GNSS_INVALID_JAMMER_IND !=
            gnss_measurement_report_ptr.jammerIndicator.agcMetricDb) {
            LOC_LOGv("AGC is valid: agcMetricDb = 0x%X bpMetricDb = 0x%X",
                gnss_measurement_report_ptr.jammerIndicator.agcMetricDb,
                gnss_measurement_report_ptr.jammerIndicator.bpMetricDb);

            measurementData.agcLevelDb =
                (double)gnss_measurement_report_ptr.jammerIndicator.agcMetricDb / 100.0;
            measurementData.flags |= GNSS_MEASUREMENTS_DATA_AUTOMATIC_GAIN_CONTROL_BIT;
        } else {
            LOC_LOGv("AGC is invalid: agcMetricDb = 0x%X bpMetricDb = 0x%X",
                 gnss_measurement_report_ptr.jammerIndicator.agcMetricDb,
                 gnss_measurement_report_ptr.jammerIndicator.bpMetricDb);
        }
        bAgcIsPresent = true;
    } else {
        LOC_LOGv("AGC is not present");
        bAgcIsPresent = false;
    }

    // code type
    if (gnss_measurement_report_ptr.measurementCodeType_valid) {
        switch (gnss_measurement_report_ptr.measurementCodeType)
        {
        case eQMI_LOC_GNSS_CODE_TYPE_A_V02:
            measurementData.codeType = GNSS_MEASUREMENTS_CODE_TYPE_A; break;
        case eQMI_LOC_GNSS_CODE_TYPE_B_V02:
            measurementData.codeType = GNSS_MEASUREMENTS_CODE_TYPE_B; break;
        case eQMI_LOC_GNSS_CODE_TYPE_C_V02:
            measurementData.codeType = GNSS_MEASUREMENTS_CODE_TYPE_C; break;
        case eQMI_LOC_GNSS_CODE_TYPE_I_V02:
            measurementData.codeType = GNSS_MEASUREMENTS_CODE_TYPE_I; break;
        case eQMI_LOC_GNSS_CODE_TYPE_L_V02:
            measurementData.codeType = GNSS_MEASUREMENTS_CODE_TYPE_L; break;
        case eQMI_LOC_GNSS_CODE_TYPE_M_V02:
            measurementData.codeType = GNSS_MEASUREMENTS_CODE_TYPE_M; break;
        case eQMI_LOC_GNSS_CODE_TYPE_P_V02:
            measurementData.codeType = GNSS_MEASUREMENTS_CODE_TYPE_P; break;
        case eQMI_LOC_GNSS_CODE_TYPE_Q_V02:
            measurementData.codeType = GNSS_MEASUREMENTS_CODE_TYPE_Q; break;
        case eQMI_LOC_GNSS_CODE_TYPE_S_V02:
            measurementData.codeType = GNSS_MEASUREMENTS_CODE_TYPE_S; break;
        case eQMI_LOC_GNSS_CODE_TYPE_W_V02:
            measurementData.codeType = GNSS_MEASUREMENTS_CODE_TYPE_W; break;
        case eQMI_LOC_GNSS_CODE_TYPE_X_V02:
            measurementData.codeType = GNSS_MEASUREMENTS_CODE_TYPE_X; break;
        case eQMI_LOC_GNSS_CODE_TYPE_Y_V02:
            measurementData.codeType = GNSS_MEASUREMENTS_CODE_TYPE_Y; break;
        case eQMI_LOC_GNSS_CODE_TYPE_Z_V02:
            measurementData.codeType = GNSS_MEASUREMENTS_CODE_TYPE_Z; break;
        case eQMI_LOC_GNSS_CODE_TYPE_N_V02:
            measurementData.codeType = GNSS_MEASUREMENTS_CODE_TYPE_N; break;
        default:
            measurementData.codeType = GNSS_MEASUREMENTS_CODE_TYPE_OTHER; break;
        }
    } else {
        measurementData.codeType = GNSS_MEASUREMENTS_CODE_TYPE_OTHER;
    }

    memset(measurementData.otherCodeTypeName, 0, GNSS_MAX_NAME_LENGTH);
    if (GNSS_MEASUREMENTS_CODE_TYPE_OTHER == measurementData.codeType) {
        if (gnss_measurement_report_ptr.otherCodeTypeName_valid) {
            strlcpy(measurementData.otherCodeTypeName,
                    gnss_measurement_report_ptr.otherCodeTypeName,
                    std::min((uint32_t)sizeof(measurementData.otherCodeTypeName),
                             (uint32_t)gnss_measurement_report_ptr.otherCodeTypeName_len+1));
            LOC_LOGv("measurementData.otherCodeTypeName = %s", measurementData.otherCodeTypeName);
        }
    }

    // basebandCarrierToNoiseDbHz
    measurementData.basebandCarrierToNoiseDbHz = 0.0;
    if (measurementData.carrierToNoiseDbHz > gnss_measurement_info.gloRfLoss / 10.0) {
        measurementData.basebandCarrierToNoiseDbHz = measurementData.carrierToNoiseDbHz -
                gnss_measurement_info.gloRfLoss / 10.0;
    }

    // intersignal bias
    if (gnss_measurement_report_ptr.gnssSignalType_valid &&
        (gnss_measurement_report_ptr.systemTime_valid ||
         gnss_measurement_report_ptr.gloTime_valid)) {
        measurementData.gnssSignalType =
                convertQmiGnssSignalType(gnss_measurement_report_ptr.gnssSignalType);
    }

    LOC_LOGv(" GNSS measurement raw data received from modem:\n"
             " Input => gnssSvId=%d validMask=0x%04x validMeasStatus=0x%" PRIx64
             "  CNo=%d gloRfLoss=%d dopplerShift=%.2f dopplerShiftUnc=%.2f"
             "  fineSpeed=%.2f fineSpeedUnc=%.2f"
             "  svTimeMs=%u svTimeSubMs=%.2f svTimeUncMs=%.2f codeType=%d"
             "  carrierPhase=%.2f carrierPhaseUnc=%.6f cycleSlipCount=%u\n"
             " GNSS measurement data after conversion:"
             " Output => size=%" PRIu32 "svid=%d time_offset_ns=%.2f stateMask=0x%08x"
             "  received_sv_time_in_ns=%" PRIu64 " received_sv_time_uncertainty_in_ns=%" PRIu64
             "  c_n0_dbhz=%.2f baseband_c_n0_dbhz=%.2f"
             "  pseudorange_rate_mps=%.2f pseudorange_rate_uncertainty_mps=%.2f"
             "  adrStateMask=0x%02x adrMeters=%.2f adrUncertaintyMeters=%.6f"
             "  carrierFrequencyHz=%.2f codeType=%d",
             gnss_measurement_info.gnssSvId,                                    // %d
             gnss_measurement_info.validMask,                                   // 0x%4x
             validMeasStatus,                                                   // %PRIx64
             gnss_measurement_info.CNo,                                         // %d
             gnss_measurement_info.gloRfLoss,                                   // %d
             gnss_measurement_info.svTimeSpeed.dopplerShift,                    // %f
             gnss_measurement_info.svTimeSpeed.dopplerShiftUnc,                 // %f
             gnss_measurement_info.fineSpeed,                                   // %f
             gnss_measurement_info.fineSpeedUnc,                                // %f
             gnss_measurement_info.svTimeSpeed.svTimeMs,                        // %u
             gnss_measurement_info.svTimeSpeed.svTimeSubMs,                     // %f
             gnss_measurement_info.svTimeSpeed.svTimeUncMs,                     // %f
             gnss_measurement_report_ptr.measurementCodeType,                   // %d
             gnss_measurement_info.carrierPhase,                                // %f
             gnss_measurement_report_ptr.svCarrierPhaseUncertainty[index],      // %f
             gnss_measurement_info.cycleSlipCount,                              // %u
             measurementData.size,                                              // %zu
             measurementData.svId,                                              // %d
             measurementData.timeOffsetNs,                                      // %f
             measurementData.stateMask,                                         // 0x%8x
             measurementData.receivedSvTimeNs,                                  // %PRIu64
             measurementData.receivedSvTimeUncertaintyNs,                       // %PRIu64
             measurementData.carrierToNoiseDbHz,                                // %g
             measurementData.basebandCarrierToNoiseDbHz,                        // %g
             measurementData.pseudorangeRateMps,                                // %g
             measurementData.pseudorangeRateUncertaintyMps,                     // %g
             measurementData.adrStateMask,                                      // 0x%2x
             measurementData.adrMeters,                                         // %f
             measurementData.adrUncertaintyMeters,                              // %f
             measurementData.carrierFrequencyHz,                                // %f
             measurementData.codeType);                                         // %d
    return bAgcIsPresent;
}

/*convert GnssMeasurementsClock type from QMI LOC to loc eng format*/
int LocApiV02 :: convertGnssClock (GnssMeasurementsClock& clock,
    const qmiLocEventGnssSvMeasInfoIndMsgT_v02& gnss_measurement_info)
{
    static uint32_t oldRefFCount = 0;
    static uint32_t newRefFCount = 0;
    static uint32_t oldDiscCount = 0;
    static uint32_t newDiscCount = 0;
    static uint32_t localDiscCount = 0;
    int msInWeek = -1;

    LOC_LOGV ("%s:%d]: entering\n", __func__, __LINE__);

    // size
    clock.size = sizeof(GnssMeasurementsClock);

    // flag initiation
    GnssMeasurementsClockFlagsMask flags = 0;

    if (gnss_measurement_info.systemTimeExt_valid &&
        gnss_measurement_info.numClockResets_valid) {
        newRefFCount = gnss_measurement_info.systemTimeExt.refFCount;
        newDiscCount = gnss_measurement_info.numClockResets;
        if ((true == mMeasurementsStarted) ||
            (oldDiscCount != newDiscCount) ||
            (newRefFCount <= oldRefFCount))
        {
            if (true == mMeasurementsStarted)
            {
                mMeasurementsStarted = false;
            }
            // do not increment in full power mode
            if (GNSS_POWER_MODE_M1 != mPowerMode) {
                localDiscCount++;
            }
        }
        oldDiscCount = newDiscCount;
        oldRefFCount = newRefFCount;

        // timeNs & timeUncertaintyNs
        clock.timeNs = (int64_t)gnss_measurement_info.systemTimeExt.refFCount * 1e6;
        clock.hwClockDiscontinuityCount = localDiscCount;
        clock.timeUncertaintyNs = 0.0;

        msInWeek = (int)gnss_measurement_info.systemTime.systemMsec;
        if (gnss_measurement_info.systemTime_valid) {
            uint16_t systemWeek = gnss_measurement_info.systemTime.systemWeek;
            uint32_t systemMsec = gnss_measurement_info.systemTime.systemMsec;
            float sysClkBias = gnss_measurement_info.systemTime.systemClkTimeBias;
            float sysClkUncMs = gnss_measurement_info.systemTime.systemClkTimeUncMs;
            bool isTimeValid = (sysClkUncMs <= 16.0f); // 16ms

            if (systemWeek != C_GPS_WEEK_UNKNOWN && isTimeValid) {
                // fullBiasNs, biasNs & biasUncertaintyNs
                int64_t totalMs = ((int64_t)systemWeek) *
                                  ((int64_t)WEEK_MSECS) + ((int64_t)systemMsec);
                int64_t gpsTimeNs = totalMs * 1000000 - (int64_t)(sysClkBias * 1e6);
                clock.fullBiasNs = clock.timeNs - gpsTimeNs;
                clock.biasNs = sysClkBias * 1e6 - (double)((int64_t)(sysClkBias * 1e6));
                clock.biasUncertaintyNs = (double)sysClkUncMs * 1e6;
                flags |= (GNSS_MEASUREMENTS_CLOCK_FLAGS_FULL_BIAS_BIT |
                          GNSS_MEASUREMENTS_CLOCK_FLAGS_BIAS_BIT |
                          GNSS_MEASUREMENTS_CLOCK_FLAGS_BIAS_UNCERTAINTY_BIT);

                if (mGnssMeasurements->gnssSvMeasurementSet.svMeasSetHeader.flags &
                    GNSS_SV_MEAS_HEADER_HAS_LEAP_SECOND) {
                    clock.leapSecond = mGnssMeasurements->
                            gnssSvMeasurementSet.svMeasSetHeader.leapSec.leapSec;
                    flags |= GNSS_MEASUREMENTS_CLOCK_FLAGS_LEAP_SECOND_BIT;
                    LOC_LOGV("clock.leapSecond: %d", clock.leapSecond);
                } else {
                    LOC_LOGV("GNSS_SV_MEAS_HEADER_HAS_LEAP_SECOND is not set");
                }
            }
        }
    }

    // driftNsps & driftUncertaintyNsps
    if (gnss_measurement_info.rcvrClockFrequencyInfo_valid)
    {
        double driftMPS = gnss_measurement_info.rcvrClockFrequencyInfo.clockDrift;
        double driftUncMPS = gnss_measurement_info.rcvrClockFrequencyInfo.clockDriftUnc;

        clock.driftNsps = driftMPS * MPS_TO_NSPS;
        clock.driftUncertaintyNsps = driftUncMPS * MPS_TO_NSPS;

        flags |= (GNSS_MEASUREMENTS_CLOCK_FLAGS_DRIFT_BIT |
                  GNSS_MEASUREMENTS_CLOCK_FLAGS_DRIFT_UNCERTAINTY_BIT);
    }

    // referenceSignalTypeForIsb
    clock.referenceSignalTypeForIsb.svType = GNSS_SV_TYPE_GPS;
    clock.referenceSignalTypeForIsb.carrierFrequencyHz = GPS_L1CA_CARRIER_FREQUENCY;
    clock.referenceSignalTypeForIsb.codeType = GNSS_MEASUREMENTS_CODE_TYPE_C;
    clock.referenceSignalTypeForIsb.otherCodeTypeName[0] = '\0';

    clock.flags = flags;

    LOC_LOGV(" %s:%d]: GNSS measurement clock data received from modem: \n", __func__, __LINE__);
    LOC_LOGV(" Input => systemTime_valid=%d systemTimeExt_valid=%d numClockResets_valid=%d\n",
             gnss_measurement_info.systemTime_valid,                      // %d
             gnss_measurement_info.systemTimeExt_valid,                   // %d
        gnss_measurement_info.numClockResets_valid);                 // %d

    LOC_LOGV("  systemWeek=%d systemMsec=%d systemClkTimeBias=%f\n",
             gnss_measurement_info.systemTime.systemWeek,                 // %d
             gnss_measurement_info.systemTime.systemMsec,                 // %d
        gnss_measurement_info.systemTime.systemClkTimeBias);         // %f

    LOC_LOGV("  systemClkTimeUncMs=%f refFCount=%d numClockResets=%d\n",
             gnss_measurement_info.systemTime.systemClkTimeUncMs,         // %f
        gnss_measurement_info.systemTimeExt.refFCount,               // %d
        gnss_measurement_info.numClockResets);                       // %d

    LOC_LOGV("  clockDrift=%f clockDriftUnc=%f\n",
        gnss_measurement_info.rcvrClockFrequencyInfo.clockDrift,     // %f
        gnss_measurement_info.rcvrClockFrequencyInfo.clockDriftUnc); // %f


    LOC_LOGV(" %s:%d]: GNSS measurement clock after conversion: \n", __func__, __LINE__);
    LOC_LOGV(" Output => timeNs=%" PRId64 "\n",
        clock.timeNs);                       // %PRId64

    LOC_LOGV("  fullBiasNs=%" PRId64 " biasNs=%g bias_uncertainty_ns=%g\n",
        clock.fullBiasNs,                    // %PRId64
        clock.biasNs,                        // %g
        clock.biasUncertaintyNs);            // %g

    LOC_LOGV("  driftNsps=%g drift_uncertainty_nsps=%g\n",
        clock.driftNsps,                     // %g
        clock.driftUncertaintyNsps);         // %g

    LOC_LOGV("  hw_clock_discontinuity_count=%d flags=0x%04x\n",
        clock.hwClockDiscontinuityCount,     // %lld
        clock.flags);                        // %04x

    return msInWeek;
}

/* event callback registered with the loc_api v02 interface */
void LocApiV02 :: eventCb(locClientHandleType /*clientHandle*/,
  uint32_t eventId, locClientEventIndUnionType eventPayload)
{
  LOC_LOGd("event id = 0x%X", eventId);

  switch(eventId)
  {
    //Position Report
    case QMI_LOC_EVENT_POSITION_REPORT_IND_V02:
      reportPosition(eventPayload.pPositionReportEvent);
      break;

    // Satellite report
    case QMI_LOC_EVENT_GNSS_SV_INFO_IND_V02:
      reportSv(eventPayload.pGnssSvInfoReportEvent);
      break;

    // Status report
    case QMI_LOC_EVENT_ENGINE_STATE_IND_V02:
      reportEngineState(eventPayload.pEngineState);
      break;

    case QMI_LOC_EVENT_FIX_SESSION_STATE_IND_V02:
      reportFixSessionState(eventPayload.pFixSessionState);
      break;

    // NMEA
    case QMI_LOC_EVENT_NMEA_IND_V02:
      reportNmea(eventPayload.pNmeaReportEvent);
      break;

    // XTRA request
    case QMI_LOC_EVENT_INJECT_PREDICTED_ORBITS_REQ_IND_V02:
      LOC_LOGD("%s:%d]: XTRA download request\n", __func__,
                    __LINE__);
      reportXtraServerUrl(eventPayload.pInjectPredictedOrbitsReqEvent);
      requestXtraData();
      break;

    // time request
    case QMI_LOC_EVENT_INJECT_TIME_REQ_IND_V02:
      LOC_LOGD("%s:%d]: Time request\n", __func__,
                    __LINE__);
      requestTime();
      break;

    //position request
    case QMI_LOC_EVENT_INJECT_POSITION_REQ_IND_V02:
      LOC_LOGD("%s:%d]: Position request\n", __func__,
                    __LINE__);
      requestLocation();
      break;

    // NI request
    case QMI_LOC_EVENT_NI_NOTIFY_VERIFY_REQ_IND_V02:
      reportNiRequest(eventPayload.pNiNotifyVerifyReqEvent);
      break;

    // AGPS connection request
    case QMI_LOC_EVENT_LOCATION_SERVER_CONNECTION_REQ_IND_V02:
      reportAtlRequest(eventPayload.pLocationServerConnReqEvent);
      break;

    case QMI_LOC_EVENT_GNSS_MEASUREMENT_REPORT_IND_V02:
      LOC_LOGd("GNSS Measurement Report");
      if (mInSession) {
          reportGnssMeasurementData(*eventPayload.pGnssSvRawInfoEvent);
      }
      break;

    case QMI_LOC_EVENT_SV_POLYNOMIAL_REPORT_IND_V02:
      LOC_LOGD("%s:%d]: GNSS SV Polynomial Ind\n", __func__,
               __LINE__);
      reportSvPolynomial(eventPayload.pGnssSvPolyInfoEvent);
      break;

    case QMI_LOC_EVENT_GPS_EPHEMERIS_REPORT_IND_V02:
    case QMI_LOC_EVENT_GLONASS_EPHEMERIS_REPORT_IND_V02:
    case QMI_LOC_EVENT_BDS_EPHEMERIS_REPORT_IND_V02:
    case QMI_LOC_EVENT_GALILEO_EPHEMERIS_REPORT_IND_V02:
    case QMI_LOC_EVENT_QZSS_EPHEMERIS_REPORT_IND_V02:
      reportSvEphemeris(eventId, eventPayload);
      break;

    //Unpropagated position report
    case QMI_LOC_EVENT_UNPROPAGATED_POSITION_REPORT_IND_V02:
      reportPosition(eventPayload.pPositionReportEvent, true);
      break;

    case QMI_LOC_GET_BLACKLIST_SV_IND_V02:
      LOC_LOGd("GET blacklist SV Ind");
      reportGnssSvIdConfig(*eventPayload.pGetBlacklistSvEvent);
      break;

    case QMI_LOC_GET_CONSTELLATION_CONTROL_IND_V02:
      LOC_LOGd("GET constellation Ind");
      reportGnssSvTypeConfig(*eventPayload.pGetConstellationConfigEvent);
      break;

    case  QMI_LOC_EVENT_WIFI_REQ_IND_V02:
      LOC_LOGd("WIFI Req Ind");
      requestOdcpi(*eventPayload.pWifiReqEvent);
      break;

    case QMI_LOC_EVENT_REPORT_IND_V02:
      reportLocEvent(eventPayload.pLocEvent);
      break;

    // System info event regarding next leap second
    case QMI_LOC_SYSTEM_INFO_IND_V02:
      reportSystemInfo(eventPayload.pLocSystemInfoEvent);
      break;

    case QMI_LOC_LOCATION_REQUEST_NOTIFICATION_IND_V02:
      reportLocationRequestNotification(eventPayload.pLocReqNotifEvent);
      break;

    case QMI_LOC_EVENT_GEOFENCE_BREACH_NOTIFICATION_IND_V02:
      LOC_LOGd("Got QMI_LOC_EVENT_GEOFENCE_BREACH_NOTIFICATION_IND_V02");
      geofenceBreachEvent(eventPayload.pGeofenceBreachEvent);
      break;

    case QMI_LOC_EVENT_GEOFENCE_BATCHED_BREACH_NOTIFICATION_IND_V02:
      LOC_LOGd("Got QMI_LOC_EVENT_GEOFENCE_BATCHED_BREACH_NOTIFICATION_IND_V02");
      geofenceBreachEvent(eventPayload.pGeofenceBatchedBreachEvent);
      break;

    case QMI_LOC_EVENT_GEOFENCE_GEN_ALERT_IND_V02:
      geofenceStatusEvent(eventPayload.pGeofenceGenAlertEvent);
      break;

    case QMI_LOC_EVENT_GEOFENCE_BATCHED_DWELL_NOTIFICATION_IND_V02:
      LOC_LOGd("Got QMI_LOC_EVENT_GEOFENCE_BATCHED_DWELL_NOTIFICATION_IND_V02");
      geofenceDwellEvent(eventPayload.pGeofenceBatchedDwellEvent);
      break;

    case QMI_LOC_EVENT_BATCH_FULL_NOTIFICATION_IND_V02:
      LOC_LOGd("Got QMI_LOC_EVENT_BATCH_FULL_NOTIFICATION_IND_V02");
      batchFullEvent(eventPayload.pBatchCount);
      break;

    case QMI_LOC_EVENT_BATCHING_STATUS_IND_V02:
      LOC_LOGd("Got QMI_LOC_EVENT_BATCHING_STATUS_IND_V02");
      batchStatusEvent(eventPayload.pBatchingStatusEvent);
      break;

    case QMI_LOC_EVENT_DBT_POSITION_REPORT_IND_V02:
      LOC_LOGd("Got QMI_LOC_EVENT_DBT_POSITION_REPORT_IND_V02");
      onDbtPosReportEvent(eventPayload.pDbtPositionReportEvent);
      break;

    case QMI_LOC_LATENCY_INFORMATION_IND_V02:
      reportLatencyInfo(eventPayload.pLocLatencyInfoIndMsg);
      break;

    case QMI_LOC_EVENT_ENGINE_LOCK_STATE_IND_V02:
        LOC_LOGd("Got QMI_LOC_EVENT_ENGINE_STATE_IND_V02");
        reportEngineLockStatus(eventPayload.pEngineLockStateIndMsg->engineLockState);
        break;
  }
}

/* Call the service LocAdapterBase down event*/
void LocApiV02 :: errorCb(locClientHandleType /*handle*/,
                             locClientErrorEnumType errorId)
{
  if(errorId == eLOC_CLIENT_ERROR_SERVICE_UNAVAILABLE)
  {
    LOC_LOGE("%s:%d]: Service unavailable error\n",
                  __func__, __LINE__);

    handleEngineDownEvent();
  }
}

void LocApiV02 ::getWwanZppFix()
{
    sendMsg(new LocApiMsg([this] () {

    locClientReqUnionType req_union;
    qmiLocGetAvailWwanPositionReqMsgT_v02 zpp_req;
    memset(&zpp_req, 0, sizeof(zpp_req));

    req_union.pGetAvailWwanPositionReq = &zpp_req;

    LOC_LOGD("%s:%d]: Get ZPP Fix from available wwan position\n", __func__, __LINE__);
    locClientStatusEnumType status =
            locClientSendReq(QMI_LOC_GET_AVAILABLE_WWAN_POSITION_REQ_V02, req_union);

    if (status != eLOC_CLIENT_SUCCESS) {
        LOC_LOGe("error! status = %s\n", loc_get_v02_client_status_name(status));
    }
    }));
}

void LocApiV02 ::getBestAvailableZppFix()
{
    sendMsg(new LocApiMsg([this] () {

    locClientReqUnionType req_union;
    qmiLocGetBestAvailablePositionReqMsgT_v02 zpp_req;

    memset(&zpp_req, 0, sizeof(zpp_req));
    req_union.pGetBestAvailablePositionReq = &zpp_req;

    LOC_LOGd("Get ZPP Fix from best available source\n");

    locClientStatusEnumType status =
            locClientSendReq(QMI_LOC_GET_BEST_AVAILABLE_POSITION_REQ_V02, req_union);

    if (status != eLOC_CLIENT_SUCCESS) {
        LOC_LOGe("error! status = %s\n", loc_get_v02_client_status_name(status));
    }
    }));
}

LocationError LocApiV02 :: setGpsLockSync(GnssConfigGpsLock lock)
{
    LocationError err = LOCATION_ERROR_SUCCESS;
    qmiLocSetEngineLockReqMsgT_v02 setEngineLockReq;
    qmiLocSetEngineLockIndMsgT_v02 setEngineLockInd;
    locClientStatusEnumType status;
    locClientReqUnionType req_union;

    memset(&setEngineLockReq, 0, sizeof(setEngineLockReq));
    setEngineLockReq.lockType = convertGpsLockFromAPItoQMI((GnssConfigGpsLock)lock);;
    setEngineLockReq.subType_valid = true;
    setEngineLockReq.subType = eQMI_LOC_LOCK_ALL_SUB_V02;
    setEngineLockReq.lockClient_valid = false;
    req_union.pSetEngineLockReq = &setEngineLockReq;
    LOC_LOGd("API lock type = 0x%X QMI lockType = %d", lock, setEngineLockReq.lockType);
    memset(&setEngineLockInd, 0, sizeof(setEngineLockInd));
    status = locSyncSendReq(QMI_LOC_SET_ENGINE_LOCK_REQ_V02,
                            req_union, LOC_ENGINE_SYNC_REQUEST_LONG_TIMEOUT,
                            QMI_LOC_SET_ENGINE_LOCK_IND_V02,
                            &setEngineLockInd);
    if (eLOC_CLIENT_SUCCESS != status || eQMI_LOC_SUCCESS_V02 != setEngineLockInd.status) {
        LOC_LOGe("Set engine lock failed. status: %s, ind status:%s",
            loc_get_v02_client_status_name(status),
            loc_get_v02_qmi_status_name(setEngineLockInd.status));
        err = LOCATION_ERROR_GENERAL_FAILURE;
    } else {
        if (setEngineLockReq.lockType == eQMI_LOC_LOCK_NONE_V02 ||
                setEngineLockReq.lockType == eQMI_LOC_LOCK_MT_V02) {
            reportEngineLockStatus(eQMI_LOC_ENGINE_LOCK_STATE_ENABLED_V02);
            setEngineLockState(ENGINE_LOCK_STATE_ENABLED);
        } else {
            reportEngineLockStatus(eQMI_LOC_ENGINE_LOCK_STATE_DISABLED_V02);
            setEngineLockState(ENGINE_LOCK_STATE_DISABLED);
        }
    }
    return err;
}

void LocApiV02::requestForAidingData(GnssAidingDataSvMask svDataMask)
{
    sendMsg(new LocApiMsg([this, svDataMask] () {
        locClientEventMaskType qmiMask = 0;

        if (svDataMask & GNSS_AIDING_DATA_SV_POLY_BIT)
            qmiMask |= QMI_LOC_EVENT_MASK_GNSS_SV_POLYNOMIAL_REPORT_V02;

        if (svDataMask & GNSS_AIDING_DATA_SV_EPHEMERIS_BIT)
            qmiMask |= QMI_LOC_EVENT_MASK_EPHEMERIS_REPORT_V02;

        if (svDataMask & GNSS_AIDING_DATA_SV_IONOSPHERE_BIT)
            qmiMask |= QMI_LOC_EVENT_MASK_GNSS_EVENT_REPORT_V02;

        sendRequestForAidingData(qmiMask);
    }));
}

/*
  Returns
  Current value of GPS Lock on success
  -1 on failure
*/
int LocApiV02 :: getGpsLock(uint8_t subType)
{
    qmiLocGetEngineLockReqMsgT_v02 getEngineLockReq;
    qmiLocGetEngineLockIndMsgT_v02 getEngineLockInd;
    locClientStatusEnumType status;
    locClientReqUnionType req_union;
    int ret=0;
    memset(&getEngineLockInd, 0, sizeof(getEngineLockInd));

    //Passing req_union as a parameter even though this request has no payload
    //since NULL or 0 gives an error during compilation
    getEngineLockReq.subType_valid = true;
    getEngineLockReq.subType = (qmiLocLockSubInfoEnumT_v02)subType;
    req_union.pGetEngineLockReq = &getEngineLockReq;
    status = locSyncSendReq(QMI_LOC_GET_ENGINE_LOCK_REQ_V02,
                            req_union, LOC_ENGINE_SYNC_REQUEST_TIMEOUT,
                            QMI_LOC_GET_ENGINE_LOCK_IND_V02,
                            &getEngineLockInd);
    if(status != eLOC_CLIENT_SUCCESS || getEngineLockInd.status != eQMI_LOC_SUCCESS_V02) {
        LOC_LOGE("%s:%d]: Set engine lock failed. status: %s, ind status:%s\n",
                 __func__, __LINE__,
                 loc_get_v02_client_status_name(status),
                 loc_get_v02_qmi_status_name(getEngineLockInd.status));
        ret = -1;
    }
    else {
        if(getEngineLockInd.lockType_valid) {
            ret = (int)convertGpsLockFromQMItoAPI(getEngineLockInd.lockType);
        }
        else {
            LOC_LOGE("%s:%d]: Lock Type not valid\n", __func__, __LINE__);
            ret = -1;
        }
    }
    return ret;
}

void LocApiV02 :: getEngineLockStateSync() {

    qmiLocGetEngineLockReqMsgT_v02 getEngineLockReq;
    qmiLocGetEngineLockIndMsgT_v02 getEngineLockInd;
    locClientStatusEnumType status;
    locClientReqUnionType req_union;

    memset(&getEngineLockInd, 0, sizeof(getEngineLockInd));

    /*Passing req_union as a parameter even though this request has no payload
    since NULL or 0 gives an error during compilation*/
    getEngineLockReq.subType_valid = true;
    getEngineLockReq.subType = eQMI_LOC_LOCK_ALL_SUB_V02;
    req_union.pGetEngineLockReq = &getEngineLockReq;

    status = locSyncSendReq(QMI_LOC_GET_ENGINE_LOCK_REQ_V02,
                            req_union, LOC_ENGINE_SYNC_REQUEST_TIMEOUT,
                            QMI_LOC_GET_ENGINE_LOCK_IND_V02,
                            &getEngineLockInd);

    if (status != eLOC_CLIENT_SUCCESS || getEngineLockInd.status != eQMI_LOC_SUCCESS_V02) {
        LOC_LOGE("%s:%d]: Get engine lock failed. status: %s, ind status:%s\n",
                 __func__, __LINE__,
                 loc_get_v02_client_status_name(status),
                 loc_get_v02_qmi_status_name(getEngineLockInd.status));
    } else {
        if (getEngineLockInd.lockType == eQMI_LOC_LOCK_NONE_V02 ||
                getEngineLockInd.lockType == eQMI_LOC_LOCK_MT_V02) {
            setEngineLockState(ENGINE_LOCK_STATE_ENABLED);
        } else {
            setEngineLockState(ENGINE_LOCK_STATE_DISABLED);
        }
    }
}

LocationError
LocApiV02:: setXtraVersionCheckSync(uint32_t check)
{
    LocationError err = LOCATION_ERROR_SUCCESS;
    qmiLocSetXtraVersionCheckReqMsgT_v02 req;
    qmiLocSetXtraVersionCheckIndMsgT_v02 ind;
    locClientStatusEnumType status;
    locClientReqUnionType req_union;

    LOC_LOGD("%s:%d]: Enter. check: %u", __func__, __LINE__, check);
    memset(&req, 0, sizeof(req));
    memset(&ind, 0, sizeof(ind));
    switch (check) {
    case 0:
        req.xtraVersionCheckMode = eQMI_LOC_XTRA_VERSION_CHECK_DISABLE_V02;
        break;
    case 1:
        req.xtraVersionCheckMode = eQMI_LOC_XTRA_VERSION_CHECK_AUTO_V02;
        break;
    case 2:
        req.xtraVersionCheckMode = eQMI_LOC_XTRA_VERSION_CHECK_XTRA2_V02;
        break;
    case 3:
        req.xtraVersionCheckMode = eQMI_LOC_XTRA_VERSION_CHECK_XTRA3_V02;
        break;
    default:
        req.xtraVersionCheckMode = eQMI_LOC_XTRA_VERSION_CHECK_DISABLE_V02;
        break;
    }

    req_union.pSetXtraVersionCheckReq = &req;
    status = locSyncSendReq(QMI_LOC_SET_XTRA_VERSION_CHECK_REQ_V02,
                            req_union, LOC_ENGINE_SYNC_REQUEST_TIMEOUT,
                            QMI_LOC_SET_XTRA_VERSION_CHECK_IND_V02,
                            &ind);
    if(status != eLOC_CLIENT_SUCCESS || ind.status != eQMI_LOC_SUCCESS_V02) {
        LOC_LOGE("%s:%d]: Set xtra version check failed. status: %s, ind status:%s\n",
                 __func__, __LINE__,
                 loc_get_v02_client_status_name(status),
                 loc_get_v02_qmi_status_name(ind.status));
        err = LOCATION_ERROR_GENERAL_FAILURE;
    }

    LOC_LOGD("%s:%d]: Exit. err: %u", __func__, __LINE__, err);
    return err;
}

int LocApiV02::setSvMeasurementConstellation(const locClientEventMaskType mask)
{
    enum loc_api_adapter_err ret_val = LOC_API_ADAPTER_ERR_SUCCESS;
    qmiLocSetGNSSConstRepConfigReqMsgT_v02 setGNSSConstRepConfigReq;
    qmiLocSetGNSSConstRepConfigIndMsgT_v02 setGNSSConstRepConfigInd;
    locClientStatusEnumType status;
    locClientReqUnionType req_union;

    qmiLocGNSSConstellEnumT_v02 svConstellation = eQMI_SYSTEM_GPS_V02 |
                                                eQMI_SYSTEM_GLO_V02 |
                                                eQMI_SYSTEM_BDS_V02 |
                                                eQMI_SYSTEM_GAL_V02 |
                                                eQMI_SYSTEM_QZSS_V02 |
                                                eQMI_SYSTEM_NAVIC_V02;
    LOC_LOGd("set GNSS measurement to report constellation: %" PRIx64 " "
            "report mask = 0x%" PRIx64 "\n", svConstellation, mask);

    memset(&setGNSSConstRepConfigReq, 0, sizeof(setGNSSConstRepConfigReq));

    setGNSSConstRepConfigReq.measReportConfig_valid = true;
    if ((mask & QMI_LOC_EVENT_MASK_GNSS_MEASUREMENT_REPORT_V02) ||
        mMasterRegisterNotSupported) {
        setGNSSConstRepConfigReq.measReportConfig = svConstellation;
    }

    setGNSSConstRepConfigReq.svPolyReportConfig_valid = true;
    if (mask & QMI_LOC_EVENT_MASK_GNSS_SV_POLYNOMIAL_REPORT_V02) {
        setGNSSConstRepConfigReq.svPolyReportConfig = svConstellation;
    }
    req_union.pSetGNSSConstRepConfigReq = &setGNSSConstRepConfigReq;
    memset(&setGNSSConstRepConfigInd, 0, sizeof(setGNSSConstRepConfigInd));

    status = locSyncSendReq(QMI_LOC_SET_GNSS_CONSTELL_REPORT_CONFIG_V02,
        req_union,
        LOC_ENGINE_SYNC_REQUEST_TIMEOUT,
        QMI_LOC_SET_GNSS_CONSTELL_REPORT_CONFIG_IND_V02,
        &setGNSSConstRepConfigInd);

    if (status != eLOC_CLIENT_SUCCESS ||
        (setGNSSConstRepConfigInd.status != eQMI_LOC_SUCCESS_V02 &&
            setGNSSConstRepConfigInd.status != eQMI_LOC_ENGINE_BUSY_V02)) {
        LOC_LOGE("%s:%d]: Set GNSS constellation failed. status: %s, ind status:%s\n",
            __func__, __LINE__,
            loc_get_v02_client_status_name(status),
            loc_get_v02_qmi_status_name(setGNSSConstRepConfigInd.status));
        ret_val = LOC_API_ADAPTER_ERR_GENERAL_FAILURE;
    }
    else {
        LOC_LOGD("%s:%d]: Set GNSS constellation succeeded.\n",
            __func__, __LINE__);
    }

    return ret_val;
}

void LocApiV02::setConstrainedTuncMode(bool enabled,
                                       float tuncConstraint,
                                       uint32_t energyBudget,
                                       LocApiResponse *adapterResponse) {
    sendMsg(new LocApiMsg([this, enabled, tuncConstraint, energyBudget, adapterResponse] () {

    LocationError err = LOCATION_ERROR_SUCCESS;
    qmiLocSetConstrainedTuncModeReqMsgT_v02 req;
    qmiLocSetConstrainedTuncModeIndMsgT_v02 ind;
    locClientStatusEnumType status;
    locClientReqUnionType req_union;

    memset(&req, 0, sizeof(req));
    memset(&ind, 0, sizeof(ind));
    req.tuncConstraintOn = enabled;
    if (tuncConstraint > 0.0) {
        req.tuncConstraint_valid = true;
        req.tuncConstraint = tuncConstraint;
    }
    if (energyBudget != 0) {
        req.energyBudget_valid = true;
        req.energyBudget = energyBudget;
    }

    LOC_LOGd("Enter. enabled %d, tuncConstraint (%d, %f),"
             "energyBudget (%d, %d)", req.tuncConstraintOn,
             req.tuncConstraint_valid, req.tuncConstraint,
             req.energyBudget_valid, req.energyBudget);

    req_union.pSetConstrainedTuncModeReq = &req;
    status = locSyncSendReq(QMI_LOC_SET_CONSTRAINED_TUNC_MODE_REQ_V02,
                            req_union, LOC_ENGINE_SYNC_REQUEST_TIMEOUT,
                            QMI_LOC_SET_CONSTRAINED_TUNC_MODE_IND_V02,
                            &ind);
    if(status != eLOC_CLIENT_SUCCESS || ind.status != eQMI_LOC_SUCCESS_V02) {
        LOC_LOGe("failed, status: %s, ind status:%s\n",
                 loc_get_v02_client_status_name(status),
                 loc_get_v02_qmi_status_name(ind.status));
        err = LOCATION_ERROR_GENERAL_FAILURE;
    }

    if (adapterResponse) {
        adapterResponse->returnToSender(err);
    }
    LOC_LOGv("Exit. err: %u", err);
  }));
}

void LocApiV02::setPositionAssistedClockEstimatorMode
        (bool enabled, LocApiResponse *adapterResponse) {

    sendMsg(new LocApiMsg([this, enabled, adapterResponse] () {

    LocationError err = LOCATION_ERROR_SUCCESS;
    qmiLocEnablePositionAssistedClockEstReqMsgT_v02 req;
    qmiLocEnablePositionAssistedClockEstIndMsgT_v02 ind;
    locClientStatusEnumType status;
    locClientReqUnionType req_union;

    LOC_LOGd("Enter. enabled %d", enabled);
    memset(&req, 0, sizeof(req));
    memset(&ind, 0, sizeof(ind));
    req.enablePositionAssistedClockEst = enabled;

    req_union.pSetEnablePositionAssistedClockEstReq = &req;
    status = locSyncSendReq(QMI_LOC_ENABLE_POSITION_ASSISTED_CLOCK_EST_REQ_V02,
                            req_union, LOC_ENGINE_SYNC_REQUEST_TIMEOUT,
                            QMI_LOC_ENABLE_POSITION_ASSISTED_CLOCK_EST_IND_V02,
                            &ind);
    if(status != eLOC_CLIENT_SUCCESS || ind.status != eQMI_LOC_SUCCESS_V02) {
        LOC_LOGe("failed. status: %s, ind status:%s\n",
                 loc_get_v02_client_status_name(status),
                 loc_get_v02_qmi_status_name(ind.status));
        err = LOCATION_ERROR_GENERAL_FAILURE;
    }
    if (adapterResponse) {
        adapterResponse->returnToSender(err);
    }
    LOC_LOGv("Exit. err: %u", err);
    }));
}

void LocApiV02::getGnssEnergyConsumed() {
    sendMsg(new LocApiMsg([this] {

    LocationError err = LOCATION_ERROR_SUCCESS;

    qmiLocQueryGNSSEnergyConsumedReqMsgT_v02 req;
    qmiLocQueryGNSSEnergyConsumedIndMsgT_v02 ind;
    locClientStatusEnumType status;
    locClientReqUnionType req_union;

    LOC_LOGd("Enter. ");
    memset(&req, 0, sizeof(req));
    memset(&ind, 0, sizeof(ind));

    req_union.pQueryGNSSEnergyConsumedReq = &req;
    status = locSyncSendReq(QMI_LOC_QUERY_GNSS_ENERGY_CONSUMED_REQ_V02,
                            req_union, LOC_ENGINE_SYNC_REQUEST_TIMEOUT,
                            QMI_LOC_QUERY_GNSS_ENERGY_CONSUMED_IND_V02,
                            &ind);

    if(status != eLOC_CLIENT_SUCCESS) {
        LOC_LOGe("failed. status: %s\n", loc_get_v02_client_status_name(status));
        // report invalid read to indicate error (based on QMI message)
        LocApiBase::reportGnssEngEnergyConsumedEvent(0xffffffffffffffff);
        err = LOCATION_ERROR_GENERAL_FAILURE;
    } else {
        LocApiBase::reportGnssEngEnergyConsumedEvent(ind.energyConsumedSinceFirstBoot);
    }

    LOC_LOGd("Exit. err: %u", err);
    }));
}

void LocApiV02 :: updateSystemPowerState(PowerStateType powerState){
    sendMsg(new LocApiMsg([this, powerState] () {

    LocationError err = LOCATION_ERROR_SUCCESS;
    qmiLocInjectPlatformPowerStateReqMsgT_v02 req;
    qmiLocInjectPlatformPowerStateIndMsgT_v02 ind;

    locClientStatusEnumType status;
    locClientReqUnionType req_union;

    LOC_LOGd("Enter. power state %d", powerState);
    qmiLocPlatformPowerStateEnumT_v02 qmiPowerState = eQMI_LOC_POWER_STATE_UNKNOWN_V02;
    switch (powerState) {
    case POWER_STATE_SUSPEND:
        qmiPowerState = eQMI_LOC_POWER_STATE_SUSPENDED_V02;
        break;
    case POWER_STATE_RESUME:
        qmiPowerState = eQMI_LOC_POWER_STATE_RESUME_V02;
        break;
    case POWER_STATE_SHUTDOWN:
        qmiPowerState = eQMI_LOC_POWER_STATE_SHUTDOWN_V02;
        break;
    default:
        break;
    }

    // unknown power state will not be injected to modem
    if (eQMI_LOC_POWER_STATE_UNKNOWN_V02 != qmiPowerState) {
        memset(&req, 0, sizeof(req));
        memset(&ind, 0, sizeof(ind));
        req.powerState = qmiPowerState;
        req_union.pInjectPowerStateReq = &req;

        status = locSyncSendReq(QMI_LOC_INJECT_PLATFORM_POWER_STATE_REQ_V02,
                                req_union, LOC_ENGINE_SYNC_REQUEST_TIMEOUT,
                                QMI_LOC_INJECT_PLATFORM_POWER_STATE_IND_V02,
                                &ind);
        if (status != eLOC_CLIENT_SUCCESS || ind.status != eQMI_LOC_SUCCESS_V02) {
            LOC_LOGe("failed. status: %s, ind status:%s\n",
                     loc_get_v02_client_status_name(status),
                     loc_get_v02_qmi_status_name(ind.status));
            err = LOCATION_ERROR_GENERAL_FAILURE;
        }
    }

    LOC_LOGd("Exit. err: %u", err);
    }));
}

void LocApiV02::reportLatencyInfo(const qmiLocLatencyInformationIndMsgT_v02* pLocLatencyInfo)
{
    GnssLatencyInfo gnssLatencyInfo = {};

    if (nullptr == pLocLatencyInfo) {
        LOC_LOGe("pLocLatencyInfo is nullptr");
        return;
    }

    if (eQMI_LOC_LATENCY_INFO_TYPE_MEASUREMENT_V02 != pLocLatencyInfo->latencyInfo) {
        LOC_LOGe("Invalid Latency Info Type");
        return;
    }

    /* check refCount */
    if (pLocLatencyInfo->fCountOfMeasBlk != mRefFCount) {
        LOC_LOGw("FCount mismatch: Latency Fcount=%d Meas. FCount=%d",
                 pLocLatencyInfo->fCountOfMeasBlk,
                 mRefFCount);
        return;
    }

    if (pLocLatencyInfo->sysTickAtChkPt1_valid) {
        gnssLatencyInfo.meQtimer1 = pLocLatencyInfo->sysTickAtChkPt1;
    }
    if (pLocLatencyInfo->sysTickAtChkPt2_valid) {
        gnssLatencyInfo.meQtimer2 = pLocLatencyInfo->sysTickAtChkPt2;
    }
    if (pLocLatencyInfo->sysTickAtChkPt3_valid) {
        gnssLatencyInfo.meQtimer3 = pLocLatencyInfo->sysTickAtChkPt3;
    }
    if (pLocLatencyInfo->sysTickAtChkPt4_valid) {
        gnssLatencyInfo.peQtimer1 = pLocLatencyInfo->sysTickAtChkPt4;
    }
    if (pLocLatencyInfo->sysTickAtChkPt5_valid) {
        gnssLatencyInfo.peQtimer2 = pLocLatencyInfo->sysTickAtChkPt5;
    }
    if (pLocLatencyInfo->sysTickAtChkPt6_valid) {
        gnssLatencyInfo.peQtimer3 = pLocLatencyInfo->sysTickAtChkPt6;
    }
    if (pLocLatencyInfo->sysTickAtChkPt7_valid) {
        gnssLatencyInfo.smQtimer1 = pLocLatencyInfo->sysTickAtChkPt7;
    }
    if (pLocLatencyInfo->sysTickAtChkPt8_valid) {
        gnssLatencyInfo.smQtimer2 = pLocLatencyInfo->sysTickAtChkPt8;
    }
    if (pLocLatencyInfo->sysTickAtChkPt9_valid) {
        gnssLatencyInfo.smQtimer3 = pLocLatencyInfo->sysTickAtChkPt9;
    }
    if (pLocLatencyInfo->sysTickAtChkPt10_valid) {
        gnssLatencyInfo.locMwQtimer = pLocLatencyInfo->sysTickAtChkPt10;
    }
    gnssLatencyInfo.hlosQtimer1 = mHlosQtimer1;
    gnssLatencyInfo.hlosQtimer2 = mHlosQtimer2;
    LOC_LOGv("meQtimer1=%" PRIi64 " "
             "meQtimer2=%" PRIi64 " "
             "meQtimer3=%" PRIi64 " "
             "peQtimer1=%" PRIi64 " "
             "peQtimer2=%" PRIi64 " "
             "peQtimer3=%" PRIi64 " "
             "smQtimer1=%" PRIi64 " "
             "smQtimer2=%" PRIi64 " "
             "smQtimer3=%" PRIi64 " "
             "locMwQtimer=%" PRIi64 " "
             "hlosQtimer1=%" PRIi64 " "
             "hlosQtimer2=%" PRIi64 " ",
             gnssLatencyInfo.meQtimer1, gnssLatencyInfo.meQtimer2,
             gnssLatencyInfo.meQtimer3, gnssLatencyInfo.peQtimer1, gnssLatencyInfo.peQtimer2,
             gnssLatencyInfo.peQtimer3, gnssLatencyInfo.smQtimer1, gnssLatencyInfo.smQtimer2,
             gnssLatencyInfo.smQtimer3, gnssLatencyInfo.locMwQtimer,
             gnssLatencyInfo.hlosQtimer1, gnssLatencyInfo.hlosQtimer2);

    LocApiBase::reportLatencyInfo(gnssLatencyInfo);
}

void LocApiV02::reportEngineLockStatus(const qmiLocEngineLockStateEnumT_v02 engineLockState)
{
    EngineLockState lockState = convertEngineLockState(engineLockState);
    LOC_LOGd("Lock State old %d new %d", getEngineLockState(), lockState);

    if (lockState != getEngineLockState() && ENGINE_LOCK_STATE_INVALID != lockState ) {
        setEngineLockState(lockState);
        LocApiBase::reportEngineLockStatus(lockState);
    }
}

void LocApiV02::configRobustLocation
        (bool enable, bool enableForE911, LocApiResponse *adapterResponse) {

    sendMsg(new LocApiMsg([this, enable, enableForE911, adapterResponse] () {

    LocationError err = LOCATION_ERROR_SUCCESS;
    qmiLocSetRobustLocationReqMsgT_v02 req;
    qmiLocGenReqStatusIndMsgT_v02 ind;
    locClientStatusEnumType status;
    locClientReqUnionType req_union;

    LOC_LOGd("Enter. enabled %d, enableForE911 %d", enable, enableForE911);
    memset(&req, 0, sizeof(req));
    memset(&ind, 0, sizeof(ind));
    req.enable = enable;
    req.enableForE911_valid = true;
    req.enableForE911 = enableForE911;
    if (enable == false && enableForE911 == true) {
        LOC_LOGw("enableForE911 is not allowed when enable is set to false");
        // change enableForE911 to false to simplify processing
        req.enableForE911 = false;
    }

    req_union.pSetRobustLocationReq = &req;
    status = locSyncSendReq(QMI_LOC_SET_ROBUST_LOCATION_CONFIG_REQ_V02,
                            req_union, LOC_ENGINE_SYNC_REQUEST_LONG_TIMEOUT,
                            QMI_LOC_SET_ROBUST_LOCATION_CONFIG_IND_V02,
                            &ind);
    if (status != eLOC_CLIENT_SUCCESS || ind.status != eQMI_LOC_SUCCESS_V02) {
        LOC_LOGe("failed. status: %s, ind status:%s\n",
                 loc_get_v02_client_status_name(status),
                 loc_get_v02_qmi_status_name(ind.status));
        if (status == eLOC_CLIENT_FAILURE_UNSUPPORTED ||
                status == eLOC_CLIENT_FAILURE_INVALID_MESSAGE_ID) {
            err = LOCATION_ERROR_NOT_SUPPORTED;
        } else {
            err = LOCATION_ERROR_GENERAL_FAILURE;
        }
    }
    if (adapterResponse) {
        adapterResponse->returnToSender(err);
    }
    LOC_LOGv("Exit. err: %u", err);
    }));
}

void LocApiV02 :: getRobustLocationConfig(uint32_t sessionId, LocApiResponse *adapterResponse)
{
    sendMsg(new LocApiMsg([this, sessionId, adapterResponse] () {

    int ret=0;
    LocationError err = LOCATION_ERROR_SUCCESS;
    locClientStatusEnumType status = eLOC_CLIENT_FAILURE_GENERAL;
    locClientReqUnionType req_union = {};
    qmiLocGetRobustLocationConfigIndMsgT_v02 getRobustLocationConfigInd = {};

    status = locSyncSendReq(QMI_LOC_GET_ROBUST_LOCATION_CONFIG_REQ_V02,
                            req_union, LOC_ENGINE_SYNC_REQUEST_LONG_TIMEOUT,
                            QMI_LOC_GET_ROBUST_LOCATION_CONFIG_IND_V02,
                            &getRobustLocationConfigInd);

    if ((status == eLOC_CLIENT_SUCCESS) &&
        (getRobustLocationConfigInd.status == eQMI_LOC_SUCCESS_V02)) {
        err = LOCATION_ERROR_SUCCESS;
    }else {
        LOC_LOGe("getRobustLocationConfig: failed. status: %s, ind status:%s",
                 loc_get_v02_client_status_name(status),
                 loc_get_v02_qmi_status_name(getRobustLocationConfigInd.status));
        if (status == eLOC_CLIENT_FAILURE_UNSUPPORTED ||
                status == eLOC_CLIENT_FAILURE_INVALID_MESSAGE_ID) {
            err = LOCATION_ERROR_NOT_SUPPORTED;
        } else {
            err = LOCATION_ERROR_GENERAL_FAILURE;
        }
    }

    if (LOCATION_ERROR_SUCCESS != err) {
        adapterResponse->returnToSender(err);
    } else {
        GnssConfig config = {};
        uint32_t robustLocationValidMask = 0;
        config.flags |= GNSS_CONFIG_FLAGS_ROBUST_LOCATION_BIT;
        if (getRobustLocationConfigInd.isEnabled_valid) {
            robustLocationValidMask |= GNSS_CONFIG_ROBUST_LOCATION_ENABLED_VALID_BIT;
        }
        if (getRobustLocationConfigInd.isEnabledForE911_valid) {
            robustLocationValidMask |= GNSS_CONFIG_ROBUST_LOCATION_ENABLED_FOR_E911_VALID_BIT;
        }
        if (getRobustLocationConfigInd.robustLocationVersion_valid) {
            robustLocationValidMask |= GNSS_CONFIG_ROBUST_LOCATION_VERSION_VALID_BIT ;
        }

        config.robustLocationConfig.validMask =
                (GnssConfigRobustLocationValidMask) robustLocationValidMask;
        config.robustLocationConfig.enabled = getRobustLocationConfigInd.isEnabled;
        config.robustLocationConfig.enabledForE911 = getRobustLocationConfigInd.isEnabledForE911;
        config.robustLocationConfig.version.major =
                getRobustLocationConfigInd.robustLocationVersion.major;
        config.robustLocationConfig.version.minor =
                getRobustLocationConfigInd.robustLocationVersion.minor;

        LOC_LOGd("session id: %d, mask: 0x%x, enabled: %d, enabledForE911: %d, version: %d %d",
                 sessionId, config.robustLocationConfig.validMask,
                 config.robustLocationConfig.enabled,
                 config.robustLocationConfig.enabledForE911,
                 config.robustLocationConfig.version.major,
                 config.robustLocationConfig.version.minor);

        LocApiBase::reportGnssConfig(sessionId, config);
    }

    LOC_LOGv("Exit. err: %u", err);
    }));
}

void LocApiV02::configMinGpsWeek(uint16_t minGpsWeek, LocApiResponse *adapterResponse) {

    sendMsg(new LocApiMsg([this, minGpsWeek, adapterResponse] () {

    LOC_LOGd("Enter. minGpsWeek %d", minGpsWeek);

    LocationError err = LOCATION_ERROR_SUCCESS;
    qmiLocSetMinGpsWeekNumberReqMsgT_v02 req = {};
    qmiLocGenReqStatusIndMsgT_v02 ind = {};
    locClientStatusEnumType status;
    locClientReqUnionType req_union = {};

    req.minGpsWeekNumber = minGpsWeek;
    req_union.pSetMinGpsWeekReq = &req;

    status = locSyncSendReq(QMI_LOC_SET_MIN_GPS_WEEK_NUMBER_REQ_V02,
                            req_union, LOC_ENGINE_SYNC_REQUEST_LONG_TIMEOUT,
                            QMI_LOC_SET_MIN_GPS_WEEK_NUMBER_IND_V02,
                            &ind);
    if (status != eLOC_CLIENT_SUCCESS || ind.status != eQMI_LOC_SUCCESS_V02) {
        LOC_LOGe("failed. status: %s, ind status:%s",
                 loc_get_v02_client_status_name(status),
                 loc_get_v02_qmi_status_name(ind.status));
        if (status == eLOC_CLIENT_FAILURE_UNSUPPORTED ||
                status == eLOC_CLIENT_FAILURE_INVALID_MESSAGE_ID) {
            err = LOCATION_ERROR_NOT_SUPPORTED;
        } else {
            err = LOCATION_ERROR_GENERAL_FAILURE;
        }
    }
    if (adapterResponse) {
        adapterResponse->returnToSender(err);
    }
    LOC_LOGv("Exit. err: %u", err);
    }));
}

void LocApiV02 :: getMinGpsWeek(uint32_t sessionId, LocApiResponse *adapterResponse)
{
    sendMsg(new LocApiMsg([this, sessionId, adapterResponse] () {

    LocationError err = LOCATION_ERROR_SUCCESS;
    locClientStatusEnumType status = eLOC_CLIENT_FAILURE_GENERAL;
    locClientReqUnionType req_union = {};
    qmiLocGetMinGpsWeekNumberIndMsgT_v02 getInd = {};
    uint16_t minGpsWeek = 0;

    status = locSyncSendReq(QMI_LOC_GET_MIN_GPS_WEEK_NUMBER_REQ_V02,
                            req_union, LOC_ENGINE_SYNC_REQUEST_LONG_TIMEOUT,
                            QMI_LOC_GET_MIN_GPS_WEEK_NUMBER_IND_V02,
                            &getInd);

    if ((status == eLOC_CLIENT_SUCCESS) && (getInd.status == eQMI_LOC_SUCCESS_V02) &&
            getInd.minGpsWeekNumber_valid) {
        minGpsWeek = getInd.minGpsWeekNumber;
        err = LOCATION_ERROR_SUCCESS;
    }else {
        LOC_LOGe("failed. status: %s, ind status:%s",
                 loc_get_v02_client_status_name(status),
                 loc_get_v02_qmi_status_name(getInd.status));
        if (status == eLOC_CLIENT_FAILURE_UNSUPPORTED ||
                status == eLOC_CLIENT_FAILURE_INVALID_MESSAGE_ID) {
            err = LOCATION_ERROR_NOT_SUPPORTED;
        } else {
            err = LOCATION_ERROR_GENERAL_FAILURE;
        }
    }

    if (LOCATION_ERROR_SUCCESS != err) {
        adapterResponse->returnToSender(err);
    } else {
        GnssConfig config = {};
        config.flags |= GNSS_CONFIG_FLAGS_MIN_GPS_WEEK_BIT;
        config.minGpsWeek = minGpsWeek;
        LOC_LOGd("session id %d, minGpsWeek %d", sessionId, minGpsWeek);
        LocApiBase::reportGnssConfig(sessionId, config);
    }

    LOC_LOGe("Exit. err: %u", err);
    }));
}

LocationError LocApiV02::setParameterSync(const GnssConfig & gnssConfig) {
    LocationError err = LOCATION_ERROR_NOT_SUPPORTED;
    qmiLocSetParameterReqMsgT_v02 req = {};
    qmiLocGenReqStatusIndMsgT_v02 ind = {};
    locClientStatusEnumType status = eLOC_CLIENT_FAILURE_GENERAL;
    locClientReqUnionType req_union = {};

    req.paramType = eQMI_LOC_PARAMETER_TYPE_RESERVED_V02;
    if (gnssConfig.flags & GNSS_CONFIG_FLAGS_MIN_SV_ELEVATION_BIT) {
        req.paramType = eQMI_LOC_PARAMETER_TYPE_MINIMUM_SV_ELEVATION_V02;
        req.minSvElevation_valid = 1;
        req.minSvElevation = gnssConfig.minSvElevation;
        LOC_LOGd("set min sv elevation to %d", req.minSvElevation);
    }

    if (req.paramType != eQMI_LOC_PARAMETER_TYPE_RESERVED_V02) {
        req_union.pSetParameterReq = &req;
        status = locSyncSendReq(QMI_LOC_SET_PARAMETER_REQ_V02,
                                req_union, LOC_ENGINE_SYNC_REQUEST_LONG_TIMEOUT,
                                QMI_LOC_SET_PARAMETER_IND_V02,
                                &ind);
        if (status != eLOC_CLIENT_SUCCESS || ind.status != eQMI_LOC_SUCCESS_V02) {
            LOC_LOGe("failed. status: %s, ind status:%s\n",
                     loc_get_v02_client_status_name(status),
                     loc_get_v02_qmi_status_name(ind.status));
            if (status == eLOC_CLIENT_FAILURE_UNSUPPORTED ||
                    status == eLOC_CLIENT_FAILURE_INVALID_MESSAGE_ID) {
                err = LOCATION_ERROR_NOT_SUPPORTED;
            } else {
                err = LOCATION_ERROR_GENERAL_FAILURE;
            }
        } else {
            err = LOCATION_ERROR_SUCCESS;
        }
    }

    LOC_LOGv("Exit. err: %u", err);
    return err;
}

void LocApiV02 :: getParameter(uint32_t sessionId, GnssConfigFlagsMask flags,
                               LocApiResponse* adapterResponse) {

    LOC_LOGe ("get parameter 0x%x", flags);
    sendMsg(new LocApiMsg([this, sessionId, flags, adapterResponse] () {

    LocationError err = LOCATION_ERROR_GENERAL_FAILURE;
    locClientStatusEnumType status = eLOC_CLIENT_FAILURE_GENERAL;
    locClientReqUnionType req_union = {};
    qmiLocGetParameterReqMsgT_v02 getParameterReq = {};
    qmiLocGetParameterIndMsgT_v02 getParameterInd = {};
    GnssConfig config = {};

    do {
        getParameterReq.paramType = eQMI_LOC_PARAMETER_TYPE_RESERVED_V02;
        switch (flags) {
        case GNSS_CONFIG_FLAGS_MIN_SV_ELEVATION_BIT:
            getParameterReq.paramType = eQMI_LOC_PARAMETER_TYPE_MINIMUM_SV_ELEVATION_V02;
            break;
        default:
            break;
        }

        if (getParameterReq.paramType == eQMI_LOC_PARAMETER_TYPE_RESERVED_V02) {
            err = LOCATION_ERROR_NOT_SUPPORTED;
            break;
        }

        req_union.pGetParameterReq = &getParameterReq;
        status = locSyncSendReq(QMI_LOC_GET_PARAMETER_REQ_V02,
                                req_union, LOC_ENGINE_SYNC_REQUEST_LONG_TIMEOUT,
                                QMI_LOC_GET_PARAMETER_IND_V02,
                                &getParameterInd);

        if (!((status == eLOC_CLIENT_SUCCESS) &&
              (getParameterInd.status == eQMI_LOC_SUCCESS_V02))) {
            LOC_LOGe("getParameterConfig: failed. status: %s, ind status:%s",
                     loc_get_v02_client_status_name(status),
                     loc_get_v02_qmi_status_name(getParameterInd.status));
            if (status == eLOC_CLIENT_FAILURE_UNSUPPORTED ||
                    status == eLOC_CLIENT_FAILURE_INVALID_MESSAGE_ID) {
                err = LOCATION_ERROR_NOT_SUPPORTED;
            } else {
                err = LOCATION_ERROR_GENERAL_FAILURE;
            }
            break;
        }

        switch (flags) {
        case GNSS_CONFIG_FLAGS_MIN_SV_ELEVATION_BIT:
            if ((getParameterInd.paramType == eQMI_LOC_PARAMETER_TYPE_MINIMUM_SV_ELEVATION_V02) &&
                    (getParameterInd.minSvElevation_valid == 1)) {
                config.flags = GNSS_CONFIG_FLAGS_MIN_SV_ELEVATION_BIT;
                config.minSvElevation = getParameterInd.minSvElevation;
                err = LOCATION_ERROR_SUCCESS;
            }
            break;
        default:
            break;
        }
    } while (0);

    if (config.flags) {
        LocApiBase::reportGnssConfig(sessionId, config);
    } else {
        // return to the caller on failure
        if (adapterResponse) {
            adapterResponse->returnToSender(err);
        }
    }
    LOC_LOGv("Exit. err: %u", err);

    }));
}

bool LocApiV02 :: cacheGnssMeasurementSupport()
{
    bool gnssMeasurementSupported = false;

    /*for GNSS Measurement service, use
      QMI_LOC_SET_GNSS_CONSTELL_REPORT_CONFIG_V02
      to check if modem support this feature or not*/

    if (LOC_API_ADAPTER_ERR_SUCCESS ==
        setSvMeasurementConstellation(QMI_LOC_EVENT_MASK_GNSS_MEASUREMENT_REPORT_V02)) {
        gnssMeasurementSupported = true;
    }

    LOC_LOGd("gnssMeasurementSupported is %d\n", gnssMeasurementSupported);

    return gnssMeasurementSupported;
}

locClientStatusEnumType LocApiV02::locSyncSendReq(uint32_t req_id,
        locClientReqUnionType req_payload, uint32_t timeout_msec,
        uint32_t ind_id, void* ind_payload_ptr) {
    locClientStatusEnumType status = loc_sync_send_req(clientHandle, req_id, req_payload,
            timeout_msec, ind_id, ind_payload_ptr);
    if (eLOC_CLIENT_FAILURE_ENGINE_BUSY == status ||
            (eLOC_CLIENT_SUCCESS == status && nullptr != ind_payload_ptr &&
            eQMI_LOC_ENGINE_BUSY_V02 == *((qmiLocStatusEnumT_v02*)ind_payload_ptr))) {
        if (mResenders.empty() && ((mQmiMask & QMI_LOC_EVENT_MASK_ENGINE_STATE_V02) == 0)) {
            locClientRegisterEventMask(clientHandle,
                                       mQmiMask | QMI_LOC_EVENT_MASK_ENGINE_STATE_V02, isMaster());
        }
        LOC_LOGd("Engine busy, cache req: %d", req_id);
        uint32_t reqLen = 0;
        void* pReqData = nullptr;
        locClientReqUnionType req_payload_copy = {nullptr};
        validateRequest(req_id, req_payload, &pReqData, &reqLen);
        if (nullptr != pReqData) {
            req_payload_copy.pReqData = malloc(reqLen);
            if (nullptr != req_payload_copy.pReqData) {
                memcpy(req_payload_copy.pReqData, pReqData, reqLen);
            }
        }
        // something would be wrong if (nullptr != pReqData && nullptr == req_payload_copy)
        if (nullptr == pReqData || nullptr != req_payload_copy.pReqData) {
            mResenders.push_back([=](){
                    // ignore indicator, we use nullptr as the last parameter
                    loc_sync_send_req(clientHandle, req_id, req_payload_copy,
                                      timeout_msec, ind_id, nullptr);
                    if (nullptr != req_payload_copy.pReqData) {
                        free(req_payload_copy.pReqData);
                    }
                });
        }
    }
    return status;
}

void LocApiV02 ::
handleWwanZppFixIndication(const qmiLocGetAvailWwanPositionIndMsgT_v02& zpp_ind)
{
    LocGpsLocation zppLoc;
    memset(&zppLoc, 0, sizeof(zppLoc));

    LOC_LOGD("Got Wwan Zpp fix location validity (lat:%d, lon:%d, timestamp:%d accuracy:%d)\n "
             "(%.7f, %.7f), timestamp %" PRIu64 ", accuracy %f",
             zpp_ind.latitude_valid,
             zpp_ind.longitude_valid,
             zpp_ind.timestampUtc_valid,
             zpp_ind.horUncCircular_valid,
             zpp_ind.latitude,
             zpp_ind.longitude,
             zpp_ind.timestampUtc,
             zpp_ind.horUncCircular);

    if ((zpp_ind.latitude_valid == false) ||
        (zpp_ind.longitude_valid == false) ||
        (zpp_ind.horUncCircular_valid == false)) {
        LOC_LOGE(" Location not valid lat=%u lon=%u unc=%u",
                 zpp_ind.latitude_valid,
                 zpp_ind.longitude_valid,
                 zpp_ind.horUncCircular_valid);
    } else {

        zppLoc.size = sizeof(LocGpsLocation);
        if (zpp_ind.timestampUtc_valid) {
            zppLoc.timestamp = zpp_ind.timestampUtc;
        } else {
            /* The UTC time from modem is not valid.
            In this case, we use current system time instead.*/

            struct timespec time_info_current;
            clock_gettime(CLOCK_REALTIME,&time_info_current);
            zppLoc.timestamp = (time_info_current.tv_sec)*1e3 +
                               (time_info_current.tv_nsec)/1e6;
            LOC_LOGD("zpp timestamp got from system: %" PRIu64, zppLoc.timestamp);
        }

        zppLoc.flags = LOC_GPS_LOCATION_HAS_LAT_LONG | LOC_GPS_LOCATION_HAS_ACCURACY;
        zppLoc.latitude = zpp_ind.latitude;
        zppLoc.longitude = zpp_ind.longitude;
        zppLoc.accuracy = zpp_ind.horUncCircular;

        // If horCircularConfidence_valid is true, and horCircularConfidence value
        // is less than 68%, then scale the accuracy value to 68% confidence.
        if (zpp_ind.horCircularConfidence_valid)
        {
            scaleAccuracyTo68PercentConfidence(zpp_ind.horCircularConfidence,
                                               zppLoc, true);
        }

        if (zpp_ind.altitudeWrtEllipsoid_valid) {
            zppLoc.flags |= LOC_GPS_LOCATION_HAS_ALTITUDE;
            zppLoc.altitude = zpp_ind.altitudeWrtEllipsoid;
        }

        if (zpp_ind.vertUnc_valid) {
            zppLoc.flags |= LOC_GPS_LOCATION_HAS_VERT_UNCERTAINITY;
            zppLoc.vertUncertainity = zpp_ind.vertUnc;
        }
    }

    LocApiBase::reportWwanZppFix(zppLoc);
}

void LocApiV02::
    handleZppBestAvailableFixIndication(const qmiLocGetBestAvailablePositionIndMsgT_v02 &zpp_ind)
{
    LocGpsLocation zppLoc;
    GpsLocationExtended location_extended;
    LocPosTechMask tech_mask;

    memset(&zppLoc, 0, sizeof(zppLoc));
    zppLoc.size = sizeof(zppLoc);

    memset(&location_extended, 0, sizeof(location_extended));
    location_extended.size = sizeof(location_extended);

    tech_mask = LOC_POS_TECH_MASK_DEFAULT;

    LOC_LOGD("Got Zpp fix location validity (lat:%d, lon:%d, timestamp:%d accuracy:%d)"
            " (%.7f, %.7f), timestamp %" PRIu64 ", accuracy %f",
            zpp_ind.latitude_valid,
            zpp_ind.longitude_valid,
            zpp_ind.timestampUtc_valid,
            zpp_ind.horUncCircular_valid,
            zpp_ind.latitude,
            zpp_ind.longitude,
            zpp_ind.timestampUtc,
            zpp_ind.horUncCircular);

        if (zpp_ind.timestampUtc_valid) {
            zppLoc.timestamp = zpp_ind.timestampUtc;
        } else {
            /* The UTC time from modem is not valid.
                    In this case, we use current system time instead.*/

          struct timespec time_info_current;
          clock_gettime(CLOCK_REALTIME,&time_info_current);
          zppLoc.timestamp = (time_info_current.tv_sec)*1e3 +
                  (time_info_current.tv_nsec)/1e6;
          LOC_LOGD("zpp timestamp got from system: %" PRIu64, zppLoc.timestamp);
        }

        if (zpp_ind.latitude_valid && zpp_ind.longitude_valid &&
                zpp_ind.horUncCircular_valid ) {
            zppLoc.flags = LOC_GPS_LOCATION_HAS_LAT_LONG | LOC_GPS_LOCATION_HAS_ACCURACY;
            zppLoc.latitude = zpp_ind.latitude;
            zppLoc.longitude = zpp_ind.longitude;
            zppLoc.accuracy = zpp_ind.horUncCircular;

            // If horCircularConfidence_valid is true, and horCircularConfidence value
            // is less than 68%, then scale the accuracy value to 68% confidence.
            if (zpp_ind.horCircularConfidence_valid)
            {
                scaleAccuracyTo68PercentConfidence(zpp_ind.horCircularConfidence,
                                                   zppLoc, true);
            }

            if (zpp_ind.altitudeWrtEllipsoid_valid) {
                zppLoc.flags |= LOC_GPS_LOCATION_HAS_ALTITUDE;
                zppLoc.altitude = zpp_ind.altitudeWrtEllipsoid;
            }

            if (zpp_ind.horSpeed_valid) {
                zppLoc.flags |= LOC_GPS_LOCATION_HAS_SPEED;
                zppLoc.speed = zpp_ind.horSpeed;
            }

            if (zpp_ind.heading_valid) {
                zppLoc.flags |= LOC_GPS_LOCATION_HAS_BEARING;
                zppLoc.bearing = zpp_ind.heading;
            }

            if (zpp_ind.vertUnc_valid) {
                location_extended.flags |= GPS_LOCATION_EXTENDED_HAS_VERT_UNC;
                location_extended.vert_unc = zpp_ind.vertUnc;
            }

            if (zpp_ind.horSpeedUnc_valid) {
                location_extended.flags |= GPS_LOCATION_EXTENDED_HAS_SPEED_UNC;
                location_extended.speed_unc = zpp_ind.horSpeedUnc;
            }

            if (zpp_ind.headingUnc_valid) {
                location_extended.flags |= GPS_LOCATION_EXTENDED_HAS_BEARING_UNC;
                location_extended.bearing_unc = zpp_ind.headingUnc;
            }

            if (zpp_ind.technologyMask_valid) {
                tech_mask = zpp_ind.technologyMask;
            }

            if(zpp_ind.spoofReportMask_valid) {
                zppLoc.flags |= LOC_GPS_LOCATION_HAS_SPOOF_MASK;
                zppLoc.spoof_mask = (uint32_t)zpp_ind.spoofReportMask;
                LOC_LOGD("%s:%d QMI_spoofReportMask:0x%x", __func__, __LINE__,
                             (uint8_t)zppLoc.spoof_mask);
            }
        }

        LocApiBase::reportZppBestAvailableFix(zppLoc, location_extended, tech_mask);
}


LocPosTechMask LocApiV02 :: convertPosTechMask(
  qmiLocPosTechMaskT_v02 mask)
{
   LocPosTechMask locTechMask = LOC_POS_TECH_MASK_DEFAULT;

   if (mask & QMI_LOC_POS_TECH_MASK_SATELLITE_V02)
      locTechMask |= LOC_POS_TECH_MASK_SATELLITE;

   if (mask & QMI_LOC_POS_TECH_MASK_CELLID_V02)
      locTechMask |= LOC_POS_TECH_MASK_CELLID;

   if (mask & QMI_LOC_POS_TECH_MASK_WIFI_V02)
      locTechMask |= LOC_POS_TECH_MASK_WIFI;

   if (mask & QMI_LOC_POS_TECH_MASK_SENSORS_V02)
      locTechMask |= LOC_POS_TECH_MASK_SENSORS;

   if (mask & QMI_LOC_POS_TECH_MASK_REFERENCE_LOCATION_V02)
      locTechMask |= LOC_POS_TECH_MASK_REFERENCE_LOCATION;

   if (mask & QMI_LOC_POS_TECH_MASK_INJECTED_COARSE_POSITION_V02)
      locTechMask |= LOC_POS_TECH_MASK_INJECTED_COARSE_POSITION;

   if (mask & QMI_LOC_POS_TECH_MASK_AFLT_V02)
      locTechMask |= LOC_POS_TECH_MASK_AFLT;

   if (mask & QMI_LOC_POS_TECH_MASK_HYBRID_V02)
      locTechMask |= LOC_POS_TECH_MASK_HYBRID;

   return locTechMask;
}

LocNavSolutionMask LocApiV02 :: convertNavSolutionMask(
  qmiLocNavSolutionMaskT_v02 mask)
{
   LocNavSolutionMask locNavMask = 0;

   if (mask & QMI_LOC_NAV_MASK_SBAS_CORRECTION_IONO_V02)
      locNavMask |= LOC_NAV_MASK_SBAS_CORRECTION_IONO;

   if (mask & QMI_LOC_NAV_MASK_SBAS_CORRECTION_FAST_V02)
      locNavMask |= LOC_NAV_MASK_SBAS_CORRECTION_FAST;

   if (mask & QMI_LOC_POS_TECH_MASK_WIFI_V02)
      locNavMask |= LOC_POS_TECH_MASK_WIFI;

   if (mask & QMI_LOC_NAV_MASK_SBAS_CORRECTION_LONG_V02)
      locNavMask |= LOC_NAV_MASK_SBAS_CORRECTION_LONG;

   if (mask & QMI_LOC_NAV_MASK_SBAS_INTEGRITY_V02)
      locNavMask |= LOC_NAV_MASK_SBAS_INTEGRITY;

   if (mask & QMI_LOC_NAV_MASK_CORRECTION_DGNSS_V02)
      locNavMask |= LOC_NAV_MASK_DGNSS_CORRECTION;

   if (mask & QMI_LOC_NAV_MASK_ONLY_SBAS_CORRECTED_SV_USED_V02)
      locNavMask |= LOC_NAV_MASK_ONLY_SBAS_CORRECTED_SV_USED;

   return locNavMask;
}

qmiLocApnTypeMaskT_v02 LocApiV02::convertLocApnTypeMask(LocApnTypeMask mask)
{
    qmiLocApnTypeMaskT_v02 qmiMask = 0;

    if (mask & LOC_APN_TYPE_MASK_DEFAULT) {
        qmiMask |= QMI_LOC_APN_TYPE_MASK_DEFAULT_V02;
    }
    if (mask & LOC_APN_TYPE_MASK_IMS) {
        qmiMask |= QMI_LOC_APN_TYPE_MASK_IMS_V02;
    }
    if (mask & LOC_APN_TYPE_MASK_MMS) {
        qmiMask |= QMI_LOC_APN_TYPE_MASK_MMS_V02;
    }
    if (mask & LOC_APN_TYPE_MASK_DUN) {
        qmiMask |= QMI_LOC_APN_TYPE_MASK_DUN_V02;
    }
    if (mask & LOC_APN_TYPE_MASK_SUPL) {
        qmiMask |= QMI_LOC_APN_TYPE_MASK_SUPL_V02;
    }
    if (mask & LOC_APN_TYPE_MASK_HIPRI) {
        qmiMask |= QMI_LOC_APN_TYPE_MASK_HIPRI_V02;
    }
    if (mask & LOC_APN_TYPE_MASK_FOTA) {
        qmiMask |= QMI_LOC_APN_TYPE_MASK_FOTA_V02;
    }
    if (mask & LOC_APN_TYPE_MASK_CBS) {
        qmiMask |= QMI_LOC_APN_TYPE_MASK_CBS_V02;
    }
    if (mask & LOC_APN_TYPE_MASK_IA) {
        qmiMask |= QMI_LOC_APN_TYPE_MASK_IA_V02;
    }
    if (mask & LOC_APN_TYPE_MASK_EMERGENCY) {
        qmiMask |= QMI_LOC_APN_TYPE_MASK_EMERGENCY_V02;
    }

    return qmiMask;
}

LocApnTypeMask LocApiV02::convertQmiLocApnTypeMask(qmiLocApnTypeMaskT_v02 qmiMask)
{
    LocApnTypeMask mask = 0;

    if (qmiMask & QMI_LOC_APN_TYPE_MASK_DEFAULT_V02) {
        mask |= LOC_APN_TYPE_MASK_DEFAULT;
    }
    if (qmiMask & QMI_LOC_APN_TYPE_MASK_IMS_V02) {
        mask |= LOC_APN_TYPE_MASK_IMS;
    }
    if (qmiMask & QMI_LOC_APN_TYPE_MASK_MMS_V02) {
        mask |= LOC_APN_TYPE_MASK_MMS;
    }
    if (qmiMask & QMI_LOC_APN_TYPE_MASK_DUN_V02) {
        mask |= LOC_APN_TYPE_MASK_DUN;
    }
    if (qmiMask & QMI_LOC_APN_TYPE_MASK_SUPL_V02) {
        mask |= LOC_APN_TYPE_MASK_SUPL;
    }
    if (qmiMask & QMI_LOC_APN_TYPE_MASK_HIPRI_V02) {
        mask |= LOC_APN_TYPE_MASK_HIPRI;
    }
    if (qmiMask & QMI_LOC_APN_TYPE_MASK_FOTA_V02) {
        mask |= LOC_APN_TYPE_MASK_FOTA;
    }
    if (qmiMask & QMI_LOC_APN_TYPE_MASK_CBS_V02) {
        mask |= LOC_APN_TYPE_MASK_CBS;
    }
    if (qmiMask & QMI_LOC_APN_TYPE_MASK_IA_V02) {
        mask |= LOC_APN_TYPE_MASK_IA;
    }
    if (qmiMask & QMI_LOC_APN_TYPE_MASK_EMERGENCY_V02) {
        mask |= LOC_APN_TYPE_MASK_EMERGENCY;
    }

    return mask;
}

GnssConfigSuplVersion
LocApiV02::convertSuplVersion(const uint32_t suplVersion)
{
    switch (suplVersion) {
        case 0x00020004:
            return GNSS_CONFIG_SUPL_VERSION_2_0_4;
        case 0x00020002:
            return GNSS_CONFIG_SUPL_VERSION_2_0_2;
        case 0x00020000:
            return GNSS_CONFIG_SUPL_VERSION_2_0_0;
        case 0x00010000:
        default:
            return GNSS_CONFIG_SUPL_VERSION_1_0_0;
    }
}

GnssConfigLppeControlPlaneMask
LocApiV02::convertLppeCp(const uint32_t lppeControlPlaneMask)
{
    GnssConfigLppeControlPlaneMask mask = 0;
    if ((1<<0) & lppeControlPlaneMask) {
        mask |= GNSS_CONFIG_LPPE_CONTROL_PLANE_DBH_BIT;
    }
    if ((1<<1) & lppeControlPlaneMask) {
        mask |= GNSS_CONFIG_LPPE_CONTROL_PLANE_WLAN_AP_MEASUREMENTS_BIT;
    }
    if ((1<<2) & lppeControlPlaneMask) {
        mask |= GNSS_CONFIG_LPPE_CONTROL_PLANE_SRN_AP_MEASUREMENTS_BIT;
    }
    if ((1<<3) & lppeControlPlaneMask) {
        mask |= GNSS_CONFIG_LPPE_CONTROL_PLANE_SENSOR_BARO_MEASUREMENTS_BIT;
    }
    return mask;
}

GnssConfigLppeUserPlaneMask
LocApiV02::convertLppeUp(const uint32_t lppeUserPlaneMask)
{
    GnssConfigLppeUserPlaneMask mask = 0;
    if ((1 << 0) & lppeUserPlaneMask) {
        mask |= GNSS_CONFIG_LPPE_USER_PLANE_DBH_BIT;
    }
    if ((1 << 1) & lppeUserPlaneMask) {
        mask |= GNSS_CONFIG_LPPE_USER_PLANE_WLAN_AP_MEASUREMENTS_BIT;
    }
    if ((1 << 2) & lppeUserPlaneMask) {
        mask |= GNSS_CONFIG_LPPE_USER_PLANE_SRN_AP_MEASUREMENTS_BIT;
    }
    if ((1 << 3) & lppeUserPlaneMask) {
        mask |= GNSS_CONFIG_LPPE_USER_PLANE_SENSOR_BARO_MEASUREMENTS_BIT;
    }
    return mask;
}

LocationError
LocApiV02::setEmergencyExtensionWindowSync(const uint32_t emergencyExtensionSeconds)
{
    LocationError err = LOCATION_ERROR_SUCCESS;
    locClientStatusEnumType result = eLOC_CLIENT_SUCCESS;
    locClientReqUnionType req_union;
    qmiLocSetProtocolConfigParametersReqMsgT_v02 eCbW_req;
    qmiLocSetProtocolConfigParametersIndMsgT_v02 eCbW_ind;

    memset(&eCbW_req, 0, sizeof(eCbW_req));
    memset(&eCbW_ind, 0, sizeof(eCbW_ind));

    eCbW_req.emergencyCallbackWindow_valid = 1;
    eCbW_req.emergencyCallbackWindow = emergencyExtensionSeconds;

    req_union.pSetProtocolConfigParametersReq = &eCbW_req;

    LOC_LOGd("emergencyCallbackWindow = %d", emergencyExtensionSeconds);

    result = locSyncSendReq(QMI_LOC_SET_PROTOCOL_CONFIG_PARAMETERS_REQ_V02,
        req_union, LOC_ENGINE_SYNC_REQUEST_TIMEOUT,
        QMI_LOC_SET_PROTOCOL_CONFIG_PARAMETERS_IND_V02,
        &eCbW_ind);

    if (result != eLOC_CLIENT_SUCCESS ||
        eQMI_LOC_SUCCESS_V02 != eCbW_ind.status)
    {
        LOC_LOGe("Error status = %s, ind..status = %s ",
            loc_get_v02_client_status_name(result),
            loc_get_v02_qmi_status_name(eCbW_ind.status));
        err = LOCATION_ERROR_GENERAL_FAILURE;
    }

    return err;
}

void
LocApiV02::setMeasurementCorrections(const GnssMeasurementCorrections& gnssMeasurementCorrections)
{
    sendMsg(new LocApiMsg([this, gnssMeasurementCorrections] {

    LocationError err = LOCATION_ERROR_SUCCESS;
    locClientStatusEnumType result = eLOC_CLIENT_SUCCESS;
    locClientReqUnionType req_union = {};

    qmiLocEventInjectEnvAidingReqMsgT_v02 setEnvAidingReqMsg;
    qmiLocGenReqStatusIndMsgT_v02 genReqStatusIndMsg;

    memset(&setEnvAidingReqMsg, 0, sizeof(setEnvAidingReqMsg));
    memset(&genReqStatusIndMsg, 0, sizeof(genReqStatusIndMsg));

    LOC_LOGv("GnssMeasurementCorrections from modem:\n"
             " hasEnvironmentBearing = %d"
             " environmentBearingDegrees = %.2f"
             " environmentBearingUncertaintyDegrees = %.2f"
             " latitudeDegrees = %.2f"
             " longitudeDegrees = %.2f"
             " horizontalPositionUncertaintyMeters = %.2f"
             " altitudeMeters = %.2f"
             " verticalPositionUncertaintyMeters = %.2f"
             " toaGpsNanosecondsOfWeek = %" PRIu64,
             gnssMeasurementCorrections.hasEnvironmentBearing,
             gnssMeasurementCorrections.environmentBearingDegrees,
             gnssMeasurementCorrections.environmentBearingUncertaintyDegrees,
             gnssMeasurementCorrections.latitudeDegrees,
             gnssMeasurementCorrections.longitudeDegrees,
             gnssMeasurementCorrections.horizontalPositionUncertaintyMeters,
             gnssMeasurementCorrections.altitudeMeters,
             gnssMeasurementCorrections.verticalPositionUncertaintyMeters,
             gnssMeasurementCorrections.toaGpsNanosecondsOfWeek);

    for (int i = 0; i < gnssMeasurementCorrections.satCorrections.size(); i++) {
        LOC_LOGv("gnssMeasurementCorrections.satCorrections:\n"
            "satCorrections[%d].svType = %d "
            "satCorrections[%d].svId = %d "
            "satCorrections[%d].carrierFrequencyHz = %.2f "
            "satCorrections[%d].probSatIsLos = %.2f "
            "satCorrections[%d].excessPathLengthMeters = %.2f "
            "satCorrections[%d].excessPathLengthUncertaintyMeters = %.2f "
            "satCorrections[%d].reflectingPlane.latitudeDegree = %.2f "
            "satCorrections[%d].reflectingPlane.longitudeDegrees = %.2f "
            "satCorrections[%d].reflectingPlane.altitudeMeters = %.2f "
            "satCorrections[%d].reflectingPlane.azimuthDegrees = %.2f ",
            i, gnssMeasurementCorrections.satCorrections[i].svType,
            i, gnssMeasurementCorrections.satCorrections[i].svId,
            i, gnssMeasurementCorrections.satCorrections[i].carrierFrequencyHz,
            i, gnssMeasurementCorrections.satCorrections[i].probSatIsLos,
            i, gnssMeasurementCorrections.satCorrections[i].excessPathLengthMeters,
            i, gnssMeasurementCorrections.satCorrections[i].excessPathLengthUncertaintyMeters,
            i, gnssMeasurementCorrections.satCorrections[i].reflectingPlane.latitudeDegrees,
            i, gnssMeasurementCorrections.satCorrections[i].reflectingPlane.longitudeDegrees,
            i, gnssMeasurementCorrections.satCorrections[i].reflectingPlane.altitudeMeters,
            i, gnssMeasurementCorrections.satCorrections[i].reflectingPlane.azimuthDegrees);
    }

    setEnvAidingReqMsg.maxMessageNum = 1;
    setEnvAidingReqMsg.seqNum = 1;

    setEnvAidingReqMsg.envBearingValidity_valid = true;
    setEnvAidingReqMsg.envBearingValidity = gnssMeasurementCorrections.hasEnvironmentBearing;
    setEnvAidingReqMsg.envBearingDegrees_valid = true;
    setEnvAidingReqMsg.envBearingDegrees = gnssMeasurementCorrections.environmentBearingDegrees;
    setEnvAidingReqMsg.envBearingUncDegrees_valid = true;
    setEnvAidingReqMsg.envBearingUncDegrees =
            gnssMeasurementCorrections.environmentBearingUncertaintyDegrees;

    setEnvAidingReqMsg.latitudeDegrees_valid = true;
    setEnvAidingReqMsg.latitudeDegrees = gnssMeasurementCorrections.latitudeDegrees;
    setEnvAidingReqMsg.longitudeDegrees_valid = true;
    setEnvAidingReqMsg.longitudeDegrees = gnssMeasurementCorrections.longitudeDegrees;
    setEnvAidingReqMsg.horizontalPositionUncMeters_valid = true;
    setEnvAidingReqMsg.horizontalPositionUncMeters =
            gnssMeasurementCorrections.horizontalPositionUncertaintyMeters;
    setEnvAidingReqMsg.altitudeMeters_valid = true;
    setEnvAidingReqMsg.altitudeMeters = gnssMeasurementCorrections.altitudeMeters;
    setEnvAidingReqMsg.altitudeUncMeters_valid = true;
    setEnvAidingReqMsg.altitudeUncMeters =
            gnssMeasurementCorrections.verticalPositionUncertaintyMeters;
    setEnvAidingReqMsg.toaGpsNanosecondsOfWeek_valid = true;
    setEnvAidingReqMsg.toaGpsNanosecondsOfWeek =
            gnssMeasurementCorrections.toaGpsNanosecondsOfWeek;

    uint32_t len = gnssMeasurementCorrections.satCorrections.size();
    if (len > 0) {
        if (len >= QMI_LOC_ENV_AIDING_CORRECTION_MAX_SV_USED_V02) {
            setEnvAidingReqMsg.svCorrection_len = QMI_LOC_ENV_AIDING_CORRECTION_MAX_SV_USED_V02;
        } else {
            setEnvAidingReqMsg.svCorrection_len = len;
        }
        setEnvAidingReqMsg.svCorrection_valid = true;
        for (int i = 0; i < setEnvAidingReqMsg.svCorrection_len; i++) {
            setEnvAidingReqMsg.svCorrection[i].svCorrectionFlags = 0;
            if (gnssMeasurementCorrections.satCorrections[i].flags &
                (GNSS_MEAS_CORR_HAS_SAT_IS_LOS_PROBABILITY_BIT)) {
                setEnvAidingReqMsg.svCorrection[i].probabilitySvIsLineofSight =
                        gnssMeasurementCorrections.satCorrections[i].probSatIsLos;
                setEnvAidingReqMsg.svCorrection[i].svCorrectionFlags |=
                        QMI_LOC_ENV_AIDING_SV_CORRECTION_LINE_OF_SIGHT_PROBABILITY_VALID_V02;
            }
            if (gnssMeasurementCorrections.satCorrections[i].flags &
                (GNSS_MEAS_CORR_HAS_EXCESS_PATH_LENGTH_BIT)) {
                setEnvAidingReqMsg.svCorrection[i].excessPathLengthMeters =
                        gnssMeasurementCorrections.satCorrections[i].excessPathLengthMeters;
                setEnvAidingReqMsg.svCorrection[i].svCorrectionFlags |=
                        QMI_LOC_ENV_AIDING_SV_CORRECTION_EXCESS_PATH_LENGTH_VALID_V02;
            }
            if (gnssMeasurementCorrections.satCorrections[i].flags &
                (GNSS_MEAS_CORR_HAS_EXCESS_PATH_LENGTH_UNC_BIT)) {
                setEnvAidingReqMsg.svCorrection[i].excessPathLengthUncMeters =
                        gnssMeasurementCorrections.satCorrections[i].
                                excessPathLengthUncertaintyMeters;
                setEnvAidingReqMsg.svCorrection[i].svCorrectionFlags |=
                        QMI_LOC_ENV_AIDING_SV_CORRECTION_EXCESS_PATH_LENGTH_UNC_VALID_V02;
            }
            if (gnssMeasurementCorrections.satCorrections[i].flags &
                (GNSS_MEAS_CORR_HAS_REFLECTING_PLANE_BIT)) {
                setEnvAidingReqMsg.svCorrection[i].reflectingPlane.latitudeDegrees =
                        gnssMeasurementCorrections.satCorrections[i].
                                reflectingPlane.latitudeDegrees;
                setEnvAidingReqMsg.svCorrection[i].reflectingPlane.longitudeDegrees =
                        gnssMeasurementCorrections.satCorrections[i].
                                reflectingPlane.longitudeDegrees;
                setEnvAidingReqMsg.svCorrection[i].reflectingPlane.altitudeMeters =
                        gnssMeasurementCorrections.satCorrections[i].
                                reflectingPlane.altitudeMeters;
                setEnvAidingReqMsg.svCorrection[i].reflectingPlane.azimuthDegrees =
                        gnssMeasurementCorrections.satCorrections[i].
                                reflectingPlane.azimuthDegrees;
                setEnvAidingReqMsg.svCorrection[i].svCorrectionFlags |=
                        QMI_LOC_ENV_AIDING_SV_CORRECTION_REFLECTING_PLANE_VALID_V02;
            }
            switch (gnssMeasurementCorrections.satCorrections[i].svType) {
            case GNSS_SV_TYPE_GPS:
                setEnvAidingReqMsg.svCorrection[i].constellation =
                        eQMI_LOC_SV_SYSTEM_GPS_V02;
                break;

            case GNSS_SV_TYPE_SBAS:
                setEnvAidingReqMsg.svCorrection[i].constellation =
                        eQMI_LOC_SV_SYSTEM_SBAS_V02;
                break;

            case GNSS_SV_TYPE_GLONASS:
                setEnvAidingReqMsg.svCorrection[i].constellation =
                        eQMI_LOC_SV_SYSTEM_GLONASS_V02;
                break;

            case GNSS_SV_TYPE_QZSS:
                setEnvAidingReqMsg.svCorrection[i].constellation =
                        eQMI_LOC_SV_SYSTEM_QZSS_V02;
                break;

            case GNSS_SV_TYPE_BEIDOU:
                setEnvAidingReqMsg.svCorrection[i].constellation =
                        eQMI_LOC_SV_SYSTEM_BDS_V02;
                break;

            case GNSS_SV_TYPE_GALILEO:
                setEnvAidingReqMsg.svCorrection[i].constellation =
                        eQMI_LOC_SV_SYSTEM_GALILEO_V02;
                break;

            case GNSS_SV_TYPE_NAVIC:
                setEnvAidingReqMsg.svCorrection[i].constellation =
                        eQMI_LOC_SV_SYSTEM_NAVIC_V02;
                break;

            case GNSS_SV_TYPE_UNKNOWN:
            default:
                break;
            }
            setEnvAidingReqMsg.svCorrection[i].svid =
                    gnssMeasurementCorrections.satCorrections[i].svId;
            setEnvAidingReqMsg.svCorrection[i].carrierFrequencyHz =
                    gnssMeasurementCorrections.satCorrections[i].carrierFrequencyHz;
        }
    }

    req_union.pEnvAidingReqMsg = &setEnvAidingReqMsg;

    result = locSyncSendReq(QMI_LOC_INJECT_ENV_AIDING_REQ_V02,
                            req_union, LOC_ENGINE_SYNC_REQUEST_TIMEOUT,
                            QMI_LOC_INJECT_ENV_AIDING_IND_V02,
                            &genReqStatusIndMsg);

    if (result != eLOC_CLIENT_SUCCESS ||
        eQMI_LOC_SUCCESS_V02 != genReqStatusIndMsg.status)
    {
        LOC_LOGe("Error status = %s, ind..status = %s ",
            loc_get_v02_client_status_name(result),
            loc_get_v02_qmi_status_name(genReqStatusIndMsg.status));
        err = LOCATION_ERROR_GENERAL_FAILURE;
    }

    }));
}

LocationError
LocApiV02::setBlacklistSvSync(const GnssSvIdConfig& config)
{
    locClientStatusEnumType status = eLOC_CLIENT_FAILURE_GENERAL;
    locClientReqUnionType req_union = {};

    qmiLocSetBlacklistSvReqMsgT_v02 setBlacklistSvMsg;
    qmiLocGenReqStatusIndMsgT_v02 genReqStatusIndMsg;

    // Clear all fields
    memset(&setBlacklistSvMsg, 0, sizeof(setBlacklistSvMsg));
    memset(&genReqStatusIndMsg, 0, sizeof(genReqStatusIndMsg));

    // Fill in the request details
    setBlacklistSvMsg.glo_persist_blacklist_sv_valid = true;
    setBlacklistSvMsg.glo_persist_blacklist_sv = config.gloBlacklistSvMask;
    setBlacklistSvMsg.glo_clear_persist_blacklist_sv_valid = true;
    setBlacklistSvMsg.glo_clear_persist_blacklist_sv = ~config.gloBlacklistSvMask;

    setBlacklistSvMsg.bds_persist_blacklist_sv_valid = true;
    setBlacklistSvMsg.bds_persist_blacklist_sv = config.bdsBlacklistSvMask;
    setBlacklistSvMsg.bds_clear_persist_blacklist_sv_valid = true;
    setBlacklistSvMsg.bds_clear_persist_blacklist_sv = ~config.bdsBlacklistSvMask;

    setBlacklistSvMsg.qzss_persist_blacklist_sv_valid = true;
    setBlacklistSvMsg.qzss_persist_blacklist_sv = config.qzssBlacklistSvMask;
    setBlacklistSvMsg.qzss_clear_persist_blacklist_sv_valid = true;
    setBlacklistSvMsg.qzss_clear_persist_blacklist_sv = ~config.qzssBlacklistSvMask;

    setBlacklistSvMsg.gal_persist_blacklist_sv_valid = true;
    setBlacklistSvMsg.gal_persist_blacklist_sv = config.galBlacklistSvMask;
    setBlacklistSvMsg.gal_clear_persist_blacklist_sv_valid = true;
    setBlacklistSvMsg.gal_clear_persist_blacklist_sv = ~config.galBlacklistSvMask;

    setBlacklistSvMsg.sbas_persist_blacklist_sv_valid = true;
    setBlacklistSvMsg.sbas_persist_blacklist_sv = config.sbasBlacklistSvMask,
    setBlacklistSvMsg.sbas_clear_persist_blacklist_sv_valid = true;
    setBlacklistSvMsg.sbas_clear_persist_blacklist_sv = ~config.sbasBlacklistSvMask;

    setBlacklistSvMsg.navic_persist_blacklist_sv_valid = true;
    setBlacklistSvMsg.navic_persist_blacklist_sv = config.navicBlacklistSvMask,
    setBlacklistSvMsg.navic_clear_persist_blacklist_sv_valid = true;
    setBlacklistSvMsg.navic_clear_persist_blacklist_sv = ~config.navicBlacklistSvMask;

    LOC_LOGd(">>> configConstellations, "
             "glo blacklist mask =0x%" PRIx64 ", "
             "qzss blacklist mask =0x%" PRIx64 ",\n"
             "bds blacklist mask =0x%" PRIx64 ", "
             "gal blacklist mask =0x%" PRIx64 ",\n"
             "sbas blacklist mask =0x%" PRIx64 ", "
             "navic blacklist mask =0x%" PRIx64 ", ",
             setBlacklistSvMsg.glo_persist_blacklist_sv,
             setBlacklistSvMsg.qzss_persist_blacklist_sv,
             setBlacklistSvMsg.bds_persist_blacklist_sv,
             setBlacklistSvMsg.gal_persist_blacklist_sv,
             setBlacklistSvMsg.sbas_persist_blacklist_sv,
             setBlacklistSvMsg.navic_persist_blacklist_sv);
    // Update in request union
    req_union.pSetBlacklistSvReq = &setBlacklistSvMsg;

    // Send the request
    status = loc_sync_send_req(clientHandle,
                               QMI_LOC_SET_BLACKLIST_SV_REQ_V02,
                               req_union,
                               LOC_ENGINE_SYNC_REQUEST_TIMEOUT,
                               QMI_LOC_SET_BLACKLIST_SV_IND_V02,
                               &genReqStatusIndMsg);
    if(status != eLOC_CLIENT_SUCCESS ||
            genReqStatusIndMsg.status != eQMI_LOC_SUCCESS_V02) {
        LOC_LOGe("Set Blacklist SV failed. status: %s ind status %s",
                 loc_get_v02_client_status_name(status),
                 loc_get_v02_qmi_status_name(genReqStatusIndMsg.status));
        return LOCATION_ERROR_GENERAL_FAILURE;
    }

    return LOCATION_ERROR_SUCCESS;
}

void
LocApiV02::setBlacklistSv(const GnssSvIdConfig& config, LocApiResponse* adapterResponse)
{
    sendMsg(new LocApiMsg([this, config, adapterResponse] () {
                LocationError err = setBlacklistSvSync(config);
                if (adapterResponse) {
                    adapterResponse->returnToSender(err);
                }
            }));
}

void LocApiV02::getBlacklistSv()
{
    sendMsg(new LocApiMsg([this] () {

    locClientStatusEnumType status = eLOC_CLIENT_FAILURE_GENERAL;
    locClientReqUnionType req_union = {};

    // Nothing to update in request union

    // Send the request
    status = locClientSendReq(QMI_LOC_GET_BLACKLIST_SV_REQ_V02, req_union);
    if(status != eLOC_CLIENT_SUCCESS) {
        LOC_LOGe("Get Blacklist SV failed. status: %s",
                 loc_get_v02_client_status_name(status));
    }

    }));
}

void
LocApiV02::setConstellationControl(const GnssSvTypeConfig& config,
                                   LocApiResponse *adapterResponse)
{
    // QMI will return INVALID parameter if enabledSvTypesMask is 0,
    // so we just return back to the caller as this is no-op
    if (0 == config.enabledSvTypesMask) {
        if (NULL != adapterResponse) {
            adapterResponse->returnToSender(LOCATION_ERROR_SUCCESS);
        }
        return;
    }

    sendMsg(new LocApiMsg([this, config, adapterResponse] () {

    locClientStatusEnumType status = eLOC_CLIENT_FAILURE_GENERAL;
    locClientReqUnionType req_union = {};

    qmiLocSetConstellationConfigReqMsgT_v02 setConstellationConfigMsg;
    qmiLocGenReqStatusIndMsgT_v02 genReqStatusIndMsg;

    // Clear all fields
    memset (&setConstellationConfigMsg, 0, sizeof(setConstellationConfigMsg));
    memset(&genReqStatusIndMsg, 0, sizeof(genReqStatusIndMsg));

    // Fill in the request details
    setConstellationConfigMsg.resetConstellations = false;

    setConstellationConfigMsg.enableMask_valid = true;
    setConstellationConfigMsg.enableMask = config.enabledSvTypesMask;

    // disableMask is not supported in modem
    // if we set disableMask, QMI call will return error
    LOC_LOGe("enable: %d 0x%" PRIx64 ", blacklisted: %d 0x%" PRIx64 "",
             setConstellationConfigMsg.enableMask_valid,
             setConstellationConfigMsg.enableMask,
             setConstellationConfigMsg.disableMask_valid,
             setConstellationConfigMsg.disableMask);

    // Update in request union
    req_union.pSetConstellationConfigReq = &setConstellationConfigMsg;

    // Send the request
    status = loc_sync_send_req(clientHandle,
                               QMI_LOC_SET_CONSTELLATION_CONTROL_REQ_V02,
                               req_union,
                               LOC_ENGINE_SYNC_REQUEST_TIMEOUT,
                               QMI_LOC_SET_CONSTELLATION_CONTROL_IND_V02,
                               &genReqStatusIndMsg);
    if(status != eLOC_CLIENT_SUCCESS ||
            genReqStatusIndMsg.status != eQMI_LOC_SUCCESS_V02) {
        LOC_LOGe("Set Constellation Config failed. status: %s ind status %s",
                 loc_get_v02_client_status_name(status),
                 loc_get_v02_qmi_status_name(genReqStatusIndMsg.status));
    }

    LocationError err = LOCATION_ERROR_GENERAL_FAILURE;
    if (eLOC_CLIENT_SUCCESS == status) {
        err = LOCATION_ERROR_SUCCESS;
    }

    if (adapterResponse != NULL) {
        adapterResponse->returnToSender(err);
    }

    }));
}

void
LocApiV02::getConstellationControl()
{
    sendMsg(new LocApiMsg([this] () {

    locClientStatusEnumType status = eLOC_CLIENT_FAILURE_GENERAL;
    locClientReqUnionType req_union = {};

    qmiLocGetConstellationConfigIndMsgT_v02 getConstIndMsg;
    memset(&getConstIndMsg, 0, sizeof(getConstIndMsg));

    // Nothing to update in request union

    // Send the request
    status = loc_sync_send_req(clientHandle,
                               QMI_LOC_GET_CONSTELLATION_CONTROL_REQ_V02,
                               req_union,
                               LOC_ENGINE_SYNC_REQUEST_TIMEOUT,
                               QMI_LOC_GET_CONSTELLATION_CONTROL_IND_V02,
                               &getConstIndMsg);
    if(status == eLOC_CLIENT_SUCCESS &&
            getConstIndMsg.status == eQMI_LOC_SUCCESS_V02) {
        LOC_LOGd("GET constellation Ind");
        reportGnssSvTypeConfig(getConstIndMsg);
    } else {
        LOC_LOGe("Get Constellation failed. status: %s, ind status: %s",
                 loc_get_v02_client_status_name(status),
                 loc_get_v02_qmi_status_name(getConstIndMsg.status));
    }

    }));
}

void
LocApiV02::resetConstellationControl(LocApiResponse *adapterResponse)
{
    sendMsg(new LocApiMsg([this, adapterResponse] () {

    locClientStatusEnumType status = eLOC_CLIENT_FAILURE_GENERAL;
    locClientReqUnionType req_union = {};

    qmiLocSetConstellationConfigReqMsgT_v02 setConstellationConfigMsg;
    qmiLocGenReqStatusIndMsgT_v02 genReqStatusIndMsg;

    // Clear all fields
    memset (&setConstellationConfigMsg, 0, sizeof(setConstellationConfigMsg));
    memset(&genReqStatusIndMsg, 0, sizeof(genReqStatusIndMsg));

    // Fill in the request details
    setConstellationConfigMsg.resetConstellations = true;

    // Update in request union
    req_union.pSetConstellationConfigReq = &setConstellationConfigMsg;

    // Send the request
    status = loc_sync_send_req(clientHandle,
                               QMI_LOC_SET_CONSTELLATION_CONTROL_REQ_V02,
                               req_union,
                               LOC_ENGINE_SYNC_REQUEST_TIMEOUT,
                               QMI_LOC_SET_CONSTELLATION_CONTROL_IND_V02,
                               &genReqStatusIndMsg);
    if(status != eLOC_CLIENT_SUCCESS ||
            genReqStatusIndMsg.status != eQMI_LOC_SUCCESS_V02) {
        LOC_LOGe("Reset Constellation Config failed. "
                 "status: %s ind status %s",
                 loc_get_v02_client_status_name(status),
                 loc_get_v02_qmi_status_name(genReqStatusIndMsg.status));
    }

    LocationError err = LOCATION_ERROR_GENERAL_FAILURE;
    if (eLOC_CLIENT_SUCCESS == status) {
        err = LOCATION_ERROR_SUCCESS;
    }
    if (adapterResponse) {
        adapterResponse->returnToSender(err);
    }
    }));
}

void
LocApiV02::reportGnssSvIdConfig(
        const qmiLocGetBlacklistSvIndMsgT_v02& ind)
{
    // Validate status
    if (ind.status != eQMI_LOC_SUCCESS_V02) {
        LOC_LOGe("Ind failure status %d", ind.status);
        return;
    }

    // Parse all fields
    GnssSvIdConfig config = {};
    convertQmiBlacklistedSvConfigToGnssConfig(ind, config);
    // Pass on GnssSvConfig
    LocApiBase::reportGnssSvIdConfig(config);
}

void
LocApiV02::reportGnssSvTypeConfig(
        const qmiLocGetConstellationConfigIndMsgT_v02& ind)
{
    // Validate status
    if (ind.status != eQMI_LOC_SUCCESS_V02) {
        LOC_LOGe("Ind failure status %d", ind.status);
        return;
    }

    // Parse all fields
    GnssSvTypeConfig config = {};
    config.size = sizeof(GnssSvTypeConfig);
    convertToGnssSvTypeConfig(ind, config);

    // Pass on GnssSvConfig
    LocApiBase::reportGnssSvTypeConfig(config);
}

void
LocApiV02::convertToGnssSvTypeConfig(
        const qmiLocGetConstellationConfigIndMsgT_v02& ind,
        GnssSvTypeConfig& config)
{
    // Enabled Mask
    if (ind.bds_status_valid &&
            (ind.bds_status == eQMI_LOC_CONSTELLATION_ENABLED_MANDATORY_V02 ||
                    ind.bds_status == eQMI_LOC_CONSTELLATION_ENABLED_INTERNALLY_V02 ||
                    ind.bds_status == eQMI_LOC_CONSTELLATION_ENABLED_BY_CLIENT_V02)) {
        config.enabledSvTypesMask |= GNSS_SV_TYPES_MASK_BDS_BIT;
    }
    if (ind.glonass_status_valid &&
            (ind.glonass_status == eQMI_LOC_CONSTELLATION_ENABLED_MANDATORY_V02 ||
                    ind.glonass_status == eQMI_LOC_CONSTELLATION_ENABLED_INTERNALLY_V02 ||
                    ind.glonass_status == eQMI_LOC_CONSTELLATION_ENABLED_BY_CLIENT_V02)) {
        config.enabledSvTypesMask |= GNSS_SV_TYPES_MASK_GLO_BIT;
    }
    if (ind.galileo_status_valid &&
            (ind.galileo_status == eQMI_LOC_CONSTELLATION_ENABLED_MANDATORY_V02 ||
                    ind.galileo_status == eQMI_LOC_CONSTELLATION_ENABLED_INTERNALLY_V02 ||
                    ind.galileo_status == eQMI_LOC_CONSTELLATION_ENABLED_BY_CLIENT_V02)) {
        config.enabledSvTypesMask |= GNSS_SV_TYPES_MASK_GAL_BIT;
    }
    if (ind.qzss_status_valid &&
            (ind.qzss_status == eQMI_LOC_CONSTELLATION_ENABLED_MANDATORY_V02 ||
                    ind.qzss_status == eQMI_LOC_CONSTELLATION_ENABLED_INTERNALLY_V02 ||
                    ind.qzss_status == eQMI_LOC_CONSTELLATION_ENABLED_BY_CLIENT_V02)) {
        config.enabledSvTypesMask |= GNSS_SV_TYPES_MASK_QZSS_BIT;
    }
    if (ind.navic_status_valid &&
            (ind.navic_status == eQMI_LOC_CONSTELLATION_ENABLED_MANDATORY_V02 ||
                    ind.navic_status == eQMI_LOC_CONSTELLATION_ENABLED_INTERNALLY_V02 ||
                    ind.navic_status == eQMI_LOC_CONSTELLATION_ENABLED_BY_CLIENT_V02)) {
        config.enabledSvTypesMask |= GNSS_SV_TYPES_MASK_NAVIC_BIT;
    }

    // Disabled Mask
    if (ind.bds_status_valid &&
            (ind.bds_status == eQMI_LOC_CONSTELLATION_DISABLED_NOT_SUPPORTED_V02 ||
                    ind.bds_status == eQMI_LOC_CONSTELLATION_DISABLED_INTERNALLY_V02 ||
                    ind.bds_status == eQMI_LOC_CONSTELLATION_DISABLED_BY_CLIENT_V02 ||
                    ind.bds_status == eQMI_LOC_CONSTELLATION_DISABLED_NO_MEMORY_V02)) {
        config.blacklistedSvTypesMask |= GNSS_SV_TYPES_MASK_BDS_BIT;
    }
    if (ind.glonass_status_valid &&
            (ind.glonass_status == eQMI_LOC_CONSTELLATION_DISABLED_NOT_SUPPORTED_V02 ||
                    ind.glonass_status == eQMI_LOC_CONSTELLATION_DISABLED_INTERNALLY_V02 ||
                    ind.glonass_status == eQMI_LOC_CONSTELLATION_DISABLED_BY_CLIENT_V02 ||
                    ind.glonass_status == eQMI_LOC_CONSTELLATION_DISABLED_NO_MEMORY_V02)) {
        config.blacklistedSvTypesMask |= GNSS_SV_TYPES_MASK_GLO_BIT;
    }
    if (ind.galileo_status_valid &&
            (ind.galileo_status == eQMI_LOC_CONSTELLATION_DISABLED_NOT_SUPPORTED_V02 ||
                    ind.galileo_status == eQMI_LOC_CONSTELLATION_DISABLED_INTERNALLY_V02 ||
                    ind.galileo_status == eQMI_LOC_CONSTELLATION_DISABLED_BY_CLIENT_V02 ||
                    ind.galileo_status == eQMI_LOC_CONSTELLATION_DISABLED_NO_MEMORY_V02)) {
        config.blacklistedSvTypesMask |= GNSS_SV_TYPES_MASK_GAL_BIT;
    }
    if (ind.qzss_status_valid &&
            (ind.qzss_status == eQMI_LOC_CONSTELLATION_DISABLED_NOT_SUPPORTED_V02 ||
                    ind.qzss_status == eQMI_LOC_CONSTELLATION_DISABLED_INTERNALLY_V02 ||
                    ind.qzss_status == eQMI_LOC_CONSTELLATION_DISABLED_BY_CLIENT_V02 ||
                    ind.qzss_status == eQMI_LOC_CONSTELLATION_DISABLED_NO_MEMORY_V02)) {
        config.blacklistedSvTypesMask |= GNSS_SV_TYPES_MASK_QZSS_BIT;
    }
    if (ind.navic_status_valid &&
            (ind.navic_status == eQMI_LOC_CONSTELLATION_DISABLED_NOT_SUPPORTED_V02 ||
                    ind.navic_status == eQMI_LOC_CONSTELLATION_DISABLED_INTERNALLY_V02 ||
                    ind.navic_status == eQMI_LOC_CONSTELLATION_DISABLED_BY_CLIENT_V02 ||
                    ind.navic_status == eQMI_LOC_CONSTELLATION_DISABLED_NO_MEMORY_V02)) {
        config.blacklistedSvTypesMask |= GNSS_SV_TYPES_MASK_NAVIC_BIT;
    }
}

void LocApiV02::onDbtPosReportEvent(const qmiLocEventDbtPositionReportIndMsgT_v02* pDbtPosReport)
{
    UlpLocation location;
    memset (&location, 0, sizeof (location));
    location.size = sizeof(location);
    const qmiLocDbtPositionStructT_v02 *pReport = &pDbtPosReport->dbtPosition;

    // time stamp
    location.gpsLocation.timestamp = pReport->timestampUtc;
    // latitude & longitude
    location.gpsLocation.flags |= LOC_GPS_LOCATION_HAS_LAT_LONG;
    location.gpsLocation.latitude  = pReport->latitude;
    location.gpsLocation.longitude = pReport->longitude;
    // Altitude
    if (pReport->altitudeWrtEllipsoid_valid == 1) {
        location.gpsLocation.flags |= LOC_GPS_LOCATION_HAS_ALTITUDE;
        location.gpsLocation.altitude = pReport->altitudeWrtEllipsoid;
    }
    // Speed
    if (pReport->speedHorizontal_valid == 1) {
        location.gpsLocation.flags |= LOC_GPS_LOCATION_HAS_SPEED;
        location.gpsLocation.speed = pReport->speedHorizontal;
    }
    // Heading
    if (pReport->heading_valid == 1) {
        location.gpsLocation.flags |= LOC_GPS_LOCATION_HAS_BEARING;
        location.gpsLocation.bearing = pReport->heading;
    }
    // Accuracy
    location.gpsLocation.flags |= LOC_GPS_LOCATION_HAS_ACCURACY;
    location.gpsLocation.accuracy = sqrt(pReport->horUncEllipseSemiMinor*
                                         pReport->horUncEllipseSemiMinor +
                                         pReport->horUncEllipseSemiMajor*
                                         pReport->horUncEllipseSemiMajor);
    // Source used
    LocPosTechMask loc_technology_mask = LOC_POS_TECH_MASK_DEFAULT;
    if (pDbtPosReport->positionSrc_valid) {
        switch (pDbtPosReport->positionSrc) {
            case eQMI_LOC_POSITION_SRC_GNSS_V02:
                loc_technology_mask = LOC_POS_TECH_MASK_SATELLITE;
                break;
            case eQMI_LOC_POSITION_SRC_CELLID_V02:
                loc_technology_mask = LOC_POS_TECH_MASK_CELLID;
                break;
            case eQMI_LOC_POSITION_SRC_ENH_CELLID_V02:
                loc_technology_mask = LOC_POS_TECH_MASK_CELLID;
                break;
            case eQMI_LOC_POSITION_SRC_WIFI_V02:
                loc_technology_mask = LOC_POS_TECH_MASK_WIFI;
                break;
            case eQMI_LOC_POSITION_SRC_TERRESTRIAL_V02:
                loc_technology_mask = LOC_POS_TECH_MASK_REFERENCE_LOCATION;
                break;
            case eQMI_LOC_POSITION_SRC_GNSS_TERRESTRIAL_HYBRID_V02:
                loc_technology_mask = LOC_POS_TECH_MASK_HYBRID;
                break;
            default:
                loc_technology_mask = LOC_POS_TECH_MASK_DEFAULT;
        }
    }

    GpsLocationExtended locationExtended;
    memset(&locationExtended, 0, sizeof (GpsLocationExtended));
    locationExtended.size = sizeof(locationExtended);

    if (pReport->vertUnc_valid) {
       locationExtended.flags |= GPS_LOCATION_EXTENDED_HAS_VERT_UNC;
       locationExtended.vert_unc = pReport->vertUnc;
    }
    if (pDbtPosReport->speedUnc_valid) {
        locationExtended.flags |= GPS_LOCATION_EXTENDED_HAS_SPEED_UNC;
        locationExtended.speed_unc = pDbtPosReport->speedUnc;
    }
    if (pDbtPosReport->headingUnc_valid) {
       locationExtended.flags |= GPS_LOCATION_EXTENDED_HAS_BEARING_UNC;
       locationExtended.bearing_unc = pDbtPosReport->headingUnc;
    }
    // Calling the base
    reportDBTPosition(location,
                      locationExtended,
                      LOC_SESS_SUCCESS,
                      loc_technology_mask);
}

void LocApiV02::batchFullEvent(const qmiLocEventBatchFullIndMsgT_v02* batchFullInfo)
{
    struct MsgGetBatchedLocations : public LocMsg {
        LocApiBase& mApi;
        size_t mCount;
        uint32_t mAccumulatedDistance;
        qmiLocBatchingTypeEnumT_v02 mBatchType;
        inline MsgGetBatchedLocations(LocApiV02& api,
                                      size_t count,
                                      uint32_t accumulatedDistance,
                                      qmiLocBatchingTypeEnumT_v02 batchType) :
            LocMsg(),
            mApi(api),
            mCount(count),
            mAccumulatedDistance(accumulatedDistance),
            mBatchType(batchType) {}
        inline virtual void proc() const {
            // Explicit check for if OTB is supported or not to work around an
            // issue with older modems where uninitialized batchInd is being sent
            if ((mBatchType == eQMI_LOC_OUTDOOR_TRIP_BATCHING_V02) &&
                (ContextBase::isMessageSupported(LOC_API_ADAPTER_MESSAGE_OUTDOOR_TRIP_BATCHING))) {
                mApi.getBatchedTripLocationsSync(mCount, mAccumulatedDistance);
            } else {
                mApi.getBatchedLocationsSync(mCount);
            }
        }
    };

    sendMsg(new MsgGetBatchedLocations(*this,
                                        batchFullInfo->batchCount,
                                        (batchFullInfo->accumulatedDistance_valid ?
                                                batchFullInfo->accumulatedDistance: 0),
                                        (batchFullInfo->batchType_valid ? batchFullInfo->batchType :
                                                eQMI_LOC_LOCATION_BATCHING_V02)));

}

void LocApiV02::batchStatusEvent(const qmiLocEventBatchingStatusIndMsgT_v02* batchStatusInfo)
{
    BatchingStatus batchStatus;

    switch(batchStatusInfo->batchingStatus)
    {
        case eQMI_LOC_BATCH_POS_UNAVAILABLE_V02:
            batchStatus = BATCHING_STATUS_POSITION_UNAVAILABLE;
            break;
        case eQMI_LOC_BATCH_POS_AVAILABLE_V02:
            batchStatus = BATCHING_STATUS_POSITION_AVAILABE;
            break;
        default:
            batchStatus = BATCHING_STATUS_POSITION_UNAVAILABLE;
    }

    handleBatchStatusEvent(batchStatus);
}

// For Geofence
void LocApiV02::geofenceBreachEvent(const qmiLocEventGeofenceBreachIndMsgT_v02* breachInfo)
{
    uint32_t hwId = breachInfo->geofenceId;
    int64_t timestamp = time(NULL); // get the current time
    Location location;
    memset(&location, 0, sizeof(Location));
    location.size = sizeof(Location);

    if (breachInfo->geofencePosition_valid) {
        // Latitude & Longitude
        location.flags |= LOCATION_HAS_LAT_LONG_BIT;
        if (breachInfo->geofencePosition.latitude >= -90 &&
            breachInfo->geofencePosition.latitude <= 90 &&
            breachInfo->geofencePosition.longitude >= -180 &&
            breachInfo->geofencePosition.longitude <= 180) {
            // latitude and longitude look to be in the expected format
            location.latitude  = breachInfo->geofencePosition.latitude;
            location.longitude = breachInfo->geofencePosition.longitude;
        }
        else {
            // latitude and longitude must be in wrong format, so convert
            location.latitude  = breachInfo->geofencePosition.latitude * LAT_LONG_TO_RADIANS;
            location.longitude = breachInfo->geofencePosition.longitude * LAT_LONG_TO_RADIANS;
        }

        // Time stamp (UTC)
        location.timestamp = breachInfo->geofencePosition.timestampUtc;

        // Altitude
        location.flags |= LOCATION_HAS_ALTITUDE_BIT;
        location.altitude = breachInfo->geofencePosition.altitudeWrtEllipsoid;

        // Speed
        if (breachInfo->geofencePosition.speedHorizontal_valid == 1) {
            location.flags |= LOCATION_HAS_SPEED_BIT;
            location.speed = breachInfo->geofencePosition.speedHorizontal;
        }

        // Heading
        if (breachInfo->geofencePosition.heading_valid == 1) {
            location.flags |= LOCATION_HAS_BEARING_BIT;
            location.bearing = breachInfo->geofencePosition.heading;
        }

        // Uncertainty (circular)
        location.flags |= LOCATION_HAS_ACCURACY_BIT;
        location.accuracy = sqrt(
            (breachInfo->geofencePosition.horUncEllipseSemiMinor *
            breachInfo->geofencePosition.horUncEllipseSemiMinor) +
            (breachInfo->geofencePosition.horUncEllipseSemiMajor *
            breachInfo->geofencePosition.horUncEllipseSemiMajor));

        location.techMask = LOCATION_TECHNOLOGY_GNSS_BIT;

        LOC_LOGV("%s:%d]: Location lat=%8.2f long=%8.2f ",
                 __func__, __LINE__, location.latitude, location.longitude);

    } else {
       LOC_LOGE("%s:%d]: NO Location ", __func__, __LINE__);
    }

    GeofenceBreachType breachType;
    switch (breachInfo->breachType) {
    case eQMI_LOC_GEOFENCE_BREACH_TYPE_ENTERING_V02:
        breachType = GEOFENCE_BREACH_ENTER;
        break;
    case eQMI_LOC_GEOFENCE_BREACH_TYPE_LEAVING_V02:
        breachType = GEOFENCE_BREACH_EXIT;
        break;
    default:
        breachType = GEOFENCE_BREACH_UNKNOWN;
        break;
    }

    // calling the base
    geofenceBreach(1, &hwId, location, breachType, timestamp);
}

void
LocApiV02::geofenceBreachEvent(const qmiLocEventGeofenceBatchedBreachIndMsgT_v02* breachInfo)
{
    if (NULL == breachInfo)
        return;

    int64_t timestamp = time(NULL); // get the current time
    Location location;
    memset(&location, 0, sizeof(Location));
    location.size = sizeof(Location);

    if (breachInfo->geofencePosition_valid) {
        // Latitude & Longitude
        location.flags |= LOCATION_HAS_LAT_LONG_BIT;
        if (breachInfo->geofencePosition.latitude >= -90 &&
            breachInfo->geofencePosition.latitude <= 90 &&
            breachInfo->geofencePosition.longitude >= -180 &&
            breachInfo->geofencePosition.longitude <= 180) {
            // latitude and longitude look to be in the expected format
            location.latitude  = breachInfo->geofencePosition.latitude;
            location.longitude = breachInfo->geofencePosition.longitude;
        }
        else {
            // latitdue and longitude must be in wrong format, so convert
            location.latitude  = breachInfo->geofencePosition.latitude *
                                 LAT_LONG_TO_RADIANS;
            location.longitude = breachInfo->geofencePosition.longitude *
                                 LAT_LONG_TO_RADIANS;
        }

        // Time stamp (UTC)
        location.timestamp = breachInfo->geofencePosition.timestampUtc;

        // Altitude
        location.flags |= LOCATION_HAS_ALTITUDE_BIT;
        location.altitude = breachInfo->geofencePosition.altitudeWrtEllipsoid;

        // Speed
        if (breachInfo->geofencePosition.speedHorizontal_valid == 1) {
            location.flags |= LOCATION_HAS_SPEED_BIT;
            location.speed = breachInfo->geofencePosition.speedHorizontal;
        }

        // Heading
        if (breachInfo->geofencePosition.heading_valid == 1) {
            location.flags |= LOCATION_HAS_BEARING_BIT;
            location.bearing = breachInfo->geofencePosition.heading;
        }

        // Uncertainty (circular)
        location.flags |= LOCATION_HAS_ACCURACY_BIT;
        location.accuracy = sqrt(
            (breachInfo->geofencePosition.horUncEllipseSemiMinor *
            breachInfo->geofencePosition.horUncEllipseSemiMinor) +
            (breachInfo->geofencePosition.horUncEllipseSemiMajor *
            breachInfo->geofencePosition.horUncEllipseSemiMajor));

        location.techMask = LOCATION_TECHNOLOGY_GNSS_BIT;

        LOC_LOGV("%s]: latitude=%8.2f longitude=%8.2f ",
                 __func__, location.latitude, location.longitude);

    } else {
       LOC_LOGE("%s:%d]: NO Location ", __func__, __LINE__);
    }

    GeofenceBreachType breachType;
    switch (breachInfo->breachType) {
    case eQMI_LOC_GEOFENCE_BREACH_TYPE_ENTERING_V02:
        breachType = GEOFENCE_BREACH_ENTER;
        break;
    case eQMI_LOC_GEOFENCE_BREACH_TYPE_LEAVING_V02:
        breachType = GEOFENCE_BREACH_EXIT;
        break;
    default:
        breachType = GEOFENCE_BREACH_UNKNOWN;
        break;
    }

    size_t count = 0;
    if (1 == breachInfo->geofenceIdDiscreteList_valid) {
        count += breachInfo->geofenceIdDiscreteList_len;
    }

    if (1 == breachInfo->geofenceIdContinuousList_valid) {
        for (uint32_t i = 0; i < breachInfo->geofenceIdContinuousList_len; i++) {
            count += breachInfo->geofenceIdContinuousList[i].idHigh -
                     breachInfo->geofenceIdContinuousList[i].idLow + 1;
        }
    }

    if (0 == count) {
        return;
    }

    uint32_t* hwIds = new uint32_t[count];
    if (hwIds == nullptr) {
        LOC_LOGE("new allocation failed, fatal error.");
        return;
    }
    uint32_t index = 0;
    if (1 == breachInfo->geofenceIdDiscreteList_valid) {
        for (uint32_t i = 0; i<breachInfo->geofenceIdDiscreteList_len; i++) {
            LOC_LOGV("%s]: discrete hwID %u breachType %u",
                     __func__, breachInfo->geofenceIdDiscreteList[i], breachType);
             if (index < count) {
                 hwIds[index++] = breachInfo->geofenceIdDiscreteList[i];
             }
        }
    }

    if (1 == breachInfo->geofenceIdContinuousList_valid) {
        for (uint32_t i = 0; i < breachInfo->geofenceIdContinuousList_len; i++) {
            for (uint32_t j = breachInfo->geofenceIdContinuousList[i].idLow;
                 j <= breachInfo->geofenceIdContinuousList[i].idHigh;
                 j++) {
                     LOC_LOGV("%s]: continuous hwID %u breachType %u",__func__, j, breachType);
                if (index < count) {
                    hwIds[index++] = j;
                }
            }
        }
    }

    // calling the base
    geofenceBreach(count, hwIds, location, breachType, timestamp);

    delete[] hwIds;
}

void LocApiV02::geofenceStatusEvent(const qmiLocEventGeofenceGenAlertIndMsgT_v02* alertInfo)
{
    const char* names[] = {
        "bad value",
        "GEOFENCE_GEN_ALERT_GNSS_UNAVAILABLE",
        "GEOFENCE_GEN_ALERT_GNSS_AVAILABLE",
        "GEOFENCE_GEN_ALERT_OOS",
        "GEOFENCE_GEN_ALERT_TIME_INVALID"
    };
    int index = alertInfo->geofenceAlert;
    if (index < 0 || index > 4) {
        index = 0;
    }
    LOC_LOGV("%s]: GEOFENCE_GEN_ALERT - %s", __func__, names[index]);

    GeofenceStatusAvailable available = GEOFENCE_STATUS_AVAILABILE_NO;
    switch (alertInfo->geofenceAlert) {
    case eQMI_LOC_GEOFENCE_GEN_ALERT_GNSS_UNAVAILABLE_V02:
        available = GEOFENCE_STATUS_AVAILABILE_NO;
        break;
    case eQMI_LOC_GEOFENCE_GEN_ALERT_GNSS_AVAILABLE_V02:
        available = GEOFENCE_STATUS_AVAILABILE_YES;
        break;
    default:
        return;
        break;
    }

    // calling the base
    geofenceStatus(available);
}

void
LocApiV02::geofenceDwellEvent(const qmiLocEventGeofenceBatchedDwellIndMsgT_v02 *dwellInfo)
{
    if (NULL == dwellInfo)
        return;

    int64_t timestamp = time(NULL); // get the current time
    GeofenceBreachType breachType;
    if (eQMI_LOC_GEOFENCE_DWELL_TYPE_INSIDE_V02 == dwellInfo->dwellType) {
        breachType = GEOFENCE_BREACH_DWELL_IN;
    } else if (eQMI_LOC_GEOFENCE_DWELL_TYPE_OUTSIDE_V02 == dwellInfo->dwellType) {
        breachType = GEOFENCE_BREACH_DWELL_OUT;
    } else {
        LOC_LOGW("%s]: unknown dwell type %d", __func__, dwellInfo->dwellType);
        breachType = GEOFENCE_BREACH_UNKNOWN;
    }

    Location location;
    memset(&location, 0, sizeof(Location));
    location.size = sizeof(Location);

    if (dwellInfo->geofencePosition_valid) {
        // Latitude & Longitude
        location.flags |= LOCATION_HAS_LAT_LONG_BIT;
        if (dwellInfo->geofencePosition.latitude >= -90 &&
            dwellInfo->geofencePosition.latitude <= 90 &&
            dwellInfo->geofencePosition.longitude >= -180 &&
            dwellInfo->geofencePosition.longitude <= 180) {
            // latitude and longitude look to be in the expected format
            location.latitude  = dwellInfo->geofencePosition.latitude;
            location.longitude = dwellInfo->geofencePosition.longitude;
        } else {
            // latitude and longitude must be in wrong format, so convert
            location.latitude = dwellInfo->geofencePosition.latitude *
                                   LAT_LONG_TO_RADIANS;
            location.longitude = dwellInfo->geofencePosition.longitude *
                                    LAT_LONG_TO_RADIANS;
        }

        // Time stamp (UTC)
        location.timestamp = dwellInfo->geofencePosition.timestampUtc;

        // Altitude
        location.flags |= LOCATION_HAS_ALTITUDE_BIT;
        location.altitude = dwellInfo->geofencePosition.altitudeWrtEllipsoid;

        // Speed
        if (dwellInfo->geofencePosition.speedHorizontal_valid == 1) {
            location.flags |= LOCATION_HAS_SPEED_BIT;
            location.speed = dwellInfo->geofencePosition.speedHorizontal;
        }

        // Heading
        if (dwellInfo->geofencePosition.heading_valid == 1) {
            location.flags |= LOCATION_HAS_BEARING_BIT;
            location.bearing = dwellInfo->geofencePosition.heading;
        }

        // Uncertainty (circular)
        location.flags |= LOCATION_HAS_ACCURACY_BIT;
        location.accuracy = sqrt(
            (dwellInfo->geofencePosition.horUncEllipseSemiMinor *
            dwellInfo->geofencePosition.horUncEllipseSemiMinor) +
            (dwellInfo->geofencePosition.horUncEllipseSemiMajor *
            dwellInfo->geofencePosition.horUncEllipseSemiMajor));

            LOC_LOGV("%s]: latitude=%8.2f longitude=%8.2f ",
                     __func__, location.latitude, location.longitude);
    } else {
       LOC_LOGE("%s:%d]: NO Location ", __func__, __LINE__);
    }

    size_t count = 0;
    if (1 == dwellInfo->geofenceIdDiscreteList_valid) {
        count += dwellInfo->geofenceIdDiscreteList_len;
    }

    if (1 == dwellInfo->geofenceIdContinuousList_valid) {
        for (uint32_t i = 0; i < dwellInfo->geofenceIdContinuousList_len; i++) {
            count += dwellInfo->geofenceIdContinuousList[i].idHigh -
                     dwellInfo->geofenceIdContinuousList[i].idLow + 1;
        }
    }

    if (0 == count) {
        return;
    }

    uint32_t* hwIds = new uint32_t[count];
    if (hwIds == nullptr) {
        LOC_LOGE("new allocation failed, fatal error.");
        return;
    }
    uint32_t index = 0;
    if (1 == dwellInfo->geofenceIdDiscreteList_valid) {
        for (uint32_t i = 0; i<dwellInfo->geofenceIdDiscreteList_len; i++) {
            LOC_LOGV("%s]: discrete hwID %u breachType %u",
                     __func__, dwellInfo->geofenceIdDiscreteList[i], breachType);
             if (index < count) {
                 hwIds[index++] = dwellInfo->geofenceIdDiscreteList[i];
             }
        }
    }

    if (1 == dwellInfo->geofenceIdContinuousList_valid) {
        for (uint32_t i = 0; i < dwellInfo->geofenceIdContinuousList_len; i++) {
            for (uint32_t j = dwellInfo->geofenceIdContinuousList[i].idLow;
                 j <= dwellInfo->geofenceIdContinuousList[i].idHigh;
                 j++) {
                LOC_LOGV("%s]: continuous hwID %u breachType %u", __func__, j , breachType);
                if (index < count) {
                    hwIds[index++] = j;
                }
            }
        }
    }

    // calling the base
    geofenceBreach(count, hwIds, location, breachType, timestamp);

    delete[] hwIds;
}

void
LocApiV02::addGeofence(uint32_t clientId,
                        const GeofenceOption& options,
                        const GeofenceInfo& info,
                        LocApiResponseData<LocApiGeofenceData>* adapterResponseData)
{
    sendMsg(new LocApiMsg([this, clientId, options, info, adapterResponseData] () {

    LOC_LOGD("%s]: lat=%8.2f long=%8.2f radius %8.2f breach=%u respon=%u dwell=%u",
             __func__, info.latitude, info.longitude, info.radius,
             options.breachTypeMask, options.responsiveness, options.dwellTime);
    LocationError err = LOCATION_ERROR_GENERAL_FAILURE;

    qmiLocAddCircularGeofenceReqMsgT_v02 addReq;
    memset(&addReq, 0, sizeof(addReq));

    if (options.breachTypeMask & GEOFENCE_BREACH_ENTER_BIT)
        addReq.breachMask |= QMI_LOC_GEOFENCE_BREACH_ENTERING_MASK_V02;
    if (options.breachTypeMask & GEOFENCE_BREACH_EXIT_BIT)
        addReq.breachMask |= QMI_LOC_GEOFENCE_BREACH_LEAVING_MASK_V02;

    // confidence
    addReq.confidence_valid = true;
    addReq.confidence = eQMI_LOC_GEOFENCE_CONFIDENCE_HIGH_V02; // always high

    // custom responsiveness
    addReq.customResponsivenessValue_valid = true;
    // The (min,max) custom responsiveness we support in seconds is (1, 65535)
    addReq.customResponsivenessValue =
            ((options.responsiveness < 1000U) ? 1 :
            std::min((options.responsiveness / 1000U), (uint32_t)UINT16_MAX));

    // dwell time
    if (options.dwellTime > 0) {
        addReq.dwellTime_valid = 1;
        addReq.dwellTime = options.dwellTime;
        addReq.dwellTypeMask_valid = 1;
        if (options.breachTypeMask & GEOFENCE_BREACH_DWELL_IN_BIT) {
            addReq.dwellTypeMask |= QMI_LOC_GEOFENCE_DWELL_TYPE_INSIDE_MASK_V02;
        }
        if (options.breachTypeMask & GEOFENCE_BREACH_DWELL_OUT_BIT) {
            addReq.dwellTypeMask |= QMI_LOC_GEOFENCE_DWELL_TYPE_OUTSIDE_MASK_V02;
        }
    }
    addReq.circularGeofenceArgs.latitude = info.latitude;
    addReq.circularGeofenceArgs.longitude = info.longitude;
    addReq.circularGeofenceArgs.radius = info.radius;
    addReq.includePosition = true;
    addReq.transactionId = clientId;

    LOC_SEND_SYNC_REQ(AddCircularGeofence, ADD_CIRCULAR_GEOFENCE, addReq);

    LocApiGeofenceData data;
    if (rv && ind.geofenceId_valid != 0) {
        data.hwId = ind.geofenceId;
        err = LOCATION_ERROR_SUCCESS;
    } else {
        if (eQMI_LOC_MAX_GEOFENCE_PROGRAMMED_V02 == ind.status) {
            err = LOCATION_ERROR_GEOFENCES_AT_MAX;
        }
        LOC_LOGE("%s]: failed! rv is %d, ind.geofenceId_valid is %d",
                 __func__, rv, ind.geofenceId_valid);
    }

    if (adapterResponseData != NULL) {
        adapterResponseData->returnToSender(err, data);
    }
    }));
}

void
LocApiV02::removeGeofence(uint32_t hwId, uint32_t clientId, LocApiResponse* adapterResponse)
{
    sendMsg(new LocApiMsg([this, hwId, clientId, adapterResponse] () {

    LOC_LOGD("%s]: hwId %u", __func__, hwId);
    LocationError err = LOCATION_ERROR_GENERAL_FAILURE;

    qmiLocDeleteGeofenceReqMsgT_v02 deleteReq;
    memset(&deleteReq, 0, sizeof(deleteReq));

    deleteReq.geofenceId = hwId;
    deleteReq.transactionId = clientId;

    LOC_SEND_SYNC_REQ(DeleteGeofence, DELETE_GEOFENCE, deleteReq);

    if (rv) {
        err = LOCATION_ERROR_SUCCESS;
    } else {
        LOC_LOGE("%s]: failed! rv is %d", __func__, rv);
    }

    if (adapterResponse != NULL) {
        adapterResponse->returnToSender(err);
    }
    }));
}

void
LocApiV02::pauseGeofence(uint32_t hwId, uint32_t clientId, LocApiResponse* adapterResponse)
{
    sendMsg(new LocApiMsg([this, hwId, clientId, adapterResponse] () {

    LOC_LOGD("%s]: hwId %u", __func__, hwId);
    LocationError err = LOCATION_ERROR_GENERAL_FAILURE;

    qmiLocEditGeofenceReqMsgT_v02 editReq;
    memset(&editReq, 0, sizeof(editReq));

    editReq.geofenceId = hwId;
    editReq.transactionId = clientId;
    editReq.geofenceState_valid = 1;
    editReq.geofenceState = eQMI_LOC_GEOFENCE_STATE_SUSPEND_V02;

    LOC_SEND_SYNC_REQ(EditGeofence, EDIT_GEOFENCE, editReq);

    if (rv) {
        err = LOCATION_ERROR_SUCCESS;
    } else {
        LOC_LOGE("%s]: failed! rv is %d", __func__, rv);
    }

    if (adapterResponse != NULL) {
        adapterResponse->returnToSender(err);
    }
    }));
}

void
LocApiV02::resumeGeofence(uint32_t hwId, uint32_t clientId, LocApiResponse* adapterResponse)
{
    sendMsg(new LocApiMsg([this, hwId, clientId, adapterResponse] () {

    LOC_LOGD("%s]: hwId %u", __func__, hwId);
    LocationError err = LOCATION_ERROR_GENERAL_FAILURE;

    qmiLocEditGeofenceReqMsgT_v02 editReq;
    memset(&editReq, 0, sizeof(editReq));

    editReq.geofenceId = hwId;
    editReq.transactionId = clientId;
    editReq.geofenceState_valid = 1;
    editReq.geofenceState = eQMI_LOC_GEOFENCE_STATE_ACTIVE_V02;

    LOC_SEND_SYNC_REQ(EditGeofence, EDIT_GEOFENCE, editReq);

    if (rv) {
        err = LOCATION_ERROR_SUCCESS;
    } else {
        LOC_LOGE("%s]: failed! rv is %d", __func__, rv);
    }

    if (adapterResponse != NULL) {
        adapterResponse->returnToSender(err);
    }
    }));
}

void
LocApiV02::modifyGeofence(uint32_t hwId,
                           uint32_t clientId,
                           const GeofenceOption& options, LocApiResponse* adapterResponse)
{
    sendMsg(new LocApiMsg([this, hwId, clientId, options, adapterResponse] () {

    LOC_LOGD("%s]: breach=%u respon=%u dwell=%u",
             __func__, options.breachTypeMask, options.responsiveness, options.dwellTime);
    LocationError err = LOCATION_ERROR_GENERAL_FAILURE;

    qmiLocEditGeofenceReqMsgT_v02 editReq;
    memset(&editReq, 0, sizeof(editReq));

    editReq.geofenceId = hwId;
    editReq.transactionId = clientId;

    editReq.breachMask_valid = 1;
    if (options.breachTypeMask & GEOFENCE_BREACH_ENTER_BIT) {
        editReq.breachMask |= QMI_LOC_GEOFENCE_BREACH_ENTERING_MASK_V02;
    }
    if (options.breachTypeMask & GEOFENCE_BREACH_EXIT_BIT) {
        editReq.breachMask |= QMI_LOC_GEOFENCE_BREACH_LEAVING_MASK_V02;
    }

    editReq.responsiveness_valid = 1;
    if (options.responsiveness <= GF_RESPONSIVENESS_THRESHOLD_MSEC_HIGH) {
        editReq.responsiveness = eQMI_LOC_GEOFENCE_RESPONSIVENESS_HIGH_V02;
    } else if (options.responsiveness <= GF_RESPONSIVENESS_THRESHOLD_MSEC_MEDIUM) {
        editReq.responsiveness = eQMI_LOC_GEOFENCE_RESPONSIVENESS_MED_V02;
    } else {
        editReq.responsiveness = eQMI_LOC_GEOFENCE_RESPONSIVENESS_LOW_V02;
    }

    LOC_SEND_SYNC_REQ(EditGeofence, EDIT_GEOFENCE, editReq);

    if (rv) {
        err = LOCATION_ERROR_SUCCESS;
    } else {
        LOC_LOGE("%s]: failed! rv is %d", __func__, rv);
    }
    if (adapterResponse != NULL) {
        adapterResponse->returnToSender(err);
    }
    }));

}

void LocApiV02::setBatchSize(size_t size)
{
    LOC_LOGD("%s]: mDesiredBatchSize %zu", __func__, size);
    mDesiredBatchSize = size;
    // set to zero so the actual batch size will be queried from modem on first startBatching call
    mBatchSize = 0;
}

void LocApiV02::setTripBatchSize(size_t size)
{
    mDesiredTripBatchSize = size;
    // set to zero so the actual trip batch size will be queried from modem on
    // first startOutdoorTripBatching call
    mTripBatchSize = 0;
}

LocationError
LocApiV02::queryBatchBuffer(size_t desiredSize,
        size_t &allocatedSize, BatchingMode batchMode)
{
    LocationError err = LOCATION_ERROR_GENERAL_FAILURE;
    qmiLocGetBatchSizeReqMsgT_v02 batchSizeReq;

    memset(&batchSizeReq, 0, sizeof(batchSizeReq));
    batchSizeReq.transactionId = 1;
    batchSizeReq.batchSize = desiredSize;

    batchSizeReq.batchType_valid = 1;
    batchSizeReq.batchType = (batchMode == BATCHING_MODE_ROUTINE ? eQMI_LOC_LOCATION_BATCHING_V02 :
            eQMI_LOC_OUTDOOR_TRIP_BATCHING_V02);
    LOC_SEND_SYNC_REQ(GetBatchSize, GET_BATCH_SIZE, batchSizeReq);

    if (rv) {
        allocatedSize = ind.batchSize;
        LOC_LOGV("%s:%d]: get batching size succeeded. The modem batch size for"
                " batch mode %u is %zu. Desired batch size : %zu.",
                __func__, __LINE__, batchMode, allocatedSize, desiredSize);
        if (allocatedSize != 0) {
            err = LOCATION_ERROR_SUCCESS;
        }
    } else {
        if ((batchMode == BATCHING_MODE_TRIP) &&
            (ind.status == eQMI_LOC_INVALID_PARAMETER_V02) &&
            (ind.batchSize > 0)) {

            LOC_LOGW("%s:%d]: get batching size failed. The modem max threshold batch size "
                    "for batch mode %u is %u. Desired batch size : %zu. "
                    "Retrying with max threshold size ...",
                    __func__, __LINE__, batchMode, ind.batchSize, desiredSize);

            desiredSize = ind.batchSize;
            batchSizeReq.batchSize = ind.batchSize;
            LOC_SEND_SYNC_REQ(GetBatchSize, GET_BATCH_SIZE, batchSizeReq);

            if (rv) {
                allocatedSize = ind.batchSize;
                LOC_LOGV("%s:%d]: get batching size succeeded. The modem batch size for"
                        " batch mode %u is %zu. Desired batch size : %zu.",
                        __func__, __LINE__, batchMode, allocatedSize, desiredSize);
                if (allocatedSize != 0) {
                    err = LOCATION_ERROR_SUCCESS;
                }
                return err;
            }
        }

        allocatedSize = 0;
        LOC_LOGE("%s:%d]: get batching size failed for batch mode %u and desired batch size %zu"
                "Or modem does not support batching",
                __func__, __LINE__, batchMode, desiredSize);
    }

    return err;
}

LocationError
LocApiV02::releaseBatchBuffer(BatchingMode batchMode) {

    LocationError err = LOCATION_ERROR_GENERAL_FAILURE;

    qmiLocReleaseBatchReqMsgT_v02 batchReleaseReq;
    memset(&batchReleaseReq, 0, sizeof(batchReleaseReq));
    batchReleaseReq.transactionId = 1;

    batchReleaseReq.batchType_valid = 1;
    switch (batchMode) {
        case BATCHING_MODE_ROUTINE:
            batchReleaseReq.batchType = eQMI_LOC_LOCATION_BATCHING_V02;
        break;

        case BATCHING_MODE_TRIP:
            batchReleaseReq.batchType = eQMI_LOC_OUTDOOR_TRIP_BATCHING_V02;
        break;

        default:
            err = LOCATION_ERROR_INVALID_PARAMETER;
            LOC_LOGE("%s:%d]: release batch failed for batch mode %u",
                __func__, __LINE__, batchMode);
            return err;
    }

    LOC_SEND_SYNC_REQ(ReleaseBatch, RELEASE_BATCH, batchReleaseReq);

    if (rv) {
        LOC_LOGV("%s:%d]: release batch succeeded for batch mode %u",
                 __func__, __LINE__, batchMode);
        mTripBatchSize = 0;
        err = LOCATION_ERROR_SUCCESS;
    } else {
        LOC_LOGE("%s:%d]: release batch failed for batch mode %u",
                __func__, __LINE__, batchMode);
    }

    return err;
}


void
LocApiV02::setOperationMode(GnssSuplMode mode)
{
    locClientStatusEnumType status;
    locClientReqUnionType req_union;

    qmiLocSetOperationModeReqMsgT_v02 set_mode_msg;
    qmiLocSetOperationModeIndMsgT_v02 set_mode_ind;

    memset (&set_mode_msg, 0, sizeof(set_mode_msg));
    memset (&set_mode_ind, 0, sizeof(set_mode_ind));

    if (GNSS_SUPL_MODE_MSB == mode) {
        set_mode_msg.operationMode = eQMI_LOC_OPER_MODE_MSB_V02;
        LOC_LOGV("%s:%d]: operationMode MSB", __func__, __LINE__);
    } else if (GNSS_SUPL_MODE_MSA == mode) {
        set_mode_msg.operationMode = eQMI_LOC_OPER_MODE_MSA_V02;
        LOC_LOGV("%s:%d]: operationMode MSA", __func__, __LINE__);
    } else {
        set_mode_msg.operationMode = eQMI_LOC_OPER_MODE_STANDALONE_V02;
        LOC_LOGV("%s:%d]: operationMode STANDALONE", __func__, __LINE__);
    }

    req_union.pSetOperationModeReq = &set_mode_msg;

    status = locSyncSendReq(QMI_LOC_SET_OPERATION_MODE_REQ_V02,
                            req_union, LOC_ENGINE_SYNC_REQUEST_TIMEOUT,
                            QMI_LOC_SET_OPERATION_MODE_IND_V02,
                            &set_mode_ind);

    if (eLOC_CLIENT_SUCCESS != status || eQMI_LOC_SUCCESS_V02 != set_mode_ind.status) {
        LOC_LOGE ("%s:%d]: Failed status = %d ind.status = %d",
                  __func__, __LINE__, status, set_mode_ind.status);
    }
}

void
LocApiV02::startTimeBasedTracking(const TrackingOptions& options, LocApiResponse* adapterResponse)
{
    sendMsg(new LocApiMsg([this, options, adapterResponse] () {

    LOC_LOGD("%s] minInterval %u", __func__, options.minInterval);
    LocationError err = LOCATION_ERROR_SUCCESS;

    if (!mInSession) {
        mPositionElapsedRealTimeCal.reset();
        mMeasElapsedRealTimeCal.reset();
    }

    mInSession = true;
    mMeasurementsStarted = true;
    registerEventMask(mMask);
    setOperationMode(options.mode);

    locClientStatusEnumType status;
    locClientReqUnionType req_union;
    qmiLocStartReqMsgT_v02 start_msg;
    memset(&start_msg, 0, sizeof(start_msg));

    start_msg.sessionId = 1; // dummy session id

    // interval
    uint32_t minInterval = options.minInterval;
    mMinInterval = minInterval;

    /*set interval for intermediate fixes*/
    start_msg.minIntermediatePositionReportInterval_valid = 1;
    start_msg.minIntermediatePositionReportInterval = minInterval;

    /*set interval for final fixes*/
    start_msg.minInterval_valid = 1;
    start_msg.minInterval = minInterval;

    // accuracy
    start_msg.horizontalAccuracyLevel_valid = 1;
    start_msg.horizontalAccuracyLevel = eQMI_LOC_ACCURACY_HIGH_V02;

    // recurrence
    start_msg.fixRecurrence_valid = 1;
    start_msg.fixRecurrence = eQMI_LOC_RECURRENCE_PERIODIC_V02;

    // altitude assumed
    start_msg.configAltitudeAssumed_valid = 1;
    start_msg.configAltitudeAssumed =
        eQMI_LOC_ALTITUDE_ASSUMED_IN_GNSS_SV_INFO_DISABLED_V02;

    // power mode
    if (GNSS_POWER_MODE_INVALID != options.powerMode) {
        start_msg.powerMode_valid = 1;
        start_msg.powerMode.powerMode =
                convertPowerMode(options.powerMode);
        start_msg.powerMode.timeBetweenMeasurement = options.tbm;
        // Force low accuracy for background power modes
        if (GNSS_POWER_MODE_M3 == options.powerMode ||
                GNSS_POWER_MODE_M4 == options.powerMode ||
                GNSS_POWER_MODE_M5 == options.powerMode) {
            start_msg.horizontalAccuracyLevel =  eQMI_LOC_ACCURACY_LOW_V02;
        }
        // Force TBM = TBF for power mode M4
        if (GNSS_POWER_MODE_M4 == options.powerMode) {
            start_msg.powerMode.timeBetweenMeasurement = start_msg.minInterval;
        }
    }

    req_union.pStartReq = &start_msg;
    status = locClientSendReq(QMI_LOC_START_REQ_V02, req_union);
    if (eLOC_CLIENT_SUCCESS != status) {
        LOC_LOGE ("%s]: failed! status %d",
                  __func__, status);
        err = LOCATION_ERROR_GENERAL_FAILURE;
    }

    if (adapterResponse != NULL) {
        adapterResponse->returnToSender(err);
    }
    }));
}

void LocApiV02::configConstellationMultiBand(const GnssSvTypeConfig& secondaryBandConfig,
                                             LocApiResponse* adapterResponse) {

    sendMsg(new LocApiMsg([this, secondaryBandConfig, adapterResponse] () {

    LocationError err = LOCATION_ERROR_SUCCESS;
    locClientStatusEnumType status = eLOC_CLIENT_FAILURE_GENERAL;
    locClientReqUnionType req_union = {};

    qmiLocSetMultibandConfigReqMsgT_v02 reqMsg = {};
    qmiLocGenReqStatusIndMsgT_v02 genReqStatusIndMsg = {};

    // Fill in the request details
    reqMsg.secondaryGnssConfig_valid = true;
    if (secondaryBandConfig.enabledSvTypesMask & GNSS_SV_TYPES_MASK_GPS_BIT) {
        reqMsg.secondaryGnssConfig |= eQMI_SYSTEM_GPS_V02;
    }
    if (secondaryBandConfig.enabledSvTypesMask & GNSS_SV_TYPES_MASK_GLO_BIT) {
        reqMsg.secondaryGnssConfig |= eQMI_SYSTEM_GLO_V02;
    }
    if (secondaryBandConfig.enabledSvTypesMask & GNSS_SV_TYPES_MASK_BDS_BIT) {
        reqMsg.secondaryGnssConfig |= eQMI_SYSTEM_BDS_V02;
    }
    if (secondaryBandConfig.enabledSvTypesMask & GNSS_SV_TYPES_MASK_QZSS_BIT) {
        reqMsg.secondaryGnssConfig |= eQMI_SYSTEM_QZSS_V02;
    }
    if (secondaryBandConfig.enabledSvTypesMask & GNSS_SV_TYPES_MASK_GAL_BIT) {
        reqMsg.secondaryGnssConfig |= eQMI_SYSTEM_GAL_V02;
    }
    if (secondaryBandConfig.enabledSvTypesMask & GNSS_SV_TYPES_MASK_NAVIC_BIT) {
        reqMsg.secondaryGnssConfig |= eQMI_SYSTEM_NAVIC_V02;
    }

    LOC_LOGd("hal secondary band enabled constellation: 0x%" PRIx64 ", "
             "qmi secondary band disabed constellation: 0x%" PRIx64 "",
             secondaryBandConfig.enabledSvTypesMask, reqMsg.secondaryGnssConfig);

    // Update in request union
    req_union.pSetMultibandConfigReq = &reqMsg;

    // Send the request
    status = loc_sync_send_req(clientHandle,
                               QMI_LOC_SET_MULTIBAND_CONFIG_REQ_V02,
                               req_union,
                               LOC_ENGINE_SYNC_REQUEST_TIMEOUT,
                               QMI_LOC_SET_MULTIBAND_CONFIG_IND_V02,
                               &genReqStatusIndMsg);
    if (status != eLOC_CLIENT_SUCCESS ||
            genReqStatusIndMsg.status != eQMI_LOC_SUCCESS_V02) {
        LOC_LOGe("Config multiband failed. status: %s ind status %s",
                 loc_get_v02_client_status_name(status),
                 loc_get_v02_qmi_status_name(genReqStatusIndMsg.status));

        if (status == eLOC_CLIENT_FAILURE_UNSUPPORTED ||
                status == eLOC_CLIENT_FAILURE_INVALID_MESSAGE_ID) {
            err = LOCATION_ERROR_NOT_SUPPORTED;
        } else {
            err = LOCATION_ERROR_GENERAL_FAILURE;
        }
    }

    if (adapterResponse) {
        adapterResponse->returnToSender(err);
    }
    }));
}

void LocApiV02::getConstellationMultiBandConfig(
        uint32_t sessionId, LocApiResponse* adapterResponse) {

    sendMsg(new LocApiMsg([this, sessionId, adapterResponse] () {

    LocationError err = LOCATION_ERROR_SUCCESS;
    locClientStatusEnumType status = eLOC_CLIENT_FAILURE_GENERAL;
    locClientReqUnionType req_union = {};

    qmiLocGetConstellationConfigIndMsgT_v02 getConstellationConfigInd = {};
    qmiLocGetBlacklistSvIndMsgT_v02 getBlacklistSvConfigInd = {};
    qmiLocGetMultibandConfigIndMsgT_v02 getMultibandConfigInd = {};
    GnssConfig gnssConfig = {};

    // get multiband config
    status = locSyncSendReq(QMI_LOC_GET_MULTIBAND_CONFIG_REQ_V02,
                            req_union, LOC_ENGINE_SYNC_REQUEST_LONG_TIMEOUT,
                            QMI_LOC_GET_MULTIBAND_CONFIG_IND_V02,
                            &getMultibandConfigInd);
    if ((status == eLOC_CLIENT_SUCCESS) && (getMultibandConfigInd.status == eQMI_LOC_SUCCESS_V02) &&
            getMultibandConfigInd.secondaryGnssConfig_valid) {
        convertQmiSecondaryConfigToGnssConfig(getMultibandConfigInd.secondaryGnssConfig,
                                              gnssConfig.secondaryBandConfig);
        gnssConfig.flags |= GNSS_CONFIG_FLAGS_CONSTELLATION_SECONDARY_BAND_BIT;
    } else {
        LOC_LOGe("Get multiband config failed. status: %s, ind status:%s",
                 loc_get_v02_client_status_name(status),
                 loc_get_v02_qmi_status_name(getMultibandConfigInd.status));

        if (status == eLOC_CLIENT_FAILURE_UNSUPPORTED ||
                status == eLOC_CLIENT_FAILURE_INVALID_MESSAGE_ID) {
            err = LOCATION_ERROR_NOT_SUPPORTED;
        } else {
            err = LOCATION_ERROR_GENERAL_FAILURE;
        }
    }

    if (gnssConfig.flags) {
        LocApiBase::reportGnssConfig(sessionId, gnssConfig);
    } else {
        if (err != LOCATION_ERROR_NOT_SUPPORTED) {
            err = LOCATION_ERROR_GENERAL_FAILURE;
        }
        adapterResponse->returnToSender(err);
    }

    LOC_LOGd("Exit. err: %u", err);
    }));
}


void LocApiV02::convertQmiSecondaryConfigToGnssConfig(
        qmiLocGNSSConstellEnumT_v02 qmiSecondaryBandConfig,
        GnssSvTypeConfig& secondaryBandConfig) {

    LOC_LOGd("qmi secondary band config: 0x%" PRIx64 "", qmiSecondaryBandConfig);
    memset(&secondaryBandConfig, 0, sizeof(secondaryBandConfig));
    secondaryBandConfig.size = sizeof(secondaryBandConfig);

    if (qmiSecondaryBandConfig & eQMI_SYSTEM_GPS_V02) {
        secondaryBandConfig.enabledSvTypesMask |= GNSS_SV_TYPES_MASK_GPS_BIT;
    }
    if (qmiSecondaryBandConfig & eQMI_SYSTEM_GLO_V02) {
        secondaryBandConfig.enabledSvTypesMask |= GNSS_SV_TYPES_MASK_GLO_BIT;
    }
    if (qmiSecondaryBandConfig &eQMI_SYSTEM_BDS_V02) {
        secondaryBandConfig.enabledSvTypesMask |= GNSS_SV_TYPES_MASK_BDS_BIT;
    }
    if (qmiSecondaryBandConfig & eQMI_SYSTEM_QZSS_V02) {
        secondaryBandConfig.enabledSvTypesMask |= GNSS_SV_TYPES_MASK_QZSS_BIT;
    }
    if (qmiSecondaryBandConfig & eQMI_SYSTEM_GAL_V02) {
        secondaryBandConfig.enabledSvTypesMask |= GNSS_SV_TYPES_MASK_GAL_BIT;
    }
    if (qmiSecondaryBandConfig & eQMI_SYSTEM_NAVIC_V02) {
        secondaryBandConfig.enabledSvTypesMask |= GNSS_SV_TYPES_MASK_NAVIC_BIT;
    }

    secondaryBandConfig.blacklistedSvTypesMask =
            (~secondaryBandConfig.enabledSvTypesMask & GNSS_SV_TYPES_MASK_ALL);
}

void LocApiV02::convertQmiBlacklistedSvConfigToGnssConfig(
        const qmiLocGetBlacklistSvIndMsgT_v02& qmiBlacklistConfig,
        GnssSvIdConfig& gnssBlacklistConfig) {

    GnssSvIdConfig svConfig = {};
    gnssBlacklistConfig.size = sizeof(gnssBlacklistConfig);

    if (qmiBlacklistConfig.glo_persist_blacklist_sv_valid) {
        gnssBlacklistConfig.gloBlacklistSvMask = qmiBlacklistConfig.glo_persist_blacklist_sv;
    }

    if (qmiBlacklistConfig.bds_persist_blacklist_sv_valid) {
        gnssBlacklistConfig.bdsBlacklistSvMask = qmiBlacklistConfig.bds_persist_blacklist_sv;
    }

    if (qmiBlacklistConfig.qzss_persist_blacklist_sv_valid) {
        gnssBlacklistConfig.qzssBlacklistSvMask = qmiBlacklistConfig.qzss_persist_blacklist_sv;
    }

    if (qmiBlacklistConfig.gal_persist_blacklist_sv_valid) {
        gnssBlacklistConfig.galBlacklistSvMask = qmiBlacklistConfig.gal_persist_blacklist_sv;
    }

    if (qmiBlacklistConfig.sbas_persist_blacklist_sv_valid) {
        gnssBlacklistConfig.sbasBlacklistSvMask = qmiBlacklistConfig.sbas_persist_blacklist_sv;
    }

    if (qmiBlacklistConfig.navic_persist_blacklist_sv_valid) {
        gnssBlacklistConfig.navicBlacklistSvMask = qmiBlacklistConfig.navic_persist_blacklist_sv;
    }

    LOC_LOGd("%d %d %d %d %d %d , blacklist bds 0x%" PRIx64 ", "
             "glo 0x%" PRIx64", qzss 0x%" PRIx64 ", "
             "gal 0x%" PRIx64 ", sbas 0x%" PRIx64 ", "
             "navic 0x%" PRIx64 "",
             qmiBlacklistConfig.glo_persist_blacklist_sv_valid,
             qmiBlacklistConfig.bds_persist_blacklist_sv_valid,
             qmiBlacklistConfig.qzss_persist_blacklist_sv_valid,
             qmiBlacklistConfig.gal_persist_blacklist_sv_valid,
             qmiBlacklistConfig.sbas_persist_blacklist_sv_valid,
             qmiBlacklistConfig.navic_persist_blacklist_sv_valid,
             gnssBlacklistConfig.bdsBlacklistSvMask, gnssBlacklistConfig.gloBlacklistSvMask,
             gnssBlacklistConfig.qzssBlacklistSvMask, gnssBlacklistConfig.galBlacklistSvMask,
             gnssBlacklistConfig.sbasBlacklistSvMask, gnssBlacklistConfig.navicBlacklistSvMask);
}

qmiLocPowerModeEnumT_v02
LocApiV02::convertPowerMode(GnssPowerMode powerMode)
{
    switch(powerMode) {
    case GNSS_POWER_MODE_M1:
        return eQMI_LOC_POWER_MODE_IMPROVED_ACCURACY_V02;
    case GNSS_POWER_MODE_M2:
        return eQMI_LOC_POWER_MODE_NORMAL_V02;
    case GNSS_POWER_MODE_M3:
        return eQMI_LOC_POWER_MODE_BACKGROUND_DEFINED_POWER_V02;
    case GNSS_POWER_MODE_M4:
        return eQMI_LOC_POWER_MODE_BACKGROUND_DEFINED_TIME_V02;
    case GNSS_POWER_MODE_M5:
        return eQMI_LOC_POWER_MODE_BACKGROUND_KEEP_WARM_V02;
    default:
        LOC_LOGE("Invalid power mode %d", powerMode);
    }

    return QMILOCPOWERMODEENUMT_MIN_ENUM_VAL_V02;
}

void
LocApiV02::stopTimeBasedTracking(LocApiResponse* adapterResponse)
{
    sendMsg(new LocApiMsg([this, adapterResponse] () {

    LOC_LOGD("%s] ", __func__);
    LocationError err = LOCATION_ERROR_SUCCESS;

    locClientStatusEnumType status;
    locClientReqUnionType req_union;
    qmiLocStopReqMsgT_v02 stop_msg;
    memset(&stop_msg, 0, sizeof(stop_msg));
    stop_msg.sessionId = 1; // dummy session id
    req_union.pStopReq = &stop_msg;

    status = locClientSendReq(QMI_LOC_STOP_REQ_V02, req_union);
    if (status != eLOC_CLIENT_SUCCESS) {
        LOC_LOGE ("%s]: failed! status %d",
                  __func__, status);
        err = LOCATION_ERROR_GENERAL_FAILURE;
    } else {
        mInSession = false;
        mPowerMode = GNSS_POWER_MODE_INVALID;

        // if engine on never happend, deregister events
        // without waiting for Engine Off
        if (!mEngineOn) {
            registerEventMask(mMask);
        }
    }

    if (adapterResponse != NULL) {
        adapterResponse->returnToSender(err);
    }
    }));
}

void
LocApiV02::startDistanceBasedTracking(uint32_t sessionId,
                                       const LocationOptions& options,
                                       LocApiResponse* adapterResponse)
{
    sendMsg(new LocApiMsg([this, sessionId, options, adapterResponse] () {

    LOC_LOGD("%s] id %u minInterval %u minDistance %u",
             __func__, sessionId, options.minInterval, options.minDistance);
    LocationError err = LOCATION_ERROR_SUCCESS;

    /** start distance based tracking session*/
    locClientStatusEnumType status;
    locClientReqUnionType req_union;
    qmiLocStartDbtReqMsgT_v02 start_dbt_req;
    qmiLocStartDbtIndMsgT_v02 start_dbt_ind;
    memset(&start_dbt_req, 0, sizeof(start_dbt_req));
    memset(&start_dbt_ind, 0, sizeof(start_dbt_ind));

    // request id
    start_dbt_req.reqId = sessionId;

    // distance
    start_dbt_req.minDistance = options.minDistance;

    // time
    uint32_t minInterval = options.minInterval;
    start_dbt_req.maxLatency_valid = 1;
    start_dbt_req.maxLatency = minInterval/1000; //in seconds
    if (0 == start_dbt_req.maxLatency) {
        start_dbt_req.maxLatency = 1; //in seconds
    }

    // type
    start_dbt_req.distanceType = eQMI_LOC_DBT_DISTANCE_TYPE_STRAIGHT_LINE_V02;

    /* original location disabled by default, as the original location is
       the one cached in the modem buffer and its timestamps is not fresh.*/
    start_dbt_req.needOriginLocation = 0;

    start_dbt_req.usageType_valid = 1;
    start_dbt_req.usageType = eQMI_LOC_DBT_USAGE_NAVIGATION_V02;
    req_union.pStartDbtReq = &start_dbt_req;

    status = locSyncSendReq(QMI_LOC_START_DBT_REQ_V02,
                            req_union,
                            LOC_ENGINE_SYNC_REQUEST_TIMEOUT,
                            QMI_LOC_START_DBT_IND_V02,
                            &start_dbt_ind);

    if (eLOC_CLIENT_SUCCESS != status ||
        eQMI_LOC_SUCCESS_V02 != start_dbt_ind.status) {
        LOC_LOGE("%s] failed! status %d ind.status %d",
              __func__, status, start_dbt_ind.status);
    }

    if (adapterResponse != NULL) {
        adapterResponse->returnToSender(err);
    }
    }));
}

void
LocApiV02::stopDistanceBasedTracking(uint32_t sessionId, LocApiResponse* adapterResponse)
{
    sendMsg(new LocApiMsg([this, sessionId, adapterResponse] () {

    LOC_LOGD("%s] id %u", __func__, sessionId);
    LocationError err = LOCATION_ERROR_SUCCESS;

    locClientStatusEnumType status;
    locClientReqUnionType req_union;
    qmiLocStopDbtReqMsgT_v02 stop_dbt_req;
    qmiLocStopDbtIndMsgT_v02 stop_dbt_Ind;
    memset(&stop_dbt_req, 0, sizeof(stop_dbt_req));
    memset(&stop_dbt_Ind, 0, sizeof(stop_dbt_Ind));

    stop_dbt_req.reqId = sessionId;

    req_union.pStopDbtReq = &stop_dbt_req;

    status = locSyncSendReq(QMI_LOC_STOP_DBT_REQ_V02,
                            req_union,
                            LOC_ENGINE_SYNC_REQUEST_TIMEOUT,
                            QMI_LOC_STOP_DBT_IND_V02,
                            &stop_dbt_Ind);

    if (eLOC_CLIENT_SUCCESS != status ||
        eQMI_LOC_SUCCESS_V02 != stop_dbt_Ind.status) {
        LOC_LOGE("%s] failed! status %d ind.status %d",
              __func__, status, stop_dbt_Ind.status);
    }

    adapterResponse->returnToSender(err);
    }));
}

void
LocApiV02::startBatching(uint32_t sessionId,
                          const LocationOptions& options,
                          uint32_t accuracy,
                          uint32_t timeout,
                          LocApiResponse* adapterResponse)
{
    sendMsg(new LocApiMsg([this, sessionId, options, accuracy, timeout, adapterResponse] () {

    LOC_LOGD("%s]: id %u minInterval %u minDistance %u accuracy %u timeout %u",
             __func__, sessionId, options.minInterval, options.minDistance, accuracy, timeout);
    LocationError err = LOCATION_ERROR_SUCCESS;

    setOperationMode(options.mode);

    //get batch size if needs
    if (mBatchSize == 0) {
        if (LOCATION_ERROR_SUCCESS != queryBatchBuffer(mDesiredBatchSize,
                mBatchSize, BATCHING_MODE_ROUTINE)) {
            if (adapterResponse != NULL) {
                adapterResponse->returnToSender(LOCATION_ERROR_GENERAL_FAILURE);
            }
            return;
        }
    }

    qmiLocStartBatchingReqMsgT_v02 startBatchReq;
    memset(&startBatchReq, 0, sizeof(startBatchReq));

    // interval
    startBatchReq.minInterval_valid = 1;
    if (options.minInterval >= FLP_BATCHING_MINIMUN_INTERVAL) {
        startBatchReq.minInterval = options.minInterval;
    } else {
        startBatchReq.minInterval = FLP_BATCHING_MINIMUN_INTERVAL; // 1 second
    }

    // distance
    startBatchReq.minDistance_valid = 1;
    startBatchReq.minDistance = options.minDistance;

    // accuracy
    startBatchReq.horizontalAccuracyLevel_valid = 1;
    switch(accuracy) {
    case 0:
        startBatchReq.horizontalAccuracyLevel = eQMI_LOC_ACCURACY_LOW_V02;
        break;
    case 1:
        startBatchReq.horizontalAccuracyLevel = eQMI_LOC_ACCURACY_MED_V02;
        break;
    case 2:
        startBatchReq.horizontalAccuracyLevel = eQMI_LOC_ACCURACY_HIGH_V02;
        break;
    default:
        startBatchReq.horizontalAccuracyLevel = eQMI_LOC_ACCURACY_LOW_V02;
    }

    // time out
    if (timeout > 0) {
        startBatchReq.fixSessionTimeout_valid = 1;
        startBatchReq.fixSessionTimeout = timeout;
    } else {
        // modem will use the default time out (20 seconds)
        startBatchReq.fixSessionTimeout_valid = 0;
    }

    // batch request id
    startBatchReq.requestId_valid = 1;
    startBatchReq.requestId = sessionId;

    // batch all fixes always
    startBatchReq.batchAllPos_valid = 1;
    startBatchReq.batchAllPos = true;

    LOC_SEND_SYNC_REQ(StartBatching, START_BATCHING, startBatchReq);

    if (!rv) {
        LOC_LOGE("%s] failed!", __func__);
        if (eQMI_LOC_GNSS_DISABLED_V02 == ind.status) {
            err = LOCATION_ERROR_GNSS_DISABLED;
        } else {
            err = LOCATION_ERROR_GENERAL_FAILURE;
        }
    }

    if (adapterResponse != NULL) {
        adapterResponse->returnToSender(err);
    }

    }));
}

void
LocApiV02::stopBatching(uint32_t sessionId, LocApiResponse* adapterResponse)
{
    sendMsg(new LocApiMsg([this, sessionId, adapterResponse] () {

    LOC_LOGD("%s] id %u", __func__, sessionId);
    LocationError err = LOCATION_ERROR_SUCCESS;

    locClientStatusEnumType status;

    qmiLocStopBatchingReqMsgT_v02 stopBatchingReq;
    memset(&stopBatchingReq, 0, sizeof(stopBatchingReq));

    stopBatchingReq.requestId_valid = 1;
    stopBatchingReq.requestId = sessionId;

    stopBatchingReq.batchType_valid = 1;
    stopBatchingReq.batchType = eQMI_LOC_LOCATION_BATCHING_V02;

    LOC_SEND_SYNC_REQ(StopBatching, STOP_BATCHING, stopBatchingReq);

    if (!rv) {
        LOC_LOGE("%s] failed!", __func__);
        err = LOCATION_ERROR_GENERAL_FAILURE;
    }
    if (adapterResponse != NULL) {
        adapterResponse->returnToSender(err);
    }
    }));
}

LocationError
LocApiV02::startOutdoorTripBatchingSync(uint32_t tripDistance, uint32_t tripTbf, uint32_t timeout)
{
    LOC_LOGD("%s]: minInterval %u minDistance %u timeout %u",
             __func__, tripTbf, tripDistance, timeout);
    LocationError err = LOCATION_ERROR_SUCCESS;

    //get batch size if needs
    if (mTripBatchSize == 0) {
        if (LOCATION_ERROR_SUCCESS != queryBatchBuffer(mDesiredTripBatchSize,
            mTripBatchSize, BATCHING_MODE_TRIP)) {
            return LOCATION_ERROR_GENERAL_FAILURE;
        }
    }

    qmiLocStartOutdoorTripBatchingReqMsgT_v02 startOutdoorTripBatchReq;
    memset(&startOutdoorTripBatchReq, 0, sizeof(startOutdoorTripBatchReq));

    if (tripDistance > 0) {
        startOutdoorTripBatchReq.batchDistance = tripDistance;
    } else {
        startOutdoorTripBatchReq.batchDistance = FLP_BATCHING_MIN_TRIP_DISTANCE; // 1 meter
    }

    if (tripTbf >= FLP_BATCHING_MINIMUN_INTERVAL) {
        startOutdoorTripBatchReq.minTimeInterval = tripTbf;
    } else {
        startOutdoorTripBatchReq.minTimeInterval = FLP_BATCHING_MINIMUN_INTERVAL; // 1 second
    }

    // batch all fixes always
    startOutdoorTripBatchReq.batchAllPos_valid = 1;
    startOutdoorTripBatchReq.batchAllPos = true;

    // time out
    if (timeout > 0) {
        startOutdoorTripBatchReq.fixSessionTimeout_valid = 1;
        startOutdoorTripBatchReq.fixSessionTimeout = timeout;
    } else {
        // modem will use the default time out (20 seconds)
        startOutdoorTripBatchReq.fixSessionTimeout_valid = 0;
    }

    LOC_SEND_SYNC_REQ(StartOutdoorTripBatching, START_OUTDOOR_TRIP_BATCHING,
            startOutdoorTripBatchReq);

    if (!rv) {
        LOC_LOGE("%s] failed!", __func__);
        err = LOCATION_ERROR_GENERAL_FAILURE;
    }

    return err;
}

void
LocApiV02::startOutdoorTripBatching(uint32_t tripDistance, uint32_t tripTbf, uint32_t timeout,
                                          LocApiResponse* adapterResponse) {
    sendMsg(new LocApiMsg([this, tripDistance, tripTbf, timeout, adapterResponse] () {
        if (adapterResponse != NULL) {
            adapterResponse->returnToSender(startOutdoorTripBatchingSync(tripDistance,
                                                                         tripTbf,
                                                                         timeout));
        }
    }));
}

void
LocApiV02::reStartOutdoorTripBatching(uint32_t ongoingTripDistance,
                                            uint32_t ongoingTripInterval,
                                            uint32_t batchingTimeout,
                                            LocApiResponse* adapterResponse)
{
    sendMsg(new LocApiMsg([this, ongoingTripDistance, ongoingTripInterval, batchingTimeout,
                           adapterResponse] () {
        LocationError err = LOCATION_ERROR_SUCCESS;

        uint32_t accumulatedDistance = 0;
        uint32_t numOfBatchedPositions = 0;
        queryAccumulatedTripDistanceSync(accumulatedDistance, numOfBatchedPositions);

        if (numOfBatchedPositions) {
            getBatchedTripLocationsSync(numOfBatchedPositions, 0);
        }

        stopOutdoorTripBatchingSync(false);

        err = startOutdoorTripBatchingSync(ongoingTripDistance,
                                           ongoingTripInterval,
                                           batchingTimeout);
        if (adapterResponse != NULL) {
            adapterResponse->returnToSender(err);
        }
    }));
}

LocationError
LocApiV02::stopOutdoorTripBatchingSync(bool deallocBatchBuffer)
{
    LOC_LOGD("%s] dellocBatchBuffer : %d", __func__, deallocBatchBuffer);
    LocationError err = LOCATION_ERROR_SUCCESS;

    locClientStatusEnumType status;

    qmiLocStopBatchingReqMsgT_v02 stopBatchingReq;
    memset(&stopBatchingReq, 0, sizeof(stopBatchingReq));

    stopBatchingReq.batchType_valid = 1;
    stopBatchingReq.batchType = eQMI_LOC_OUTDOOR_TRIP_BATCHING_V02;
    LOC_SEND_SYNC_REQ(StopBatching, STOP_BATCHING, stopBatchingReq);

    if (!rv) {
        LOC_LOGE("%s] failed!", __func__);
        err = LOCATION_ERROR_GENERAL_FAILURE;
        return err;
    }

    if (deallocBatchBuffer) {
        err = releaseBatchBuffer(BATCHING_MODE_TRIP);
    }

    return err;
}

void
LocApiV02::stopOutdoorTripBatching(bool deallocBatchBuffer, LocApiResponse* adapterResponse)
{
    sendMsg(new LocApiMsg([this, deallocBatchBuffer, adapterResponse] () {
        if (adapterResponse != NULL) {
            adapterResponse->returnToSender(stopOutdoorTripBatchingSync(deallocBatchBuffer));
        }
    }));
}

LocationError
LocApiV02::getBatchedLocationsSync(size_t count)
{
    LOC_LOGD("%s] count %zu.", __func__, count);
    LocationError err = LOCATION_ERROR_SUCCESS;

    size_t entriesToReadInTotal = std::min(mBatchSize,count);
    if (entriesToReadInTotal == 0) {
        LOC_LOGD("%s] No batching memory allocated in modem or nothing to read", __func__);
        // calling the base class
        reportLocations(NULL, 0, BATCHING_MODE_ROUTINE);
    } else {
        size_t entriesToRead =
            std::min(entriesToReadInTotal, (size_t)QMI_LOC_READ_FROM_BATCH_MAX_SIZE_V02);
        size_t entriesGotInTotal = 0;
        size_t entriesGotInEachTime = 0;
        //allocate +1 just in case we get one more location added during read
        Location* pLocationsFromModem = new Location[entriesToReadInTotal+1]();
        if (pLocationsFromModem == nullptr) {
            LOC_LOGE("new allocation failed, fatal error.");
            return LOCATION_ERROR_GENERAL_FAILURE;
        }
        memset(pLocationsFromModem, 0, sizeof(Location)*(entriesToReadInTotal+1));
        Location* tempLocationP = new Location[QMI_LOC_READ_FROM_BATCH_MAX_SIZE_V02]();
        if (tempLocationP == nullptr) {
            LOC_LOGE("new allocation failed, fatal error.");
            return LOCATION_ERROR_GENERAL_FAILURE;
        }
        do {
            memset(tempLocationP, 0, QMI_LOC_READ_FROM_BATCH_MAX_SIZE_V02*sizeof(Location));
            readModemLocations(tempLocationP,
                               entriesToRead,
                               BATCHING_MODE_ROUTINE,
                               entriesGotInEachTime);
            for (size_t i = 0; i<entriesGotInEachTime; i++) {
                size_t index = entriesToReadInTotal-entriesGotInTotal-i;

                // make sure index is not too large or small to fit in the array
                if (index <= entriesToReadInTotal && (int)index >= 0) {
                    pLocationsFromModem[index] = tempLocationP[i];
                } else {
                    LOC_LOGW("%s] dropped an unexpected location.", __func__);
                }
            }
            entriesGotInTotal += entriesGotInEachTime;
            entriesToRead = std::min(entriesToReadInTotal - entriesGotInTotal,
                                     (size_t)QMI_LOC_READ_FROM_BATCH_MAX_SIZE_V02);
        } while (entriesGotInEachTime > 0 && entriesToRead > 0);
        delete[] tempLocationP;

        LOC_LOGD("%s] Read out %zu batched locations from modem in total.",
                 __func__, entriesGotInTotal);

        // offset is by default 1 because we allocated one extra location during read
        uint32_t offset = 1;
        if (entriesToReadInTotal > entriesGotInTotal) {
            offset = 1 + entriesToReadInTotal - entriesGotInTotal;
            LOC_LOGD("%s] offset %u", __func__, offset);
        } else if (entriesGotInTotal > entriesToReadInTotal) {
            // offset is zero because we need one extra slot for the extra location
            offset = 0;
            LOC_LOGD("%s] Read %zu extra location(s) than expected.",
                     __func__, entriesGotInTotal - entriesToReadInTotal);
            // we got one extra location added during modem read, so one location will
            // be out of order and needs to be found and put in order
            int64_t currentTimeStamp = pLocationsFromModem[entriesToReadInTotal].timestamp;
            for (int i=entriesToReadInTotal-1; i >= 0; i--) {
                // find the out of order location
                if (currentTimeStamp < pLocationsFromModem[i].timestamp) {
                    LOC_LOGD("%s] Out of order location is index %d timestamp %" PRIu64,
                             __func__, i, pLocationsFromModem[i].timestamp);
                    // save the out of order location to be put at end of array
                    Location tempLocation(pLocationsFromModem[i]);
                    // shift the remaining locations down one
                    for (size_t j=i; j<entriesToReadInTotal; j++) {
                        pLocationsFromModem[j] = pLocationsFromModem[j+1];
                    }
                    // put the out of order location at end of array now
                    pLocationsFromModem[entriesToReadInTotal] = tempLocation;
                    break;
                } else {
                    currentTimeStamp = pLocationsFromModem[i].timestamp;
                }
            }
        }

        LOC_LOGD("%s] Calling reportLocations with count:%zu and entriesGotInTotal:%zu",
                  __func__, count, entriesGotInTotal);

        // calling the base class
        reportLocations(pLocationsFromModem+offset, entriesGotInTotal, BATCHING_MODE_ROUTINE);
        delete[] pLocationsFromModem;
    }
    return err;

}

void
LocApiV02::getBatchedLocations(size_t count, LocApiResponse* adapterResponse)
{
    sendMsg(new LocApiMsg([this, count, adapterResponse] () {
          if (adapterResponse != NULL) {
              adapterResponse->returnToSender(getBatchedLocationsSync(count));
          }
    }));
}

LocationError LocApiV02::getBatchedTripLocationsSync(size_t count, uint32_t accumulatedDistance)
{
    LocationError err = LOCATION_ERROR_SUCCESS;
    size_t idxLocationFromModem = 0;

    size_t entriesToReadInTotal = std::min(mTripBatchSize, count);
    if (entriesToReadInTotal == 0) {
        LOC_LOGD("%s] No trip batching memory allocated in modem or nothing to read", __func__);
        // calling the base class
        reportLocations(NULL, 0, BATCHING_MODE_TRIP);
    } else {
        size_t entriesToRead =
                std::min(entriesToReadInTotal, (size_t)QMI_LOC_READ_FROM_BATCH_MAX_SIZE_V02);
        size_t entriesGotInTotal = 0;
        size_t entriesGotInEachTime = 0;

        Location* pLocationsFromModem = new Location[entriesToReadInTotal]();
        if (pLocationsFromModem == nullptr) {
            LOC_LOGE("new allocation failed, fatal error.");
            return LOCATION_ERROR_GENERAL_FAILURE;
        }
        memset(pLocationsFromModem, 0, sizeof(Location)*(entriesToReadInTotal));
        Location* tempLocationP = new Location[QMI_LOC_READ_FROM_BATCH_MAX_SIZE_V02]();
        if (tempLocationP == nullptr) {
            LOC_LOGE("new allocation failed, fatal error.");
            return LOCATION_ERROR_GENERAL_FAILURE;
        }

        do {
            memset(tempLocationP, 0, QMI_LOC_READ_FROM_BATCH_MAX_SIZE_V02*sizeof(Location));
            readModemLocations(tempLocationP,
                               entriesToRead,
                               BATCHING_MODE_TRIP,
                               entriesGotInEachTime);
            for (size_t iEntryIndex = 0; iEntryIndex<entriesGotInEachTime;
                    iEntryIndex++, idxLocationFromModem++) {
                // make sure index is not too large fit in the array
                if (idxLocationFromModem < entriesToReadInTotal) {
                    pLocationsFromModem[idxLocationFromModem] = tempLocationP[iEntryIndex];
                } else {
                    LOC_LOGW("%s] dropped an unexpected location.", __func__);
                }
            }
            entriesGotInTotal += entriesGotInEachTime;
            entriesToRead = std::min(entriesToReadInTotal - entriesGotInTotal,
                                     (size_t)QMI_LOC_READ_FROM_BATCH_MAX_SIZE_V02);
        } while (entriesGotInEachTime > 0 && entriesToRead > 0);
        delete[] tempLocationP;

        LOC_LOGD("%s] Calling reportLocations with count:%zu and entriesGotInTotal:%zu",
                  __func__, count, entriesGotInTotal);

        // calling the base class
        reportLocations(pLocationsFromModem, entriesGotInTotal, BATCHING_MODE_TRIP);

        if (accumulatedDistance != 0) {
            LOC_LOGD("%s] Calling reportCompletedTrips with distance %u:",
                    __func__, accumulatedDistance);
            reportCompletedTrips(accumulatedDistance);
        }

        delete[] pLocationsFromModem;
    }

    return err;
}

void LocApiV02::getBatchedTripLocations(size_t count, uint32_t accumulatedDistance,
                                              LocApiResponse* adapterResponse)
{
    sendMsg(new LocApiMsg([this, count, accumulatedDistance, adapterResponse] () {
        if (adapterResponse != NULL) {
            adapterResponse->returnToSender(getBatchedTripLocationsSync(count, accumulatedDistance));
        }
    }));
}

void
LocApiV02::readModemLocations(Location* pLocationPiece,
                              size_t count,
                              BatchingMode batchingMode,
                              size_t& numbOfEntries)
{
    LOC_LOGD("%s] count %zu.", __func__, count);
    numbOfEntries = 0;
    qmiLocReadFromBatchReqMsgT_v02 getBatchLocatonReq;
    memset(&getBatchLocatonReq, 0, sizeof(getBatchLocatonReq));

    if (count != 0) {
        getBatchLocatonReq.numberOfEntries = count;
        getBatchLocatonReq.transactionId = 1;
    }

    getBatchLocatonReq.batchType_valid = 1;
    getBatchLocatonReq.batchType = ((batchingMode == BATCHING_MODE_ROUTINE) ?
            eQMI_LOC_LOCATION_BATCHING_V02: eQMI_LOC_OUTDOOR_TRIP_BATCHING_V02);

    LOC_SEND_SYNC_REQ(ReadFromBatch, READ_FROM_BATCH, getBatchLocatonReq);

    if (!rv) {
        LOC_LOGE("%s] Reading batched locations from modem failed.", __func__);
        return;
    }
    if (ind.numberOfEntries_valid != 0 &&
        ind.batchedReportList_valid != 0) {
        for (uint32_t i=0; i<ind.numberOfEntries; i++) {
            Location temp;
            memset (&temp, 0, sizeof(temp));
            temp.size = sizeof(Location);
            if ((ind.batchedReportList[i].validFields &
                 QMI_LOC_BATCHED_REPORT_MASK_VALID_LATITUDE_V02) &&
                (ind.batchedReportList[i].validFields &
                 QMI_LOC_BATCHED_REPORT_MASK_VALID_LONGITUDE_V02)) {
                temp.latitude = ind.batchedReportList[i].latitude;
                temp.longitude = ind.batchedReportList[i].longitude;
                temp.flags |= LOCATION_HAS_LAT_LONG_BIT;
            }
            if (ind.batchedReportList[i].validFields &
                QMI_LOC_BATCHED_REPORT_MASK_VALID_ALT_WRT_ELP_V02) {
                temp.altitude = ind.batchedReportList[i].altitudeWrtEllipsoid;
                temp.flags |= LOCATION_HAS_ALTITUDE_BIT;
            }
            if (ind.batchedReportList[i].validFields &
                QMI_LOC_BATCHED_REPORT_MASK_VALID_TIMESTAMP_UTC_V02) {
                temp.timestamp = ind.batchedReportList[i].timestampUtc;
            }
            if (ind.batchedReportList[i].validFields &
                QMI_LOC_BATCHED_REPORT_MASK_VALID_HOR_CIR_UNC_V02) {
                temp.accuracy = ind.batchedReportList[i].horUncCircular;
                temp.flags |= LOCATION_HAS_ACCURACY_BIT;
            }
            if (ind.batchedReportList[i].validFields &
                QMI_LOC_BATCHED_REPORT_MASK_VALID_HEADING_V02) {
                temp.bearing = ind.batchedReportList[i].heading;
                temp.flags |= LOCATION_HAS_BEARING_BIT;
            }
            if (ind.batchedReportList[i].validFields &
                QMI_LOC_BATCHED_REPORT_MASK_VALID_SPEED_HOR_V02) {
                temp.speed = ind.batchedReportList[i].speedHorizontal;
                temp.flags |= LOCATION_HAS_SPEED_BIT;
            }
            if (ind.batchedReportList[i].validFields &
                QMI_LOC_BATCHED_REPORT_MASK_VALID_TECH_MASK_V02) {
                if (QMI_LOC_POS_TECH_MASK_SATELLITE_V02 & ind.batchedReportList[i].technologyMask) {
                    temp.techMask |= LOCATION_TECHNOLOGY_GNSS_BIT;
                }
                if (QMI_LOC_POS_TECH_MASK_CELLID_V02 & ind.batchedReportList[i].technologyMask) {
                    temp.techMask |= LOCATION_TECHNOLOGY_CELL_BIT;
                }
                if (QMI_LOC_POS_TECH_MASK_WIFI_V02 & ind.batchedReportList[i].technologyMask) {
                    temp.techMask |= LOCATION_TECHNOLOGY_WIFI_BIT;
                }
                if (QMI_LOC_POS_TECH_MASK_SENSORS_V02 & ind.batchedReportList[i].technologyMask) {
                    temp.techMask |= LOCATION_TECHNOLOGY_SENSORS_BIT;
                }
            }
            if (ind.batchedReportList[i].validFields &
                QMI_LOC_BATCHED_REPORT_MASK_VALID_VERT_UNC_V02) {
                temp.verticalAccuracy = ind.batchedReportList[i].vertUnc;
                temp.flags |= LOCATION_HAS_VERTICAL_ACCURACY_BIT;
            }
            if (ind.batchedReportList[i].validFields &
                QMI_LOC_BATCHED_REPORT_MASK_VALID_SPEED_UNC_V02) {
                temp.speedAccuracy = ind.batchedReportList[i].speedUnc;
                temp.flags |= LOCATION_HAS_SPEED_ACCURACY_BIT;
            }
            if (ind.batchedReportList[i].validFields &
                QMI_LOC_BATCHED_REPORT_MASK_VALID_HEADING_UNC_V02) {
                temp.bearingAccuracy = ind.batchedReportList[i].headingUnc;
                temp.flags |= LOCATION_HAS_BEARING_ACCURACY_BIT;
            }
            pLocationPiece[i] = temp;
        }
        numbOfEntries = ind.numberOfEntries;
        LOC_LOGD("%s] Read out %zu batched locations from modem.",
                 __func__, numbOfEntries);
    } else {
        LOC_LOGD("%s] Modem does not return batched location.", __func__);
    }
}

LocationError LocApiV02::queryAccumulatedTripDistanceSync(uint32_t &accumulatedTripDistance,
        uint32_t &numOfBatchedPositions)
{
    locClientStatusEnumType st = eLOC_CLIENT_SUCCESS;
    locClientReqUnionType req_union;
    locClientStatusEnumType status;

    qmiLocQueryOTBAccumulatedDistanceReqMsgT_v02 accumulated_distance_req;
    qmiLocQueryOTBAccumulatedDistanceIndMsgT_v02 accumulated_distance_ind;

    memset(&accumulated_distance_req,0,sizeof(accumulated_distance_req));
    memset(&accumulated_distance_ind, 0, sizeof(accumulated_distance_ind));

    req_union.pQueryOTBAccumulatedDistanceReq = &accumulated_distance_req;

    status = locSyncSendReq(QMI_LOC_QUERY_OTB_ACCUMULATED_DISTANCE_REQ_V02,
                            req_union,
                            LOC_ENGINE_SYNC_REQUEST_TIMEOUT,
                            QMI_LOC_QUERY_OTB_ACCUMULATED_DISTANCE_IND_V02,
                            &accumulated_distance_ind);
    if ((eLOC_CLIENT_SUCCESS != status) ||
        (eQMI_LOC_SUCCESS_V02 != accumulated_distance_ind.status))
    {
        LOC_LOGE ("%s:%d]: error! status = %d, accumulated_distance_ind.status = %d\n",
                __func__, __LINE__,
                (status),
                (accumulated_distance_ind.status));
        return LOCATION_ERROR_GENERAL_FAILURE;
    }
    else
    {
        LOC_LOGD("Got accumulated distance: %u number of accumulated positions %u",
        accumulated_distance_ind.accumulatedDistance, accumulated_distance_ind.batchedPosition);

        accumulatedTripDistance = accumulated_distance_ind.accumulatedDistance;
        numOfBatchedPositions = accumulated_distance_ind.batchedPosition;

        return LOCATION_ERROR_SUCCESS;
    }
}

void LocApiV02::queryAccumulatedTripDistance(
        LocApiResponseData<LocApiBatchData>* adapterResponseData)
{
    sendMsg(new LocApiMsg([this, adapterResponseData] () {
        LocationError err = LOCATION_ERROR_SUCCESS;
        LocApiBatchData data;
        err = queryAccumulatedTripDistanceSync(data.accumulatedDistance,
                                               data.numOfBatchedPositions);
        if (adapterResponseData != NULL) {
            adapterResponseData->returnToSender(err, data);
        }
    }));
}

void LocApiV02::addToCallQueue(LocApiResponse* adapterResponse)
{
    sendMsg(new LocApiMsg([adapterResponse] () {
        if (adapterResponse != NULL) {
            adapterResponse->returnToSender(LOCATION_ERROR_SUCCESS);
        }
    }));
}

/* convert dgnss constellation mask from QMI loc to loc eng format */
void LocApiV02::convertGnssConestellationMask(
        qmiLocGNSSConstellEnumT_v02 qmiConstellationEnum,
        GnssConstellationTypeMask& constellationMask) {

    constellationMask = 0x0;
    if (qmiConstellationEnum & eQMI_SYSTEM_GPS_V02) {
        constellationMask |= GNSS_CONSTELLATION_TYPE_GPS_BIT;
    }

    if (qmiConstellationEnum & eQMI_SYSTEM_GLO_V02) {
        constellationMask |= GNSS_CONSTELLATION_TYPE_GLONASS_BIT;
    }

    if (qmiConstellationEnum & eQMI_SYSTEM_BDS_V02) {
        constellationMask |= GNSS_CONSTELLATION_TYPE_BEIDOU_BIT;
    }

    if (qmiConstellationEnum & eQMI_SYSTEM_GAL_V02) {
        constellationMask |= GNSS_CONSTELLATION_TYPE_GALILEO_BIT;
    }

    if (qmiConstellationEnum & eQMI_SYSTEM_QZSS_V02) {
        constellationMask |= GNSS_CONSTELLATION_TYPE_QZSS_BIT;
    }

    if (qmiConstellationEnum & eQMI_SYSTEM_NAVIC_V02) {
        constellationMask |= GNSS_CONSTELLATION_TYPE_NAVIC_BIT;
    }
}

GnssSignalTypeMask LocApiV02::convertQmiGnssSignalType(
        qmiLocGnssSignalTypeMaskT_v02 qmiGnssSignalType) {
    GnssSignalTypeMask gnssSignalType = (GnssSignalTypeMask)0;

    switch (qmiGnssSignalType) {
    case QMI_LOC_MASK_GNSS_SIGNAL_TYPE_GPS_L1CA_V02:
        gnssSignalType = GNSS_SIGNAL_GPS_L1CA;
        break;
    case QMI_LOC_MASK_GNSS_SIGNAL_TYPE_GPS_L1C_V02:
        gnssSignalType = GNSS_SIGNAL_GPS_L1C;
        break;
    case QMI_LOC_MASK_GNSS_SIGNAL_TYPE_GPS_L2C_L_V02:
        gnssSignalType = GNSS_SIGNAL_GPS_L2;
        break;
    case QMI_LOC_MASK_GNSS_SIGNAL_TYPE_GPS_L5_Q_V02:
        gnssSignalType = GNSS_SIGNAL_GPS_L5;
        break;
    case QMI_LOC_MASK_GNSS_SIGNAL_TYPE_GLONASS_G1_V02:
        gnssSignalType = GNSS_SIGNAL_GLONASS_G1;
        break;
    case QMI_LOC_MASK_GNSS_SIGNAL_TYPE_GLONASS_G2_V02:
        gnssSignalType = GNSS_SIGNAL_GLONASS_G2;
        break;
    case QMI_LOC_MASK_GNSS_SIGNAL_TYPE_GALILEO_E1_C_V02:
        gnssSignalType = GNSS_SIGNAL_GALILEO_E1;
        break;
    case QMI_LOC_MASK_GNSS_SIGNAL_TYPE_GALILEO_E5A_Q_V02:
        gnssSignalType = GNSS_SIGNAL_GALILEO_E5A;
        break;
    case QMI_LOC_MASK_GNSS_SIGNAL_TYPE_GALILEO_E5B_Q_V02:
        gnssSignalType = GNSS_SIGNAL_GALILEO_E5B;
        break;
    case QMI_LOC_MASK_GNSS_SIGNAL_TYPE_BEIDOU_B1_I_V02:
        gnssSignalType = GNSS_SIGNAL_BEIDOU_B1I;
        break;
    case QMI_LOC_MASK_GNSS_SIGNAL_TYPE_BEIDOU_B1C_V02:
        gnssSignalType = GNSS_SIGNAL_BEIDOU_B1C;
        break;
    case QMI_LOC_MASK_GNSS_SIGNAL_TYPE_BEIDOU_B2_I_V02:
        gnssSignalType = GNSS_SIGNAL_BEIDOU_B2I;
        break;
    case QMI_LOC_MASK_GNSS_SIGNAL_TYPE_BEIDOU_B2A_I_V02:
        gnssSignalType = GNSS_SIGNAL_BEIDOU_B2AI;
        break;
    case QMI_LOC_MASK_GNSS_SIGNAL_TYPE_QZSS_L1CA_V02:
        gnssSignalType = GNSS_SIGNAL_QZSS_L1CA;
        break;
    case QMI_LOC_MASK_GNSS_SIGNAL_TYPE_QZSS_L1S_V02:
        gnssSignalType =  GNSS_SIGNAL_QZSS_L1S;
        break;
    case QMI_LOC_MASK_GNSS_SIGNAL_TYPE_QZSS_L2C_L_V02:
        gnssSignalType = GNSS_SIGNAL_QZSS_L2;
        break;
    case QMI_LOC_MASK_GNSS_SIGNAL_TYPE_QZSS_L5_Q_V02:
        gnssSignalType = GNSS_SIGNAL_QZSS_L5;
        break;
    case QMI_LOC_MASK_GNSS_SIGNAL_TYPE_SBAS_L1_CA_V02:
        gnssSignalType = GNSS_SIGNAL_SBAS_L1;
        break;
    case QMI_LOC_MASK_GNSS_SIGNAL_TYPE_NAVIC_L5_V02:
        gnssSignalType = GNSS_SIGNAL_NAVIC_L5;
        break;
    case QMI_LOC_MASK_GNSS_SIGNAL_TYPE_BEIDOU_B2A_Q_V02:
        gnssSignalType = GNSS_SIGNAL_BEIDOU_B2AQ;
        break;
    default:
        break;
    }

    return gnssSignalType;
}
