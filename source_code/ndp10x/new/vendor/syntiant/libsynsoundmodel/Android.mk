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
LOCAL_MODULE := libsynsoundmodel

LOCAL_CFLAGS := -g -Wall -Werror
LOCAL_MODULE_TAGS := optional

LOCAL_C_INCLUDES += \
	hardware/libhardware/include/ \
	kernel-4.4/sound/drivers/syntiant/ \
	vendor/syntiant/spkr_id/ \
	external/boringssl/src/include/ \
	external/e2fsprogs/lib/ \
	vendor/syntiant/syngup/ \
    $(LOCAL_PATH)/../

LOCAL_SRC_FILES := syntiant_soundmodel.c

LOCAL_SHARED_LIBRARIES := \
    liblog \
    libcutils \
    libmeeami_spkrid \
    libsyngup \
    libext2_uuid

LOCAL_PROPRIETARY_MODULE := true
include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := soundmodel_test

LOCAL_CFLAGS := -g -Wall -Werror
LOCAL_MODULE_TAGS := optional

LOCAL_SRC_FILES := soundmodel_test.c

LOCAL_SHARED_LIBRARIES := \
    libsynsoundmodel

LOCAL_PROPRIETARY_MODULE := true
include $(BUILD_EXECUTABLE)