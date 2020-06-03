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

#ifndef SYNTIANT_AUDIO_SESSION_H
#define SYNTIANT_AUDIO_SESSION_H

#include <media/AudioRecord.h>
#include <media/mediaplayer.h>

namespace android {

class SyntiantAudioRecordSession {
 public:
  SyntiantAudioRecordSession();
  ~SyntiantAudioRecordSession();

  int start(uint32_t sampleRate, size_t channels, size_t bits,
            audio_session_t sessionId = AUDIO_SESSION_ALLOCATE,
            audio_source_t source = AUDIO_SOURCE_MIC);
  int read(void* buffer, size_t num_bytes);
  void stop();

 private:
  sp<AudioRecord> pAudioRecord;
  bool mSessionStarted;
};

class SyntiantAudioPlaySession {
 public:
  SyntiantAudioPlaySession(std::string filename);
  ~SyntiantAudioPlaySession();
  int start();
  int setVolume(float volume);
  int isPlaying();
  void stop();

 private:
  std::string mFilename;
  int mFd;
  float mDefaultVolume;
  sp<MediaPlayer> pMediaPlayer;
};
};

#endif