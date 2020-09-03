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

#define LOG_TAG "ndp10x_hal_test"
#define LOG_NDEBUG 0
#include <cutils/log.h>

#define STRING_LEN 256
#define BYTES_PER_AUDIO_SAMPLE (2)
#define BYTES_PER_MILLISECOND (16000 * BYTES_PER_AUDIO_SAMPLE / 1000)

#define MAX(a, b) ((a) > (b) ? (a) : (b))

#include <assert.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "ndp10x_hal.h"

int load_synpkg_from_file(ndp10x_handle_t handle, char* path) {
  // ALOGI("%s : enter", __func__);

  if (path == NULL) {
    fprintf(stderr, "Package path cannot be null\n");
    return -EINVAL;
  }

  struct stat file_state;
  int pfd, r1;
  void* package;
  int s = SYNTIANT_NDP_ERROR_NONE;

  if (stat(path, &file_state) < 0) {
    perror("unable to stat synpkg file");
    exit(1);
  }
  printf("package: %s\n", path);
  printf("file Size: %lld bytes\n", file_state.st_size);

  package = malloc(file_state.st_size);
  pfd = open(path, O_RDONLY);

  if (pfd < 0) {
    perror("unable to open synpkg file");
    exit(1);
  }

  r1 = read(pfd, package, file_state.st_size);
  if (r1 < 0) {
    perror("unable to read synpkg file");
    exit(1);
  }

  if (r1 < file_state.st_size) {
    perror("truncated synpkg file read");
    exit(1);
  }

  s = ndp10x_hal_load(handle, package, (size_t)file_state.st_size);

  free(package);
  if (close(pfd) < 0) {
    perror("package file close failed");
  }

  return s;
}

int load_synpkgs(ndp10x_handle_t handle, char* packages) {
  int s = 0;
  char* cur_package = strtok(packages, ",");
  // ALOGI("%s : enter", __func__);
  while (cur_package != NULL) {
    s = load_synpkg_from_file(handle, cur_package);
    if (s < 0) {
      fprintf(stderr, "error while loading %s", cur_package);
      goto exit;
    }

    cur_package = strtok(NULL, ",");
  }

exit:
  return s;
}

void watch(int ndpfd, int num_matches, int print_stats __unused, int extract_mode,
           int extract_len_ms, int post_keyword_len_ms) {
  int s = 0;
  struct ndp10x_watch_s watch;
  // char **labels;
  struct tm* tm;
  char time[128];
  int num_phrases;
  uint8_t* audio_buffer = NULL;
  struct ndp10x_pcm_extract_s extract;
  FILE* fptr;
  char buf[256];
  int wrt_size;
  int cnt = 0;
  int audio_buf_len = MAX(extract_len_ms, post_keyword_len_ms) * BYTES_PER_MILLISECOND;

  if (extract_mode) {
    audio_buffer = malloc(audio_buf_len);
    if (!audio_buffer) {
      fprintf(stderr, "couldn't allocate buffer for audio data\n");
      exit(1);
    }

    memset(audio_buffer, 0, audio_buf_len);
  }

  s = ndp10x_hal_flush_results(ndpfd);
  if (s < 0) {
    printf("%s : error while flushing results", __func__);
    exit(1);
  }

  memset(&watch, 0, sizeof(watch));
  // get_config(ndpfd, &labels, &classes);
  s = ndp10x_hal_get_num_phrases(ndpfd, &num_phrases);
  if (s < 0) {
    printf("%s : error while getting num phrases", __func__);
    exit(1);
  }
  num_phrases--;

  printf("\nwatching for %d keyword%s\n", num_phrases, num_phrases ? "s" : "");
  while (1) {
    watch.timeout = -1;
    watch.classes = (1 << num_phrases) - 1;

    watch.extract_match_mode = extract_mode;
    watch.extract_before_match = extract_len_ms;

    // if (ioctl(ndpfd, WATCH, &watch) < 0) {
    //     perror("ndp10x watch ioctl failed");
    //     exit(1);
    // }

    s = ndp10x_hal_wait_for_match_and_extract(
        ndpfd, num_phrases - 1, (short*)audio_buffer,
        extract_len_ms * BYTES_PER_MILLISECOND / sizeof(short), NULL);
    if (s < 0) {
      printf("%s : error while extracting activation audio", __func__);
      exit(1);
    }

    if (extract_mode) {
      memset(buf, 0, sizeof(unsigned char) * 32);
      snprintf(buf, 32, "/data/%s%d%s", "pcm_audio", (unsigned int)cnt++, ".raw");
      fptr = fopen(buf, "wb");
      if (!fptr) {
        perror("Couldn't open file for saving audio data");
        exit(1);
      }
      wrt_size = fwrite(audio_buffer, 1, extract_len_ms * BYTES_PER_MILLISECOND, fptr);
      if (ferror(fptr)) {
        perror("PCM data write failed");
      }
      printf("%d bytes written to %s\n", wrt_size, buf);
      fclose(fptr);
    }

    assert(watch.match);
    assert(watch.class_index < num_phrases);
    tm = localtime((const time_t*)&watch.ts.tv_sec);
    strftime(time, sizeof(time), "%Y-%m-%d %H:%M:%S", tm);
    printf("%s match -> %d\n", time, watch.class_index);
    // if (print_stats) {
    //     stats(ndpfd, 1, 1, NULL);
    // }

    if (extract_mode && (post_keyword_len_ms > 0)) {
      fptr = fopen(buf, "ab");
      if (!fptr) {
        perror("Couldn't open file for appending audio data");
        exit(1);
      }

      wrt_size = 0;

      memset(&extract, 0, sizeof(extract));
      extract.buffer = (uintptr_t)audio_buffer;
      extract.buffer_length = post_keyword_len_ms * BYTES_PER_MILLISECOND;

      printf("extract %d bytes of post-keyword audio\n", extract.buffer_length);
      // if (ioctl(ndpfd, PCM_EXTRACT, &extract) < 0) {
      //     perror("ndp10x extract ioctl failed");
      //     exit(1);
      // }

      s = ndp10x_hal_pcm_extract(ndpfd, (short*)audio_buffer,
                                 post_keyword_len_ms * BYTES_PER_MILLISECOND / sizeof(short));

      wrt_size = fwrite(audio_buffer, 1, post_keyword_len_ms * BYTES_PER_MILLISECOND, fptr);
      if (ferror(fptr)) {
        perror("PCM data append failed");
      }

      printf("extracted %d bytes of post-keyword audio\n",
             post_keyword_len_ms * BYTES_PER_MILLISECOND);
      fclose(fptr);
    }

    if (num_matches > 0) {
      num_matches--;
      if (!num_matches) return;
    }
  }
}

int model_recognize(char* packages, int extract_mode, int extract_len, int post_keyword_len) {
  // ALOGI("%s : enter", __func__);
  ndp10x_handle_t handle;

  int s = 0;

  handle = ndp10x_hal_open();

  if (handle < 0) {
    ALOGE("%s : error while opening ndp10x hal ", __func__);
    return EXIT_FAILURE;
  }

  s = ndp10x_hal_init(handle);
  if (s < 0) {
    fprintf(stderr, "Could not Initialize ndp\n");
    goto exit;
  }

  s = load_synpkgs(handle, packages);
  if (s < 0) {
    fprintf(stderr, "Could not load packages\n");
    goto exit;
  }

  s = ndp10x_hal_set_input(handle, NDP10X_HAL_INPUT_TYPE_MIC, 1);
  if (s < 0) {
    fprintf(stderr, "Could not set input\n");
    goto exit;
  }

  watch(handle, -1, 0, extract_mode, extract_len, post_keyword_len);

exit:
  s = ndp10x_hal_close(handle);
  return s;
}

void show_usage() {
  printf(
      "\nusage:\n\n"
      "ndp10x_driver_test [option] \n\n"
      "Options:\n\n"
      "Recognize:\n"
      "\t-w -f [firmware] -m [model]\n\n"
      "Recognize & record (~1.5 secs of pre-roll & match data) + n sec of audio post-match:\n"
      "\t-w -r -t [rec_sec] -f [firmware] -m [model]\n"
      "play:\n"
      "\t-p [play file name] -f [firmware] -m [model]\n\n"
      "Record\n"
      "\t-r -t [rec_sec] -n [number of recordings] -f [firmware] -m [model]\n");
}

int main(int argc, char** args) {
  setbuf(stdout, NULL);
  setbuf(stderr, NULL);
  char* packages = "/vendor/firmware/hellojio_217.1_jio_phone_f372i_ndp10x-b0-kw_v42.synpkg";
  int c = 0;

  // if (argc < 2)
  // {
  //     printf("invalid arguments");
  //     show_usage();
  //     return -1;
  // }

  while ((c = getopt(argc, args, "p:")) != -1) {
    switch (c) {
      case 'p':
        packages = optarg;
        break;

      default:
        show_usage();
        goto INVALID;
    }
  }

  model_recognize(packages, 1, 1500, 3000);

  return 0;

INVALID:
  printf("\nPlease try again.\n\n");
  return 0;
}
