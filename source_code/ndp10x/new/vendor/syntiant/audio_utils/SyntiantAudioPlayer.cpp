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
  printf("syntiant_audio_player \t --wav_file {-w} <PATH_TO_WAV_FILE> default:None \n");
}

int main(int argc, char** argv) {
  int c;
  int option_index = 0;

  char wav_file[256] = { 0 };

  while (1) {
    static struct option long_options[] = { { "wav_file", required_argument, 0, 'w' },
                                            { "help", no_argument, 0, 'h' },
                                            {
                                                0, 0, 0, 0,
                                            } };

    c = getopt_long(argc, argv, "w:h:", long_options, &option_index);
    if (c == -1) break;

    switch (c) {
      case 0:
        break;
      case 'w':
        strcpy(wav_file, optarg);
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
  int status = 0;

  android::ProcessState::self()->startThreadPool();

  string filename = string(wav_file);

  SyntiantAudioPlaySession psession(filename);
  status = psession.start();

  if (status < 0) {
    std::cerr << "Error while starting play session" << endl;
  }

  while (psession.isPlaying()) {
    sleep(1);
  }

  psession.stop();
  return 0;
}