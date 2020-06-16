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

#ifndef NDP10X_MB_H
#define NDP10X_MB_H

#ifdef __cplusplus
extern "C" {
#endif

#include <syntiant-firmware/ndp10x_result.h>

/**
 * @brief defines the states of MCU to HOST mailbox exchange
 *
 */
enum m2h_state_e { M2H_STATE_IDLE = 0, M2H_STATE_MATCH = 1 };

/**
 * @brief defines the states of HOST to MCU mailbox exchange
 *
 */
enum h2m_state_e {
    H2M_STATE_IDLE = 0,
    H2M_STATE_EXTOP = 1,
    H2M_STATE_DATA_OUT = 2
};

/**
 * @brief define constants to extract information from mailbox.
 *
 */
enum ndp10x_mb_protcol_e {
    NDP10X_MB_HOST_TO_MCU_OWNER = 0x08,
    NDP10X_MB_HOST_TO_MCU_M = 0x07,
    NDP10X_MB_HOST_TO_MCU_S = 0,
    NDP10X_MB_MCU_TO_HOST_OWNER = 0x80,
    NDP10X_MB_MCU_TO_HOST_M = 0x70,
    NDP10X_MB_MCU_TO_HOST_S = 4
};

#define NDP10X_MB_HOST_TO_MCU_INSERT(m, r)                                     \
    ((((m) & ~NDP10X_MB_HOST_TO_MCU_M) ^ NDP10X_MB_HOST_TO_MCU_OWNER)          \
        | ((r) << NDP10X_MB_HOST_TO_MCU_S))

#define NDP10X_MB_MCU_TO_HOST_INSERT(m, r)                                     \
    ((((m) & ~NDP10X_MB_MCU_TO_HOST_M) ^ NDP10X_MB_MCU_TO_HOST_OWNER)          \
        | ((r) << NDP10X_MB_MCU_TO_HOST_S))

/* short and long request codes are numbered contiguously (for now) */
enum ndp10x_mb_request_e {
    NDP10X_MB_REQUEST_NOP = 0x0,
    NDP10X_MB_REQUEST_CONT = 0x1,
    NDP10X_MB_REQUEST_MATCH = 0x2,
    NDP10X_MB_REQUEST_EXTOP = 0x3,
    NDP10X_MB_REQUEST_DATA = 0x4,
    NDP10X_MB_REQUEST_MIADDR = 0x8
};
#define NDP10X_MB_REQUEST_DECODER                                              \
    {                                                                          \
        "nop", "cont", "match", "extop", "data(0)", "data(1)", "data(2)",      \
            "data(3)", "miaddr"                                                \
    }

/* response codes (3-bit) */
enum ndp10x_mb_response_e {
    NDP10X_MB_RESPONSE_SUCCESS = 0x0,
    NDP10X_MB_RESPONSE_CONT = NDP10X_MB_REQUEST_CONT,
    NDP10X_MB_RESPONSE_ERROR = 0x2,
    NDP10X_MB_RESPONSE_DATA = NDP10X_MB_REQUEST_DATA
};
#define NDP10X_MB_RESPONSE_DECODER                                             \
    {                                                                          \
        "success", "cont", "error", "ILLEGAL(3)", "data(0)", "data(1)",        \
            "data(2)", "data(3)"                                               \
    }

/* error codes (6-bit) */
enum ndp10x_mb_error_e {
    NDP10X_MB_ERROR_FAIL = 0x0,
    NDP10X_MB_ERROR_UNEXPECTED = 0x1
};
#define NDP10X_MB_ERROR_DECODER                                                \
    {                                                                          \
        "fail", "unexpected"                                                   \
    }

enum ndp10x_mb_match_offsets_e {
    NDP10X_MB_MATCH_SUMMARY_O = 0x00,
    NDP10X_MB_MATCH_BINARY_O = 0x04,
    NDP10X_MB_MATCH_STRENGTH_O = 0x0c
};

/**
 * @brief Mailbox state object
 *
 */
struct ndp10x_mb_state_s {
    uint32_t enable_match_for_every_frame;

    uint32_t m2h_state;
    uint32_t m2h_req;
    uint32_t m2h_rsp_success;
    uint32_t m2h_rsp_unknown;
    uint32_t m2h_match_skipped;

    uint32_t h2m_state;
    uint32_t h2m_req_nop;
    uint32_t h2m_req_extop;
    uint32_t h2m_req_data;
    uint32_t h2m_req_cont;

    uint32_t h2m_unexpected_nop;
    uint32_t h2m_unexpected_extop;
    uint32_t h2m_unexpected_cont;
    uint32_t h2m_unexpected_data;
    uint32_t h2m_req_unknown;
    uint32_t h2m_extop_unknown;

    uint32_t h2m_data;
    uint32_t h2m_data_count;

    uint32_t previous_mbox;
};

/**
 * @brief Handles M2H responses & H2M requests.
 *
 * @param mb_state NDP10X firmware mailbox state object
 */
void ndp10x_mb_respond(struct ndp10x_mb_state_s *mb_state);

/**
 * @brief Sends a M2H request to host
 *
 * @param mb_state NDP10X firmware mailbox state object
 * @param req Request to be sent to Host
 */
void ndp10x_mb_send_m2h_request(
    struct ndp10x_mb_state_s *mb_state, uint8_t req);

/**
 * @brief Send a MATCH request to host based on result
 *
 * @param mb_state  NDP10X firmware mailbox state object
 * @param result result calculated by posterior handler.
 */
void ndp10x_mb_send_match(
    struct ndp10x_mb_state_s *mb_state, struct ndp10x_result_s *result);

#ifdef __cplusplus
}
#endif

#endif
