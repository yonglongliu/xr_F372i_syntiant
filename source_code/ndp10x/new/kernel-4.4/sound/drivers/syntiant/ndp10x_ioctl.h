/*
 * SYNTIANT CONFIDENTIAL
 * _____________________
 *
 *   Copyright (c) 2019-2020 Syntiant Corporation
 *   All Rights Reserved.
 *
 *  NOTICE:  All information contained herein is, and remains the property of
 *  Syntiant Corporation and its suppliers, if any.  The intellectual and
 *  technical concepts contained herein are proprietary to Syntiant Corporation
 *  and its suppliers and may be covered by U.S. and Foreign Patents, patents
 *  in process, and are protected by trade secret or copyright law.
 *  Dissemination of this information or reproduction of this material is
 *  strictly forbidden unless prior written permission is obtained from
 *  Syntiant Corporation.
 */
#ifndef NDP10X_IOCTL_H
#define NDP10X_IOCTL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <linux/ioctl.h>
#include <linux/types.h>
/* parallel to struct syntiant_ndp_config_s */
/* lengths are in/out parameters */
struct ndp10x_ndp_config_s {
    __u64 device_type;            /**< device type identifier string */
    __u64 firmware_version;       /**< firmware version string */
    __u64 parameters_version;     /**< parameters version string */
    __u64 labels;                 /**< class labels strings array.  Each
                                       label is a C (NUL-terminated) string
                                       followed immediately by the next label.
                                       The label strings will be 0 length if
                                       label strings are not available */
    __u64 pkg_version;            /**< package version string */
    unsigned int device_type_len;
    unsigned int classes;         /**< number of active classes */
    unsigned int firmware_version_len;
    /**< length of supplied firmware version string */
    unsigned int parameters_version_len;
    /**< length of supplied parameters version string */
    unsigned int labels_len;      /**< length of supplied labels string array */
    unsigned int pkg_version_len; /**< length of supplied package version str */
};

struct ndp10x_load_s {
    __u64 package;
    unsigned int length;
    int error_code;
};


struct ndp_timespec_s {
    uint64_t tv_sec;        /* seconds */
    uint64_t tv_nsec;       /* nanoseconds */
};

struct ndp10x_watch_s {
    uint64_t classes;    /**< bit map of classes on which to report a match */
    int timeout;         /**< seconds -- < 0 -> wait indefinitely */
    int flush;           /**< discard outstanding watch events */
    int match;           /**< set if a match is detected */
    int class_index;     /**< match index if a match is detected */
    unsigned int info;   /**< additional information if a match is detected */
    int extract_match_mode;    /**< when watching in this mode, driver stops
                                    collecting audio in the background */
    int extract_before_match;  /**< ms of audio to extract before match. 
                                    Will return amount of audio available for extraction */
    struct ndp_timespec_s ts;
};

struct ndp10x_transfer_s {
    __u64 out;
    __u64 in;
    int mcu;
    uint32_t addr;
    unsigned int count;
};

struct ndp10x_driver_config_s {
    unsigned int spi_speed;
    unsigned int spi_padding_bytes;
    int spi_split_flag;
    uint32_t spi_send_speed;
    unsigned int extract_ring_size;   /* MS, default 10000 */
    unsigned int send_ring_size;      /* MS, default 100 */
    unsigned int result_per_frame;
};

struct ndp10x_pcm_extract_s {
    __u64 buffer;
    unsigned int buffer_length;
    int nonblock;
    int flush;
    unsigned int extracted_length;
    unsigned int remaining_length;
};

struct ndp10x_pcm_send_s {
    __u64 buffer;
    unsigned int buffer_length;
    int nonblock;
    unsigned int sent_length;
};

struct ndp10x_statistics_s {
    uint64_t isrs;
    uint64_t polls;
    uint64_t frames;
    uint64_t results;
    uint64_t results_dropped;
    uint64_t extracts;
    uint64_t extract_bytes;
    uint64_t extract_bytes_dropped;
    uint64_t sends;
    uint64_t send_bytes;
    unsigned int send_ring_used;
    unsigned int extract_ring_used;
    unsigned int result_ring_used;
    int clear;
};

#define RESULT_MAX_CLASSES 64
#define RESULT_MAX_FEATURES 40

#define INIT _IO('a', 'a')
#define NDP10X_CONFIG _IOR('a', 'b', __u64)
#define NDP_CONFIG _IOR('a', 'c', __u64)
#define LOAD _IOR('a', 'd', __u64)
#define TRANSFER _IOR('a', 'e', __u64)
#define WATCH _IOR('a', 'f', __u64)
#define DRIVER_CONFIG _IOR('a', 'g', __u64)
#define PCM_EXTRACT _IOR('a', 'h', __u64)
#define PCM_SEND _IOR('a', 'i', __u64)
#define STATS _IOR('a', 'j', __u64)

#ifdef __cplusplus
}
#endif

#endif
