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

#include <stdint.h>
#include <sys/stat.h>
#include <dlfcn.h>
#include <memory>
#include <SystemStatus.h>
#include <LocationApiMsg.h>
#include <gps_extended_c.h>

#ifdef POWERMANAGER_ENABLED
#include <PowerEvtHandler.h>
#endif
#include <LocHalDaemonClientHandler.h>
#include <LocationApiService.h>
#include <location_interface.h>
#include <loc_misc_utils.h>

using namespace std;

#define MAX_GEOFENCE_COUNT (20)
#define MAINT_TIMER_INTERVAL_MSEC (60000)
#define AUTO_START_CLIENT_NAME "default"

typedef void* (getLocationInterface)();

/******************************************************************************
LocationApiService - static members
******************************************************************************/
LocationApiService* LocationApiService::mInstance = nullptr;
std::mutex LocationApiService::mMutex;

/******************************************************************************
LocHaldIpcListener
******************************************************************************/
class LocHaldIpcListener : public ILocIpcListener {
protected:
    LocationApiService& mService;
public:
    inline LocHaldIpcListener(LocationApiService& service) : mService(service) {}
    // override from LocIpc
    inline void onReceive(const char* data, uint32_t length,
                          const LocIpcRecver* recver) override {
        mService.processClientMsg(data, length);
    }
};
class LocHaldLocalIpcListener : public LocHaldIpcListener {
    const string mClientSockPath = SOCKET_LOC_CLIENT_DIR;
    const string mClientSockPathnamePrefix = SOCKET_LOC_CLIENT_DIR LOC_CLIENT_NAME_PREFIX;
public:
    inline LocHaldLocalIpcListener(LocationApiService& service) : LocHaldIpcListener(service) {}
    inline void onListenerReady() override {
        if (0 != chown(SOCKET_TO_LOCATION_HAL_DAEMON, UID_GPS, GID_LOCCLIENT)) {
            LOC_LOGe("chown to group locclient failed %s", strerror(errno));
        }

        // traverse client sockets directory - then broadcast READY message
        LOC_LOGd(">-- onListenerReady Finding client sockets...");

        DIR *dirp = opendir(mClientSockPath.c_str());
        if (!dirp) {
            LOC_LOGw("%s not created", mClientSockPath.c_str());
            return;
        }

        struct dirent *dp = nullptr;
        struct stat sbuf = {0};
        while (nullptr != (dp = readdir(dirp))) {
            std::string fname = mClientSockPath;
            fname += dp->d_name;
            if (-1 == lstat(fname.c_str(), &sbuf)) {
                continue;
            }
            if ('.' == (dp->d_name[0])) {
                continue;
            }

            if (0 == fname.compare(0, mClientSockPathnamePrefix.size(),
                                   mClientSockPathnamePrefix)) {
                shared_ptr<LocIpcSender> sender = LocHalDaemonClientHandler::createSender(fname);
                LocAPIHalReadyIndMsg msg(SERVICE_NAME, &mService.mPbufMsgConv);
                string pbStr;
                if (msg.serializeToProtobuf(pbStr)) {
                    LOC_LOGd("<-- Sending ready to socket: %s, msg size %d",
                            fname.c_str(), pbStr.size());
                    LocIpc::send(*sender, reinterpret_cast<const uint8_t*>(pbStr.c_str()),
                            pbStr.size());
                } else {
                    LOC_LOGe("LocAPIHalReadyIndMsg serializeToProtobuf failed");
                }
            }
        }
        closedir(dirp);
    }
};

/******************************************************************************
LocationApiService - constructors
******************************************************************************/
LocationApiService::LocationApiService(const configParamToRead & configParamRead) :

    mLocationControlId(0),
    mAutoStartGnss(configParamRead.autoStartGnss),
    mPowerState(POWER_STATE_UNKNOWN),
    mPositionMode((GnssSuplMode)configParamRead.positionMode),
    mMsgTask("LocHalDaemonMaintenanceMsgTask"),
    mMaintTimer(this),
    mGtpWwanSsLocationApi(nullptr),
    mOptInTerrestrialService(-1)
#ifdef POWERMANAGER_ENABLED
    ,mPowerEventObserver(nullptr)
#endif
    {

    LOC_LOGd("AutoStartGnss=%u", mAutoStartGnss);
    LOC_LOGd("GnssSessionTbfMs=%u", configParamRead.gnssSessionTbfMs);
    LOC_LOGd("DeleteAllBeforeAutoStart=%u", configParamRead.deleteAllBeforeAutoStart);
    LOC_LOGd("DeleteAllOnEnginesMask=%u", configParamRead.posEngineMask);
    LOC_LOGd("PositionMode=%u", configParamRead.positionMode);

    // create Location control API
    mControlCallabcks.size = sizeof(mControlCallabcks);
    mControlCallabcks.responseCb = [this](LocationError err, uint32_t id) {
        onControlResponseCallback(err, id);
    };
    mControlCallabcks.collectiveResponseCb =
            [this](size_t count, LocationError *errs, uint32_t *ids) {
        onControlCollectiveResponseCallback(count, errs, ids);
    };
    mControlCallabcks.gnssConfigCb =
            [this](uint32_t sessionId, const GnssConfig& config) {
        onGnssConfigCallback(sessionId, config);
    };

    mLocationControlApi = LocationControlAPI::createInstance(mControlCallabcks);
    if (nullptr == mLocationControlApi) {
        LOC_LOGd("Failed to create LocationControlAPI");
        return;
    }

    // enable
    mLocationControlId = mLocationControlApi->enable(LOCATION_TECHNOLOGY_TYPE_GNSS);
    LOC_LOGd("-->enable=%u", mLocationControlId);
    // this is a unique id assigned to this daemon - will be used when disable

#ifdef POWERMANAGER_ENABLED
    // register power event handler
    mPowerEventObserver = PowerEvtHandler::getPwrEvtHandler(this);
    if (nullptr == mPowerEventObserver) {
        LOC_LOGe("Failed to regiseter Powerevent handler");
        return;
    }
#endif

    mMaintTimer.start(MAINT_TIMER_INTERVAL_MSEC, false);

    // create a default client if enabled by config
    if (mAutoStartGnss) {
        if ((configParamRead.deleteAllBeforeAutoStart) &&
                (configParamRead.posEngineMask != 0)) {
            GnssAidingData aidingData = {};
            aidingData.deleteAll = true;
            aidingData.posEngineMask = configParamRead.posEngineMask;
            mLocationControlApi->gnssDeleteAidingData(aidingData);
        }

        LOC_LOGd("--> Starting a default client...");
        LocHalDaemonClientHandler* pClient =
                new LocHalDaemonClientHandler(this, AUTO_START_CLIENT_NAME, LOCATION_CLIENT_API);
        mClients.emplace(AUTO_START_CLIENT_NAME, pClient);

        pClient->updateSubscription(
                E_LOC_CB_GNSS_LOCATION_INFO_BIT | E_LOC_CB_GNSS_SV_BIT);

        LocationOptions locationOption = {};
        locationOption.size = sizeof(locationOption);
        locationOption.minInterval = configParamRead.gnssSessionTbfMs;
        locationOption.minDistance = 0;
        locationOption.mode = mPositionMode;

        pClient->startTracking(locationOption);
        pClient->mTracking = true;
        loc_boot_kpi_marker("L - Auto Session Start");
        pClient->mPendingMessages.push(E_LOCAPI_START_TRACKING_MSG_ID);
    }

    // start receiver - never return
    LOC_LOGd("Ready, start Ipc Receivers");
    auto recver = LocIpc::getLocIpcLocalRecver(make_shared<LocHaldLocalIpcListener>(*this),
            SOCKET_TO_LOCATION_HAL_DAEMON);
    // blocking: set to false
    mIpc.startNonBlockingListening(recver);

    mBlockingRecver = LocIpc::getLocIpcQrtrRecver(make_shared<LocHaldIpcListener>(*this),
            LOCATION_CLIENT_API_QSOCKET_HALDAEMON_SERVICE_ID,
            LOCATION_CLIENT_API_QSOCKET_HALDAEMON_INSTANCE_ID);
    mIpc.startBlockingListening(*mBlockingRecver);
}

LocationApiService::~LocationApiService() {
    mIpc.stopNonBlockingListening();
    mIpc.stopBlockingListening(*mBlockingRecver);

    // free resource associated with the client
    for (auto each : mClients) {
        LOC_LOGd(">-- deleted client [%s]", each.first.c_str());
        each.second->cleanup();
    }

    // delete location contorol API handle
    mLocationControlApi->disable(mLocationControlId);
    mLocationControlApi->destroy();
}

/******************************************************************************
LocationApiService - implementation - registration
******************************************************************************/
void LocationApiService::processClientMsg(const char* data, uint32_t length) {

    string mMsgData(data, length);

    // parse received message
    // Protobuff Encoding enabled, so we need to convert the message from proto
    // encoded format to local structure
    PBLocAPIMsgHeader pbLocApiMsg;
    if (0 == pbLocApiMsg.ParseFromString(mMsgData)) {
        LOC_LOGe("Failed to parse pbLocApiMsg from input stream!! length: %u",
                mMsgData.length());
        return;
    }

    ELocMsgID eLocMsgid = mPbufMsgConv.getEnumForPBELocMsgID(pbLocApiMsg.msgid());
    string sockName = pbLocApiMsg.msocketname();
    uint32_t msgVer = pbLocApiMsg.msgversion();
    uint32_t payloadSize = pbLocApiMsg.payloadsize();
    // pbLocApiMsg.payload() contains the payload data.

    LOC_LOGi(">-- onReceive Rcvd msg id: %d, remote client: %s, payload size: %d", eLocMsgid,
            sockName.c_str(), payloadSize);
    LocAPIMsgHeader locApiMsg(sockName.c_str(), eLocMsgid);

    // throw away msg that does not come from location hal daemon client, e.g. LCA/LIA
    if (false == locApiMsg.isValidClientMsg(payloadSize)) {
        return;
    }

    switch (eLocMsgid) {
        case E_LOCAPI_CLIENT_REGISTER_MSG_ID: {
            // new client
            PBLocAPIClientRegisterReqMsg pbLocApiClientRegReqMsg;
            if (0 == pbLocApiClientRegReqMsg.ParseFromString(pbLocApiMsg.payload())) {
                LOC_LOGe("Failed to parse pbLocApiClientRegReqMsg from payload!!");
                return;
            }
            LocAPIClientRegisterReqMsg msg(pbLocApiMsg.msocketname().c_str(),
                    pbLocApiClientRegReqMsg, &mPbufMsgConv);
            newClient(reinterpret_cast<LocAPIClientRegisterReqMsg*>(&msg));
            break;
        }
        case E_LOCAPI_CLIENT_DEREGISTER_MSG_ID: {
            // delete client
            LocAPIClientDeregisterReqMsg msg(sockName.c_str(), &mPbufMsgConv);
            deleteClient(reinterpret_cast<LocAPIClientDeregisterReqMsg*>(&msg));
            break;
        }

        case E_LOCAPI_START_TRACKING_MSG_ID: {
            // start
            PBLocAPIStartTrackingReqMsg pbLocApiStartTrackMsg;
            if (0 == pbLocApiStartTrackMsg.ParseFromString(pbLocApiMsg.payload())) {
                LOC_LOGe("Failed to parse pbLocApiStartTrackMsg from payload!!");
                return;
            }
            LocAPIStartTrackingReqMsg msg(sockName.c_str(), pbLocApiStartTrackMsg, &mPbufMsgConv);
            startTracking(reinterpret_cast<LocAPIStartTrackingReqMsg*>(&msg));
            break;
        }
        case E_LOCAPI_STOP_TRACKING_MSG_ID: {
            // stop
            LocAPIStopTrackingReqMsg msg(sockName.c_str(), &mPbufMsgConv);
            stopTracking(reinterpret_cast<LocAPIStopTrackingReqMsg*>(&msg));
            break;
        }
        case E_LOCAPI_UPDATE_CALLBACKS_MSG_ID: {
            // update subscription
            PBLocAPIUpdateCallbacksReqMsg pbLocApiUpdateCbsReqMsg;
            if (0 == pbLocApiUpdateCbsReqMsg.ParseFromString(pbLocApiMsg.payload())) {
                LOC_LOGe("Failed to parse pbLocApiUpdateCbsReqMsg from payload!!");
                return;
            }
            LocAPIUpdateCallbacksReqMsg msg(sockName.c_str(), pbLocApiUpdateCbsReqMsg,
                    &mPbufMsgConv);
            updateSubscription(reinterpret_cast<LocAPIUpdateCallbacksReqMsg*>(&msg));
            break;
        }
        case E_LOCAPI_UPDATE_TRACKING_OPTIONS_MSG_ID: {
            PBLocAPIUpdateTrackingOptionsReqMsg pbLocApiUpdateTrackOptMsg;
            if (0 == pbLocApiUpdateTrackOptMsg.ParseFromString(pbLocApiMsg.payload())) {
                LOC_LOGe("Failed to parse pbLocApiUpdateTrackOptMsg from payload!!");
                return;
            }
            LocAPIUpdateTrackingOptionsReqMsg msg(sockName.c_str(), pbLocApiUpdateTrackOptMsg,
                    &mPbufMsgConv);
            updateTrackingOptions(reinterpret_cast <LocAPIUpdateTrackingOptionsReqMsg*>(&msg));
            break;
        }

        case E_LOCAPI_START_BATCHING_MSG_ID: {
            // start
            PBLocAPIStartBatchingReqMsg pbLocApiStartBatchReq;
            if (0 == pbLocApiStartBatchReq.ParseFromString(pbLocApiMsg.payload())) {
                LOC_LOGe("Failed to parse pbLocApiStartBatchReq from payload!!");
                return;
            }
            LocAPIStartBatchingReqMsg msg(sockName.c_str(), pbLocApiStartBatchReq, &mPbufMsgConv);
            startBatching(reinterpret_cast<LocAPIStartBatchingReqMsg*>(&msg));
            break;
        }
        case E_LOCAPI_STOP_BATCHING_MSG_ID: {
            // stop
            LocAPIStopBatchingReqMsg msg(sockName.c_str(), &mPbufMsgConv);
            stopBatching(reinterpret_cast<LocAPIStopBatchingReqMsg*>(&msg));
            break;
        }
        case E_LOCAPI_UPDATE_BATCHING_OPTIONS_MSG_ID: {
            PBLocAPIUpdateBatchingOptionsReqMsg pbLocApiUpdateBatch;
            if (0 == pbLocApiUpdateBatch.ParseFromString(pbLocApiMsg.payload())) {
                LOC_LOGe("Failed to parse pbLocApiUpdateBatch from payload!!");
                return;
            }
            LocAPIUpdateBatchingOptionsReqMsg msg(sockName.c_str(), pbLocApiUpdateBatch,
                    &mPbufMsgConv);
            updateBatchingOptions(reinterpret_cast <LocAPIUpdateBatchingOptionsReqMsg*>(&msg));
            break;
        }
        case E_LOCAPI_ADD_GEOFENCES_MSG_ID: {
            PBLocAPIAddGeofencesReqMsg pbLocApiAddGf;
            if (0 == pbLocApiAddGf.ParseFromString(pbLocApiMsg.payload())) {
                LOC_LOGe("Failed to parse pbLocApiAddGf from payload!!");
                return;
            }
            LocAPIAddGeofencesReqMsg msg(sockName.c_str(), pbLocApiAddGf, &mPbufMsgConv);
            addGeofences(reinterpret_cast<LocAPIAddGeofencesReqMsg*>(&msg));
            break;
        }
        case E_LOCAPI_REMOVE_GEOFENCES_MSG_ID: {
            PBLocAPIRemoveGeofencesReqMsg pbLocApiRnGfReq;
            if (0 == pbLocApiRnGfReq.ParseFromString(pbLocApiMsg.payload())) {
                LOC_LOGe("Failed to parse pbLocApiRnGfReq from payload!!");
                return;
            }
            LocAPIRemoveGeofencesReqMsg msg(sockName.c_str(), pbLocApiRnGfReq, &mPbufMsgConv);
            removeGeofences(reinterpret_cast<LocAPIRemoveGeofencesReqMsg*>(&msg));
            break;
        }
        case E_LOCAPI_MODIFY_GEOFENCES_MSG_ID: {
            PBLocAPIModifyGeofencesReqMsg pbLocApiModGf;
            if (0 == pbLocApiModGf.ParseFromString(pbLocApiMsg.payload())) {
                LOC_LOGe("Failed to parse pbLocApiModGf from payload!!");
                return;
            }
            LocAPIModifyGeofencesReqMsg msg(sockName.c_str(), pbLocApiModGf, &mPbufMsgConv);
            modifyGeofences(reinterpret_cast<LocAPIModifyGeofencesReqMsg*>(&msg));
            break;
        }
        case E_LOCAPI_PAUSE_GEOFENCES_MSG_ID: {
            PBLocAPIPauseGeofencesReqMsg pbLocApiPauseGf;
            if (0 == pbLocApiPauseGf.ParseFromString(pbLocApiMsg.payload())) {
                LOC_LOGe("Failed to parse pbLocApiPauseGf from payload!!");
                return;
            }
            LocAPIPauseGeofencesReqMsg msg(sockName.c_str(), pbLocApiPauseGf, &mPbufMsgConv);
            pauseGeofences(reinterpret_cast<LocAPIPauseGeofencesReqMsg*>(&msg));
            break;
        }
        case E_LOCAPI_RESUME_GEOFENCES_MSG_ID: {
            PBLocAPIResumeGeofencesReqMsg pbLocApiResumeGf;
            if (0 == pbLocApiResumeGf.ParseFromString(pbLocApiMsg.payload())) {
                LOC_LOGe("Failed to parse pbLocApiResumeGf from payload!!");
                return;
            }
            LocAPIResumeGeofencesReqMsg msg(sockName.c_str(), pbLocApiResumeGf, &mPbufMsgConv);
            resumeGeofences(reinterpret_cast<LocAPIResumeGeofencesReqMsg*>(&msg));
            break;
        }
        case E_LOCAPI_CONTROL_UPDATE_NETWORK_AVAILABILITY_MSG_ID: {
            PBLocAPIUpdateNetworkAvailabilityReqMsg pbLocApiUpdateNetwAvail;
            if (0 == pbLocApiUpdateNetwAvail.ParseFromString(pbLocApiMsg.payload())) {
                LOC_LOGe("Failed to parse pbLocApiUpdateNetwAvail from payload!!");
                return;
            }
            LocAPIUpdateNetworkAvailabilityReqMsg msg(sockName.c_str(), pbLocApiUpdateNetwAvail,
                    &mPbufMsgConv);
            updateNetworkAvailability(msg.mAvailability);
            break;
        }
        case E_LOCAPI_GET_GNSS_ENGERY_CONSUMED_MSG_ID: {
            getGnssEnergyConsumed(sockName.c_str());
            break;
        }
        case E_LOCAPI_GET_SINGLE_TERRESTRIAL_POS_REQ_MSG_ID: {
            PBLocAPIGetSingleTerrestrialPosReqMsg pbMsg;
            if (0 == pbMsg.ParseFromString(pbLocApiMsg.payload())) {
                LOC_LOGe("Failed to parse PBLocAPIGetSingleTerrestrialPosReqMsg from payload!!");
                return;
            }
            LocAPIGetSingleTerrestrialPosReqMsg msg(sockName.c_str(), pbMsg, &mPbufMsgConv);
            getSingleTerrestrialPos(&msg);
            break;
        }
        case E_LOCAPI_PINGTEST_MSG_ID: {
            PBLocAPIPingTestReqMsg pbLocApiPingTestMsg;
            if (0 == pbLocApiPingTestMsg.ParseFromString(pbLocApiMsg.payload())) {
                LOC_LOGe("Failed to parse pbLocApiPingTestMsg from payload!!");
                return;
            }
            LocAPIPingTestReqMsg msg(sockName.c_str(), pbLocApiPingTestMsg, &mPbufMsgConv);
            pingTest(reinterpret_cast<LocAPIPingTestReqMsg*>(&msg));
            break;
        }

        // location configuration API
        case E_INTAPI_CONFIG_CONSTRAINTED_TUNC_MSG_ID: {
            PBLocConfigConstrainedTuncReqMsg pbLocApiConfConstrTunc;
            if (0 == pbLocApiConfConstrTunc.ParseFromString(pbLocApiMsg.payload())) {
                LOC_LOGe("Failed to parse pbLocApiConfConstrTunc from payload!!");
                return;
            }
            LocConfigConstrainedTuncReqMsg msg(sockName.c_str(), pbLocApiConfConstrTunc,
                    &mPbufMsgConv);
            configConstrainedTunc(reinterpret_cast<LocConfigConstrainedTuncReqMsg*>(&msg));
            break;
        }

        case E_INTAPI_CONFIG_POSITION_ASSISTED_CLOCK_ESTIMATOR_MSG_ID: {
            PBLocConfigPositionAssistedClockEstimatorReqMsg pbLocApiConfPosAsstdClockEst;
            if (0 == pbLocApiConfPosAsstdClockEst.ParseFromString(pbLocApiMsg.payload())) {
                LOC_LOGe("Failed to parse pbLocApiConfPosAsstdClockEst from payload!!");
                return;
            }
            LocConfigPositionAssistedClockEstimatorReqMsg msg(sockName.c_str(),
                    pbLocApiConfPosAsstdClockEst, &mPbufMsgConv);
            configPositionAssistedClockEstimator(reinterpret_cast
                        <LocConfigPositionAssistedClockEstimatorReqMsg*>(&msg));
            break;
        }

        case E_INTAPI_CONFIG_SV_CONSTELLATION_MSG_ID: {
            PBLocConfigSvConstellationReqMsg pbLocApiConfSvConstReqMsg;
            if (0 == pbLocApiConfSvConstReqMsg.ParseFromString(pbLocApiMsg.payload())) {
                LOC_LOGe("Failed to parse pbLocApiConfSvConstReqMsg from payload!!");
                return;
            }
            LocConfigSvConstellationReqMsg msg(sockName.c_str(), pbLocApiConfSvConstReqMsg,
                    &mPbufMsgConv);
            configConstellations(reinterpret_cast<LocConfigSvConstellationReqMsg*>(&msg));
            break;
        }

        case E_INTAPI_CONFIG_CONSTELLATION_SECONDARY_BAND_MSG_ID: {
            PBLocConfigConstellationSecondaryBandReqMsg pbLocCfgConstlSecBandReqMsg;
            if (0 == pbLocCfgConstlSecBandReqMsg.ParseFromString(pbLocApiMsg.payload())) {
                LOC_LOGe("Failed to parse pbLocCfgConstlSecBandReqMsg from payload!!");
                return;
            }
            LocConfigConstellationSecondaryBandReqMsg msg(sockName.c_str(),
                    pbLocCfgConstlSecBandReqMsg, &mPbufMsgConv);
            configConstellationSecondaryBand(reinterpret_cast
                    <LocConfigConstellationSecondaryBandReqMsg*>(&msg));
            break;
        }

        case E_INTAPI_CONFIG_AIDING_DATA_DELETION_MSG_ID: {
            PBLocConfigAidingDataDeletionReqMsg pbLocConfAidDataDelMsg;
            if (0 == pbLocConfAidDataDelMsg.ParseFromString(pbLocApiMsg.payload())) {
                LOC_LOGe("Failed to parse pbLocConfAidDataDelMsg from payload!!");
                return;
            }
            LocConfigAidingDataDeletionReqMsg msg(sockName.c_str(), pbLocConfAidDataDelMsg,
                    &mPbufMsgConv);
            configAidingDataDeletion(reinterpret_cast<LocConfigAidingDataDeletionReqMsg*>(&msg));
            break;
        }

        case E_INTAPI_CONFIG_LEVER_ARM_MSG_ID: {
            PBLocConfigLeverArmReqMsg pbLocConfLeverArmMsg;
            if (0 == pbLocConfLeverArmMsg.ParseFromString(pbLocApiMsg.payload())) {
                LOC_LOGe("Failed to parse pbLocConfLeverArmMsg from payload!!");
                return;
            }
            LocConfigLeverArmReqMsg msg(sockName.c_str(), pbLocConfLeverArmMsg, &mPbufMsgConv);
            configLeverArm(reinterpret_cast<LocConfigLeverArmReqMsg*>(&msg));
            break;
        }

        case E_INTAPI_CONFIG_ROBUST_LOCATION_MSG_ID: {
            PBLocConfigRobustLocationReqMsg pbLocConfRobustLocMsg;
            if (0 == pbLocConfRobustLocMsg.ParseFromString(pbLocApiMsg.payload())) {
                LOC_LOGe("Failed to parse pbLocConfRobustLocMsg from payload!!");
                return;
            }
            LocConfigRobustLocationReqMsg msg(sockName.c_str(), pbLocConfRobustLocMsg,
                    &mPbufMsgConv);
            configRobustLocation(reinterpret_cast<LocConfigRobustLocationReqMsg*>(&msg));
            break;
        }

        case E_INTAPI_CONFIG_MIN_GPS_WEEK_MSG_ID: {
            PBLocConfigMinGpsWeekReqMsg pbLocConfMinGpsWeek;
            if (0 == pbLocConfMinGpsWeek.ParseFromString(pbLocApiMsg.payload())) {
                LOC_LOGe("Failed to parse pbLocConfMinGpsWeek from payload!!");
                return;
            }
            LocConfigMinGpsWeekReqMsg msg(sockName.c_str(), pbLocConfMinGpsWeek, &mPbufMsgConv);
            configMinGpsWeek(reinterpret_cast<LocConfigMinGpsWeekReqMsg*>(&msg));
            break;
        }

        case E_INTAPI_CONFIG_DEAD_RECKONING_ENGINE_MSG_ID: {
            PBLocConfigDrEngineParamsReqMsg pbLocCfgDrEngParamReq;
            if (0 == pbLocCfgDrEngParamReq.ParseFromString(pbLocApiMsg.payload())) {
                LOC_LOGe("Failed to parse pbLocCfgDrEngParamReq from payload!!");
                return;
            }
            LocConfigDrEngineParamsReqMsg msg(sockName.c_str(), pbLocCfgDrEngParamReq,
                    &mPbufMsgConv);
            configDeadReckoningEngineParams(
                    reinterpret_cast<LocConfigDrEngineParamsReqMsg*>(&msg));
            break;
        }

        case E_INTAPI_CONFIG_MIN_SV_ELEVATION_MSG_ID: {
            PBLocConfigMinSvElevationReqMsg pbLocConfMinSvElev;
            if (0 == pbLocConfMinSvElev.ParseFromString(pbLocApiMsg.payload())) {
                LOC_LOGe("Failed to parse pbLocConfMinSvElev from payload!!");
                return;
            }
            LocConfigMinSvElevationReqMsg msg(sockName.c_str(), pbLocConfMinSvElev, &mPbufMsgConv);
            configMinSvElevation(reinterpret_cast<LocConfigMinSvElevationReqMsg*>(&msg));
            break;
        }

        case E_INTAPI_CONFIG_ENGINE_RUN_STATE_MSG_ID: {
            PBLocConfigEngineRunStateReqMsg pbLocConfEngineRunState;
            if (0 == pbLocConfEngineRunState.ParseFromString(pbLocApiMsg.payload())) {
                LOC_LOGe("Failed to parse pbLocConfEngineRunState from payload!!");
                return;
            }
            LocConfigEngineRunStateReqMsg msg(sockName.c_str(),
                                              pbLocConfEngineRunState,
                                              &mPbufMsgConv);
            configEngineRunState(reinterpret_cast<LocConfigEngineRunStateReqMsg*>(&msg));
            break;
        }

        case E_INTAPI_CONFIG_USER_CONSENT_TERRESTRIAL_POSITIONING_MSG_ID: {
            PBLocConfigUserConsentTerrestrialPositioningReqMsg pbMsg;
            if (0 == pbMsg.ParseFromString(pbLocApiMsg.payload())) {
                LOC_LOGe("Failed to parse PBLocConfigUserConsentTerrestrialPositioningReqMsg!!");
                return;
            }
            LocConfigUserConsentTerrestrialPositioningReqMsg msg(
                    sockName.c_str(), pbMsg, &mPbufMsgConv);
            configUserConsentTerrestrialPositioning(reinterpret_cast
                    <LocConfigUserConsentTerrestrialPositioningReqMsg*>(&msg));
            break;
        }

        case E_INTAPI_GET_ROBUST_LOCATION_CONFIG_REQ_MSG_ID: {
            getGnssConfig(&locApiMsg, GNSS_CONFIG_FLAGS_ROBUST_LOCATION_BIT);
            break;
        }

        case E_INTAPI_GET_MIN_GPS_WEEK_REQ_MSG_ID: {
            getGnssConfig(&locApiMsg, GNSS_CONFIG_FLAGS_MIN_GPS_WEEK_BIT);
            break;
        }

        case E_INTAPI_GET_MIN_SV_ELEVATION_REQ_MSG_ID: {
            getGnssConfig(&locApiMsg, GNSS_CONFIG_FLAGS_MIN_SV_ELEVATION_BIT);
            break;
        }

        case E_INTAPI_GET_CONSTELLATION_SECONDARY_BAND_CONFIG_REQ_MSG_ID: {
            getConstellationSecondaryBandConfig(
                    (const LocConfigGetConstellationSecondaryBandConfigReqMsg*) &locApiMsg);
            break;
        }

        default: {
            LOC_LOGe("Unknown message with id: %d ", eLocMsgid);
            break;
        }
    }
}

/******************************************************************************
LocationApiService - implementation - registration
******************************************************************************/
void LocationApiService::newClient(LocAPIClientRegisterReqMsg *pMsg) {

    std::lock_guard<std::mutex> lock(mMutex);
    std::string clientname(pMsg->mSocketName);

    // if this name is already used return error
    if (mClients.find(clientname) != mClients.end()) {
        LOC_LOGe("invalid client=%s already existing", clientname.c_str());
        return;
    }

    // store it in client property database
    LocHalDaemonClientHandler *pClient =
            new LocHalDaemonClientHandler(this, clientname, pMsg->mClientType);
    if (!pClient) {
        LOC_LOGe("failed to register client=%s", clientname.c_str());
        return;
    }

    mClients.emplace(clientname, pClient);
    LOC_LOGi(">-- registered new client=%s", clientname.c_str());
}

void LocationApiService::deleteClient(LocAPIClientDeregisterReqMsg *pMsg) {

    std::lock_guard<std::mutex> lock(mMutex);
    std::string clientname(pMsg->mSocketName);
    deleteClientbyName(clientname);
}

void LocationApiService::deleteClientbyName(const std::string clientname) {
    LOC_LOGi(">-- deleteClient client=%s", clientname.c_str());

    // delete this client from property db
    LocHalDaemonClientHandler* pClient = getClient(clientname);

    if (!pClient) {
        LOC_LOGe(">-- deleteClient invlalid client=%s", clientname.c_str());
        return;
    }
    mClients.erase(clientname);
    mTerrestrialFixReqs.erase(clientname);
    pClient->cleanup();
}
/******************************************************************************
LocationApiService - implementation - tracking
******************************************************************************/
void LocationApiService::startTracking(LocAPIStartTrackingReqMsg *pMsg) {

    std::lock_guard<std::mutex> lock(mMutex);
    LocHalDaemonClientHandler* pClient = getClient(pMsg->mSocketName);
    if (!pClient) {
        LOC_LOGe(">-- start invlalid client=%s", pMsg->mSocketName);
        return;
    }

    LocationOptions locationOption = pMsg->locOptions;
    // set the mode according to the master position mode
    locationOption.mode = mPositionMode;

    if (!pClient->startTracking(locationOption)) {
        LOC_LOGe("Failed to start session");
        return;
    }
    // success
    pClient->mTracking = true;
    pClient->mPendingMessages.push(E_LOCAPI_START_TRACKING_MSG_ID);

    LOC_LOGi(">-- start started session");
    return;
}

void LocationApiService::stopTracking(LocAPIStopTrackingReqMsg *pMsg) {

    std::lock_guard<std::mutex> lock(mMutex);
    LocHalDaemonClientHandler* pClient = getClient(pMsg->mSocketName);
    if (!pClient) {
        LOC_LOGe(">-- stop invlalid client=%s", pMsg->mSocketName);
        return;
    }

    pClient->mTracking = false;
    pClient->unsubscribeLocationSessionCb();
    pClient->stopTracking();
    pClient->mPendingMessages.push(E_LOCAPI_STOP_TRACKING_MSG_ID);
    LOC_LOGi(">-- stopping session");
}

// no need to hold the lock as lock has been held on calling functions
void LocationApiService::suspendAllTrackingSessions() {
    for (auto client : mClients) {
        // stop session if running
        if (client.second && client.second->mTracking) {
            client.second->stopTracking();
            client.second->mPendingMessages.push(E_LOCAPI_STOP_TRACKING_MSG_ID);
            LOC_LOGi("--> suspended");
        }
    }
}

// no need to hold the lock as lock has been held on calling functions
void LocationApiService::resumeAllTrackingSessions() {
    for (auto client : mClients) {
        // start session if not running
        if (client.second && client.second->mTracking) {

            // resume session with preserved options
            if (!client.second->startTracking()) {
                LOC_LOGe("Failed to start session");
                return;
            }
            // success
            client.second->mPendingMessages.push(E_LOCAPI_START_TRACKING_MSG_ID);
            LOC_LOGi("--> resumed");
        }
    }
}

void LocationApiService::updateSubscription(LocAPIUpdateCallbacksReqMsg *pMsg) {

    std::lock_guard<std::mutex> lock(mMutex);
    LocHalDaemonClientHandler* pClient = getClient(pMsg->mSocketName);
    if (!pClient) {
        LOC_LOGe(">-- updateSubscription invlalid client=%s", pMsg->mSocketName);
        return;
    }

    pClient->updateSubscription(pMsg->locationCallbacks);

    LOC_LOGi(">-- update subscription client=%s mask=0x%x",
            pMsg->mSocketName, pMsg->locationCallbacks);
}

void LocationApiService::updateTrackingOptions(LocAPIUpdateTrackingOptionsReqMsg *pMsg) {

    std::lock_guard<std::mutex> lock(mMutex);

    LocHalDaemonClientHandler* pClient = getClient(pMsg->mSocketName);
    if (pClient) {
        LocationOptions locationOption = pMsg->locOptions;
        // set the mode according to the master position mode
        locationOption.mode = mPositionMode;
        pClient->updateTrackingOptions(locationOption);
        pClient->mPendingMessages.push(E_LOCAPI_UPDATE_TRACKING_OPTIONS_MSG_ID);
    }

    LOC_LOGi(">-- update tracking options");
}

void LocationApiService::updateNetworkAvailability(bool availability) {

    LOC_LOGi(">-- updateNetworkAvailability=%u", availability);
    std::string apn("");
    GnssInterface* gnssInterface = getGnssInterface();
    if (gnssInterface) {
        // Map the network connectivity to MOBILE for now.
        // In next phase, when we support third party connectivity manager,
        // we plan to deplicate this API.
        gnssInterface->updateConnectionStatus(
                availability, loc_core::TYPE_MOBILE, false, NETWORK_HANDLE_UNKNOWN, apn);
    }
}

void LocationApiService::getGnssEnergyConsumed(const char* clientSocketName) {

    LOC_LOGi(">-- getGnssEnergyConsumed by=%s", clientSocketName);

    GnssInterface* gnssInterface = getGnssInterface();
    if (!gnssInterface) {
        LOC_LOGe(">-- getGnssEnergyConsumed null GnssInterface");
        return;
    }

    std::lock_guard<std::mutex> lock(mMutex);
    bool requestAlreadyPending = false;
    for (auto each : mClients) {
        if ((each.second != nullptr) &&
            (each.second->hasPendingEngineInfoRequest(E_ENGINE_INFO_CB_GNSS_ENERGY_CONSUMED_BIT))) {
            requestAlreadyPending = true;
            break;
        }
    }

    std::string clientname(clientSocketName);
    LocHalDaemonClientHandler* pClient = getClient(clientname);
    if (pClient) {
        pClient->addEngineInfoRequst(E_ENGINE_INFO_CB_GNSS_ENERGY_CONSUMED_BIT);

        // this is first client coming to request GNSS energy consumed
        if (requestAlreadyPending == false) {
            LOC_LOGd("--< issue request to GNSS HAL");

            // callback function when gpss engine report back energy consumed info
            GnssEnergyConsumedCallback reportEnergyCb =
                [this](uint64_t total) {
                    onGnssEnergyConsumedCb(total);
                };

            gnssInterface->getGnssEnergyConsumed(reportEnergyCb);
        }
    }
}

void LocationApiService::getConstellationSecondaryBandConfig(
        const LocConfigGetConstellationSecondaryBandConfigReqMsg* pReqMsg) {

    LOC_LOGi(">--getConstellationConfig");
    GnssInterface* gnssInterface = getGnssInterface();
    if (!gnssInterface) {
        LOC_LOGe(">-- null GnssInterface");
        return;
    }

    std::lock_guard<std::mutex> lock(mMutex);
    // retrieve the constellation enablement/disablement config
    // blacklisted SV info and secondary band config
    uint32_t sessionId = gnssInterface-> gnssGetSecondaryBandConfig();

    // if sessionId is 0, e.g.: error callback will be delivered
    // by addConfigRequestToMap
    addConfigRequestToMap(sessionId, pReqMsg);
}

/******************************************************************************
LocationApiService - implementation - batching
******************************************************************************/
void LocationApiService::startBatching(LocAPIStartBatchingReqMsg *pMsg) {

    std::lock_guard<std::mutex> lock(mMutex);
    LocHalDaemonClientHandler* pClient = getClient(pMsg->mSocketName);
    if (!pClient) {
        LOC_LOGe(">-- start invalid client=%s", pMsg->mSocketName);
        return;
    }

    if (!pClient->startBatching(pMsg->intervalInMs, pMsg->distanceInMeters,
                pMsg->batchingMode)) {
        LOC_LOGe("Failed to start session");
        return;
    }
    // success
    pClient->mBatching = true;
    pClient->mBatchingMode = pMsg->batchingMode;
    pClient->mPendingMessages.push(E_LOCAPI_START_BATCHING_MSG_ID);

    LOC_LOGi(">-- start batching session");
    return;
}

void LocationApiService::stopBatching(LocAPIStopBatchingReqMsg *pMsg) {
    std::lock_guard<std::mutex> lock(mMutex);
    LocHalDaemonClientHandler* pClient = getClient(pMsg->mSocketName);
    if (!pClient) {
        LOC_LOGe(">-- stop invalid client=%s", pMsg->mSocketName);
        return;
    }

    pClient->mBatching = false;
    pClient->mBatchingMode = BATCHING_MODE_NO_AUTO_REPORT;
    pClient->updateSubscription(0);
    pClient->stopBatching();
    pClient->mPendingMessages.push(E_LOCAPI_STOP_BATCHING_MSG_ID);
    LOC_LOGi(">-- stopping batching session");
}

void LocationApiService::updateBatchingOptions(LocAPIUpdateBatchingOptionsReqMsg *pMsg) {
    std::lock_guard<std::mutex> lock(mMutex);
    LocHalDaemonClientHandler* pClient = getClient(pMsg->mSocketName);
    if (pClient) {
        pClient->updateBatchingOptions(pMsg->intervalInMs, pMsg->distanceInMeters,
                pMsg->batchingMode);
        pClient->mPendingMessages.push(E_LOCAPI_UPDATE_BATCHING_OPTIONS_MSG_ID);
    }

    LOC_LOGi(">-- update batching options");
}

/******************************************************************************
LocationApiService - implementation - geofence
******************************************************************************/
void LocationApiService::addGeofences(LocAPIAddGeofencesReqMsg* pMsg) {
    std::lock_guard<std::mutex> lock(mMutex);
    LocHalDaemonClientHandler* pClient = getClient(pMsg->mSocketName);
    if (!pClient) {
        LOC_LOGe(">-- start invlalid client=%s", pMsg->mSocketName);
        return;
    }
    if (pMsg->geofences.count > MAX_GEOFENCE_COUNT) {
        LOC_LOGe(">-- geofence count greater than MAX =%d", pMsg->geofences.count);
        return;
    }
    GeofenceOption* gfOptions =
            (GeofenceOption*)malloc(pMsg->geofences.count * sizeof(GeofenceOption));
    GeofenceInfo* gfInfos = (GeofenceInfo*)malloc(pMsg->geofences.count * sizeof(GeofenceInfo));
    uint32_t* clientIds = (uint32_t*)malloc(pMsg->geofences.count * sizeof(uint32_t));
    if ((nullptr == gfOptions) || (nullptr == gfInfos) || (nullptr == clientIds)) {
        LOC_LOGe("Failed to malloc memory!");
        if (clientIds != nullptr) {
            free(clientIds);
        }
        if (gfInfos != nullptr) {
            free(gfInfos);
        }
        if (gfOptions != nullptr) {
            free(gfOptions);
        }
        return;
    }

    for(int i=0; i < pMsg->geofences.count; ++i) {
        gfOptions[i] = pMsg->geofences.gfPayload[i].gfOption;
        gfInfos[i] = pMsg->geofences.gfPayload[i].gfInfo;
        clientIds[i] = pMsg->geofences.gfPayload[i].gfClientId;
    }

    uint32_t* sessions = pClient->addGeofences(pMsg->geofences.count, gfOptions, gfInfos);
    if (!sessions) {
        LOC_LOGe("Failed to add geofences");
        free(clientIds);
        free(gfInfos);
        free(gfOptions);
        return;
    }
    pClient->setGeofenceIds(pMsg->geofences.count, clientIds, sessions);
    // success
    pClient->mGfPendingMessages.push(E_LOCAPI_ADD_GEOFENCES_MSG_ID);

    LOC_LOGi(">-- add geofences");
    free(clientIds);
    free(gfInfos);
    free(gfOptions);
}

void LocationApiService::removeGeofences(LocAPIRemoveGeofencesReqMsg* pMsg) {
    std::lock_guard<std::mutex> lock(mMutex);
    LocHalDaemonClientHandler* pClient = getClient(pMsg->mSocketName);
    if (nullptr == pClient) {
        LOC_LOGe("removeGeofences - Null client!");
        return;
    }
    uint32_t* sessions = pClient->getSessionIds(pMsg->gfClientIds.count, pMsg->gfClientIds.gfIds);
    if (pClient && sessions) {
        pClient->removeGeofences(pMsg->gfClientIds.count, sessions);
        pClient->mGfPendingMessages.push(E_LOCAPI_REMOVE_GEOFENCES_MSG_ID);
    }

    LOC_LOGi(">-- remove geofences");
    free(sessions);
}
void LocationApiService::modifyGeofences(LocAPIModifyGeofencesReqMsg* pMsg) {
    std::lock_guard<std::mutex> lock(mMutex);
    LocHalDaemonClientHandler* pClient = getClient(pMsg->mSocketName);
    if (nullptr == pClient) {
        LOC_LOGe("modifyGeofences - Null client!");
        return;
    }
    if (pMsg->geofences.count > MAX_GEOFENCE_COUNT) {
        LOC_LOGe("modifyGeofences - geofence count greater than MAX =%d", pMsg->geofences.count);
        return;
    }
    GeofenceOption* gfOptions = (GeofenceOption*)
            malloc(sizeof(GeofenceOption) * pMsg->geofences.count);
    uint32_t* clientIds = (uint32_t*)malloc(sizeof(uint32_t) * pMsg->geofences.count);
    if (nullptr == gfOptions || nullptr == clientIds) {
        LOC_LOGe("Failed to malloc memory!");
        if (clientIds != nullptr) {
            free(clientIds);
        }
        if (gfOptions != nullptr) {
            free(gfOptions);
        }
        return;
    }
    for (int i=0; i<pMsg->geofences.count; ++i) {
        gfOptions[i] = pMsg->geofences.gfPayload[i].gfOption;
        clientIds[i] = pMsg->geofences.gfPayload[i].gfClientId;
    }
    uint32_t* sessions = pClient->getSessionIds(pMsg->geofences.count, clientIds);

    if (pClient && sessions) {
        pClient->modifyGeofences(pMsg->geofences.count, sessions, gfOptions);
        pClient->mGfPendingMessages.push(E_LOCAPI_MODIFY_GEOFENCES_MSG_ID);
    }

    LOC_LOGi(">-- modify geofences");
    free(sessions);
    free(clientIds);
    free(gfOptions);
}
void LocationApiService::pauseGeofences(LocAPIPauseGeofencesReqMsg* pMsg) {
    std::lock_guard<std::mutex> lock(mMutex);
    LocHalDaemonClientHandler* pClient = getClient(pMsg->mSocketName);
    if (nullptr == pClient) {
        LOC_LOGe("pauseGeofences - Null client!");
        return;
    }
    uint32_t* sessions = pClient->getSessionIds(pMsg->gfClientIds.count, pMsg->gfClientIds.gfIds);
    if (pClient && sessions) {
        pClient->pauseGeofences(pMsg->gfClientIds.count, sessions);
        pClient->mGfPendingMessages.push(E_LOCAPI_PAUSE_GEOFENCES_MSG_ID);
    }

    LOC_LOGi(">-- pause geofences");
    free(sessions);
}
void LocationApiService::resumeGeofences(LocAPIResumeGeofencesReqMsg* pMsg) {
    std::lock_guard<std::mutex> lock(mMutex);
    LocHalDaemonClientHandler* pClient = getClient(pMsg->mSocketName);
    if (nullptr == pClient) {
        LOC_LOGe("resumeGeofences - Null client!");
        return;
    }
    uint32_t* sessions = pClient->getSessionIds(pMsg->gfClientIds.count, pMsg->gfClientIds.gfIds);
    if (pClient && sessions) {
        pClient->resumeGeofences(pMsg->gfClientIds.count, sessions);
        pClient->mGfPendingMessages.push(E_LOCAPI_RESUME_GEOFENCES_MSG_ID);
    }

    LOC_LOGi(">-- resume geofences");
    free(sessions);
}

void LocationApiService::pingTest(LocAPIPingTestReqMsg* pMsg) {

    // test only - ignore this request when config is not enabled
    std::lock_guard<std::mutex> lock(mMutex);
    LocHalDaemonClientHandler* pClient = getClient(pMsg->mSocketName);
    if (!pClient) {
        LOC_LOGe(">-- pingTest invlalid client=%s", pMsg->mSocketName);
        return;
    }
    pClient->pingTest();
    LOC_LOGd(">-- pingTest");
}

void LocationApiService::configConstrainedTunc(
        const LocConfigConstrainedTuncReqMsg* pMsg){

    if (!pMsg) {
        return;
    }
    std::lock_guard<std::mutex> lock(mMutex);
    uint32_t sessionId = mLocationControlApi->configConstrainedTimeUncertainty(
            pMsg->mEnable, pMsg->mTuncConstraint, pMsg->mEnergyBudget);
    LOC_LOGi(">-- enable: %d, tunc constraint %f, energy budget %d, session ID = %d",
             pMsg->mEnable, pMsg->mTuncConstraint, pMsg->mEnergyBudget,
             sessionId);
    addConfigRequestToMap(sessionId, pMsg);
}

void LocationApiService::configPositionAssistedClockEstimator(
        const LocConfigPositionAssistedClockEstimatorReqMsg* pMsg)
{
    std::lock_guard<std::mutex> lock(mMutex);
    if (!pMsg || !mLocationControlApi) {
        return;
    }

    uint32_t sessionId = mLocationControlApi->
            configPositionAssistedClockEstimator(pMsg->mEnable);
    LOC_LOGi(">-- enable: %d, session ID = %d", pMsg->mEnable,  sessionId);

    addConfigRequestToMap(sessionId, pMsg);
}

void LocationApiService::configConstellations(const LocConfigSvConstellationReqMsg* pMsg) {

    if (!pMsg) {
        return;
    }
    std::lock_guard<std::mutex> lock(mMutex);

    uint32_t sessionId = mLocationControlApi->configConstellations(
            pMsg->mConstellationEnablementConfig, pMsg->mBlacklistSvConfig);

    LOC_LOGi(">-- reset sv type config: %d, enable constellations: 0x%" PRIx64 ", "
             "blacklisted consteallations: 0x%" PRIx64 ", ",
             (pMsg->mConstellationEnablementConfig.size == 0),
             pMsg->mConstellationEnablementConfig.enabledSvTypesMask,
             pMsg->mConstellationEnablementConfig.blacklistedSvTypesMask);
    addConfigRequestToMap(sessionId, pMsg);
}

void LocationApiService::configConstellationSecondaryBand(
        const LocConfigConstellationSecondaryBandReqMsg* pMsg) {

    if (!pMsg) {
        return;
    }
    std::lock_guard<std::mutex> lock(mMutex);

    uint32_t sessionId = mLocationControlApi->configConstellationSecondaryBand(
            pMsg->mSecondaryBandConfig);

    LOC_LOGi(">-- secondary band size %d, enabled constellation: 0x%" PRIx64 ", "
             "secondary band disabed constellation: 0x%" PRIx64 "",
             pMsg->mSecondaryBandConfig.size,
             pMsg->mSecondaryBandConfig.enabledSvTypesMask,
             pMsg->mSecondaryBandConfig.blacklistedSvTypesMask);
    addConfigRequestToMap(sessionId, pMsg);
}

void LocationApiService::configAidingDataDeletion(LocConfigAidingDataDeletionReqMsg* pMsg) {

    if (!pMsg) {
        return;
    }
    std::lock_guard<std::mutex> lock(mMutex);

    LOC_LOGi(">-- client %s, deleteAll %d",
             pMsg->mSocketName, pMsg->mAidingData.deleteAll);

    // suspend all sessions before calling delete
    suspendAllTrackingSessions();

    uint32_t sessionId = mLocationControlApi->gnssDeleteAidingData(pMsg->mAidingData);
    addConfigRequestToMap(sessionId, pMsg);

#ifdef POWERMANAGER_ENABLED
    // We do not need to resume the session if device is suspend/shutdown state
    // as sessions will resumed when power state changes to resume
    if ((POWER_STATE_SUSPEND == mPowerState) ||
        (POWER_STATE_SHUTDOWN == mPowerState)) {
        return;
    }
#endif

    // resume all sessions after calling aiding data deletion
    resumeAllTrackingSessions();
}

void LocationApiService::configLeverArm(const LocConfigLeverArmReqMsg* pMsg){

    if (!pMsg) {
        return;
    }
    std::lock_guard<std::mutex> lock(mMutex);

    uint32_t sessionId = mLocationControlApi->configLeverArm(pMsg->mLeverArmConfigInfo);
    addConfigRequestToMap(sessionId, pMsg);
}

void LocationApiService::configRobustLocation(const LocConfigRobustLocationReqMsg* pMsg){

    if (!pMsg) {
        return;
    }
    std::lock_guard<std::mutex> lock(mMutex);

    LOC_LOGi(">-- client %s, enable %d, enableForE911 %d",
             pMsg->mSocketName, pMsg->mEnable, pMsg->mEnableForE911);

    uint32_t sessionId = mLocationControlApi->configRobustLocation(
            pMsg->mEnable, pMsg->mEnableForE911);
    addConfigRequestToMap(sessionId, pMsg);
}

void LocationApiService::configMinGpsWeek(const LocConfigMinGpsWeekReqMsg* pMsg){

    if (!pMsg) {
        return;
    }
    std::lock_guard<std::mutex> lock(mMutex);

    LOC_LOGi(">-- client %s, minGpsWeek %u",
             pMsg->mSocketName, pMsg->mMinGpsWeek);

    uint32_t sessionId =
            mLocationControlApi->configMinGpsWeek(pMsg->mMinGpsWeek);
    addConfigRequestToMap(sessionId, pMsg);
}

void LocationApiService::configMinSvElevation(const LocConfigMinSvElevationReqMsg* pMsg){

    if (!pMsg) {
        return;
    }
    std::lock_guard<std::mutex> lock(mMutex);
    LOC_LOGi(">-- client %s, minSvElevation %u", pMsg->mSocketName, pMsg->mMinSvElevation);

    GnssConfig gnssConfig = {};
    gnssConfig.flags = GNSS_CONFIG_FLAGS_MIN_SV_ELEVATION_BIT;
    gnssConfig.minSvElevation = pMsg->mMinSvElevation;
    uint32_t sessionId = gnssUpdateConfig(gnssConfig);

    addConfigRequestToMap(sessionId, pMsg);
}

void LocationApiService::configEngineRunState(const LocConfigEngineRunStateReqMsg* pMsg) {
    if (!pMsg) {
        return;
    }
    std::lock_guard<std::mutex> lock(mMutex);

    LOC_LOGi(">-- client %s, eng type 0x%x, eng state %d",
             pMsg->mSocketName, pMsg->mEngType, pMsg->mEngState);
    uint32_t sessionId =
            mLocationControlApi->configEngineRunState(pMsg->mEngType, pMsg->mEngState);
    addConfigRequestToMap(sessionId, pMsg);
}

void LocationApiService::configUserConsentTerrestrialPositioning(
        LocConfigUserConsentTerrestrialPositioningReqMsg* pMsg) {
    if (!pMsg) {
        return;
    }
    std::lock_guard<std::mutex> lock(mMutex);

    LOC_LOGi(">-- client %s, current user consent %d, new usr consent %d",
             pMsg->mSocketName, mOptInTerrestrialService, pMsg->mUserConsent);

    mOptInTerrestrialService = pMsg->mUserConsent;
    if ((mOptInTerrestrialService == true) && (mGtpWwanSsLocationApi == nullptr)) {
        // set callback functions for Location API
        mGtpWwanSsLocationApiCallbacks.size = sizeof(mGtpWwanSsLocationApiCallbacks);

        // mandatory callback
        mGtpWwanSsLocationApiCallbacks.capabilitiesCb = [this](LocationCapabilitiesMask mask) {
            onCapabilitiesCallback(mask);
        };
        mGtpWwanSsLocationApiCallbacks.responseCb = [this](LocationError err, uint32_t id) {
            onResponseCb(err, id);
        };
        mGtpWwanSsLocationApiCallbacks.collectiveResponseCb =
                [this](size_t count, LocationError* errs, uint32_t* ids) {
            onCollectiveResponseCallback(count, errs, ids);
        };

        mGtpWwanPosCallback = [this](Location location) {
            onGtpWwanTrackingCallback(location);
        };

        mGtpWwanSsLocationApi = LocationAPI::createInstance(mGtpWwanSsLocationApiCallbacks);
        if (mGtpWwanSsLocationApi) {
            mGtpWwanSsLocationApi->enableNetworkProvider();
        }
    }

    uint32_t sessionId = mLocationControlApi->setOptInStatus(pMsg->mUserConsent);
    addConfigRequestToMap(sessionId, pMsg);
}

void LocationApiService::getGnssConfig(const LocAPIMsgHeader* pReqMsg,
                                       GnssConfigFlagsBits configFlag) {

    std::lock_guard<std::mutex> lock(mMutex);
    if (!pReqMsg) {
        return;
    }

    uint32_t sessionId = 0;
    uint32_t* sessionIds = mLocationControlApi->gnssGetConfig(
                configFlag);
    if (sessionIds) {
        LOC_LOGd(">-- session id %d", *sessionIds);
        sessionId = *sessionIds;
        // free the memory used by sessionIds
        delete[] sessionIds;
    }
    // if sessionId is 0, e.g.: error callback will be delivered
    // by addConfigRequestToMap
    addConfigRequestToMap(sessionId, pReqMsg);
}

void LocationApiService::configDeadReckoningEngineParams(const LocConfigDrEngineParamsReqMsg* pMsg){
    if (!pMsg) {
        return;
    }
    std::lock_guard<std::mutex> lock(mMutex);
    uint32_t sessionId = mLocationControlApi->configDeadReckoningEngineParams(
            pMsg->mDreConfig);
    addConfigRequestToMap(sessionId, pMsg);
}

void LocationApiService::addConfigRequestToMap(
        uint32_t sessionId, const LocAPIMsgHeader* pMsg){
    // for config request that is invoked from location integration API
    // if session id is valid, we need to add it to the map so when response
    // comes back, we can deliver the response to the integration api client
    if (sessionId != 0) {
        ConfigReqClientData configClientData;
        configClientData.configMsgId = pMsg->msgId;
        configClientData.clientName  = pMsg->mSocketName;
        mConfigReqs.emplace(sessionId, configClientData);
    } else {
        // if session id is 0, we need to deliver failed response back to the
        // client
        LocHalDaemonClientHandler* pClient = getClient(pMsg->mSocketName);
        if (pClient) {
            pClient->onControlResponseCb(LOCATION_ERROR_GENERAL_FAILURE, pMsg->msgId);
        }
    }
}

/******************************************************************************
LocationApiService - Location Control API callback functions
******************************************************************************/
void LocationApiService::onControlResponseCallback(LocationError err, uint32_t sessionId) {
    std::lock_guard<std::mutex> lock(mMutex);
    LOC_LOGd("--< onControlResponseCallback err=%u id=%u", err, sessionId);

    auto configReqData = mConfigReqs.find(sessionId);
    if (configReqData != std::end(mConfigReqs)) {
        LocHalDaemonClientHandler* pClient = getClient(configReqData->second.clientName);
        if (pClient) {
            pClient->onControlResponseCb(err, configReqData->second.configMsgId);
        }
        mConfigReqs.erase(configReqData);
        LOC_LOGd("--< map size %d", mConfigReqs.size());
    } else {
        LOC_LOGe("--< client not found for session id %d", sessionId);
    }
}

void LocationApiService::onControlCollectiveResponseCallback(
    size_t count, LocationError *errs, uint32_t *ids) {
    std::lock_guard<std::mutex> lock(mMutex);
    if (count != 1) {
        LOC_LOGe("--< onControlCollectiveResponseCallback, count is %d, expecting 1", count);
        return;
    }

    uint32_t sessionId = *ids;
    LocationError err = *errs;
    LOC_LOGd("--< onControlCollectiveResponseCallback, session id is %d, err is %d",
             sessionId, err);
    // as we only update one setting at a time, we only need to process
    // the first id
    auto configReqData = mConfigReqs.find(sessionId);
    if (configReqData != std::end(mConfigReqs)) {
        LocHalDaemonClientHandler* pClient = getClient(configReqData->second.clientName.c_str());
        if (pClient) {
            pClient->onControlResponseCb(err, configReqData->second.configMsgId);
        }
        mConfigReqs.erase(configReqData);
        LOC_LOGd("--< map size %d", mConfigReqs.size());
    } else {
        LOC_LOGe("--< client not found for session id %d", sessionId);
    }
}

void LocationApiService::onGnssConfigCallback(uint32_t sessionId,
                                              const GnssConfig& config) {
    std::lock_guard<std::mutex> lock(mMutex);
    LOC_LOGd("--< onGnssConfigCallback, req cnt %d", mConfigReqs.size());

    auto configReqData = mConfigReqs.find(sessionId);
    if (configReqData != std::end(mConfigReqs)) {
        LocHalDaemonClientHandler* pClient = getClient(configReqData->second.clientName);
        if (pClient) {
            // invoke the respCb to deliver success status
            pClient->onControlResponseCb(LOCATION_ERROR_SUCCESS, configReqData->second.configMsgId);
            // invoke the configCb to deliver the config
            pClient->onGnssConfigCb(configReqData->second.configMsgId, config);
        }
        mConfigReqs.erase(configReqData);
        LOC_LOGd("--< map size %d", mConfigReqs.size());
    } else {
        LOC_LOGe("--< client not found for session id %d", sessionId);
    }
}
// mandatory callback for location api
void LocationApiService::onCapabilitiesCallback(LocationCapabilitiesMask mask) {
}

// mandatory callback for location api
void LocationApiService::onResponseCb(LocationError err, uint32_t id) {
}

// mandatory callback for location api
void LocationApiService::onCollectiveResponseCallback(
        size_t count, LocationError *errs, uint32_t *ids) {
}

void LocationApiService::onGtpWwanTrackingCallback(Location location) {
    std::lock_guard<std::mutex> lock(mMutex);
    LOC_LOGd("--< onGtpWwanTrackingCallback optIn=%u loc flags=0x%x", mOptInTerrestrialService,
            location.flags);

    if ((mTerrestrialFixReqs.size() != 0) &&
            (location.flags & LOCATION_HAS_LAT_LONG_BIT) && (mOptInTerrestrialService == 1)) {

        for (auto it = mTerrestrialFixReqs.begin(); it != mTerrestrialFixReqs.end();) {
            LocHalDaemonClientHandler* pClient = getClient(it->first);
            pClient->sendTerrestrialFix(LOCATION_ERROR_SUCCESS, location);
            ++it;
        }
        mTerrestrialFixReqs.clear();
        mGtpWwanSsLocationApi->stopNetworkLocation(&mGtpWwanPosCallback);
    }
}

/******************************************************************************
LocationApiService - power event handlers
******************************************************************************/
#ifdef POWERMANAGER_ENABLED
void LocationApiService::onPowerEvent(PowerStateType powerState) {
    std::lock_guard<std::mutex> lock(mMutex);
    LOC_LOGd("--< onPowerEvent %d", powerState);

    mPowerState = powerState;
    if ((POWER_STATE_SUSPEND == powerState) ||
            (POWER_STATE_SHUTDOWN == powerState)) {
        suspendAllTrackingSessions();
    } else if (POWER_STATE_RESUME == powerState) {
        resumeAllTrackingSessions();
    }

    GnssInterface* gnssInterface = getGnssInterface();
    if (!gnssInterface) {
        LOC_LOGe(">-- getGnssEnergyConsumed null GnssInterface");
        return;
    }
    gnssInterface->updateSystemPowerState(powerState);
}
#endif

/******************************************************************************
LocationApiService - on query callback from location engines
******************************************************************************/
void LocationApiService::onGnssEnergyConsumedCb(uint64_t totalGnssEnergyConsumedSinceFirstBoot) {
    std::lock_guard<std::mutex> lock(mMutex);
    LOC_LOGd("--< onGnssEnergyConsumedCb");

    LocAPIGnssEnergyConsumedIndMsg msg(SERVICE_NAME, totalGnssEnergyConsumedSinceFirstBoot,
            &mPbufMsgConv);
    for (auto each : mClients) {
        // deliver the engergy info to registered client
        each.second->onGnssEnergyConsumedInfoAvailable(msg);
    }
}

/******************************************************************************
LocationApiService - other utilities
******************************************************************************/
GnssInterface* LocationApiService::getGnssInterface() {

    static bool getGnssInterfaceFailed = false;
    static GnssInterface* gnssInterface = nullptr;

    if (nullptr == gnssInterface && !getGnssInterfaceFailed) {
        void * tempPtr = nullptr;
        getLocationInterface* getter = (getLocationInterface*)
                dlGetSymFromLib(tempPtr, "libgnss.so", "getGnssInterface");

        if (nullptr == getter) {
            getGnssInterfaceFailed = true;
        } else {
            gnssInterface = (GnssInterface*)(*getter)();
        }
    }
    return gnssInterface;
}

void LocationApiService::performMaintenance() {
    ClientNameIpcSenderMap   clientsToCheck;

    // Hold the lock when we access global variable of mClients
    // copy out the client name and shared_ptr of ipc sender for the clients.
    // We do not use mClients directly or making a copy of mClients, as the
    // client handler object can become invalid when the client gets
    // deleted by the thread of LocationApiService.
    {
        std::lock_guard<std::mutex> lock(mMutex);
        for (auto client : mClients) {
            if (client.first.compare(AUTO_START_CLIENT_NAME) != 0) {
                clientsToCheck.emplace(client.first, client.second->getIpcSender());
            }
        }
    }

    for (auto client : clientsToCheck) {
        string pbStr;
        bool messageSent = false;
        LocAPIPingTestReqMsg msg(SERVICE_NAME, &mPbufMsgConv);
        if (msg.serializeToProtobuf(pbStr)) {
            messageSent = LocIpc::send(*client.second,
                    reinterpret_cast<uint8_t *>((uint8_t *)pbStr.c_str()), pbStr.size());
        } else {
            LOC_LOGe("LocAPIPingTestReqMsg serializeToProtobuf failed");
        }
        LOC_LOGd("send ping message returned %d for client %s", messageSent, client.first.c_str());
        if (messageSent == false) {
            LOC_LOGe("--< ping failed for client %s", client.first.c_str());
            deleteClientbyName(client.first);
        }
    }

    // after maintenace, start next timer
    mMaintTimer.start(MAINT_TIMER_INTERVAL_MSEC, false);
}


// Maintenance timer to clean up resources when client exists without sending
// out de-registration message
void MaintTimer::timeOutCallback() {
    LOC_LOGd("maint timer fired");

    struct PerformMaintenanceReq : public LocMsg {
        PerformMaintenanceReq(LocationApiService* locationApiService) :
                mLocationApiService(locationApiService){}
        virtual ~PerformMaintenanceReq() {}
        void proc() const {
            mLocationApiService->performMaintenance();
        }
        LocationApiService* mLocationApiService;
    };
    mLocationApiService->getMsgTask().sendMsg(new PerformMaintenanceReq(mLocationApiService));
}

/******************************************************************************
LocationApiService - GTP WWAN functionality
******************************************************************************/
void LocationApiService::getSingleTerrestrialPos(
        LocAPIGetSingleTerrestrialPosReqMsg* pReqMsg) {

    std::string clientName(pReqMsg->mSocketName);

    LOC_LOGd(">--getSingleTerrestrialPos, timeout msec %d, tech mask 0x%x, horQoS %f",
             pReqMsg->mTimeoutMsec, pReqMsg->mTechMask, pReqMsg->mHorQoS);

    std::lock_guard<std::mutex> lock(mMutex);
    // Make sure client has opt-in for the service
    if (mOptInTerrestrialService != 1) {
        LocHalDaemonClientHandler* pClient = getClient(clientName);
        if (pClient) {
            // inform client that GTP service is not supported
            Location location = {};
            pClient->sendTerrestrialFix(LOCATION_ERROR_NOT_SUPPORTED, location);
        }
    } else {
        mTerrestrialFixReqs.emplace(std::piecewise_construct,
                                    std::forward_as_tuple(clientName),
                                    std::forward_as_tuple(this, clientName));

        auto it = mTerrestrialFixReqs.find(clientName);
        if (it != mTerrestrialFixReqs.end()) {
            it->second.start(pReqMsg->mTimeoutMsec, false);
        }

        if (mTerrestrialFixReqs.size() == 1) {
            mGtpWwanSsLocationApi->startNetworkLocation(&mGtpWwanPosCallback);
        }
    }
}

void LocationApiService::gtpFixRequestTimeout(const std::string& clientName) {
    std::lock_guard<std::mutex> lock(LocationApiService::mMutex);

    LOC_LOGd("timer out processing for client %s", clientName.c_str());
    auto it = mTerrestrialFixReqs.find(clientName);
    if (it != mTerrestrialFixReqs.end()) {
        LocHalDaemonClientHandler* pClient = getClient(clientName);
        if (pClient) {
            // inform client of timeout
            Location location = {};
            pClient->sendTerrestrialFix(LOCATION_ERROR_TIMEOUT, location);
        }
        mTerrestrialFixReqs.erase(clientName);
        // stop tracking if there is no more request
        if (mTerrestrialFixReqs.size() == 1) {
            mGtpWwanSsLocationApi->stopNetworkLocation(&mGtpWwanPosCallback);
        }
    }
}

void SingleTerrestrialFixTimer::timeOutCallback() {
    LOC_LOGd("SingleTerrestrialFix timeout timer fired");

    struct SingleTerrestrialFixTimeoutReq : public LocMsg {
        SingleTerrestrialFixTimeoutReq(LocationApiService* locationApiService,
                                       const std::string &clientName) :
                mLocationApiService(locationApiService),
                mClientName(clientName) {}
        virtual ~SingleTerrestrialFixTimeoutReq() {}
        void proc() const {
            mLocationApiService->gtpFixRequestTimeout(mClientName);
        }
        LocationApiService* mLocationApiService;
        std::string         mClientName;
    };

    mLocationApiService->getMsgTask().sendMsg(new SingleTerrestrialFixTimeoutReq(
                mLocationApiService, mClientName));
}
