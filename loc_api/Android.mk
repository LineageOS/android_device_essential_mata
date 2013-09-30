ifneq ($(BOARD_VENDOR_QCOM_GPS_LOC_API_HARDWARE),)

LOCAL_PATH := $(call my-dir)

#add QMI libraries for QMI targets
QMI_BOARD_PLATFORM_LIST := msm8960
QMI_BOARD_PLATFORM_LIST += msm8974
QMI_BOARD_PLATFORM_LIST += msm8226
QMI_BOARD_PLATFORM_LIST += msm8610
QMI_BOARD_PLATFORM_LIST += apq8084

ifeq ($(call is-board-platform-in-list,$(QMI_BOARD_PLATFORM_LIST)),true)
include $(call all-subdir-makefiles)
endif #is-board-platform-in-list

endif#BOARD_VENDOR_QCOM_GPS_LOC_API_HARDWARE
