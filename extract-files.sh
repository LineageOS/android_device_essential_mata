#!/bin/bash
#
# Copyright (C) 2016 The CyanogenMod Project
# Copyright (C) 2017-2023 The LineageOS Project
#
# SPDX-License-Identifier: Apache-2.0
#

set -e

DEVICE=mata
VENDOR=essential

# Load extract_utils and do some sanity checks
MY_DIR="${BASH_SOURCE%/*}"
if [[ ! -d "${MY_DIR}" ]]; then MY_DIR="${PWD}"; fi

ANDROID_ROOT="${MY_DIR}/../../.."

HELPER="${ANDROID_ROOT}/tools/extract-utils/extract_utils.sh"
if [ ! -f "${HELPER}" ]; then
    echo "Unable to find helper script at ${HELPER}"
    exit 1
fi
source "${HELPER}"

# Default to sanitizing the vendor folder before extraction
CLEAN_VENDOR=true

ONLY_FIRMWARE=
KANG=
SECTION=

while [ "${#}" -gt 0 ]; do
    case "${1}" in
        --only-firmware )
                ONLY_FIRMWARE=true
                ;;
        -n | --no-cleanup )
                CLEAN_VENDOR=false
                ;;
        -k | --kang )
                KANG="--kang"
                ;;
        -s | --section )
                SECTION="${2}"; shift
                CLEAN_VENDOR=false
                ;;
        * )
                SRC="${1}"
                ;;
    esac
    shift
done

if [ -z "${SRC}" ]; then
    SRC="adb"
fi

function blob_fixup() {
    case "${1}" in
        system_ext/etc/permissions/com.qti.dpmframework.xml|system_ext/etc/permissions/qti_libpermissions.xml)
            sed -i "s/name=\"android.hidl.manager-V1.0-java/name=\"android.hidl.manager@1.0-java/g" "${2}"
            ;;
        system_ext/lib64/lib-imscamera.so)
            grep -q "libgui_shim.so" "${2}" || "${PATCHELF}" --add-needed "libgui_shim.so" "${2}"
            ;;
        system_ext/lib64/lib-imsvideocodec.so)
            grep -q "libgui_shim.so" "${2}" || "${PATCHELF}" --add-needed "libgui_shim.so" "${2}"
            grep -q "libui_shim.so" "${2}" || "${PATCHELF}" --add-needed "libui_shim.so" "${2}"
            "${PATCHELF}" --replace-needed "libqdMetaData.so" "libqdMetaData.system.so" "${2}"
            ;;
        vendor/bin/hbtp_daemon)
            "${PATCHELF}" --replace-needed "libhidlbase.so" "libhidlbase-v32.so" "${2}"
            ;;
        vendor/etc/init/android.hardware.biometrics.fingerprint@2.1-service.mata.rc)
            sed -i 's/service fps_hal_mata/service vendor.fps_hal_mata/g' "${2}"
            ;;
        vendor/etc/init/vendor.essential.hardware.sidecar@1.0-service.rc)
            sed -i 's/service sidecar-hal-1-0/service vendor.sidecar-hal-1-0/g' "${2}"
            ;;
        vendor/etc/izat.conf)
            sed -i "216s/PROCESS_STATE=ENABLED/PROCESS_STATE=DISABLED/g" izat.conf
            ;;
        vendor/lib/libmmcamera2_stats_lib.so)
            sed -i "s/\x58\x46\xeb\xf7\x1a\xee/\x00\x20\xeb\xf7\x1a\xee/" "${2}"
            sed -i "s/\x38\x46\xd9\xf7\x0e\xec/\x00\x20\xd9\xf7\x0e\xec/" "${2}"
            sed -i "s/\x20\x68\xd9\xf7\x08\xec/\x00\x20\xd9\xf7\x08\xec/" "${2}"
            ;;
        vendor/lib*/libtrueportrait.so)
            "${PATCHELF}" --replace-needed "libstdc++.so" "libstdc++_vendor.so" "${2}"
            ;;
        recovery/root/vendor/bin/hbtp_daemon)
            "${PATCHELF}" --remove-needed libhidltransport.so --remove-needed libhwbinder.so "${2}"
            "${PATCHELF}" --replace-needed "libhidlbase.so" "libhidlbase-v32.so" "${2}"
            ;;
        recovery/root/vendor/lib64/libhbtpclient.so|\
        recovery/root/vendor/lib64/vendor.qti.hardware.improvetouch.blobmanager@1.0-service.so|\
        recovery/root/vendor/lib64/vendor.qti.hardware.improvetouch.blobmanager@1.0_vendor.so|\
        recovery/root/vendor/lib64/vendor.qti.hardware.improvetouch.gesturemanager@1.0-service.so|\
        recovery/root/vendor/lib64/vendor.qti.hardware.improvetouch.gesturemanager@1.0_vendor.so|\
        recovery/root/vendor/lib64/vendor.qti.hardware.improvetouch.touchcompanion@1.0-service.so|\
        recovery/root/vendor/lib64/vendor.qti.hardware.improvetouch.touchcompanion@1.0_vendor.so)
            "${PATCHELF}" --remove-needed libhidltransport.so --remove-needed libhwbinder.so "${2}"
            ;;
    esac
}

# Initialize the helper
setup_vendor "${DEVICE}" "${VENDOR}" "${ANDROID_ROOT}" false "${CLEAN_VENDOR}"

if [ -z "${ONLY_FIRMWARE}" ]; then
  extract "${MY_DIR}/proprietary-files.txt" "${SRC}" "${KANG}" --section "${SECTION}"
  extract "${MY_DIR}/proprietary-files-recovery.txt" "${SRC}" "${KANG}" --section "${SECTION}"
fi

if [ -z "${SECTION}" ]; then
    extract_firmware "${MY_DIR}/proprietary-firmware.txt" "${SRC}"
fi

"${MY_DIR}/setup-makefiles.sh"
