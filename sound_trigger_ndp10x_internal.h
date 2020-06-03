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

#ifndef SOUND_TRIGGER_NDP10X_INTERNAL_H
#define SOUND_TRIGGER_NDP10X_INTERNAL_H

#include <hardware/sound_trigger.h>
#include "syntiant_st_speaker_id.h"

#define ENABLE_ACTIVATION_DUMP
#define ENABLE_SPEAKER_ID_ACTIVATION_DUMP

#define ENABLE_SPEAKER_ID_SUPPORT

// TODO : Get these settings from SoundModel
#define ACTIVATION_RECORD_TIME_MS 1500
#define ACTIVATION_RECORD_NUM_SAMPLES (ACTIVATION_RECORD_TIME_MS * 16000 / 1000)

/*
 * below uuid is got from below link at 06/20/2019
 * http://www.itu.int/ITU-T/asn1/uuid.html
 * 84c8645b-0bca-4500-a4ef-265f5d21dc72
 */
#define SYNTIANT_STHAL_UUID               \
  {                                       \
    0x84c8645b, 0x0bca, 0x4500, 0xa4ef, { \
      0x26, 0x5f, 0x5d, 0x21, 0xdc, 0x72  \
    }                                     \
  } /* uuid */

#define SYNTIANT_SPEAKER_ID_WW_LEN 19200
#define SYNTIANT_SPEAKER_ID_MAX_USERS 1

enum syntiant_ndp10x_stdev_mode {
  STDEV_MODE_IDLE = 0,      // mode before start_recognition
  STDEV_MODE_WATCHING = 1,  // mode between start_recognition and recognition_event
  STDEV_MODE_RECORDING = 2  // mode after recognition_event
};

struct syntiant_speaker_id_s {
  void* pSpkrid_handle;
  int user_model_sz;
  // int *user_model_bin;
  size_t ww_len;
  size_t enrolled_users;
};

struct syntiant_ndp10x_stdev {
  struct sound_trigger_hw_device device;
  int ndp_handle;
  int initialized;

  unsigned int current_mode;

  sound_model_handle_t model_handle;
  unsigned int supported_recognition_modes;

  int keyphrase_id;
  unsigned int recognition_modes;

  recognition_callback_t recognition_callback;
  void* recognition_cookie;

  sound_model_callback_t sound_model_callback;
  void* sound_model_cookie;

  pthread_mutex_t lock;
  pthread_t watch_thread;

  int capture_handle;

  short activation_samples[ACTIVATION_RECORD_NUM_SAMPLES];
  short* activation_samples_currptr;
  size_t num_unread_activation_samples;

  bool is_any_model_loaded;
  bool close_watch_thread;

  bool currently_ahal_playing;
  bool currently_ahal_recording;

#ifdef ENABLE_BARGE_IN_SUPPORT
  void* bHandle;
#endif

#ifdef ENABLE_SPEAKER_ID_SUPPORT
  struct syntiant_speaker_id_s speaker_id;
#endif
};

int stdev_update_input();

#endif
