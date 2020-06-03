/*
 * SYNTIANT CONFIDENTIAL
 * _____________________
 *
 *   Copyright (c) 2018 Syntiant Corporation
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

#ifndef SYNTIANT_ST_SPEAKER_ID_H
#define SYNTIANT_ST_SPEAKER_ID_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SYNTIANT_SPEAKER_ID_API_VERSION 3

#define SYNTIANT_SPEAKER_ID_DEFAULT_OPT_CNT 14
#define SYNTIANT_SPEAKER_ID_DEFAULT_UTTER_CNT 5
#define SYNTIANT_SPEAKER_ID_WW_LEN 19200

enum syntiant_st_speaker_id_errors_e {
  SYNTIANT_ST_SPEAKER_ID_ERROR_NONE = 0,
  SYNTIANT_ST_SPEAKER_ID_ERROR_NO_MEM = 1,
  SYNTIANT_ST_SPEAKER_ID_ERROR_NEED_LONGER_AUDIO = 2,
  SYNTIANT_ST_SPEAKER_ID_ERROR_MODEL_USER_ALREADY_EXISTS = 3,
  SYNTIANT_ST_SPEAKER_ID_ERROR_MODEL_TOO_MANY_USERS = 4,
  SYNTIANT_ST_SPEAKER_ID_ERROR_MODEL_NO_SUCH_USER = 5,
  SYNTIANT_ST_SPEAKER_ID_ERROR_NEED_MORE_AUDIO = 6,
  SYNTIANT_ST_SPEAKER_ID_ERROR_MODEL_NO_USER_IDENTIFIED = 8,
  SYNTIANT_ST_SPEAKER_ID_ERROR_INVALID_OPT_PT = 9,
  SYNTIANT_ST_SPEAKER_ID_ERROR_INVALID_UTT_CNT = 10
};

#define MAX_NUM_USERS 5

/**
 * @brief initializes and allocates the speaker id engine
 *
 * @param[in] max_users maximum number of users that we want to identify
 * @param[in] length_audio_recording length of audio recording to be used for
 * training  & identification of user.
 * @param[out] spkr_id_engine_out handle to speaker id engine
 * @return int error code
 * SYNTIANT_ST_SPEAKER_ID_ERROR_NONE -> Success
 * SYNTIANT_ST_SPEAKER_ID_ERROR_NO_MEM -> Error while allocating memory
 * SYNTIANT_ST_SPEAKER_ID_ERROR_NEED_LONGER_AUDIO -> Error if
 * length_audio_recording is too short
 * SYNTIANT_ST_SPEAKER_ID_ERROR_INVALID_OPT_PT -> Error if invalid opt_pt is given
 * SYNTIANT_ST_SPEAKER_ID_ERROR_INVALID_UTT_CNT -> Error if
 * invalid utterance count is given
 *
 */
int syntiant_st_speaker_id_engine_init(unsigned int max_users, unsigned int length_audio_recording,
                                       short opt_cnt, short e_uttercnt, void** spkr_id_engine_out,
                                       int* spkr_id_user_sound_model_size_out);

/**
 * @brief deallocates the speaker id engine
 *
 * @param[in] spkr_id_engine handle to speaker id engine
 * @return int error code
 * SYNTIANT_ST_SPEAKER_ID_ERROR_NONE -> Success
 */
int syntiant_st_speaker_id_engine_uninit(void* spkr_id_engine);

/**
 * @brief Adds a user and its model to engine so that engine can verify the
 * user in a audio recording. This API is to be used after device reboot when we
 * have already enrolled a user earlier.
 *
 * @param[in] spkr_id_engine handle to speaker id engine
 * @param[in] user_id unique id for a user
 * @param[in] spkr_id_user_sound_model sound model for the user
 * @param[in] spkr_id_user_sound_model_size size of sound model for the user.
 * @return int error code
 * SYNTIANT_ST_SPEAKER_ID_ERROR_NONE -> Success
 * SYNTIANT_ST_SPEAKER_ID_ERROR_MODEL_USER_ALREADY_EXISTS -> A model has
 * already been added earlier to engine with same user id.
 * SYNTIANT_ST_SPEAKER_ID_ERROR_MODEL_TOO_MANY_USERS -> Trying to add user after
 * max_users has been reached.
 */
int syntiant_st_speaker_id_engine_add_user(void* spkr_id_engine, unsigned int user_id,
                                           void* spkr_id_user_sound_model,
                                           unsigned int spkr_id_user_sound_model_size);

/**
 * @brief removes a user & its sound model from speaker id engine.
 *
 * @param[in] spkr_id_engine handle to speaker id engine
 * @param[in] user_id id for a user to be removed
 * @return int error code
 * SYNTIANT_ST_SPEAKER_ID_ERROR_NONE -> Success
 * SYNTIANT_ST_SPEAKER_ID_ERROR_MODEL_NO_SUCH_USER -> if user_id is not present
 * in the speaker id engine
 */
int syntiant_st_speaker_id_engine_remove_user(void* spkr_id_engine, unsigned int user_id);

/**
 * @brief gets sound model for a user from the speaker id engine after training.
 *
 * @param[in] spkr_id_engine handle to speaker id engine
 * @param[in] user_id id for the user
 * @param[in] spkr_id_user_sound_model_size sizes in bytes allocated for the
 *  sound model
 * @param[out] spkr_id_user_sound_model_out sound model for the user.
 * @return int error code
 * SYNTIANT_ST_SPEAKER_ID_ERROR_NONE -> Success
 * SYNTIANT_ST_SPEAKER_ID_ERROR_MODEL_NO_SUCH_USER -> if user_id is not present
 * in the speaker id engine
 */
int syntiant_st_speaker_id_engine_get_user_model(void* spkr_id_engine, unsigned int user_id,
                                                 unsigned int spkr_id_user_sound_model_size,
                                                 void* spkr_id_user_sound_model_out);

/**
 * @brief trains a user sound model from given audio recordings in the
 * speaker id engine. This API can be called multiple times with same User id
 * and new audio recordings.
 *
 * Note: This function will not verify whether the wakeword is present
 * in the audio recordings
 *
 * Note: The audio recordings are expected at 16 Khz, 16 bit
 *
 * @param spkr_id_engine handle to speaker id engine
 * @param user_id unique id for user to be trained
 * @param num_recordings num of audio recordings
 * @param length_recordings length of audio recordings in samples
 * @param audio_recordings audio recordings in the form of 2D array
 *  audio_recordings[num_recordings][len_recordings]
 * @return int error code
 * SYNTIANT_ST_SPEAKER_ID_ERROR_NONE -> Success
 * SYNTIANT_ST_SPEAKER_ID_ERROR_MODEL_USER_ALREADY_EXISTS -> if user_id is
 *  already exists in the engine.
 * SYNTIANT_ST_SPEAKER_ID_ERROR_MODEL_TOO_MANY_USERS -> If too many users are
 *  already added in the engine.
 * SYNTIANT_ST_SPEAKER_ID_ERROR_NEED_MORE_AUDIO -> if more audio is needed
 *  for the user to create its sound model.
 * SYNTIANT_ST_SPEAKER_ID_ERROR_NEED_LONGER_AUDIO -> if given audio recording
 * is shorter than expected.
 *
 */
int syntiant_st_speaker_id_engine_train_user(void* spkr_id_engine, unsigned int user_id,
                                             unsigned int num_recordings,
                                             unsigned int length_recordings,
                                             short** audio_recordings);

/**
 * @brief identifies the person in the audio recording.
 *
 * Note: This function will not verify whether the wakeword is present
 * in the audio recordings
 *
 * Note: The audio recordings are expected at 16 Khz, 16 bit
 *
 * @param spkr_id_engine handle to speaker id engine
 * @param length_recording length of audio recording in samples
 * @param audio_recording audio recordings @ 16 Khz, 16 bit
 * @param user_id_out id of the identified user
 * @param confidence_score_out output confidence score representing confidence
 * in detecting user.
 * @return int error code
 * SYNTIANT_ST_SPEAKER_ID_ERROR_NONE -> Success
 * SYNTIANT_ST_SPEAKER_ID_ERROR_NEED_LONGER_AUDIO -> if given audio recording
 * is shorter than expected.
 * SYNTIANT_ST_SPEAKER_ID_ERROR_MODEL_NO_USER_IDENTIFIED -> if no user could
 * be identified in given audio recording.
 */
int syntiant_st_speaker_id_engine_identify_user(void* spkr_id_engine, unsigned int length_recording,
                                                short* audio_recording, unsigned int* user_id_out,
                                                int* confidence_score_out);

#ifdef __cplusplus
}
#endif

#endif
