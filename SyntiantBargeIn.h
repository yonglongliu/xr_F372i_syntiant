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

#ifndef SYNTIANT_BARGE_IN_H
#define SYNTIANT_BARGE_IN_H

#include <list>
#include <mutex>

#include "ndp10x_hal.h"

// #define ENABLE_NDP_DEBUG_RECORDING 1

#define NDP_BARGE_IN_PIPE_PATH "/data/local/media/ndp_barge_in_pipe"
#define NDP_BARGE_IN_RECORD_PATH "/data/local/media/ndp_barge_in_record.raw"
#define NDP_BARGE_IN_SEND_PATH "/data/local/media/ndp_barge_in_send.raw"
#define NDP_BARGE_IN_EXTRACT_PATH "/data/local/media/ndp_barge_in_extract.raw"

class SyntiantBargeIn {
 public:
  SyntiantBargeIn();
  ~SyntiantBargeIn();

  int init();
  int start();
  int stop();
  int uninit();

 private:
  ndp10x_handle_t ndp_handle;
  bool start_barge_in;

  std::list<int32_t> samples_16b_1ch_vec;
  std::mutex sample_lock;

  pthread_t playback_checker_thread = 0;
  pthread_t recorder_thread = 0;
  pthread_t pcm_sender_thread = 0;
#ifdef ENABLE_NDP_DEBUG_RECORDING
  pthread_t pcm_extractor_thread = 0;
#endif

  static void* pcm_recorder_thread_main(void*);
  static void* pcm_sender_thread_main(void*);
  static void* pcm_extractor_thread_main(void*);
};

#endif