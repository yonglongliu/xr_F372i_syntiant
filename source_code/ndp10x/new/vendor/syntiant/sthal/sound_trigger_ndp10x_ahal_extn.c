/*
 * SYNTIANT CONFIDENTIAL
 * _____________________
 *
 *   Copyright (c) 2019 Syntiant Corporation
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

#define LOG_TAG "SyntiantSoundTriggerHAL"
#define LOG_NDEBUG 0

#include <cutils/log.h>
#include <utils/Log.h>
#include "ndp10x_hal.h"
#include "sound_trigger_ndp10x_internal.h"
#include "syntiant_barge_in_c_api.h"
#define MAX_STRING_LENGTH 256
enum ahal_notification_type_e {
  AHAL_NOTIFICATION_PLAYBACK_STARTED = 0,
  AHAL_NOTIFICATION_PLAYBACK_STOPPED = 1,
  AHAL_NOTIFICATION_CAPTURE_STARTED = 2,
  AHAL_NOTIFICATION_CAPTURE_STOPPED = 3
};

extern struct syntiant_ndp10x_stdev ndp10x_stdev_g;

/* TODO: When capruting with KaiOS I notice that the handle is not
 * matching the capture handle in callback, so disable the check
 * with the following define */
#define DISABLE_CAPTURE_HANDLE_CHECK

// static FILE *fp;

// static void dump_activation_to_file(short *samples, size_t num_samples)
// {
//     FILE *fp = NULL;
//     static size_t activation_counter = 0;
//     char activation_record_filename[MAX_STRING_LENGTH];
//     size_t bytes_written = 0;

//     snprintf(activation_record_filename, MAX_STRING_LENGTH,
//              "/data/aov/%s_%zu.raw", "sthal_debug_dump",
//              activation_counter++ % 1000);

//     ALOGI("%s: Now dumping activation audio to %s", __func__,
//           activation_record_filename);
//     fp = fopen(activation_record_filename, "wb");

//     if (fp == NULL)
//     {
//         ALOGE("%s : error opening %s file", __func__,
//               activation_record_filename);
//         return;
//     }

//     bytes_written = fwrite(samples, 1,
//                            num_samples * sizeof(short),
//                            fp);

//     if (bytes_written != num_samples * sizeof(short))
//     {
//         ALOGE("%s : error only %zu bytes written instead of %zu", __func__, bytes_written,
//         num_samples * sizeof(short));
//     }

//     fclose(fp);
// }

// int sound_trigger_check_barge_in_capture_handle(int capture_handle)
// {
//     struct syntiant_ndp10x_stdev *stdev = &ndp10x_stdev_g;
//     int handle = 0;

//     handle = syntiant_barge_in_get_handle(stdev->bHandle);

//     ALOGV("%s : enter barge in handle(%d) capture_handle(%d)", __func__,
//           handle, capture_handle);

//     bool is_barge_in_capture = false;

//     if (capture_handle == handle)
//     {
//         is_barge_in_capture = true;
//     }

//     return is_barge_in_capture;
// }

/**
 * To be only called with stdev->lock locked
 */
int sound_trigger_check_capture_handle(int capture_handle) {
  struct syntiant_ndp10x_stdev* stdev = &ndp10x_stdev_g;
  // ALOGV("%s : enter stdev->capture_handle(%d) capture_handle(%d)", __func__,
  //       stdev->capture_handle, capture_handle);

  bool is_sound_trigger_capture = false;

  if (capture_handle == stdev->capture_handle) {
    is_sound_trigger_capture = true;
  }

#ifdef DISABLE_CAPTURE_HANDLE_CHECK
  is_sound_trigger_capture = 1;
#endif
  return is_sound_trigger_capture;
}

/**
 * This API assumes to be only called after Wakeword detection
 * so you need a proper capture_handle which is provided in
 * callback
 */

__attribute__((visibility("default"))) int sound_trigger_open_for_streaming(int capture_handle) {
  struct syntiant_ndp10x_stdev* stdev = &ndp10x_stdev_g;
  int ret = 0;

  ALOGV("%s : enter", __func__);

  pthread_mutex_lock(&stdev->lock);

  if (!stdev->initialized) {
    ALOGE("%s: stdev has not been initialized: %d", __func__, stdev->initialized);
    ret = -EFAULT;
    goto exit;
  }

  if (!sound_trigger_check_capture_handle(capture_handle)) {
    ALOGE("%s: capture handle mismatch", __func__);
    ret = -EINVAL;
  }

// fp = fopen("/data/local/media/sthal_debug.raw", "wb");
// if (fp == NULL) {
//     ALOGE("%s : error opening  /data/local/media/sthal_debug.raw", __func__);
// }

exit:
  pthread_mutex_unlock(&stdev->lock);
  return ret;
}

__attribute__((visibility("default"))) size_t sound_trigger_read_samples(int capture_handle,
                                                                         uint8_t* buffer,
                                                                         size_t bytes) {
  struct syntiant_ndp10x_stdev* stdev = &ndp10x_stdev_g;
  int ret = 0;
  unsigned int n = 0;
  size_t num_samples = 0;
  short* samples = (short*)buffer;

  memset(buffer, 0, bytes);

  if (bytes % sizeof(short) != 0) {
    ALOGE("%s : num of bytes must of multiple of sizeof(short)", __func__);
    return -EINVAL;
  }

  num_samples = bytes / sizeof(short);

  ALOGE("%s : enter capture_handle(%d) buffer(%p) num_samples(%zu)", __func__, capture_handle,
        samples, num_samples);

  pthread_mutex_lock(&stdev->lock);

  if (!stdev->initialized) {
    ALOGE("%s: stdev has not been initialized", __func__);
    ret = -EFAULT;
    goto exit_locked;
  }

  if (!sound_trigger_check_capture_handle(capture_handle)) {
    ALOGE("%s: capture handle mismatch", __func__);
    ret = -EINVAL;
    goto exit_locked;
  }

  if (stdev->num_unread_activation_samples) {
    ALOGV("%s: Reading from cache   unread=%d   ptr=%p", __func__,
          stdev->num_unread_activation_samples, stdev->activation_samples_currptr);

    n = num_samples <= stdev->num_unread_activation_samples ? num_samples
                                                            : stdev->num_unread_activation_samples;

    memcpy(samples, stdev->activation_samples_currptr, n * sizeof(short));

    stdev->num_unread_activation_samples -= n;
    stdev->activation_samples_currptr += n;

    if (n == num_samples) {
      // dump_activation_to_file((short *) buffer, bytes /sizeof(short));
      goto exit_locked;
    } else {
      /* not enough in cache, still need to read rest from driver */
      num_samples -= n;
      samples += n;
    }
  }

  // dump_activation_to_file((short *) buffer, bytes /sizeof(short));

  /* unlock prior to extracting as extract may block if someone calls stop recognition
   * e.g. by exiting the app
   * this in turn may lead to deadlock */
  pthread_mutex_unlock(&stdev->lock);

  ALOGV("%s : now reading from driver num_samples(%zu) buffer(%p)", __func__, num_samples, samples);
  ret = ndp10x_hal_pcm_extract(stdev->ndp_handle, samples, num_samples);
  if (ret < 0) {
    ALOGE("%s : failed to do pcm extract", __func__);
    ret = -EIO;
  }

  ret = bytes;

  ALOGE("%s : exiting ", __func__);

  return ret;

exit_locked:
  pthread_mutex_unlock(&stdev->lock);
  ALOGE("%s : exiting ", __func__);

  // if (fp != NULL) {
  //     ALOGE("%s : writing to debug ", __func__);
  //     fwrite(buffer, 1, bytes, fp);
  // }

  return ret;
}

__attribute__((visibility("default"))) int sound_trigger_close_for_streaming(
    int capture_handle __unused) {
  struct syntiant_ndp10x_stdev* stdev = &ndp10x_stdev_g;
  int s = 0;
  ALOGE("%s called", __func__);
  pthread_mutex_lock(&stdev->lock);
  stdev->current_mode = STDEV_MODE_IDLE;
  stdev->capture_handle = 0;
  pthread_mutex_unlock(&stdev->lock);

  // if (fp!= NULL) {
  //     fclose(fp);
  // }

  s = stdev_update_input();
  return s;
}

__attribute__((visibility("default"))) int sound_trigger_notify(uint8_t notification) {
  struct syntiant_ndp10x_stdev* stdev = &ndp10x_stdev_g;
  int s = 0;
  pthread_mutex_lock(&stdev->lock);

  switch (notification) {
    case AHAL_NOTIFICATION_PLAYBACK_STARTED:
      stdev->currently_ahal_playing = true;
      ALOGI("%s: Playback started", __func__);
      break;
    case AHAL_NOTIFICATION_PLAYBACK_STOPPED:
      stdev->currently_ahal_playing = false;
      ALOGI("%s: Playback stopped", __func__);
      break;
    case AHAL_NOTIFICATION_CAPTURE_STARTED:
      stdev->currently_ahal_recording = true;
      ALOGI("%s: Capture started", __func__);
      break;
    case AHAL_NOTIFICATION_CAPTURE_STOPPED:
      stdev->currently_ahal_recording = false;
      ALOGI("%s: Capture stopped", __func__);
      break;
  }
  pthread_mutex_unlock(&stdev->lock);

  s = stdev_update_input();

  return s;
}