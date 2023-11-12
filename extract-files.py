#!/usr/bin/env -S PYTHONPATH=../../../tools/extract-utils python3
#
# SPDX-FileCopyrightText: 2024 The LineageOS Project
# SPDX-License-Identifier: Apache-2.0
#

from extract_utils.fixups_blob import (
    blob_fixup,
    blob_fixups_user_type,
)
from extract_utils.main import (
    ExtractUtils,
    ExtractUtilsModule,
)

namespace_imports = [
    'device/essential/mata',
    'hardware/qcom-caf/msm8998',
    'hardware/qcom-caf/wlan',
    'vendor/qcom/opensource/data-ipa-cfg-mgr-legacy-um',
    'vendor/qcom/opensource/dataservices',
]

blob_fixups: blob_fixups_user_type = {
    ('system_ext/etc/permissions/com.qti.dpmframework.xml',
     'system_ext/etc/permissions/qti_libpermissions.xml'): blob_fixup()
        .regex_replace('name="android.hidl.manager-V1.0-java', 'name="android.hidl.manager@1.0-java'),
    'system_ext/lib64/lib-imscamera.so': blob_fixup()
        .add_needed('libgui_shim.so'),
    'system_ext/lib64/lib-imsvideocodec.so': blob_fixup()
        .add_needed('libgui_shim.so')
        .add_needed('libui_shim.so')
        .replace_needed('libqdMetaData.so', 'libqdMetaData.system.so'),
    'vendor/bin/hbtp_daemon': blob_fixup()
        .add_needed('libhidlbase_shim.so'),
    'vendor/etc/init/android.hardware.biometrics.fingerprint@2.1-service.mata.rc': blob_fixup()
        .regex_replace('service fps_hal_mata', 'service vendor.fps_hal_mata'),
    'vendor/etc/init/vendor.essential.hardware.sidecar@1.0-service.rc': blob_fixup()
        .regex_replace('service sidecar-hal-1-0', 'service vendor.sidecar-hal-1-0'),
    'vendor/etc/izat.conf': blob_fixup()
        .regex_replace('PROCESS_STATE=ENABLED', 'PROCESS_STATE=DISABLED'),
    'vendor/lib/libmmcamera2_stats_lib.so': blob_fixup()
        .sig_replace('58 46 EB F7 1A EE', '00 20 EB F7 1A EE')
        .sig_replace('38 46 D9 F7 0E EC', '00 20 D9 F7 0E EC')
        .sig_replace('20 68 D9 F7 08 EC', '00 20 D9 F7 08 EC'),
    'vendor/lib64/vendor.qti.hardware.improvetouch.touchcompanion@1.0_vendor.so': blob_fixup()
        .add_needed('libhidlbase_shim.so'),
    'vendor/lib*/libtrueportrait.so': blob_fixup()
        .replace_needed('libstdc++.so', 'libstdc++_vendor.so'),
    'vendor/lib*/libwvhidl.so': blob_fixup()
        .add_needed('libcrypto_shim.so'),
    ('recovery/root/vendor/bin/hbtp_daemon',
     'recovery/root/vendor/lib64/vendor.qti.hardware.improvetouch.touchcompanion@1.0_vendor.so'): blob_fixup()
        .remove_needed('libhidltransport.so')
        .remove_needed('libhwbinder.so')
        .replace_needed('libhidlbase.so', 'libhwbinder.so'),
    ('recovery/root/vendor/lib64/libhbtpclient.so',
     'recovery/root/vendor/lib64/vendor.qti.hardware.improvetouch.blobmanager@1.0-service.so',
     'recovery/root/vendor/lib64/vendor.qti.hardware.improvetouch.blobmanager@1.0_vendor.so',
     'recovery/root/vendor/lib64/vendor.qti.hardware.improvetouch.gesturemanager@1.0-service.so',
     'recovery/root/vendor/lib64/vendor.qti.hardware.improvetouch.gesturemanager@1.0_vendor.so',
     'recovery/root/vendor/lib64/vendor.qti.hardware.improvetouch.touchcompanion@1.0-service.so'): blob_fixup()
        .remove_needed('libhidltransport.so')
        .remove_needed('libhwbinder.so'),
}  # fmt: skip

module = ExtractUtilsModule(
    'mata',
    'essential',
    blob_fixups=blob_fixups,
    namespace_imports=namespace_imports,
    add_firmware_proprietary_file=True,
    check_elf=False,
)

if __name__ == '__main__':
    module.add_proprietary_file('proprietary-files-recovery.txt')
    utils = ExtractUtils.device(module)
    utils.run()
