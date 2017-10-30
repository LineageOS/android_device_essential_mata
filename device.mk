# AAPT
PRODUCT_AAPT_CONFIG := normal
PRODUCT_AAPT_PREF_CONFIG := xxhdpi

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
    device/essential/mata/audio/a2dp_audio_policy_configuration.xml:system/etc/a2dp_audio_policy_configuration.xml \
    device/essential/mata/audio/aanc_tuning_mixer.txt:system/etc/aanc_tuning_mixer.txt \
    device/essential/mata/audio/audio_effects.conf:system/etc/audio_effects.conf \
    device/essential/mata/audio/audio_platform_info.xml:system/etc/audio_platform_info.xml \
    device/essential/mata/audio/audio_policy_configuration.xml:system/etc/audio_policy_configuration.xml \
    device/essential/mata/audio/listen_platform_info.xml:system/etc/listen_platform_info.xml \
    device/essential/mata/audio/mixer_paths_i2s.xml:system/etc/mixer_paths_i2s.xml \
    device/essential/mata/audio/mixer_paths_qvr.xml:system/etc/mixer_paths_qvr.xml \
    device/essential/mata/audio/mixer_paths_tasha.xml:system/etc/mixer_paths_tasha.xml \
    device/essential/mata/audio/mixer_paths_tavil.xml:system/etc/mixer_paths_tavil.xml \
    device/essential/mata/audio/r_submix_audio_policy_configuration.xml:system/etc/r_submix_audio_policy_configuration.xml \
    device/essential/mata/audio/sound_trigger_mixer_paths.xml:system/etc/sound_trigger_mixer_paths.xml \
    device/essential/mata/audio/sound_trigger_mixer_paths_wcd9330.xml:system/etc/sound_trigger_mixer_paths_wcd9330.xml \
    device/essential/mata/audio/sound_trigger_mixer_paths_wcd9340.xml:system/etc/sound_trigger_mixer_paths_wcd9340.xml \
    device/essential/mata/audio/sound_trigger_platform_info.xml:system/etc/sound_trigger_platform_info.xml \
    device/essential/mata/audio/usb_audio_policy_configuration.xml:system/etc/usb_audio_policy_configuration.xml

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
    dalvik.vm.heapsize=36m \
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

# GPS
PRODUCT_PACKAGES += \
    gps.msm8998 \
    gps.conf \
    libgnsspps \
    libvehiclenetwork-native

PRODUCT_COPY_FILES += \
    device/essential/mata/gps/flp.conf:system/vendor/etc/flp.conf \
    device/essential/mata/gps/izat.conf:system/vendor/etc/izat.conf \
    device/essential/mata/gps/lowi.conf:system/vendor/etc/lowi.conf \
    device/essential/mata/gps/sap.conf:system/vendor/etc/sap.conf \
    device/essential/mata/gps/xtwifi.conf:system/vendor/etc/xtwifi.conf

# Init
PRODUCT_COPY_FILES += \
    device/essential/mata/rootdir/bin/init.qti.qseecomd.sh:system/bin/init.qti.qseecomd.sh \
    device/essential/mata/rootdir/etc/hbtp_grip.sh:system/etc/hbtp_grip.sh \
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

PRODUCT_COPY_FILES += \
    device/essential/mata/rootdir/root/dsp/.keep:root/dsp/.keep \
    device/essential/mata/rootdir/root/bt_firmware/.keep:root/bt_firmware/.keep \
    device/essential/mata/rootdir/root/persist/.keep:root/persist/.keep \
    device/essential/mata/rootdir/root/firmware/.keep:root/firmware/.keep

# IRQ
PRODUCT_COPY_FILES += \
    device/essential/mata/configs/msm_irqbalance.conf:system/vendor/etc/msm_irqbalance.conf

# IRSC
PRODUCT_COPY_FILES += \
    device/essential/mata/configs/sec_config:system/etc/sec_config

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

# Touchscreen
PRODUCT_PACKAGES += \
    libtinyxml2

# Update engine
PRODUCT_PACKAGES += brillo_update_payload

# Wifi
PRODUCT_PACKAGES += \
    ipacm \
    ipacm-diag \
    IPACM_cfg.xml \
    libqsap_sdk \
    libQWiFiSoftApCfg \
    libwpa_client \
    hostapd \
    dhcpcd.conf \
    wpa_supplicant \
    wpa_supplicant.conf
