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

#ifndef LOCATION_INTEGRATION_API_H
#define LOCATION_INTEGRATION_API_H

#include <unordered_set>
#ifdef NO_UNORDERED_SET_OR_MAP
    #include <map>
#else
    #include <unordered_map>
#endif

namespace location_integration
{
/**
 * Configuration API types that are currently supported
 */
enum LocConfigTypeEnum{
    /** Blacklist some SV constellations from being used by the GNSS
     *  standard position engine (SPE). <br/> */
    CONFIG_CONSTELLATIONS = 1,
    /** Enable/disable the constrained time uncertainty feature and
     *  configure related parameters when the feature is
     *  enabled. <br/> */
    CONFIG_CONSTRAINED_TIME_UNCERTAINTY = 2,
    /** Enable/disable the position assisted clock estimator
     *  feature. <br/> */
    CONFIG_POSITION_ASSISTED_CLOCK_ESTIMATOR = 3,
    /** Delete aiding data. This enum is applicable for
     *  deleteAllAidingData() and deleteAidingData(). <br/> */
    CONFIG_AIDING_DATA_DELETION = 4,
    /** Config lever arm parameters. <br/> */
    CONFIG_LEVER_ARM = 5,
    /** Config robust location feature. <br/> */
    CONFIG_ROBUST_LOCATION = 6,
    /** Config minimum GPS week used by the GNSS standard
     *  position engine (SPE). <br/> */
    CONFIG_MIN_GPS_WEEK = 7,
    /** Config vehicle Body-to-Sensor mount angles for dead
     *  reckoning position engine. <br/> */
    CONFIG_BODY_TO_SENSOR_MOUNT_PARAMS = 8,
    /** Config various parameters for dead reckoning position
     *  engine. <br/> */
    CONFIG_DEAD_RECKONING_ENGINE = 8,
    /** Config minimum SV elevation angle setting used by the GNSS
     *  standard position engine (SPE).
     *  <br/> */
    CONFIG_MIN_SV_ELEVATION = 9,
    /** Config the secondary band for configurations used by the GNSS
     *  standard position engine (SPE).
     *  <br/> */
    CONFIG_CONSTELLATION_SECONDARY_BAND = 10,
    /** Config the run state, e.g.: pause/resume, of the position
     * engine <br/> */
    CONFIG_ENGINE_RUN_STATE = 11,
    /** Config user consent to use GTP terrestrial positioning
     *  service. <br/> */
    CONFIG_USER_CONSENT_TERRESTRIAL_POSITIONING = 12,

    /** Get configuration regarding robust location setting used by
     *  the GNSS standard position engine (SPE).  <br/> */
    GET_ROBUST_LOCATION_CONFIG = 100,
    /** Get minimum GPS week configuration used by the GNSS standard
     *  position engine (SPE).
     *  <br/> */
    GET_MIN_GPS_WEEK = 101,
    /** Get minimum SV elevation angle setting used by the GNSS
     *  standard position engine (SPE). <br/> */
    GET_MIN_SV_ELEVATION = 102,
    /** Get the secondary band configuration for constellation
     *  used by the GNSS standard position engine (SPE). <br/> */
    GET_CONSTELLATION_SECONDARY_BAND_CONFIG = 103,
} ;

/**
 *  Specify the asynchronous response when calling location
 *  integration API. */
enum LocIntegrationResponse {
    /** Location integration API request is processed
     *  successfully. <br/>  */
    LOC_INT_RESPONSE_SUCCESS = 1,
    /** Location integration API request is not processed
     *  successfully. <br/>  */
    LOC_INT_RESPONSE_FAILURE = 2,
    /** Location integration API request is not supported. <br/> */
    LOC_INT_RESPONSE_NOT_SUPPORTED = 3,
    /** Location integration API request has invalid parameter.
     *  <br/> */
    LOC_INT_RESPONSE_PARAM_INVALID = 4,
} ;

/** Specify the position engine types used in location
 *  integration api module. <br/> */
enum LocIntegrationEngineType {
    /** This is the standard GNSS position engine. <br/> */
    LOC_INT_ENGINE_SPE   = 1,
    /** This is the precise position engine. <br/> */
    LOC_INT_ENGINE_PPE   = 2,
    /** This is the dead reckoning position engine. <br/> */
    LOC_INT_ENGINE_DRE   = 3,
    /** This is the vision positioning engine. <br/>  */
    LOC_INT_ENGINE_VPE   = 4,
};

/** Specify the position engine run state. <br/> */
enum LocIntegrationEngineRunState {
    /** Request the position engine to be put into pause state.
     *  <br/> */
    LOC_INT_ENGINE_RUN_STATE_PAUSE   = 1,
    /** Request the position engine to be put into resume state.
     *  <br/> */
    LOC_INT_ENGINE_RUN_STATE_RESUME   = 2,
};

/**
 * Define the priority to be used when the corresponding
 * configuration API specified by type is invoked. Priority is
 * specified via uint32_t and lower number means lower
 * priority.  <br/>
 *
 * Currently, all configuration requests, regardless of
 * priority, will be honored. Priority based request
 * honoring will come in subsequent phases and more
 * detailed description on this will be available then. <br/>
 */
typedef std::unordered_map<LocConfigTypeEnum, uint32_t>
        LocConfigPriorityMap;

/** Gnss constellation type mask. <br/> */
typedef uint32_t GnssConstellationMask;

/**
 *  Specify SV Constellation types that is used in
 *  constellation configuration. <br/> */
enum GnssConstellationType {
    /** GLONASS SV system  <br/> */
    GNSS_CONSTELLATION_TYPE_GLONASS  = 1,
    /** QZSS SV system <br/> */
    GNSS_CONSTELLATION_TYPE_QZSS     = 2,
    /** BEIDOU SV system <br/> */
    GNSS_CONSTELLATION_TYPE_BEIDOU   = 3,
    /** GALILEO SV system <br/> */
    GNSS_CONSTELLATION_TYPE_GALILEO  = 4,
    /** SBAS SV system <br/> */
    GNSS_CONSTELLATION_TYPE_SBAS     = 5,
    /** NAVIC SV system <br/> */
    GNSS_CONSTELLATION_TYPE_NAVIC    = 6,
    /** GPS SV system <br/> */
    GNSS_CONSTELLATION_TYPE_GPS      = 7,
    /** Maximum constellatoin system */
    GNSS_CONSTELLATION_TYPE_MAX      = 7,
};

/**
 * Specify the constellation and sv id for an SV. This struct
 * is used to specify the blacklisted SVs in
 * configConstellations(). <br/> */
struct GnssSvIdInfo {
    /** constellation for the sv <br/>  */
    GnssConstellationType constellation;
    /** sv id range for the constellation: <br/>
     * GLONASS SV id range: 65 to 96 <br/>
     * QZSS SV id range: 193 to 197 <br/>
     * BDS SV id range: 201 to 263 <br/>
     * GAL SV id range: 301 to 336 <br/>
     * SBAS SV id range: 120 to 158 and 183 to 191 <br/>
     * NAVIC SV id range: 401 to 414 <br/>
     */
    uint32_t              svId;
};

/**
 *  Mask used to specify the set of aiding data that can be
 *  deleted via deleteAidingData(). <br/> */
enum AidingDataDeletionMask {
    /** Mask to delete ephemeris aiding data <br/> */
    AIDING_DATA_DELETION_EPHEMERIS  = (1 << 0),
    /** Mask to delete calibration data from dead reckoning position
     *  engine.<br/> */
    AIDING_DATA_DELETION_DR_SENSOR_CALIBRATION  = (1 << 1),
};

/**
 *  Lever ARM type <br/> */
enum LeverArmType {
    /** Lever arm parameters regarding the VRP (Vehicle Reference
     *  Point) w.r.t the origin (at the GNSS Antenna). <br/> */
    LEVER_ARM_TYPE_GNSS_TO_VRP = 1,
    /** Lever arm regarding GNSS Antenna w.r.t the origin at the
     *  IMU (inertial measurement unit) for DR (dead reckoning
     *  engine). <br/> */
    LEVER_ARM_TYPE_DR_IMU_TO_GNSS = 2,
    /** Lever arm regarding GNSS Antenna w.r.t the origin at the
     *  IMU (inertial measurement unit) for VEPP (vision enhanced
     *  precise positioning engine). <br/>
     *  Note: this enum will be deprecated. <br/> */
    LEVER_ARM_TYPE_VEPP_IMU_TO_GNSS = 3,
    /** Lever arm regarding GNSS Antenna w.r.t the origin at the
     *  IMU (inertial measurement unit) for VPE (vision positioning
     *  engine). <br/> */
    LEVER_ARM_TYPE_VPE_IMU_TO_GNSS = 3,
};

/**
 * Specify parameters related to lever arm. <br/> */
struct LeverArmParams {
    /** Offset along the vehicle forward axis, in unit of meters
     *  <br/> */
    float forwardOffsetMeters;
    /** Offset along the vehicle starboard axis, in unit of meters.
     *  Left side offset is negative, and right side offset is
     *  positive. <br/> */
    float sidewaysOffsetMeters;
    /** Offset along the vehicle up axis, in unit of meters. <br/> */
    float upOffsetMeters;
};

/**
 * Specify vehicle body-to-Sensor mount parameters for use by
 * dead reckoning positioning engine. <br/> */
struct BodyToSensorMountParams {
    /** The misalignment of the sensor board along the
     *  horizontal plane of the vehicle chassis measured looking
     *  from the vehicle to forward direction. <br/>
     *  In unit of degrees. <br/>
     *  Range [-180.0, 180.0]. <br/> */
    float rollOffset;
    /** The misalignment along the horizontal plane of the vehicle
     *  chassis measured looking from the vehicle to the right
     *  side. Positive pitch indicates vehicle is inclined such
     *  that forward wheels are at higher elevation than rear
     *  wheels. <br/>
     *  In unit of degrees. <br/>
     *  Range [-180.0, 180.0]. <br/> */
    float yawOffset;
    /** The angle between the vehicle forward direction and the
     *  sensor axis as seen from the top of the vehicle, and
     *  measured in counterclockwise direction. <br/>
     *  In unit of degrees. <br/>
     *  Range [-180.0, 180.0]. <br/> */
    float pitchOffset;
    /** Single uncertainty number that may be the largest of the
     *  uncertainties for roll offset, pitch offset and yaw
     *  offset. <br/>
     *  In unit of degrees. <br/>
     *  Range [-180.0, 180.0]. <br/> */
    float offsetUnc;
};

/** Specify the valid mask for the configuration paramters of
 *  dead reckoning position engine <br/> */
enum DeadReckoningEngineConfigValidMask {
    /** DeadReckoningEngineConfig has valid
     *  DeadReckoningEngineConfig::bodyToSensorMountParams. */
    BODY_TO_SENSOR_MOUNT_PARAMS_VALID    = (1<<0),
    /** DeadReckoningEngineConfig has valid
     *  DeadReckoningEngineConfig::vehicleSpeedScaleFactor. */
    VEHICLE_SPEED_SCALE_FACTOR_VALID     = (1<<1),
    /** DeadReckoningEngineConfig has valid
     *  DeadReckoningEngineConfig::vehicleSpeedScaleFactorUnc. */
    VEHICLE_SPEED_SCALE_FACTOR_UNC_VALID = (1<<2),
    /** DeadReckoningEngineConfig has valid
     *  DeadReckoningEngineConfig::gyroScaleFactor. */
    GYRO_SCALE_FACTOR_VALID              = (1<<3),
    /** DeadReckoningEngineConfig has valid
     *  DeadReckoningEngineConfig::gyroScaleFactorUnc. */
    GYRO_SCALE_FACTOR_UNC_VALID          = (1<<4),
};

/** Specify the valid mask for the dead reckoning position
 *  engine <br/> */
struct DeadReckoningEngineConfig{
    /** Specify the valid fields in the config. */
    DeadReckoningEngineConfigValidMask validMask;
    /** Body to sensor mount parameters for use by dead reckoning
     *  positioning engine */
    BodyToSensorMountParams bodyToSensorMountParams;

    /** Vehicle Speed Scale Factor configuration input for the dead
      * reckoning positioning engine. The multiplicative scale
      * factor is applied to received Vehicle Speed value (in m/s)
      * to obtain the true Vehicle Speed.
      *
      * Range is [0.9 to 1.1].
      *
      * Note: The scale factor is specific to a given vehicle
      * make & model. */
    float vehicleSpeedScaleFactor;
    /** Vehicle Speed Scale Factor Uncertainty (68% confidence)
      * configuration input for the dead reckoning positioning
      * engine.
      *
      * Range is [0.0 to 0.1].
      *
      * Note: The scale factor unc is specific to a given vehicle
      * make & model. */
    float vehicleSpeedScaleFactorUnc;

    /** Gyroscope Scale Factor configuration input for the dead
      * reckoning positioning engine. The multiplicative scale
      * factor is applied to received gyroscope value to obtain the
      * true value.
      *
      * Range is [0.9 to 1.1].
      *
      * Note: The scale factor is specific to the Gyroscope sensor
      * and typically derived from either sensor data-sheet or
      * from actual calibration. */
    float gyroScaleFactor;

    /** Gyroscope Scale Factor uncertainty (68% confidence)
      * configuration input for the dead reckoning positioning
      * engine.
      *
      * Range is [0.0 to 0.1].
      * engine.
      *
      * Note: The scale factor unc is specific to the make & model
      * of Gyroscope sensor and typically derived from either
      * sensor data-sheet or from actual calibration. */
    float gyroScaleFactorUnc;
};

/**
 * Define the lever arm parameters to be used with
 * configLeverArm(). <br/>
 *
 * For the types of lever arm parameters that can be configured,
 * refer to LeverArmType. <br/>
 */
typedef std::unordered_map<LeverArmType, LeverArmParams> LeverArmParamsMap;

/**
 * Specify the absolute set of constellations and SVs
 * that should not be used by the GNSS standard position engine
 * (SPE). <br/>
 *
 * To blacklist all SVs from one constellation, use
 * GNSS_SV_ID_BLACKLIST_ALL as sv id for that constellation.
 * <br/>
 *
 * To specify only a subset of the SVs to be blacklisted, for
 * each SV, specify its constelaltion and the SV id and put in
 * the vector. <br/>
 *
 * All SVs being blacklisted should not be used in positioning.
 * For SBAS, SVs are not used in positioning by the GNSS
 * standard position engine (SPE) by default. Blacklisting SBAS
 * SV only blocks SBAS data demod and will not disable SBAS
 * cross-correlation detection algorithms as they are necessary
 * for optimal GNSS standard position engine (SPE) performance.
 * Also, if SBAS is disabld via NV in modem, then it can not be
 * re-enabled via location integration API. <br/>
 *
 * For SV id range, refer documentation of GnssSvIdInfo::svId. <br/>
 */
#define GNSS_SV_ID_BLACKLIST_ALL (0)
typedef std::vector<GnssSvIdInfo> LocConfigBlacklistedSvIdList;

/**
 * Define the constellation set.
 */
typedef std::unordered_set<GnssConstellationType> ConstellationSet;

/** @brief
    Used to get the asynchronous notification of the processing
    status of the configuration APIs. <br/>

    In order to get the notification, an instantiation
    LocConfigCb() need to be passed to the constructor of
    LocationIntegration API. Please refer to each function for
    details regarding how this callback will be invoked. <br/>

    @param
    response: if the response is not LOC_INT_API_RESPONSE_SUCCESS,
    then the integration API of requestType has failed. <br/>
*/
typedef std::function<void(
    /** location configuration request type <br/> */
    LocConfigTypeEnum      configType,
    /** processing status for the location configuration request
     *  <br/> */
    LocIntegrationResponse response
)> LocConfigCb;

/** Specify the valid mask for robust location configuration
 *  used by the GNSS standard position engine (SPE). The robust
 *  location configuraiton can be retrieved by invoking
 *  getRobustLocationConfig(). <br/> */
enum RobustLocationConfigValidMask {
    /** RobustLocationConfig has valid
     *  RobustLocationConfig::enabled. <br/> */
    ROBUST_LOCATION_CONFIG_VALID_ENABLED          = (1<<0),
    /** RobustLocationConfig has valid
     *  RobustLocationConfig::enabledForE911. <br/> */
    ROBUST_LOCATION_CONFIG_VALID_ENABLED_FOR_E911 = (1<<1),
    /** RobustLocationConfig has valid
     *  RobustLocationConfig::version. <br/> */
    ROBUST_LOCATION_CONFIG_VALID_VERSION = (1<<2),
};

/** Specify the versioning info of robust location module for
 *  the GNSS standard position engine (SPE). The versioning info
 *  is part of RobustLocationConfig and will be returned when
 *  invoking getRobustLocationConfig(). RobustLocationConfig()
 *  will be returned via
 *  LocConfigGetRobustLocationConfigCb(). <br/> */
struct RobustLocationVersion {
    /** Major version number. <br/> */
    uint8_t major;
    /** Minor version number. <br/> */
    uint16_t minor;
};

/** Specify the robust location configuration used by the GNSS
 *  standard position engine (SPE) that will be returned when
 *  invoking getRobustLocationConfig(). The configuration will
 *  be returned via LocConfigGetRobustLocationConfigCb().
 *  <br/> */
struct RobustLocationConfig {
    /** Bitwise OR of RobustLocationConfigValidMask to specify
     *  the valid fields. <br/> */
    RobustLocationConfigValidMask validMask;
    /** Specify whether robust location feature is enabled or
     *  not. <br/> */
    bool enabled;
    /** Specify whether robust location feature is enabled or not
     *  when device is on E911 call. <br/> */
    bool enabledForE911;
    /** Specify the version info of robust location module used
     *  by the GNSS standard position engine (SPE). <br/> */
    RobustLocationVersion version;
};

/**
 *  Specify the callback to retrieve the robust location setting
 *  used by the GNSS standard position engine (SPE). The
 *  callback will be invoked for successful processing of
 *  getRobustLocationConfig(). <br/>
 *
 *  In order to receive the robust location configuration,
 *  client shall instantiate the callback and pass it to the
 *  LocationIntegrationApi constructor and then invoke
 *  getRobustLocationConfig(). <br/> */
typedef std::function<void(
    RobustLocationConfig robustLocationConfig
)> LocConfigGetRobustLocationConfigCb;

/**
 *  Specify the callback to retrieve the minimum GPS week
 *  configuration used by the GNSS standard position engine
 *  (SPE). The callback will be invoked for successful
 *  processing of getMinGpsWeek(). The callback shall be passed
 *  to the LocationIntegrationApi constructor. <br/> */
typedef std::function<void(
   uint16_t minGpsWeek
)> LocConfigGetMinGpsWeekCb;

/**
 *  Specify the callback to retrieve the minimum SV elevation
 *  angle setting used by the GNSS standard position engine
 *  (SPE). The callback will be invoked for successful
 *  processing of getMinSvElevation(). The callback shall be
 *  passed to the LocationIntegrationApi constructor. <br/> */
typedef std::function<void(
   uint8_t minSvElevation
)> LocConfigGetMinSvElevationCb;

/** @brief
    LocConfigGetConstellationSecondaryBandConfigCb is for
    receiving the GNSS secondary band configuration for
    constellation. <br/>

    @param secondaryBandDisablementSet: GNSS secondary
    band control configuration. <br/>
    An empty set means secondary bands are enabled for every
    supported constellation. <br/>
*/
typedef std::function<void(
    const ConstellationSet& secondaryBandDisablementSet
)> LocConfigGetConstellationSecondaryBandConfigCb;

/**
 *  Specify the set of callbacks that can be passed to
 *  LocationIntegrationAPI constructor to receive configuration
 *  command processing status and the requested data. <br/>
 */
struct LocIntegrationCbs {
    /** Callback to receive the procesings status, e.g.: success
     *  or failure.  <br/> */
    LocConfigCb configCb;
    /** Callback to receive the robust location setting.  <br/> */
    LocConfigGetRobustLocationConfigCb getRobustLocationConfigCb;
    /** Callback to receive the minimum GPS week setting used by
     *  the GNSS standard position engine (SPE). <br/> */
    LocConfigGetMinGpsWeekCb getMinGpsWeekCb;
    /** Callback to receive the minimum SV elevation angle setting
     *  used by the GNSS standard position engine (SPE). <br/> */
    LocConfigGetMinSvElevationCb getMinSvElevationCb;
    /** Callback to receive the secondary band configuration for
     *  constellation. <br/> */
    LocConfigGetConstellationSecondaryBandConfigCb getConstellationSecondaryBandConfigCb;
};

class LocationIntegrationApiImpl;
class LocationIntegrationApi
{
public:

    /** @brief
        Creates an instance of LocationIntegrationApi object with
        the specified priority map and callback functions. For this
        phase, the priority map will be ignored. <br/>

        @param
        priorityMap: specify the priority for each of the
        configuration type that this integration API client would
        like to control. <br/>

        @param
        integrationCbs: set of callbacks to receive info from
        location integration API. For example, client can pass
        LocConfigCb() to receive the asynchronous processing status
        of configuration command. <br/>
    */
    LocationIntegrationApi(const LocConfigPriorityMap& priorityMap,
                           LocIntegrationCbs& integrationCbs);

    /** @brief Default destructor <br/> */
    ~LocationIntegrationApi();

    /** @brief
        Blacklist some constellations or subset of SVs from the
        constellation from being used by the GNSS standard
        position engine (SPE). <br/>

        Please note this API call is not incremental and the new
        setting will completely overwrite the previous call.
        blacklistedSvList shall contain the complete list
        of blacklisted constellations and blacklisted SVs.
        Constellations and SVs not specified in the parameter will
        be considered to be allowed to get used by GNSS standard
        position engine (SPE).
        <br/>

        Please also note that GPS constellation can not be disabled
        and GPS SV can not be blacklisted. So, if GPS constellation
        is specified to be disabled or GPS SV is specified to be
        blacklisted in the blacklistedSvList, those will be ignored.
        <br/>

        Client should wait for the command to finish, e.g.: via
        LocConfigCb() received before issuing a second
        configConstellations() command. Behavior is not defined if
        client issues a second request of configConstellations()
        without waiting for the finish of the previous
        configConstellations() request. <br/>

        @param
        blacklistedSvList, if not set to nullptr, shall contain
        the complete list of blacklisted constellations and
        blacklisted SVs. Constellations and SVs not specified in
        this parameter will be considered to be allowed to get used
        by GNSS standard position engine (SPE). <br/>

        Nullptr of blacklistedSvList will be interpreted as to reset
        the constellation configuration to device default. <br/>

        Empty blacklistedSvList will be interpreted as to not
        disable any constellation and to not blacklist any SV. <br/>

        @return true, if request is successfully processed as
                requested. When returning true, LocConfigCb() will
                be invoked to deliver asynchronous processing
                status. <br/>

        @return false, if request is not successfully processed as
                requested. When returning false, LocConfigCb() will
                not be invoked. <br/>
    */
    bool configConstellations(const LocConfigBlacklistedSvIdList*
                              blacklistedSvList=nullptr);

    /** @brief
        This API configures the secondary band constellations used
        by the GNSS standard position engine. <br/>

        Please note this API call is not incremental and the new
        setting will completely overwrite the previous call. </br>

        secondaryBandDisablementSet contains
        the enable/disable secondary band info for supported
        constellations. If a constellation is not specified in the
        set, it will be treated as to enable the secondary bands
        for that constellation. Also, please note that the secondary
        bands can only be disabled then re-enabled for the
        constellation via this API if the secondary bands are
        enabled in NV in modem. If the NV in modem is set to disable
        the secondary bands for a particular constellation, then
        attempt to enable the secondary bands for this constellation
        via this API will be no-op. <br/>

        Client should wait for the command to finish, e.g.: via
        LocConfigCb() received before issuing a second
        configConstellationSecondaryBand() command. Behavior is not
        defined if client issues a second request of
        configConstellationSecondaryBand() without waiting for the
        finish of the previous configConstellationSecondaryBand()
        request. <br/>

        @param
        secondaryBandDisablementSet: specify the set of
        constellations whose secondary bands need to be
        disabled. <br/>

        Nullptr and empty secondaryBandDisablementSet will be
        interpreted as to enable the secondary bands for all
        supported constellations. Please note that if the secondary
        band for the constellation is disabled via modem NV, then it
        can not be enabled via this API. <br/>

        @return true, if request is successfully processed as
                requested. When returning true, LocConfigCb() will
                be invoked to deliver asynchronous processing
                status. <br/>

        @return false, if request is not successfully processed as
                requested. When returning false, LocConfigCb() will
                not be invoked. <br/>
    */
    bool configConstellationSecondaryBand(
            const ConstellationSet* secondaryBandDisablementSet);

    /** @brief
        Retrieve the secondary band config for constellation used by
        the standard GNSS engine (SPE). <br/>

        @return true, if the API request has been accepted. The
                successful status will be returned via configCB, and
                secondary band configuration used by the GNSS
                standard position engine (SPE) will be returned via
                LocConfigGetConstellationSecondaryBandConfigCb()
                passed via the constructor. <br/>

        @return false, if the API request has not been accepted for
                further processing. When returning false, LocConfigCb()
                and LocConfigGetConstellationSecondaryBandConfigCb()
                will not be invoked. <br/>
    */
    bool getConstellationSecondaryBandConfig();

     /** @brief
         Enable or disable the constrained time uncertainty feature.
         <br/>

         Client should wait for the command to finish, e.g.:
         via LocConfigCb() received before issuing a second
         configConstrainedTimeUncertainty() command. Behavior is not
         defined if client issues a second request of
         configConstrainedTimeUncertainty() without waiting for
         the finish of the previous
         configConstrainedTimeUncertainty() request. <br/>

         @param
         enable: true to enable the constrained time uncertainty
         feature and false to disable the constrainted time
         uncertainty feature. <br/>

         @param
         tuncThresholdMs: this specifies the time uncertainty
         threshold that GNSS standard position engine (SPE) need to
         maintain, in units of milli-seconds. Default is 0.0 meaning
         that default value of time uncertainty threshold will be
         used. This parameter is ignored when request is to disable
         this feature. <br/>

         @param
         energyBudget: this specifies the power budget that the GNSS
         standard position engine (SPE) is allowed to spend to
         maintain the time uncertainty. Default is 0 meaning that GPS
         engine is not constained by power budget and can spend as
         much power as needed. The parameter need to be specified in
         units of 0.1 milli watt second, e.g.: an energy budget of
         2.0 milli watt will be of 20 units. This parameter is
         ignored when request is to disable this feature. <br/>

        @return true, if the constrained time uncertainty feature is
                successfully enabled or disabled as requested.
                When returning true, LocConfigCb() will be invoked to
                deliver asynchronous processing status. <br/>

        @return false, if the constrained time uncertainty feature is
                not successfully enabled or disabled as requested.
                When returning false, LocConfigCb() will not be
                invoked. <br/>
    */
    bool configConstrainedTimeUncertainty(bool enable,
                                          float tuncThresholdMs = 0.0,
                                          uint32_t energyBudget = 0);

    /** @brief
        Enable or disable position assisted clock estimator feature.
        <br/>

        Client should wait for the command to finish, e.g.: via
        LocConfigCb() received before issuing a second
        configPositionAssistedClockEstimator(). Behavior is
        not defined if client issues a second request of
        configPositionAssistedClockEstimator() without waiting for
        the finish of the previous
        configPositionAssistedClockEstimator() request. <br/>

        @param
        enable: true to enable position assisted clock estimator and
        false to disable the position assisted clock estimator
        feature. <br/>

        @return true, if position assisted clock estimator is
                successfully enabled or disabled as requested. When
                returning true, LocConfigCb() will be invoked to
                deliver asynchronous processing status. <br/>

        @return false, if position assisted clock estimator is not
                successfully enabled or disabled as requested. When
                returning false, LocConfigCb() will not be invoked. <br/>
    */
    bool configPositionAssistedClockEstimator(bool enable);

   /** @brief
        Request deletion of all aiding data from all position
        engines on the device. <br/>

        Invoking this API will trigger cold start of all position
        engines on the device. This will cause significant delay
        for the position engines to produce next fix and may have
        other performance impact. So, this API should only be
        exercised with caution and only for very limited usage
        scenario, e.g.: for performance test and certification
        process. <br/>

        @return true, if the API request has been accepted for
                further processing. When returning true, LocConfigCb()
                with configType set to CONFIG_AIDING_DATA_DELETION
                will be invoked to deliver the asynchronous
                processing status. <br/>

        @return false, if the API request has not been accepted for
                further processing. When returning false, LocConfigCb()
                will not be invoked. <br/>
    */
    bool deleteAllAidingData();


   /** @brief
        Request deletion of the specified aiding data from all
        position engines on the device. <br/>

        Invoking this API may cause noticeable delay for the
        position engine to produce first fix and may have other
        performance impact. For example, remove ephemeris data may
        trigger the GNSS standard position engine (SPE) to do warm
        start. So, this API should only be exercised with caution
        and only for very limited usage scenario, e.g.: for
        performance test and certification process. <br/>

        @param aidingDataMask, specify the set of aiding data to
                be deleted from all position engines. Currently,
                only ephemeris deletion is supported. <br/>

        @return true, if the API request has been accepted for
                further processing. When returning true, LocConfigCb()
                with configType set to CONFIG_AIDING_DATA_DELETION
                will be invoked to deliver the asynchronous
                processing status. <br/>

        @return false, if the API request has not been accepted for
                further processing. When returning false, LocConfigCb()
                will not be invoked. <br/>
    */
    bool deleteAidingData(AidingDataDeletionMask aidingDataMask);


    /** @brief
        Sets the lever arm parameters for the vehicle. <br/>

        LeverArm is sytem level parameters and it is not expected to
        change. So, it is needed to issue configLeverArm() once for
        every application processor boot-up. For multiple
        invocations of this API, client should wait
        for the command to finish, e.g.: via LocConfigCb() received
        before issuing a second configLeverArm(). Behavior is not
        defined if client issues a second request of cconfigLeverArm
        without waiting for the finish of the previous
        configLeverArm() request. <br/>

        @param
        configInfo: lever arm configuration info regarding below two
        types of lever arm info: <br/>
        1: GNSS Antenna w.r.t the origin at the IMU (inertial
        measurement unit) for DR engine <br/>
        2: GNSS Antenna w.r.t the origin at the IMU (inertial
        measurement unit) for VEPP engine <br/>
        3: VRP (Vehicle Reference Point) w.r.t the origin (at the
        GNSS Antenna). Vehicle manufacturers prefer the position
        output to be tied to a specific point in the vehicle rather
        than where the antenna is placed (midpoint of the rear axle
        is typical). <br/>

        Caller can specify which types of lever arm info to
        configure via the leverMarkTypeMask. <br/>

        @return true, if lever arm parameters are successfully
                configured as requested. When returning true,
                LocConfigCb() will be invoked to deliver asynchronous
                processing status. <br/>

        @return false, if lever arm parameters are not successfully
                configured as requested. When returning false,
                LocConfigCb() will not be invoked. <br/>
    */
    bool configLeverArm(const LeverArmParamsMap& configInfo);


    /** @brief
        Enable or disable robust location 2.0 feature. When enabled,
        location_client::GnssLocation shall report conformity index
        of the GNSS navigation solution, which is a measure of
        robustness of the underlying navigation solution. It
        indicates how well the various input data considered for
        navigation solution conform to expectations. In the presence
        of detected spoofed inputs, the navigation solution may take
        corrective actions to mitigate the spoofed inputs and
        improve robustness of the solution. <br/>

        @param
        enable: true to enable robust location and false to disable
        robust location. <br/>

        @param
        enableForE911: true to enable robust location when device is
        on E911 session and false to disable on E911 session. <br/>
        This parameter is only valid if robust location is enabled.
        <br/>

        @return true, if robust location are successfully configured
                as requested. When returning true, LocConfigCb() will be
                invoked to deliver asynchronous processing status.
                <br/>

        @return false, if robust location are not successfully
                configured as requested. When returning false,
                LocConfigCb() will not be invoked. <br/>
    */
    bool configRobustLocation(bool enable, bool enableForE911=false);

    /** @brief
        Query robust location 2.0 setting and version info used by
        the GNSS standard position engine (SPE). <br/>

        If processing of the command fails, the failure status will
        be returned via LocConfigCb(). If the processing of the command
        is successful, the successful status will be returned via
        configCB, and the robust location config info will be
        returned via LocConfigGetRobustLocationConfigCb() passed via
        the constructor. <br/>

        @return true, if the API request has been accepted. <br/>

        @return false, if the API request has not been accepted for
                further processing. When returning false, LocConfigCb()
                and LocConfigGetRobustLocationConfigCb() will not be
                invoked. <br/>
    */
    bool getRobustLocationConfig();

    /** @brief
        Config the minimum GPS week used by the GNSS standard
        position engine (SPE). <br/>

        Also, if this API is called while GNSS standard position
        engine(SPE) is in middle of a session, LocConfigCb() will still
        be invoked shortly after to indicate the setting has been
        accepted by SPE engine, however the actual setting can not
        be applied until the current session ends, and this may take
        up to 255 seconds in poor GPS signal condition. <br/>

        Client should wait for the command to finish, e.g.: via
        LocConfigCb() received before issuing a second configMinGpsWeek()
        command. Behavior is not defined if client issues a second
        request of configMinGpsWeek() without waiting for the
        previous configMinGpsWeek() to finish. <br/>

        @param
        minGpsWeek: minimum GPS week to be used by the GNSS standard
        position engine (SPE). <br/>

        @return true, if minimum GPS week configuration has been
                accepted for further processing. When returning
                true, LocConfigCb() will be invoked to deliver
                asynchronous processing status. <br/>

        @return false, if configuring minimum GPS week is not
                accepted for further processing. When returning
                false, LocConfigCb() will not be invoked. <br/>
    */
    bool configMinGpsWeek(uint16_t minGpsWeek);

    /** @brief
        Retrieve minimum GPS week configuration used by the GNSS
        standard position engine (SPE). If processing of the command
        fails, the failure status will be returned via
        LocConfigCb(). If the processing of the command is
        successful, the successful status will be returned via
        configCB, and the minimum GPS week info will be returned via
        LocConfigGetMinGpsWeekCb() passed via the constructor. <br/>

        Also, if this API is called right after configMinGpsWeek(),
        the returned setting may not match the one specified in
        configMinGpsWeek(), as the setting configured via
        configMinGpsWeek() can not be applied to the GNSS standard
        position engine(SPE) when the engine is in middle of a
        session. In poor GPS signal condition, the session may take
        up to 255 seconds to finish. If after 255 seconds of
        invoking configMinGpsWeek(), the returned value still does
        not match, then the caller need to reapply the setting by
        invoking configMinGpsWeek() again. <br/>

        @return true, if the API request has been accepted. <br/>

        @return false, if the API request has not been accepted for
                further processing. When returning false, LocConfigCb()
                and LocConfigGetMinGpsWeekCb() will not be invoked.
                <br/>
    */
    bool getMinGpsWeek();

    /** @brief
        Configure the vehicle body-to-Sensor mount parameters
        for dead reckoning position engine. <br/>

        Client should wait for the command to finish, e.g.:
        via LocConfigCb() received before issuing a second
        configBodyToSensorMountParams() command. Behavior is not
        defined if client issues a second request of
        configBodyToSensorMountParams() without waiting for the
        finish of the previous configBodyToSensorMountParams()
        request. <br/>

        @param
        b2sParams: vehicle body-to-Sensor mount angles and
        uncertainty. <br/>

        @return true, if the request is accepted for further
                processing. When returning true, LocConfigCb() will be
                invoked to deliver asynchronous processing status. <br/>

        @return false, if the request is not accepted for further
                processing. When returning false, LocConfigCb() will not
                be invoked. <br/>
    */
    bool configBodyToSensorMountParams(const BodyToSensorMountParams& b2sParams);

    /** @brief
        Configure various parameters for dead reckoning position engine. <br/>

        Client should wait for the command to finish, e.g.:
        via LocConfigCb() received before issuing a second
        configDeadReckoningEngineParams() command. Behavior is not
        defined if client issues a second request of
        configDeadReckoningEngineParams() without waiting for the
        finish of the previous configDeadReckoningEngineParams()
        request. <br/>

        @param
        config: dead reckoning engine configuration. <br/>

        @return true, if the request is accepted for further
                processing. When returning true, LocConfigCb() will be
                invoked to deliver asynchronous processing status. <br/>

        @return false, if the request is not accepted for further
                processing. When returning false, LocConfigCb() will not
                be invoked. <br/>
    */
    bool configDeadReckoningEngineParams(const DeadReckoningEngineConfig& config);

    /** @brief
        Configure the minimum SV elevation angle setting used by the
        GNSS standard position engine (SPE). Configuring minimum SV
        elevation setting will not cause SPE to stop tracking low
        elevation SVs. It only controls the list of SVs that are
        used in the filtered position solution, so SVs with
        elevation below the setting will be excluded from use in the
        filtered position solution. Configuring this setting to
        large angle will cause more SVs to get filtered out in the
        filtered position solution and will have negative
        performance impact. <br/>

        Also, the SV info report as specified in
        location_client::GnssSv and SV measurement report as
        specified in location_client::GnssMeasurementsData will not
        be filtered based on the minimum SV elevation angle setting. <br/>

        To apply the setting, the GNSS standard position engine(SPE)
        will require MGP to be turned off briefly. This may cause
        glitch for on-going tracking session and may have other
        performance impact. So, it is advised to use this API with
        caution and only for very limited usage scenario, e.g.: for
        performance test and certification process and for one-time
        device configuration. <br/>

        Also, if this API is called while the GNSS standard position
        engine(SPE) is in middle of a session, LocConfigCb() will still
        be invoked shortly after to indicate the setting has been
        accepted by SPE engine, however the actual setting can not
        be applied until the current session ends, and this may take
        up to 255 seconds in poor GPS signal condition. <br/>

        Client should wait for the command to finish, e.g.: via
        LocConfigCb() received before issuing a second
        configMinSvElevation() command. Behavior is not defined if
        client issues a second request of configMinSvElevation()
        without waiting for the previous configMinSvElevation() to
        finish. <br/>

        @param
        minSvElevation: minimum SV elevation to be used by the GNSS
        standard position engine (SPE). Valid range is [0, 90] in
        unit of degree. <br/>

        @return true, if minimum SV elevation setting has been
                accepted for further processing. When returning
                true, LocConfigCb() will be invoked to deliver
                asynchronous processing status. <br/>

        @return false, if configuring minimum SV elevation is not
                accepted for further processing. When returning
                false, LocConfigCb() will not be invoked. <br/>
    */
    bool configMinSvElevation(uint8_t minSvElevation);

    /** @brief
        Retrieve minimum SV elevation angle setting used by the GNSS
        standard position engine (SPE). <br/>

        Also, if this API is called right after
        configMinSvElevation(), the returned setting may not match
        the one specified in configMinSvElevation(), as the setting
        configured via configMinSvElevation() can not be applied to
        the GNSS standard position engine(SPE) when the engine is in
        middle of a session. In poor GPS signal condition, the
        session may take up to 255 seconds to finish. If after 255
        seconds of invoking configMinSvElevation(), the returned
        value still does not match, then the caller need to reapply
        the setting by invoking configMinSvElevation() again. <br/>

        @return true, if the API request has been accepted. The
                successful status will be returned via configCB, and the
                minimum SV elevation angle setting will be returned
                via LocConfigGetMinSvElevationCb() passed via the
                constructor. <br/>

        @return false, if the API request has not been accepted for
                further processing. When returning false, LocConfigCb()
                and LocConfigGetMinSvElevationCb() will not be
                invoked. <br/>
    */
    bool getMinSvElevation();

    /** @brief
        This API is used to instruct the specified engine to be in
        the pause/resume state. <br/>

        When the engine is placed in paused state, the engine will
        stop. If there is an on-going session, engine will no longer
        produce fixes. In the paused state, calling API to delete
        aiding data from the paused engine may not have effect.
        Request to delete Aiding data shall be issued after
        engine resume. <br/>

        Currently, only DRE engine will support pause/resume
        request. LocConfigCb() will return
        LOC_INT_RESPONSE_NOT_SUPPORTED when request is made to
        pause/resume none-DRE engine. <br/>

        Request to pause/resume DRE engine can be made with or
        without an on-going session. With QDR engine, on resume,
        GNSS position & heading re-acquisition is needed for DR
        engine to engage. If DR engine is already in the requested
        state, the request will be no-op and the API call will
        return success and LocConfigCb() will return
        LOC_INT_RESPONSE_SUCCESS. <br/>

        @param
        engType: the engine that is instructed to change its run
        state. <br/>

        engState: the new engine run state that the engine is
        instructed to be in. <br/>

        @return true, if the API request has been accepted. The
                status will be returned via configCB. When returning
                true, LocConfigCb() will be invoked to deliver
                asynchronous processing status.
                <br/>

        @return false, if the API request has not been accepted for
                further processing. <br/>
    */
    bool configEngineRunState(LocIntegrationEngineType engType,
                              LocIntegrationEngineRunState engState);


    /** @brief
        Set client consent to use terrestrial positioning. <br/>

        Client must call this API with userConsent set to true in order
        to retrieve terrestrial position via
        LocationClientApi::getSingleTerrestrialPosition(). <br/>

        The consent will remain effective across power cycles, until
        this API is called with a different value.  <br/>

        @param
        true: client agrees to the privacy entailed when using terrestrial
        positioning.
        false: client does not agree to the privacy entailed when using
        terrestrial positioning. Due to this, client will not be able to
        retrieve terrestrial position.

        @return true, if client constent has been accepted for further processing.
                When returning true, LocConfigCb() will be invoked to deliver
                asynchronous processing status. <br/>

        @return false, if client constent has not been accepted for further
                processing. When returning false, no further processing
                will be performed and LocConfigCb() will not be invoked.
                <br/>
    */
    bool setUserConsentForTerrestrialPositioning(bool userConsent);

private:
    LocationIntegrationApiImpl* mApiImpl;
};

} // namespace location_integration

#endif /* LOCATION_INTEGRATION_API_H */
