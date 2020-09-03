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
LOCAL_MODULE := libsyngup

LOCAL_CFLAGS := -g -Wall
ifeq ($(TARGET_BUILD_VARIANT),userdebug)
LOCAL_CFLAGS += -DLOG_NDEBUG=0
endif
LOCAL_MODULE_TAGS := optional

LOCAL_C_INCLUDES += \
	kernel-4.4/sound/drivers/syntiant/\
	external/boringssl/src/include/\
	$(LOCAL_PATH)/../

LOCAL_SRC_FILES := syngup.c

LOCAL_SHARED_LIBRARIES := \
    liblog \
    libz \
    libcrypto

LOCAL_PROPRIETARY_MODULE := true
include $(BUILD_SHARED_LIBRARY)
