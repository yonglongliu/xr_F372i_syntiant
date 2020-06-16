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

LOCAL_MODULE := sthal_fwk_test
LOCAL_CFLAGS := -g -Wall
LOCAL_MODULE_TAGS := optional

LOCAL_C_INCLUDES += \
kernel/sound/drivers/syntiant/\
external/boringssl/src/include/\
vendor/syntiant/sthal/ \
vendor/syntiant/audio_utils/ \
$(call include-path-for, audio-utils) \
vendor/syntiant/libsynsoundmodel/

LOCAL_SRC_FILES := \
SyntiantSoundTriggerTest.cpp \
SyntiantSoundTriggerTestSession.cpp \
SyntiantUserModelEnroller.cpp  \

LOCAL_SHARED_LIBRARIES := libsoundtrigger \
                          libbinder \
                          libcutils \
                          libutils \
                          libstagefright \
                          libmedia \
                          liblog \
                          libaudioutils \
                          libsynaudioutils \
			              libsynsoundmodel 

LOCAL_STATIC_LIBRARIES := libsndfile

LOCAL_PROPRIETARY_MODULE := true

include $(BUILD_EXECUTABLE)
