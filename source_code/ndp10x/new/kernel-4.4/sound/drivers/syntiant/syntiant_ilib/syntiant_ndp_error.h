/*
 * SYNTIANT CONFIDENTIAL
 * _____________________
 *
 *   Copyright (c) 2018-2020 Syntiant Corporation
 *   All Rights Reserved.
 *
 *  NOTICE:  All information contained herein is, and remains the property of
 *  Syntiant Corporation and its suppliers, if any.  The intellectual and
 *  technical concepts contained herein are proprietary to Syntiant Corporation
 *  and its suppliers and may be covered by U.S. and Foreign Patents, patents
 *  in process, and are protected by trade secret or copyright law.
 *  Dissemination of this information or reproduction of this material is
 *  strictly forbidden unless prior written permission is obtained from
 *  Syntiant Corporation.
 */
#ifndef SYNTIANT_NDP_ERROR_H
#define SYNTIANT_NDP_ERROR_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief error codes
 */
enum syntiant_ndp_errors_e {
    SYNTIANT_NDP_ERROR_NONE = 0,    /**< operation successful */
    SYNTIANT_NDP_ERROR_FAIL = 1,    /**< general failure */
    SYNTIANT_NDP_ERROR_ARG = 2,     /**< invalid argument error */
    SYNTIANT_NDP_ERROR_UNINIT = 3,  /**< device unintialized or no fw loaded */
    SYNTIANT_NDP_ERROR_PACKAGE = 4, /**< package format error */
    SYNTIANT_NDP_ERROR_UNSUP = 5,   /**< operation not supported */
    SYNTIANT_NDP_ERROR_NOMEM = 6,   /**< out of memory */
    SYNTIANT_NDP_ERROR_BUSY = 7,    /**< operation in progress */
    SYNTIANT_NDP_ERROR_TIMEOUT = 8, /**< operation timeout */
    SYNTIANT_NDP_ERROR_MORE = 9,    /**< more data is expected */ 
    SYNTIANT_NDP_ERROR_CONFIG = 10, /**< config error */
    SYNTIANT_NDP_ERROR_CRC = 11,    /**< CRC mismatch */
    SYNTIANT_NDP_ERROR_INVALID_NETWORK = 12, /**< invalid network id */
    SYNTIANT_NDP_ERROR_LAST = SYNTIANT_NDP_ERROR_INVALID_NETWORK
};

#define SYNTIANT_NDP_ERROR_NAMES                                               \
    {                                                                          \
        "none", "fail", "arg", "uninit", "package", "unsup", "nomem", "busy",  \
            "timeout", "more", "config", "crc"                                 \
    }
    
#define SYNTIANT_NDP_ERROR_NAME(e)                                             \
    (((e) < SYNTIANT_NDP_ERROR_NONE || SYNTIANT_NDP_ERROR_LAST < (e))          \
            ? "*unknown*"                                                      \
            : syntiant_ndp_error_names[e])
     
extern char *syntiant_ndp_error_names[];


/**
 * @brief return a string naming the error code
 *
 * @param e the error code
 * @return a pointer to a string naming the error
 */
extern const char *syntiant_ndp_error_name(int e);

#ifdef __cplusplus
}
#endif

#endif