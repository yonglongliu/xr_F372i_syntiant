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

LOCAL_MODULE := sound_trigger.primary.$(TARGET_BOARD_PLATFORM)
LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_CFLAGS := -g -Wall -Werror
ifeq ($(TARGET_BUILD_VARIANT),userdebug)
LOCAL_CFLAGS += -DLOG_NDEBUG=0
LOCAL_CFLAGS += -DDEFAULT_MAX_NUMBER_OF_DUMP_FILES=20
else
LOCAL_CFLAGS += -DDEFAULT_MAX_NUMBER_OF_DUMP_FILES=0
endif
LOCAL_MODULE_TAGS := optional

LOCAL_C_INCLUDES += \
	hardware/libhardware/include/ \
	kernel-4.4/sound/drivers/syntiant/ \
	external/boringssl/src/include/\
    vendor/syntiant/sthal/barge_in \
    vendor/syntiant/ndp10x_hal \
    vendor/syntiant/audio_utils/ \
    vendor/syntiant/spkr_id/ \
    vendor/syntiant/libsynsoundmodel/ \
    vendor/syntiant/syngup/ \
    $(LOCAL_PATH)/../

LOCAL_SRC_FILES := \
sound_trigger_ndp10x.c \
sound_trigger_ndp10x_threads.c \
sound_trigger_ndp10x_ahal_extn.c 

LOCAL_SHARED_LIBRARIES := \
    liblog \
    libcutils \
    libndp10xhal \
    libmeeami_spkrid \
    libsyngup

LOCAL_PROPRIETARY_MODULE := true

include $(BUILD_SHARED_LIBRARY)
include $(call all-makefiles-under,$(LOCAL_PATH))
