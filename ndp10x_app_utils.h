/*
 * SYNTIANT CONFIDENTIAL
 * _____________________
 *
 *   Copyright (c) 2018 Syntiant Corporation
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

#ifndef DEMO_UTILS_H
#define DEMO_UTILS_H

#include "syntiant_ilib/syntiant_ndp10x.h"

#define STRING_LEN 256
#define MAX_LABELS 64
#define CHUNK_SIZE 8192

struct demo_statistics_s {
  uint64_t isrs;
  uint64_t polls;
  uint64_t frames;
  uint64_t results;
  uint64_t results_dropped;
  int result_ring_used;
  uint64_t extracts;
  uint64_t extract_bytes;
  uint64_t extract_bytes_dropped;
  int extract_ring_used;
  uint64_t sends;
  uint64_t send_bytes;
  int send_ring_used;
};

extern int transfer(int ndpfd, int mcu, uint32_t addr, uint8_t* out, uint8_t* in, int count);
extern void load_pkg(int ndpfd, char* path);
extern void get_config(int ndpfd, char*** labels, int* classes);
extern void initialization(int ndpfd, int spi);
extern void watch(int ndpfd, int num_watches, int print_stats, int extract_mode, int extract_len_ms,
                  int post_keyword_len_ms);
extern void stats(int ndpfd, int print, int clear, struct demo_statistics_s* stats);

extern void load_synpkgs(int ndpfd, char* __fw_name, char* __package_name);
extern int ndp10x_play(char* play_file, char* fw_name, char* package_name);
extern int ndp10x_record(int rec_sec, int nRecds, char* fw_name, char* package_name);

#endif
