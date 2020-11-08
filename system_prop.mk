#
# system props for mata
#


# Audio
PRODUCT_SYSTEM_DEFAULT_PROPERTIES += \
    audio.deep_buffer.media=true \
    audio.offload.video=true

# Camera
PRODUCT_SYSTEM_DEFAULT_PROPERTIES += \
    vendor.camera.aux.packagelist="org.codeaurora.snapcam,com.essential.klik,org.lineageos.snap"

# Dalvik
PRODUCT_PROPERTY_OVERRIDES += \
    dalvik.vm.heapgrowthlimit=256m \
    dalvik.vm.heapstartsize=8m \
    dalvik.vm.heapsize=512m \
    dalvik.vm.heaptargetutilization=0.75 \
    dalvik.vm.heapminfree=512k \
    dalvik.vm.heapmaxfree=8m \
    dalvik.vm.dex2oat64.enabled=true \
    ro.sys.fw.dex2oat_thread_count=8

# Logging
PRODUCT_SYSTEM_DEFAULT_PROPERTIES += \
    persist.log.tag.improveTouch=WARNING \
    persist.log.tag.ThermalEngine=WARNING \
    persist.log.tag.WCNSS_FILTER=WARNING

# Recovery
PRODUCT_SYSTEM_DEFAULT_PROPERTIES += \
    ro.recovery.ui.margin_height=136 \
    ro.recovery.ui.margin_width=64

# Voice assistant
PRODUCT_SYSTEM_DEFAULT_PROPERTIES += \
    ro.opa.eligible_device=true

# Wifi Display
PRODUCT_SYSTEM_DEFAULT_PROPERTIES += \
    persist.hwc.enable_vds=1
