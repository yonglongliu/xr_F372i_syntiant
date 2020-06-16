/*
 * SYNTIANT CONFIDENTIAL
 *
 * _____________________
 *
 * Copyright (c) 2017-2020 Syntiant Corporation
 * All Rights Reserved.
 *
 * NOTICE:  All information contained herein is, and remains the property of
 * Syntiant Corporation and its suppliers, if any.  The intellectual and
 * technical concepts contained herein are proprietary to Syntiant Corporation
 * and its suppliers and may be covered by U.S. and Foreign Patents, patents in
 * process, and are protected by trade secret or copyright law.  Dissemination
 * of this information or reproduction of this material is strictly forbidden
 * unless prior written permission is obtained from Syntiant Corporation.
 *
 */

#include <syntiant_ilib/syntiant_portability.h>

int ERROR_PRINTF(const char * fmt, ...){
    int ret;
    va_list args;
    va_start(args, fmt);
    #ifdef __KERNEL__
    ret = vprintk(fmt, args);
    #else
    ret = vfprintf(stderr, fmt, args) + fprintf(stderr, "\n");
    #endif
    va_end(args);
    return ret;
}