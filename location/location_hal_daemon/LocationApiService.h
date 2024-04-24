/* Copyright (c) 2018-2020 The Linux Foundation. All rights reserved.
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

#ifndef LOCATIONAPISERVICE_H
#define LOCATIONAPISERVICE_H

#include <string>
#include <mutex>

#include <MsgTask.h>
#include <loc_cfg.h>
#include <LocIpc.h>
#include <LocTimer.h>
#ifdef POWERMANAGER_ENABLED
#include <PowerEvtHandler.h>
#endif
#include <location_interface.h>
#include <LocationAPI.h>
#include <LocationApiMsg.h>

#include <LocHalDaemonClientHandler.h>

#ifdef NO_UNORDERED_SET_OR_MAP
    #include <map>
#else
    #include <unordered_map>
#endif

#undef LOG_TAG
#define LOG_TAG "LocSvc_HalDaemon"

typedef struct {
    uint32_t autoStartGnss;
    uint32_t gnssSessionTbfMs;
    uint32_t deleteAllBeforeAutoStart;
    uint32_t posEngineMask;
    uint32_t positionMode;
} configParamToRead;


/******************************************************************************
LocationApiService
******************************************************************************/

typedef struct {
    // this stores the client name and the command type that client requests
    // the info will be used to send back command response
    std::string clientName;
    ELocMsgID   configMsgId;
} ConfigReqClientData;

// periodic timer to perform maintenance work, e.g.: resource cleanup
// for location hal daemon
typedef std::unordered_map<std::string, shared_ptr<LocIpcSender>> ClientNameIpcSenderMap;
class MaintTimer : public LocTimer {
public:
    MaintTimer(LocationApiService* locationApiService) :
            mLocationApiService(locationApiService) {
    };

    ~MaintTimer()  = default;

public:
    void timeOutCallback() override;

private:
    LocationApiService* mLocationApiService;
};

class SingleTerrestrialFixTimer : public LocTimer {
public:

    SingleTerrestrialFixTimer(LocationApiService* locationApiService,
                              std::string& clientName) :
            mLocationApiService(locationApiService),
            mClientName(clientName) {
    }

    ~SingleTerrestrialFixTimer() {
    }

public:
    void timeOutCallback() override;

private:
    LocationApiService* mLocationApiService;
    const std::string mClientName;
};

// This keeps track of the client that requests single fix terrestrial position
// and the timer that will fire when the timeout value has reached
typedef std::unordered_map<std::string, SingleTerrestrialFixTimer>
        SingleTerrestrialFixClientMap;

class LocationApiService
{
public:

    // singleton instance
    LocationApiService(const LocationApiService&) = delete;
    LocationApiService& operator = (const LocationApiService&) = delete;

    static LocationApiService* getInstance(
            const configParamToRead & configParamRead) {
        if (nullptr == mInstance) {
            mInstance = new LocationApiService(configParamRead);
        }
        return mInstance;
    }

    static void destroy() {
        if (nullptr != mInstance) {
            delete mInstance;
            mInstance = nullptr;
        }
    }

    // APIs can be invoked by IPC
    void processClientMsg(const char* data, uint32_t length);

    // from IPC receiver
    void onListenerReady(bool externalApIpc);

#ifdef POWERMANAGER_ENABLED
    void onPowerEvent(PowerStateType powerState);
#endif

    // other APIs
    void deleteClientbyName(const std::string name);

    // protobuf conversion util class
    LocationApiPbMsgConv mPbufMsgConv;

    static std::mutex mMutex;

    // Utility routine used by maintenance timer
    void performMaintenance();

    // Utility routine used by gtp fix timeout timer
    void gtpFixRequestTimeout(const std::string& clientName);

    inline const MsgTask& getMsgTask() const {return mMsgTask;};

private:
    // APIs can be invoked to process client's IPC messgage
    void newClient(LocAPIClientRegisterReqMsg*);
    void deleteClient(LocAPIClientDeregisterReqMsg*);

    void startTracking(LocAPIStartTrackingReqMsg*);
    void stopTracking(LocAPIStopTrackingReqMsg*);

    void suspendAllTrackingSessions();
    void resumeAllTrackingSessions();

    void updateSubscription(LocAPIUpdateCallbacksReqMsg*);
    void updateTrackingOptions(LocAPIUpdateTrackingOptionsReqMsg*);
    void updateNetworkAvailability(bool availability);
    void getGnssEnergyConsumed(const char* clientSocketName);
    void getSingleTerrestrialPos(LocAPIGetSingleTerrestrialPosReqMsg*);

    void startBatching(LocAPIStartBatchingReqMsg*);
    void stopBatching(LocAPIStopBatchingReqMsg*);
    void updateBatchingOptions(LocAPIUpdateBatchingOptionsReqMsg*);

    void addGeofences(LocAPIAddGeofencesReqMsg*);
    void removeGeofences(LocAPIRemoveGeofencesReqMsg*);
    void modifyGeofences(LocAPIModifyGeofencesReqMsg*);
    void pauseGeofences(LocAPIPauseGeofencesReqMsg*);
    void resumeGeofences(LocAPIResumeGeofencesReqMsg*);

    void pingTest(LocAPIPingTestReqMsg*);

    inline uint32_t gnssUpdateConfig(const GnssConfig& config) {
        uint32_t* sessionIds =  mLocationControlApi->gnssUpdateConfig(config);
        // in our usage, we only configure one setting at a time,
        // so we have only one sessionId
        uint32_t sessionId = 0;
        if (sessionIds) {
            sessionId = *sessionIds;
            delete [] sessionIds;
        }
        return sessionId;
    }

    // Location control API callback
    void onControlResponseCallback(LocationError err, uint32_t id);
    void onControlCollectiveResponseCallback(size_t count, LocationError *errs, uint32_t *ids);
    void onGnssConfigCallback(uint32_t sessionId, const GnssConfig& config);
    void onGnssEnergyConsumedCb(uint64_t totalEnergyConsumedSinceFirstBoot);

    // Callbacks for location api used service GTP WWAN fix request
    void onCapabilitiesCallback(LocationCapabilitiesMask mask);
    void onResponseCb(LocationError err, uint32_t id);
    void onCollectiveResponseCallback(size_t count, LocationError *errs, uint32_t *ids);
    void onGtpWwanTrackingCallback(Location location);

    // Location configuration API requests
    void configConstrainedTunc(
            const LocConfigConstrainedTuncReqMsg* pMsg);
    void configPositionAssistedClockEstimator(
            const LocConfigPositionAssistedClockEstimatorReqMsg* pMsg);
    void configConstellations(
            const LocConfigSvConstellationReqMsg* pMsg);
    void configConstellationSecondaryBand(
            const LocConfigConstellationSecondaryBandReqMsg* pMsg);
    void configAidingDataDeletion(
            LocConfigAidingDataDeletionReqMsg* pMsg);
    void configLeverArm(const LocConfigLeverArmReqMsg* pMsg);
    void configRobustLocation(const LocConfigRobustLocationReqMsg* pMsg);
    void configMinGpsWeek(const LocConfigMinGpsWeekReqMsg* pMsg);
    void configDeadReckoningEngineParams(const LocConfigDrEngineParamsReqMsg* pMsg);
    void configMinSvElevation(const LocConfigMinSvElevationReqMsg* pMsg);
    void configEngineRunState(const LocConfigEngineRunStateReqMsg* pMsg);
    void configUserConsentTerrestrialPositioning(
            LocConfigUserConsentTerrestrialPositioningReqMsg* pMsg);

    // Location configuration API get/read requests
    void getGnssConfig(const LocAPIMsgHeader* pReqMsg,
                       GnssConfigFlagsBits configFlag);
    void getConstellationSecondaryBandConfig(
            const LocConfigGetConstellationSecondaryBandConfigReqMsg* pReqMsg);

    // Location configuration API util routines
    void addConfigRequestToMap(uint32_t sessionId,
                               const LocAPIMsgHeader* pMsg);

    LocationApiService(const configParamToRead & configParamRead);
    virtual ~LocationApiService();

    // private utilities
    inline LocHalDaemonClientHandler* getClient(const std::string& clientname) {
        // find client from property db
        auto client = mClients.find(clientname);
        if (client == std::end(mClients)) {
            LOC_LOGe("Failed to find client %s", clientname.c_str());
            return nullptr;
        }
        return client->second;
    }

    inline LocHalDaemonClientHandler* getClient(const char* socketName) {
        std::string clientname(socketName);
        return getClient(clientname);
    }

    GnssInterface* getGnssInterface();

#ifdef POWERMANAGER_ENABLED
    // power event observer
    PowerEvtHandler* mPowerEventObserver;
#endif

    // singleton instance
    static LocationApiService *mInstance;

    // IPC interface
    LocIpc mIpc;
    unique_ptr<LocIpcRecver> mBlockingRecver;

    // Client propery database
    std::unordered_map<std::string, LocHalDaemonClientHandler*> mClients;
    std::unordered_map<uint32_t, ConfigReqClientData> mConfigReqs;

    // Location Control API interface
    uint32_t mLocationControlId;
    LocationControlCallbacks mControlCallabcks;
    LocationControlAPI *mLocationControlApi;

    // Configration
    const uint32_t mAutoStartGnss;
    GnssSuplMode   mPositionMode;

    PowerStateType  mPowerState;

    // maintenance timer
    MaintTimer mMaintTimer;

   // msg task used by timers
    const MsgTask   mMsgTask;

    // Terrestrial service related APIs
    // Location api interface for single short wwan fix
    LocationAPI* mGtpWwanSsLocationApi;
    LocationCallbacks mGtpWwanSsLocationApiCallbacks;
    trackingCallback mGtpWwanPosCallback;
    // -1: not set, 0: user not opt-in, 1: user opt in
    int mOptInTerrestrialService;
    SingleTerrestrialFixClientMap mTerrestrialFixReqs;
};

#endif //LOCATIONAPISERVICE_H

