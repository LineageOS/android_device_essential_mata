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

#ifndef DEVICE_GOOGLE_WAHOO_HEALTH_LEARNEDCAPACITYBACKUPRESTORE_H
#define DEVICE_GOOGLE_WAHOO_HEALTH_LEARNEDCAPACITYBACKUPRESTORE_H

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/strings.h>
#include <string>

namespace device {
namespace google {
namespace wahoo {
namespace health {

class LearnedCapacityBackupRestore {
  public:
    LearnedCapacityBackupRestore();
    void Restore();
    void Backup();

  private:
    int sw_cap_;
    int hw_cap_;

    void ReadFromStorage();
    void SaveToStorage();
    void ReadFromSRAM();
    void SaveToSRAM();
};

}  // namespace health
}  // namespace wahoo
}  // namespace google
}  // namespace device

#endif  // #ifndef DEVICE_GOOGLE_WAHOO_HEALTH_LEARNEDCAPACITYBACKUPRESTORE_H
