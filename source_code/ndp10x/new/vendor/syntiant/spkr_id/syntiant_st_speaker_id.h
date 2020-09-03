/*
 * SYNTIANT CONFIDENTIAL
 * _____________________
 *
 *   Copyright (c) 2018-2020 Syntiant Corporation
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

#define SYNTIANT_SPEAKER_ID_API_VERSION 5

#define SYNTIANT_SPEAKER_ID_DEFAULT_OPT_CNT 14
#define SYNTIANT_SPEAKER_ID_DEFAULT_UTTER_CNT 5
#define SYNTIANT_SPEAKER_ID_WW_LEN 33600

enum syntiant_st_speaker_id_errors_e {
    SYNTIANT_ST_SPEAKER_ID_ERROR_NONE = 0x0000, 
    SYNTIANT_ST_SPEAKER_ID_ERROR_NO_MEM = 0x0001,
    SYNTIANT_ST_SPEAKER_ID_ERROR_NEED_LONGER_AUDIO = 0x0002,
    SYNTIANT_ST_SPEAKER_ID_ERROR_MODEL_USER_ALREADY_EXISTS = 0x0003,
    SYNTIANT_ST_SPEAKER_ID_ERROR_MODEL_TOO_MANY_USERS = 0x0004,
    SYNTIANT_ST_SPEAKER_ID_ERROR_MODEL_NO_SUCH_USER = 0x0005,
    SYNTIANT_ST_SPEAKER_ID_ERROR_NEED_MORE_AUDIO = 0x0006,
    SYNTIANT_ST_SPEAKER_ID_ERROR_MODEL_NO_USER_IDENTIFIED = 0x0007,
    SYNTIANT_ST_SPEAKER_ID_ERROR_INVALID_OPT_PT = 0x0008,
    SYNTIANT_ST_SPEAKER_ID_ERROR_INVALID_UTT_CNT = 0x0009,
    SYNTIANT_ST_SPEAKER_ID_ERROR_INCOMPATIBLE_USER_MODEL = 0x0010,
    SYNTIANT_ST_SPEAKER_ID_ERROR_NOISY_ENV_UTT = 0x0100,
    SYNTIANT_ST_SPEAKER_ID_ERROR_SOFT_SPOKEN_UTT = 0x0200,
    SYNTIANT_ST_SPEAKER_ID_ERROR_SATURATED_UTT = 0x0300,
    SYNTIANT_ST_SPEAKER_ID_ERROR_LOUD_SPOKEN_UTT = 0x0400
};

#define MAX_NUM_USERS 5

/**
 * @brief Validate the Enrollment Utterance
 * @param audio_recording audio recordings @ 16 Khz, 16 bit
 * @param[in] length_audio_recording length of audio recording to be used for enrollment
 * @return int error code
 * SYNTIANT_ST_SPEAKER_ID_ERROR_NONE -> Success
 * SYNTIANT_ST_SPEAKER_ID_ERROR_NEED_LONGER_AUDIO -> Length of audio recording is insufficient
 * SYNTIANT_ST_SPEAKER_ID_ERROR_SATURATED_UTT -> Signal is saturated
 * SYNTIANT_ST_SPEAKER_ID_ERROR_NOISY_ENV_UTT -> Heavy Noise Environment
 * SYNTIANT_ST_SPEAKER_ID_ERROR_SOFT_SPOKEN_UTT -> Low Voice or soft spoken
 */
int syntiant_st_speaker_id_engine_enroll_utt_validation(
short *audio_recording, 
unsigned int length_audio_recording );


/**
 * @brief initializes and allocates the speaker id engine
 * 
 * @param[in] max_users maximum number of users that we want to identify 
 * @param[in] length_audio_recording length of audio recording to be used for 
 * training  & identification of user.
 * @param[in] opt_cnt Operating point need to be used.
 * @param[in] e_uttercnt Number utterances to train at the time of enrollment.
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
 
int syntiant_st_speaker_id_engine_init(
    unsigned int max_users,
    unsigned int length_audio_recording,
    short opt_cnt,
    short e_uttercnt,
    void **spkr_id_engine_out,
    int *spkr_id_user_sound_model_size_out);

/**
 * @brief deallocates the speaker id engine
 * 
 * @param[in] spkr_id_engine handle to speaker id engine
 * @return int error code
 * SYNTIANT_ST_SPEAKER_ID_ERROR_NONE -> Success
 */
int syntiant_st_speaker_id_engine_uninit(
    void *spkr_id_engine);

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
int syntiant_st_speaker_id_engine_add_user(
    void *spkr_id_engine,
    unsigned int user_id,
    void *spkr_id_user_sound_model,
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
int syntiant_st_speaker_id_engine_remove_user(
    void *spkr_id_engine,
    unsigned int user_id);


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
int syntiant_st_speaker_id_engine_get_user_model(
    void *spkr_id_engine,
    unsigned int user_id,
    unsigned int spkr_id_user_sound_model_size,
    void *spkr_id_user_sound_model_out);

/**
 * @brief it will set the operating point for speaker id engine
 * 
 * @param[in] sm spkr_id_engine handle to speaker id engine
 * @param[in] opt_pnt pass the operating point you want to set
 * @return int error code
 * SYNTIANT_ST_SPEAKER_ID_ERROR_NONE -> Success
 * SYNTIANT_ST_SPEAKER_ID_ERROR_INVALID_OPT_PT -> invalid operating point set
 */
int syntiant_st_speaker_id_engine_set_opt_pnt(
void *sm,
short opt_pnt);
/**
 * @brief it gives the current operating point set for speaker id engine
 * 
 * @param[in] sm spkr_id_engine handle to speaker id engine
 * @param[out] opt_pnt returns the operating which is set in speaker id engine
 * @return int error code
 * SYNTIANT_ST_SPEAKER_ID_ERROR_NONE -> Success
 */
int syntiant_st_speaker_id_engine_get_opt_pnt(
void *sm,
short *opt_pnt
);

/*
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
int syntiant_st_speaker_id_engine_train_user(
    void * spkr_id_engine, 
    unsigned int user_id, 
    unsigned int num_recordings, 
    unsigned int length_recordings, 
    short **audio_recordings);

/**
 * @brief identifies the person in the audio recording. 
 * 
 * Note: This function will not verify whether the wakeword is present 
 * in the audio recordings 
 * 
 * Note: The audio recordings are expected at 16 Khz, 16 bit
 * 
 * @param[in] spkr_id_engine handle to speaker id engine        
 * @param[in] length_recording length of audio recording in samples
 * @param[in] audio_recording audio recordings @ 16 Khz, 16 bit
 * @param[out] user_id_out id will be 0 it is impostor or else returns user id of speaker.
 * @param[out] confidence_score_out output confidence score representing confidence 
 * in detecting user.
 * @param[in] snr_val SNR of utterance.
 * @return int error code
 * SYNTIANT_ST_SPEAKER_ID_ERROR_NONE -> Success 
 * SYNTIANT_ST_SPEAKER_ID_ERROR_NEED_LONGER_AUDIO -> if given audio recording
 * is shorter than expected.
 * SYNTIANT_ST_SPEAKER_ID_ERROR_NOISY_ENV_UTT -> returns if utterance is spoken in a noisy environment
 * SYNTIANT_ST_SPEAKER_ID_ERROR_SOFT_SPOKEN_UTT -> returns this error if utterance uttered is of very low volume.
 * SYNTIANT_ST_SPEAKER_ID_ERROR_SATURATED_UTT -> returns if utterance is saturated.
 */
int syntiant_st_speaker_id_engine_identify_user(
    void * spkr_id_engine,
    unsigned int length_recording, 
    short *audio_recording, 
    unsigned int * user_id_out,
    int * confidence_score_out,
    uint8_t snr_val
    );

#ifdef __cplusplus
}
#endif

#endif /* SYNTIANT_ST_SPEAKER_ID_H */