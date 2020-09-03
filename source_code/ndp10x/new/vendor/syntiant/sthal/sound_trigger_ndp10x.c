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

#define MAX_STRING_LENGTH 256

#include <cutils/log.h>
#include <cutils/properties.h>
#include <hardware/hardware.h>
#include <hardware/sound_trigger.h>
#include <system/sound_trigger.h>

#include "sound_trigger_ndp10x.h"
#include "sound_trigger_ndp10x_internal.h"

#include "ndp10x_hal.h"

#ifdef ENABLE_BARGE_IN_SUPPORT
#include "syntiant_barge_in_c_api.h"
#endif

#include <syngup.h>

#include "syntiant_defs.h"


void* stdev_ndp10x_watch_thread_loop(void* p);

struct sound_trigger_properties hw_properties = {
  "Syntiant Corp.",               /* implementor */
  "Sound Trigger HAL for NDP10x", /* description */
  1,                              /* version */
  SYNTIANT_STHAL_UUID, 1,         /* max_sound_models this is use to indicate max concurrent model*/
  1,                              /* max_key_phrases */
  1,                              /* max_users*/
  RECOGNITION_MODE_VOICE_TRIGGER | RECOGNITION_MODE_USER_IDENTIFICATION,
  /* recognition_modes */
  false, /* capture_transition */
  0,     /* max_buffer_ms */
  true,  /* concurrent_capture */
  true,  /* trigger_in_event */
  0      /* power_consumption_mw */
};

struct syntiant_ndp10x_stdev ndp10x_stdev_g = {
  .lock = PTHREAD_MUTEX_INITIALIZER,
};

int stdev_stop_recognition(const struct sound_trigger_hw_device* dev, sound_model_handle_t handle);

// Can be called from AHAL thread or STHAL thread
int stdev_update_input(void) {
  int s = 0;
  struct syntiant_ndp10x_stdev* stdev = &ndp10x_stdev_g;

  int input_type = NDP10X_HAL_INPUT_TYPE_NONE;

  pthread_mutex_lock(&stdev->lock);
  ALOGV("%s : current_mode(%u) currently_ahal_recording(%d) currently_ahal_playing(%d)", __func__,
        stdev->current_mode, stdev->currently_ahal_recording, stdev->currently_ahal_playing);

  // This is little complicated to understand
  // There are different cases to look at

  // 1. if any app wants to record, we will disable
  // the NDP mic as well as the barge in

  // 2. if any app starts playing music, we will
  // switch the input from PDM to PCM only if we
  // are in watching mode.

  if (stdev->current_mode == STDEV_MODE_IDLE) {
// barge in only or stop_recognition
#ifdef ENABLE_BARGE_IN_SUPPORT
    syntiant_barge_in_stop(stdev->bHandle);
#endif
    input_type = NDP10X_HAL_INPUT_TYPE_NONE;
    goto exit;
  } else if (stdev->current_mode == STDEV_MODE_WATCHING) {
    // barge in or start_recognition
    if (stdev->currently_ahal_recording) {
      // Stop NDP clock because this is a call
      input_type = NDP10X_HAL_INPUT_TYPE_NONE;
// force stop barge in if running
#ifdef ENABLE_BARGE_IN_SUPPORT
      syntiant_barge_in_stop(stdev->bHandle);
#endif
      goto exit;
    } else if (stdev->currently_ahal_playing) {
      input_type = NDP10X_HAL_INPUT_TYPE_PCM;
#ifdef ENABLE_BARGE_IN_SUPPORT
      syntiant_barge_in_start(stdev->bHandle);
#endif
      goto exit;
    } else {
      input_type = NDP10X_HAL_INPUT_TYPE_MIC;
// force stop barge in if running
#ifdef ENABLE_BARGE_IN_SUPPORT
      syntiant_barge_in_stop(stdev->bHandle);
#endif
      goto exit;
    }
    goto no_change;  // never reach here
  } else if (stdev->current_mode == STDEV_MODE_RECORDING) {
    // barge in only
    if (stdev->currently_ahal_recording) {
      input_type = NDP10X_HAL_INPUT_TYPE_NONE;
// force stop barge in if running
#ifdef ENABLE_BARGE_IN_SUPPORT
      syntiant_barge_in_stop(stdev->bHandle);
#endif
      goto exit;
    }
    goto no_change;
  } else {
    ALOGE("%s : unknown mode", __func__);
    goto no_change;
  }

exit:
  ALOGV("%s : setting input to %d ", __func__, input_type);
  s = ndp10x_hal_set_input(stdev->ndp_handle, input_type, stdev->is_any_model_loaded);
  if (s < 0) {
    ALOGE("%s : error while setting input %d", __func__, input_type);
  }
no_change:
  pthread_mutex_unlock(&stdev->lock);
  return s;
}

static int stdev_get_properties(const struct sound_trigger_hw_device* dev __unused,
                                struct sound_trigger_properties* properties) {
  ALOGV("%s : enter ", __func__);
  if (properties == NULL) return -EINVAL;
  memcpy(properties, &hw_properties, sizeof(struct sound_trigger_properties));
  return 0;
}

/**
 * To be called with mutex locked
 */
int stdev_load_ndp_package(struct syntiant_ndp10x_stdev* stdev, struct syn_pkg* pkg) {
  int s = 0;

  s = ndp10x_hal_load(stdev->ndp_handle, pkg->data, pkg->size);
  if (s) {
    ALOGE("%s : error while loading package : error code = %d", __func__, s);
    goto error;
  }

  s = ndp10x_hal_set_tank_size(stdev->ndp_handle, STHAL_TANK_SIZE);
  if (s) {
    ALOGE("%s : error while setting tank size : error code = %d", __func__, s);
    goto error;
  }

  // TODO: may be move it to start_recognition
  /* flush any pending results in result buffer in driver */
  s = ndp10x_hal_flush_results(stdev->ndp_handle);
  if (s) {
    ALOGE("%s : error while flushing results : error code = %d", __func__, s);
    goto error;
  }

error:
  return s;
}

#ifdef ENABLE_SPEAKER_ID_SUPPORT
int stdev_load_spkr_id_model(struct syntiant_ndp10x_stdev* stdev, size_t num_users, unsigned int* user_ids,
                             size_t spkr_id_model_size_per_user, uint8_t* spkr_id_models,
                             size_t spkr_id_models_size) {
  int s = 0;
  if (stdev->speaker_id.pSpkrid_handle == NULL) {
    return -EINVAL;
  }

  if (num_users != 1) {
    return -EINVAL;
  }

  if ((int)spkr_id_model_size_per_user != stdev->speaker_id.user_model_sz) {
    return -EINVAL;
  }

  if (spkr_id_models_size != num_users * spkr_id_model_size_per_user) {
    return -EINVAL;
  }

  if (stdev->speaker_id.enrolled_users) {
    ALOGE("%s : currently only one speaker id user is supported", __func__);
    return -EINVAL;
  }

  s = syntiant_st_speaker_id_engine_add_user(stdev->speaker_id.pSpkrid_handle, user_ids[0], spkr_id_models,

                                             spkr_id_models_size);
  if (s != SYNTIANT_ST_SPEAKER_ID_ERROR_NONE) {
    ALOGE("%s : Error while adding user model : %d", __func__, s);
    s = -EINVAL;
    goto error;
  }

  /* only supports one enrolled user for now */
  stdev->speaker_id.enrolled_user_ids[0] = user_ids[0];
  stdev->speaker_id.enrolled_users = 1;

error:
  return s;
}
#endif

// TODO: Replace opaque_sound_model with syngup
int stdev_load_phrase_sound_model(struct syntiant_ndp10x_stdev* stdev,
                                  struct sound_trigger_phrase_sound_model* phrase_sound_model) {
  int s = 0;
  struct syngup_package package;
  struct syn_pkg* pkg;

  ALOGV("%s : enter ", __func__);

  if (phrase_sound_model->num_phrases != 1) {
    ALOGE("%s : sound models with single key phrase are only supported", __func__);
    return -EINVAL;
  }

  if ((phrase_sound_model->phrases[0].recognition_mode & RECOGNITION_MODE_USER_IDENTIFICATION) &&
      phrase_sound_model->phrases[0].num_users > 1) {
    ALOGE(
        "%s : sound models with mode RECOGNITION_MODE_USER_IDENTIFICATION can only contain 1 user",
        __func__);
    return -EINVAL;
  }

  pthread_mutex_lock(&stdev->lock);

  memset(&package, 0, sizeof(package));
  uint8_t* buffer = ((uint8_t*)phrase_sound_model) + phrase_sound_model->common.data_offset;
  size_t package_size = phrase_sound_model->common.data_size;

  s = syngup_extract_blobs(&package, buffer, package_size);
  if (s) {
    ALOGE("%s : Can not extract blobs from syngup\n", __func__);
    goto error;
  }
  ALOGV("%s : ** Get synpkg **\n", __func__);
  s = syngup_get_synpkg(&package, &pkg);
  if (s) {
    ALOGE("%s : Can not extract blobs from syngup package:%d \n", __func__, s);
    goto error;
  }
  ALOGV("%s : ** Load synpkg now **\n", __func__);
  s = stdev_load_ndp_package(stdev, pkg);

  if (s < 0) {
    ALOGE("%s : error while loading ndp package", __func__);
    goto error;
  }
  pkg = NULL;

#ifdef ENABLE_SPEAKER_ID_SUPPORT
#/* TODO: handle this correctly...  */
#if 0
  if ((phrase_sound_model->phrases[0].recognition_mode & RECOGNITION_MODE_USER_IDENTIFICATION) !=
      RECOGNITION_MODE_USER_IDENTIFICATION) {
    ALOGV("%s : Sound model without USER_IDENTIFICATION", __func__);
    goto no_speaker_id;
  }
#endif

  s = syngup_get_sound_package(&package, &pkg);
  if (s) {
    ALOGI("%s : Can not find user sound model\n", __func__);
    s = 0;
    goto error;
  }
  ALOGV("%s : ** Found user sound model **\n", __func__);
  ALOGV("%s : ** Data len %d **\n", __func__, pkg->size);

  uint8_t* spkr_id_models = pkg->data;
  /* take the meta data from user model and use here */
  struct phrase* ph =
        (struct phrase*)((uint8_t*)pkg->mdata + offsetof(struct synpkg_metadata, phrases));
  unsigned int user_id = 1;
  if (ph->users) {
    user_id = ph->users[0];
    if (!user_id) {
      /* user ID 0 is now reserved for imposter */
      user_id = 1;
    }
  }
  s = stdev_load_spkr_id_model(stdev, 1, &user_id, pkg->size, spkr_id_models, pkg->size);

  if (s < 0) {
    ALOGE("%s : error while loading spkr id model %d", __func__, s);
    goto error;
  } else {
    /* take the meta data from user model and use here */
    phrase_sound_model->phrases[0].recognition_mode = ph->recog_mode;
    ALOGV("%s : Setting recog modes %d  [user id[0]=%d]", __func__, ph->recog_mode, ph->users[0]);
  }
#endif

error:
  if (!s) {
    stdev->supported_recognition_modes = phrase_sound_model->phrases[0].recognition_mode;
    ALOGV("%s : Supported recog modes %d", __func__, stdev->supported_recognition_modes);
  }
  syngup_cleanup(&package);
  pthread_mutex_unlock(&stdev->lock);
  return s;
}

int stdev_load_sound_model(const struct sound_trigger_hw_device* dev,
                           struct sound_trigger_sound_model* sound_model,
                           sound_model_callback_t callback, void* cookie,
                           sound_model_handle_t* handle) {
  struct syntiant_ndp10x_stdev* stdev = (struct syntiant_ndp10x_stdev*)dev;
  int s = 0;

  ALOGV("%s : enter ", __func__);

  if (dev == NULL) {
    ALOGE("%s : invalid dev", __func__);
    return -EINVAL;
  }

  if (handle == NULL || sound_model == NULL) {
    ALOGE("%s : invalid handle ", __func__);
    return -EINVAL;
  }

  if (sound_model->type != SOUND_MODEL_TYPE_KEYPHRASE) {
    ALOGE("%s : unspported model type -> %d", __func__, sound_model->type);
    return -EINVAL;
  }

  if ((sound_model->data_size) &&
      (sound_model->data_offset < sizeof(struct sound_trigger_phrase_sound_model))) {
    ALOGE("%s : improper data offsets %d ", __func__, sound_model->data_offset);
    return -EINVAL;
  }

  struct sound_trigger_phrase_sound_model* phrase_sound_model =
      (struct sound_trigger_phrase_sound_model*)sound_model;

  s = stdev_load_phrase_sound_model(stdev, phrase_sound_model);
  if (s < 0) {
    ALOGE("%s : error while loading phrase sound model", __func__);
    goto error;
  }

  ALOGV("%s : successfully loaded phrase sound model", __func__);

  pthread_mutex_lock(&stdev->lock);
  stdev->sound_model_callback = callback;
  stdev->sound_model_cookie = cookie;
  stdev->model_handle = 1;
  if (handle) {
    *handle = stdev->model_handle;
  }

  stdev->is_any_model_loaded = true;
  ALOGV("Sound model loaded successfully");

error:
  pthread_mutex_unlock(&stdev->lock);
  stdev_stop_recognition(dev, stdev->model_handle);
  return s;
}

int stdev_unload_sound_model(const struct sound_trigger_hw_device* dev,
                             sound_model_handle_t handle) {
  struct syntiant_ndp10x_stdev* stdev = (struct syntiant_ndp10x_stdev*)dev;
  size_t i;

  ALOGV("%s : enter ", __func__);

  // Note : We are only stopping recognition because we only support
  // single sound model.
  int status = stdev_stop_recognition(dev, handle);
  if (status) {
    return status;
  }

  /* re-init to prepare for new round of load */
  if (ndp10x_hal_init(stdev->ndp_handle) < 0) {
    ALOGE("%s : ndp10x default init ioctl failed", __func__);
  }

#ifdef ENABLE_SPEAKER_ID_SUPPORT
  if (stdev->speaker_id.enrolled_users > 0) {
    int operating_point;
    for (i = 0; i < stdev->speaker_id.enrolled_users; i++) {
      status = syntiant_st_speaker_id_engine_remove_user(stdev->speaker_id.pSpkrid_handle, 
                                                         stdev->speaker_id.enrolled_user_ids[i]);
      if (status != SYNTIANT_ST_SPEAKER_ID_ERROR_NONE) {
        ALOGE("%s : error while removing user %d", __func__, 
              stdev->speaker_id.enrolled_user_ids[i]);
      }
    }

  /* the Meeami library currently has an issue in that remove_user API does not work
   * properly - so in addition to the above remove_user call, do a new round of uninit .. init */
    status = syntiant_st_speaker_id_engine_uninit(stdev->speaker_id.pSpkrid_handle);
    if (status != SYNTIANT_ST_SPEAKER_ID_ERROR_NONE) {
      ALOGE("%s : speaker id uninit error %d", __func__, status);
    }

    operating_point = /*    012345678901234567890123456789  -- max len is 32 bytes*/
        property_get_int32("syntiant.sthal.operating_point", SYNTIANT_SPEAKER_ID_DEFAULT_OPT_CNT);

    status = syntiant_st_speaker_id_engine_init(
        SYNTIANT_SPEAKER_ID_MAX_USERS, stdev->speaker_id.ww_len,
        operating_point, SYNTIANT_SPEAKER_ID_DEFAULT_UTTER_CNT,
        &stdev->speaker_id.pSpkrid_handle, &stdev->speaker_id.user_model_sz);
    if (status != SYNTIANT_ST_SPEAKER_ID_ERROR_NONE) {
      ALOGE("%s : speaker id init error %d", __func__, status);
    }

    stdev->speaker_id.enrolled_users = 0;
  }
#endif

  pthread_mutex_lock(&stdev->lock);
  stdev->model_handle = 0;
  stdev->sound_model_callback = NULL;
  stdev->sound_model_cookie = NULL;
  stdev->is_any_model_loaded = false;
  pthread_mutex_unlock(&stdev->lock);

  /* signal to watch thread. This in turn will cause watch thread
   * to go back and wait for next start_recognition */
  pthread_kill(stdev->watch_thread, SIGUSR1);

  return 0;
}

int stdev_start_recognition(const struct sound_trigger_hw_device* dev, sound_model_handle_t handle,
                            const struct sound_trigger_recognition_config* config,
                            recognition_callback_t callback, void* cookie) {
  struct syntiant_ndp10x_stdev* stdev = (struct syntiant_ndp10x_stdev*)dev;
  int s = 0;

  unsigned int keyphrase_id = 0;
  unsigned int recognition_modes = 0;

  ALOGV("%s : enter", __func__);
  ALOGV("%s : Recognition config capture_handle(%d) capture_device(0x%x) capture_requested(%d)",
        __func__, config->capture_handle, config->capture_device, config->capture_requested);
  ALOGV("%s :   recognition_modes(%d)",
        __func__, config->phrases[0].recognition_modes);

  if (handle != stdev->model_handle) {
    ALOGE("%s : handle_error handle=%d   stdev->model_handle=%d", __func__, handle,
          stdev->model_handle);
    return -EINVAL;
  }

  // TODO : may be change this for local commands
  if (config->num_phrases != 1) {
    ALOGE("%s : Cannot start recognition for more than one phrase for a sound model", __func__);
    return -EINVAL;
  }

  keyphrase_id = config->phrases[0].id;
  recognition_modes = config->phrases[0].recognition_modes;
  if (keyphrase_id != 0) {
    ALOGE("%s : Keyphrase cannot be Non Zero value", __func__);
    return -EINVAL;
  }

  pthread_mutex_lock(&stdev->lock);

  /* flush any pending results in result buffer in driver */
  s = ndp10x_hal_flush_results(stdev->ndp_handle);
  if (s) {
    ALOGE("%s : error while flushing results : error code = %d", __func__, s);
  }

  if ((recognition_modes & stdev->supported_recognition_modes) != recognition_modes) {
    ALOGE("%s : Unsupported recognition mode ", __func__);
    s = -EINVAL;
    goto locked_exit;
  }

  // TODO : Figure out how to support more than one keyphrase
  stdev->keyphrase_id = 0;
  stdev->recognition_modes = recognition_modes;
  stdev->recognition_callback = callback;
  stdev->recognition_cookie = cookie;
  stdev->capture_handle = config->capture_handle;
  stdev->current_mode = STDEV_MODE_WATCHING;
  pthread_mutex_unlock(&stdev->lock);

  s = stdev_update_input();
  if (s) {
    ALOGE("%s : error while starting to listen : status(%d)", __func__, s);
  }

  return s;

locked_exit:
  pthread_mutex_unlock(&stdev->lock);
  return s;
}

int stdev_stop_recognition(const struct sound_trigger_hw_device* dev, sound_model_handle_t handle) {
  struct syntiant_ndp10x_stdev* stdev = (struct syntiant_ndp10x_stdev*)dev;
  int s = 0;

  ALOGV("%s : enter", __func__);

  if (handle != stdev->model_handle) {
    ALOGE("%s : handle(%d) != stdev->model_handle(%d) ", __func__, handle, stdev->model_handle);
    return -EINVAL;
  }

  pthread_mutex_lock(&stdev->lock);
  stdev->recognition_callback = NULL;
  stdev->recognition_cookie = NULL;
  stdev->keyphrase_id = -1;
  stdev->capture_handle = 0;
  stdev->recognition_modes = 0;
  stdev->current_mode = STDEV_MODE_IDLE;
  pthread_mutex_unlock(&stdev->lock);

  s = stdev_update_input();
  if (s) {
    ALOGE("%s : error while stopping to listen : status(%d)", __func__, s);
  }

  ALOGV("%s : exit", __func__);
  return s;
}

int stdev_close(hw_device_t* dev) {
  struct syntiant_ndp10x_stdev* stdev = (struct syntiant_ndp10x_stdev*)dev;
  int status = 0;

  ALOGV("%s : enter", __func__);

  if (dev == NULL) {
    ALOGE("%s : invalid dev ", __func__);
    return -EINVAL;
  }

  /* signal stop thread */
  stdev->close_watch_thread = true;

  /* free mutex semaphone*/
  pthread_mutex_destroy(&stdev->lock);

  status = close(stdev->ndp_handle);
  if (status < 0) {
    ALOGE("%s : Error closing stdev", __func__);
    return -EINVAL;
  }

#ifdef ENABLE_SPEAKER_ID_SUPPORT
  status = syntiant_st_speaker_id_engine_uninit(stdev->speaker_id.pSpkrid_handle);
  if (status != SYNTIANT_ST_SPEAKER_ID_ERROR_NONE) {
    ALOGE("%s : speaker id uninit error %d", __func__, status);
    return status;
  }
#endif

  stdev->initialized = false;
  return 0;
}

int stdev_open(const hw_module_t* module, const char* name, hw_device_t** device) {
  struct syntiant_ndp10x_stdev* stdev;
  int status = 0;
#ifdef ENABLE_SPEAKER_ID_SUPPORT
  int operating_point;
#endif
  ALOGV("%s : stdev_open() called - name = %s", __func__, name);

  if (strcmp(name, SOUND_TRIGGER_HARDWARE_INTERFACE) != 0) {
    ALOGE("%s : Wrong hardware interface", __func__);
    return -EINVAL;
  }
  stdev = &ndp10x_stdev_g;

  stdev->device.common.tag = HARDWARE_DEVICE_TAG;
  stdev->device.common.version = SOUND_TRIGGER_DEVICE_API_VERSION_1_0;
  stdev->device.common.module = (struct hw_module_t*)module;
  stdev->device.common.close = stdev_close;
  stdev->device.get_properties = stdev_get_properties;
  stdev->device.load_sound_model = stdev_load_sound_model;
  stdev->device.unload_sound_model = stdev_unload_sound_model;
  stdev->device.start_recognition = stdev_start_recognition;
  stdev->device.stop_recognition = stdev_stop_recognition;

  pthread_mutex_init(&stdev->lock, (const pthread_mutexattr_t*)NULL);

  stdev->close_watch_thread = false;

  *device = &stdev->device.common;
  stdev->ndp_handle = ndp10x_hal_open();
  if (stdev->ndp_handle < 0) {
    ALOGE("%s : ndp10x open failed", __func__);
    return -ENODEV;
  }

  /* init */
  if (ndp10x_hal_init(stdev->ndp_handle) < 0) {
    ALOGE("%s : ndp10x default init ioctl failed", __func__);
    return -EIO;
  }
  ALOGV("%s : device initialized successfully!", __func__);

  stdev->initialized = true;

  pthread_mutex_lock(&stdev->lock);
  stdev->is_any_model_loaded = false;
  stdev->currently_ahal_playing = false;
  stdev->currently_ahal_recording = false;
  pthread_mutex_unlock(&stdev->lock);

#ifdef ENABLE_BARGE_IN_SUPPORT
  status = syntiant_barge_in_init(&stdev->bHandle);
  if (status < 0) {
    ALOGE("%s : error while init barge in", __func__);
    return status;
  }
#endif

#ifdef ENABLE_SPEAKER_ID_SUPPORT
  stdev->speaker_id.ww_len = SYNTIANT_SPEAKER_ID_WW_LEN;
  operating_point = /*    012345678901234567890123456789  -- max len is 32 bytes*/
      property_get_int32("syntiant.sthal.operating_point", SYNTIANT_SPEAKER_ID_DEFAULT_OPT_CNT);
  status = syntiant_st_speaker_id_engine_init(
      SYNTIANT_SPEAKER_ID_MAX_USERS, stdev->speaker_id.ww_len, operating_point,
      SYNTIANT_SPEAKER_ID_DEFAULT_UTTER_CNT, &stdev->speaker_id.pSpkrid_handle,
      &stdev->speaker_id.user_model_sz);
  if (status != SYNTIANT_ST_SPEAKER_ID_ERROR_NONE) {
    ALOGE("%s : speaker id init error %d", __func__, status);
    return status;
  }
#endif

  ALOGV("%s : Starting Watch Thread", __func__);
  pthread_create(&stdev->watch_thread, NULL, stdev_ndp10x_watch_thread_loop, stdev);

  ALOGV("%s : device configured successfully!", __func__);
  return status;
}

struct hw_module_methods_t hal_module_methods = {
  .open = stdev_open,
};

struct sound_trigger_module HAL_MODULE_INFO_SYM = {
  .common =
      {
          .tag = HARDWARE_MODULE_TAG,
          .module_api_version = SOUND_TRIGGER_MODULE_API_VERSION_1_0,
          .hal_api_version = HARDWARE_HAL_API_VERSION,
          .id = SOUND_TRIGGER_HARDWARE_MODULE_ID,
          .name = "Syntiant NDP10X",
          .author = "Syntiant Corp.",
          .methods = &hal_module_methods,
      },
};
