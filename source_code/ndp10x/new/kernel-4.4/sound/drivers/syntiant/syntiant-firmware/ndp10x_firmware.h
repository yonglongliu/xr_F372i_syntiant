/*
 * SYNTIANT CONFIDENTIAL
 *
 * _____________________
 *
 * Copyright (c) 2017-2018 Syntiant Corporation
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

/*
 * NOTE:
 * 1. this header file is used in the syntiant-ilib repo as an interface file
 * 2. Some compilers do not pack the structs which means they are always 32 bit
 * aligned so they are explicitly made 32 bit aligned so please follow that.
 */

#ifndef NDP10X_FIRMWARE_H
#define NDP10X_FIRMWARE_H

#include <syntiant-firmware/ndp10x_mb.h>
#include <syntiant-firmware/ndp10x_result.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FW_VER "##FIRMWARE_VERSION##"

#define FW_VER_SIZE 24

/**
 * @brief defines constants used by ilib code to store configuration in
 * ndp memory. This constants define beginning and length of free memory
 * location.
 *
 */
enum ndp10x_memory_region_constants_e {
    NDP10X_ILIB_SCRATCH_ORIGIN = 0x20017000,
    NDP10X_ILIB_SCRATCH_LENGTH = 0x01000 /* 4K */
};

/**
 * @brief indexes for addresses of various components of firmware. The
 * addresses are stored in fw_state.addresses and this constants are used
 * as indices for addresses array.
 *
 */
enum ndp10x_fw_state_address_indices_e {
    NDP10X_FW_STATE_ADDRESS_INDEX_FW_STATE = 0x0,
    NDP10X_FW_STATE_ADDRESS_INDEX_GAIN_CONTROLLER = 0x1,
    NDP10X_FW_STATE_ADDRESS_INDEX_POSTERIOR_STATE = 0x2,
    NDP10X_FW_STATE_ADDRESS_INDEX_SMAX_SMOOTHER = 0x3,
    NDP10X_FW_STATE_ADDRESS_INDEX_POSTERIOR_PARAMS = 0x4,
    NDP10X_FW_STATE_ADDRESS_INDEX_TANK_START = 0x5,
    NDP10X_FW_STATE_ADDRESS_INDEX_TANK_END = 0x6
};
    
/**
 * @brief addresses for various firmware data structures to be located
 * by the ILib.  This structure is placed immediately preceeding the
 * first instruction to be executed (thus can be located by following
 * the starting address vector and working 'backwards').  The population is
 * done in startup.s and its size MUST NOT CHANGE without changing
 * the code in startup.s.
 */
struct ndp10x_fw_state_pointers_s {
    uint32_t addresses[8];
};

/* this structure is defined in startup.s */
extern struct ndp10x_fw_state_pointers_s fw_state_pointers;

enum ndp10x_firmware_constants_e {
    NDP10X_MATCH_RING_SIZE = 2,
    NDP10X_MATCH_SNR_SHIFT = 8,
    NDP10X_MATCH_SNR_MASK  = 0xff << NDP10X_MATCH_SNR_SHIFT,
    NDP10X_MATCH_CONFIDENCE_SHIFT = 16,
    NDP10X_MATCH_CONFIDENCE_MASK  = 0xff << NDP10X_MATCH_CONFIDENCE_SHIFT
};

struct ndp10x_fw_match_s {
    uint32_t summary;
    uint32_t tankptr;
};
    
/**
 * @brief Data Structure for storing firmware state.
 * Note: Please do not change this data struture because this struture is used
 * by ilib running on host for operational or debugging purposes.
 * Please add your data after this data structure and set the address for your
 * data in fw_state_pointers.
 *
 */
struct ndp10x_fw_state_s {
    /*
     * These members must be in fixed locations and never change.
     * The uILib relies on these members and does not include
     * the firmware header files.
     */
    uint32_t tankptr;     /* tank pointer with all 17 bits */
    uint32_t match_ring_size;
    uint32_t match_producer;
    struct ndp10x_fw_match_s match_ring[NDP10X_MATCH_RING_SIZE];

    /*
     * the remaining members should be kept in a stable location but
     * will be accessed through these header files by the full ILib
     * so there is less pain of death to moving them
     */
    uint32_t debug;                   /* debug scratch area */
    uint32_t enable;
    uint32_t prev_enable;

    uint32_t tank_reset;              /* host->mcu set to 1 when tank reset */
    
    uint32_t result_fifo_full;
    
    /* interrupt counters*/
    uint32_t mb_int_count;
    uint32_t freq_int_count;
    uint32_t dnn_int_count;
    uint32_t unknown_int_count;

    /* firmware version */
    char version[FW_VER_SIZE];

    /* mailbox state */
    struct ndp10x_mb_state_s mb_state;

    /* frame results after posterior computations */
    struct ndp10x_result_s result;

};

extern struct ndp10x_fw_state_s fw_state;

/**
 * @brief enum values to enable/disable various components of firmware. This
 * constants will also be used by ilib code to enable/disable the components.
 *
 * Note: please release header files to ilib if you change this constants.
 */
enum ndp10x_fw_state_address_enable_e {
    NDP10X_FW_STATE_ENABLE_AGC = 0x1,
    NDP10X_FW_STATE_ENABLE_POSTERIOR = 0x2,
    NDP10X_FW_STATE_ENABLE_SMAX_SMOOTHER = 0x4
};

/**
 * @brief enum values to reset various components of firmware. This
 * constants will also be used by ilib code to reset the components.
 *
 * Note: please release header files to ilib if you change this constants.
 */
enum ndp10x_fw_state_address_reset_e {
    NDP10X_FW_STATE_RESET_POSTERIOR_STATE = 0x1,
    NDP10X_FW_STATE_RESET_SMAX_SMOOTHER = 0x2,
    NDP10X_FW_STATE_RESET_POSTERIOR_PARAMS = 0x4
};

/**
 * @brief handles the dnn interrupt. This interrupt happens whenever dnn block
 * completes one inference.
 *
 * @param fw_state
 */
void ndp10x_dnn_int_handler(struct ndp10x_fw_state_s *fw_state);

/**
 * @brief handles the mailbox interrupt. This interrupt happens whenever there
 * is a new Request from host or Response from host.
 *
 * @param fw_state
 */
void ndp10x_mb_int_handler(struct ndp10x_fw_state_s *fw_state);

/**
 * @brief handles the freq block interrupt. This interrupt happens whenever
 * freq block is done computing filterbanks for a frame.
 * @param fw_state
 */
void ndp10x_freq_int_handler(struct ndp10x_fw_state_s *fw_state);

#ifdef __cplusplus
}
#endif
#endif /* NDP10X_FIRMWARE_H */
