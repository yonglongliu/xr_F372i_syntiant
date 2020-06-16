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

#ifndef SYNTIANT_SOUNDMODEL_H
#define SYNTIANT_SOUNDMODEL_H

#define LIBSYNSOUNDMODEL_VERSION 2

#define SYNTIANT_ST_SOUND_MODEL_MAX_ENROLLMENT_RECORDINGS 10

#include <system/audio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ENROLLMENT_MAX_AUDIO_DATA_SIZE (3 * 16000 * sizeof(short))

struct syntiant_st_sound_model_enrollment_recording_s {
  audio_config_t audio_config;
  uint8_t audio_data[ENROLLMENT_MAX_AUDIO_DATA_SIZE];
};

/* get size of the destination sound model object */
ssize_t syntiant_st_sound_model_get_size_from_binary_sound_model(uint8_t* binary_sound_model_data,
                                                                 size_t binary_sound_model_size);

/* Parse file data into a sound model
 * binary_sound_model_data: buffer containing file data
 * binary_sound_model_size: size of the buffer
 * dest_buffer: sound model from parsed file copied to caller supplied buffer
*/
int syntiant_st_sound_model_build_from_binary_sound_model(uint8_t* binary_sound_model_data,
                                                          uint32_t binary_sound_model_size,
                                                          uint8_t* buffer);

ssize_t syntiant_st_sound_model_get_size_when_extended(
    struct sound_trigger_phrase_sound_model* phrase_sound_model);

int syntiant_st_sound_model_extend(
    struct sound_trigger_phrase_sound_model* source_sm, unsigned int user_id,
    size_t num_enrollment_recordings,
    struct syntiant_st_sound_model_enrollment_recording_s* recordings, uint8_t** destination_sm,
    uint32_t* dest_model_size);

int syntiant_st_sound_model_get_enrollment_recording_requirements(
    struct sound_trigger_phrase_sound_model* phrase_sound_model, unsigned int* num_recordings,
    audio_config_t* audio_config);

/* Compatibility layer for KaiOS initial implementation */

#define SYNTIANT_SPEAKER_ID_NUM_ENROLLMENT_SAMPLES 5
#define SYNTIANT_SPEAKER_ID_WW_LEN 33600

enum syntiant_error_type_e { SYNTIANT_ERROR_NONE = 0, SYNTIANT_ERROR_FAIL = 1 };

struct syntiant_user_sound_model_s {
  unsigned int model_size;
  uint8_t model[];
};

/* get size of the destination sound model object */
unsigned int syntiant_get_keyphrase_sound_model_size(uint8_t* file_data, size_t file_size);

/* Populate dest_model with settings from file
 * it is caller responsibility to allocate memory for dest_model
 * size of allocation can be obtained via syntiant_get_keyphrase_sound_model_size()
 */
int syntiant_parse_sound_model(struct sound_trigger_phrase_sound_model* dest_model,
                               uint8_t* file_data, uint32_t file_size);

/* extend a (base) sound model with user-specific data in files */
int ExtendSoundModel(struct sound_trigger_phrase_sound_model* model,
                     char* sample_filenames[SYNTIANT_SPEAKER_ID_NUM_ENROLLMENT_SAMPLES],
                     struct syntiant_user_sound_model_s** dest_model, int user_id);

/* extend a (base) sound model with user-specific data in audio samples
 * SYNTIANT_SPEAKER_ID_NUM_ENROLLMENT_SAMPLES each of SYNTIANT_SPEAKER_ID_WW_LEN
 * samples
 */
int ExtendSoundModel_Samples(struct sound_trigger_phrase_sound_model* model,
                             int16_t* samples[SYNTIANT_SPEAKER_ID_NUM_ENROLLMENT_SAMPLES],
                             struct syntiant_user_sound_model_s** dest_model, int user_id);

#ifdef __cplusplus
}
#endif

#endif
