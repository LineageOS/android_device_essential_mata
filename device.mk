GAPPS_VARIANT := nano
$(call inherit-product-if-exists, vendor/opengapps/build/opengapps-packages.mk)

# AAPT
PRODUCT_AAPT_CONFIG := normal
PRODUCT_AAPT_PREF_CONFIG := 560dpi
PRODUCT_AAPT_PREBUILT_DPI := xxxhdpi xxhdpi xhdpi hdpi

# Audio
PRODUCT_PACKAGES += \
    audiod \
    audio.a2dp.default \
    audio.primary.msm8998 \
    audio.r_submix.default \
    audio.usb.default \
    libaudio-resampler \
    libqcompostprocbundle \
    libqcomvisualizer \
    libqcomvoiceprocessing \
    libvolumelistener \
    tinymix

PRODUCT_COPY_FILES += \
    device/essential/mata/audio/aanc_tuning_mixer.txt:system/etc/aanc_tuning_mixer.txt \
    device/essential/mata/audio/audio_platform_info.xml:system/etc/audio_platform_info.xml \
    device/essential/mata/audio/audio_platform_info_i2s.xml:system/etc/audio_platform_info_i2s.xml \
    device/essential/mata/audio/audio_policy_configuration.xml:system/etc/audio_policy_configuration.xml \
    device/essential/mata/audio/listen_platform_info.xml:system/etc/listen_platform_info.xml \
    device/essential/mata/audio/mixer_paths.xml:system/etc/mixer_paths.xml \
    device/essential/mata/audio/mixer_paths_dtp.xml:system/etc/mixer_paths_dtp.xml \
    device/essential/mata/audio/mixer_paths_i2s.xml:system/etc/mixer_paths_i2s.xml \
    device/essential/mata/audio/mixer_paths_qvr.xml:system/etc/mixer_paths_qvr.xml \
    device/essential/mata/audio/mixer_paths_skuk.xml:system/etc/mixer_paths_skuk.xml \
    device/essential/mata/audio/mixer_paths_tasha.xml:system/etc/mixer_paths_tasha.xml \
    device/essential/mata/audio/mixer_paths_tasha_mic.xml:system/etc/mixer_paths_tasha_mic.xml \
    device/essential/mata/audio/mixer_paths_tavil.xml:system/etc/mixer_paths_tavil.xml \
    device/essential/mata/audio/sound_trigger_mixer_paths.xml:system/etc/sound_trigger_mixer_paths.xml \
    device/essential/mata/audio/sound_trigger_mixer_paths_wcd9330.xml:system/etc/sound_trigger_mixer_paths_wcd9330.xml \
    device/essential/mata/audio/sound_trigger_mixer_paths_wcd9340.xml:system/etc/sound_trigger_mixer_paths_wcd9340.xml \
    device/essential/mata/audio/sound_trigger_platform_info.xml:system/etc/sound_trigger_platform_info.xml

PRODUCT_COPY_FILES += \
    frameworks/av/services/audiopolicy/config/a2dp_audio_policy_configuration.xml:/system/etc/a2dp_audio_policy_configuration.xml \
    frameworks/av/services/audiopolicy/config/audio_policy_volumes.xml:/system/etc/audio_policy_volumes.xml \
    frameworks/av/services/audiopolicy/config/default_volume_tables.xml:/system/etc/default_volume_tables.xml \
    frameworks/av/services/audiopolicy/config/r_submix_audio_policy_configuration.xml:/system/etc/r_submix_audio_policy_configuration.xml \
    frameworks/av/services/audiopolicy/config/usb_audio_policy_configuration.xml:/system/etc/usb_audio_policy_configuration.xml

# Boot animation
TARGET_SCREEN_HEIGHT := 2560
TARGET_SCREEN_WIDTH := 1312

# Camera
PRODUCT_COPY_FILES += \
    device/essential/mata/configs/camera/camera_config.xml:system/etc/camera/camera_config.xml \
    device/essential/mata/configs/camera/imx258_chromatix.xml:system/etc/camera/imx258_chromatix.xml \
    device/essential/mata/configs/camera/imx268_chromatix.xml:system/etc/camera/imx268_chromatix.xml \
    device/essential/mata/configs/camera/imx258_mono_chromatix.xml:system/etc/camera/imx258_mono_chromatix.xml

PRODUCT_PACKAGES += \
    libxml2 \
    Snap

# Dalvik/HWUI
PRODUCT_PROPERTY_OVERRIDES += \
    ro.hwui.texture_cache_size=72 \
    ro.hwui.layer_cache_size=48 \
    ro.hwui.r_buffer_cache_size=8 \
    ro.hwui.path_cache_size=32 \
    ro.hwui.gradient_cache_size=1 \
    ro.hwui.drop_shadow_cache_size=6 \
    ro.hwui.texture_cache_flushrate=0.4 \
    ro.hwui.text_small_cache_width=1024 \
    ro.hwui.text_small_cache_height=1024 \
    ro.hwui.text_large_cache_width=2048 \
    ro.hwui.text_large_cache_height=1024

PRODUCT_PROPERTY_OVERRIDES += \
    dalvik.vm.heapgrowthlimit=256m \
    dalvik.vm.heapstartsize=8m \
    dalvik.vm.heapsize=512m \
    dalvik.vm.heaptargetutilization=0.75 \
    dalvik.vm.heapminfree=512k \
    dalvik.vm.heapmaxfree=8m

# Display
PRODUCT_PACKAGES += \
    copybit.msm8998 \
    gralloc.msm8998 \
    hwcomposer.msm8998 \
    memtrack.msm8998 \
    liboverlay \
    libtinyxml

# Fingerprint
PRODUCT_PACKAGES += \
    fingerprintd

PRODUCT_COPY_FILES += \
    device/essential/mata/keylayout/uinput-fpc.kl:system/usr/keylayout/uinput-fpc.kl

# GPS
PRODUCT_PACKAGES += \
    gps.msm8998 \
    libgnsspps \
    libvehiclenetwork-native

PRODUCT_COPY_FILES += \
    device/essential/mata/configs/gps/flp.conf:system/vendor/etc/flp.conf \
    device/essential/mata/configs/gps/izat.conf:system/vendor/etc/izat.conf \
    device/essential/mata/configs/gps/lowi.conf:system/vendor/etc/lowi.conf \
    device/essential/mata/configs/gps/sap.conf:system/vendor/etc/sap.conf \
    device/essential/mata/configs/gps/xtwifi.conf:system/vendor/etc/xtwifi.conf

# Init
PRODUCT_COPY_FILES += \
    device/essential/mata/rootdir/bin/init.qti.qseecomd.sh:system/bin/init.qti.qseecomd.sh \
    device/essential/mata/rootdir/etc/init.qcom.post_boot.sh:system/etc/init.qcom.post_boot.sh \
    device/essential/mata/rootdir/root/fstab.mata:root/fstab.mata \
    device/essential/mata/rootdir/root/init.class_main.sh:root/init.class_main.sh \
    device/essential/mata/rootdir/root/init.mata.alt.rc:root/init.mata.alt.rc \
    device/essential/mata/rootdir/root/init.mata.diag.rc:root/init.mata.diag.rc \
    device/essential/mata/rootdir/root/init.mata.fingerprint.rc:root/init.mata.fingerprint.rc \
    device/essential/mata/rootdir/root/init.mata.lcm.rc:root/init.mata.lcm.rc \
    device/essential/mata/rootdir/root/init.mata.power_off_charging.rc:root/init.mata.power_off_charging.rc \
    device/essential/mata/rootdir/root/init.mata.ramdump.rc:root/init.mata.ramdump.rc \
    device/essential/mata/rootdir/root/init.mata.rc:root/init.mata.rc \
    device/essential/mata/rootdir/root/init.mata.sensor.rc:root/init.mata.sensor.rc \
    device/essential/mata/rootdir/root/init.mata.sensors.sh:root/init.mata.sensors.sh \
    device/essential/mata/rootdir/root/init.mata.target.rc:root/init.mata.target.rc \
    device/essential/mata/rootdir/root/init.mata.ufs.rc:root/init.mata.ufs.rc \
    device/essential/mata/rootdir/root/init.mata.usb.rc:root/init.mata.usb.rc \
    device/essential/mata/rootdir/root/init.qcom.early_boot.sh:root/init.qcom.early_boot.sh \
    device/essential/mata/rootdir/root/init.qcom.sh:root/init.qcom.sh \
    device/essential/mata/rootdir/root/ueventd.mata.rc:root/ueventd.mata.rc

# IPv6 tethering
PRODUCT_PACKAGES += \
    ebtables \
    ethertypes

# IRQ
PRODUCT_COPY_FILES += \
    device/essential/mata/configs/msm_irqbalance.conf:system/vendor/etc/msm_irqbalance.conf

# IRSC
PRODUCT_COPY_FILES += \
    device/essential/mata/configs/sec_config:system/etc/sec_config

# Led packages
PRODUCT_PACKAGES += \
    lights.msm8998

# Media
PRODUCT_COPY_FILES += \
    device/essential/mata/media/media_profiles.xml:system/etc/media_profiles.xml \
    device/essential/mata/media/media_codecs.xml:system/etc/media_codecs.xml \
    device/essential/mata/media/media_codecs_performance.xml:system/etc/media_codecs_performance.xml \
    frameworks/av/media/libstagefright/data/media_codecs_google_audio.xml:system/etc/media_codecs_google_audio.xml \
    frameworks/av/media/libstagefright/data/media_codecs_google_telephony.xml:system/etc/media_codecs_google_telephony.xml \
    frameworks/av/media/libstagefright/data/media_codecs_google_video.xml:system/etc/media_codecs_google_video.xml

# NFC
PRODUCT_PACKAGES += \
    com.android.nfc_extras \
    com.nxp.nfc.nq \
    nfc_nci.nqx.default \
    NQNfcNci \
    nqnfcee_access.xml \
    nqnfcse_access.xml \
    Tag

PRODUCT_COPY_FILES += \
    device/essential/mata/configs/libnfc-brcm.conf:system/etc/libnfc-brcm.conf \
    device/essential/mata/configs/libnfc-nxp.conf:system/etc/libnfc-nxp.conf

# Permissions
PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/android.hardware.bluetooth.xml:system/etc/permissions/android.hardware.bluetooth.xml \
    frameworks/native/data/etc/android.hardware.bluetooth_le.xml:system/etc/permissions/android.hardware.bluetooth_le.xml \
    frameworks/native/data/etc/android.hardware.camera.flash-autofocus.xml:system/etc/permissions/android.hardware.camera.flash-autofocus.xml \
    frameworks/native/data/etc/android.hardware.camera.front.xml:system/etc/permissions/android.hardware.camera.front.xml \
    frameworks/native/data/etc/android.hardware.camera.full.xml:system/etc/permissions/android.hardware.camera.full.xml \
    frameworks/native/data/etc/android.hardware.camera.raw.xml:system/etc/permissions/android.hardware.camera.raw.xml \
    frameworks/native/data/etc/android.hardware.fingerprint.xml:system/etc/permissions/android.hardware.fingerprint.xml \
    frameworks/native/data/etc/android.hardware.location.gps.xml:system/etc/permissions/android.hardware.location.gps.xml \
    frameworks/native/data/etc/android.hardware.nfc.hce.xml:system/etc/permissions/android.hardware.nfc.hce.xml \
    frameworks/native/data/etc/android.hardware.nfc.xml:system/etc/permissions/android.hardware.nfc.xml \
    frameworks/native/data/etc/android.hardware.opengles.aep.xml:system/etc/permissions/android.hardware.opengles.aep.xml \
    frameworks/native/data/etc/android.hardware.sensor.accelerometer.xml:system/etc/permissions/android.hardware.sensor.accelerometer.xml \
    frameworks/native/data/etc/android.hardware.sensor.ambient_temperature.xml:system/etc/permissions/android.hardware.sensor.ambient_temperature.xml \
    frameworks/native/data/etc/android.hardware.sensor.barometer.xml:system/etc/permissions/android.hardware.sensor.barometer.xml \
    frameworks/native/data/etc/android.hardware.sensor.compass.xml:system/etc/permissions/android.hardware.sensor.compass.xml \
    frameworks/native/data/etc/android.hardware.sensor.gyroscope.xml:system/etc/permissions/android.hardware.sensor.gyroscope.xml \
    frameworks/native/data/etc/android.hardware.sensor.hifi_sensors.xml:system/etc/permissions/android.hardware.sensor.hifi_sensors.xml \
    frameworks/native/data/etc/android.hardware.sensor.light.xml:system/etc/permissions/android.hardware.sensor.light.xml \
    frameworks/native/data/etc/android.hardware.sensor.proximity.xml:system/etc/permissions/android.hardware.sensor.proximity.xml \
    frameworks/native/data/etc/android.hardware.sensor.relative_humidity.xml:system/etc/permissions/android.hardware.sensor.relative_humidity.xml \
    frameworks/native/data/etc/android.hardware.sensor.stepcounter.xml:system/etc/permissions/android.hardware.sensor.stepcounter.xml \
    frameworks/native/data/etc/android.hardware.sensor.stepdetector.xml:system/etc/permissions/android.hardware.sensor.stepdetector.xml \
    frameworks/native/data/etc/android.hardware.telephony.cdma.xml:system/etc/permissions/android.hardware.telephony.cdma.xml \
    frameworks/native/data/etc/android.hardware.telephony.gsm.xml:system/etc/permissions/android.hardware.telephony.gsm.xml \
    frameworks/native/data/etc/android.hardware.touchscreen.multitouch.jazzhand.xml:system/etc/permissions/android.hardware.touchscreen.multitouch.jazzhand.xml \
    frameworks/native/data/etc/android.hardware.usb.accessory.xml:system/etc/permissions/android.hardware.usb.accessory.xml \
    frameworks/native/data/etc/android.hardware.usb.host.xml:system/etc/permissions/android.hardware.usb.host.xml \
    frameworks/native/data/etc/android.hardware.vulkan.level-0.xml:system/etc/permissions/android.hardware.vulkan.level-0.xml \
    frameworks/native/data/etc/android.hardware.vulkan.version-1_0_3.xml:system/etc/permissions/android.hardware.vulkan.version-1_0_3.xml \
    frameworks/native/data/etc/android.hardware.wifi.direct.xml:system/etc/permissions/android.hardware.wifi.direct.xml \
    frameworks/native/data/etc/android.hardware.wifi.xml:system/etc/permissions/android.hardware.wifi.xml \
    frameworks/native/data/etc/android.software.midi.xml:system/etc/permissions/android.software.midi.xml \
    frameworks/native/data/etc/com.android.nfc_extras.xml:system/etc/permissions/com.android.nfc_extras.xml \
    frameworks/native/data/etc/com.nxp.mifare.xml:system/etc/permissions/com.nxp.mifare.xml \
    frameworks/native/data/etc/handheld_core_hardware.xml:system/etc/permissions/handheld_core_hardware.xml

# QMI
PRODUCT_PACKAGES += \
    libjson

# OMX
PRODUCT_PACKAGES += \
    libc2dcolorconvert \
    libextmedia_jni \
    libOmxAacEnc \
    libOmxAmrEnc \
    libOmxCore \
    libOmxEvrcEnc \
    libOmxQcelp13Enc \
    libOmxVdec \
    libOmxVenc \
    libstagefrighthw

# Permissions
PRODUCT_PACKAGES += \
    fs_config_files

# Power
PRODUCT_PACKAGES += \
    power.msm8998

# Radio
PRODUCT_PACKAGES += \
    librmnetctl \
    libprotobuf-cpp-full

# Sensors
PRODUCT_COPY_FILES += \
    device/essential/mata/configs/sensor_def_qcomdev.conf:system/etc/sensors/sensor_def_qcomdev.conf \
    device/essential/mata/configs/hals.conf:system/etc/sensors/hals.conf

PRODUCT_PACKAGES += \
    sensors.msm8998

# Tethering
PRODUCT_PROPERTY_OVERRIDES += \
    net.tethering.noprovisioning=true

# Touchscreen
PRODUCT_PACKAGES += \
    libtinyxml2

# Update engine
PRODUCT_PACKAGES += brillo_update_payload

# Verity
PRODUCT_SYSTEM_VERITY_PARTITION := /dev/block/bootdevice/by-name/system
$(call inherit-product, build/target/product/verity.mk)

# Wifi
PRODUCT_COPY_FILES += \
    device/essential/mata/wifi/hostapd_default.conf:system/etc/hostapd/hostapd_default.conf \
    device/essential/mata/wifi/hostapd.accept:system/etc/hostapd/hostapd.accept \
    device/essential/mata/wifi/hostapd.deny:system/etc/hostapd/hostapd.deny \
    device/essential/mata/wifi/wifi_concurrency_cfg.txt:system/etc/wifi/wifi_concurrency_cfg.txt \
    device/essential/mata/wifi/p2p_supplicant_overlay.conf:system/etc/wifi/p2p_supplicant_overlay.conf \
    device/essential/mata/wifi/wpa_supplicant_overlay.conf:system/etc/wifi/wpa_supplicant_overlay.conf \
    device/essential/mata/wifi/WCNSS_qcom_cfg.ini:system/etc/wifi/WCNSS_qcom_cfg.ini

PRODUCT_PACKAGES += \
    ipacm \
    IPACM_cfg.xml \
    libqsap_sdk \
    libQWiFiSoftApCfg \
    libwpa_client \
    hostapd \
    dhcpcd.conf \
    wpa_supplicant \
    wpa_supplicant.conf
