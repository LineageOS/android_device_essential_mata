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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>
#include <string.h>
#include <sstream>
#include <grp.h>
#include <sys/types.h>
#include <sys/prctl.h>
#include <sys/capability.h>
#include <semaphore.h>
#include <loc_pla.h>
#include <loc_cfg.h>
#include <loc_misc_utils.h>
#ifdef NO_UNORDERED_SET_OR_MAP
    #include <map>
#else
    #include <unordered_map>
#endif

#include <LocationClientApi.h>
#include <LocationIntegrationApi.h>

using namespace location_client;
using namespace location_integration;

static bool     outputEnabled = true;
// debug events counter
static uint32_t numLocationCb = 0;
static uint32_t numGnssLocationCb = 0;
static uint32_t numEngLocationCb = 0;
static uint32_t numGnssSvCb = 0;
static uint32_t numGnssNmeaCb = 0;
static uint32_t numDataCb         = 0;
static uint32_t numGnssMeasurementsCb = 0;

#define DISABLE_REPORT_OUTPUT "disableReportOutput"
#define ENABLE_REPORT_OUTPUT  "enableReportOutput"
#define DISABLE_TUNC       "disableTunc"
#define ENABLE_TUNC        "enableTunc"
#define DISABLE_PACE       "disablePACE"
#define ENABLE_PACE        "enablePACE"
#define RESET_SV_CONFIG    "resetSVConfig"
#define CONFIG_SV          "configSV"
#define CONFIG_SECONDARY_BAND     "configSecondaryBand"
#define GET_SECONDARY_BAND_CONFIG "getSecondaryBandConfig"
#define MULTI_CONFIG_SV    "multiConfigSV"
#define DELETE_ALL         "deleteAll"
#define DELETE_AIDING_DATA "deleteAidingData"
#define CONFIG_LEVER_ARM   "configLeverArm"
#define CONFIG_ROBUST_LOCATION  "configRobustLocation"
#define GET_ROBUST_LOCATION_CONFIG "getRobustLocationConfig"
#define CONFIG_MIN_GPS_WEEK "configMinGpsWeek"
#define GET_MIN_GPS_WEEK    "getMinGpsWeek"
#define CONFIG_DR_ENGINE    "configDrEngine"
#define CONFIG_MIN_SV_ELEVATION "configMinSvElevation"
#define GET_MIN_SV_ELEVATION    "getMinSvElevation"
#define CONFIG_ENGINE_RUN_STATE "configEngineRunState"
#define SET_USER_CONSENT    "setUserConsentForTerrestrialPositioning"
#define GET_SINGLE_GTP_WWAN_FIX    "getSingleGtpWwanFix"

// debug utility
static uint64_t getTimestamp() {
    struct timespec ts;
    clock_gettime(CLOCK_BOOTTIME, &ts);
    uint64_t absolute_micro =
            ((uint64_t)(ts.tv_sec)) * 1000000ULL + ((uint64_t)(ts.tv_nsec)) / 1000ULL;
    return absolute_micro;
}

/******************************************************************************
Callback functions
******************************************************************************/
static void onCapabilitiesCb(location_client::LocationCapabilitiesMask mask) {
    printf("<<< onCapabilitiesCb mask=0x%" PRIx64 "\n", mask);
    printf("<<< onCapabilitiesCb mask string=%s",
            LocationClientApi::capabilitiesToString(mask).c_str());
}

static void onResponseCb(location_client::LocationResponse response) {
    printf("<<< onResponseCb err=%u\n", response);
}

static void onLocationCb(const location_client::Location& location) {
    numLocationCb++;
    if (!outputEnabled) {
        return;
    }
    printf("<<< onLocationCb cnt=%u time=%" PRIu64" mask=0x%x lat=%f lon=%f alt=%f\n",
           numLocationCb,
           location.timestamp,
           location.flags,
           location.latitude,
           location.longitude,
           location.altitude);
}

static void onGtpResponseCb(location_client::LocationResponse response) {
    printf("<<< onGtpResponseCb err=%u\n", response);
}

static void onGtpLocationCb(const location_client::Location& location) {
    numLocationCb++;
    if (!outputEnabled) {
        return;
    }
    printf("<<< onGtpLocationCb cnt=%u time=%" PRIu64" mask=0x%x lat=%f lon=%f alt=%f\n",
           numLocationCb,
           location.timestamp,
           location.flags,
           location.latitude,
           location.longitude,
           location.altitude);
}

static void onGnssLocationCb(const location_client::GnssLocation& location) {
    numGnssLocationCb++;
    if (!outputEnabled) {
        return;
    }
    printf("<<< onGnssLocationCb cnt=%u time=%" PRIu64" mask=0x%x lat=%f lon=%f alt=%f\n",
            numGnssLocationCb,
            location.timestamp,
            location.flags,
            location.latitude,
            location.longitude,
            location.altitude);
}

static void onEngLocationsCb(const std::vector<location_client::GnssLocation>& locations) {
    numEngLocationCb++;
    if (!outputEnabled) {
        return;
    }
    for (auto gnssLocation : locations) {
       printf("<<< onEngLocationsCb: cnt=%u time=%" PRIu64" mask=0x%x lat=%f lon=%f alt=%f\n"
              "info mask=0x%" PRIx64 ", nav solution maks = 0x%x, eng type %d, eng mask 0x%x, "
              "session status %d",
              numEngLocationCb,
              gnssLocation.timestamp,
              gnssLocation.flags,
              gnssLocation.latitude,
              gnssLocation.longitude,
              gnssLocation.altitude,
              gnssLocation.gnssInfoFlags,
              gnssLocation.navSolutionMask,
              gnssLocation.locOutputEngType,
              gnssLocation.locOutputEngMask,
              gnssLocation.sessionStatus);
    }
}

static void onGnssSvCb(const std::vector<location_client::GnssSv>& gnssSvs) {
    numGnssSvCb++;
    if (!outputEnabled) {
        return;
    }
    std::stringstream ss;
    ss << "<<< onGnssSvCb c=" << numGnssSvCb << " s=" << gnssSvs.size();
    for (auto sv : gnssSvs) {
        ss << " " << sv.type << ":" << sv.svId << "/" << (uint32_t)sv.cN0Dbhz;
    }
    printf("%s\n", ss.str().c_str());
}

static void onGnssNmeaCb(uint64_t timestamp, const std::string& nmea) {
    numGnssNmeaCb++;
    if (!outputEnabled) {
        return;
    }
    printf("<<< onGnssNmeaCb cnt=%u time=%" PRIu64" nmea=%s",
            numGnssNmeaCb, timestamp, nmea.c_str());
}

static void onGnssDataCb(const location_client::GnssData& gnssData) {
    numDataCb++;
    if (!outputEnabled) {
        return;
    }
    printf("<<< gnssDataCb cnt=%u, %s ", numDataCb, gnssData.toString().c_str());
}

static void onGnssMeasurementsCb(const location_client::GnssMeasurements& gnssMeasurements) {
    numGnssMeasurementsCb++;
    if (!outputEnabled) {
        return;
    }
    printf("<<< onGnssMeasurementsCb cnt=%u, num of meas %d\n", numGnssMeasurementsCb,
           gnssMeasurements.measurements.size());
}

static void onConfigResponseCb(location_integration::LocConfigTypeEnum    requestType,
                               location_integration::LocIntegrationResponse response) {
    printf("<<< onConfigResponseCb, type %d, err %d\n", requestType, response);
}

static void onGetRobustLocationConfigCb(RobustLocationConfig robustLocationConfig) {
    printf("<<< onGetRobustLocationConfigCb, valid flags 0x%x, enabled %d, enabledForE911 %d, "
           "version (major %u, minor %u)\n",
           robustLocationConfig.validMask, robustLocationConfig.enabled,
           robustLocationConfig.enabledForE911, robustLocationConfig.version.major,
           robustLocationConfig.version.minor);
}

static void onGetMinGpsWeekCb(uint16_t minGpsWeek) {
    printf("<<< onGetMinGpsWeekCb, minGpsWeek %d\n", minGpsWeek);
}

static void onGetMinSvElevationCb(uint8_t minSvElevation) {
    printf("<<< onGetMinSvElevationCb, minSvElevationAngleSetting: %d\n", minSvElevation);
}

static void onGetSecondaryBandConfigCb(const ConstellationSet& secondaryBandDisablementSet) {
    for (GnssConstellationType disabledSecondaryBand : secondaryBandDisablementSet) {
        printf("<<< disabled secondary band for constellation %d\n", disabledSecondaryBand);
    }
}

static void printHelp() {
    printf("\n************* options *************\n");
    printf("e: Concurrent engine report session with 100 ms interval\n");
    printf("g: Gnss report session with 100 ms interval\n");
    printf("u: Update a session with 2000 ms interval\n");
    printf("m: Interleaving fix session with 1000 and 2000 ms interval, change every 3 seconds\n");
    printf("s: Stop a session \n");
    printf("p: Ping test\n");
    printf("q: Quit\n");
    printf("r: delete client\n");
    printf("%s supress output from various reports: disable output\n", DISABLE_REPORT_OUTPUT);
    printf("%s enable output from various reports: disable output\n", ENABLE_REPORT_OUTPUT);
    printf("%s tuncThreshold energyBudget: enable tunc\n", ENABLE_TUNC);
    printf("%s: disable tunc\n", DISABLE_TUNC);
    printf("%s: enable PACE\n", ENABLE_PACE);
    printf("%s: disable PACE\n", DISABLE_PACE);
    printf("%s: reset sv config to default\n", RESET_SV_CONFIG);
    printf("%s: configure sv \n", CONFIG_SV);
    printf("%s: configure secondary band\n", CONFIG_SECONDARY_BAND);
    printf("%s: get secondary band configure \n", GET_SECONDARY_BAND_CONFIG);
    printf("%s: mulitple config SV \n", MULTI_CONFIG_SV);
    printf("%s: delete all aiding data\n", DELETE_ALL);
    printf("%s: delete ephemeris/calibration data\n", DELETE_AIDING_DATA);
    printf("%s: config lever arm\n", CONFIG_LEVER_ARM);
    printf("%s: config robust location\n", CONFIG_ROBUST_LOCATION);
    printf("%s: get robust location config\n", GET_ROBUST_LOCATION_CONFIG);
    printf("%s: set min gps week\n", CONFIG_MIN_GPS_WEEK);
    printf("%s: get min gps week\n", GET_MIN_GPS_WEEK);
    printf("%s: config DR engine\n", CONFIG_DR_ENGINE);
    printf("%s: set min sv elevation angle\n", CONFIG_MIN_SV_ELEVATION);
    printf("%s: get min sv elevation angle\n", GET_MIN_SV_ELEVATION);
    printf("%s: config engine run state\n", CONFIG_ENGINE_RUN_STATE);
    printf("%s: set user consent for terrestrial positioning 0/1\n", SET_USER_CONSENT);
    printf("%s: get single shot wwan fix\n", GET_SINGLE_GTP_WWAN_FIX);
}

void setRequiredPermToRunAsLocClient() {
#ifdef USE_GLIB
    if (0 == getuid()) {
        char groupNames[LOC_MAX_PARAM_NAME] = "diag locclient";
        printf("group ids: diag locclient\n");

        gid_t groupIds[LOC_PROCESS_MAX_NUM_GROUPS] = {};
        char *splitGrpString[LOC_PROCESS_MAX_NUM_GROUPS];
        int numGrps = loc_util_split_string(groupNames, splitGrpString,
                LOC_PROCESS_MAX_NUM_GROUPS, ' ');

        int numGrpIds=0;
        for (int i = 0; i < numGrps; i++) {
            struct group* grp = getgrnam(splitGrpString[i]);
            if (grp) {
                groupIds[numGrpIds] = grp->gr_gid;
                printf("Group %s = %d\n", splitGrpString[i], groupIds[numGrpIds]);
                numGrpIds++;
            }
        }
        if (0 != numGrpIds) {
            if (-1 == setgroups(numGrpIds, groupIds)) {
                printf("Error: setgroups failed %s", strerror(errno));
            }
        }
        // Set the group id first and then set the effective userid, to locclient.
        if (-1 == setgid(GID_LOCCLIENT)) {
            printf("Error: setgid failed. %s", strerror(errno));
        }
        // Set user id to locclient
        if (-1 == setuid(UID_LOCCLIENT)) {
            printf("Error: setuid failed. %s", strerror(errno));
        }

        // Set capabilities
        struct __user_cap_header_struct cap_hdr = {};
        cap_hdr.version = _LINUX_CAPABILITY_VERSION;
        cap_hdr.pid = getpid();
        if (prctl(PR_SET_KEEPCAPS, 1) < 0) {
            printf("Error: prctl failed. %s", strerror(errno));
        }

        // Set access to CAP_NET_BIND_SERVICE
        struct __user_cap_data_struct cap_data = {};
        cap_data.permitted = (1 << CAP_NET_BIND_SERVICE);
        cap_data.effective = cap_data.permitted;
        printf("cap_data.permitted: %d", (int)cap_data.permitted);
        if (capset(&cap_hdr, &cap_data)) {
            printf("Error: capset failed. %s", strerror(errno));
        }
    } else {
        printf("Test app started as user: %d", getuid());
    }
#endif// USE_GLIB
}

void parseSVConfig(char* buf, bool & resetConstellation,
                   LocConfigBlacklistedSvIdList &svList) {
    static char *save = nullptr;
    char* token = strtok_r(buf, " ", &save); // skip first one of "configSV"
    token = strtok_r(NULL, " ", &save);
    if (token == nullptr) {
        printf("empty sv blacklist\n");
        return;
    }

    if (strncmp(token, "reset", strlen("reset")) == 0) {
        resetConstellation = true;
        printf("reset sv constellation\n");
        return;
    }

    while (token != nullptr) {
        GnssSvIdInfo svIdInfo = {};
        svIdInfo.constellation = (GnssConstellationType) atoi(token);
        token = strtok_r(NULL, " ", &save);
        if (token == NULL) {
            break;
        }
        svIdInfo.svId = atoi(token);
        svList.push_back(svIdInfo);
        token = strtok_r(NULL, " ", &save);
    }

    printf("parse sv config:\n");
    for (GnssSvIdInfo it : svList) {
        printf("\t\tblacklisted SV: %d, sv id %d\n", (int) it.constellation, it.svId);
    }
}

void parseSecondaryBandConfig(char* buf, bool &nullSecondaryBandConfig,
                              ConstellationSet& secondaryBandDisablementSet) {
    static char *save = nullptr;
    char* token = strtok_r(buf, " ", &save); // skip first one of "configSeconaryBand"
    token = strtok_r(NULL, " ", &save);

    if (token == nullptr) {
        printf("empty secondary band disablement set\n");
        return;
    }

    if (strncmp(token, "null", strlen("null")) == 0) {
        nullSecondaryBandConfig = true;
        printf("null secondary band disablement set\n");
        return;
    }

    while (token != nullptr) {
        GnssConstellationType secondaryBandDisabled = (GnssConstellationType) atoi(token);
        secondaryBandDisablementSet.emplace(secondaryBandDisabled);
        token = strtok_r(NULL, " ", &save);
    }

    printf("\t\tnull SecondaryBandConfig %d\n", nullSecondaryBandConfig);
    for (GnssConstellationType disabledSecondaryBand : secondaryBandDisablementSet) {
        printf("\t\tdisabled secondary constellation %d\n", disabledSecondaryBand);
    }
}

void parseLeverArm (char* buf, LeverArmParamsMap &leverArmMap) {
    static char *save = nullptr;
    char* token = strtok_r(buf, " ", &save); // skip first one of "configLeverArm"
    token = strtok_r(NULL, " ", &save);
    while (token != nullptr) {
        LeverArmParams leverArm = {};
        LeverArmType type = (LeverArmType) 0;
        if (strcmp(token, "vrp") == 0) {
            type = location_integration::LEVER_ARM_TYPE_GNSS_TO_VRP;
        } else if (strcmp(token, "drimu") == 0) {
            type = location_integration::LEVER_ARM_TYPE_DR_IMU_TO_GNSS;
        } else if (strcmp(token, "veppimu") == 0) {
            type = location_integration::LEVER_ARM_TYPE_VEPP_IMU_TO_GNSS;
        } else {
            break;
        }
        token = strtok_r(NULL, " ", &save);
        if (token == NULL) {
            break;
        }
        leverArm.forwardOffsetMeters = atof(token);

        token = strtok_r(NULL, " ", &save);
        if (token == NULL) {
            break;
        }
        leverArm.sidewaysOffsetMeters = atof(token);

        token = strtok_r(NULL, " ", &save);
        if (token == NULL) {
            break;
        }
        leverArm.upOffsetMeters = atof(token);

        printf("type %d, %f %f %f\n", type, leverArm.forwardOffsetMeters,
               leverArm.sidewaysOffsetMeters, leverArm.upOffsetMeters);

        leverArmMap.emplace(type, leverArm);
        token = strtok_r(NULL, " ", &save);
    }
}

void parseDreConfig (char* buf, DeadReckoningEngineConfig& dreConfig) {
    static char *save = nullptr;
    char* token = strtok_r(buf, " ", &save); // skip first one of "configDrEngine"
    printf("Usage: configDrEngine b2s roll pitch yaw unc speed scaleFactor scaleFactorUnc "
           "gyro scaleFactor ScaleFactorUnc\n");

    uint32_t validMask = 0;
    do {
        token = strtok_r(NULL, " ", &save);
        if (token == NULL) {
            printf("missing key word b2s, speed, or gyro\n");
            break;
        }
        if (strncmp(token, "b2s", strlen("b2s"))==0) {
            token = strtok_r(NULL, " ", &save); // skip the token of "b2s"
            if (token == NULL) {
                printf("missing roll offset\n");
                break;
            }
            dreConfig.bodyToSensorMountParams.rollOffset = atof(token);

            token = strtok_r(NULL, " ", &save);
            if (token == NULL) {
                printf("missing pitch offset\n");
                break;
            }
            dreConfig.bodyToSensorMountParams.pitchOffset = atof(token);

            token = strtok_r(NULL, " ", &save);
            if (token == NULL) {
                printf("missing yaw offset\n");
                break;
            }
            dreConfig.bodyToSensorMountParams.yawOffset = atof(token);

            token = strtok_r(NULL, " ", &save);
            if (token == NULL) {
                printf("missing offset uncertainty\n");
                break;
            }
            dreConfig.bodyToSensorMountParams.offsetUnc = atof(token);

            validMask |= BODY_TO_SENSOR_MOUNT_PARAMS_VALID;
        } else if (strncmp(token, "speed", strlen("speed"))==0) {
            token = strtok_r(NULL, " ", &save);
            if (token == NULL) {
                printf("missing speed scale factor\n");
                break;
            }
            dreConfig.vehicleSpeedScaleFactor = atof(token);
            validMask |= VEHICLE_SPEED_SCALE_FACTOR_VALID;

            token = strtok_r(NULL, " ", &save);
            if (token == NULL) {
                printf("missing speed scale factor uncertainty\n");
                break;
            }
            dreConfig.vehicleSpeedScaleFactorUnc = atof(token);
            validMask |= VEHICLE_SPEED_SCALE_FACTOR_UNC_VALID;
        } else if (strncmp(token, "gyro", strlen("gyro"))==0) {
            token = strtok_r(NULL, " ", &save);
            if (token == NULL) {
                printf("missing gyro scale factor\n");
                break;
            }
            dreConfig.gyroScaleFactor = atof(token);
            validMask |= GYRO_SCALE_FACTOR_VALID;

            token = strtok_r(NULL, " ", &save);
            if (token == NULL) {
                printf("missing gyro scale factor uncertainty\n");
                break;
            }
            dreConfig.gyroScaleFactorUnc = atof(token);
            validMask |= GYRO_SCALE_FACTOR_UNC_VALID;
        }
    } while (1);

    dreConfig.validMask = (DeadReckoningEngineConfigValidMask)validMask;
}


/******************************************************************************
Main function
******************************************************************************/

static void checkForAutoStart(int argc, char *argv[]) {
    // check for auto-start option
    if (argc >= 2) {
        if (strncmp (argv[1], "auto", strlen("auto")) == 0) {
            printf("usage: location_clientapi_test_app auto");
            uint32_t pid = (uint32_t)getpid();

            LocationClientApi* pClient = new LocationClientApi(onCapabilitiesCb);
            if (nullptr == pClient) {
                printf("can not create Location client API");
                exit(1);
            }

            bool cleanup = false;
            if (argc >= 3) {
                if (strncmp (argv[2], "clean", strlen("clean")) == 0) {
                    cleanup = true;
                }
            }
            // wait for capability to come
            sleep(1);

            EngineReportCbs reportcbs;
            reportcbs.engLocationsCallback = EngineLocationsCb(onEngLocationsCb);
            reportcbs.gnssSvCallback = GnssSvCb(onGnssSvCb);
            reportcbs.gnssNmeaCallback = GnssNmeaCb(onGnssNmeaCb);
            LocReqEngineTypeMask reqEngMask = (LocReqEngineTypeMask)
                    (LOC_REQ_ENGINE_FUSED_BIT|LOC_REQ_ENGINE_SPE_BIT|
                    LOC_REQ_ENGINE_PPE_BIT);
            pClient->startPositionSession(100, reqEngMask, reportcbs, onResponseCb);
            // wait for fix report to come
            sleep(2);
            while (numEngLocationCb < 10) {
                sleep(3);
                printf("pid %u, recevied %d report\n", pid, numEngLocationCb);
            }
            printf("pid %u, recevied %d report\n", pid, numEngLocationCb);
            if (pClient && cleanup) {
                printf("calling stopPosition and delete client \n");
                pClient->stopPositionSession();
                delete pClient;
                sleep(1);
            }
            exit(0);
        }
    }
}

int main(int argc, char *argv[]) {

    setRequiredPermToRunAsLocClient();
    checkForAutoStart(argc, argv);

    // create Location client API
    LocationClientApi* pClient = new LocationClientApi(onCapabilitiesCb);
    if (!pClient) {
        printf("failed to create client, return");
        return -1;;
    }

    // callbacks
    GnssReportCbs reportcbs;
    reportcbs.gnssLocationCallback = GnssLocationCb(onGnssLocationCb);
    reportcbs.gnssSvCallback = GnssSvCb(onGnssSvCb);
    reportcbs.gnssNmeaCallback = GnssNmeaCb(onGnssNmeaCb);

    // callbacks
    EngineReportCbs enginecbs;
    enginecbs.engLocationsCallback = EngineLocationsCb(onEngLocationsCb);
    enginecbs.gnssSvCallback = GnssSvCb(onGnssSvCb);
    enginecbs.gnssNmeaCallback = GnssNmeaCb(onGnssNmeaCb);
    enginecbs.gnssMeasurementsCallback = GnssMeasurementsCb(onGnssMeasurementsCb);
    enginecbs.gnssDataCallback = GnssDataCb(onGnssDataCb);

    // create location integratin API
    LocIntegrationCbs intCbs;

    intCbs.configCb = LocConfigCb(onConfigResponseCb);
    intCbs.getRobustLocationConfigCb =
            LocConfigGetRobustLocationConfigCb(onGetRobustLocationConfigCb);
    intCbs.getMinGpsWeekCb = LocConfigGetMinGpsWeekCb(onGetMinGpsWeekCb);
    intCbs.getMinSvElevationCb = LocConfigGetMinSvElevationCb(onGetMinSvElevationCb);
    intCbs.getConstellationSecondaryBandConfigCb =
            LocConfigGetConstellationSecondaryBandConfigCb(onGetSecondaryBandConfigCb);

    LocConfigPriorityMap priorityMap;
    location_integration::LocationIntegrationApi* pIntClient =
            new LocationIntegrationApi(priorityMap, intCbs);

    printHelp();
    sleep(1); // wait for capability callback if you don't like sleep

    // main loop
    while (1) {
        char buf[300];
        memset (buf, 0, sizeof(buf));
        fgets(buf, sizeof(buf), stdin);

        printf("execute command %s\n", buf);
        if (!pIntClient) {
            pIntClient =
                    new LocationIntegrationApi(priorityMap, intCbs);
            if (nullptr == pIntClient) {
                printf("failed to create integration client\n");
                break;
            }
            sleep(1); // wait for capability callback if you don't like sleep
        }

        if (strncmp(buf, ENABLE_REPORT_OUTPUT, strlen(ENABLE_REPORT_OUTPUT)) == 0) {
            outputEnabled = true;
        } else if (strncmp(buf, DISABLE_REPORT_OUTPUT, strlen(DISABLE_REPORT_OUTPUT)) == 0) {
            outputEnabled = false;
        } else if (strncmp(buf, DISABLE_TUNC, strlen(DISABLE_TUNC)) == 0) {
            pIntClient->configConstrainedTimeUncertainty(false);
        } else if (strncmp(buf, ENABLE_TUNC, strlen(ENABLE_TUNC)) == 0) {
            // get tuncThreshold and energyBudget from the command line
            static char *save = nullptr;
            float tuncThreshold = 0.0;
            int   energyBudget = 0;
            char* token = strtok_r(buf, " ", &save); // skip first one
            token = strtok_r(NULL, " ", &save);
            if (token != NULL) {
                tuncThreshold = atof(token);
                token = strtok_r(NULL, " ", &save);
                if (token != NULL) {
                    energyBudget = atoi(token);
                }
            }
            printf("tuncThreshold %f, energyBudget %d\n", tuncThreshold, energyBudget);
            pIntClient->configConstrainedTimeUncertainty(
                    true, tuncThreshold, energyBudget);
        } else if (strncmp(buf, DISABLE_PACE, strlen(DISABLE_TUNC)) == 0) {
            pIntClient->configPositionAssistedClockEstimator(false);
        } else if (strncmp(buf, ENABLE_PACE, strlen(ENABLE_TUNC)) == 0) {
            pIntClient->configPositionAssistedClockEstimator(true);
        } else if (strncmp(buf, DELETE_ALL, strlen(DELETE_ALL)) == 0) {
            pIntClient->deleteAllAidingData();
        } else if (strncmp(buf, DELETE_AIDING_DATA, strlen(DELETE_AIDING_DATA)) == 0) {
            uint32_t aidingDataMask = 0;
            printf("deleteAidingData 1 (eph) 2 (qdr calibration data) 3 (eph+calibraiton dat)\n");
            static char *save = nullptr;
            char* token = strtok_r(buf, " ", &save); // skip first one
            token = strtok_r(NULL, " ", &save);
            if (token != NULL) {
                aidingDataMask = atoi(token);
            }
            pIntClient->deleteAidingData((AidingDataDeletionMask) aidingDataMask);
        } else if (strncmp(buf, RESET_SV_CONFIG, strlen(RESET_SV_CONFIG)) == 0) {
            pIntClient->configConstellations(nullptr);
        } else if (strncmp(buf, CONFIG_SV, strlen(CONFIG_SV)) == 0) {
            bool resetConstellation = false;
            LocConfigBlacklistedSvIdList svList;
            LocConfigBlacklistedSvIdList* svListPtr = &svList;
            parseSVConfig(buf, resetConstellation, svList);
            if (resetConstellation) {
                svListPtr = nullptr;
            }
            pIntClient->configConstellations(svListPtr);
        } else if (strncmp(buf, CONFIG_SECONDARY_BAND, strlen(CONFIG_SECONDARY_BAND)) == 0) {
            bool nullSecondaryConfig = false;
            ConstellationSet secondaryBandDisablementSet;
            ConstellationSet* secondaryBandDisablementSetPtr = &secondaryBandDisablementSet;
            parseSecondaryBandConfig(buf, nullSecondaryConfig, secondaryBandDisablementSet);
            if (nullSecondaryConfig) {
                secondaryBandDisablementSetPtr = nullptr;
                printf("setting secondary band config to null\n");
            }
            pIntClient->configConstellationSecondaryBand(secondaryBandDisablementSetPtr);
        } else if (strncmp(buf, GET_SECONDARY_BAND_CONFIG,
                           strlen(GET_SECONDARY_BAND_CONFIG)) == 0) {
            pIntClient->getConstellationSecondaryBandConfig();
        } else if (strncmp(buf, MULTI_CONFIG_SV, strlen(MULTI_CONFIG_SV)) == 0) {
            // reset
            pIntClient->configConstellations(nullptr);
            // disable GAL
            LocConfigBlacklistedSvIdList galSvList;
            GnssSvIdInfo svIdInfo = {};
            svIdInfo.constellation = GNSS_CONSTELLATION_TYPE_GALILEO;
            svIdInfo.svId = 0;
            galSvList.push_back(svIdInfo);
            pIntClient->configConstellations(&galSvList);

            // disable SBAS
            LocConfigBlacklistedSvIdList sbasSvList;
            svIdInfo.constellation = GNSS_CONSTELLATION_TYPE_SBAS;
            svIdInfo.svId = 0;
            sbasSvList.push_back(svIdInfo);
            pIntClient->configConstellations(&sbasSvList);
        } else if (strncmp(buf, CONFIG_LEVER_ARM, strlen(CONFIG_LEVER_ARM)) == 0) {
            LeverArmParamsMap configInfo;
            parseLeverArm(buf, configInfo);
            pIntClient->configLeverArm(configInfo);
        } else if (strncmp(buf, CONFIG_ROBUST_LOCATION, strlen(CONFIG_ROBUST_LOCATION)) == 0) {
            // get enable and enableForE911
            static char *save = nullptr;
            bool enable = false;
            bool enableForE911 = false;
            // skip first argument of configRobustLocation
            char* token = strtok_r(buf, " ", &save);
            token = strtok_r(NULL, " ", &save);
            if (token != NULL) {
                enable = (atoi(token) == 1);
                token = strtok_r(NULL, " ", &save);
                if (token != NULL) {
                    enableForE911 = (atoi(token) == 1);
                }
            }
            printf("enable %d, enableForE911 %d\n", enable, enableForE911);
            pIntClient->configRobustLocation(enable, enableForE911);
        } else if (strncmp(buf, GET_ROBUST_LOCATION_CONFIG,
                           strlen(GET_ROBUST_LOCATION_CONFIG)) == 0) {
            pIntClient->getRobustLocationConfig();
        } else if (strncmp(buf, CONFIG_MIN_GPS_WEEK, strlen(CONFIG_MIN_GPS_WEEK)) == 0) {
            static char *save = nullptr;
            uint16_t gpsWeekNum = 0;
            // skip first argument of configMinGpsWeek
            char* token = strtok_r(buf, " ", &save);
            token = strtok_r(NULL, " ", &save);
            if (token != NULL) {
                gpsWeekNum = (uint16_t) atoi(token);
            }
            printf("gps week num %d\n", gpsWeekNum);
            pIntClient->configMinGpsWeek(gpsWeekNum);
        } else if (strncmp(buf, GET_MIN_GPS_WEEK, strlen(GET_MIN_GPS_WEEK)) == 0) {
            pIntClient->getMinGpsWeek();
        } else if (strncmp(buf, CONFIG_DR_ENGINE, strlen(CONFIG_DR_ENGINE)) == 0) {
            DeadReckoningEngineConfig dreConfig = {};
            parseDreConfig(buf, dreConfig);
            printf("mask 0x%x, roll %f, speed %f, yaw %f\n", dreConfig.validMask,
                   dreConfig.bodyToSensorMountParams.rollOffset, dreConfig.vehicleSpeedScaleFactor,
                   dreConfig.gyroScaleFactor);
            pIntClient->configDeadReckoningEngineParams(dreConfig);
        } else if (strncmp(buf, CONFIG_MIN_SV_ELEVATION, strlen(CONFIG_MIN_SV_ELEVATION)) == 0) {
            static char *save = nullptr;
            uint8_t minSvElevation = 0;
            // skip first argument of configMinSvElevation
            char* token = strtok_r(buf, " ", &save);
            token = strtok_r(NULL, " ", &save);
            if (token != NULL) {
                minSvElevation = (uint16_t) atoi(token);
            }
            printf("min Sv elevation %d\n", minSvElevation);
            pIntClient->configMinSvElevation(minSvElevation);
        } else if (strncmp(buf, GET_MIN_SV_ELEVATION, strlen(GET_MIN_SV_ELEVATION)) == 0) {
            pIntClient->getMinSvElevation();
        } else if (strncmp(buf, CONFIG_ENGINE_RUN_STATE, strlen(CONFIG_ENGINE_RUN_STATE)) == 0) {
            printf("%s 3(DRE) 1(pause)/2(resume)", CONFIG_ENGINE_RUN_STATE);
            static char *save = nullptr;
            LocIntegrationEngineType engType = (LocIntegrationEngineType)0;
            LocIntegrationEngineRunState engState = (LocIntegrationEngineRunState) 0;
            // skip first argument of configEngineRunState
            char* token = strtok_r(buf, " ", &save);
            token = strtok_r(NULL, " ", &save);
            if (token != NULL) {
               engType = (LocIntegrationEngineType) atoi(token);
                token = strtok_r(NULL, " ", &save);
                if (token != NULL) {
                    engState = (LocIntegrationEngineRunState) atoi(token);
                }
            }
            printf("eng type %d, eng state %d\n", engType, engState);
            bool retVal = pIntClient->configEngineRunState(engType, engState);
            printf("configEngineRunState returned %d\n", retVal);
        } else if (strncmp(buf, SET_USER_CONSENT, strlen(SET_USER_CONSENT)) == 0) {
            static char *save = nullptr;
            bool userConsent = false;
            char* token = strtok_r(buf, " ", &save);
            token = strtok_r(NULL, " ", &save);
            if (token != NULL) {
                userConsent = (atoi(token) != 0);
            }
            printf("userConsent %d\n", userConsent);
            pIntClient->setUserConsentForTerrestrialPositioning(userConsent);
        } else if (strncmp(buf, GET_SINGLE_GTP_WWAN_FIX, strlen(GET_SINGLE_GTP_WWAN_FIX)) == 0) {
            static char *save = nullptr;
            uint32_t timeoutMsec = 0;
            float horQoS = 0.0;
            uint32_t techMask = 0x0;
            // skip first argument
            char* token = strtok_r(buf, " ", &save);
            // get timeout
            token = strtok_r(NULL, " ", &save);
            if (token != NULL) {
                timeoutMsec = atoi(token);
            }
            // get qos
            token = strtok_r(NULL, " ", &save);
            if (token != NULL) {
                horQoS = atof(token);
            }
            // tech mask
            token = strtok_r(NULL, " ", &save);
            if (token != NULL) {
                techMask = atoi(token);
            }
            printf("timeout msec %d, horQoS %f, techMask 0x%x\n", timeoutMsec, horQoS, techMask);
            if (!pClient) {
                pClient = new LocationClientApi(onCapabilitiesCb);
            }
            pClient->getSingleTerrestrialPosition(timeoutMsec, (TerrestrialTechnologyMask) techMask,
                                                  horQoS, onGtpLocationCb, onGtpResponseCb);
        } else {
            int command = buf[0];
            switch(command) {
            case 'e':
                if (!pClient) {
                    pClient = new LocationClientApi(onCapabilitiesCb);
                }
                if (pClient) {
                    LocReqEngineTypeMask reqEngMask = (LocReqEngineTypeMask)
                        (LOC_REQ_ENGINE_FUSED_BIT|LOC_REQ_ENGINE_SPE_BIT|
                         LOC_REQ_ENGINE_PPE_BIT);
                    pClient->startPositionSession(100, reqEngMask, enginecbs, onResponseCb);
                }
                break;
            case 'g':
                if (!pClient) {
                    pClient = new LocationClientApi(onCapabilitiesCb);
                }
                if (pClient) {
                    pClient->startPositionSession(100, reportcbs, onResponseCb);
                }
                break;
            case 'u':
                if (!pClient) {
                    pClient = new LocationClientApi(onCapabilitiesCb);
                }
                if (pClient) {
                    pClient->startPositionSession(2000, reportcbs, onResponseCb);
                }
                break;
            case 's':
                if (pClient) {
                    pClient->stopPositionSession();
                    delete pClient;
                    pClient = NULL;
                }
                break;
            case 'm':
                if (!pClient) {
                    pClient = new LocationClientApi(onCapabilitiesCb);
                }
                if (pClient) {
                    int i = 0;
                    do {
                        if (i%2 == 0) {
                            pClient->startPositionSession(2000, reportcbs, onResponseCb);
                        } else {
                            pClient->startPositionSession(1000, reportcbs, onResponseCb);
                        }
                        i++;
                        sleep(3);
                    } while (1);
                }
                break;
            case 'r':
                if (nullptr != pClient) {
                    delete pClient;
                    pClient = nullptr;
                }
                if (nullptr != pIntClient) {
                    delete pIntClient;
                    pIntClient = nullptr;
                }
                break;
            case 'q':
                goto EXIT;
                break;
            default:
                printf("unknown command %s\n", buf);
                break;
            }
        }
    }//while(1)

EXIT:
    if (nullptr != pClient) {
        delete pClient;
    }

    if (nullptr != pIntClient) {
        delete pIntClient;
    }

    printf("Done\n");
    return 0;
}
