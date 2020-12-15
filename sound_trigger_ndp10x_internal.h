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
#define ACTIVATION_RECORD_TIME_MS 2100
#define ACTIVATION_RECORD_NUM_SAMPLES (ACTIVATION_RECORD_TIME_MS * 16000 / 1000)

#define STHAL_TANK_SIZE 84000

/*
 * below uuid is a version 5 UUID encoding of syntiant.com
 * uuid.uuid5(uuid.NAMESPACE_DNS, 'syntiant.com')
 * UUID('dfa97af5-6b9b-5416-b6b0-4aa37b12918c')
 */
#define SYNTIANT_STHAL_UUID               \
  {                                       \
    0xdfa97af5, 0x6b9b, 0x5416, 0xb6b0, { \
      0x4a, 0xa3, 0x7b, 0x12, 0x91, 0xc8  \
    }                                     \
  } /* uuid */

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
  int enrolled_user_ids[SYNTIANT_SPEAKER_ID_MAX_USERS];
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
  /* host config params, not all may be used on a particular platform */
  int32_t noise_threshold;
  uint16_t noise_threshold_win;
  uint32_t confidence_threshold;
};

int stdev_update_input();

#endif
