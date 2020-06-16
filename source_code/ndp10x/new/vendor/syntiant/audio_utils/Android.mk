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
LOCAL_MODULE := libsynaudioutils

LOCAL_CFLAGS := -g -Wall -Werror
LOCAL_MODULE_TAGS := optional

LOCAL_C_INCLUDES += \
    $(call include-path-for, audio-utils) \
    $(LOCAL_PATH)/../


LOCAL_SRC_FILES := \
SyntiantAudioSession.cpp

LOCAL_SHARED_LIBRARIES := \
    liblog \
    libcutils \
    libsoundtrigger \
    libbinder \
    libcutils \
    libutils \
    libstagefright \
    libmedia \
    libaudioutils \
    liblog

LOCAL_PROPRIETARY_MODULE := true

include $(BUILD_SHARED_LIBRARY)



include $(CLEAR_VARS)
LOCAL_MODULE := syntiant_audio_recorder
LOCAL_CFLAGS := -g -Wall -Werror
LOCAL_C_INCLUDES += \
vendor/syntiant/audio_utils \
$(call include-path-for, audio-utils)

LOCAL_SRC_FILES := \
SyntiantAudioRecorder.cpp

LOCAL_SHARED_LIBRARIES := \
    libsoundtrigger \
    libbinder \
    libcutils \
    libutils \
    libstagefright \
    libmedia \
    libaudioutils \
    liblog \
    libsynaudioutils

LOCAL_STATIC_LIBRARIES := \
	libsndfile

LOCAL_CFLAGS += -Wall
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_PATH := $(TARGET_OUT_VENDOR_EXECUTABLES)
LOCAL_MODULE_TAGS := optional
include $(BUILD_EXECUTABLE)


include $(CLEAR_VARS)
LOCAL_MODULE := syntiant_audio_player
LOCAL_CFLAGS := -g -Wall -Werror
LOCAL_C_INCLUDES += \
vendor/syntiant/audio_utils \
$(call include-path-for, audio-utils)

LOCAL_SRC_FILES := \
SyntiantAudioPlayer.cpp

LOCAL_SHARED_LIBRARIES := \
    libsoundtrigger \
    libbinder \
    libcutils \
    libutils \
    libstagefright \
    libmedia \
    libaudioutils \
    liblog \
    libsynaudioutils

LOCAL_STATIC_LIBRARIES := \
	libsndfile

LOCAL_CFLAGS += -Wall
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_PATH := $(TARGET_OUT_VENDOR_EXECUTABLES)
LOCAL_MODULE_TAGS := optional
include $(BUILD_EXECUTABLE)
