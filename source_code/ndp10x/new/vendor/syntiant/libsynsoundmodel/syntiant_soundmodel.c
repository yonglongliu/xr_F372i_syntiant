/*
 * SYNTIANT CONFIDENTIAL
 * _____________________
 *
 *   Copyright (c) 2020 Syntiant Corporation
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

#define LOG_TAG "syntiant_sound_model"

#include <cutils/log.h>
#include <errno.h>
#include <hardware/hardware.h>
#include <hardware/sound_trigger.h>
#include <system/sound_trigger.h>

#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

#include "syntiant_soundmodel.h"
#include "syntiant_st_speaker_id.h"

#include <syngup.h>
#include <arpa/inet.h>
#include <uuid/uuid.h>
#include <dlfcn.h>

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

#define DISABLE_SPEAKER_ID_LIB_STDOUT

#ifdef DISABLE_SPEAKER_ID_LIB_STDOUT
#define STDOUT_WRAP_CALL(A)            \
  do {                                 \
    int bak, new;                      \
    fflush(stdout);                    \
    bak = dup(1);                      \
    new = open("/dev/null", O_WRONLY); \
    dup2(new, 1);                      \
    close(new);                        \
    (A);                               \
    fflush(stdout);                    \
    dup2(bak, 1);                      \
    close(bak);                        \
  } while (0)
#else
#define STDOUT_WRAP_CALL(A) \
  do {                      \
    (A)                     \
  } while (0)
#endif

#include "syntiant_defs.h"

struct speaker_id_lib_if_s {
  void* lib;
  int (*syntiant_st_speaker_id_engine_init)(unsigned int, unsigned int, short, short, void**, int*);
  int (*syntiant_st_speaker_id_engine_uninit)(void*);
  int (*syntiant_st_speaker_id_engine_train_user)(void*, unsigned int, unsigned int, unsigned int,
                                                  short**);
  int (*syntiant_st_speaker_id_engine_get_user_model)(void*, unsigned int, unsigned int, void*);
} speaker_id_if = { NULL };

ssize_t syntiant_st_sound_model_get_size_from_binary_sound_model(uint8_t* binary_sound_model_data,
                                                                 size_t binary_sound_model_size)

{
  ssize_t size;
  ALOGV("%s: enter", __func__);

  if (!binary_sound_model_data || binary_sound_model_size == 0) {
    ALOGE("%s : binary_sound_model_data cannot be NULL", __func__);
    return -EINVAL;
  }

  size = sizeof(struct sound_trigger_phrase_sound_model) + binary_sound_model_size;
  return size;
}

/* Parse the model from the buffer containing file data */
int syntiant_st_sound_model_build_from_binary_sound_model(uint8_t* binary_sound_model_data,
                                                          uint32_t binary_sound_model_size,
                                                          uint8_t* dest_memory) {
  int num_keyphrases = 1;
  int rc = 0;
  uint8_t i;
  unsigned int j;
  ssize_t str_len;
  uint32_t sound_model_size;
  struct syngup_package package;
  struct syn_pkg* pkg = NULL;
  struct syn_pkg* user_pkg = NULL;
  struct synpkg_metadata* mdata;
  struct phrase* p;

  if (!dest_memory) {
    return -EINVAL;
  }
  rc = syngup_extract_blobs(&package, binary_sound_model_data, binary_sound_model_size);
  if (rc) {
    ALOGE("Can not extract blobs from  syngup\n");
    return rc;
  }
  ALOGV("synpkg extracted\n");
  rc = syngup_get_synpkg(&package, &pkg);
  if (rc) {
    ALOGE("Can not find synpkg in syngup\n");
    syngup_cleanup(&package);
    return rc;
  }
  ALOGV("synpkg: %u\n", pkg->size);
  mdata = pkg->mdata;

  rc = syngup_get_sound_package(&package, &user_pkg);
  if (rc) {
    ALOGE("Can not find user synpkg in syngup\n");
    rc = 0; /* this is not an error to propagate to user */
  } else {
    ALOGV("%s : ** Data len %d **\n", __func__, user_pkg->size);
    /* sanity check: user model should probably
     * not be larger than 2x the compressed size */
    if ((user_pkg->size > 0) && (user_pkg->size < 2*binary_sound_model_size)) {
      mdata = user_pkg->mdata;
    } else {
      ALOGW("%s : Bogus user package size found\n", __func__);
    }
  }

  struct sound_trigger_phrase_sound_model* phraseSoundModel =
      (struct sound_trigger_phrase_sound_model*)(dest_memory);

  memcpy(&phraseSoundModel->common.uuid, mdata->model_uuid, sizeof(phraseSoundModel->common.uuid));
  phraseSoundModel->common.uuid.timeLow = ntohl(phraseSoundModel->common.uuid.timeLow);
  phraseSoundModel->common.uuid.timeMid = ntohs(phraseSoundModel->common.uuid.timeMid);
  phraseSoundModel->common.uuid.timeHiAndVersion = ntohs(phraseSoundModel->common.uuid.timeHiAndVersion);
  memcpy(&phraseSoundModel->common.vendor_uuid, mdata->vendor_uuid,
         sizeof(phraseSoundModel->common.vendor_uuid));
  phraseSoundModel->common.data_size = binary_sound_model_size;
  phraseSoundModel->common.data_offset = sizeof(struct sound_trigger_phrase_sound_model);
  phraseSoundModel->num_phrases = mdata->num_phrases;

  phraseSoundModel->common.type = SOUND_MODEL_TYPE_KEYPHRASE;
  phraseSoundModel->num_phrases = mdata->num_phrases;
  /* Copy over metadata */
  ALOGV("%s Copy over phrases metadata of the model\n", __func__);
  for (i = 0; i < mdata->num_phrases; i++) {
    p = (struct phrase*)mdata->phrases;
    phraseSoundModel->phrases[i].recognition_mode = p->recog_mode;
    phraseSoundModel->phrases[i].num_users = p->num_users;
    for (j = 0; j < phraseSoundModel->phrases[i].num_users; j++) {
      phraseSoundModel->phrases[i].users[j] = p->users[j];
    }
    str_len = strlen(p->text) > SOUND_TRIGGER_MAX_STRING_LEN ? SOUND_TRIGGER_MAX_STRING_LEN - 1
                                                             : strlen(p->text);
    memcpy(phraseSoundModel->phrases[i].text, p->text, str_len);
    phraseSoundModel->phrases[i].text[str_len] = '\0';
    str_len = strlen(p->locale) > SOUND_TRIGGER_MAX_LOCALE_LEN ? SOUND_TRIGGER_MAX_LOCALE_LEN - 1
                                                               : strlen(p->locale);
    memcpy(phraseSoundModel->phrases[i].locale, p->locale, str_len);
    phraseSoundModel->phrases[i].locale[str_len] = '\0';
    p = (struct phrase*)((uint8_t*)p + sizeof(*p));
    ALOGV("Phrase: text:%s locale:%s\n", phraseSoundModel->phrases[i].text,
          phraseSoundModel->phrases[i].locale);
  }
  /* copy over compressed file data to caller's buffer */
  memcpy((char*)phraseSoundModel + phraseSoundModel->common.data_offset, binary_sound_model_data,
         binary_sound_model_size);

  syngup_cleanup(&package);

  return rc;
}

int syntiant_speaker_id_if_init(void)
{
  #define SPKRID_LIB_PATH_1 "/vendor/lib/libmeeami_spkrid.so"
  #define SPKRID_LIB_PATH_2 "/system/vendor/lib/libmeeami_spkrid.so"

  speaker_id_if.lib = dlopen(SPKRID_LIB_PATH_1, RTLD_NOW);
  if (speaker_id_if.lib == NULL) {
    speaker_id_if.lib = dlopen(SPKRID_LIB_PATH_2, RTLD_NOW);
    if (speaker_id_if.lib == NULL) {
      ALOGE("%s: DLOPEN failed for speaker ID library - %s\n"  , __func__,
            dlerror());
      return -ENOENT;
    }
  }

  speaker_id_if.syntiant_st_speaker_id_engine_init =
      dlsym(speaker_id_if.lib, "syntiant_st_speaker_id_engine_init");
  if (!speaker_id_if.syntiant_st_speaker_id_engine_init) {
    ALOGE("%s: DLOPEN error locating syntiant_st_speaker_id_engine_init function"
          " -  %s\n", __func__, dlerror());
    return -EINVAL;
  }

  speaker_id_if.syntiant_st_speaker_id_engine_uninit =
      dlsym(speaker_id_if.lib, "syntiant_st_speaker_id_engine_uninit");
  if (!speaker_id_if.syntiant_st_speaker_id_engine_uninit) {
    ALOGE("%s: DLOPEN error locating syntiant_st_speaker_id_engine_uninit function"
          " -  %s\n", __func__, dlerror());
    return -EINVAL;
  }

  speaker_id_if.syntiant_st_speaker_id_engine_train_user =
      dlsym(speaker_id_if.lib, "syntiant_st_speaker_id_engine_train_user");
  if (!speaker_id_if.syntiant_st_speaker_id_engine_train_user) {
    ALOGE("%s: DLOPEN error locating syntiant_st_speaker_id_engine_train_user function"
          " -  %s\n", __func__, dlerror());
    return -EINVAL;
  }

  speaker_id_if.syntiant_st_speaker_id_engine_get_user_model =
      dlsym(speaker_id_if.lib, "syntiant_st_speaker_id_engine_get_user_model");
  if (!speaker_id_if.syntiant_st_speaker_id_engine_get_user_model) {
    ALOGE("%s: DLOPEN error locating syntiant_st_speaker_id_engine_get_user_model function"
          " -  %s\n", __func__, dlerror());
    return -EINVAL;
  }

  return 0;
}

void syntiant_speaker_id_if_uninit(void)
{
  int rv;
  if (speaker_id_if.lib) {
    rv = dlclose(speaker_id_if.lib);
    if (rv) {
      ALOGE("%s: DLCLOSE failed for speaker ID library - rv=%d (%s)\n"  , __func__,
            rv, dlerror());
    }
  }
  speaker_id_if.lib = NULL;
}

ssize_t syntiant_st_sound_model_get_size_when_extended(
    struct sound_trigger_phrase_sound_model* phrase_sound_model) {
  ssize_t s = 0;
  int* spkr_id_engine = NULL;
  int model_size = 0;
  int ret = 0;
  uint8_t* buffer;
  uint32_t buffer_size, mdata_size;
  struct syngup_package package;
  int s0;

  if (!phrase_sound_model) {
    return -EINVAL;
  }
  
  s0 = syntiant_speaker_id_if_init();
  if (s0) {
    ALOGE("%s : Error loading speaker ID library %d\n", __func__, s0);
    s = -EINVAL;
    goto exit;
  }

  STDOUT_WRAP_CALL(s0 = speaker_id_if.syntiant_st_speaker_id_engine_init(1, SYNTIANT_SPEAKER_ID_WW_LEN,
                                                          SYNTIANT_SPEAKER_ID_DEFAULT_OPT_CNT,
                                                          SYNTIANT_SPEAKER_ID_DEFAULT_UTTER_CNT,
                                                          (void**)&spkr_id_engine, &model_size));

  if (s0 != SYNTIANT_ST_SPEAKER_ID_ERROR_NONE) {
    ALOGE("%s : Error initializing speaker ID library %d", __func__, s0);
    s = -EINVAL;
    goto exit;
  }
  ALOGV("%s: speaker id model size: %d\n", __func__, model_size);
  buffer = (uint8_t*)phrase_sound_model + phrase_sound_model->common.data_offset;
  ALOGV("%s Extract blobs from syngup\n", __func__);
  ret = syngup_extract_blobs(&package, buffer, phrase_sound_model->common.data_size);
  if (ret) {
    ALOGE("Could not extract blobs from syngup: %d\n", ret);
    goto exit;
  }

  mdata_size = sizeof(struct synpkg_metadata) + sizeof(struct phrase);
  /* Get the total size of uncompressed blobs when a new model of size
     model_size and having a metadata of size mdata_size is added to syngup
   */
  s = syngup_get_total_size(&package, model_size, mdata_size);
  ALOGV("%s: total size of blob: %u\n", __func__, s);
  syngup_cleanup(&package);

  s = sizeof(struct sound_trigger_phrase_sound_model) + s;

exit:
  if (spkr_id_engine) {
    STDOUT_WRAP_CALL(speaker_id_if.syntiant_st_speaker_id_engine_uninit(spkr_id_engine));
  }

  syntiant_speaker_id_if_uninit();

  return s;
}

int syntiant_st_sound_model_get_enrollment_recording_requirements(
    struct sound_trigger_phrase_sound_model* phrase_sound_model, unsigned int* num_recordings,
    audio_config_t* audio_config) {
  int s = 0;
  if (!phrase_sound_model || !num_recordings || !audio_config) {
    return -EINVAL;
  }

  *num_recordings = SYNTIANT_SPEAKER_ID_DEFAULT_UTTER_CNT;
  *audio_config = AUDIO_CONFIG_INITIALIZER;

  audio_config->sample_rate = 16000;
  audio_config->channel_mask = audio_channel_in_mask_from_count(1);
  audio_config->format = AUDIO_FORMAT_PCM_16_BIT;
  audio_config->frame_count = SYNTIANT_SPEAKER_ID_WW_LEN;
  return 0;
}

int check_audio_config(audio_config_t* audio_config) {
  // TODO: Implement this
  return 0;
}

int syntiant_st_sound_model_extend(
    struct sound_trigger_phrase_sound_model* source_sm, unsigned int user_id,
    size_t num_enrollment_recordings,
    struct syntiant_st_sound_model_enrollment_recording_s* recordings, uint8_t** destination_sm,
    uint32_t* dest_model_size) {
  int* spkr_id_engine = NULL;
  int model_size = 0;
  int s = 0;
  size_t i = 0;
  uint8_t* buffer;
  uint8_t gen_uuid[UUID_LENGTH];

  uint8_t* spkr_id_user_model = NULL;
  ssize_t expected_dest_model_size = 0;
  short* recording_audio[SYNTIANT_SPEAKER_ID_DEFAULT_UTTER_CNT];
  struct synpkg_metadata* mdata = NULL;
  struct synpkg_metadata* synpkg_mdata;
  struct phrase* ph = NULL;
  struct syngup_package package;
  struct syn_pkg* pkg;

  ALOGV("%s : enter", __func__);

  if (!source_sm || !*destination_sm) {
    return -EINVAL;
  }

  buffer = (uint8_t*)source_sm + source_sm->common.data_offset;

  if (num_enrollment_recordings != SYNTIANT_SPEAKER_ID_DEFAULT_UTTER_CNT) {
    ALOGE("%s : num_enrollment_recording != %d", __func__, SYNTIANT_SPEAKER_ID_DEFAULT_UTTER_CNT);
    return -EINVAL;
  }

  for (i = 0; i < num_enrollment_recordings; i++) {
    s = check_audio_config(&recordings[i].audio_config);
    if (s < 0) {
      ALOGE("%s : audio config mismatch", __func__);
      return s;
    }
    recording_audio[i] = (short*)malloc(SYNTIANT_SPEAKER_ID_WW_LEN * sizeof(short));
    if (!recording_audio[i]) {
      ALOGE("%s : Error allocating memory for recordings", __func__);
      return -ENOMEM;
    }
    memcpy(recording_audio[i], recordings[i].audio_data,
           SYNTIANT_SPEAKER_ID_WW_LEN * sizeof(short));
  }
  s = syntiant_speaker_id_if_init();
  if (s) {
    ALOGE("%s : Error loading speaker ID library %d\n", __func__, s);
    s = -EINVAL;
    goto out;
  }

  ALOGV("%s: Init speaker id engine\n", __func__);
  STDOUT_WRAP_CALL(s = speaker_id_if.syntiant_st_speaker_id_engine_init(1, SYNTIANT_SPEAKER_ID_WW_LEN,
                                                          SYNTIANT_SPEAKER_ID_DEFAULT_OPT_CNT,
                                                          SYNTIANT_SPEAKER_ID_DEFAULT_UTTER_CNT,
                                                          (void**)&spkr_id_engine, &model_size));

  if (s != SYNTIANT_ST_SPEAKER_ID_ERROR_NONE) {
    ALOGE("%s : Error initializing speaker ID library %d\n", __func__, s);
    s = -EINVAL;
    goto out;
  }

  ALOGV("%s : training user model - user id %d, size:%u\n", __func__, user_id, model_size);
  STDOUT_WRAP_CALL(s = speaker_id_if.syntiant_st_speaker_id_engine_train_user(
                       spkr_id_engine, user_id, SYNTIANT_SPEAKER_ID_DEFAULT_UTTER_CNT,
                       SYNTIANT_SPEAKER_ID_WW_LEN, recording_audio));
  ALOGV("%s : Done training user model", __func__);

  for (i = 0; i < num_enrollment_recordings; i++) {
    free(recording_audio[i]);
  }

  if (s == SYNTIANT_ST_SPEAKER_ID_ERROR_NEED_MORE_AUDIO)
    ALOGE("%s : Need more number of utterances.", __func__);
  else if (s == SYNTIANT_ST_SPEAKER_ID_ERROR_NEED_LONGER_AUDIO)
    ALOGE("%s : Audio data provided is not sufficient.", __func__);
  else if (s == SYNTIANT_ST_SPEAKER_ID_ERROR_MODEL_USER_ALREADY_EXISTS)
    ALOGE("%s : user_id already exists.", __func__);
  else if (s == SYNTIANT_ST_SPEAKER_ID_ERROR_MODEL_TOO_MANY_USERS)
    ALOGE("%s : Exceeded the number of users supported.", __func__);
  else if (s == SYNTIANT_ST_SPEAKER_ID_ERROR_NONE)
    ALOGE("%s : Enrollment completed.", __func__);

  if (s != SYNTIANT_ST_SPEAKER_ID_ERROR_NONE) {
    s = -EINVAL;
    goto out;
  }

  spkr_id_user_model = malloc(model_size);
  if (!spkr_id_user_model) {
    ALOGE("%s : Error allocating memory for user model", __func__);
    s = -ENOMEM;
    goto out;
  }

  STDOUT_WRAP_CALL(s = speaker_id_if.syntiant_st_speaker_id_engine_get_user_model(
                       spkr_id_engine, user_id, model_size, spkr_id_user_model));
  if (s != SYNTIANT_ST_SPEAKER_ID_ERROR_NONE) {
    ALOGE("%s : Error obtaining trained model err = %d", __func__, s);
    s = -EINVAL;
    goto out;
  }

  s = syngup_extract_blobs(&package, buffer, source_sm->common.data_size);
  if (s) {
    ALOGE("%s : Could not extract blobs from synpkg\n", __func__);
    goto out;
  }

  /* get existing meta data */
  s = syngup_get_synpkg(&package, &pkg);
  if (s) {
    ALOGE("%s : Can not find synpkg in syngup", __func__);
    free(buffer);
    syngup_cleanup(&package);
    return s;
  }
  ALOGV("%s : Found synpkg: %u\n", __func__, pkg->size);
  synpkg_mdata = pkg->mdata;

  /* Add the metadata to the model */
  mdata = malloc(sizeof(*mdata) + sizeof(*ph));
  if (!mdata) {
    ALOGE("%s : Can not allocate memory for speaker id metadata", __func__);
    s = -ENOMEM;
    goto out2;
  }

  /* TODO: Figure out the correct approach here.
   *       In the code below:
   *       add the same meta data as found in the original synpkg, but with different UUID
   *       and with num_users = 1
   *      ... figure out a good way to genrate the new UUID
   *
   * */
  uuid_generate_time(gen_uuid);
  memset(mdata, 0, sizeof(*mdata) + sizeof(*ph));
  memcpy(mdata, synpkg_mdata, sizeof(*synpkg_mdata) + sizeof(*ph));
  memcpy(mdata->model_uuid, gen_uuid, ARRAY_SIZE(gen_uuid));
  ALOGV("%s set phrases", __func__);

  ph = (struct phrase*)((uint8_t*)mdata + offsetof(struct synpkg_metadata, phrases));
  ph->recog_mode = RECOGNITION_MODE_VOICE_TRIGGER | RECOGNITION_MODE_USER_IDENTIFICATION;
  ph->num_users = 1;
  ph->users[0] = user_id;

  ALOGV("%s : Add speaker id model to syngup", __func__);

  s = syngup_add_component(&package, spkr_id_user_model, model_size, (uint8_t*)mdata,
                           sizeof(*mdata) + sizeof(*ph));
  if (s) {
    ALOGE("%s : Failed to add speaker id model to syngup (%d)", __func__, s);
    goto out2;
  }
  ALOGV("%s : pack the blobs now", __func__);
  s = syngup_pack_blobs(&package, *destination_sm);
  ALOGV("%s : pack result %d", __func__, s);
  if (s > 0) {
    *dest_model_size = s;
  } else {
    ALOGE("%s : packing models into a syngup failed", __func__);
    *dest_model_size = 0;
    s = -EINVAL;
    goto out2;
  }

out2:
  syngup_cleanup(&package);

out:
  if (mdata) {
    free(mdata);
  }
  if (spkr_id_engine) {
    STDOUT_WRAP_CALL(speaker_id_if.syntiant_st_speaker_id_engine_uninit(spkr_id_engine));
  }
  syntiant_speaker_id_if_uninit();
  if (spkr_id_user_model) {
    free(spkr_id_user_model);
  }

  ALOGV("%s : exit", __func__);
  return s;
}

/* TODO: Verify compatiility layer for KaiOS initial implementation */
int ExtendSoundModel_Samples(struct sound_trigger_phrase_sound_model* model,
                             int16_t* samples[SYNTIANT_SPEAKER_ID_NUM_ENROLLMENT_SAMPLES],
                             struct syntiant_user_sound_model_s** dest_model, int user_id) {
  uint32_t model_size;
  uint8_t* dest_buffer = NULL;

  struct syntiant_st_sound_model_enrollment_recording_s
      recordings[SYNTIANT_SPEAKER_ID_NUM_ENROLLMENT_SAMPLES];
  int res = SYNTIANT_ERROR_NONE;
  unsigned int i;

  ALOGV("%s : enter", __func__);

  if (!dest_model) {
    ALOGE("%s : Error, dest_model is NULL", __func__);
    ALOGE("%s : exit", __func__);
    return SYNTIANT_ERROR_FAIL;
  }

  ALOGV("%s : syntiant Get model size", __func__);
  model_size = syntiant_st_sound_model_get_size_when_extended(model);
  ALOGV("%s : syntiant alloc buffer (%d bytes) for saving model", __func__, model_size);

  dest_buffer = (uint8_t*)malloc(model_size);
  if (!dest_buffer) {
    ALOGE("%s : Could not allocate %d bytes of memory for buffer", __func__, model_size);
    return SYNTIANT_ERROR_FAIL;
  }
  memset(dest_buffer, 0, model_size);

  for (i = 0; i < SYNTIANT_SPEAKER_ID_NUM_ENROLLMENT_SAMPLES; i++) {
    if (samples[i]) {
      memcpy(&recordings[i].audio_data, samples[i], SYNTIANT_SPEAKER_ID_WW_LEN * sizeof(short));
    } else {
      ALOGE("%s : No samples supplied at index %d", __func__, i);
      res = SYNTIANT_ERROR_FAIL;
    }
  }
  ALOGV("%s : Extend the syngup model, expected size:%u", __func__, model_size);
  res = syntiant_st_sound_model_extend(model, user_id, SYNTIANT_SPEAKER_ID_NUM_ENROLLMENT_SAMPLES,
                                       recordings, &dest_buffer, &model_size);
  if (res < 0) {
    res = SYNTIANT_ERROR_FAIL;
  } else {
    res = 0; /* non-negative value here signifies success */
    /* set the final size of the buffer */
    ALOGV("%s : After compaction: %u", __func__, model_size);

    *dest_model = (struct syntiant_user_sound_model_s*)malloc(
        sizeof(struct syntiant_user_sound_model_s) + model_size);
    memcpy((*dest_model)->model, dest_buffer, model_size);
    (*dest_model)->model_size = model_size;
  }

  free(dest_buffer);

  ALOGV("%s: exit res=%d", __func__, res);

  return res;
}

int ExtendSoundModel(struct sound_trigger_phrase_sound_model* model,
                     char* sample_filenames[SYNTIANT_SPEAKER_ID_NUM_ENROLLMENT_SAMPLES],
                     struct syntiant_user_sound_model_s** dest_model, int user_id) {
  int i;
  FILE* enrollfp = NULL;
  int n;
  int ret_val = SYNTIANT_ERROR_NONE;
  short int* ipbuf[SYNTIANT_SPEAKER_ID_NUM_ENROLLMENT_SAMPLES] = { NULL };
  ALOGV("%s : enter", __func__);

  for (i = 0; i < SYNTIANT_SPEAKER_ID_NUM_ENROLLMENT_SAMPLES; i++) {
    if (!sample_filenames[i]) {
      ALOGE("%s : No file name supplied at index %d", __func__, i);
      ret_val = SYNTIANT_ERROR_FAIL;
      goto out;
    }
    enrollfp = fopen(sample_filenames[i], "rb");
    if (!enrollfp) {
      ALOGE("%s : Error locating file %s", __func__, sample_filenames[i]);
      ret_val = SYNTIANT_ERROR_FAIL;
      goto out;
    }

    ipbuf[i] = malloc(sizeof(short) * SYNTIANT_SPEAKER_ID_WW_LEN);
    if (!ipbuf[i]) {
      ALOGE("%s : Error allocating mem", __func__);
      ret_val = SYNTIANT_ERROR_FAIL;
      goto out;
    } else {
      ALOGV("%s : Allocated %d bytes of mem for sample #%d", __func__,
            sizeof(short) * SYNTIANT_SPEAKER_ID_WW_LEN, i);
    }

    fseek(enrollfp, 0, SEEK_END);
    if ((unsigned long)ftell(enrollfp) >= SYNTIANT_SPEAKER_ID_WW_LEN * sizeof(short)) {
      fseek(enrollfp, -1 * SYNTIANT_SPEAKER_ID_WW_LEN * sizeof(short), SEEK_END);
    } else {
      rewind(enrollfp);
    }
    n = fread(ipbuf[i], sizeof(short), SYNTIANT_SPEAKER_ID_WW_LEN, enrollfp);
    ALOGV("%s : %d bytes read from training file", __func__, n * sizeof(short));
    fclose(enrollfp);
  }

  ret_val = ExtendSoundModel_Samples(model, ipbuf, dest_model, user_id);

out:
  for (i = 0; i < SYNTIANT_SPEAKER_ID_NUM_ENROLLMENT_SAMPLES; i++) {
    if (ipbuf[i]) {
      free(ipbuf[i]);
    }
  }

  ALOGV("%s : exit - ret_val=%d", __func__, ret_val);
  return ret_val;
}

unsigned int syntiant_get_keyphrase_sound_model_size(uint8_t* file_data, size_t file_size) {
  ALOGV("%s: enter", __func__);
  return syntiant_st_sound_model_get_size_from_binary_sound_model(file_data, file_size);
}

int syntiant_parse_sound_model(struct sound_trigger_phrase_sound_model* dest_model,
                               uint8_t* file_data, uint32_t file_size) {
  ALOGV("%s: enter", __func__);
  return syntiant_st_sound_model_build_from_binary_sound_model(file_data, file_size,
                                                               (uint8_t*)dest_model);
}