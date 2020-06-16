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

#define LOG_TAG "ndp10x_hal"
#define LOG_NDEBUG 0

#include "ndp10x_hal.h"
#include <cutils/log.h>

#include "syntiant_defs.h"

int ndp10x_hal_open() {
  int handle = 0;

  handle = open("/dev/ndp10x", 0);
  if (handle < 0) {
    PERROR("ndp10x open failed");
  }

  return handle;
}

int ndp10x_hal_close(ndp10x_handle_t handle) {
  int s = 0;
  s = close(handle);
  return s;
}

int ndp10x_hal_init(ndp10x_handle_t handle) {
  int s = 0;
  if (ioctl(handle, INIT, NULL) < 0) {
    PERROR("ndp10x default init ioctl failed");
    s = -EIO;
  }

  return s;
}

#define CHUNK_SIZE 8192
int ndp10x_hal_load(ndp10x_handle_t handle, uint8_t* package, size_t package_size) {
  struct ndp10x_load_s load;
  uint8_t* begin_ptr;
  uint8_t* end_ptr;
  int s = 0;
  int chunk_size;

  ALOGI("%s : enter package_size=%d", __func__, package_size);

  chunk_size = CHUNK_SIZE;
  begin_ptr = package;
  end_ptr = package + package_size;
  load.error_code = 0;
  load.length = 0;
  load.package = (uintptr_t)NULL;

  do {
    s = ioctl(handle, LOAD, &load);
    if (s < 0) {
      ALOGE("%s : ndp10x load ioctl failed", __func__);
      goto error;
    }
    // Advance with data loaded in last round
    begin_ptr += load.length;
    if ((begin_ptr + chunk_size) > end_ptr) chunk_size = end_ptr - begin_ptr;
    load.length = chunk_size;
    load.package = (uintptr_t)begin_ptr;
  } while (load.error_code == SYNTIANT_NDP_ERROR_MORE);

  if (load.error_code != SYNTIANT_NDP_ERROR_NONE) {
    s = load.error_code;
    goto error;
  }

error:
  ALOGI("%s : exit code %d ", __func__, s);
  return s;
}

void ndp10x_hal_print_ndp10x_config(ndp10x_handle_t handle) {
  /* TODO: Restricting to only one print out for now */
  static int done = 0;
  int s = 0;

  if (done) return;

  ALOGI("%s : enter", __func__);

  struct syntiant_ndp10x_config_s ndp10x_config;

  memset(&ndp10x_config, 0, sizeof(ndp10x_config));
  ndp10x_config.get_all = 1;

  s = ioctl(handle, NDP10X_CONFIG, &ndp10x_config);
  if (s < 0) {
    PERROR("ndp10x_config set_config ioctl failed\n");
    return;
  }

  ALOGI("%s : tank input %u", __func__, ndp10x_config.tank_input);
  ALOGI("%s : tank size %u", __func__, ndp10x_config.tank_size);
  ALOGI("%s : dnn input %u", __func__, ndp10x_config.dnn_input);
  ALOGI("%s : input clock rate %u", __func__, ndp10x_config.input_clock_rate);
  ALOGI("%s : core clock rate %u", __func__, ndp10x_config.core_clock_rate);
  ALOGI("%s : pdm clock rate %u", __func__, ndp10x_config.pdm_clock_rate);
  ALOGI("%s : pdm clock ndp %u ", __func__, ndp10x_config.pdm_clock_ndp);
  ALOGI("%s : pdm in shift %u  %u", __func__, ndp10x_config.pdm_in_shift[0],
        ndp10x_config.pdm_in_shift[1]);
  ALOGI("%s : pdm out shift %u  %u", __func__, ndp10x_config.pdm_out_shift[0],
        ndp10x_config.pdm_out_shift[1]);
  ALOGI("%s : power offset  %u", __func__, ndp10x_config.power_offset);
  ALOGI("%s : match_per_frame (%d)", __func__, ndp10x_config.match_per_frame_on);

  done = 1;
}

int ndp10x_hal_stats(ndp10x_handle_t handle, int commands) {
  struct ndp10x_statistics_s ndp_stats;
  int ret = 0;

  memset(&ndp_stats, 0, sizeof(struct ndp10x_statistics_s));

  if (commands & NDP10X_HAL_STATS_COMMAND_CLEAR) {
    ndp_stats.clear = 1;
  }

  ret = ioctl(handle, STATS, &ndp_stats);
  if (ret < 0) {
    PERROR("stats ioctl failed");
    goto exit;
  }

  if (commands & NDP10X_HAL_STATS_COMMAND_PRINT) {
    ALOGI("isrs: %llu, polls: %llu, frames: %llu\n", ndp_stats.isrs, ndp_stats.polls,
          ndp_stats.frames);
    ALOGI("results: %llu, dropped: %llu, ring used: %d\n", ndp_stats.results,
          ndp_stats.results_dropped, ndp_stats.result_ring_used);
    ALOGI("extracts: %llu, bytes: %llu, bytes dropped: %llu, ring used: %d\n", ndp_stats.extracts,
          ndp_stats.extract_bytes, ndp_stats.extract_bytes_dropped, ndp_stats.extract_ring_used);
    ALOGI("sends: %llu, bytes: %llu, ring used: %d\n", ndp_stats.sends, ndp_stats.send_bytes,
          ndp_stats.send_ring_used);
  }

exit:
  return ret;
}

int ndp10x_hal_set_tank_size(ndp10x_handle_t handle, unsigned int tank_size) {
  int ret = 0;
  struct syntiant_ndp10x_config_s ndp10x_config;

  memset(&ndp10x_config, 0, sizeof(struct syntiant_ndp10x_config_s));
  ndp10x_config.set = SYNTIANT_NDP10X_CONFIG_SET_TANK_SIZE;
  ndp10x_config.tank_size = tank_size;

  ret = ioctl(handle, NDP10X_CONFIG, &ndp10x_config);
  if (ret < 0) {
    PERROR("ndp10x_config set_config ioctl failed\n");
  }

  return ret;
}

int ndp10x_hal_set_input(ndp10x_handle_t handle, int input_type, int firmware_loaded) {
  int ret = 0;
  struct syntiant_ndp10x_config_s ndp10x_config;
  int i;

  ALOGV("%s: set input to %d", __func__, input_type);

  /* TODO: Current code unconditionally configures for PDM1 & NDP as clock src
   * We should instead use the values from loaded package instead */
  memset(&ndp10x_config, 0, sizeof(struct syntiant_ndp10x_config_s));
  ndp10x_config.set = SYNTIANT_NDP10X_CONFIG_SET_PDM_CLOCK_NDP |
                      SYNTIANT_NDP10X_CONFIG_SET_DNN_INPUT | SYNTIANT_NDP10X_CONFIG_SET_TANK_INPUT;
  ndp10x_config.pdm_clock_ndp = (input_type == NDP10X_HAL_INPUT_TYPE_MIC) ? 1 : 0;
  if (input_type == NDP10X_HAL_INPUT_TYPE_NONE) {
    ndp10x_config.dnn_input = SYNTIANT_NDP10X_CONFIG_DNN_INPUT_NONE;
    ndp10x_config.tank_input = SYNTIANT_NDP10X_CONFIG_TANK_INPUT_NONE;
    if (firmware_loaded) {
      ndp10x_config.set |= SYNTIANT_NDP10X_CONFIG_SET_MATCH_PER_FRAME_ON;
    }
    ndp10x_config.match_per_frame_on = 1;
  } else if (input_type == NDP10X_HAL_INPUT_TYPE_PCM) {
    ndp10x_config.dnn_input = SYNTIANT_NDP10X_CONFIG_DNN_INPUT_SPI;
    ndp10x_config.tank_input = SYNTIANT_NDP10X_CONFIG_TANK_INPUT_SPI;
    if (firmware_loaded) {
      ndp10x_config.set |= SYNTIANT_NDP10X_CONFIG_SET_MATCH_PER_FRAME_ON;
    }
    ndp10x_config.match_per_frame_on = 1;
  } else {
    ndp10x_config.dnn_input = SYNTIANT_NDP10X_CONFIG_DNN_INPUT_PDM1;
    ndp10x_config.tank_input = SYNTIANT_NDP10X_CONFIG_TANK_INPUT_PDM1;
    /* no change to match per frame here as it will impact how seamless
     * queries work */
  }

  ret = ioctl(handle, NDP10X_CONFIG, &ndp10x_config);
  if (ret < 0) {
    PERROR("ndp10x_config set_config ioctl failed\n");
  }

  return ret;
}

int ndp10x_hal_pcm_extract(ndp10x_handle_t handle, short* samples, size_t num_samples) {
  int ret = 0;
  struct ndp10x_pcm_extract_s extract;
  memset(&extract, 0, sizeof(extract));

  ALOGI("%s : enter num_samples(%zu) ", __func__, num_samples);

  if (num_samples == 0) {
    extract.flush = 1;
  } else {
    extract.flush = 0;
    extract.buffer = (uintptr_t)(uint8_t*)samples;
    extract.buffer_length = num_samples * sizeof(short);
  }

  ndp10x_hal_print_ndp10x_config(handle);
  ndp10x_hal_stats(handle, NDP10X_HAL_STATS_COMMAND_PRINT);

  ret = ioctl(handle, PCM_EXTRACT, &extract);
  if (ret < 0) {
    PERROR("ndp10x extract flush ioctl failed");
    goto exit;
  }

  if (extract.extracted_length != extract.buffer_length) {
    ALOGE("%s : could not extract %zu bytes ", __func__, extract.buffer_length);
    ret = -EINVAL;
  }

exit:
  return ret;
}

int ndp10x_hal_pcm_send(ndp10x_handle_t handle, short* samples, size_t num_samples) {
  struct ndp10x_pcm_send_s send;
  int ret = 0;
  memset(&send, 0, sizeof(send));
  send.buffer = (uintptr_t)(uint8_t*)samples;
  send.buffer_length = num_samples * sizeof(short);

  ret = ioctl(handle, PCM_SEND, &send);
  if (ret < 0) {
    PERROR("ndp10x PCM send ioctl failed\n");
  }

  return ret;
}

int ndp10x_hal_get_audio_frame_size(ndp10x_handle_t handle, int* audio_frame_step) {
  int ret = 0;
  struct syntiant_ndp10x_config_s ndp10x_config;
  memset(&ndp10x_config, 0, sizeof(struct syntiant_ndp10x_config_s));

  ndp10x_config.get_all = 1;

  ret = ioctl(handle, NDP10X_CONFIG, &ndp10x_config);
  if (ret < 0) {
    PERROR("ndp10x_config set_config ioctl failed\n");
    *audio_frame_step = 0;
    goto exit;
  }

  *audio_frame_step = ndp10x_config.audio_frame_step;

exit:
  return ret;
}

int ndp10x_hal_flush_results(ndp10x_handle_t handle) {
  int s = 0;
  struct ndp10x_watch_s watch_result;

  watch_result.flush = 1;
  s = ioctl(handle, WATCH, &watch_result);
  if (s < 0) {
    ALOGE("%s: Error flushing watch results", __func__);
  }

  return s;
}

#define MAX_STRING_LENGTH 256

int ndp10x_hal_get_num_phrases(ndp10x_handle_t handle, int* num_phrases) {
  int s = 0;
  struct ndp10x_ndp_config_s config;
  char device_type[MAX_STRING_LENGTH] = { 0 };
  char fwver[MAX_STRING_LENGTH] = { 0 };
  char paramver[MAX_STRING_LENGTH] = { 0 };
  char packagever[MAX_STRING_LENGTH] = { 0 };
  char label_data[MAX_STRING_LENGTH] = { 0 };

  ALOGI("%s : enter", __func__);

  memset(&config, 0, sizeof(config));
  config.device_type = (uintptr_t)device_type;
  config.device_type_len = sizeof(device_type);
  config.firmware_version = (uintptr_t)fwver;
  config.firmware_version_len = sizeof(fwver);
  config.parameters_version = (uintptr_t)paramver;
  config.parameters_version_len = sizeof(paramver);
  config.labels = (uintptr_t)label_data;
  config.labels_len = sizeof(label_data);
  config.pkg_version = (uintptr_t)packagever;
  config.pkg_version_len = sizeof(packagever);

  s = ioctl(handle, NDP_CONFIG, &config);
  if (s < 0) {
    ALOGE("%s : error while CONFIG ioctl : %d ", __func__, s);
    goto error;
  }

  // TODO: change to appropriate thing. Current models have only support
  // for classes not "phrases"
  *num_phrases = config.classes;

  ALOGE("%s : device_type %s ", __func__, device_type);
  ALOGE("%s : fwver %s ", __func__, fwver);
  ALOGE("%s : paramver %s ", __func__, paramver);
  ALOGE("%s : packagever %s ", __func__, packagever);
  ALOGE("%s : label_data %s ", __func__, label_data);

error:
  return s;
}

int ndp10x_hal_wait_for_match(ndp10x_handle_t handle, int keyphrase_id, int extract_match_mode,
                              int extract_before_match_ms, unsigned int *info) {
  int s = 0;
  int num_phrases = -1;
  struct ndp10x_watch_s watch_result;

  s = ndp10x_hal_get_num_phrases(handle, &num_phrases);
  if (s < 0) {
    ALOGE("%s : error while get num of phrases", __func__);
    goto error;
  }

  if (keyphrase_id != 0 && num_phrases > 1) {
    ALOGE("%s : invalid keyphrase id ", __func__);
    s = -EINVAL;
    goto error;
  }

  // ndp10x_hal_print_ndp10x_config(handle);

  memset(&watch_result, 0, sizeof(watch_result));

  watch_result.timeout = -1;
  // TODO: Change this to phrases once we have support for it.
  watch_result.classes = (1 << keyphrase_id);

  watch_result.extract_match_mode = extract_match_mode;
  watch_result.extract_before_match = extract_before_match_ms;

  ALOGE("%s : now watching for class %llu extract_match_mode(%d) extract_before_match(%d)",
        __func__, watch_result.classes, extract_match_mode, extract_before_match_ms);
  s = ioctl(handle, WATCH, &watch_result);

  if (s < 0) {
    ALOGE("%s : error in watch ioctl ", __func__);
    goto error;
  }

  ALOGE("%s : classes index %d keyphrase id %d ", __func__, watch_result.class_index, keyphrase_id);

  if (watch_result.match && (watch_result.class_index != keyphrase_id)) {
    ALOGE("%s : something weird happened in watch ioctl", __func__);
    s = -EINVAL;
  }

  if (info) {
    *info = watch_result.info;
  }

  ALOGE("%s : error code %d", __func__, s);

error:
  return s;
}

int ndp10x_hal_wait_for_match_and_extract(ndp10x_handle_t handle, int keyphrase_id, short* samples,
                                          size_t num_samples, unsigned int *info) {
  int s = 0;
  int extract_match_mode = (num_samples > 0) ? 1 : 0;
  int extract_before_match_ms = (((float)num_samples) / 16000) * 1000;

  ALOGI("%s : keyphrase_id(%d) num_samples(%zu)", __func__, keyphrase_id, num_samples);

  s = ndp10x_hal_wait_for_match(handle, keyphrase_id, extract_match_mode, extract_before_match_ms, info);
  if (s < 0) {
    ALOGE("%s: some error occurred while waiting for match", __func__);
    goto error;
  }

  if (num_samples == 0) {
    goto error;
  }

  s = ndp10x_hal_pcm_extract(handle, samples, num_samples);

error:
  return s;
}
