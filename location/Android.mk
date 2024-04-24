LOCAL_PATH := $(call my-dir)

GNSS_SANITIZE_DIAG := cfi bounds null unreachable integer address

include $(call all-makefiles-under,$(LOCAL_PATH))

