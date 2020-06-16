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
#ifndef SYNTIANT_NDP_PORTABILITY_H
#define SYNTIANT_NDP_PORTABILITY_H

/* integers, strings, etc */
#ifdef __KERNEL__
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/slab.h>
#else
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#ifdef WINDOWS_KERNEL
#include <wdm.h>
#endif
#endif

/* run-time assert */
#if defined(__KERNEL__) || defined(WINDOWS_KERNEL) || defined(_MSC_VER)
#ifdef assert
#undef assert
#endif
#define assert(x)
#endif

/* compile-time assert */
#define SYNTIANT_ASSERT_CONCAT_(a, b) a##b
#define SYNTIANT_ASSERT_CONCAT(a, b) SYNTIANT_ASSERT_CONCAT_(a, b)

#if defined(__MINGW32__) || defined(WIN32)
#define SYNTIANT_CASSERT(e, m)
#else
#ifdef __COUNTER__
#define SYNTIANT_CASSERT(e, m) \
    enum { SYNTIANT_ASSERT_CONCAT(static_assert_, __COUNTER__) \
           = 1/(int)(!!(e)) };
#else
#define SYNTIANT_CASSERT(e, m) \
    enum { SYNTIANT_ASSERT_CONCAT(assert_line_, __LINE__) \
           = 1/(int)(!!(e)) };
#endif
#endif

#ifndef PACK_DATA
#define SYNTIANT_PACK __attribute__((packed))
#else
#define SYNTIANT_PACK
#endif

#ifdef __KERNEL__
#define SYNTIANT_PRINTF printk
#else
#ifndef _MSC_VER
#define SYNTIANT_PRINTF printf
#else
#define SYNTIANT_PRINTF DbgPrint
#endif
#endif

int ERROR_PRINTF(const char * fmt, ...);

/* snprintf */
#if defined(__KERNEL__) || defined(WINDOWS_KERNEL)
#else

#define SYNTIANT_SNPRINTF

/* #include <strings.h> */
#include <ctype.h>
#include <sys/types.h>
#include <stdarg.h>
#include <stdlib.h>

#if !defined(HAVE_SNPRINTF)
int snprintf(char *str,size_t count,const char *fmt,...);
int asprintf(char **ptr, const char *format, ...);
#endif

#if !defined(HAVE_VSNPRINTF)
int vsnprintf (char *str, size_t count, const char *fmt, va_list args);
int vasprintf(char **ptr, const char *format, va_list ap);
#endif
#endif

#endif
