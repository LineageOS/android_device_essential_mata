#!/bin/bash
#
# Copyright (C) 2016 The CyanogenMod Project
# Copyright (C) 2017-2020 The LineageOS Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

set -e

DEVICE=mata
VENDOR=essential

# Load extract_utils and do some sanity checks
MY_DIR="${BASH_SOURCE%/*}"
if [[ ! -d "$MY_DIR" ]]; then MY_DIR="$PWD"; fi

LINEAGE_ROOT="$MY_DIR"/../../..

HELPER="$LINEAGE_ROOT"/vendor/lineage/build/tools/extract_utils.sh
if [ ! -f "$HELPER" ]; then
    echo "Unable to find helper script at $HELPER"
    exit 1
fi
. "$HELPER"

while [ "$1" != "" ]; do
    case $1 in
        -n | --no-cleanup )     CLEAN_VENDOR=false
                                ;;
        -s | --section )        shift
                                SECTION=$1
                                CLEAN_VENDOR=false
                                ;;
        * )                     SRC=$1
                                ;;
    esac
    shift
done

if [ -z "$SRC" ]; then
    SRC=adb
fi

function blob_fixup() {
    case "${1}" in
    vendor/bin/imsrcsd)
        patchelf --add-needed "libbase_shim.so" "${2}"
        ;;
    vendor/etc/init/android.hardware.biometrics.fingerprint@2.1-service.mata.rc)
        sed -i 's/service fps_hal_mata/service vendor.fps_hal_mata/g' "${2}"
        ;;
    vendor/etc/init/vendor.essential.hardware.sidecar@1.0-service.rc)
        sed -i 's/service sidecar-hal-1-0/service vendor.sidecar-hal-1-0/g' "${2}"
        ;;
    vendor/lib/hw/vulkan.msm8998.so)
        patchelf --set-soname "vulkan.msm8998.so" "${2}"
        ;;
    vendor/lib64/lib-imsrcs-v2.so)
        patchelf --add-needed "libbase_shim.so" "${2}"
        ;;
    vendor/lib64/lib-uceservice.so)
        patchelf --add-needed "libbase_shim.so" "${2}"
        ;;
    vendor/lib64/hw/vulkan.msm8998.so)
        patchelf --set-soname "vulkan.msm8998.so" "${2}"
        ;;
    esac
}

# Initialize the helper
setup_vendor "$DEVICE" "$VENDOR" "$LINEAGE_ROOT" false "$CLEAN_VENDOR"

extract "$MY_DIR"/proprietary-files.txt "$SRC" "$SECTION"
extract "$MY_DIR"/proprietary-files-recovery.txt "$SRC" "$SECTION"

"$MY_DIR"/setup-makefiles.sh
