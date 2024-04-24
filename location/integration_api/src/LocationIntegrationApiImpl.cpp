/* Copyright (c) 2019-2020 The Linux Foundation. All rights reserved.
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

#define LOG_TAG "LocSvc_LocationIntegrationApi"

#include <sys/types.h>
#include <unistd.h>
#include <loc_cfg.h>
#include <LocationIntegrationApiImpl.h>
#include <log_util.h>
#include <gps_extended_c.h>

namespace location_integration {

/******************************************************************************
Utilities
******************************************************************************/

static LocConfigTypeEnum getLocConfigTypeFromMsgId(ELocMsgID  msgId) {
    LocConfigTypeEnum configType = (LocConfigTypeEnum) 0;
    switch (msgId) {
    case E_INTAPI_CONFIG_CONSTRAINTED_TUNC_MSG_ID:
        configType = CONFIG_CONSTRAINED_TIME_UNCERTAINTY;
        break;
    case E_INTAPI_CONFIG_POSITION_ASSISTED_CLOCK_ESTIMATOR_MSG_ID:
        configType = CONFIG_POSITION_ASSISTED_CLOCK_ESTIMATOR;
        break;
    case E_INTAPI_CONFIG_SV_CONSTELLATION_MSG_ID:
        configType = CONFIG_CONSTELLATIONS;
        break;
    case E_INTAPI_CONFIG_CONSTELLATION_SECONDARY_BAND_MSG_ID:
        configType = CONFIG_CONSTELLATION_SECONDARY_BAND;
        break;
    case E_INTAPI_CONFIG_AIDING_DATA_DELETION_MSG_ID:
        configType = CONFIG_AIDING_DATA_DELETION;
        break;
    case E_INTAPI_CONFIG_LEVER_ARM_MSG_ID:
        configType = CONFIG_LEVER_ARM;
        break;
    case E_INTAPI_CONFIG_ROBUST_LOCATION_MSG_ID:
        configType = CONFIG_ROBUST_LOCATION;
        break;
    case E_INTAPI_CONFIG_MIN_GPS_WEEK_MSG_ID:
        configType = CONFIG_MIN_GPS_WEEK;
        break;
    case E_INTAPI_CONFIG_DEAD_RECKONING_ENGINE_MSG_ID:
        configType = CONFIG_DEAD_RECKONING_ENGINE;
        break;
    case E_INTAPI_CONFIG_MIN_SV_ELEVATION_MSG_ID:
        configType = CONFIG_MIN_SV_ELEVATION;
        break;
    case E_INTAPI_CONFIG_ENGINE_RUN_STATE_MSG_ID:
        configType = CONFIG_ENGINE_RUN_STATE;
        break;
    case E_INTAPI_CONFIG_USER_CONSENT_TERRESTRIAL_POSITIONING_MSG_ID:
        configType = CONFIG_USER_CONSENT_TERRESTRIAL_POSITIONING;
        break;
    case E_INTAPI_GET_ROBUST_LOCATION_CONFIG_REQ_MSG_ID:
    case E_INTAPI_GET_ROBUST_LOCATION_CONFIG_RESP_MSG_ID:
        configType = GET_ROBUST_LOCATION_CONFIG;
        break;
    case E_INTAPI_GET_MIN_GPS_WEEK_REQ_MSG_ID:
    case E_INTAPI_GET_MIN_GPS_WEEK_RESP_MSG_ID:
        configType = GET_MIN_GPS_WEEK;
        break;
    case E_INTAPI_GET_MIN_SV_ELEVATION_REQ_MSG_ID:
    case E_INTAPI_GET_MIN_SV_ELEVATION_RESP_MSG_ID:
        configType = GET_MIN_SV_ELEVATION;
        break;
    case E_INTAPI_GET_CONSTELLATION_SECONDARY_BAND_CONFIG_REQ_MSG_ID:
    case E_INTAPI_GET_CONSTELLATION_SECONDARY_BAND_CONFIG_RESP_MSG_ID:
        configType = GET_CONSTELLATION_SECONDARY_BAND_CONFIG;
        break;
    default:
        break;
    }
    return configType;
}

static LocIntegrationResponse getLocIntegrationResponse(::LocationError error) {
    LocIntegrationResponse response;

    switch (error) {
        case LOCATION_ERROR_SUCCESS:
            response = LOC_INT_RESPONSE_SUCCESS;
            break;
        case LOCATION_ERROR_NOT_SUPPORTED:
            response = LOC_INT_RESPONSE_NOT_SUPPORTED;
            break;
        case LOCATION_ERROR_INVALID_PARAMETER:
            response = LOC_INT_RESPONSE_PARAM_INVALID;
            break;
        default:
            response = LOC_INT_RESPONSE_FAILURE;
            break;
    }

    return response;
}

/******************************************************************************
ILocIpcListener override
******************************************************************************/
class IpcListener : public ILocIpcListener {
    MsgTask& mMsgTask;
    LocationIntegrationApiImpl& mApiImpl;
    const SockNode::Type mSockTpye;
public:
    inline IpcListener(LocationIntegrationApiImpl& apiImpl, MsgTask& msgTask,
                       const SockNode::Type sockType) :
            mMsgTask(msgTask), mApiImpl(apiImpl), mSockTpye(sockType) {}
    virtual void onListenerReady() override;
    virtual void onReceive(const char* data, uint32_t length,
                           const LocIpcRecver* recver) override;
};

/******************************************************************************
LocIpcQrtrWatcher override
******************************************************************************/
class IpcQrtrWatcher : public LocIpcQrtrWatcher {
    const weak_ptr<IpcListener> mIpcListener;
    const weak_ptr<LocIpcSender> mIpcSender;
    LocIpcQrtrWatcher::ServiceStatus mKnownStatus;
    LocationApiPbMsgConv mPbufMsgConv;
public:
    inline IpcQrtrWatcher(shared_ptr<IpcListener>& listener, shared_ptr<LocIpcSender>& sender,
            LocationApiPbMsgConv& pbMsgConv) :
            LocIpcQrtrWatcher({LOCATION_CLIENT_API_QSOCKET_HALDAEMON_SERVICE_ID}),
            mIpcListener(listener), mIpcSender(sender), mPbufMsgConv(pbMsgConv),
            mKnownStatus(LocIpcQrtrWatcher::ServiceStatus::DOWN) {
    }
    inline virtual void onServiceStatusChange(int serviceId, int instanceId,
            LocIpcQrtrWatcher::ServiceStatus status, const LocIpcSender& refSender) {
        if (LOCATION_CLIENT_API_QSOCKET_HALDAEMON_SERVICE_ID == serviceId &&
            LOCATION_CLIENT_API_QSOCKET_HALDAEMON_INSTANCE_ID == instanceId) {
            if (mKnownStatus != status) {
                mKnownStatus = status;
                if (LocIpcQrtrWatcher::ServiceStatus::UP == status) {
                    LOC_LOGv("case LocIpcQrtrWatcher::ServiceStatus::UP");
                    auto sender = mIpcSender.lock();
                    if (nullptr != sender) {
                        sender->copyDestAddrFrom(refSender);
                    }
                    auto listener = mIpcListener.lock();
                    if (nullptr != listener) {
                        LocAPIHalReadyIndMsg msg(SERVICE_NAME, &mPbufMsgConv);
                        string pbStr;
                        if (msg.serializeToProtobuf(pbStr)) {
                            listener->onReceive(pbStr.c_str(), pbStr.size(), nullptr);
                        } else {
                            LOC_LOGe("LocAPIHalReadyIndMsg serializeToProtobuf failed");
                        }
                    }
                }
            }
        }
    }
};

/******************************************************************************
LocationIntegrationApiImpl
******************************************************************************/
mutex LocationIntegrationApiImpl::mMutex;
bool LocationIntegrationApiImpl::mClientRunning = false;

/******************************************************************************
LocationIntegrationApiImpl - constructors
******************************************************************************/
LocationIntegrationApiImpl::LocationIntegrationApiImpl(LocIntegrationCbs& integrationCbs) :
        mHalRegistered(false),
        mIntegrationCbs(integrationCbs),
        mTuncConfigInfo{},
        mPaceConfigInfo{},
        mSvConfigInfo{},
        mLeverArmConfigInfo{},
        mRobustLocationConfigInfo{},
        mDreConfigInfo{},
        mMsgTask("IntegrationApiImpl"),
        mGtpUserConsentConfigInfo{} {
    if (integrationClientAllowed() == false) {
        return;
    }

    // get pid to generate sokect name
    uint32_t pid = (uint32_t)getpid();

#ifdef FEATURE_EXTERNAL_AP
    SockNodeEap sock(LOCATION_CLIENT_API_QSOCKET_CLIENT_SERVICE_ID,
                     pid * 100);
    size_t pathNameLength = strlcpy(mSocketName, sock.getNodePathname().c_str(),
                                    sizeof(mSocketName));
    if (pathNameLength >= sizeof(mSocketName)) {
        LOC_LOGe("socket name length exceeds limit of %d bytes", sizeof(mSocketName));
        return;
    }
    // establish an ipc sender to the hal daemon
    mIpcSender = LocIpc::getLocIpcQrtrSender(LOCATION_CLIENT_API_QSOCKET_HALDAEMON_SERVICE_ID,
                                             LOCATION_CLIENT_API_QSOCKET_HALDAEMON_INSTANCE_ID);
    if (mIpcSender == nullptr) {
        LOC_LOGe("create sender socket failed for service id: %d instance id: %d",
                 LOCATION_CLIENT_API_QSOCKET_HALDAEMON_SERVICE_ID,
                 LOCATION_CLIENT_API_QSOCKET_HALDAEMON_INSTANCE_ID);
        return;
    }
    shared_ptr<IpcListener> listener(make_shared<IpcListener>(*this, mMsgTask, SockNode::Eap));
    unique_ptr<LocIpcRecver> recver = LocIpc::getLocIpcQrtrRecver(listener,
            sock.getId1(), sock.getId2(),
            make_shared<IpcQrtrWatcher>(listener, mIpcSender, mPbufMsgConv));
#else
    SockNodeLocal sock(LOCATION_INTEGRATION_API, pid, 0);
    size_t pathNameLength = strlcpy(mSocketName, sock.getNodePathname().c_str(),
                                    sizeof(mSocketName));
    if (pathNameLength >= sizeof(mSocketName)) {
        LOC_LOGe("socket name length exceeds limit of %d bytes", sizeof(mSocketName));
        return;
    }

    LOC_LOGd("create sender socket: %s", mSocketName);
    // establish an ipc sender to the hal daemon
    mIpcSender = LocIpc::getLocIpcLocalSender(SOCKET_TO_LOCATION_HAL_DAEMON);
    if (mIpcSender == nullptr) {
        LOC_LOGe("create sender socket failed %s", SOCKET_TO_LOCATION_HAL_DAEMON);
        return;
    }
    unique_ptr<LocIpcRecver> recver = LocIpc::getLocIpcLocalRecver(
            make_shared<IpcListener>(*this, mMsgTask, SockNode::Local), mSocketName);
#endif //  FEATURE_EXTERNAL_AP

    LOC_LOGd("listen on socket: %s", mSocketName);
    mIpc.startNonBlockingListening(recver);
}

LocationIntegrationApiImpl::~LocationIntegrationApiImpl() {
}

void LocationIntegrationApiImpl::destroy() {

    struct DestroyReq : public LocMsg {
        DestroyReq(LocationIntegrationApiImpl* apiImpl) :
                mApiImpl(apiImpl) {}
        virtual ~DestroyReq() {}
        void proc() const {
            // deregister
            if (mApiImpl->mHalRegistered && (nullptr != mApiImpl->mIpcSender)) {
                string pbStr;
                LocAPIClientDeregisterReqMsg msg(mApiImpl->mSocketName, &mApiImpl->mPbufMsgConv);
                if (msg.serializeToProtobuf(pbStr)) {
                    bool rc = mApiImpl->sendMessage(
                            reinterpret_cast<uint8_t *>((uint8_t *)pbStr.c_str()), pbStr.size());
                    LOC_LOGd(">>> DeregisterReq rc=%d", rc);
                    mApiImpl->mIpcSender = nullptr;
                } else {
                    LOC_LOGe("LocAPIClientDeregisterReqMsg serializeToProtobuf failed");
                }

            }

            // delete the integration client, so another integration client can be set
            {
                lock_guard<mutex> lock(mMutex);
                mApiImpl->mClientRunning = false;
            }
            delete mApiImpl;
        }
        LocationIntegrationApiImpl* mApiImpl;
    };

    mMsgTask.sendMsg(new (nothrow) DestroyReq(this));
}

bool LocationIntegrationApiImpl::integrationClientAllowed() {
    lock_guard<mutex> lock(mMutex);

    // allow only one instance of integration client running per process
    if (!mClientRunning) {
        mClientRunning = true;
        return true;
    } else {
         LOC_LOGe("there is already a running location integration api client in the process");
         return false;
    }
}

/******************************************************************************
LocationIntegrationApiImpl -ILocIpcListener
******************************************************************************/
void IpcListener::onListenerReady() {
    struct ClientRegisterReq : public LocMsg {
        ClientRegisterReq(LocationIntegrationApiImpl& apiImpl) : mApiImpl(apiImpl) {}
        void proc() const {
            mApiImpl.sendClientRegMsgToHalDaemon();
        }
        LocationIntegrationApiImpl& mApiImpl;
    };
    if (SockNode::Local == mSockTpye) {
        if (0 != chown(mApiImpl.mSocketName, getuid(), GID_LOCCLIENT)) {
            LOC_LOGe("chown to group locclient failed %s", strerror(errno));
        }
    }
    mMsgTask.sendMsg(new (nothrow) ClientRegisterReq(mApiImpl));
}

void IpcListener::onReceive(const char* data, uint32_t length,
                            const LocIpcRecver* recver) {
    struct OnReceiveHandler : public LocMsg {
        OnReceiveHandler(LocationIntegrationApiImpl& apiImpl, IpcListener& listener,
                         const char* data, uint32_t length) :
                mApiImpl(apiImpl), mListener(listener), mMsgData(data, length) {}


        virtual ~OnReceiveHandler() {}
        void proc() const {
            // Protobuff Encoding enabled, so we need to convert the message from proto
            // encoded format to local structure
            PBLocAPIMsgHeader pbLocApiMsg;
            if (0 == pbLocApiMsg.ParseFromString(mMsgData)) {
                LOC_LOGe("Failed to parse pbLocApiMsg from input stream!! length: %u",
                        mMsgData.length());
                return;
            }

            ELocMsgID eLocMsgid = mApiImpl.mPbufMsgConv.getEnumForPBELocMsgID(pbLocApiMsg.msgid());
            string sockName = pbLocApiMsg.msocketname();
            uint32_t msgVer = pbLocApiMsg.msgversion();
            uint32_t payloadSize = pbLocApiMsg.payloadsize();
            // pbLocApiMsg.payload() contains the payload data.

            LOC_LOGi(">-- onReceive Rcvd msg id: %d, sockname: %s, payload size: %d", eLocMsgid,
                    sockName.c_str(), payloadSize);
            LocAPIMsgHeader locApiMsg(sockName.c_str(), eLocMsgid);

            // throw away message that does not come from location hal daemon
            if (false == locApiMsg.isValidServerMsg(payloadSize)) {
                return;
            }

            switch (locApiMsg.msgId) {
            case E_LOCAPI_HAL_READY_MSG_ID:
                LOC_LOGd("<<< HAL ready");
                // location hal daemon crashed and re-started
                mApiImpl.processHalReadyMsg();
                break;

            case E_INTAPI_CONFIG_CONSTRAINTED_TUNC_MSG_ID:
            case E_INTAPI_CONFIG_POSITION_ASSISTED_CLOCK_ESTIMATOR_MSG_ID:
            case E_INTAPI_CONFIG_SV_CONSTELLATION_MSG_ID:
            case E_INTAPI_CONFIG_CONSTELLATION_SECONDARY_BAND_MSG_ID:
            case E_INTAPI_CONFIG_AIDING_DATA_DELETION_MSG_ID:
            case E_INTAPI_CONFIG_LEVER_ARM_MSG_ID:
            case E_INTAPI_CONFIG_ROBUST_LOCATION_MSG_ID:
            case E_INTAPI_CONFIG_MIN_GPS_WEEK_MSG_ID:
            case E_INTAPI_CONFIG_DEAD_RECKONING_ENGINE_MSG_ID:
            case E_INTAPI_CONFIG_MIN_SV_ELEVATION_MSG_ID:
            case E_INTAPI_CONFIG_ENGINE_RUN_STATE_MSG_ID:
            case E_INTAPI_CONFIG_USER_CONSENT_TERRESTRIAL_POSITIONING_MSG_ID:
            case E_INTAPI_GET_ROBUST_LOCATION_CONFIG_REQ_MSG_ID:
            case E_INTAPI_GET_MIN_GPS_WEEK_REQ_MSG_ID:
            case E_INTAPI_GET_MIN_SV_ELEVATION_REQ_MSG_ID:
            case E_INTAPI_GET_CONSTELLATION_SECONDARY_BAND_CONFIG_REQ_MSG_ID:
            {
                PBLocAPIGenericRespMsg pbLocApiGenericRsp;
                if (0 == pbLocApiGenericRsp.ParseFromString(pbLocApiMsg.payload())) {
                    LOC_LOGe("Failed to parse pbLocApiGenericRsp from payload!!");
                    return;
                }
                LocAPIGenericRespMsg msg(sockName.c_str(), eLocMsgid, pbLocApiGenericRsp,
                        &mApiImpl.mPbufMsgConv);
                mApiImpl.processConfigRespCb((LocAPIGenericRespMsg*)&msg);
                break;
            }

            case E_INTAPI_GET_MIN_GPS_WEEK_RESP_MSG_ID:
            {
                PBLocConfigGetMinGpsWeekRespMsg configGeMinGpsWeek;
                if (0 == configGeMinGpsWeek.ParseFromString(pbLocApiMsg.payload())) {
                    LOC_LOGe("Failed to parse configGeMinGpsWeek from payload!!");
                    return;
                }
                LocConfigGetMinGpsWeekRespMsg msg(sockName.c_str(), configGeMinGpsWeek,
                        &mApiImpl.mPbufMsgConv);
                mApiImpl.processGetMinGpsWeekRespCb((LocConfigGetMinGpsWeekRespMsg*)&msg);
                break;
            }

            case E_INTAPI_GET_ROBUST_LOCATION_CONFIG_RESP_MSG_ID:
            {
                PBLocConfigGetRobustLocationConfigRespMsg configGetRobustLoc;
                if (0 == configGetRobustLoc.ParseFromString(pbLocApiMsg.payload())) {
                    LOC_LOGe("Failed to parse configGetRobustLoc from payload!!");
                    return;
                }

                LocConfigGetRobustLocationConfigRespMsg msg(sockName.c_str(), configGetRobustLoc,
                        &mApiImpl.mPbufMsgConv);
                mApiImpl.processGetRobustLocationConfigRespCb(
                        (LocConfigGetRobustLocationConfigRespMsg*)&msg);
                break;
            }

            case E_INTAPI_GET_MIN_SV_ELEVATION_RESP_MSG_ID:
            {
                PBLocConfigGetMinSvElevationRespMsg configGetMinSvElev;
                if (0 == configGetMinSvElev.ParseFromString(pbLocApiMsg.payload())) {
                    LOC_LOGe("Failed to parse configGetMinSvElev from payload!!");
                    return;
                }

                LocConfigGetMinSvElevationRespMsg msg(sockName.c_str(), configGetMinSvElev,
                        &mApiImpl.mPbufMsgConv);
                mApiImpl.processGetMinSvElevationRespCb((LocConfigGetMinSvElevationRespMsg*)&msg);
                break;
            }

            case E_INTAPI_GET_CONSTELLATION_SECONDARY_BAND_CONFIG_RESP_MSG_ID:
            {
                PBLocConfigGetConstltnSecondaryBandConfigRespMsg cfgGetConstlnSecBandCfgRespMsg;
                if (0 == cfgGetConstlnSecBandCfgRespMsg.ParseFromString(pbLocApiMsg.payload())) {
                    LOC_LOGe("Failed to parse cfgGetConstlnSecBandCfgRespMsg from payload!!");
                    return;
                }

                LocConfigGetConstellationSecondaryBandConfigRespMsg msg(sockName.c_str(),
                        cfgGetConstlnSecBandCfgRespMsg, &mApiImpl.mPbufMsgConv);
                mApiImpl.processGetConstellationSecondaryBandConfigRespCb(
                        (LocConfigGetConstellationSecondaryBandConfigRespMsg*)&msg);
                break;
            }

            default:
                LOC_LOGw("<<< unknown message %d", locApiMsg.msgId);
                break;
            }
        }
        LocationIntegrationApiImpl& mApiImpl;
        IpcListener& mListener;
        const string mMsgData;
    };

    mMsgTask.sendMsg(new (nothrow) OnReceiveHandler(mApiImpl, *this, data, length));
}

/******************************************************************************
LocationIntegrationApi - integration API implementation
******************************************************************************/
uint32_t LocationIntegrationApiImpl::configConstellations(
        const GnssSvTypeConfig& constellationEnablementConfig,
        const GnssSvIdConfig& blacklistSvConfig) {

    struct ConfigConstellationsReq : public LocMsg {
        ConfigConstellationsReq(LocationIntegrationApiImpl* apiImpl,
                                const GnssSvTypeConfig& constellationEnablementConfig,
                                const GnssSvIdConfig& blacklistSvConfig) :
                mApiImpl(apiImpl),
                mConstellationEnablementConfig(constellationEnablementConfig),
                mBlacklistSvConfig(blacklistSvConfig) {}
        virtual ~ConfigConstellationsReq() {}
        void proc() const {
            string pbStr;
            LocConfigSvConstellationReqMsg msg(mApiImpl->mSocketName,
                                               mConstellationEnablementConfig,
                                               mBlacklistSvConfig,
                                               &mApiImpl->mPbufMsgConv);
            if (msg.serializeToProtobuf(pbStr)) {
                mApiImpl->sendConfigMsgToHalDaemon(CONFIG_CONSTELLATIONS,
                                            reinterpret_cast<uint8_t*>((uint8_t *)pbStr.c_str()),
                                            pbStr.size());
               // cache the last config to be used when hal daemon restarts
                mApiImpl->mSvConfigInfo.isValid = true;
                mApiImpl->mSvConfigInfo.constellationEnablementConfig =
                        mConstellationEnablementConfig;
                mApiImpl->mSvConfigInfo.blacklistSvConfig = mBlacklistSvConfig;
            } else {
                LOC_LOGe("LocConfigSvConstellationReqMsg serializeToProtobuf failed");
            }
        }

        LocationIntegrationApiImpl* mApiImpl;
        GnssSvTypeConfig mConstellationEnablementConfig;
        GnssSvIdConfig mBlacklistSvConfig;
    };
    mMsgTask.sendMsg(new (nothrow) ConfigConstellationsReq(
            this, constellationEnablementConfig, blacklistSvConfig));
    return 0;
}

uint32_t LocationIntegrationApiImpl::configConstellationSecondaryBand(
        const GnssSvTypeConfig& secondaryBandConfig) {

    struct ConfigConstellationSecondaryBandReq : public LocMsg {
        ConfigConstellationSecondaryBandReq(LocationIntegrationApiImpl* apiImpl,
                                            const GnssSvTypeConfig& secondaryBandConfig) :
                mApiImpl(apiImpl),
                mSecondaryBandConfig(secondaryBandConfig) {}
        virtual ~ConfigConstellationSecondaryBandReq() {}
        void proc() const {
            string pbStr;
            LocConfigConstellationSecondaryBandReqMsg msg(
                    mApiImpl->mSocketName,
                    mSecondaryBandConfig,
                    &mApiImpl->mPbufMsgConv);
            if (msg.serializeToProtobuf(pbStr)) {
                mApiImpl->sendConfigMsgToHalDaemon(CONFIG_CONSTELLATION_SECONDARY_BAND,
                                            reinterpret_cast<uint8_t*>((uint8_t *)pbStr.c_str()),
                                            pbStr.size());
               // cache the last config to be used when hal daemon restarts
                mApiImpl->mSvConfigInfo.isValid = true;
                mApiImpl->mSvConfigInfo.secondaryBandConfig = mSecondaryBandConfig;
            } else {
                LOC_LOGe("LocConfigConstellationSecondaryBandReqMsg serializeToProtobuf failed");
            }
        }

        LocationIntegrationApiImpl* mApiImpl;
        GnssSvTypeConfig mSecondaryBandConfig;
    };
    mMsgTask.sendMsg(new (nothrow) ConfigConstellationSecondaryBandReq(this, secondaryBandConfig));
    return 0;
}

uint32_t LocationIntegrationApiImpl::getConstellationSecondaryBandConfig() {

    struct GetConstellationSecondaryBandConfigReq : public LocMsg {
        GetConstellationSecondaryBandConfigReq(LocationIntegrationApiImpl* apiImpl) :
                mApiImpl(apiImpl) {}
        virtual ~GetConstellationSecondaryBandConfigReq() {}
        void proc() const {
            string pbStr;
            LocConfigGetConstellationSecondaryBandConfigReqMsg msg(mApiImpl->mSocketName,
                    &mApiImpl->mPbufMsgConv);
            if (msg.serializeToProtobuf(pbStr)) {
                mApiImpl->sendConfigMsgToHalDaemon(GET_CONSTELLATION_SECONDARY_BAND_CONFIG,
                                   reinterpret_cast<uint8_t*>((uint8_t *)pbStr.c_str()),
                                   pbStr.size());
            } else {
                LOC_LOGe("LocCgGetCnstlSecondaryBandConfigReqMsg serializeToProtobuf failed");
            }
        }
        LocationIntegrationApiImpl* mApiImpl;
    };

    if (mIntegrationCbs.getConstellationSecondaryBandConfigCb == nullptr) {
        LOC_LOGd("no callback in constructor to receive secondary band config");
        // return 1 to signal error
        return 1;
    }
    mMsgTask.sendMsg(new (nothrow) GetConstellationSecondaryBandConfigReq(this));

    return 0;
}

uint32_t LocationIntegrationApiImpl::configConstrainedTimeUncertainty(
        bool enable, float tuncThreshold, uint32_t energyBudget) {

    struct ConfigConstrainedTuncReq : public LocMsg {
        ConfigConstrainedTuncReq(LocationIntegrationApiImpl* apiImpl,
                                 bool enable,
                                 float tuncThreshold,
                                 uint32_t energyBudget) :
                mApiImpl(apiImpl),
                mEnable(enable),
                mTuncThreshold(tuncThreshold),
                mEnergyBudget(energyBudget) {}
        virtual ~ConfigConstrainedTuncReq() {}
        void proc() const {
            string pbStr;
            LocConfigConstrainedTuncReqMsg msg(mApiImpl->mSocketName,
                                               mEnable, mTuncThreshold, mEnergyBudget,
                                               &mApiImpl->mPbufMsgConv);
            if (msg.serializeToProtobuf(pbStr)) {
                mApiImpl->sendConfigMsgToHalDaemon(CONFIG_CONSTRAINED_TIME_UNCERTAINTY,
                                            reinterpret_cast<uint8_t*>((uint8_t *)pbStr.c_str()),
                                            pbStr.size());
               // cache the last config to be used when hal daemon restarts
                mApiImpl->mTuncConfigInfo.isValid = true;
                mApiImpl->mTuncConfigInfo.enable = mEnable;
                mApiImpl->mTuncConfigInfo.tuncThresholdMs = mTuncThreshold;
                mApiImpl->mTuncConfigInfo.energyBudget = mEnergyBudget;
            } else {
                LOC_LOGe("LocConfigConstrainedTuncReqMsg serializeToProtobuf failed");
            }
        }
        LocationIntegrationApiImpl* mApiImpl;
        bool mEnable;
        float mTuncThreshold;
        uint32_t mEnergyBudget;
    };
    mMsgTask.sendMsg(new (nothrow)ConfigConstrainedTuncReq(
            this, enable, tuncThreshold, energyBudget));

    return 0;
}

uint32_t LocationIntegrationApiImpl::configPositionAssistedClockEstimator(bool enable) {

    struct ConfigPositionAssistedClockEstimatorReq : public LocMsg {
        ConfigPositionAssistedClockEstimatorReq(LocationIntegrationApiImpl* apiImpl,
                                                bool enable) :
                mApiImpl(apiImpl),
                mEnable(enable) {}
        virtual ~ConfigPositionAssistedClockEstimatorReq() {}
        void proc() const {
            string pbStr;
            LocConfigPositionAssistedClockEstimatorReqMsg msg(mApiImpl->mSocketName,
                                                              mEnable,
                                                              &mApiImpl->mPbufMsgConv);
            if (msg.serializeToProtobuf(pbStr)) {
                mApiImpl->sendConfigMsgToHalDaemon(CONFIG_POSITION_ASSISTED_CLOCK_ESTIMATOR,
                                        reinterpret_cast<uint8_t*>((uint8_t *)pbStr.c_str()),
                                        pbStr.size());
               // cache the last config to be used when hal daemon restarts
                mApiImpl->mPaceConfigInfo.isValid = true;
                mApiImpl->mPaceConfigInfo.enable = mEnable;
            } else {
                LOC_LOGe("LocConfigPositionAssistedClockEstimatorReqMsg serializeToProtobuf fail");
            }
        }
        LocationIntegrationApiImpl* mApiImpl;
        bool mEnable;
    };
    mMsgTask.sendMsg(new (nothrow)
            ConfigPositionAssistedClockEstimatorReq(this, enable));

    return 0;
}

uint32_t LocationIntegrationApiImpl::gnssDeleteAidingData(
        GnssAidingData& aidingData) {
    struct DeleteAidingDataReq : public LocMsg {
        DeleteAidingDataReq(LocationIntegrationApiImpl* apiImpl,
                            GnssAidingData& aidingData) :
                mApiImpl(apiImpl),
                mAidingData(aidingData) {}
        virtual ~DeleteAidingDataReq() {}
        void proc() const {
            string pbStr;
            LocConfigAidingDataDeletionReqMsg msg(mApiImpl->mSocketName,
                                                  const_cast<GnssAidingData&>(mAidingData),
                                                  &mApiImpl->mPbufMsgConv);
            if (msg.serializeToProtobuf(pbStr)) {
                mApiImpl->sendConfigMsgToHalDaemon(CONFIG_AIDING_DATA_DELETION,
                                            reinterpret_cast<uint8_t*>((uint8_t *)pbStr.c_str()),
                                            pbStr.size());
            } else {
                LOC_LOGe("LocConfigAidingDataDeletionReqMsg serializeToProtobuf failed");
            }
        }
        LocationIntegrationApiImpl* mApiImpl;
        GnssAidingData mAidingData;
    };
    mMsgTask.sendMsg(new (nothrow) DeleteAidingDataReq(this, aidingData));

    return 0;
}

uint32_t LocationIntegrationApiImpl::configLeverArm(
        const LeverArmConfigInfo& configInfo) {
    struct ConfigLeverArmReq : public LocMsg {
        ConfigLeverArmReq(LocationIntegrationApiImpl* apiImpl,
                          const LeverArmConfigInfo& configInfo) :
                mApiImpl(apiImpl),
                mConfigInfo(configInfo) {}
        virtual ~ConfigLeverArmReq() {}
        void proc() const {
            string pbStr;
            LocConfigLeverArmReqMsg msg(mApiImpl->mSocketName, mConfigInfo,
                    &mApiImpl->mPbufMsgConv);
            if (msg.serializeToProtobuf(pbStr)) {
                mApiImpl->sendConfigMsgToHalDaemon(CONFIG_LEVER_ARM,
                                            reinterpret_cast<uint8_t*>((uint8_t *)pbStr.c_str()),
                                            pbStr.size());
                // cache the last config to be used when hal daemon restarts
                mApiImpl->mLeverArmConfigInfo = mConfigInfo;
            } else {
                LOC_LOGe("LocConfigLeverArmReqMsg serializeToProtobuf failed");
            }
        }
        LocationIntegrationApiImpl* mApiImpl;
        LeverArmConfigInfo mConfigInfo;
    };

    if (configInfo.leverArmValidMask) {
        mMsgTask.sendMsg(new (nothrow) ConfigLeverArmReq(this, configInfo));
    }

    return 0;
}

uint32_t LocationIntegrationApiImpl::configRobustLocation(
        bool enable, bool enableForE911) {
    struct ConfigRobustLocationReq : public LocMsg {
        ConfigRobustLocationReq(LocationIntegrationApiImpl* apiImpl,
                                bool enable,
                                bool enableForE911) :
                mApiImpl(apiImpl),
                mEnable(enable),
                mEnableForE911(enableForE911){}
        virtual ~ConfigRobustLocationReq() {}
        void proc() const {
            string pbStr;
            LocConfigRobustLocationReqMsg msg(mApiImpl->mSocketName, mEnable,
                    mEnableForE911, &mApiImpl->mPbufMsgConv);
            if (msg.serializeToProtobuf(pbStr)) {
                mApiImpl->sendConfigMsgToHalDaemon(CONFIG_ROBUST_LOCATION,
                                            reinterpret_cast<uint8_t*>((uint8_t *)pbStr.c_str()),
                                            pbStr.size());
                mApiImpl->mRobustLocationConfigInfo.isValid = true;
                mApiImpl->mRobustLocationConfigInfo.enable = mEnable;
                mApiImpl->mRobustLocationConfigInfo.enableForE911 = mEnableForE911;
            } else {
                LOC_LOGe("LocConfigRobustLocationReqMsg serializeToProtobuf failed");
            }
        }
        LocationIntegrationApiImpl* mApiImpl;
        bool mEnable;
        bool mEnableForE911;
    };

    mMsgTask.sendMsg(new (nothrow)
                      ConfigRobustLocationReq(this, enable, enableForE911));

    return 0;
}

uint32_t LocationIntegrationApiImpl::getRobustLocationConfig() {

    struct GetRobustLocationConfigReq : public LocMsg {
        GetRobustLocationConfigReq(LocationIntegrationApiImpl* apiImpl) :
                mApiImpl(apiImpl) {}
        virtual ~GetRobustLocationConfigReq() {}
        void proc() const {
            string pbStr;
            LocConfigGetRobustLocationConfigReqMsg msg(mApiImpl->mSocketName,
                    &mApiImpl->mPbufMsgConv);
            if (msg.serializeToProtobuf(pbStr)) {
                mApiImpl->sendConfigMsgToHalDaemon(GET_ROBUST_LOCATION_CONFIG,
                                            reinterpret_cast<uint8_t*>((uint8_t *)pbStr.c_str()),
                                            pbStr.size());
            } else {
                LOC_LOGe("LocConfigGetRobustLocationConfigReqMsg serializeToProtobuf failed");
            }
        }
        LocationIntegrationApiImpl* mApiImpl;
    };

    if (mIntegrationCbs.getRobustLocationConfigCb == nullptr) {
        LOC_LOGe("no callback passed in constructor to receive robust location config");
        // return 1 to signal error
        return 1;
    }
    mMsgTask.sendMsg(new (nothrow) GetRobustLocationConfigReq(this));

    return 0;
}

uint32_t LocationIntegrationApiImpl::configMinGpsWeek(uint16_t minGpsWeek) {
    struct ConfigMinGpsWeekReq : public LocMsg {
        ConfigMinGpsWeekReq(LocationIntegrationApiImpl* apiImpl,
                            uint16_t minGpsWeek) :
                mApiImpl(apiImpl),
                mMinGpsWeek(minGpsWeek) {}
        virtual ~ConfigMinGpsWeekReq() {}
        void proc() const {
            string pbStr;
            LocConfigMinGpsWeekReqMsg msg(mApiImpl->mSocketName,
                    mMinGpsWeek, &mApiImpl->mPbufMsgConv);
            if (msg.serializeToProtobuf(pbStr)) {
                mApiImpl->sendConfigMsgToHalDaemon(CONFIG_MIN_GPS_WEEK,
                                            reinterpret_cast<uint8_t*>((uint8_t *)pbStr.c_str()),
                                            pbStr.size());
            } else {
                LOC_LOGe("LocConfigMinGpsWeekReqMsg serializeToProtobuf failed");
            }
        }
        LocationIntegrationApiImpl* mApiImpl;
        uint16_t mMinGpsWeek;
    };

    mMsgTask.sendMsg(new (nothrow) ConfigMinGpsWeekReq(this, minGpsWeek));
    return 0;
}

uint32_t LocationIntegrationApiImpl::getMinGpsWeek() {

    struct GetMinGpsWeekReq : public LocMsg {
        GetMinGpsWeekReq(LocationIntegrationApiImpl* apiImpl) :
                mApiImpl(apiImpl) {}
        virtual ~GetMinGpsWeekReq() {}
        void proc() const {
            string pbStr;
            LocConfigGetMinGpsWeekReqMsg msg(mApiImpl->mSocketName, &mApiImpl->mPbufMsgConv);
            if (msg.serializeToProtobuf(pbStr)) {
                mApiImpl->sendConfigMsgToHalDaemon(GET_MIN_GPS_WEEK,
                                            reinterpret_cast<uint8_t*>((uint8_t *)pbStr.c_str()),
                                            pbStr.size());
            } else {
                LOC_LOGe("LocConfigGetMinGpsWeekReqMsg serializeToProtobuf failed");
            }
        }
        LocationIntegrationApiImpl* mApiImpl;
    };

    if (mIntegrationCbs.getMinGpsWeekCb == nullptr) {
        LOC_LOGe("no callback passed in constructor to receive gps week info");
        // return 1 to signal error
        return 1;
    }
    mMsgTask.sendMsg(new (nothrow) GetMinGpsWeekReq(this));

    return 0;
}

uint32_t LocationIntegrationApiImpl::configDeadReckoningEngineParams(
        const ::DeadReckoningEngineConfig& dreConfig) {
    struct ConfigDrEngineParamsReq : public LocMsg {
        ConfigDrEngineParamsReq(LocationIntegrationApiImpl* apiImpl,
                                ::DeadReckoningEngineConfig dreConfig) :
                mApiImpl(apiImpl),
                mDreConfig(dreConfig){}
        virtual ~ConfigDrEngineParamsReq() {}
        void proc() const {
            string pbStr;
            mApiImpl->mDreConfigInfo.isValid = true;
            mApiImpl->mDreConfigInfo.dreConfig = mDreConfig;
            LocConfigDrEngineParamsReqMsg msg(mApiImpl->mSocketName,
                                              mApiImpl->mDreConfigInfo.dreConfig,
                                              &mApiImpl->mPbufMsgConv);
            if (msg.serializeToProtobuf(pbStr)) {
                mApiImpl->sendConfigMsgToHalDaemon(CONFIG_DEAD_RECKONING_ENGINE,
                                            reinterpret_cast<uint8_t*>((uint8_t *)pbStr.c_str()),
                                            pbStr.size());
            } else {
                LOC_LOGe("LocConfigDrEngineParamsReqMsg serializeToProtobuf failed");
            }
        }
        LocationIntegrationApiImpl* mApiImpl;
        ::DeadReckoningEngineConfig mDreConfig;
    };

    mMsgTask.sendMsg(new (nothrow) ConfigDrEngineParamsReq(this, dreConfig));

    return 0;
}

uint32_t LocationIntegrationApiImpl::configMinSvElevation(uint8_t minSvElevation) {

        struct ConfigMinSvElevationReq : public LocMsg {
        ConfigMinSvElevationReq(LocationIntegrationApiImpl* apiImpl,
                                uint8_t minSvElevation) :
                mApiImpl(apiImpl), mMinSvElevation(minSvElevation){}
        virtual ~ConfigMinSvElevationReq() {}
        void proc() const {
            string pbStr;
            LocConfigMinSvElevationReqMsg msg(mApiImpl->mSocketName,
                    mMinSvElevation, &mApiImpl->mPbufMsgConv);
            if (msg.serializeToProtobuf(pbStr)) {
                mApiImpl->sendConfigMsgToHalDaemon(CONFIG_MIN_SV_ELEVATION,
                                            reinterpret_cast<uint8_t*>((uint8_t *)pbStr.c_str()),
                                            pbStr.size());
            } else {
                LOC_LOGe("LocConfigMinSvElevationReqMsg serializeToProtobuf failed");
            }
        }
        LocationIntegrationApiImpl* mApiImpl;
        uint8_t mMinSvElevation;
    };

    mMsgTask.sendMsg(new (nothrow) ConfigMinSvElevationReq(this, minSvElevation));
    return 0;
}

uint32_t LocationIntegrationApiImpl::getMinSvElevation() {

    struct GetMinSvElevationReq : public LocMsg {
        GetMinSvElevationReq(LocationIntegrationApiImpl* apiImpl) :
                mApiImpl(apiImpl) {}
        virtual ~GetMinSvElevationReq() {}
        void proc() const {
            string pbStr;
            LocConfigGetMinSvElevationReqMsg msg(mApiImpl->mSocketName, &mApiImpl->mPbufMsgConv);
            if (msg.serializeToProtobuf(pbStr)) {
                mApiImpl->sendConfigMsgToHalDaemon(GET_MIN_SV_ELEVATION,
                                            reinterpret_cast<uint8_t*>((uint8_t *)pbStr.c_str()),
                                            pbStr.size());
            } else {
                LOC_LOGe("LocConfigGetMinSvElevationReqMsg serializeToProtobuf failed");
            }
        }
        LocationIntegrationApiImpl* mApiImpl;
    };

    if (mIntegrationCbs.getMinSvElevationCb == nullptr) {
        LOC_LOGe("no callback passed in constructor to receive min sv elevation info");
        // return 1 to signal error
        return 1;
    }
    mMsgTask.sendMsg(new (nothrow) GetMinSvElevationReq(this));

    return 0;
}

uint32_t LocationIntegrationApiImpl::configEngineRunState(
        PositioningEngineMask engType, LocEngineRunState engState) {

    struct ConfigEngineRunStateReq : public LocMsg {
        ConfigEngineRunStateReq(LocationIntegrationApiImpl* apiImpl,
                                PositioningEngineMask engType,
                                LocEngineRunState engState) :
                mApiImpl(apiImpl), mEngType(engType), mEngState(engState) {}
        virtual ~ConfigEngineRunStateReq() {}
        void proc() const {
            if (mApiImpl->mEngRunStateConfigMap.find(mEngType) ==
                        std::end(mApiImpl->mEngRunStateConfigMap)) {
                mApiImpl->mEngRunStateConfigMap.emplace(mEngType, mEngState);
            } else {
                // change the state for the eng
                mApiImpl->mEngRunStateConfigMap[mEngType] = mEngState;
            }

            string pbStr;
            LocConfigEngineRunStateReqMsg msg(mApiImpl->mSocketName,
                    mEngType, mEngState, &mApiImpl->mPbufMsgConv);
            if (msg.serializeToProtobuf(pbStr)) {
                mApiImpl->sendConfigMsgToHalDaemon(CONFIG_ENGINE_RUN_STATE,
                                            reinterpret_cast<uint8_t*>((uint8_t *)pbStr.c_str()),
                                            pbStr.size());
            } else {
                LOC_LOGe("LocConfigEngineRunStateReqMsg serializeToProtobuf failed");
            }
        }

        LocationIntegrationApiImpl* mApiImpl;
        PositioningEngineMask mEngType;
        LocEngineRunState mEngState;
    };

    mMsgTask.sendMsg(new (nothrow) ConfigEngineRunStateReq(this, engType, engState));
    return 0;
}

uint32_t LocationIntegrationApiImpl::setUserConsentForTerrestrialPositioning(bool userConsent) {
    struct SetUserConsentReq : public LocMsg {
        SetUserConsentReq(LocationIntegrationApiImpl* apiImpl,
                          bool userConsent) :
                mApiImpl(apiImpl), mUserConsent(userConsent) {}
        virtual ~SetUserConsentReq() {}
        void proc() const {
            string pbStr;
            mApiImpl->mGtpUserConsentConfigInfo.isValid = true;
            mApiImpl->mGtpUserConsentConfigInfo.userConsent = mUserConsent;
            LocConfigUserConsentTerrestrialPositioningReqMsg msg(
                    mApiImpl->mSocketName, mUserConsent, &mApiImpl->mPbufMsgConv);
            if (msg.serializeToProtobuf(pbStr)) {
                mApiImpl->sendConfigMsgToHalDaemon(CONFIG_USER_CONSENT_TERRESTRIAL_POSITIONING,
                        reinterpret_cast<uint8_t*>((uint8_t *)pbStr.c_str()), pbStr.size());
            } else {
                LOC_LOGe("serializeToProtobuf failed");
            }
        }

        LocationIntegrationApiImpl* mApiImpl;
        bool mUserConsent;
    };

    mMsgTask.sendMsg(new (nothrow) SetUserConsentReq(this, userConsent));
    return 0;
}

void LocationIntegrationApiImpl::sendConfigMsgToHalDaemon(
        LocConfigTypeEnum configType, uint8_t* pMsg,
        size_t msgSize, bool invokeResponseCb) {

    bool messageSentToHal = false;
    if (mHalRegistered) {
        bool rc = sendMessage(pMsg, msgSize);
        LOC_LOGd(">>> sendConfigMsgToHalDaemon, msg type=%d, rc=%d", configType, rc);
        if (true == rc) {
            messageSentToHal = true;
        } else {
            LOC_LOGe(">>> sendConfigMsgToHalDaemon failed for msg type=%d", configType);
        }
    }

    if (invokeResponseCb && mIntegrationCbs.configCb) {
        if (true == messageSentToHal) {
            addConfigReq(configType);
        } else {
            mIntegrationCbs.configCb(configType, LOC_INT_RESPONSE_FAILURE);
        }
    }
}

void LocationIntegrationApiImpl::sendClientRegMsgToHalDaemon(){
    string pbStr;
    LocAPIClientRegisterReqMsg msg(mSocketName, LOCATION_INTEGRATION_API, &mPbufMsgConv);
    if (msg.serializeToProtobuf(pbStr)) {
        bool rc = sendMessage(reinterpret_cast<uint8_t *>((uint8_t *)pbStr.c_str()),
                pbStr.size());
        LOC_LOGd(">>> onListenerReady::ClientRegisterReqMsg rc=%d", rc);
        if (true == rc) {
            mHalRegistered = true;
        }
    } else {
        LOC_LOGe("LocAPIClientRegisterReqMsg serializeToProtobuf failed");
    }
}

void LocationIntegrationApiImpl::processHalReadyMsg() {
    // when location hal daemon crashes and restarts,
    // we flush out all pending requests and notify each client
    // that the request has failed.
    flushConfigReqs();

    // register with hal daemon
    sendClientRegMsgToHalDaemon();

    // send cached configuration to hal daemon
    if (mSvConfigInfo.isValid) {
        string pbStrLocConfigSvConst;
        LocConfigSvConstellationReqMsg msg(mSocketName,
                                           mSvConfigInfo.constellationEnablementConfig,
                                           mSvConfigInfo.blacklistSvConfig,
                                           &mPbufMsgConv);
        if (msg.serializeToProtobuf(pbStrLocConfigSvConst)) {
            sendConfigMsgToHalDaemon(CONFIG_CONSTELLATIONS,
                    reinterpret_cast<uint8_t*>((uint8_t *)pbStrLocConfigSvConst.c_str()),
                    pbStrLocConfigSvConst.size());
        } else {
            LOC_LOGe("LocConfigSvConstellationReqMsg serializeToProtobuf failed");
        }
    }

    if (mSvConfigInfo.secondaryBandConfig.size != 0) {
        string pbStrLocConfigConstSecndBandReq;
        LocConfigConstellationSecondaryBandReqMsg msg(
                    mSocketName, mSvConfigInfo.secondaryBandConfig, &mPbufMsgConv);
        if (msg.serializeToProtobuf(pbStrLocConfigConstSecndBandReq)) {
            sendConfigMsgToHalDaemon(CONFIG_CONSTELLATION_SECONDARY_BAND,
                    reinterpret_cast<uint8_t*>((uint8_t *)pbStrLocConfigConstSecndBandReq.c_str()),
                    pbStrLocConfigConstSecndBandReq.size());
        } else {
            LOC_LOGe("LocConfigConstellationSecondaryBandReqMsg serializeToProtobuf failed");
        }
    }

    if (mTuncConfigInfo.isValid) {
        string pbStrLocConfigConstTunc;
        LocConfigConstrainedTuncReqMsg msg(mSocketName,
                                           mTuncConfigInfo.enable,
                                           mTuncConfigInfo.tuncThresholdMs,
                                           mTuncConfigInfo.energyBudget,
                                           &mPbufMsgConv);
        if (msg.serializeToProtobuf(pbStrLocConfigConstTunc)) {
            sendConfigMsgToHalDaemon(CONFIG_CONSTRAINED_TIME_UNCERTAINTY,
                    reinterpret_cast<uint8_t*>((uint8_t *)pbStrLocConfigConstTunc.c_str()),
                    pbStrLocConfigConstTunc.size());
        } else {
            LOC_LOGe("LocConfigConstrainedTuncReqMsg serializeToProtobuf failed");
        }
    }
    if (mPaceConfigInfo.isValid) {
        string pbStrLocConfigPosAsstdClockEst;
        LocConfigPositionAssistedClockEstimatorReqMsg msg(mSocketName,
                                                          mPaceConfigInfo.enable,
                                                          &mPbufMsgConv);
        if (msg.serializeToProtobuf(pbStrLocConfigPosAsstdClockEst)) {
            sendConfigMsgToHalDaemon(CONFIG_POSITION_ASSISTED_CLOCK_ESTIMATOR,
                    reinterpret_cast<uint8_t*>((uint8_t *)pbStrLocConfigPosAsstdClockEst.c_str()),
                    pbStrLocConfigPosAsstdClockEst.size());
        } else {
            LOC_LOGe("LocConfigPositionAssistedClockEstimatorReqMsg serializeToProtobuf failed");
        }
    }
    if (mLeverArmConfigInfo.leverArmValidMask) {
        string pbStrLocConfigLeverArm;
        LocConfigLeverArmReqMsg msg(mSocketName, mLeverArmConfigInfo, &mPbufMsgConv);
        if (msg.serializeToProtobuf(pbStrLocConfigLeverArm)) {
            sendConfigMsgToHalDaemon(CONFIG_LEVER_ARM,
                    reinterpret_cast<uint8_t*>((uint8_t *)pbStrLocConfigLeverArm.c_str()),
                    pbStrLocConfigLeverArm.size());
        } else {
            LOC_LOGe("LocConfigLeverArmReqMsg serializeToProtobuf failed");
        }
    }
    if (mRobustLocationConfigInfo.isValid) {
        string pbStrLocConfigRobustLoc;
        LocConfigRobustLocationReqMsg msg(mSocketName,
                                          mRobustLocationConfigInfo.enable,
                                          mRobustLocationConfigInfo.enableForE911,
                                          &mPbufMsgConv);
        if (msg.serializeToProtobuf(pbStrLocConfigRobustLoc)) {
            sendConfigMsgToHalDaemon(CONFIG_ROBUST_LOCATION,
                    reinterpret_cast<uint8_t*>((uint8_t *)pbStrLocConfigRobustLoc.c_str()),
                    pbStrLocConfigRobustLoc.size());
        } else {
            LOC_LOGe("LocConfigRobustLocationReqMsg serializeToProtobuf failed");
        }
    }
    // Do not reconfigure min gps week, as min gps week setting
    // can be overwritten by modem over  time

    if (mDreConfigInfo.isValid) {
        string pbStrLocCfgDrEngParam;
        LocConfigDrEngineParamsReqMsg msg(mSocketName, mDreConfigInfo.dreConfig, &mPbufMsgConv);
        if (msg.serializeToProtobuf(pbStrLocCfgDrEngParam)) {
            sendConfigMsgToHalDaemon(CONFIG_DEAD_RECKONING_ENGINE,
                             reinterpret_cast<uint8_t*>((uint8_t *)pbStrLocCfgDrEngParam.c_str()),
                             pbStrLocCfgDrEngParam.size());
        } else {
            LOC_LOGe("LocConfigDrEngineParamsReqMsg serializeToProtobuf failed");
        }
    }

    if (mGtpUserConsentConfigInfo.isValid) {
        string pbStr;
        LocConfigUserConsentTerrestrialPositioningReqMsg msg(
                    mSocketName, mGtpUserConsentConfigInfo.userConsent, &mPbufMsgConv);
        if (msg.serializeToProtobuf(pbStr)) {
            sendConfigMsgToHalDaemon(CONFIG_USER_CONSENT_TERRESTRIAL_POSITIONING,
                        reinterpret_cast<uint8_t*>((uint8_t *)pbStr.c_str()), pbStr.size());
        } else {
            LOC_LOGe("serializeToProtobuf failed");
        }
    }

    // send down engine state config request
    for (auto it = mEngRunStateConfigMap.begin(); it != mEngRunStateConfigMap.end(); ++it) {
        string pbStrLocCfgEngineRunState;
        LocConfigEngineRunStateReqMsg msg(mSocketName, it->first, it->second, &mPbufMsgConv);
        if (msg.serializeToProtobuf(pbStrLocCfgEngineRunState)) {
            sendConfigMsgToHalDaemon(
                    CONFIG_ENGINE_RUN_STATE,
                    reinterpret_cast<uint8_t*>((uint8_t *)pbStrLocCfgEngineRunState.c_str()),
                    pbStrLocCfgEngineRunState.size());
        }
    }
}

void LocationIntegrationApiImpl::addConfigReq(LocConfigTypeEnum configType) {
    LOC_LOGv("add configType %d", configType);

    auto configReqData = mConfigReqCntMap.find(configType);
    if (configReqData == std::end(mConfigReqCntMap)) {
        mConfigReqCntMap.emplace(configType, 1);
    } else{
        int newCnt = configReqData->second+1;
        mConfigReqCntMap.erase(configReqData);
        mConfigReqCntMap.emplace(configType, newCnt);
    }
}

void LocationIntegrationApiImpl::processConfigRespCb(const LocAPIGenericRespMsg* pRespMsg) {
    LocConfigTypeEnum configType = getLocConfigTypeFromMsgId(pRespMsg->msgId);
    LocIntegrationResponse intResponse = getLocIntegrationResponse(pRespMsg->err);
    LOC_LOGd("<<< response message id: %d, msg err: %d, "
             "config type: %d, int response %d",
             pRespMsg->msgId, pRespMsg->err, configType, intResponse);

    if (mIntegrationCbs.configCb) {
        auto configReqData = mConfigReqCntMap.find(configType);
        if (configReqData != std::end(mConfigReqCntMap)) {
            int reqCnt = configReqData->second;
            if (reqCnt > 0) {
                mIntegrationCbs.configCb(configType, intResponse);
            }
            mConfigReqCntMap.erase(configReqData);
            // there are still some request pending for this config type
            // need to store it in the map
            if (--reqCnt > 0) {
                mConfigReqCntMap.emplace(configType, reqCnt);
            }
        }
    }
}

// flush all the pending config request if location hal daemon has crashed
// and restarted
void LocationIntegrationApiImpl::flushConfigReqs() {
    for (auto itr=mConfigReqCntMap.begin(); itr!=mConfigReqCntMap.end(); ++itr) {
         int reqCnt = itr->second;
         while (reqCnt-- > 0) {
             mIntegrationCbs.configCb(itr->first, LOC_INT_RESPONSE_FAILURE);
         }
    }
    mConfigReqCntMap.clear();
}

void LocationIntegrationApiImpl::processGetRobustLocationConfigRespCb(
        const LocConfigGetRobustLocationConfigRespMsg* pRespMsg) {

    LOC_LOGd("<<< response message id: %d, mask 0x%x, enabled: %d, enabledFor911: %d",
             pRespMsg->msgId,
             pRespMsg->mRobustLoationConfig.validMask,
             pRespMsg->mRobustLoationConfig.enabled,
             pRespMsg->mRobustLoationConfig.enabledForE911);

    if (mIntegrationCbs.getRobustLocationConfigCb) {
        // conversion between the enums
        RobustLocationConfig robustConfig = {};
        uint32_t validMask = 0;;
        if (pRespMsg->mRobustLoationConfig.validMask &
                GNSS_CONFIG_ROBUST_LOCATION_ENABLED_VALID_BIT) {
            validMask |= ROBUST_LOCATION_CONFIG_VALID_ENABLED;
            robustConfig.enabled = pRespMsg->mRobustLoationConfig.enabled;
        }
        if (pRespMsg->mRobustLoationConfig.validMask &
                GNSS_CONFIG_ROBUST_LOCATION_ENABLED_FOR_E911_VALID_BIT) {
            validMask |= ROBUST_LOCATION_CONFIG_VALID_ENABLED_FOR_E911;
            robustConfig.enabledForE911 = pRespMsg->mRobustLoationConfig.enabledForE911;
        }
        if (pRespMsg->mRobustLoationConfig.validMask &
                GNSS_CONFIG_ROBUST_LOCATION_VERSION_VALID_BIT) {
            validMask |= ROBUST_LOCATION_CONFIG_VALID_VERSION;
            robustConfig.version.major = pRespMsg->mRobustLoationConfig.version.major;
            robustConfig.version.minor = pRespMsg->mRobustLoationConfig.version.minor;
        }

        robustConfig.validMask = (RobustLocationConfigValidMask) validMask;
        mIntegrationCbs.getRobustLocationConfigCb(robustConfig);
    }
}

void LocationIntegrationApiImpl::processGetMinGpsWeekRespCb(
        const LocConfigGetMinGpsWeekRespMsg* pRespMsg) {

    LOC_LOGd("<<< response message id: %d, min gps week: %d",
             pRespMsg->msgId, pRespMsg->mMinGpsWeek);
    if (mIntegrationCbs.getMinGpsWeekCb) {
        mIntegrationCbs.getMinGpsWeekCb(pRespMsg->mMinGpsWeek);
    }
}

void LocationIntegrationApiImpl::processGetMinSvElevationRespCb(
        const LocConfigGetMinSvElevationRespMsg* pRespMsg) {

    LOC_LOGd("<<< response message id: %d, min sv elevation: %d",
             pRespMsg->msgId, pRespMsg->mMinSvElevation);
    if (mIntegrationCbs.getMinSvElevationCb) {
        mIntegrationCbs.getMinSvElevationCb(pRespMsg->mMinSvElevation);
    }
}

// This function returns true of the bit mask for the specified constellation type
// is set.
static bool isBitMaskSetForConstellation(
        GnssConstellationType type, GnssSvTypesMask mask) {

    bool retVal = false;
    if ((type == GNSS_CONSTELLATION_TYPE_GLONASS) && (mask & GNSS_SV_TYPES_MASK_GLO_BIT)) {
        retVal = true;
    } else if ((type == GNSS_CONSTELLATION_TYPE_BEIDOU) && (mask & GNSS_SV_TYPES_MASK_BDS_BIT)) {
        retVal = true;
    } else if ((type == GNSS_CONSTELLATION_TYPE_QZSS) && (mask & GNSS_SV_TYPES_MASK_QZSS_BIT)) {
        retVal = true;
    } else if ((type == GNSS_CONSTELLATION_TYPE_GALILEO) && (mask & GNSS_SV_TYPES_MASK_GAL_BIT)) {
        retVal = true;
    } else if ((type == GNSS_CONSTELLATION_TYPE_NAVIC) && (mask & GNSS_SV_TYPES_MASK_NAVIC_BIT)) {
        retVal = true;
    } else if ((type == GNSS_CONSTELLATION_TYPE_GPS) && (mask & GNSS_SV_TYPES_MASK_GPS_BIT)) {
        retVal = true;
    }

    return retVal;
}

void LocationIntegrationApiImpl::processGetConstellationSecondaryBandConfigRespCb(
    const LocConfigGetConstellationSecondaryBandConfigRespMsg* pRespMsg) {

    if (mIntegrationCbs.getConstellationSecondaryBandConfigCb) {
        ConstellationSet secondaryBandDisablementSet;

        if (pRespMsg->mSecondaryBandConfig.size != 0) {
            uint32_t constellationType = 0;
            GnssSvTypesMask secondaryBandDisabledMask =
                    pRespMsg->mSecondaryBandConfig.blacklistedSvTypesMask;

            LOC_LOGd("secondary band disabled mask: 0x%" PRIx64 "", secondaryBandDisabledMask);
            for (;constellationType <= GNSS_CONSTELLATION_TYPE_MAX; constellationType++) {
                if (isBitMaskSetForConstellation((GnssConstellationType) constellationType,
                                                 secondaryBandDisabledMask)) {
                    secondaryBandDisablementSet.emplace((GnssConstellationType) constellationType);
                }
            }
        }

        mIntegrationCbs.getConstellationSecondaryBandConfigCb(secondaryBandDisablementSet);
    }
}

/******************************************************************************
LocationIntegrationApiImpl - Not implemented ILocationControlAPI functions
******************************************************************************/
uint32_t* LocationIntegrationApiImpl::gnssUpdateConfig(const GnssConfig& config) {
    return nullptr;
}

} // namespace location_integration
