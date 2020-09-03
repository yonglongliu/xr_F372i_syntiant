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

#define LOG_TAG "SyntiantAudioRecorder"
#define LOG_NDEBUG 0

#include <utils/Log.h>

#include <binder/IPCThreadState.h>
#include <binder/ProcessState.h>
#include <getopt.h>
#include <cstdio>
#include <iostream>

#include <audio_utils/sndfile.h>
#include "SyntiantAudioSession.h"

using namespace std;
using namespace android;

void print_usage() {
  printf("Usage:\n");
  printf(
      "syntiant_audio_recorder "
      " \n \
	\t--wav_file {-w} <PATH_TO_WAV_FILE> default:\"/data/recording.wav\"  \n \
	\t--channels {-c} <NUM OF CHANNELS> default:1 range:[1, 2]\n \
	\t--samplerate {-r} <SAMPLE RATE> default:16000 options: 16000, 32000, 48000\n \
	\t--bits {-b} <NUM OF BITS PER SAMPLE> default:16 options: 16, 32\n \
	\t--source {-d} <AUDIO SOURCE> default:1 range:[1, 6]\n \
	\t--time {-t} <TIME TO RECORD> default:3 seconds range:[1, INF]\n");
}

int main(int argc, char** argv) {
  int c;
  int option_index = 0;

  char wav_file[256] = "/data/recording.wav";
  size_t sampleRate = 16000;
  size_t bits = 16;
  size_t channels = 1;
  audio_session_t sessionId = AUDIO_SESSION_ALLOCATE;
  audio_source_t source = AUDIO_SOURCE_MIC;
  size_t seconds = 3;

  while (1) {
    static struct option long_options[] = { { "wav_file", required_argument, 0, 'w' },
                                            { "channels", required_argument, 0, 'c' },
                                            { "samplerate", required_argument, 0, 'r' },
                                            { "bits", required_argument, 0, 'b' },
                                            { "sessionid", required_argument, 0, 's' },
                                            { "source", required_argument, 0, 'd' },
                                            { "time", required_argument, 0, 't' },
                                            { "help", no_argument, 0, 'h' },
                                            {
                                                0, 0, 0, 0,
                                            } };

    c = getopt_long(argc, argv, "w:c:r:b:s:d:h:t:", long_options, &option_index);
    if (c == -1) break;

    switch (c) {
      case 0:
        break;
      case 'w':
        strcpy(wav_file, optarg);
        break;
      case 'c':
        channels = (size_t)(atoi(optarg));
        break;
      case 'r':
        sampleRate = (size_t)(atoi(optarg));
        break;
      case 'b':
        bits = (size_t)(atoi(optarg));
        break;
      case 's':
        sessionId = (audio_session_t)(atoi(optarg));
        break;
      case 'd':
        source = (audio_source_t)(atoi(optarg));
        break;
      case 't':
        seconds = (size_t)(atoi(optarg));
        break;
      case 'h':
        print_usage();
        return 0;
        break;
      case '?':
        break;
      default:
        abort();
    }
  }
  int recTime = 0;
  const size_t NUM_FRAMES = 160;
  const sf_count_t NUM_SAMPLES = NUM_FRAMES * channels;
  int status = 0;
  SyntiantAudioRecordSession session;
  SNDFILE* file = NULL;
  SF_INFO sfinfo;
  unsigned int format;

  short* samples = new short[NUM_SAMPLES];

  android::ProcessState::self()->startThreadPool();

  if (!samples) {
    cout << "Some error occured while allocating buffer" << endl;
    goto exit;
  }

  memset(&sfinfo, 0, sizeof(sfinfo));

  format = (bits == 16) ? SF_FORMAT_PCM_16 : SF_FORMAT_PCM_32;

  sfinfo.samplerate = sampleRate;
  sfinfo.frames = NUM_FRAMES;
  sfinfo.channels = channels;
  sfinfo.format = (SF_FORMAT_WAV | format);

  status = session.start(sampleRate, channels, bits, sessionId, source);
  if (status) {
    cout << "Some error occured while opening AudioSession" << endl;
    goto exit;
  }

  file = sf_open(wav_file, SFM_WRITE, &sfinfo);
  if (!file) {
    cout << "Error while opening wav file " << endl;
    goto exit;
  }

  recTime = time(0) + seconds;

  while (time(0) < recTime) {
    status = session.read(samples, sizeof(short) * NUM_SAMPLES);
    if (status < 0) {
      cout << "Some error occured while reading AudioSession" << endl;
      goto exit;
    }

    if (sf_writef_short(file, samples, NUM_SAMPLES) != NUM_SAMPLES) {
      cout << "Some error occured while writing audio file" << endl;
      goto exit;
    }
  }

exit:
  delete[] samples;
  sf_close(file);
  session.stop();
  return 0;
}