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
#define LOG_TAG "SyntiantAudioSession"

#include <utils/Log.h>

#include <fcntl.h>  /* For O_RDWR */
#include <unistd.h> /* For open(), creat() */

#include <iostream>
#include <sstream>

#include "SyntiantAudioSession.h"

#include "syntiant_defs.h"

using namespace android;

SyntiantAudioRecordSession::SyntiantAudioRecordSession() {}

SyntiantAudioRecordSession::~SyntiantAudioRecordSession() {}

int SyntiantAudioRecordSession::start(uint32_t sampleRate, size_t channels, size_t bits,
                                      audio_session_t sessionId, audio_source_t source) {
  // ALOGI("%s : enter", __func__);
  status_t status;
  size_t minFrameCount;
  audio_channel_mask_t channel_mask;
  audio_format_t format;

  if (bits != 16 && bits != 32) {
    std::stringstream ss;
    ss << "bits can only be 16 or 32 but given " << bits << std::endl;
    ALOGE("%s", ss.str().c_str());
    status = -EINVAL;
    goto error;
  }

  if (channels != 1 && channels != 2) {
    std::stringstream ss;
    ss << "channels can only be 1 or 2 but given " << channels << std::endl;
    ALOGE("%s", ss.str().c_str());
    status = -EINVAL;
    goto error;
  }

  format = (bits == 16) ? AUDIO_FORMAT_PCM_16_BIT : AUDIO_FORMAT_PCM_32_BIT;
  channel_mask = audio_channel_in_mask_from_count(channels);

  status = AudioRecord::getMinFrameCount(&minFrameCount, sampleRate, format, channel_mask);

  if (status) {
    std::stringstream ss;
    ss << "Error while getMinFrameCount : status = " << status << std::endl;
    ALOGE("%s", ss.str().c_str());
    goto error;
  }

  ALOGE("%s : session id %d  min frame count: %d", __func__, sessionId, minFrameCount);
  pAudioRecord =
      new AudioRecord(source, sampleRate, format, channel_mask, String16("SyntiantAudioSession"),
                      minFrameCount, NULL, NULL, 0, sessionId);

  if (pAudioRecord == nullptr) {
    std::stringstream ss;
    ss << "Error while allocating AudioRecord : status = " << status << std::endl;
    ALOGE("%s", ss.str().c_str());
    goto error;
  }

  status = pAudioRecord->initCheck();
  if (status != OK) {
    std::stringstream ss;
    ss << "Error while initCheck : status = " << status << std::endl;
    ALOGE("%s", ss.str().c_str());
    goto error;
  }

  status = pAudioRecord->start();
  if (status != 0) {
    std::stringstream ss;
    ss << "Error while start recording : status = " << status << std::endl;
    ALOGE("%s", ss.str().c_str());
    /* Hack: silence the crash */
    status = 0;
    goto error;
  }

  mSessionStarted = 1;

  return status;

error:
  if (pAudioRecord != NULL) {
    pAudioRecord.clear();
  }

  return status;
}

int SyntiantAudioRecordSession::read(void* buffer, size_t num_bytes) {
  // ALOGI("%s : enter", __func__);
  int status = 0;
  status = pAudioRecord->read(buffer, num_bytes);
  if (status < 0) {
    std::stringstream ss;
    ss << "Error while reading : status = " << status << std::endl;
    ALOGE("%s", ss.str().c_str());
    std::cout << ss.str();
  }
  // ALOGI("%s : exit status %d", __func__, status);
  return status;
}

void SyntiantAudioRecordSession::stop() {
  if (pAudioRecord != NULL) {
    if (mSessionStarted) {
      pAudioRecord->stop();
    }

    pAudioRecord.clear();
  }
}

SyntiantAudioPlaySession::SyntiantAudioPlaySession(std::string filename) {
  mFilename = filename;
  mFd = open(filename.c_str(), O_RDONLY);
  mDefaultVolume = 10;
}

SyntiantAudioPlaySession::~SyntiantAudioPlaySession() {
  close(mFd);
}

int SyntiantAudioPlaySession::start() {
  int status = 0;
  pMediaPlayer = new MediaPlayer();

  if (pMediaPlayer == NULL) {
    ALOGE("%s : error while allocating MediaPlayer", __func__);
    return -EINVAL;
  }

  status = pMediaPlayer->setDataSource(mFd, 0, 0x7fffffffL);
  if (status < 0) {
    ALOGE("%s : error while setDataSource() status = %d", __func__, status);
    goto error;
  }

  status = pMediaPlayer->setVolume(mDefaultVolume, mDefaultVolume);
  if (status < 0) {
    ALOGE("%s : error while setVolume() status = %d", __func__, status);
    goto error;
  }

  status = pMediaPlayer->prepare();
  if (status < 0) {
    ALOGE("%s : error while prepare() status = %d", __func__, status);
    goto error;
  }

  status = pMediaPlayer->start();
  if (status < 0) {
    ALOGE("%s : error while start() status = %d", __func__, status);
    goto error;
  }

  while (!pMediaPlayer->isPlaying()) {
    sleep(1);
    ALOGE("%s : waiting for player to start playing", __func__);
  }

error:
  return status;
}

int SyntiantAudioPlaySession::setVolume(float volume) {
  int status = 0;
  status = pMediaPlayer->setVolume(volume, volume);
  if (status < 0) {
    ALOGE("%s : error while setVolume() status = %d", __func__, status);
  }

  return status;
}

void SyntiantAudioPlaySession::stop() {
  if (pMediaPlayer != NULL) {
    if (pMediaPlayer->isPlaying()) {
      pMediaPlayer->stop();
    }

    pMediaPlayer.clear();
  }
}

int SyntiantAudioPlaySession::isPlaying() {
  return pMediaPlayer->isPlaying();
}
