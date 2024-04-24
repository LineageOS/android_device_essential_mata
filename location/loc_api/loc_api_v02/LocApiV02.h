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
 */

 /*
Changes from Qualcomm Innovation Center are provided under the following license:

Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted (subject to the limitations in the
disclaimer below) provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.

    * Redistributions in binary form must reproduce the above
      copyright notice, this list of conditions and the following
      disclaimer in the documentation and/or other materials provided
      with the distribution.

    * Neither the name of Qualcomm Innovation Center, Inc. nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE
GRANTED BY THIS LICENSE. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT
HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef LOC_API_V_0_2_H
#define LOC_API_V_0_2_H

#include <stdint.h>
#include <stdbool.h>
#include <LocApiBase.h>
#include <loc_api_v02_client.h>
#include <vector>
#include <functional>
#include <unordered_map>

#define LOC_SEND_SYNC_REQ(NAME, ID, REQ)  \
    int rv = true; \
    locClientStatusEnumType st; \
    locClientReqUnionType reqUnion; \
    qmiLoc##NAME##IndMsgT_v02 ind; \
\
    memset(&ind, 0, sizeof(ind)); \
    reqUnion.p##NAME##Req = &REQ; \
\
    st = locSyncSendReq(QMI_LOC_##ID##_REQ_V02,          \
                        reqUnion,                        \
                        LOC_ENGINE_SYNC_REQUEST_TIMEOUT, \
                        QMI_LOC_##ID##_IND_V02,          \
                        &ind);                           \
\
    if (st != eLOC_CLIENT_SUCCESS || \
        eQMI_LOC_SUCCESS_V02 != ind.status) { \
        LOC_LOGE ("%s:%d]: Error : st = %d, ind.status = %d", \
                  __func__, __LINE__,  st, ind.status); \
        rv = false; \
    }

using Resender = std::function<void()>;
using namespace loc_core;

typedef struct
{
    uint32_t counter;
    qmiLocSvSystemEnumT_v02 system;
    qmiLocGnssSignalTypeMaskT_v02 gnssSignalType;
    uint16_t gnssSvId;
    qmiLocMeasFieldsValidMaskT_v02 validMask;
    uint8_t cycleSlipCount;
} adrData;

typedef uint64_t GpsSvMeasHeaderFlags;
#define BIAS_GPSL1_VALID                0x00000001
#define BIAS_GPSL1_UNC_VALID            0x00000002
#define BIAS_GPSL1_GPSL5_VALID          0x00000004
#define BIAS_GPSL1_GPSL5_UNC_VALID      0x00000008
#define BIAS_GPSL1_GLOG1_VALID          0x00000010
#define BIAS_GPSL1_GLOG1_UNC_VALID      0x00000020
#define BIAS_GPSL1_GALE1_VALID          0x00000040
#define BIAS_GPSL1_GALE1_UNC_VALID      0x00000080
#define BIAS_GPSL1_BDSB1_VALID          0x00000100
#define BIAS_GPSL1_BDSB1_UNC_VALID      0x00000200
#define BIAS_GPSL1_NAVIC_VALID          0x00000400
#define BIAS_GPSL1_NAVIC_UNC_VALID      0x00000800

#define BIAS_GALE1_VALID                0x00001000
#define BIAS_GALE1_UNC_VALID            0x00002000
#define BIAS_GALE1_GALE5A_VALID         0x00004000
#define BIAS_GALE1_GALE5A_UNC_VALID     0x00008000
#define BIAS_BDSB1_VALID                0x00010000
#define BIAS_BDSB1_UNC_VALID            0x00020000
#define BIAS_BDSB1_BDSB1C_VALID         0x00040000
#define BIAS_BDSB1_BDSB1C_UNC_VALID     0x00080000
#define BIAS_BDSB1_BDSB2A_VALID         0x00100000
#define BIAS_BDSB1_BDSB2A_UNC_VALID     0x00200000

typedef struct {
    uint64_t flags;

    /* used directly */
    float gpsL1;
    float gpsL1Unc;
    float gpsL1_gpsL5;
    float gpsL1_gpsL5Unc;
    float gpsL1_gloG1;
    float gpsL1_gloG1Unc;
    float gpsL1_galE1;
    float gpsL1_galE1Unc;
    float gpsL1_bdsB1;
    float gpsL1_bdsB1Unc;
    float gpsL1_navic;
    float gpsL1_navicUnc;

    /* used for intermediate computations */
    float galE1;
    float galE1Unc;
    float galE1_galE5a;
    float galE1_galE5aUnc;
    float bdsB1;
    float bdsB1Unc;
    float bdsB1_bdsB1c;
    float bdsB1_bdsB1cUnc;
    float bdsB1_bdsB2a;
    float bdsB1_bdsB2aUnc;
} timeBiases;

/* This class derives from the LocApiBase class.
   The members of this class are responsible for converting
   the Loc API V02 data structures into Loc Adapter data structures.
   This class also implements some of the virtual functions that
   handle the requests from loc engine. */
class LocApiV02 : public LocApiBase {
protected:
  /* loc api v02 handle*/
  locClientHandleType clientHandle;

private:
  locClientEventMaskType mQmiMask;
  bool mInSession;
  GnssPowerMode mPowerMode;
  bool mEngineOn;
  bool mMeasurementsStarted;
  std::vector<Resender> mResenders;
  bool mMasterRegisterNotSupported;
  uint32_t mCounter;
  uint32_t mMinInterval;
  std::vector<adrData>  mADRdata;
  GnssMeasurements*  mGnssMeasurements;
  bool mGPSreceived;
  int  mMsInWeek;
  bool mAgcIsPresent;
  timeBiases mTimeBiases;

  size_t mBatchSize, mDesiredBatchSize;
  size_t mTripBatchSize, mDesiredTripBatchSize;
  bool mIsFirstFinalFixReported;
  bool mIsFirstStartFixReq;
  uint64_t mHlosQtimer1, mHlosQtimer2;
  uint32_t mRefFCount;

  // Below two member variables are for elapsedRealTime calculation
  ElapsedRealtimeEstimator mMeasElapsedRealTimeCal;
  ElapsedRealtimeEstimator mPositionElapsedRealTimeCal;

  /* Convert event mask from loc eng to loc_api_v02 format */
  static locClientEventMaskType convertMask(LOC_API_ADAPTER_EVENT_MASK_T mask);

  /* Convert GPS LOCK from LocationAPI format to QMI format */
  static qmiLocLockEnumT_v02 convertGpsLockFromAPItoQMI(GnssConfigGpsLock lock);

  /* Convert GPS LOCK from QMI format to LocationAPI format */
  static GnssConfigGpsLock convertGpsLockFromQMItoAPI(qmiLocLockEnumT_v02 lock);

  /* Convert Engine Lock State from QMI format to LocationAPI format */
  static EngineLockState convertEngineLockState(qmiLocEngineLockStateEnumT_v02 LockState);

  /* Convert error from loc_api_v02 to loc eng format*/
  static enum loc_api_adapter_err convertErr(locClientStatusEnumType status);

  /* convert Ni Encoding type from QMI_LOC to loc eng format */
  static GnssNiEncodingType convertNiEncoding(
    qmiLocNiDataCodingSchemeEnumT_v02 loc_encoding);

  /*convert NI notify verify type from QMI LOC to loc eng format*/
  static bool convertNiNotifyVerifyType (GnssNiNotification *notif,
      qmiLocNiNotifyVerifyEnumT_v02 notif_priv);

  /*convert signal type to carrier frequency*/
  static float convertSignalTypeToCarrierFrequency(
      qmiLocGnssSignalTypeMaskT_v02 signalType,
      uint8_t gloFrequency);

  /*convert GnssMeasurement type from QMI LOC to loc eng format*/
  bool convertGnssMeasurements (
      const qmiLocEventGnssSvMeasInfoIndMsgT_v02& gnss_measurement_report_ptr,
      int index, bool isExt, bool validDgnssSvMeas);

  /* Convert APN Type mask */
  static qmiLocApnTypeMaskT_v02 convertLocApnTypeMask(LocApnTypeMask mask);
  static LocApnTypeMask convertQmiLocApnTypeMask(qmiLocApnTypeMaskT_v02 mask);

  /* Convert Get Constellation QMI Ind info to GnssSvTypeConfig */
  static void convertToGnssSvTypeConfig(
          const qmiLocGetConstellationConfigIndMsgT_v02& ind,
          GnssSvTypeConfig& config);

  /* Convert GnssPowerMode to QMI Loc Power Mode Enum */
  static qmiLocPowerModeEnumT_v02 convertPowerMode(GnssPowerMode powerMode);

  void convertGnssMeasurementsHeader(const Gnss_LocSvSystemEnumType locSvSystemType,
      const qmiLocEventGnssSvMeasInfoIndMsgT_v02& gnss_measurement_info);

  /*convert LocGnssClock type from QMI LOC to loc eng format*/
  int convertGnssClock (GnssMeasurementsClock& clock,
      const qmiLocEventGnssSvMeasInfoIndMsgT_v02& gnss_measurement_info);

  /* convert dgnss constellation mask from QMI loc to loc eng format */
  static void convertGnssConestellationMask (
            qmiLocGNSSConstellEnumT_v02 qmiConstellationEnum,
            GnssConstellationTypeMask& constellationMask);

  /* If Confidence value is less than 68%, then scale the accuracy value to 68%
     confidence.*/
  void scaleAccuracyTo68PercentConfidence(const uint8_t confidenceValue,
                                          LocGpsLocation &gpsLocation,
                                          const bool isCircularUnc);

  /* convert position report to loc eng format and send the converted
     position to loc eng */
  void reportPosition
    (const qmiLocEventPositionReportIndMsgT_v02 *location_report_ptr,
     bool unpropagatedPosition = false);

  /* convert satellite report to loc eng format and  send the converted
     report to loc eng */
  void reportSv (const qmiLocEventGnssSvInfoIndMsgT_v02 *gnss_report_ptr);

  void  reportSvPolynomial (
  const qmiLocEventGnssSvPolyIndMsgT_v02 *gnss_sv_poly_ptr);

  void reportSvEphemeris (
  uint32_t eventId, const locClientEventIndUnionType &eventPayload);

  void populateGpsEphemeris(const qmiLocGpsEphemerisReportIndMsgT_v02 *,
          GnssSvEphemerisReport &);
  void populateGlonassEphemeris(const qmiLocGloEphemerisReportIndMsgT_v02 *,
          GnssSvEphemerisReport &);
  void populateBdsEphemeris(const qmiLocBdsEphemerisReportIndMsgT_v02 *,
          GnssSvEphemerisReport &);
  void populateGalEphemeris(const qmiLocGalEphemerisReportIndMsgT_v02 *,
          GnssSvEphemerisReport &);
  void populateQzssEphemeris(const qmiLocQzssEphemerisReportIndMsgT_v02 *,
          GnssSvEphemerisReport &);
  void populateCommonEphemeris(const qmiLocEphGnssDataStructT_v02 &, GnssEphCommon &);
  void populateGpsTimeOfReport(const qmiLocGnssTimeStructT_v02 &, GnssSystemTimeStructType &);

  void populateFeatureStatusReport(const qmiLocFeaturesStatusMaskT_v02 &featureStatusReport,
        std::unordered_map<LocationQwesFeatureType, bool> &featureMap);
  void reportLocEvent(const qmiLocEventReportIndMsgT_v02 *event_report_ptr);
  /* convert system info to location api format and dispatch to
     the registered adapter */
  void reportSystemInfo(const qmiLocSystemInfoIndMsgT_v02* system_info_ptr);
  void reportLocationRequestNotification(
      const qmiLocLocationRequestNotificationIndMsgT_v02* loc_req_notif);

  /* convert engine state report to loc eng format and send the converted
     report to loc eng */
  void reportEngineState (
    const qmiLocEventEngineStateIndMsgT_v02 *engine_state_ptr);

  /* convert fix session report to loc eng format and send the converted
     report to loc eng */
  void reportFixSessionState (
    const qmiLocEventFixSessionStateIndMsgT_v02 *fix_session_state_ptr);

  /* convert NMEA report to loc eng format and send the converted
     report to loc eng */
  void reportNmea (const qmiLocEventNmeaIndMsgT_v02 *nmea_report_ptr);

  /* convert and report an ATL request to loc engine */
  void reportAtlRequest(
    const qmiLocEventLocationServerConnectionReqIndMsgT_v02
    *server_request_ptr);

  /* convert and report NI request to loc eng */
  void reportNiRequest(
    const qmiLocEventNiNotifyVerifyReqIndMsgT_v02 *ni_req_ptr);

  /* report the xtra server info */
  void reportXtraServerUrl(
    const qmiLocEventInjectPredictedOrbitsReqIndMsgT_v02* server_request_ptr);

  /* convert and report GNSS measurement data to loc eng */
  void reportGnssMeasurementData(
    const qmiLocEventGnssSvMeasInfoIndMsgT_v02& gnss_measurement_report_ptr);

  void reportSvMeasurementInternal();

  inline void resetSvMeasurementReport(){
      memset(mGnssMeasurements, 0, sizeof(GnssMeasurements));
      mGnssMeasurements->size = sizeof(GnssMeasurements);
      mGnssMeasurements->gnssSvMeasurementSet.size = sizeof(GnssSvMeasurementSet);
      mGnssMeasurements->gnssSvMeasurementSet.isNhz = false;
      mGnssMeasurements->gnssSvMeasurementSet.svMeasSetHeader.size =
          sizeof(GnssSvMeasurementHeader);
      memset(&mTimeBiases, 0, sizeof(mTimeBiases));
      mGPSreceived = false;
      mMsInWeek = -1;
      mAgcIsPresent = false;
  }

  void setGnssBiases();

  /* convert and report ODCPI request */
  void requestOdcpi(
    const qmiLocEventWifiReqIndMsgT_v02& odcpiReq);

  void registerEventMask(LOC_API_ADAPTER_EVENT_MASK_T adapterMask);
  bool sendRequestForAidingData(locClientEventMaskType qmiMask);
  locClientEventMaskType adjustMaskIfNoSessionOrEngineOff(locClientEventMaskType qmiMask);
  bool cacheGnssMeasurementSupport();
  void registerMasterClient();
  int getGpsLock(uint8_t subType);
  void getEngineLockStateSync();
  void getRobustLocationConfig(uint32_t sessionId, LocApiResponse* adapterResponse);
  void getMinGpsWeek(uint32_t sessionId, LocApiResponse* adapterResponse);

  /* Convert get blacklist sv info to GnssSvIdConfig */
  void reportGnssSvIdConfig
    (const qmiLocGetBlacklistSvIndMsgT_v02& getBlacklistSvIndMsg);

  /* Convert get constellation info to GnssSvTypeConfig */
  void reportGnssSvTypeConfig
    (const qmiLocGetConstellationConfigIndMsgT_v02& getConstellationConfigIndMsg);

  /* Inform ODCPI availability to Modem */
  void wifiStatusInformSync();

  void sendNfwNotification(GnssNfwNotification& notification);
  LocationError queryBatchBuffer(size_t desiredSize,
          size_t &allocatedSize, BatchingMode batchMode);
  LocationError releaseBatchBuffer(BatchingMode batchMode);
  void readModemLocations(Location* pLocationPiece, size_t count,
          BatchingMode batchingMode, size_t& numbOfEntries);
  void setOperationMode(GnssSuplMode mode);
  bool needsNewTripBatchRestart(uint32_t newTripDistance, uint32_t newTripTBFInterval,
          uint32_t &accumulatedDistance, uint32_t &numOfBatchedPositions);
  void batchFullEvent(const qmiLocEventBatchFullIndMsgT_v02* batchFullInfo);
  void batchStatusEvent(const qmiLocEventBatchingStatusIndMsgT_v02* batchStatusInfo);
  void onDbtPosReportEvent(const qmiLocEventDbtPositionReportIndMsgT_v02* pDbtPosReport);
  void geofenceBreachEvent(const qmiLocEventGeofenceBreachIndMsgT_v02* breachInfo);
  void geofenceBreachEvent(const qmiLocEventGeofenceBatchedBreachIndMsgT_v02* batchedBreachInfo);
  void geofenceStatusEvent(const qmiLocEventGeofenceGenAlertIndMsgT_v02* alertInfo);
  void geofenceDwellEvent(const qmiLocEventGeofenceBatchedDwellIndMsgT_v02 *dwellEvent);
  void reportLatencyInfo(const qmiLocLatencyInformationIndMsgT_v02* pLocLatencyInfo);
  void reportEngineLockStatus(const qmiLocEngineLockStateEnumT_v02 engineLockState);

protected:
  virtual enum loc_api_adapter_err
    open(LOC_API_ADAPTER_EVENT_MASK_T mask);
  virtual enum loc_api_adapter_err
    close();

  LocApiV02(LOC_API_ADAPTER_EVENT_MASK_T exMask,
            ContextBase *context = NULL);
  virtual ~LocApiV02();

public:
  static LocApiBase* createLocApiV02(LOC_API_ADAPTER_EVENT_MASK_T exMask,
                                     ContextBase* context);
  /* event callback registered with the loc_api v02 interface */
  virtual void eventCb(locClientHandleType client_handle,
               uint32_t loc_event_id,
               locClientEventIndUnionType loc_event_payload);

  /* error callback, this function handles the  service unavailable
     error */
  void errorCb(locClientHandleType handle,
               locClientErrorEnumType errorId);

  // Tracking
  virtual void startFix(const LocPosMode& posMode, LocApiResponse *adapterResponse);
  virtual void stopFix(LocApiResponse *adapterResponse);

  void startTimeBasedTracking(const TrackingOptions& options, LocApiResponse* adapterResponse);
  void stopTimeBasedTracking(LocApiResponse* adapterResponse);
  void startDistanceBasedTracking(uint32_t sessionId, const LocationOptions& options,
         LocApiResponse* adapterResponse);
  void stopDistanceBasedTracking(uint32_t sessionId, LocApiResponse* adapterResponse);

  // Batching
  void startBatching(uint32_t sessionId, const LocationOptions& options, uint32_t accuracy,
          uint32_t timeout, LocApiResponse* adapterResponse);
  void stopBatching(uint32_t sessionId, LocApiResponse* adapterResponse);
  LocationError startOutdoorTripBatchingSync(uint32_t tripDistance, uint32_t tripTbf,
          uint32_t timeout);
  void startOutdoorTripBatching(uint32_t tripDistance, uint32_t tripTbf, uint32_t timeout,
          LocApiResponse* adapterResponse);
  void reStartOutdoorTripBatching(uint32_t ongoingTripDistance, uint32_t ongoingTripInterval,
          uint32_t batchingTimeout, LocApiResponse* adapterResponse);
  LocationError stopOutdoorTripBatchingSync(bool deallocBatchBuffer = true);
  void stopOutdoorTripBatching(bool deallocBatchBuffer = true,
          LocApiResponse* adapterResponse = nullptr);
  LocationError getBatchedLocationsSync(size_t count);
  void getBatchedLocations(size_t count, LocApiResponse* adapterResponse);
  LocationError getBatchedTripLocationsSync(size_t count, uint32_t accumulatedDistance);
  void getBatchedTripLocations(size_t count, uint32_t accumulatedDistance,
          LocApiResponse* adapterResponse);
  virtual void setBatchSize(size_t size);
  virtual void setTripBatchSize(size_t size);
  LocationError queryAccumulatedTripDistanceSync(uint32_t &accumulatedTripDistance,
          uint32_t &numOfBatchedPositions);
  void queryAccumulatedTripDistance(
          LocApiResponseData<LocApiBatchData>* adapterResponseData);

  // Geofence
  virtual void addGeofence(uint32_t clientId, const GeofenceOption& options,
          const GeofenceInfo& info, LocApiResponseData<LocApiGeofenceData>* adapterResponseData);
  virtual void removeGeofence(uint32_t hwId, uint32_t clientId, LocApiResponse* adapterResponse);
  virtual void pauseGeofence(uint32_t hwId, uint32_t clientId, LocApiResponse* adapterResponse);
  virtual void resumeGeofence(uint32_t hwId, uint32_t clientId, LocApiResponse* adapterResponse);
  virtual void modifyGeofence(uint32_t hwId, uint32_t clientId,
          const GeofenceOption& options, LocApiResponse* adapterResponse);
  virtual void addToCallQueue(LocApiResponse* adapterResponse);


  virtual void
    setTime(LocGpsUtcTime time, int64_t timeReference, int uncertainty);

  virtual void
    injectPosition(double latitude, double longitude, float accuracy, bool onDemandCpi);

  virtual void
    injectPosition(const Location& location, bool onDemandCpi);

  virtual void
    injectPosition(const GnssLocationInfoNotification &locationInfo, bool onDemandCpi);

  virtual void
    deleteAidingData(const GnssAidingData& data, LocApiResponse *adapterResponse);

  virtual void
    informNiResponse(GnssNiResponse userResponse, const void* passThroughData);

  virtual LocationError
    setServerSync(const char* url, int len, LocServerType type);
  virtual LocationError
    setServerSync(unsigned int ip, int port, LocServerType type);
  virtual void
    atlOpenStatus(int handle, int is_succ, char* apn, uint32_t apnLen, AGpsBearerType bear,
                   LocAGpsType agpsType, LocApnTypeMask mask);
  virtual void atlCloseStatus(int handle, int is_succ);
  virtual LocationError setSUPLVersionSync(GnssConfigSuplVersion version);

  virtual enum loc_api_adapter_err setNMEATypesSync(uint32_t typesMask);

  virtual LocationError setLPPConfigSync(GnssConfigLppProfileMask profileMask);


  virtual enum loc_api_adapter_err
    setSensorPropertiesSync(bool gyroBiasVarianceRandomWalk_valid, float gyroBiasVarianceRandomWalk,
                            bool accelBiasVarianceRandomWalk_valid, float accelBiasVarianceRandomWalk,
                            bool angleBiasVarianceRandomWalk_valid, float angleBiasVarianceRandomWalk,
                            bool rateBiasVarianceRandomWalk_valid, float rateBiasVarianceRandomWalk,
                            bool velocityBiasVarianceRandomWalk_valid, float velocityBiasVarianceRandomWalk);

  virtual enum loc_api_adapter_err
    setSensorPerfControlConfigSync(int controlMode, int accelSamplesPerBatch,
            int accelBatchesPerSec, int gyroSamplesPerBatch, int gyroBatchesPerSec,
            int accelSamplesPerBatchHigh, int accelBatchesPerSecHigh,
            int gyroSamplesPerBatchHigh, int gyroBatchesPerSecHigh, int algorithmConfig);
  virtual LocationError
      setAGLONASSProtocolSync(GnssConfigAGlonassPositionProtocolMask aGlonassProtocol);
  virtual LocationError setLPPeProtocolCpSync(GnssConfigLppeControlPlaneMask lppeCP);
  virtual LocationError setLPPeProtocolUpSync(GnssConfigLppeUserPlaneMask lppeUP);
  virtual void getWwanZppFix();
  virtual void
      handleWwanZppFixIndication(const qmiLocGetAvailWwanPositionIndMsgT_v02 &zpp_ind);
  virtual void
      handleZppBestAvailableFixIndication(const qmiLocGetBestAvailablePositionIndMsgT_v02 &zpp_ind);
  virtual void getBestAvailableZppFix();
  virtual LocationError setGpsLockSync(GnssConfigGpsLock lock);
  virtual void setConstrainedTuncMode(bool enabled, float tuncConstraint, uint32_t powerBudget,
                                      LocApiResponse *adapterResponse=nullptr);
  virtual void setPositionAssistedClockEstimatorMode(bool enabled,
                                                     LocApiResponse *adapterResponse=nullptr);
  virtual void getGnssEnergyConsumed();
  virtual void updateSystemPowerState(PowerStateType powerState);
  virtual void requestForAidingData(GnssAidingDataSvMask svDataMask);
  virtual void configRobustLocation(bool enable, bool enableForE911,
                                    LocApiResponse *adapterResponse=nullptr);
  virtual void configMinGpsWeek(uint16_t minGpsWeek,
                                LocApiResponse *adapterResponse=nullptr);
  virtual LocationError setParameterSync(const GnssConfig & gnssConfig);

  virtual void getParameter(uint32_t sessionId, GnssConfigFlagsMask flags,
                            LocApiResponse* adapterResponse=nullptr);
  /*
  Returns
  Current value of GPS Lock on success
  -1 on failure
  */
  virtual int setSvMeasurementConstellation(const locClientEventMaskType mask);
  virtual LocationError setXtraVersionCheckSync(uint32_t check);

  virtual LocPosTechMask convertPosTechMask(qmiLocPosTechMaskT_v02 mask);
  virtual LocNavSolutionMask convertNavSolutionMask(qmiLocNavSolutionMaskT_v02 mask);
  virtual GnssConfigSuplVersion convertSuplVersion(const uint32_t suplVersion);
  virtual GnssConfigLppeControlPlaneMask convertLppeCp(const uint32_t lppeControlPlaneMask);
  virtual GnssConfigLppeUserPlaneMask convertLppeUp(const uint32_t lppeUserPlaneMask);
  virtual LocationError setEmergencyExtensionWindowSync(const uint32_t emergencyExtensionSeconds);
  virtual void setMeasurementCorrections(
        const GnssMeasurementCorrections& gnssMeasurementCorrections);
  virtual GnssSignalTypeMask convertQmiGnssSignalType(
        qmiLocGnssSignalTypeMaskT_v02 qmiGnssSignalType);

  void convertQmiBlacklistedSvConfigToGnssConfig(
        const qmiLocGetBlacklistSvIndMsgT_v02& qmiBlacklistConfig,
        GnssSvIdConfig& gnssBlacklistConfig);

  virtual void convertQmiSecondaryConfigToGnssConfig(
        qmiLocGNSSConstellEnumT_v02 qmiSecondaryBandConfig,
        GnssSvTypeConfig& secondaryBandConfig);

  /* Requests for SV/Constellation Control */
  virtual LocationError setBlacklistSvSync(const GnssSvIdConfig& config);
  virtual void setBlacklistSv(const GnssSvIdConfig& config,
                              LocApiResponse* adapterResponse=nullptr);
  virtual void getBlacklistSv();
  virtual void setConstellationControl(const GnssSvTypeConfig& config,
                                       LocApiResponse *adapterResponse=nullptr);
  virtual void getConstellationControl();
  virtual void resetConstellationControl(LocApiResponse *adapterResponse=nullptr);

  virtual void configConstellationMultiBand(const GnssSvTypeConfig& secondaryBandConfig,
                                            LocApiResponse* adapterResponse=nullptr);

  virtual void getConstellationMultiBandConfig(uint32_t sessionId,
                                      LocApiResponse* adapterResponse=nullptr);

  locClientStatusEnumType locSyncSendReq(uint32_t req_id, locClientReqUnionType req_payload,
          uint32_t timeout_msec, uint32_t ind_id, void* ind_payload_ptr);

  inline locClientStatusEnumType locClientSendReq(uint32_t req_id,
          locClientReqUnionType req_payload) {
      return ::locClientSendReq(clientHandle, req_id, req_payload);
  }
};

extern "C" LocApiBase* getLocApi(LOC_API_ADAPTER_EVENT_MASK_T exMask,
                                 ContextBase *context);
#endif //LOC_API_V_0_2_H
