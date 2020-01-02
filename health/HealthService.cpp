/*
 * Copyright (C) 2020 The LineageOS Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "android.hardware.health@2.0-service.mata"
#include <android-base/logging.h>

#include <android-base/file.h>
#include <android-base/parseint.h>
#include <android-base/strings.h>
#include <health2/Health.h>
#include <health2/service.h>
#include <healthd/healthd.h>
#include <hidl/HidlTransportSupport.h>
#include <pixelhealth/BatteryMetricsLogger.h>
#include <pixelhealth/CycleCountBackupRestore.h>
#include <pixelhealth/DeviceHealth.h>
#include <pixelhealth/LowBatteryShutdownMetrics.h>

#include <fstream>
#include <iomanip>
#include <string>
#include <vector>

#include "LearnedCapacityBackupRestore.h"

namespace {

using android::hardware::health::V2_0::DiskStats;
using android::hardware::health::V2_0::StorageAttribute;
using android::hardware::health::V2_0::StorageInfo;
using hardware::google::pixel::health::CycleCountBackupRestore;
using device::google::wahoo::health::LearnedCapacityBackupRestore;

const std::string kChargerStatus = "sys/class/power_supply/battery/status";

static CycleCountBackupRestore ccBackupRestoreBMS(
    8, "/sys/class/power_supply/bms/device/cycle_counts_bins",
    "/persist/battery/qcom_cycle_counts_bins");

static LearnedCapacityBackupRestore lcBackupRestore;

const std::string kUFSHealthFile{"/sys/kernel/debug/ufshcd0/dump_health_desc"};
const std::string kUFSHealthVersionFile{"/sys/kernel/debug/ufshcd0/show_hba"};
const std::string kDiskStatsFile{"/sys/block/sda/stat"};
const std::string kUFSName{"UFS0"};

std::ifstream assert_open(const std::string &path) {
    std::ifstream stream(path);
    if (!stream.is_open()) {
        LOG(FATAL) << "Cannot read " << path;
    }
    return stream;
}

void fill_ufs_storage_attribute(StorageAttribute *attr) {
    attr->isInternal = true;
    attr->isBootDevice = true;
    attr->name = kUFSName;
}

}  // anonymous namespace

void healthd_board_init(struct healthd_config *config) {
    ccBackupRestoreBMS.Restore();
    lcBackupRestore.Restore();

    config->batteryStatusPath = kChargerStatus.c_str();
}

int healthd_board_battery_update(struct android::BatteryProperties *props) {
    ccBackupRestoreBMS.Backup(props->batteryLevel);
    lcBackupRestore.Backup();

    return 0;
}

void get_storage_info(std::vector<StorageInfo> &vec_storage_info) {
    vec_storage_info.resize(1);
    StorageInfo *storage_info = &vec_storage_info[0];
    fill_ufs_storage_attribute(&storage_info->attr);

    std::string version;
    if (!android::base::ReadFileToString(std::string(kUFSHealthVersionFile), &version)) {
        return;
    }

    std::vector<std::string> lines = android::base::Split(version, "\n");
    if (lines.empty()) {
        return;
    }

    char rev[8];
    if (sscanf(lines[6].c_str(), "hba->ufs_version = 0x%7s\n", rev) < 1) {
        return;
    }

    storage_info->version = "ufs " + std::string(rev);

    std::string buffer;
    if (!android::base::ReadFileToString(std::string(kUFSHealthFile), &buffer)) {
        return;
    }

    lines = android::base::Split(buffer, "\n");
    if (lines.empty()) {
        return;
    }

    for (size_t i = 1; i < lines.size(); i++) {
        char token[32];
        uint16_t val;
        int ret;
        if ((ret = sscanf(lines[i].c_str(),
                   "Health Descriptor[Byte offset 0x%*d]: %31s = 0x%hx",
                   token, &val)) < 2) {
            continue;
        }

        if (std::string(token) == "bPreEOLInfo") {
            storage_info->eol = val;
        } else if (std::string(token) == "bDeviceLifeTimeEstA") {
            storage_info->lifetimeA = val;
        } else if (std::string(token) == "bDeviceLifeTimeEstB") {
            storage_info->lifetimeB = val;
        }
    }

    return;
}

void get_disk_stats(std::vector<DiskStats> &vec_stats) {
    vec_stats.resize(1);
    DiskStats *stats = &vec_stats[0];
    fill_ufs_storage_attribute(&stats->attr);

    auto stream = assert_open(kDiskStatsFile);
    // Regular diskstats entries
    stream >> stats->reads >> stats->readMerges >> stats->readSectors >> stats->readTicks >>
        stats->writes >> stats->writeMerges >> stats->writeSectors >> stats->writeTicks >>
        stats->ioInFlight >> stats->ioTicks >> stats->ioInQueue;
    return;
}

int main(void) {
    return health_service_main();
}
