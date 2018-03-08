# Copyright (C) 2017 The Android Open Source Project
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

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_OWNER := qcom
LOCAL_MODULE_TAGS := optional

LOCAL_MODULE := android.hardware.power@1.1-service.wahoo
LOCAL_INIT_RC := android.hardware.power@1.1-service.wahoo.rc
LOCAL_SRC_FILES := service.cpp \
    Power.cpp \
    InteractionHandler.cpp \
    power-helper.c \
    metadata-parser.c \
    utils.c \
    list.c \
    hint-data.c \
    powerhintparser.c

LOCAL_C_INCLUDES := external/libxml2/include \
                    external/icu/icu4c/source/common

# Include target-specific files.
LOCAL_SRC_FILES += power-8998.c


# Enable interaction boost all the time
LOCAL_CFLAGS += -DINTERACTION_BOOST -Werror

LOCAL_SHARED_LIBRARIES := \
    libbase \
    liblog \
    libcutils \
    libdl \
    libxml2 \
    libhidlbase \
    libhidltransport \
    libhardware \
    libutils \
    android.hardware.power@1.1 \

include $(BUILD_EXECUTABLE)
