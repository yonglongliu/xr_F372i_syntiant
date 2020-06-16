/*
 * SYNTIANT CONFIDENTIAL
 * _____________________
 *
 *   Copyright (c) 2018 Syntiant Corporation
 *   All Rights Reserved.
 *
 *  NOTICE:  All information contained herein is, and remains the property of
 *  Syntiant Corporation and its suppliers, if any.  The intellectual and
 *  technical concepts contained herein are proprietary to Syntiant Corporation
 *  and its suppliers and may be covered by U.S. and Foreign Patents, patents in
 *  process, and are protected by trade secret or copyright law.  Dissemination
 *  of this information or reproduction of this material is strictly forbidden
 *  unless prior written permission is obtained from Syntiant Corporation.
 */
#ifndef NDP10X_HAL_H
#define NDP10X_HAL_H

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ndp10x_ioctl.h"
#include "syntiant_ilib/ndp10x_regs.h"
#include "syntiant_ilib/ndp10x_spi_regs.h"
#include "syntiant_ilib/syntiant_ndp.h"
#include "syntiant_ilib/syntiant_ndp10x.h"
#include "syntiant_ilib/syntiant_ndp_error.h"

#define PERROR(str) \
  { ALOGE("%s: PERROR('%s'): errno:%d:'%s'", __func__, str, errno, strerror(errno)); }

#ifdef __cplusplus
extern "C" {
#endif

typedef int ndp10x_handle_t;

int ndp10x_hal_open();

int ndp10x_hal_close(ndp10x_handle_t handle);

int ndp10x_hal_init(ndp10x_handle_t handle);

int ndp10x_hal_pcm_extract(ndp10x_handle_t handle, short* samples, size_t num_samples);

int ndp10x_hal_pcm_send(ndp10x_handle_t handle, short* samples, size_t num_samples);

enum ndp10x_hal_stats_commands_e {
  NDP10X_HAL_STATS_COMMAND_CLEAR = 0x1,
  NDP10X_HAL_STATS_COMMAND_PRINT = 0x2
};
int ndp10x_hal_stats(ndp10x_handle_t handle, int commands);

enum ndp10x_hal_input_type_e {
  NDP10X_HAL_INPUT_TYPE_NONE = 0x0,
  NDP10X_HAL_INPUT_TYPE_MIC = 0x1,
  NDP10X_HAL_INPUT_TYPE_PCM = 0x2
};
int ndp10x_hal_set_input(ndp10x_handle_t handle, int input, int firmware_loaded);

int ndp10x_hal_get_audio_frame_size(ndp10x_handle_t handle, int* audio_frame_step);

int ndp10x_hal_load(ndp10x_handle_t handle, uint8_t* package, size_t package_size);

int ndp10x_hal_flush_results(ndp10x_handle_t handle);

int ndp10x_hal_get_num_phrases(ndp10x_handle_t handle, int* num_phrases);

int ndp10x_hal_wait_for_match(ndp10x_handle_t handle, int keyphrase_id, int extract_match_mode,
                              int extract_before_match, unsigned int *info);

int ndp10x_hal_wait_for_match_and_extract(ndp10x_handle_t handle, int keyphrase_id, short* samples,
                                          size_t num_samples, unsigned int *info);

int ndp10x_hal_set_tank_size(ndp10x_handle_t handle, unsigned int tank_size);

#ifdef __cplusplus
}
#endif

#endif