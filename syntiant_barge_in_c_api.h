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

#ifndef SYNTIANT_BARGE_IN_C_API_H
#define SYNTIANT_BARGE_IN_C_API_H
#ifdef __cplusplus
extern "C" {
#endif
int syntiant_barge_in_init(void** bHandle);

int syntiant_barge_in_start(void* bHandle);

int syntiant_barge_in_stop(void* bHandle);

void syntiant_barge_in_uninit(void* bHandle);

#ifdef __cplusplus
}
#endif

#endif