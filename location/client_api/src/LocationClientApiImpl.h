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

#ifndef LOCATIONCLIENTAPIIMPL_H
#define LOCATIONCLIENTAPIIMPL_H

#include <mutex>

#include <loc_pla.h>
#include <LocIpc.h>
#include <LocationDataTypes.h>
#include <ILocationAPI.h>
#include <LocationClientApi.h>
#include <MsgTask.h>
#include <LocationApiMsg.h>
#include <LocationApiPbMsgConv.h>
#include <LCAReportLoggerUtil.h>
#ifdef NO_UNORDERED_SET_OR_MAP
    #include <set>
    #include <map>
#else
    #include <unordered_set>
    #include <unordered_map>
#endif

using namespace std;
using namespace loc_util;

namespace location_client
{

enum ReportCbEnumType {
    REPORT_CB_TYPE_NONE   = 0,
    /** cb for GNSS info, including location, sv info, nmea and
     *  etc */
    REPORT_CB_GNSS_INFO   = 1,
    /** cb for GNSS info, including location, sv info, nmea and
     *  etc and also for location of other engines running in the
     *  system */
    REPORT_CB_ENGINE_INFO = 2,
};

struct ClientCallbacks {
    CapabilitiesCb capabilitycb;
    ResponseCb responsecb;
    CollectiveResponseCb collectivecb;
    LocationCb locationcb;
    BatchingCb batchingcb;
    GeofenceBreachCb gfbreachcb;
    GnssReportCbs gnssreportcbs;
    EngineReportCbs engreportcbs;
};

typedef std::function<void(
    uint32_t response
)> PingTestCb;


class GeofenceImpl: public std::enable_shared_from_this<GeofenceImpl> {
    uint32_t mId;
    Geofence mGeofence;
    static uint32_t nextId();
    static mutex mGfMutex;
public:
    GeofenceImpl(Geofence* geofence) : mId(nextId()), mGeofence(*geofence) {
    }
    void bindGeofence(Geofence* geofence) {
        geofence->mGeofenceImpl = shared_from_this();
    }
    inline uint32_t getClientId() { return mId; }
};

class IpcListener;

class LocationClientApiImpl : public ILocationAPI {
    friend IpcListener;
public:
    LocationClientApiImpl(CapabilitiesCb capabitiescb);
    void destroy();

    // Tracking
    virtual void updateCallbacks(LocationCallbacks&) override;

    virtual uint32_t startTracking(TrackingOptions&) override;

    virtual void stopTracking(uint32_t id) override;

    virtual void updateTrackingOptions(uint32_t id, TrackingOptions&) override;

    //Batching
    virtual uint32_t startBatching(BatchingOptions&) override;

    virtual void stopBatching(uint32_t id) override;

    virtual void updateBatchingOptions(uint32_t id, BatchingOptions&) override;

    virtual void getBatchedLocations(uint32_t id, size_t count) override;

    //Geofence
    virtual uint32_t* addGeofences(size_t count, GeofenceOption*,
                                   GeofenceInfo*) override;

    virtual void removeGeofences(size_t count, uint32_t* ids) override;

    virtual void modifyGeofences(size_t count, uint32_t* ids,
                                 GeofenceOption* options) override;

    virtual void pauseGeofences(size_t count, uint32_t* ids) override;

    virtual void resumeGeofences(size_t count, uint32_t* ids) override;

    //GNSS
    virtual void gnssNiResponse(uint32_t id, GnssNiResponse response) override;

    // other interface
    void updateNetworkAvailability(bool available);
    void updateCallbackFunctions(const ClientCallbacks&,
                                 ReportCbEnumType reportCbType = REPORT_CB_TYPE_NONE);
    void getGnssEnergyConsumed(GnssEnergyConsumedCb gnssEnergyConsumedCallback,
                               ResponseCb responseCallback);
    void updateLocationSystemInfoListener(LocationSystemInfoCb locSystemInfoCallback,
                                          ResponseCb responseCallback);
    void diagLogGnssLocation(const GnssLocation &gnssLocation);
    inline LocationCapabilitiesMask getCapabilities() {return mCapsMask;}

    bool checkGeofenceMap(size_t count, uint32_t* ids);
    void addGeofenceMap(uint32_t id, Geofence& geofence);
    void eraseGeofenceMap(size_t count, uint32_t* ids);

    std::vector<uint32_t>               mLastAddedClientIds;
    std::unordered_map<uint32_t, Geofence> mGeofenceMap; //clientId --> Geofence object
    // convenient methods
    inline bool sendMessage(const uint8_t* data, uint32_t length) const {
        return (mIpcSender != nullptr) && LocIpc::send(*mIpcSender, data, length);
    }

    void pingTest(PingTestCb pingTestCallback);
    inline uint16_t getYearOfHw() {return mYearOfHw;}
    void invokePositionSessionResponseCb(LocationResponse responseCode);

    void getSingleTerrestrialPos(uint32_t timeoutMsec, TerrestrialTechMask techMask,
                                 float horQoS, LocationCb terrestrialPositionCallback,
                                 ResponseCb responseCallback);

private:
    ~LocationClientApiImpl();
    void capabilitesCallback(ELocMsgID  msgId, const void* msgData);
    void updateTrackingOptionsSync(LocationClientApiImpl* pImpl, TrackingOptions& option);

    // protobuf conversion util class
    LocationApiPbMsgConv mPbufMsgConv;

    // internal session parameter
    static uint32_t         mClientIdGenerator;
    static mutex            mMutex;
    uint32_t                mClientId;
    uint32_t                mSessionId;
    uint32_t                mBatchingId;
    bool                    mHalRegistered;
    // For client on different processor, socket name will start with
    // defined constant of SOCKET_TO_EXTERANL_AP_LOCATION_CLIENT_BASE.
    // For client on same processor, socket name will start with
    // SOCKET_LOC_CLIENT_DIR + LOC_CLIENT_NAME_PREFIX.
    char                       mSocketName[MAX_SOCKET_PATHNAME_LENGTH];
    // for client on a different processor, 0 is invalid
    uint32_t                   mInstanceId;
    LocationCallbacksMask      mCallbacksMask;
    LocationOptions            mLocationOptions;
    BatchingOptions            mBatchingOptions;
    LocationCapabilitiesMask   mCapsMask;
    //Year of HW information, 0 is invalid
    uint16_t                   mYearOfHw;
    bool                       mPositionSessionResponseCbPending;

    // callbacks
    CapabilitiesCb          mCapabilitiesCb;
    ResponseCb              mResponseCb;
    CollectiveResponseCb    mCollectiveResCb;
    LocationCb              mLocationCb;
    BatchingCb              mBatchingCb;
    GeofenceBreachCb        mGfBreachCb;
    PingTestCb              mPingTestCb;

    // location callbacks
    GnssLocationCb          mGnssLocationCb;
    EngineLocationsCb       mEngLocationsCb;

    // other GNSS related callback
    GnssSvCb                mGnssSvCb;
    GnssNmeaCb              mGnssNmeaCb;
    GnssDataCb              mGnssDataCb;
    GnssMeasurementsCb      mGnssMeasurementsCb;

    GnssEnergyConsumedCb    mGnssEnergyConsumedInfoCb;
    ResponseCb              mGnssEnergyConsumedResponseCb;

    LocationSystemInfoCb    mLocationSysInfoCb;
    ResponseCb              mLocationSysInfoResponseCb;

    // Terrestrial fix callback
    LocationCb              mSingleTerrestrialPosCb;
    ResponseCb              mSingleTerrestrialPosRespCb;

    MsgTask                    mMsgTask;

    LocIpc                     mIpc;
    shared_ptr<LocIpcSender>   mIpcSender;

    LCAReportLoggerUtil        mLogger;
};

} // namespace location_client

#endif /* LOCATIONCLIENTAPIIMPL_H */
