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

#include "LearnedCapacityBackupRestore.h"

namespace device {
namespace google {
namespace wahoo {
namespace health {

static constexpr char kChgFullFile[] = "sys/class/power_supply/bms/charge_full";
static constexpr char kSysCFPersistFile[] = "/persist/battery/qcom_charge_full";
static constexpr int kBuffSize = 256;

LearnedCapacityBackupRestore::LearnedCapacityBackupRestore() : sw_cap_(0), hw_cap_(0) {}

void LearnedCapacityBackupRestore::Restore() {
    ReadFromStorage();
    ReadFromSRAM();
    if (sw_cap_ == 0) {
        // First backup
        sw_cap_ = hw_cap_;
        SaveToStorage();
    } else {
        // Always restore backup value
        SaveToSRAM();
    }
}

void LearnedCapacityBackupRestore::Backup() {
    ReadFromSRAM();
    if (sw_cap_ != hw_cap_) {
        // Always backup the new FG computed learned capacity
        sw_cap_ = hw_cap_;
        SaveToStorage();
    }
}

void LearnedCapacityBackupRestore::ReadFromStorage() {
    std::string buffer;

    if (!android::base::ReadFileToString(std::string(kSysCFPersistFile), &buffer)) {
        LOG(ERROR) << "Cannot read the storage file";
        return;
    }

    if (sscanf(buffer.c_str(), "%d", &sw_cap_) < 1)
        LOG(ERROR) << "data format is wrong in the storage file: " << buffer;
    else
        LOG(DEBUG) << "Storage data: " << buffer;
}

void LearnedCapacityBackupRestore::SaveToStorage() {
    char strData[kBuffSize];

    snprintf(strData, kBuffSize, "%d", sw_cap_);

    LOG(DEBUG) << "Save to Storage: " << strData;

    if (!android::base::WriteStringToFile(strData, std::string(kSysCFPersistFile)))
        LOG(ERROR) << "Write file error: " << strerror(errno);
}

void LearnedCapacityBackupRestore::ReadFromSRAM() {
    std::string buffer;

    if (!android::base::ReadFileToString(std::string(kChgFullFile), &buffer)) {
        LOG(ERROR) << "Read cycle counter error: " << strerror(errno);
        return;
    }

    buffer = android::base::Trim(buffer);

    if (sscanf(buffer.c_str(), "%d", &hw_cap_) < 1)
        LOG(ERROR) << "Failed to parse SRAM bins: " << buffer;
    else
        LOG(DEBUG) << "SRAM data: " << buffer;
}

void LearnedCapacityBackupRestore::SaveToSRAM() {
    char strData[kBuffSize];

    snprintf(strData, kBuffSize, "%d", hw_cap_);

    LOG(DEBUG) << "Save to SRAM: " << strData;

    if (!android::base::WriteStringToFile(strData, std::string(kChgFullFile)))
        LOG(ERROR) << "Write data error: " << strerror(errno);
}

}  // namespace health
}  // namespace wahoo
}  // namespace google
}  // namespace device
