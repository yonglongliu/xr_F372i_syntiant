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
#define LOG_TAG "SyntiantBargeIn::C_API"
#define LOG_NDEBUG 0

#include <utils/Log.h>

#include "SyntiantBargeIn.h"
#include "syntiant_barge_in_c_api.h"

int syntiant_barge_in_init(void** bHandle) {
  ALOGI("%s : enter", __func__);
  SyntiantBargeIn* barge_in = new SyntiantBargeIn();
  if (barge_in == nullptr) {
    ALOGE("%s : error while allocating ", __func__);
    return -ENOMEM;
  }

  int status = barge_in->init();
  if (status < 0) {
    ALOGE("%s : error while init ", __func__);
    return status;
  }

  *bHandle = barge_in;
  ALOGI("%s : exit", __func__);
  return 0;
}

int syntiant_barge_in_start(void* bHandle) {
  ALOGI("%s : enter", __func__);
  SyntiantBargeIn* barge_in = (SyntiantBargeIn*)bHandle;
  return barge_in->start();
}

int syntiant_barge_in_stop(void* bHandle) {
  ALOGI("%s : enter", __func__);
  SyntiantBargeIn* barge_in = (SyntiantBargeIn*)bHandle;
  return barge_in->stop();
}

void syntiant_barge_in_uninit(void* bHandle) {
  ALOGI("%s : enter", __func__);
  SyntiantBargeIn* barge_in = (SyntiantBargeIn*)bHandle;
  barge_in->uninit();
  delete barge_in;
  ALOGI("%s : exit", __func__);
}
