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

#define LOG_TAG "SyntiantSoundTriggerTest"
#define LOG_NDEBUG 0

#include <utils/Log.h>

#include <binder/IPCThreadState.h>
#include <binder/ProcessState.h>

#include <getopt.h>
#include <cstdio>
#include <iostream>

#include <cutils/properties.h>

#include "SyntiantSoundTriggerTestSession.h"

using namespace android;
using namespace std;

#define DEFAULT_FW_FILE "/vendor/firmware/hellojio_jio_phone.syngup"
#define DEFAULT_WAV_FILE "/data/aov/syntiant_st_test"
#define DEFAULT_SAVED_MODEL_FILE "/data/syntiant_user_sm.syngup"

void print_usage(char* prog_name) {
  printf("Usage:\n");
  printf("%s \t--firmware_file {-f} <PATH_TO_FIRMWARE_FILE> default:\"" DEFAULT_FW_FILE
         "\"\n \
	\t--saved_sound_model_file {-o} <PATH_TO_SAVED_SOUND_MODEL> default:\"" DEFAULT_SAVED_MODEL_FILE
         "\"\n \
	\t--wav_file {-w} <PATH_PREFIX_TO_WAV_FILES> default:\"" DEFAULT_WAV_FILE
         "\"\n \
	\t--record_len_ms {-r} <How long in ms to record after wakeword detection> default:0 (disabled) range:[0, INF]\n",
         prog_name);
}

int main(int argc, char** argv) {
  int c;
  int option_index = 0;

  char firmware_file[256] = {0};
  char saved_model_file[256] = {0};
  char wav_file[256] = { 0 };
  size_t record_len_ms = 0;
  bool short_output_format = false;
  bool enroll_user = false;
  int user_id = 0;
  unsigned int recognition_mode = RECOGNITION_MODE_VOICE_TRIGGER;
  int eng_mode = 1;
  char eng_mode_str[10] = "";

  static struct option long_options[] = { { "firmware", required_argument, 0, 'f' },
                                          { "saved_model", no_argument, 0, 'o' },
                                          { "wav_file", required_argument, 0, 'w' },
                                          { "record_len_ms", required_argument, 0, 'r' },
                                          { "help", no_argument, 0, 'h' },
                                          { "short_output", no_argument, 0, 's' },
                                          { "enroll", no_argument, 0, 'e' },
                                          { "user_id", required_argument, 0, 'u' },
                                          { "recognition_mode", no_argument, 0, 'm' },
                                          { "engineering_mode", no_argument, 0, 'x' },
                                          {
                                              0,
                                              0,
                                              0,
                                              0,
                                          } };
  strncpy(firmware_file, DEFAULT_FW_FILE, sizeof(firmware_file));
  strncpy(saved_model_file, DEFAULT_SAVED_MODEL_FILE, sizeof(saved_model_file));
  strncpy(wav_file, DEFAULT_WAV_FILE, sizeof(wav_file));

  while (1) {
    c = getopt_long(argc, argv, "ef:o:w:r:m:h:su:x", long_options, &option_index);
    if (c == -1) break;

    switch (c) {
      case 0:
        break;
      case 'f':
        strncpy(firmware_file, optarg, sizeof(firmware_file));
        break;
      case 'o':
        strncpy(saved_model_file, optarg, sizeof(saved_model_file));
        break;
      case 'w':
        strncpy(wav_file, optarg, sizeof(wav_file));
        break;
      case 'r':
        record_len_ms = (size_t)(atoi(optarg));
        break;
      case 'e':
        enroll_user = true;
        break;
      case 's':
        short_output_format = true;
        break;
      case 'm':
        recognition_mode = atoi(optarg);
        break;
      case 'h':
        print_usage(argv[0]);
        return 0;
        break;
      case 'u':
        user_id = atoi(optarg);
        break;
      case 'x':
        eng_mode = 0;
        break;
      case '?':
        break;
      default:
        abort();
    }
  }

  if (getuid()) {
    cout << "Please run as root user" << endl;
    exit(0);
  }

  sprintf(eng_mode_str, "%d", eng_mode);
  property_set("syntiant.sthal.eng_mode", eng_mode_str);

  sprintf(eng_mode_str, "%d", 1000);
  property_set("syntiant.sthal.dump_files", eng_mode_str);

  android::ProcessState::self()->startThreadPool();
  SyntiantFakeVoiceAssistant session(firmware_file, saved_model_file, short_output_format,
                                     enroll_user, user_id, recognition_mode, record_len_ms);
  int status = session.run();
  if (status) {
    cout << "Some error occured " << status << endl;
  }

  exit(0);
  return 0;
}
