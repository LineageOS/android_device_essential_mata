/*
 * Copyright (C) 2018 The Android Open Source Project
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

#include "CycleCountBackupRestore.h"

namespace device {
namespace essential {
namespace mata {
namespace health {

static constexpr char kCycCntFile[] = "sys/class/power_supply/bms/device/cycle_counts_bins";
static constexpr char kSysPersistFile[] = "/persist/battery/qcom_cycle_counts_bins";
static constexpr int kBuffSize = 256;

CycleCountBackupRestore::CycleCountBackupRestore() { }

void CycleCountBackupRestore::Restore()
{
    ReadFromStorage();
    ReadFromSRAM();
    UpdateAndSave();
}

void CycleCountBackupRestore::Backup()
{
    ReadFromSRAM();
    UpdateAndSave();
}

void CycleCountBackupRestore::ReadFromStorage()
{
    std::string buffer;

    if (!android::base::ReadFileToString(std::string(kSysPersistFile), &buffer)) {
        LOG(ERROR) << "Cannot read the storage file";
        return;
    }

    if (sscanf(buffer.c_str(), "%d %d %d %d %d %d %d %d",
               &sw_bins_[0], &sw_bins_[1], &sw_bins_[2], &sw_bins_[3],
               &sw_bins_[4], &sw_bins_[5], &sw_bins_[6], &sw_bins_[7])
        != kBucketCount)
        LOG(ERROR) << "data format is wrong in the storage file: " << buffer;
    else
        LOG(INFO) << "Storage data: " << buffer;
}

void CycleCountBackupRestore::SaveToStorage()
{
    char strData[kBuffSize];

    snprintf(strData, kBuffSize, "%d %d %d %d %d %d %d %d",
             sw_bins_[0], sw_bins_[1], sw_bins_[2], sw_bins_[3],
             sw_bins_[4], sw_bins_[5], sw_bins_[6], sw_bins_[7]);

    LOG(INFO) << "Save to Storage: " << strData;

    if (!android::base::WriteStringToFile(strData, std::string(kSysPersistFile)))
        LOG(ERROR) << "Write file error: " << strerror(errno);
}

void CycleCountBackupRestore::ReadFromSRAM()
{
    std::string buffer;

    if (!android::base::ReadFileToString(std::string(kCycCntFile), &buffer)) {
        LOG(ERROR) << "Read cycle counter error: " << strerror(errno);
        return;
    }

    buffer = android::base::Trim(buffer);

    if (sscanf(buffer.c_str(), "%d %d %d %d %d %d %d %d",
               &hw_bins_[0], &hw_bins_[1], &hw_bins_[2], &hw_bins_[3],
               &hw_bins_[4], &hw_bins_[5], &hw_bins_[6], &hw_bins_[7])
        != kBucketCount)
        LOG(ERROR) << "Failed to parse SRAM bins: " << buffer;
    else
        LOG(INFO) << "SRAM data: " << buffer;
}

void CycleCountBackupRestore::SaveToSRAM()
{
    char strData[kBuffSize];

    snprintf(strData, kBuffSize, "%d %d %d %d %d %d %d %d",
             hw_bins_[0], hw_bins_[1], hw_bins_[2], hw_bins_[3],
             hw_bins_[4], hw_bins_[5], hw_bins_[6], hw_bins_[7]);

    LOG(INFO) << "Save to SRAM: "  << strData ;

    if (!android::base::WriteStringToFile(strData, std::string(kCycCntFile)))
        LOG(ERROR) << "Write data error: " << strerror(errno);
}


void CycleCountBackupRestore::UpdateAndSave()
{
    bool backup = false;
    bool restore = false;
    for (int i = 0; i < kBucketCount; i++) {
        if (hw_bins_[i] < sw_bins_[i]) {
            hw_bins_[i] = sw_bins_[i];
            restore = true;
        } else if (hw_bins_[i] > sw_bins_[i]) {
            sw_bins_[i] = hw_bins_[i];
            backup = true;
        }
    }
    if (restore)
        SaveToSRAM();
    if (backup)
        SaveToStorage();
}

} // namespace health
} // namespace mata
} // namespace essential
} // namespace device
