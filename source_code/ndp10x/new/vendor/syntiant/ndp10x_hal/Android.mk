# SYNTIANT CONFIDENTIAL
# _____________________
#
#   Copyright (c) 2019 Syntiant Corporation
#   All Rights Reserved.
#
#  NOTICE:  All information contained herein is, and remains the property of
#  Syntiant Corporation and its suppliers, if any.  The intellectual and
#  technical concepts contained herein are proprietary to Syntiant Corporation
#  and its suppliers and may be covered by U.S. and Foreign Patents, patents in
#  process, and are protected by trade secret or copyright law.  Dissemination
#  of this information or reproduction of this material is strictly forbidden
#  unless prior written permission is obtained from Syntiant Corporation.
#

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := libndp10xhal

LOCAL_CFLAGS := -g -Wall -Werror
ifeq ($(TARGET_BUILD_VARIANT),userdebug)
LOCAL_CFLAGS += -DLOG_NDEBUG=0
endif
LOCAL_MODULE_TAGS := optional

LOCAL_C_INCLUDES += \
	hardware/libhardware/include/ \
	kernel-4.4/sound/drivers/syntiant/ \
    $(LOCAL_PATH)/../

LOCAL_SRC_FILES := \
ndp10x_hal.c

LOCAL_SHARED_LIBRARIES := \
    liblog \
    libcutils

LOCAL_PROPRIETARY_MODULE := true

include $(BUILD_SHARED_LIBRARY)


include $(CLEAR_VARS)

LOCAL_SRC_FILES := ndp10x_hal_test.c
LOCAL_CFLAGS := -g -Wall -Werror

LOCAL_C_INCLUDES += \
	hardware/libhardware/include/ \
	kernel-4.4/sound/drivers/syntiant/

LOCAL_SHARED_LIBRARIES := \
        libcutils \
        liblog \
        libndp10xhal

LOCAL_MODULE := ndp10x_hal_test
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_TAGS := optional

include $(BUILD_EXECUTABLE)
