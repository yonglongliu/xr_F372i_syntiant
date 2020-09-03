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
#define LOG_TAG "SyntiantSoundTriggerTestSession"
#define LOG_NDEBUG 0

#include <utils/Log.h>

#include <audio_utils/sndfile.h>
#include <errno.h>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include "SyntiantAudioSession.h"

#include "SyntiantSoundTriggerTestSession.h"

#include <sound_trigger_ndp10x.h>

using namespace android;

SyntiantFakeVoiceAssistant::SyntiantFakeVoiceAssistant(std::string modelFile, std::string smFile,
                                                       bool useShortOutput, bool userEnrollEnabled, int userId,
                                                       unsigned int recognition_modes,
                                                       unsigned int record_length)
    : modelFile(modelFile),
      savedSMFile(smFile),
      soundTriggerCallbacks(this),
      wav_prefix("/data/aov/fake_va"),
      record_len_ms(record_length),
      mShortOutput(useShortOutput),
      mEnrollEnabled(userEnrollEnabled),
      mUserId(userId),
      mRecognitionModes(recognition_modes) {}

SyntiantFakeVoiceAssistant::~SyntiantFakeVoiceAssistant() {}

int SyntiantFakeVoiceAssistant::listModules() {
  int status = 0;
  unsigned int num_st_modules = 0;
  struct sound_trigger_module_descriptor* st_descriptors = NULL;

  status = SoundTrigger::listModules(st_descriptors, &num_st_modules);
  ALOGV("%s: num_modules:%d status:%d\n", __func__, num_st_modules, status);
  if (status != 0) {
    std::stringstream ss;
    ss << "Could not find any sound trigger modules : status = " << num_st_modules << std::endl;
    ALOGE("%s", ss.str().c_str());
    std::cerr << ss.str().c_str();
    goto exit;
  }

  st_descriptors = new struct sound_trigger_module_descriptor[num_st_modules];
  memset(st_descriptors, 0, sizeof(st_descriptors[0]) * num_st_modules);
  status = SoundTrigger::listModules(st_descriptors, &num_st_modules);
  if (status != 0) {
    std::stringstream ss;
    ss << "Could not find any sound trigger modules : status = " << num_st_modules << std::endl;
    ALOGE("%s", ss.str().c_str());
    goto exit;
  }

  for (unsigned int i = 0; i < num_st_modules; i++) {
    std::stringstream ss;
    ss << "sound trigger module handle " << st_descriptors[i].handle << std::endl;
    ss << "Implementor " << st_descriptors[i].properties.implementor << std::endl;
    char uuid_str[256] = { 0 };
    SoundTrigger::guidToString(&st_descriptors[i].properties.uuid, uuid_str, 256);
    ss << "uuid " << uuid_str << " handle: " << st_descriptors[i].handle << std::endl;
    ALOGI("%s", ss.str().c_str());
    std::cout << ss.str() << std::endl;
    ss << "Unload the module and prepare for a fresh module load" << std::endl;
  }

exit:
  if (st_descriptors) {
    delete[] st_descriptors;
  }
  return status;
}

static sp<SoundTrigger> getSoundTrigger(sp<SoundTrigger> module) {
  SoundTrigger* const st = (SoundTrigger*)module.get();
  std::cout << "return sound trigger " << std::endl;
  return sp<SoundTrigger>(st);
}

int SyntiantFakeVoiceAssistant::attach() {
  // Note: This assumes that only one sound trigger modules is available.
  sound_trigger_module_handle_t module_handle = 1;

  st_module = SoundTrigger::attach(module_handle, soundTriggerCallbacks);
  if (st_module.get() == nullptr) {
    std::stringstream ss;
    ss << "Sound Trigger module not found for handle = " << module_handle << std::endl;
    ALOGE("%s", ss.str().c_str());
    return -1;
  }

  return 0;
}

// int SyntiantFakeVoiceAssistant::init()
// {
//    int s = 0;
//    s = listModules();
//    if (s)
//     goto exit;

//   s = attach();
//   if (s)
//     goto exit;

//   exit:
//    return s;
// }

int SyntiantFakeVoiceAssistant::buildSoundModel(std::string modelFile, sp<IMemory>* soundMemory) {
  int rc = 0;
  uint32_t sound_model_size;
  uint32_t file_size;
  struct sound_trigger_phrase_sound_model* sm = NULL;
  uint8_t* sound_memory;

  FILE* fp = fopen(modelFile.c_str(), "rb");
  if (fp == NULL) {
    ALOGE("%s Could not open sound model file : \n", __func__, modelFile.c_str());
    return -EINVAL;
  }

  fseek(fp, 0, SEEK_END);
  file_size = ftell(fp);
  fseek(fp, 0, SEEK_SET);

  uint8_t* buffer = (uint8_t*)malloc(file_size);
  if (!buffer) {
    ALOGE("%s Can not allocate memory: %u bytes\n", __func__, file_size);
    fclose(fp);
    return -ENOMEM;
  }

  uint32_t bytes_read = fread(buffer, 1, file_size, fp);
  if (bytes_read != file_size) {
    ALOGE("%s Error while reading from file\n", __func__);
    free(buffer);
    fclose(fp);
    return -EINVAL;
  }

  sound_model_size = sizeof(struct sound_trigger_phrase_sound_model) + file_size;
  sp<MemoryDealer> memoryDealer = new MemoryDealer(sound_model_size, "For Sound Model");

  if (memoryDealer == NULL) {
    std::stringstream ss;
    ss << "Memory Dealer could not allocated for Sound Model" << std::endl;
    ALOGE("%s", ss.str().c_str());
    free(buffer);
    fclose(fp);
    return -ENOMEM;
  }

  *soundMemory = memoryDealer->allocate(sound_model_size);
  if ((*soundMemory)->pointer() == nullptr) {
    std::stringstream ss;
    ss << "Memory could not allocated for Sound Model" << std::endl;
    ALOGE("%s", ss.str().c_str());
    free(buffer);
    fclose(fp);
    return -ENOMEM;
  }
  sound_memory = (uint8_t*)(*soundMemory)->pointer();

  rc = syntiant_st_sound_model_build_from_binary_sound_model(buffer, file_size, sound_memory);
  if (rc) {
    ALOGE("Could not obtain sound model from binary data\n", __func__);
  } else {
    struct sound_trigger_phrase_sound_model* phraseSoundModel =
        (struct sound_trigger_phrase_sound_model*)(sound_memory);
    for (unsigned int i = 0; i < phraseSoundModel->num_phrases; i++) {
      mPhrases[i] = phraseSoundModel->phrases[i].text;
    }
  }
  if (fp) {
    fclose(fp);
  }
  if (buffer) {
    free(buffer);
  }
  // TODO: instead of building all of this here based on syngup directly
  // we should use syntiant_st_sound_model_build_from_binary_sound_model
  // which should then fill up the data appropriately (based on base model + user model)
  return rc;
}

sound_model_handle_t SyntiantFakeVoiceAssistant::loadSoundModel(sp<IMemory>& sound_memory) {
  sound_model_handle_t sound_model_handle = 0;

  int status = 0;

  status = st_module->loadSoundModel(sound_memory, &sound_model_handle);

  if (status < 0) {
    std::stringstream ss;
    ss << "Error while loading sound model : status =" << status << std::endl;
    ALOGE("%s", ss.str().c_str());
    sound_model_handle = -1;
  }
  ALOGV("%s: sound handle: %d\n", __func__, sound_model_handle);
  return sound_model_handle;
}

sound_model_handle_t SyntiantFakeVoiceAssistant::loadSoundModel(std::string firmware_file_name) {
  int status = buildSoundModel(firmware_file_name, &mSoundMemory);
  if (!status) {
    return loadSoundModel(mSoundMemory);
  } else {
    return status;
  }
}

int SyntiantFakeVoiceAssistant::startRecognition(sound_model_handle_t sound_model_handle) {
  unsigned int opaque_data_size = 0;
  int status = 0;

  size_t recogSize = sizeof(struct sound_trigger_recognition_config) + opaque_data_size;

  sp<IMemory> recog_config_memory;
  struct sound_trigger_recognition_config* rc_config;

  sp<MemoryDealer> recogMemDealer = new MemoryDealer(recogSize, "for Recognition Config");
  if (recogMemDealer == 0) {
    std::stringstream ss;
    ss << "Memory could not allocated for Recognition Config" << std::endl;
    ALOGE("%s", ss.str().c_str());
    goto exit;
  }

  recog_config_memory = recogMemDealer->allocate(recogSize);
  if (recog_config_memory == 0 || recog_config_memory->pointer() == NULL) {
    std::stringstream ss;
    ss << "Memory could not allocated for Recognition Config" << std::endl;
    ALOGE("%s", ss.str().c_str());
    goto exit;
  }

  rc_config = (struct sound_trigger_recognition_config*)recog_config_memory->pointer();

  rc_config->data_size = opaque_data_size;
  rc_config->data_offset = sizeof(struct sound_trigger_recognition_config);
  rc_config->capture_requested = true;
  rc_config->num_phrases = 1;
  rc_config->phrases[0].id = 0;
  rc_config->phrases[0].recognition_modes = mRecognitionModes;
  rc_config->phrases[0].confidence_level = 100;  // TODO: Figure out whether to use it.

  if (!mShortOutput) {
    std::cout << "Start recognition" << std::endl;
  }
  status = st_module->startRecognition(sound_model_handle, recog_config_memory);
  if (status != 0) {
    std::stringstream ss;
    ss << "Error while starting Recognition: status = " << status << std::endl;
    ALOGE("%s", ss.str().c_str());
    goto exit;
  }

exit:
  return status;
}

int SyntiantFakeVoiceAssistant::stopRecognition(sound_model_handle_t sound_model_handle) {
  int status = 0;

  status = st_module->stopRecognition(sound_model_handle);
  std::cout << "Stop recognition" << std::endl;

  if (status != 0) {
    std::stringstream ss;
    ss << "Error while stopping Recognition: status = " << status << std::endl;
    ALOGE("%s", ss.str().c_str());
  }

  return status;
}

int SyntiantFakeVoiceAssistant::unloadSoundModel(sound_model_handle_t sound_model_handle) {
  int status = 0;

  if (sound_model_handle < 0) {
    return status;
  }
  status = st_module->unloadSoundModel(sound_model_handle);
  if (status != 0) {
    std::stringstream ss;
    ss << "Error while unloading Sound Model: status = " << status << std::endl;
    ALOGE("%s", ss.str().c_str());
  }
  return status;
}

void SyntiantFakeVoiceAssistant::detach() {
  st_module->detach();
}

void SyntiantFakeVoiceAssistant::onRecognitionEvent(struct sound_trigger_recognition_event* event) {
  AutoMutex lock(mLock);
  mRecognitionEventHappened = true;

  struct sound_trigger_phrase_recognition_event* phrase_event =
      (struct sound_trigger_phrase_recognition_event*)event;

  std::stringstream ss;
  ss << "Phrase Event detected !" << std::endl;
  ss << "common.status = " << phrase_event->common.status << std::endl;
  ss << "common.type = " << static_cast<int>(phrase_event->common.type) << std::endl;
  ss << "common.model = " << phrase_event->common.model << std::endl;
  ss << "common.capture_available = " << phrase_event->common.capture_available << std::endl;
  ss << "common.capture_session = " << phrase_event->common.capture_session << std::endl;
  ss << "common.capture_delay_ms = " << phrase_event->common.capture_delay_ms << std::endl;
  ss << "common.capture_preamble_ms = " << phrase_event->common.capture_preamble_ms << std::endl;
  ss << "common.trigger_in_data = " << phrase_event->common.trigger_in_data << std::endl;
  ss << "common.audio_config.channel_mask = " << phrase_event->common.audio_config.channel_mask
     << std::endl;
  ss << "common.audio_config.sample_rate = " << phrase_event->common.audio_config.sample_rate
     << std::endl;
  ss << "common.audio_config.frame_count = " << phrase_event->common.audio_config.frame_count
     << std::endl;
  ss << "common.audio_config.format = "
     << static_cast<unsigned long>(phrase_event->common.audio_config.format) << std::endl;
  ss << "common.data_size = " << phrase_event->common.data_size << std::endl;
  ss << "common.data_offset = " << phrase_event->common.data_offset << std::endl;

  ss << "num_phrases = " << phrase_event->num_phrases << std::endl;

  for (unsigned int i = 0; i < phrase_event->num_phrases; i++) {
    ss << "phrase_extras[" << i
       << "].confidence_level = " << phrase_event->phrase_extras[i].confidence_level << std::endl;
    ss << "phrase_extras[" << i << "].id = " << phrase_event->phrase_extras[i].id << std::endl;
    ss << "phrase_extras[" << i
       << "].recognition_modes = " << phrase_event->phrase_extras[i].recognition_modes << std::endl;
    ss << "phrase_extras[" << i << "].num_levels = " << phrase_event->phrase_extras[i].num_levels
       << std::endl;
    if (phrase_event->phrase_extras[i].num_levels > 0) {
      for (unsigned int j = 0; j < phrase_event->phrase_extras[i].num_levels; j++) {
        ss << "  levels[" << j << "].user_id = " << phrase_event->phrase_extras[i].levels[j].user_id
           << std::endl;
        ss << "  levels[" << j << "].level = " << phrase_event->phrase_extras[i].levels[j].level
           << std::endl;
      }
    }
  }

  ALOGI("%s", ss.str().c_str());

  if (!mShortOutput) {
    std::cout << ss.str().c_str() << std::endl;
  } else {
    struct timespec ts;
    struct tm* tm;
    char time[128];

    clock_gettime(CLOCK_REALTIME, &ts);
    tm = localtime(&ts.tv_sec);
    strftime(time, sizeof(time), "%Y-%m-%d %H:%M:%S", tm);

    std::cout << time << " match -> " << mPhrases[phrase_event->phrase_extras[0].id];
    if (phrase_event->phrase_extras[0].recognition_modes & RECOGNITION_MODE_USER_IDENTIFICATION) {
      std::cout << " -- Known User";
    }
    std::cout << std::endl;

    if (mEnrollEnabled) {
      struct syntiant_ndp10x_recognition_event_opaque_s* p = NULL;
      p = (struct syntiant_ndp10x_recognition_event_opaque_s*)((uint8_t*)phrase_event +
                                                               phrase_event->common.data_offset);

      unsigned int len = p->u.opaque_v1.audio_data_len * sizeof(uint16_t);
      uint8_t* data = (uint8_t*)&p->u.opaque_v1.audio_data[0];

      mSpeakerIDEnroller.add(data, len, phrase_event->phrase_extras[0].confidence_level);
    }
  }

  capture_session = (audio_session_t)event->capture_session;
  ALOGI("%s : capture session %d", __func__, event->capture_session);
}

void SyntiantFakeVoiceAssistant::onSoundModelEvent(struct sound_trigger_model_event* event) {
  AutoMutex lock(mLock);
  mSoundModelEventHappened = true;
  memcpy(&mSoundModelEvent, event, sizeof(mSoundModelEvent));
  ALOGI("%s ", __func__);
}

void SyntiantFakeVoiceAssistant::onServiceStateChange(sound_trigger_service_state_t state) {
  AutoMutex lock(mLock);
  mServiceState = state;
  ALOGI("%s ", __func__);
}

void SyntiantFakeVoiceAssistant::onServiceDied() {
  std::stringstream ss;
  ss << "Sound Trigger Service Died " << std::endl;
  ALOGE("%s", ss.str().c_str());
}

int SyntiantFakeVoiceAssistant::record(void) {
  audio_source_t source = AUDIO_SOURCE_VOICE_RECOGNITION;
  SNDFILE* file = nullptr;
  SyntiantAudioRecordSession session;
  SF_INFO sfinfo;
  unsigned int format;
  uint32_t sampleRate = 16000;
  size_t bits = 16;
  size_t channels = 1;
  int recTime = 0;
  const size_t NUM_FRAMES = 160;
  const size_t NUM_SAMPLES = NUM_FRAMES * channels;
  short samples[NUM_SAMPLES];
  int status = 0;
  std::stringstream ss;

  unsigned int num_samples_to_read = record_len_ms * sampleRate/1000;

  memset(&sfinfo, 0, sizeof(sfinfo));
  memset(samples, 0, sizeof(sizeof(short) * NUM_SAMPLES));

  format = (bits == 16) ? SF_FORMAT_PCM_16 : SF_FORMAT_PCM_32;

  sfinfo.samplerate = sampleRate;
  sfinfo.frames = NUM_FRAMES;
  sfinfo.channels = channels;
  sfinfo.format = (SF_FORMAT_WAV | format);

  //std::cout << "  record() : capture session " << (long unsigned int)capture_session << std::endl;

  status = session.start(sampleRate, channels, bits, capture_session, source);
  if (status) {
    std::cerr << "Some error occured while opening AudioSession" << std::endl;
    goto exit;
  }

  ss << wav_prefix << "_" << dump_counter++ << ".wav";

  std::cout << "  Writing audio to " << ss.str() << std::endl;

  file = sf_open(ss.str().c_str(), SFM_WRITE, &sfinfo);
  if (!file) {
    std::cerr << "Error while opening wav file " << std::endl;
    goto exit;
  }

  recTime = time(0) + record_len_ms / 1000;

  while (num_samples_to_read) {
    status = session.read(samples, sizeof(short) * NUM_SAMPLES);
    if (status < 0) {
      std::cerr << "Some error occured while reading AudioSession " << status << std::endl;
      goto exit;
    }

    if (sf_writef_short(file, samples, NUM_SAMPLES) != (int)NUM_SAMPLES) {
      std::cerr << "Some error occured while writing audio file" << std::endl;
      goto exit;
    }
    num_samples_to_read -= NUM_SAMPLES;

    /* bail out if something seems to take an awful long time.
     * [5 secs longer than is should is a long time...] */
    if (time(0) > (5 + recTime)) {
      std::cerr << "Some error occured while reading samples" << std::endl;
      break;
    }
  }
  session.stop();

exit:
  if (file) {
    sf_close(file);
    file = NULL;
  }
  return status;
}

int SyntiantFakeVoiceAssistant::run() {
  sound_model_handle_t sm_handle = -1;
  int status = 0;
  uint8_t* buffer = NULL;
  uint32_t buffer_size = 0;
  FILE* fp = NULL;
  bool first_round = true;

  status = listModules();
  if (status) goto exit;

  status = attach();
  if (status) goto exit;

  status = setCaptureState(true);
  if (status) {
     goto exit;
  }

  sm_handle = loadSoundModel(modelFile);
  if (sm_handle < 0) {
    status = -1;
    goto exit;
  }

  {
    AutoMutex lock(mLock);
    mRecognitionEventHappened = false;
  }

  while (1) {
    status = startRecognition(sm_handle);
    if (status) goto exit;

    /* allow some time for DMIC to settle, so we don't see any artifacts
     * from turning on clk */
    if (first_round && mEnrollEnabled) {
      sleep(3);
      first_round = false;
      std::cout << "Ready to enroll!" << std::endl;
    }

    while (1) {
      AutoMutex lock(mLock);
      if (mRecognitionEventHappened) {
        std::stringstream ss;
        ss << "Recognition Event Happened " << std::endl;
        ALOGE("%s", ss.str().c_str());
        if (!mShortOutput) {
          std::cout << ss.str();
        }
        mRecognitionEventHappened = false;

        /* attempt to train */
        if (mEnrollEnabled) {
          int res = mSpeakerIDEnroller.train(
              (struct sound_trigger_phrase_sound_model*)(mSoundMemory)->pointer(), &buffer,
              &buffer_size, mUserId);
          if (res == SyntiantUserModelEnroller::TRAINING_DONE) {
            std::cout << "----------------------------------------" << std::endl;
            std::cout << "Model training complete!" << std::endl;
            std::cout << "----------------------------------------" << std::endl;
            goto exit;
          }
        }
        break;
      }
      /* prevent using all CPU cycles */
      usleep(10);
    }

    if (record_len_ms) {
      status = this->record();
      if (status < 0) {
        goto exit;
      }
    }
  }

exit:
  if (buffer) {
    ALOGV("%s: free up model buffer\n", __func__);
    fp = fopen(this->savedSMFile.c_str(), "wb");
    if (fp) {
      fwrite(buffer, buffer_size, 1, fp);
      free(buffer);
      buffer = NULL;
      fclose(fp);
      fp = NULL;
    }
  }
  if (sm_handle >= 0) {
    status = unloadSoundModel(sm_handle);
    if (status) {
      std::cerr << "Error unloading sound model " << status << std::endl;
    }

    detach();
  }
  return status;
}

  int SyntiantFakeVoiceAssistant::setCaptureState(bool active) {
    int status = 0;

    status = SoundTrigger::setCaptureState(active);
    if (status) {
      std::stringstream ss;
      std::cout << "setCaptureState() status=" << status << std::endl;
      ALOGE("%s", ss.str().c_str());
    }

    return status;
  }
