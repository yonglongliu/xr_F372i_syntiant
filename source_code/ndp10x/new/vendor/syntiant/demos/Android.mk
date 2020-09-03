LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := ndp10x_driver_test
LOCAL_MODULE_PATH := $(TARGET_OUT_VENDOR_EXECUTABLES)

LOCAL_SRC_FILES := main.c \
	           ndp10x_app_utils.c \
                   ndp10x_play.c \
                   ndp10x_record.c

LOCAL_CFLAGS := -Wall -fpic

LOCAL_C_INCLUDES := kernel-4.4/sound/drivers/syntiant

include $(BUILD_EXECUTABLE)


