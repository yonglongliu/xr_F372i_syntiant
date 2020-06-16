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

#ifndef NDP10X_RESULTS_H
#define NDP10X_RESULTS_H

#ifdef __cplusplus
extern "C" {
#endif

enum ndp10x_results_constants_e {
    NDP10X_RESULT_NUM_CLASSES = 64,
    NDP10X_RESULT_SOFTMAX_SMOOTHER_MAX_QUEUE_SIZE = 12
};

/**
 * @brief Stores all the results collected from hardware and firmware
 *
 */
struct ndp10x_result_s {
    uint32_t num_classes;
    uint32_t max_index;
    uint8_t raw_strengths[NDP10X_RESULT_NUM_CLASSES];
    uint32_t softmax_strengths[NDP10X_RESULT_NUM_CLASSES];
    uint32_t winner_one_hot[2];
    uint32_t summary; /* formatted the same as NDP10X_SPI_MATCH */
    uint32_t tankptr;
};

/**
 * @brief intializes the result object
 *
 * @param result Result Object
 */
void ndp10x_result_init(struct ndp10x_result_s *result);

/**
 * @brief Computes the softmax on activations read from hardware.
 *
 * @param result Result Object
 */
void ndp10x_result_compute_softmax(struct ndp10x_result_s *result);

/**
 * @brief reads the activations from hardware registers
 *
 * @param result Result object
 */
void ndp10x_result_read_activations(struct ndp10x_result_s *result);

/**
 * @brief Represents the Softmax Smoother
 *
 */
struct ndp10x_result_softmax_smoother_s {
    uint32_t queue_curr_ptr[NDP10X_RESULT_NUM_CLASSES];
    uint32_t queues[NDP10X_RESULT_NUM_CLASSES]
                   [NDP10X_RESULT_SOFTMAX_SMOOTHER_MAX_QUEUE_SIZE];
};

/**
 * @brief intializes the softmax smoother
 *
 * @param smoother Smoother object
 */
void ndp10x_result_softmax_smoother_init(
    struct ndp10x_result_softmax_smoother_s *smoother);


/**
 * @brief smoothes the softmax probabilties.
 *
 * @param smoother Smoother object
 * @param result result object
 */
void ndp10x_result_softmax_smoother_do_smoothing(
    struct ndp10x_result_softmax_smoother_s *smoother,
    uint8_t *lengths, struct ndp10x_result_s *result);

#ifdef __cplusplus
}
#endif
#endif
