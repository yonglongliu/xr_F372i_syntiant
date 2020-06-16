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

#ifndef NDP10X_PH_H
#define NDP10X_PH_H

#ifdef __cplusplus
extern "C" {
#endif

#include <syntiant-firmware/ndp10x_result.h>

#ifdef CORTEX_M0
typedef uint32_t pointer_t;
#else
typedef void *pointer_t;
#endif

enum ndp10x_ph_action_e {
    NDP10X_PH_ACTION_STATE_M = 0x7f,
    NDP10X_PH_ACTION_STAY = NDP10X_PH_ACTION_STATE_M,
    NDP10X_PH_ACTION_MATCH_M = 0x80
};

enum ndp_ph_action_e {
    NDP_PH_ACTION_MATCH = 0,
    NDP_PH_ACTION_STATE = 1,
    NDP_PH_ACTION_STAY = 2
};

/**
 * @brief parameters for each class
 *
 */
struct ndp10x_ph_class_params_s {
    uint32_t window;
    uint32_t threshold;
    uint32_t backoff;
    uint32_t action;
    uint32_t smoothing_queue_size;
};

/**
 * @brief parameters for each state
 *
 */
struct ndp10x_ph_state_params_s {
    uint32_t timeout;
    uint32_t timeout_action;
    uint32_t class_params_offset;
};

/**
 * @brief All parameters used by posterior handler
 *
 */
struct ndp10x_ph_params_s {
    uint32_t num_classes;
    uint32_t num_states;
    /* the frame processing function to be used */
    uint32_t ph_type;
    /* place holder of 1 state (3 words) with 64 classes (5 words/class) */
    uint32_t params_memory[3 + 64 * 5];
};

/**
 * @brief Posterior Handler state
 *
 */
struct ndp10x_ph_state_s {
    uint32_t current_class;
    uint32_t class_counts[NDP10X_RESULT_NUM_CLASSES];
    uint32_t current_state;
    uint32_t backoff_counter;
    uint32_t timeout;
    uint32_t timeout_action;
    uint32_t frame_counter;
    uint32_t winner;
    uint32_t winner_prob;
    uint32_t threshold;
    uint32_t match;
    uint32_t window;
    uint32_t tankptr;
    uint32_t class_tankptr;

    pointer_t params_addr;
};

/**
 * @brief intializes the posterior handler params. It sets the memory to zero.
 *
 * @param params
 */
void ndp10x_ph_params_init(struct ndp10x_ph_params_s *params);

/**
 * @brief initializes the Softmax smoother from posterior handler params.
 *
 * @param ph_params Posterior Handler params
 * @param lengths smoothing queue lengths
 */
void ndp10x_ph_get_smoother_queue_lengths(struct ndp10x_ph_params_s *ph_params,
                                          uint8_t *lengths);

/**
 * @brief intializes the posterior handler. It sets the memory to zero.
 *
 * @param ph_state Posterior handler state
 */
void ndp10x_ph_init(struct ndp10x_ph_state_s *ph_state);

/**
 * @brief process a new frame result and picks a winner
 *
 * @param ph_state Posterior handler state
 */
void ndp10x_ph_process_frame(struct ndp10x_ph_state_s *ph_state,
                             struct ndp10x_result_s *result, uint32_t tankptr);

/**
 * @brief uses a single counter to keep track of the winning classes. This  the default
 *   frame processor.
 *
 * @param ph_state Posterior handler state
 */
void ndp10x_single_counter_frame_processor(struct ndp10x_ph_state_s *ph_state,
                                           struct ndp10x_result_s *result, uint32_t tankptr);

/**
 * @brief uses a per class counter to keep track of the winning classes. We do a state
 *  transition when a winning class has been detected (i.e., it has meet the per class
 *  threshold for the per class defined number of frames or window size). In this design,
 *  we stay in the current state for time_out (defined per state) number of frames and
 *  then trainsition to the state defined by the timeout_action. The current implementation
 *  does NOT use backoff and leverages states to avoid redundant predictions. Further, we
 *  assume the per class variables, threshold and window size, are the same across all the
 *  states. This is necessary to have a valid count when we move from one state to another 
 *  (note that the counters persist through state transitions). Lastly, window size can be
 *  used to enforce which class is currently active (i.e., pick a large window size for the
 *  classes that are not active in the current state).
 *
 * @param ph_state Posterior handler state
 * @param result The result structure
 * @param tankptr The audio tank pointer
 */
void ndp10x_multi_counter_frame_processor(struct ndp10x_ph_state_s *ph_state,
                                          struct ndp10x_result_s *result, uint32_t tankptr);

#ifdef __cplusplus
}
#endif

#endif
