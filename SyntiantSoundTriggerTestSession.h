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

#ifndef SYNTIANT_SOUND_TRIGGER_TEST_SESSION_H
#define SYNTIANT_SOUND_TRIGGER_TEST_SESSION_H

#include <binder/MemoryDealer.h>
#include <string>

#include <soundtrigger/SoundTrigger.h>
#include <soundtrigger/SoundTriggerCallback.h>

#include "SyntiantUserModelEnroller.h"

namespace android {

class SyntiantFakeVoiceAssistant : public SoundTriggerCallback {
  enum steps_e {
    STEP_SPEAKER_ENROLLMENT = 1 << 1,
    STEP_WAKEWORD_DETECTION = 1 << 2,
    STEP_SPEAKER_IDENTIFICATION = 1 << 3,
    STEP_QUERY_RECORDING = 1 << 4,
    STEP_CONVERT_MODEL = 1 << 5
  };

 public:
  SyntiantFakeVoiceAssistant(std::string modelFile, std::string savedVMFile,
                             bool useShortOutput = false, bool userEnrollEnabled = false, int userId = 0,
                             unsigned int recognition_modes = RECOGNITION_MODE_VOICE_TRIGGER,
                             unsigned int record_len = 0);
  ~SyntiantFakeVoiceAssistant();

  int run();
  int stop();

 private:
  std::string modelFile;
  std::string savedSMFile;
  sp<SoundTriggerCallback> soundTriggerCallbacks;
  sp<SoundTrigger> st_module;

  int init();
  int listModules();
  int attach();
  void detach();

  int buildSoundModel(std::string firmware_file_name, sp<IMemory>* memory);
  sound_model_handle_t loadSoundModel(sp<IMemory>& memory);
  sound_model_handle_t loadSoundModel(std::string firmware_file_name);
  int startRecognition(sound_model_handle_t sound_model_handle);
  int stopRecognition(sound_model_handle_t sound_model_handle);
  int unloadSoundModel(sound_model_handle_t sound_model_handle);

  int dump_counter;

  // std::string firmware_file_name;
  std::string wav_prefix;
  size_t record_len_ms;
  // size_t pre_roll_len_ms;
  // size_t recogMemSize;

  Mutex mLock;
  bool mRecognitionEventHappened;
  audio_session_t capture_session;

  bool mSoundModelEventHappened;
  struct sound_trigger_model_event mSoundModelEvent;

  bool mServiceStateChangeHappened;
  sound_trigger_service_state_t mServiceState;

  sp<IMemory> mSoundMemory;

  /* speaker ID relaated */
  bool mShortOutput;
  bool mEnrollEnabled;
  int mUserId;
  std::string mPhrases[SOUND_TRIGGER_MAX_PHRASES];
  SyntiantUserModelEnroller mSpeakerIDEnroller;
  unsigned int mRecognitionModes;

  int setCaptureState(bool enable);

  int record(void);
  void onRecognitionEvent(struct sound_trigger_recognition_event* event);
  void onSoundModelEvent(struct sound_trigger_model_event* event);
  void onServiceStateChange(sound_trigger_service_state_t state);
  void onServiceDied();
};

};  // namespace android
#endif
