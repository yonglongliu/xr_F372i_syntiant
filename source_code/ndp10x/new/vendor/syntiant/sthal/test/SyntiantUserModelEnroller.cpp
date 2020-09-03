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

#define LOG_TAG "SyntiantUserModelEnroller"
#define LOG_NDEBUG 0

#include <iostream>

#include "SyntiantUserModelEnroller.h"
#include "syntiant_soundmodel.h"
#include <sound_trigger_ndp10x.h>


// file name (incl index) for dumps of enrollment data
#define ENROLL_DUMP_FILE_NAME "/data/aov/user_model_enroll_data%d.raw"

// Enable the following to use the compatible (old/legacy) interface
// #define USE_COMPAT_INTERFACE

SyntiantUserModelEnroller::SyntiantUserModelEnroller()
    : mCurrentIdx(0),
      mUseCompatInterface(
#ifndef USE_COMPAT_INTERFACE
          false
#else
          true
#endif
          ){};

SyntiantUserModelEnroller::~SyntiantUserModelEnroller() {}

int SyntiantUserModelEnroller::add(uint8_t* data, unsigned int data_len,
                                   unsigned long confidence_level) {
  struct sound_trigger_phrase_sound_model dummy_model;
  unsigned int num_recordings;
  audio_config_t audio_config;
  char buf[256];

  if (mCurrentIdx >= SYNTIANT_ST_SOUND_MODEL_MAX_ENROLLMENT_RECORDINGS) {
    return -1;
  }

  syntiant_st_sound_model_get_enrollment_recording_requirements(&dummy_model, &num_recordings,
                                                                &audio_config);

  if (data_len > audio_config.frame_count * sizeof(short)) {
    std::cerr << "#### Received long audio sample (" << data_len << "). Adjusting length." << std::endl;
    data += data_len - audio_config.frame_count * sizeof(short);
    data_len = audio_config.frame_count * sizeof(short);
  }

#if 0
  /* validate that no excessive clipping is happening */
  const int clip_threshold = 10;
  int n_clipped = 0;
  short *pData = (short*)data;
  for (int n = 0; n < data_len/sizeof(short); n++) {
    if((*pData == -0x7fff) ||
       (*pData ==  0x7fff)) {
      n_clipped++;
    }
    pData++;

    if(n_clipped > clip_threshold) {
      std::cerr << ">>>> Speaking too loudly, compromising enrollment!  "
                   "Speak softer or move phone further away from mouth"
                   << std::endl;
      return 0;
    }
  }
#else
  if (confidence_level == UTTERANCE_SATURATED) {
    std::cerr << ">>>> Speaking too loudly, compromising enrollment!  "
                 "Speak softer or move phone further away from mouth"
                 << std::endl;
    return 0;
  } else if (confidence_level == UTTERANCE_TOO_NOISY) {
    std::cerr << ">>>> Environment too noisy, compromising enrollment!  "
                 "Please find a quieter area during enrollment"
                 << std::endl;
    return 0;
  } else if (confidence_level == UTTERANCE_TOO_SOFT) {
    std::cerr << ">>>> Speaking too softly, compromising enrollment!  "
                 "Speak a little louder or move closer to phone"
                 << std::endl;
    return 0;
  }
#endif

  if (strlen(ENROLL_DUMP_FILE_NAME) > 0) {
    sprintf(buf, ENROLL_DUMP_FILE_NAME, mCurrentIdx);
    FILE* fp = fopen(buf, "wb");
    if (fp) {
      fwrite(data, data_len, 1, fp);
      fclose(fp);
    } else {
      std::cerr << "Error opening file " << buf << std::endl;
    }
  }

  memcpy(mRecordings[mCurrentIdx], data, data_len);

  mCurrentIdx++;
  std::cout << "Completed enrollment step " << mCurrentIdx << " of " << num_recordings << std::endl;

  return 0;
}

int SyntiantUserModelEnroller::train(struct sound_trigger_phrase_sound_model* src_model,
                                     uint8_t** dest_buffer, uint32_t* dest_size, int user_id) {
  unsigned int num_recordings;
  audio_config_t req_audio_config;

  uint32_t model_size;
  int s;

  syntiant_st_sound_model_get_enrollment_recording_requirements(src_model, &num_recordings,
                                                                &req_audio_config);

  /* in our test app, ::train may be called even when there are not enough
   * samples, but function will only do something if we have enough samples
   * for enrollment */
  if (mCurrentIdx >= num_recordings) {
    struct syntiant_st_sound_model_enrollment_recording_s recordings[num_recordings];
    if (!mUseCompatInterface) {
      ALOGV("%s : syntiant Get model size", __func__);
      model_size = syntiant_st_sound_model_get_size_when_extended(src_model);
      if (dest_buffer) {
        if (*dest_buffer) {
          ALOGV("%s : free up existing buffer", __func__);
          free(*dest_buffer);
        }
        ALOGV("%s : syntiant alloc buffer (%d bytes) for saving model", __func__, model_size);
        *dest_buffer = (uint8_t*)malloc(model_size);
        if (!*dest_buffer) {
          ALOGE("%s : Could not allocate memory for buffer", __func__);
          return -ENOMEM;
        }
        memset(*dest_buffer, 0, model_size);
      }

      for (unsigned int i = 0; i < num_recordings; i++) {
        memcpy(&recordings[i].audio_data, mRecordings[i],
               req_audio_config.frame_count * sizeof(short));
      }
      ALOGE("%s Extend the syngup model (user id %d), expected size:%u", __func__, user_id, model_size);
      int res = syntiant_st_sound_model_extend(src_model, user_id, num_recordings, recordings,
                                               dest_buffer, &model_size);
      /* set the final size of the buffer */
      ALOGV("%s : After compaction: %u", __func__, model_size);
      *dest_size = model_size;
    } else {
      int16_t* recordings[SYNTIANT_ST_SOUND_MODEL_MAX_ENROLLMENT_RECORDINGS];
      struct syntiant_user_sound_model_s* dest_model;
      FILE* fp;

      *dest_buffer = NULL;
      *dest_size = 0;

      for (unsigned int i = 0; i < num_recordings; i++) {
        recordings[i] = (int16_t*)&mRecordings[i];
      }

      s = ExtendSoundModel_Samples(src_model, recordings, &dest_model, user_id);
      ALOGV("%s : ExtendSoundModel_Samples: %d", __func__, s);

      /* don't rely on saving via regular mechanism in the test app */
      if (dest_model) {
        fp = fopen("/data/test_spkr.syngup", "wb");
        if (fp) {
          uint8_t* buffer = dest_model->model;
          unsigned int buffer_size = dest_model->model_size;

          ALOGV("%s : Write %d bytes to file ", __func__, buffer_size);
          fwrite(buffer, buffer_size, 1, fp);
          free(dest_model);
          fclose(fp);
        }
      }
    }
    return TRAINING_DONE;
  } else {
    return TRAINING_NEED_MORE_SAMPLES;
  }
}

int SyntiantUserModelEnroller::clear() {
  mCurrentIdx = 0;
  return 0;
}
