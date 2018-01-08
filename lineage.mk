# Inherit some common Lineage stuff.
$(call inherit-product, $(SRC_TARGET_DIR)/product/core_64_bit.mk)
$(call inherit-product, vendor/lineage/config/common_full_phone.mk)

$(call inherit-product, $(SRC_TARGET_DIR)/product/full_base_telephony.mk)
$(call inherit-product, $(SRC_TARGET_DIR)/product/languages_full.mk)

# Vendor blobs
$(call inherit-product-if-exists, vendor/essential/mata/mata-vendor.mk)

# Device
$(call inherit-product, device/essential/mata/device.mk)

GAPPS_VARIANT := stock
GAPPS_EXCLUDED_PACKAGES := EditorsDocs EditorsSheets EditorsSlides GoogleCamera PrebuiltNewsWeathr
GAPPS_FORCE_PACKAGE_OVERRIDES := true
GAPPS_FORCE_PIXEL_LAUNCHER := true
$(call inherit-product,vendor/opengapps/build/opengapps-packages.mk)

# TWRP
ifeq ($(WITH_TWRP),true)
$(call inherit-product, device/essential/mata/twrp/twrp.mk)
else
TARGET_RECOVERY_FSTAB := device/essential/mata/rootdir/etc/fstab.mata
endif

# Device identifiers
PRODUCT_DEVICE := mata
PRODUCT_NAME := lineage_mata
PRODUCT_BRAND := essential
PRODUCT_MODEL := PH-1
PRODUCT_MANUFACTURER := Essential Products
PRODUCT_RELEASE_NAME := mata

PRODUCT_BUILD_PROP_OVERRIDES += \
        PRODUCT_NAME=mata \
        BUILD_FINGERPRINT=essential/mata/mata:8.0.0/OPM1.170911.254/255:user/release-keys \
        PRIVATE_BUILD_DESC="mata-user 8.0.0 OPM1.170911.254 255 release-keys"
