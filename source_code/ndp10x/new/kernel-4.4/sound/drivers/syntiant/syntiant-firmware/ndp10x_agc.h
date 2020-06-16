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

#ifndef NDP10X_AGC_H
#define NDP10X_AGC_H

#ifdef __cplusplus
extern "C" {
#endif

enum ndp10x_agc_constants_e {
    NDP10X_AGC_MAX_FILTER_BANK = 38,
    NDP10X_AGC_MIN_FILTER_BANK = 1,
    NDP10X_AGC_FILTER_BANK_Q = 2 /* Filterbank output is Q2 */
};

struct max_gain_adj_t {
    uint8_t max_neg_adjust_gain;
    uint8_t max_pos_adjust_gain;
    uint8_t reserved[2];
};

struct ndp10x_agc_state_s {
    uint32_t frame_counter;
    int32_t reference_gain;                    /* Q0*/
    int32_t reference_noise_threshold;         /* Q23*/
    int32_t adjustment_gain_hysteresis;        /* Q23*/
    struct max_gain_adj_t max_adjustment_gain; /* Q0*/

    int32_t min_threshold;     /* Q23*/
    int32_t min_increase;      /* Q23*/
    int32_t when_to_reset_min; /* Q0*/

    int32_t shwd_reset_period; /* Q0 Reset the shadow tracker every so often*/
    int32_t shdw_tracker_confdnc;  /* Q0 Number of updates before shadow*/
                                   /* tracker is considered*/
    int32_t thr_reset_min_to_shdw; /* Q23 Reset only if the shadow tracker is
                                    at least thrResetMintoShdw above the
                                    min tracker*/

    int32_t timeout_after_adj_gain; /* Q0*/

    int8_t filterbanks[40];     /* Q0*/
    int32_t adjustment_gain;    /* Q0*/
    int32_t timeout_adj_gain;   /* Q0*/
    int32_t sum_logbank_energy; /* Q23*/

    int32_t min_tracker;            /* Q23*/
    int32_t consecutive_min_update; /* Q0*/

    int32_t min_tracker_shdw; /* Q23 Shadow min tracker that gets reset every
                               shdwReset and used to reset the minTracker*/
    int32_t
        shdw_reset_timer; /* Q0 Timer for periodic reset of the shadow timer*/
    int32_t shdw_tracker_upds; /* Q0 Counter of updates to the shadow tracker*/

    int32_t smoothed_min_tracker; /* Q23*/

    /*******************************************/
    /* Constants and state memory for Gen2 AGC */
    /*******************************************/

    /* Constant */
    int32_t nom_speech_quiet; /* Reference level for speech in quiet (-90) */

    /* Gen2 adaptive noise smoothing constant */
    int32_t alpha_noise_shift_base;     /* alpha_noise in the form of
                                          (1-1/2^alpha_noise_shift), e.g.
                                          (1-2^(-7)) */
    int32_t alpha_noise_tracking_shift; /* alpha_noise_tracking in the form of
                                          (1-1/2^alpha_noise_tracking_shift),
                                          e.g. (1-2^(-7))
                                          */

    /* Gen2 adaptive noise smoothing state memory */
    int32_t noise_level;          /* Smoothed noise level */
    int32_t noise_level_tracking; /* Smoothed tracking between estimated and */
                                  /* instant speech level */

    /*  Gen2 speech level estimation constants */
    int32_t max_thrs;
    int32_t max_decrease; /* Drift updownwards by 15dB per second in absense */
                          /* of max update */
    int32_t when_to_reset_max;  /* Reset max tracker after 2 seconds of no */
                                /* down-update */
    int32_t thr_to_clear_noise; /* Level above noise floor (10 dB) to allow */
                                /* max tracker to drift lower */
    int32_t alpha_speech_shift_base; /* AlphaSpeech in the form of */
                                     /* (1-1/2^AlphaSpeechShift), e.g. */
                                     /* (1-2^(-7)) */
    int32_t
        alpha_speech_tracking_shift; /* AlphaSpeechTracking in the form of */
                                     /* (1-1/2^alpha_speech_tracking_shift), */
                                     /* e.g. (1-2^(-7))    */

    /* Gen2 speech level estimation state memory */
    int32_t max_tracker;          /* Initialize max tracker to minimum  */
    int32_t max_tracker_shdw;     /* Initialize shadow max tracker to minimum */
    int32_t smoothed_max_tracker; /* Smoothed max tracker - initialized on */
                                  /* the "safe" side of nominal */
    int32_t shdw_max_tracker_upds; /* Counter of updates to the shadow max */
                                   /* tracker */
    int32_t consec_no_max_upd;     /* Number of consecutive frames without */
                                   /* up-update of the maximum tracker */
    int32_t speech_level;          /* Smoothed speech level */
    int32_t speech_level_tracking; /* Smoothed tracking between estimated and */
                                   /* instant speech level */
};

/**
 * @brief initializes the agc state.
 *
 * @param astate agc state object
 */
void ndp10x_agc_state_init(struct ndp10x_agc_state_s *astate);

/**
 * @brief process a new frames & adjusts the mic gain based on stats from new
 * frame.
 *
 * @param astate agc state object
 *
 * @return 8-bit SNR estimation
 */
uint32_t ndp10x_agc_process_new_frame(struct ndp10x_agc_state_s *astate);

#ifdef __cplusplus
}
#endif

#endif
