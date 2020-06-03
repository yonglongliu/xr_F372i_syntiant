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

#define LOG_TAG "SyntiantBargeIn"
// #define LOG_NDEBUG 0

#include <errno.h>
#include <pthread.h>
#include <utils/Log.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <list>
#include <mutex>
#include <vector>

#include "SyntiantBargeIn.h"

#include "SyntiantAudioSession.h"
#include "ndp10x_hal.h"

typedef void* (*THREADFUNCPTR)(void*);

#define FRAME_SIZE 416
// #define FRAME_SIZE 160
#define NUM_SAMPLES_16K_1CH FRAME_SIZE

using namespace std;
using namespace android;

SyntiantBargeIn::SyntiantBargeIn() {
  samples_16b_1ch_vec = list<int32_t>();
}

SyntiantBargeIn::~SyntiantBargeIn() {}

int SyntiantBargeIn::init() {
  int ret = 0;
  ndp_handle = open("/dev/ndp10x", 0);
  if (ndp_handle < 0) {
    ALOGE(" %s : error opening /dev/ndp10x", __func__);
    return -EIO;
  }

  ret = pthread_create(&recorder_thread, NULL, &SyntiantBargeIn::pcm_recorder_thread_main, this);
  if (ret) {
    ALOGE("%s: error starting recorder_thread", __func__);
    goto error;
  }

  ret = pthread_create(&pcm_sender_thread, NULL, &SyntiantBargeIn::pcm_sender_thread_main, this);
  if (ret) {
    ALOGE("%s: error starting pcm_sender_thread", __func__);
    goto error;
  }

#ifdef ENABLE_NDP_DEBUG_RECORDING
  ret = pthread_create(&pcm_extractor_thread, NULL, &SyntiantBargeIn::pcm_extractor_thread_main,
                       this);
  if (ret) {
    ALOGE("%s: error starting pcm_extractor_thread", __func__);
    goto error;
  }
#endif

error:
  return ret;
}

int SyntiantBargeIn::uninit() {
  pthread_join(playback_checker_thread, NULL);
  pthread_join(recorder_thread, NULL);
  pthread_join(pcm_sender_thread, NULL);

#ifdef ENABLE_NDP_DEBUG_RECORDING
  pthread_join(pcm_extractor_thread, NULL);
#endif
  close(ndp_handle);
  return 0;
}

int SyntiantBargeIn::start() {
  ALOGE("%s : starting barge in ", __func__);
  start_barge_in = true;
  return 0;
}

int SyntiantBargeIn::stop() {
  ALOGE("%s : stopping barge in ", __func__);
  start_barge_in = false;
  return 0;
}

void* SyntiantBargeIn::pcm_extractor_thread_main(void* d) {
  SyntiantBargeIn* This = (SyntiantBargeIn*)d;
  FILE* fp;

  while (1) {
    while (!This->start_barge_in) {
      // ALOGI("%s: Waiting for recording to enable ", __func__);
      sleep(1);
      continue;
    }
    fp = fopen(NDP_BARGE_IN_EXTRACT_PATH, "wb");
    if (!fp) {
      ALOGE("%s: error opening %s", __func__, NDP_BARGE_IN_EXTRACT_PATH);
      break;
    }

    int ret = 0;

    short samples[NUM_SAMPLES_16K_1CH];

    ret = ndp10x_hal_pcm_extract(This->ndp_handle, NULL, 0);

    while (This->start_barge_in) {
      ret = ndp10x_hal_pcm_extract(This->ndp_handle, samples, NUM_SAMPLES_16K_1CH);
      if (ret < 0) {
        continue;
      }
      fwrite(samples, sizeof(short), NUM_SAMPLES_16K_1CH, fp);
    }

    fclose(fp);
  }
  return NULL;
}

void* SyntiantBargeIn::pcm_sender_thread_main(void* d) {
  SyntiantBargeIn* This = (SyntiantBargeIn*)d;
  FILE* fp;
  vector<short> samples_16b_1ch(NUM_SAMPLES_16K_1CH);
  size_t sz;

  ALOGI("%s: Thread starting ", __func__);

  while (1) {
    if (!This->start_barge_in) {
      // ALOGI("%s: Waiting for recording to enable ", __func__);
      sleep(1);
      continue;
    }

    fp = fopen(NDP_BARGE_IN_SEND_PATH, "wb");
    if (!fp) {
      ALOGE("%s: Could not open file %s", __func__, NDP_BARGE_IN_SEND_PATH);
      break;
    }

    while (This->start_barge_in) {
      sz = This->samples_16b_1ch_vec.size();
      if (sz < NUM_SAMPLES_16K_1CH) {
        // ALOGI("%s: waiting for more samples to send", __func__);
        // sleep(1);
        // TODO: Possible change this approach
        usleep(1000);
        continue;
      }

      This->sample_lock.lock();
      for (size_t i = 0; i < NUM_SAMPLES_16K_1CH; i++) {
        samples_16b_1ch[i] = This->samples_16b_1ch_vec.front();
        This->samples_16b_1ch_vec.pop_front();
      }

      This->sample_lock.unlock();
      fwrite(samples_16b_1ch.data(), sizeof(short), NUM_SAMPLES_16K_1CH, fp);
      // ALOGI("%s : send", __func__);
      ndp10x_hal_pcm_send(This->ndp_handle, samples_16b_1ch.data(), NUM_SAMPLES_16K_1CH);
    }
    fclose(fp);
  }
  return NULL;
}

void* SyntiantBargeIn::pcm_recorder_thread_main(void* d) {
  SyntiantBargeIn* This = (SyntiantBargeIn*)d;

  short samples[NUM_SAMPLES_16K_1CH];

  SyntiantAudioRecordSession rec_session;

  int ret = 0;

  FILE* fp;

  while (1) {
    if (!This->start_barge_in) {
      // ALOGI("%s: Waiting for recording to enable ", __func__);
      sleep(1);
      continue;
    }
    fp = fopen(NDP_BARGE_IN_RECORD_PATH, "wb");
    if (!fp) {
      ALOGE("%s: error opening %s", __func__, NDP_BARGE_IN_RECORD_PATH);
      break;
    }

    ret = rec_session.start(16000, 1, 16);
    if (ret < 0) {
      ALOGE("%s: unable to start recording ", __func__);
      This->start_barge_in = false;
      continue;
    }

    while (This->start_barge_in) {
      // ALOGI("%s : record", __func__);
      ret = rec_session.read(samples, sizeof(samples));
      // ALOGI("%s : record done", __func__);
      if (ret < 0) {
        break;
      }
      // ALOGI("%s : record read", __func__);

      This->sample_lock.lock();
      This->samples_16b_1ch_vec.insert(This->samples_16b_1ch_vec.end(), &samples[0],
                                       &samples[NUM_SAMPLES_16K_1CH]);
      This->sample_lock.unlock();
      if (fp) {
        fwrite(samples, sizeof(short), NUM_SAMPLES_16K_1CH, fp);
      }
    }

    rec_session.stop();
    if (fp) {
      fclose(fp);
    }
  }

  return NULL;
}
