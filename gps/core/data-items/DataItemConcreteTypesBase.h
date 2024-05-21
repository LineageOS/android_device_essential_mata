/* Copyright (c) 2015-2017, 2020, The Linux Foundation. All rights reserved.
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
 *
 */

#ifndef __DATAITEMCONCRETEBASETYPES__
#define __DATAITEMCONCRETEBASETYPES__

#include <string>
#include <cstring>
#include <sstream>
#include <DataItemId.h>
#include <IDataItemCore.h>
#include <gps_extended_c.h>
#include <inttypes.h>

#define MAC_ADDRESS_LENGTH    6
// MAC address length in bytes
// QMI_LOC_SRN_MAC_ADDR_LENGTH_V02
#define SRN_MAC_ADDRESS_LENGTH    6
#define WIFI_SUPPLICANT_DEFAULT_STATE    0

static constexpr char sDelimit = ':';

namespace loc_core
{
using namespace std;

enum NetworkType {
    TYPE_MOBILE = 0,
    TYPE_WIFI,
    TYPE_ETHERNET,
    TYPE_BLUETOOTH,
    TYPE_MMS,
    TYPE_SUPL,
    TYPE_DUN,
    TYPE_HIPRI,
    TYPE_WIMAX,
    TYPE_PROXY,
    TYPE_UNKNOWN,
};

typedef struct NetworkInfoType
{
    // Unique network handle ID
    uint64_t networkHandle;
    // Type of network for corresponding network handle
    NetworkType networkType;
    NetworkInfoType() : networkHandle(NETWORK_HANDLE_UNKNOWN), networkType(TYPE_UNKNOWN) {}
    NetworkInfoType(string strObj) {
        size_t posDelimit = strObj.find(sDelimit);

        if ( posDelimit != string::npos) {
            int32_t type = TYPE_UNKNOWN;
            string handleStr = strObj.substr(0, posDelimit);
            string typeStr = strObj.substr(posDelimit + 1, strObj.length() - posDelimit - 1);
            stringstream(handleStr) >> networkHandle;
            stringstream(typeStr) >> type;
            networkType = (NetworkType) type;
        } else {
            networkHandle = NETWORK_HANDLE_UNKNOWN;
            networkType = TYPE_UNKNOWN;
        }
    }
    bool operator== (const NetworkInfoType& other) {
        return ((networkHandle == other.networkHandle) && (networkType == other.networkType));
    }
    string toString() {
        string valueStr;
        valueStr.clear ();
        char nethandle [32];
        memset (nethandle, 0, 32);
        snprintf(nethandle, sizeof(nethandle), "%" PRIu64, networkHandle);
        valueStr += string(nethandle);
        valueStr += sDelimit;
        char type [12];
        memset (type, 0, 12);
        snprintf (type, 12, "%u", networkType);
        valueStr += string (type);
        return valueStr;
    }
} NetworkInfoType;


class AirplaneModeDataItemBase : public IDataItemCore  {
public:
    AirplaneModeDataItemBase(bool mode):
        mMode(mode),
        mId(AIRPLANEMODE_DATA_ITEM_ID) {}
    virtual ~AirplaneModeDataItemBase() {}
    inline virtual DataItemId getId() { return mId; }
    virtual void stringify(string& /*valueStr*/) {}
    virtual int32_t copy(IDataItemCore* /*src*/, bool* /*dataItemCopied = NULL*/) {return 1;}
// Data members
    bool mMode;

protected:
    DataItemId mId;
};

class ENHDataItemBase : public IDataItemCore {
public:
    ENHDataItemBase(bool enabled) :
        mEnabled(enabled),
        mId(ENH_DATA_ITEM_ID) {}
    virtual ~ENHDataItemBase() {}
    inline virtual DataItemId getId() { return mId; }
    virtual void stringify(string& /*valueStr*/) {}
    virtual int32_t copy(IDataItemCore* /*src*/, bool* /*dataItemCopied = NULL*/) {return 1;}
// Data members
    bool mEnabled;
protected:
    DataItemId mId;
};

class GPSStateDataItemBase : public IDataItemCore {
public:
    GPSStateDataItemBase(bool enabled) :
        mEnabled(enabled),
        mId(GPSSTATE_DATA_ITEM_ID) {}
    virtual ~GPSStateDataItemBase() {}
    inline virtual DataItemId getId() { return mId; }
    virtual void stringify(string& /*valueStr*/) {}
    virtual int32_t copy(IDataItemCore* /*src*/, bool* /*dataItemCopied = NULL*/) {return 1;}
// Data members
    bool mEnabled;
protected:
    DataItemId mId;
};

class NLPStatusDataItemBase : public IDataItemCore {
public:
    NLPStatusDataItemBase(bool enabled) :
        mEnabled(enabled),
        mId(NLPSTATUS_DATA_ITEM_ID) {}
    virtual ~NLPStatusDataItemBase() {}
    inline virtual DataItemId getId() { return mId; }
    virtual void stringify(string& /*valueStr*/) {}
    virtual int32_t copy(IDataItemCore* /*src*/, bool* /*dataItemCopied = NULL*/) {return 1;}
// Data members
    bool mEnabled;
protected:
    DataItemId mId;
};

class WifiHardwareStateDataItemBase : public IDataItemCore {
public:
    WifiHardwareStateDataItemBase(bool enabled) :
        mEnabled(enabled),
        mId(WIFIHARDWARESTATE_DATA_ITEM_ID) {}
    virtual ~WifiHardwareStateDataItemBase() {}
    inline virtual DataItemId getId() { return mId; }
    virtual void stringify(string& /*valueStr*/) {}
    virtual int32_t copy(IDataItemCore* /*src*/, bool* /*dataItemCopied = NULL*/) {return 1;}
// Data members
    bool mEnabled;
protected:
    DataItemId mId;
};

class ScreenStateDataItemBase : public IDataItemCore {
public:
    ScreenStateDataItemBase(bool state) :
        mState(state),
        mId(SCREEN_STATE_DATA_ITEM_ID) {}
    virtual ~ScreenStateDataItemBase() {}
    inline virtual DataItemId getId() { return mId; }
    virtual void stringify(string& /*valueStr*/) {}
    virtual int32_t copy(IDataItemCore* /*src*/, bool* /*dataItemCopied = NULL*/) {return 1;}
// Data members
    bool mState;
protected:
    DataItemId mId;
};

class PowerConnectStateDataItemBase : public IDataItemCore {
public:
    PowerConnectStateDataItemBase(bool state) :
        mState(state),
        mId(POWER_CONNECTED_STATE_DATA_ITEM_ID) {}
    virtual ~PowerConnectStateDataItemBase() {}
    inline virtual DataItemId getId() { return mId; }
    virtual void stringify(string& /*valueStr*/) {}
    virtual int32_t copy(IDataItemCore* /*src*/, bool* /*dataItemCopied = NULL*/) {return 1;}
// Data members
    bool mState;
protected:
    DataItemId mId;
};

class TimeZoneChangeDataItemBase : public IDataItemCore {
public:
    TimeZoneChangeDataItemBase(int64_t currTimeMillis, int32_t rawOffset, int32_t dstOffset) :
        mCurrTimeMillis (currTimeMillis),
        mRawOffsetTZ (rawOffset),
        mDstOffsetTZ (dstOffset),
        mId(TIMEZONE_CHANGE_DATA_ITEM_ID) {}
    virtual ~TimeZoneChangeDataItemBase() {}
    inline virtual DataItemId getId() { return mId; }
    virtual void stringify(string& /*valueStr*/) {}
    virtual int32_t copy(IDataItemCore* /*src*/, bool* /*dataItemCopied = NULL*/) {return 1;}
// Data members
    int64_t mCurrTimeMillis;
    int32_t mRawOffsetTZ;
    int32_t mDstOffsetTZ;
protected:
    DataItemId mId;
};

class TimeChangeDataItemBase : public IDataItemCore {
public:
    TimeChangeDataItemBase(int64_t currTimeMillis, int32_t rawOffset, int32_t dstOffset) :
        mCurrTimeMillis (currTimeMillis),
        mRawOffsetTZ (rawOffset),
        mDstOffsetTZ (dstOffset),
        mId(TIME_CHANGE_DATA_ITEM_ID) {}
    virtual ~TimeChangeDataItemBase() {}
    inline virtual DataItemId getId() { return mId; }
    virtual void stringify(string& /*valueStr*/) {}
    virtual int32_t copy(IDataItemCore* /*src*/, bool* /*dataItemCopied = NULL*/) {return 1;}
// Data members
    int64_t mCurrTimeMillis;
    int32_t mRawOffsetTZ;
    int32_t mDstOffsetTZ;
protected:
    DataItemId mId;
};

class ShutdownStateDataItemBase : public IDataItemCore {
public:
    ShutdownStateDataItemBase(bool state) :
        mState (state),
        mId(SHUTDOWN_STATE_DATA_ITEM_ID) {}
    virtual ~ShutdownStateDataItemBase() {}
    inline virtual DataItemId getId() { return mId; }
    virtual void stringify(string& /*valueStr*/) {}
    virtual int32_t copy(IDataItemCore* /*src*/, bool* /*dataItemCopied = NULL*/) {return 1;}
// Data members
    bool mState;
protected:
    DataItemId mId;
};

class AssistedGpsDataItemBase : public IDataItemCore {
public:
    AssistedGpsDataItemBase(bool enabled) :
        mEnabled(enabled),
        mId(ASSISTED_GPS_DATA_ITEM_ID) {}
    virtual ~AssistedGpsDataItemBase() {}
    inline virtual DataItemId getId() { return mId; }
    virtual void stringify(string& /*valueStr*/) {}
    virtual int32_t copy(IDataItemCore* /*src*/, bool* /*dataItemCopied = NULL*/) {return 1;}
// Data members
    bool mEnabled;
protected:
    DataItemId mId;
};

class NetworkInfoDataItemBase : public IDataItemCore {
public:
    NetworkInfoDataItemBase(
    NetworkType initialType, int32_t type, string typeName, string subTypeName,
    bool available, bool connected, bool roaming, uint64_t networkHandle, string apn):
            mAllTypes(typeToAllTypes(initialType)),
            mType(type),
            mTypeName(typeName),
            mSubTypeName(subTypeName),
            mAvailable(available),
            mConnected(connected),
            mRoaming(roaming),
            mNetworkHandle(networkHandle),
            mId(NETWORKINFO_DATA_ITEM_ID), mApn(apn) {
                mAllNetworkHandles[0].networkHandle = networkHandle;
                mAllNetworkHandles[0].networkType = initialType;
            }
    virtual ~NetworkInfoDataItemBase() {}
    inline virtual DataItemId getId() { return mId; }
    virtual void stringify(string& /*valueStr*/) {}
    virtual int32_t copy(IDataItemCore* /*src*/, bool* /*dataItemCopied = NULL*/) {return 1;}
    inline virtual NetworkType getType(void) const {
        return (NetworkType)mType;
    }
    inline uint64_t getAllTypes() { return mAllTypes; }
    inline NetworkInfoType* getNetworkHandle() {
        return &mAllNetworkHandles[0];
    }
    // Data members
    uint64_t mAllTypes;
    int32_t mType;
    string mTypeName;
    string mSubTypeName;
    bool mAvailable;
    bool mConnected;
    bool mRoaming;
    NetworkInfoType mAllNetworkHandles[MAX_NETWORK_HANDLES];
    uint64_t mNetworkHandle;
    string mApn;
protected:
    DataItemId mId;
    inline uint64_t typeToAllTypes(NetworkType type) {
        return (type >= TYPE_UNKNOWN || type < TYPE_MOBILE) ?  0 : (1<<type);
    }
};

class ServiceStatusDataItemBase : public IDataItemCore {
public:
    ServiceStatusDataItemBase(int32_t serviceState) :
        mServiceState (serviceState),
        mId(SERVICESTATUS_DATA_ITEM_ID) {}
    virtual ~ServiceStatusDataItemBase() {}
    inline virtual DataItemId getId() { return mId; }
    virtual void stringify(string& /*valueStr*/) {}
    virtual int32_t copy(IDataItemCore* /*src*/, bool* /*dataItemCopied = NULL*/) {return 1;}
// Data members
    int32_t mServiceState;
protected:
    DataItemId mId;
};

class ModelDataItemBase : public IDataItemCore {
public:
    ModelDataItemBase(const string & name) :
        mModel (name),
        mId(MODEL_DATA_ITEM_ID) {}
    virtual ~ModelDataItemBase() {}
    inline virtual DataItemId getId() { return mId; }
    virtual void stringify(string& /*valueStr*/) {}
    virtual int32_t copy(IDataItemCore* /*src*/, bool* /*dataItemCopied = NULL*/) {return 1;}
// Data members
    string mModel;
protected:
    DataItemId mId;
};

class ManufacturerDataItemBase : public IDataItemCore {
public:
    ManufacturerDataItemBase(const string & name) :
        mManufacturer (name),
        mId(MANUFACTURER_DATA_ITEM_ID) {}
    virtual ~ManufacturerDataItemBase() {}
    inline virtual DataItemId getId() { return mId; }
    virtual void stringify(string& /*valueStr*/) {}
    virtual int32_t copy(IDataItemCore* /*src*/, bool* /*dataItemCopied = NULL*/) {return 1;}
// Data members
    string mManufacturer;
protected:
    DataItemId mId;
};

class RilServiceInfoDataItemBase : public IDataItemCore {
public:
    inline RilServiceInfoDataItemBase() :
            mData(nullptr), mId(RILSERVICEINFO_DATA_ITEM_ID) {}
    inline virtual ~RilServiceInfoDataItemBase() { if (nullptr != mData) free(mData); }
    inline virtual DataItemId getId() { return mId; }
    virtual void stringify(string& /*valueStr*/) {}
    virtual int32_t copy(IDataItemCore* /*src*/, bool* /*dataItemCopied = NULL*/) {return 1;}
    inline RilServiceInfoDataItemBase(const RilServiceInfoDataItemBase& peer) :
            RilServiceInfoDataItemBase() {
        peer.setPeerData(*this);
    }
    inline virtual bool operator==(const RilServiceInfoDataItemBase& other) const {
        return other.mData == mData;
    }
    inline virtual void setPeerData(RilServiceInfoDataItemBase& /*peer*/) const {}
    void* mData;
protected:
    DataItemId mId;
};

class RilCellInfoDataItemBase : public IDataItemCore {
public:
    inline RilCellInfoDataItemBase() :
            mData(nullptr), mId(RILCELLINFO_DATA_ITEM_ID) {}
    inline virtual ~RilCellInfoDataItemBase() { if (nullptr != mData) free(mData); }
    inline virtual DataItemId getId() { return mId; }
    virtual void stringify(string& /*valueStr*/) {}
    virtual int32_t copy(IDataItemCore* /*src*/, bool* /*dataItemCopied = NULL*/) {return 1;}
    inline RilCellInfoDataItemBase(const RilCellInfoDataItemBase& peer) :
            RilCellInfoDataItemBase() {
        peer.setPeerData(*this);
    }
    inline virtual bool operator==(const RilCellInfoDataItemBase& other) const {
        return other.mData == mData;
    }
    inline virtual void setPeerData(RilCellInfoDataItemBase& /*peer*/) const {}
    void* mData;
protected:
    DataItemId mId;
};

class WifiSupplicantStatusDataItemBase : public IDataItemCore {
public:
    WifiSupplicantStatusDataItemBase() :
        mState((WifiSupplicantState)WIFI_SUPPLICANT_DEFAULT_STATE),
        mApMacAddressValid(false),
        mWifiApSsidValid(false),
        mId(WIFI_SUPPLICANT_STATUS_DATA_ITEM_ID) {
            memset (&mApMacAddress, 0, sizeof (mApMacAddress));
            mWifiApSsid.clear();
        }
    virtual ~WifiSupplicantStatusDataItemBase() {}
    inline virtual DataItemId getId() { return mId; }
    virtual void stringify(string& /*valueStr*/) {}
    virtual int32_t copy(IDataItemCore* /*src*/, bool* /*dataItemCopied = NULL*/) {return 1;}
    // Data members
    typedef enum WifiSupplicantState {
        DISCONNECTED,
        INTERFACE_DISABLED,
        INACTIVE,
        SCANNING,
        AUTHENTICATING,
        ASSOCIATING,
        ASSOCIATED,
        FOUR_WAY_HANDSHAKE,
        GROUP_HANDSHAKE,
        COMPLETED,
        DORMANT,
        UNINITIALIZED,
        INVALID
    } WifiSupplicantState;
    /* Represents whether access point attach state*/
    WifiSupplicantState mState;
    /* Represents info on whether ap mac address is valid */
    bool mApMacAddressValid;
    /* Represents mac address of the wifi access point*/
    uint8_t mApMacAddress[MAC_ADDRESS_LENGTH];
    /* Represents info on whether ap SSID is valid */
    bool mWifiApSsidValid;
    /* Represents Wifi SSID string*/
    string mWifiApSsid;
protected:
    DataItemId mId;
};

class TacDataItemBase : public IDataItemCore {
public:
    TacDataItemBase(const string & name) :
        mValue (name),
        mId(TAC_DATA_ITEM_ID) {}
    virtual ~TacDataItemBase() {}
    inline virtual DataItemId getId() { return mId; }
    virtual void stringify(string& /*valueStr*/) {}
    virtual int32_t copy(IDataItemCore* /*src*/, bool* /*dataItemCopied = NULL*/) {return 1;}
// Data members
    string mValue;
protected:
    DataItemId mId;
};

class MccmncDataItemBase : public IDataItemCore {
public:
    MccmncDataItemBase(const string & name) :
        mValue(name),
        mId(MCCMNC_DATA_ITEM_ID) {}
    virtual ~MccmncDataItemBase() {}
    inline virtual DataItemId getId() { return mId; }
    virtual void stringify(string& /*valueStr*/) {}
    virtual int32_t copy(IDataItemCore* /*src*/, bool* /*dataItemCopied = NULL*/) {return 1;}
// Data members
    string mValue;
protected:
    DataItemId mId;
};

class SrnDeviceScanDetailsDataItemBase : public IDataItemCore {
public:
    SrnDeviceScanDetailsDataItemBase(DataItemId Id) :
        mValidSrnData(false),
        mApSrnRssi(-1),
        mApSrnTimestamp(0),
        mRequestTimestamp(0),
        mReceiveTimestamp(0),
        mErrorCause(-1),
        mId(Id) {}
    virtual ~SrnDeviceScanDetailsDataItemBase() {}
    inline virtual DataItemId getId() { return mId; }
    // Data members common to all SRN tech types
    /* Represents info on whether SRN data is valid (no error)*/
    bool mValidSrnData;
    /* SRN device RSSI reported */
    int32_t mApSrnRssi;
    /* MAC adress of SRN device */
    uint8_t mApSrnMacAddress[SRN_MAC_ADDRESS_LENGTH];
    /* UTC timestamp at which the scan was requested.for this SRN device*/
    int64_t mApSrnTimestamp;
    /* UTC timestamp at which the scan was started. */
    int64_t mRequestTimestamp;
    /* UTC timestamp at which the scan was received.*/
    int64_t mReceiveTimestamp;
    /* Reason for the error/failure if SRN details are not valid */
    int32_t mErrorCause;
protected:
    DataItemId mId;
};

class BtDeviceScanDetailsDataItemBase : public SrnDeviceScanDetailsDataItemBase {

public:
    BtDeviceScanDetailsDataItemBase() :
        SrnDeviceScanDetailsDataItemBase(BT_SCAN_DATA_ITEM_ID) {}
    virtual ~BtDeviceScanDetailsDataItemBase() {}
    virtual void stringify(string& /*valueStr*/) {}
    virtual int32_t copy(IDataItemCore* /*src*/, bool* /*dataItemCopied = NULL*/) {return 1;}
};

class BtLeDeviceScanDetailsDataItemBase : public SrnDeviceScanDetailsDataItemBase {

public:
    BtLeDeviceScanDetailsDataItemBase() :
        SrnDeviceScanDetailsDataItemBase(BTLE_SCAN_DATA_ITEM_ID) {}
    virtual ~BtLeDeviceScanDetailsDataItemBase() {}
    virtual void stringify(string& /*valueStr*/) {}
    virtual int32_t copy(IDataItemCore* /*src*/, bool* /*dataItemCopied = NULL*/) {return 1;}
};

class BatteryLevelDataItemBase : public IDataItemCore {
public:
    inline BatteryLevelDataItemBase(uint8_t batteryPct) :
            mBatteryPct(batteryPct), mId(BATTERY_LEVEL_DATA_ITEM_ID) {}
    inline ~BatteryLevelDataItemBase() {}
    inline virtual DataItemId getId() { return mId; }
// Data members
    uint8_t mBatteryPct;
protected:
    DataItemId mId;
};

} // namespace loc_core

#endif //__DATAITEMCONCRETEBASETYPES__
