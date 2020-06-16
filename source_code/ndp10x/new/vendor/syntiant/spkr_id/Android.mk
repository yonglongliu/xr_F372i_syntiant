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
LOCAL_MODULE := libmeeami_spkrid
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_SUFFIX := .so
LOCAL_SRC_FILES := libmeeami_spkrid.so
LOCAL_PROPRIETARY_MODULE := true
include $(BUILD_PREBUILT)


include $(CLEAR_VARS)

LOCAL_SRC_FILES := tst_spkrid_app.c
LOCAL_CFLAGS := -g -Wall -Werror

LOCAL_C_INCLUDES += \
	hardware/libhardware/include/ \
	vendor/syntiant/spkr_id/

LOCAL_SHARED_LIBRARIES := \
        libcutils \
        liblog \
        libmeeami_spkrid

LOCAL_MODULE := syntiant_spkr_id_test
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_TAGS := optional

include $(BUILD_EXECUTABLE)
