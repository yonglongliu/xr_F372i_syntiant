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
#include <cutils/properties.h>

#include "ndp10x_hal.h"
#include "sound_trigger_ndp10x.h"
#include "sound_trigger_ndp10x_internal.h"

#define MAX_STRING_LENGTH 256

/* Enable PERFORM_RECOGNITION_MODE_CHECK #define if you want checks for recognition_modes
 * if enabled, and if recognition_modes = 2, code will _only_ deliver callbacks when speaker ID validation
 * passes */
//#define PERFORM_RECOGNITION_MODE_CHECK

/* engineering mode support - default is to only send callback for speacker ID models if speaker
 * ID matches - with engineering mode enabled via property, we also deliver a callback when
 * speaker ID does not match */
#define PERFORM_RECOGNITION_MODE_CHECK_ENG_MODE

static void stdev_ndp10x_watch_thread_sig(int sig, siginfo_t* info, void* ucontext) {
  (void)sig;
  (void)info;
  (void)ucontext;
  ALOGV("%s: received signal %d", __func__, sig);
  return;
}

#define DEFAULT_MAX_NUMBER_OF_DUMP_FILES 20
static void dump_activation_to_file(short* samples, size_t num_samples, char* prefix, int inc_counter) {
  FILE* fp = NULL;
  static size_t activation_counter = 0;
  char activation_record_filename[MAX_STRING_LENGTH];
  size_t bytes_written = 0;
  int num_dump_files = property_get_int32("syntiant.sthal.dump_files", DEFAULT_MAX_NUMBER_OF_DUMP_FILES);
  if (num_dump_files <= 0) {
    num_dump_files = DEFAULT_MAX_NUMBER_OF_DUMP_FILES;
  }

  snprintf(activation_record_filename, MAX_STRING_LENGTH, "/data/aov/%s_%zu.raw", prefix,
           activation_counter % num_dump_files);
  if (inc_counter) {
    activation_counter++;
  }

  ALOGI("%s: Now dumping %d bytes of audio to %s", __func__, num_samples * sizeof(short),
        activation_record_filename);
  fp = fopen(activation_record_filename, "wb");

  if (fp == NULL) {
    ALOGE("%s : error opening %s file", __func__, activation_record_filename);
    return;
  }

  bytes_written = fwrite(samples, 1, num_samples * sizeof(short), fp);

  if (bytes_written != num_samples * sizeof(short)) {
    ALOGE("%s : error only %zu bytes written instead of %zu", __func__, bytes_written,
          num_samples * sizeof(short));
  }

  fclose(fp);
}

void* stdev_ndp10x_watch_thread_loop(void* p) {
  struct syntiant_ndp10x_stdev* stdev = (struct syntiant_ndp10x_stdev*)p;
  int s = 0;
  short samples[ACTIVATION_RECORD_NUM_SAMPLES];
  size_t num_samples = ACTIVATION_RECORD_NUM_SAMPLES;
  int opaque_size = 0;
  struct sound_trigger_phrase_recognition_event* event;

  int is_any_model_loaded = 0;
  int keyphrase_id = -1;
  unsigned int recognition_modes = 0;
  unsigned int current_mode = 0;

  unsigned int user_id = -1;
  int confidence_score = 0;
  bool user_detected = 0;
  int inc_num_act = 0;

  struct sigaction sa;

  unsigned int match_additional_info;

  ALOGV("%s starting watch thread", __func__);

  sa.sa_handler = NULL;
  sa.sa_sigaction = stdev_ndp10x_watch_thread_sig;
  sa.sa_flags = SA_SIGINFO;
  sigemptyset(&sa.sa_mask);

  if (sigaction(SIGUSR1, &sa, NULL) < 0) {
    ALOGE("%s: error settting watch thread sigaction", __func__);
  }

  while (1) {
    pthread_mutex_lock(&stdev->lock);

    is_any_model_loaded = stdev->is_any_model_loaded;
    keyphrase_id = stdev->keyphrase_id;
    recognition_modes = stdev->recognition_modes;
    current_mode = stdev->current_mode;

    pthread_mutex_unlock(&stdev->lock);

    /* TODO:
    * would be a better approach to move to condition variable to
    * trigger checks of the following instead of periodic checks via sleeps */
    if (!is_any_model_loaded) {
      sleep(1);
      continue;
    }

    if (keyphrase_id != 0 || recognition_modes == 0) {
      usleep(1000);
      continue;
    }

    if (current_mode != STDEV_MODE_WATCHING) {
      usleep(1000);
      continue;
    }

    ALOGE("%s : Going into watch mode ", __func__);

    s = ndp10x_hal_wait_for_match_and_extract(stdev->ndp_handle, keyphrase_id, samples,
                                              num_samples, &match_additional_info);

    if (s < 0) {
      ALOGE("%s : some error occurred while waiting and extracting", __func__);
      continue;
    }

#ifdef ENABLE_ACTIVATION_DUMP
    inc_num_act = 0;
#if defined(ENABLE_SPEAKER_ID_SUPPORT) && defined(ENABLE_SPEAKER_ID_ACTIVATION_DUMP)
    if (recognition_modes & RECOGNITION_MODE_USER_IDENTIFICATION) {
      if (stdev->speaker_id.pSpkrid_handle == NULL || stdev->speaker_id.enrolled_users <= 0) {
        inc_num_act = 1;
      }
    } else {
      inc_num_act = 1;
    }
#else
    inc_num_act = 1;
#endif
    dump_activation_to_file(samples, ACTIVATION_RECORD_NUM_SAMPLES, "sthal_dump", inc_num_act);
#endif
    user_detected = 0;
    pthread_mutex_lock(&stdev->lock);

#ifdef ENABLE_SPEAKER_ID_SUPPORT
    if (recognition_modes & RECOGNITION_MODE_USER_IDENTIFICATION) {
      ALOGV("%s : USER_IDENTIFICATION mode", __func__);
      if (stdev->speaker_id.pSpkrid_handle == NULL || stdev->speaker_id.enrolled_users <= 0) {
        ALOGV("%s : no speaker_id...", __func__);
        goto no_speaker_id;
      }

      unsigned int length_recording = stdev->speaker_id.ww_len;

      size_t sample_shift = ACTIVATION_RECORD_NUM_SAMPLES - stdev->speaker_id.ww_len;
      short* audio_recording = samples + sample_shift;

      ALOGV("%s : verifying user  - SNLR=%d (%.2f)", __func__, match_additional_info & 0xff, (match_additional_info & 0xff) / 8.0);
#ifdef ENABLE_SPEAKER_ID_ACTIVATION_DUMP
      dump_activation_to_file(audio_recording, length_recording, "sthal_speaker_id_dump", 1);
#endif
      s = syntiant_st_speaker_id_engine_identify_user(stdev->speaker_id.pSpkrid_handle,
                                                      length_recording, audio_recording, &user_id,
                                                      &confidence_score, match_additional_info & 0xff);

      if (s != SYNTIANT_ST_SPEAKER_ID_ERROR_NONE) {
        ALOGE("%s : Identifying user result : %d ", __func__, s);
      } else {
        ALOGV("%s : user %d detected", __func__, user_id);
        user_detected = 1;
      }
    }
#endif

  no_speaker_id:

    memcpy(stdev->activation_samples, samples, sizeof(short) * num_samples);
    stdev->activation_samples_currptr = stdev->activation_samples;
    stdev->num_unread_activation_samples = num_samples;

    if (stdev->recognition_callback == NULL) {
      ALOGE("%s : recognition_callback == NULL", __func__);
      stdev->current_mode = STDEV_MODE_RECORDING;
      stdev->keyphrase_id = -1;
      stdev->recognition_modes = 0;

      pthread_mutex_unlock(&stdev->lock);
      continue;
    }


#ifdef PERFORM_RECOGNITION_MODE_CHECK
    if (!user_detected && !(recognition_modes & RECOGNITION_MODE_VOICE_TRIGGER)) {
      ALOGE("%s : no user detected and recognition mode (%d) does not indicate voice_trigger",
            __func__, recognition_modes);

      pthread_mutex_unlock(&stdev->lock);
      continue;
    }
#endif

#ifdef PERFORM_RECOGNITION_MODE_CHECK_ENG_MODE
  {
    int engineering_mode = property_get_int32("syntiant.sthal.eng_mode", 0);
    if (!user_detected && (recognition_modes & RECOGNITION_MODE_USER_IDENTIFICATION) 
        && engineering_mode != 1) {
        ALOGE("%s : Skipping callback - No user detected and recognition mode (%d) contains USER_IDENTIFICATION",
              __func__, recognition_modes);

        pthread_mutex_unlock(&stdev->lock);
        continue;
      }
  }
#endif

    stdev->current_mode = STDEV_MODE_RECORDING;
    stdev->keyphrase_id = -1;
    stdev->recognition_modes = 0;

    opaque_size = 0;
    if (num_samples) {
      opaque_size =
          sizeof(struct syntiant_ndp10x_recognition_event_opaque_s) + num_samples * sizeof(int16_t);
    }

    event = (struct sound_trigger_phrase_recognition_event*)calloc(
        1, sizeof(struct sound_trigger_phrase_recognition_event) + opaque_size);

    event->common.status = RECOGNITION_STATUS_SUCCESS;
    event->common.type = SOUND_MODEL_TYPE_KEYPHRASE;
    event->common.capture_available = 1;
    event->common.capture_delay_ms = -ACTIVATION_RECORD_TIME_MS;
    event->common.model = stdev->model_handle;
    event->common.trigger_in_data = 1;

    event->common.audio_config = AUDIO_CONFIG_INITIALIZER;

    event->common.audio_config.channel_mask = audio_channel_in_mask_from_count(1);
    event->common.audio_config.format = AUDIO_FORMAT_PCM_16_BIT;
    event->common.audio_config.sample_rate = 16000;

    event->common.audio_config.frame_count = ACTIVATION_RECORD_NUM_SAMPLES;
    ALOGE("%s : frame count %u", __func__, event->common.audio_config.frame_count);
    event->num_phrases = 1;
    event->phrase_extras[0].recognition_modes = RECOGNITION_MODE_VOICE_TRIGGER;
    event->phrase_extras[0].confidence_level = 100;
    if (user_detected) {
      event->phrase_extras[0].recognition_modes |= RECOGNITION_MODE_USER_IDENTIFICATION;
      event->phrase_extras[0].num_levels = 1;
      event->phrase_extras[0].levels[0].level = confidence_score;
      event->phrase_extras[0].levels[0].user_id = user_id;
    } else {
      event->phrase_extras[0].num_levels = 0;
    }

    event->common.data_offset = sizeof(struct sound_trigger_phrase_recognition_event);

    if (opaque_size) {
      event->common.data_size = opaque_size;
      struct syntiant_ndp10x_recognition_event_opaque_s* p =
          (struct syntiant_ndp10x_recognition_event_opaque_s*)((uint8_t*)event +
                                                               event->common.data_offset);
      p->version = SYNTIANT_SOUND_TRIGGER_OPAQUE_VERSION_V1;
      p->u.opaque_v1.start_of_keyword_ms = 0;
      p->u.opaque_v1.end_of_keyword_ms = ACTIVATION_RECORD_TIME_MS;
      p->u.opaque_v1.audio_data_len = num_samples;

      memcpy(p->u.opaque_v1.audio_data, samples, num_samples * sizeof(short));

      ALOGV(
          "%s Sending opaque data of size=%d start=%u end=%u "
          "audio_data_len=%d",
          __func__, opaque_size, p->u.opaque_v1.start_of_keyword_ms,
          p->u.opaque_v1.end_of_keyword_ms, p->u.opaque_v1.audio_data_len);
    } else {
      event->common.data_size = 0;
    }

    ALOGV("%s send recognition_callback model %d", __func__, stdev->model_handle);
    stdev->recognition_callback(&event->common, stdev->recognition_cookie);
    free(event);

    pthread_mutex_unlock(&stdev->lock);
  }

  ALOGV("%s stopping watch thread", __func__);
  return p;
}