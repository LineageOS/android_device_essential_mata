#!/usr/bin/env -S PYTHONPATH=../../../tools/extract-utils python3
#
# SPDX-FileCopyrightText: 2024 The LineageOS Project
# SPDX-License-Identifier: Apache-2.0
#

import extract_utils.tools
extract_utils.tools.DEFAULT_PATCHELF_VERSION = '0_9'

from extract_utils.fixups_blob import (
    blob_fixup,
    blob_fixups_user_type,
)
from extract_utils.fixups_lib import (
    lib_fixup_remove,
    lib_fixups,
    lib_fixups_user_type,
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

def lib_fixup_vendor_suffix(lib: str, partition: str, *args, **kwargs):
    return f'{lib}_{partition}' if partition == 'vendor' else None

lib_fixups: lib_fixups_user_type = {
    **lib_fixups,
    (
        'com.qualcomm.qti.dpm.api@1.0',
        'libchromaflash',
        'libdualcameraddm',
        'liboptizoom',
        'liboptizoom',
        'libtrueportrait',
        'libubifocus',
        'vendor.qti.imsrtpservice@3.0',
        'vendor.qti.hardware.qccsyshal@1.0',
        'vendor.qti.hardware.qccvndhal@1.0',
    ): lib_fixup_vendor_suffix,
    (
        'libwpa_client',
    ): lib_fixup_remove,
}

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
    'vendor/bin/hw/android.hardware.drm@1.2-service.widevine': blob_fixup()
        .replace_needed('libhidltransport.so', 'libhidlbase.so')
        .remove_needed('libhwbinder.so'),
    ('vendor/bin/hbtp_daemon',
     'vendor/lib/vendor.qti.hardware.improvetouch.blobmanager@1.0_vendor.so',
     'vendor/lib/vendor.qti.hardware.improvetouch.gesturemanager@1.0_vendor.so',
     'vendor/lib/vendor.qti.hardware.improvetouch.touchcompanion@1.0_vendor.so',
     'vendor/lib64/vendor.qti.hardware.improvetouch.blobmanager@1.0_vendor.so',
     'vendor/lib64/vendor.qti.hardware.improvetouch.gesturemanager@1.0_vendor.so',
     'vendor/lib64/vendor.qti.hardware.improvetouch.touchcompanion@1.0_vendor.so'): blob_fixup()
        .replace_needed('libhidlbase.so', 'libhidlbase-v32.so'),
    'vendor/etc/init/android.hardware.biometrics.fingerprint@2.1-service.mata.rc': blob_fixup()
        .regex_replace('service fps_hal_mata', 'service vendor.fps_hal_mata'),
    'vendor/etc/init/vendor.essential.hardware.sidecar@1.0-service.rc': blob_fixup()
        .regex_replace('service sidecar-hal-1-0', 'service vendor.sidecar-hal-1-0'),
    'vendor/etc/izat.conf': blob_fixup()
        .patch_file('gps/0001-gps-izat-Disable-slim_daemon.patch'),
    'vendor/lib/libmmcamera_faceproc.so': blob_fixup()
        .patchelf_version('0_18')
        .clear_symbol_version('__aeabi_memcpy')
        .clear_symbol_version('__aeabi_memset')
        .clear_symbol_version('__gnu_Unwind_Find_exidx'),
    'vendor/lib/libmmcamera2_stats_lib.so': blob_fixup()
        .sig_replace('58 46 EB F7 1A EE', '00 20 EB F7 1A EE')
        .sig_replace('38 46 D9 F7 0E EC', '00 20 D9 F7 0E EC')
        .sig_replace('20 68 D9 F7 08 EC', '00 20 D9 F7 08 EC'),
    ('vendor/lib/libtrueportrait.so', 'vendor/lib64/libtrueportrait.so'): blob_fixup()
        .replace_needed('libstdc++.so', 'libstdc++_vendor.so'),
    'vendor/lib64/libwvhidl.so': blob_fixup()
        .add_needed('libcrypto_shim.so'),
    ('recovery/root/vendor/bin/hbtp_daemon',
     'recovery/root/vendor/lib64/vendor.qti.hardware.improvetouch.touchcompanion@1.0_vendor.so'): blob_fixup()
        .remove_needed('libhidltransport.so')
        .remove_needed('libhwbinder.so')
        .replace_needed('libhidlbase.so', 'libhidlbase-v32.so'),
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
    lib_fixups=lib_fixups,
    namespace_imports=namespace_imports,
    add_firmware_proprietary_file=True,
)

if __name__ == '__main__':
    module.add_proprietary_file('proprietary-files-recovery.txt')
    utils = ExtractUtils.device(module)
    utils.run()
