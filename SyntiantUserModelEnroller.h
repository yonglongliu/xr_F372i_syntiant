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
#ifndef SYNTIANT_USER_MODEL_ENROLLER_H
#define SYNTIANT_USER_MODEL_ENROLLER_H

#include <string>

#include <soundtrigger/SoundTrigger.h>
#include <soundtrigger/SoundTriggerCallback.h>

#include <syntiant_soundmodel.h>

class SyntiantUserModelEnroller {
 public:
  SyntiantUserModelEnroller();
  ~SyntiantUserModelEnroller();

  enum {
    TRAINING_NEED_MORE_SAMPLES = 1,
    TRAINING_DONE = 2,
  };

  int clear();
  int add(uint8_t* data, unsigned int data_len);
    int train(struct sound_trigger_phrase_sound_model* src_model, uint8_t** buffer, uint32_t* size, int user_id = 0);

 private:
  unsigned int mCurrentIdx;
  /* supports max 2 seconds of audio @16 ksamples/sec */
  uint8_t mRecordings[SYNTIANT_ST_SOUND_MODEL_MAX_ENROLLMENT_RECORDINGS][16000 * sizeof(short) * 2];

  /* when true => use the original interface for ExtendSoundModel when enrolling */
  bool mUseCompatInterface;
};

#endif
