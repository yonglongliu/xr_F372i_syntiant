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

#ifndef SOUND_TRIGGER_NDP10X_H
#define SOUND_TRIGGER_NDP10X_H

#include <stdint.h>

#define SYNTIANT_SOUND_TRIGGER_OPAQUE_VERSION_V1 1
/* opaque data matching SYNTIANT_SOUND_TRIGGER_OPAQUE_VERSION_V1 */
struct syntiant_ndp10x_recognition_event_opaque_v1_s {
  uint32_t start_of_keyword_ms;
  uint32_t end_of_keyword_ms;
  uint32_t audio_data_len; /* number of entries in audio_data array */
  int16_t audio_data[];
};

union syntiant_ndp10x_recognition_event_opaque_u {
  struct syntiant_ndp10x_recognition_event_opaque_v1_s opaque_v1;
};

struct syntiant_ndp10x_recognition_event_opaque_s {
  uint32_t version;
  union syntiant_ndp10x_recognition_event_opaque_u u;
};

enum ndp_recognition_score_validation_mapping_e {
  UTTERANCE_SATURATED = 1,
  UTTERANCE_TOO_NOISY = 2,
  UTTERANCE_TOO_SOFT  = 3
};
#endif
