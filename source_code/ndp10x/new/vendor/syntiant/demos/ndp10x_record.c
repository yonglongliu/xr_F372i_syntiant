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

#include <assert.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <unistd.h>
// #include "../evb/ndp9101_pi/include/ndp9101_pi/ndp9101_pi_i2c_clock.h"
#include "ndp10x_app_utils.h"
#include "ndp10x_ioctl.h"
#include "syntiant_ilib/ndp10x_spi_regs.h"
#include "syntiant_ilib/syntiant_ndp.h"
#include "syntiant_ilib/syntiant_ndp_error.h"

#define NDP_WATCH_COUNT (10)

#define AUDIO_BUFF_LENGTH (16000 * 2)

#define EXTRACT_WAV
#ifdef EXTRACT_WAV
/* assuming little endian, and structure with no padding */
struct wav_header_s {
    char riff[4];
    uint32_t file_size;
    char wave[4];
    char fmt[4];
    uint32_t fmt_size;
    uint16_t type;
    uint16_t channels;
    uint32_t sample_rate;
    uint32_t bytes_per_second;
    uint16_t bytes_per_frame;
    uint16_t bits_per_sample;
    char data[4];
    uint32_t data_size;
};


void
write_wav(char *filename, uint8_t* data_ptr, int sample_bytes, int total_len)
{
    FILE *wav_fp;
    struct wav_header_s wav_hdr;
    int wrt_size;
    wav_hdr.riff[0] = 'R';
    wav_hdr.riff[1] = 'I';
    wav_hdr.riff[2] = 'F';
    wav_hdr.riff[3] = 'F';
    wav_hdr.file_size = 36 + total_len;
    wav_hdr.wave[0] = 'W';
    wav_hdr.wave[1] = 'A';
    wav_hdr.wave[2] = 'V';
    wav_hdr.wave[3] = 'E';
    wav_hdr.fmt[0] = 'f';
    wav_hdr.fmt[1] = 'm';
    wav_hdr.fmt[2] = 't';
    wav_hdr.fmt[3] = ' ';
    wav_hdr.fmt_size = 16;
    wav_hdr.type = 1;
    wav_hdr.channels = 1;
    wav_hdr.sample_rate = 16000;
    wav_hdr.bytes_per_second = 16000 * 1 * sample_bytes;
    wav_hdr.bytes_per_frame = 1 * sample_bytes;
    wav_hdr.bits_per_sample = sample_bytes * 8;
    wav_hdr.data[0] = 'd';
    wav_hdr.data[1] = 'a';
    wav_hdr.data[2] = 't';
    wav_hdr.data[3] = 'a';
    wav_hdr.data_size = total_len;
    wav_fp = fopen(filename, "wb");
    if (!wav_fp) {
        perror("error opening wav file");
        return;
    }
    /* write wav header */
    fwrite(&wav_hdr, 1, sizeof(wav_hdr), wav_fp);

    /* write data */
    wrt_size = fwrite(data_ptr, 1, total_len, wav_fp);
    if (ferror(wav_fp)) {
        perror("PCM data write failed");
    }
    printf("%d bytes data written to %s\n", wrt_size, filename);

    fclose(wav_fp);
}
#endif
int ndp10x_record(int rec_time, int terms, char* fw_name, char* package_name) {
  int ndpfd;
  char** labels;
  int classes;
  struct ndp10x_pcm_extract_s extract;
  uint8_t* audio_buffer;
  size_t wrt_size;
  char buf[32];
  FILE* fptr;
  int cnt;
#ifdef EXTRACT_WAV
    int sample_bytes;
    struct syntiant_ndp10x_config_s ndp10x_config;
#endif

  ndpfd = open("/dev/ndp10x", 0);
  if (ndpfd < 0) {
    perror("ndp10x open failed");
    exit(1);
  }

  stats(ndpfd, 0, 1, NULL);

  if (fw_name && package_name) {
    initialization(ndpfd, 0);
    printf("device initialized successfully!\n");

    load_synpkgs(ndpfd, fw_name, package_name);
    printf("synpkg loaded successfully!\n");
  }

  get_config(ndpfd, &labels, &classes);
#ifdef EXTRACT_WAV
   // get_ndp_config(ndpfd, &ndp10x_config);
    sample_bytes = 2;//ndp10x_config.tank_bits / 8;
    if(!sample_bytes){
        perror("wrong sample bytes value");
        exit(1);
    }
#endif

  audio_buffer = malloc(AUDIO_BUFF_LENGTH * rec_time);
  if (!audio_buffer) {
    fprintf(stderr, "couldn't allocate buffer for audio data\n");
    exit(1);
  }

  memset(audio_buffer, 0, sizeof(uint8_t) * rec_time * AUDIO_BUFF_LENGTH);

  stats(ndpfd, 0, 1, NULL);

  if (fw_name && package_name) {
    memset(&extract, 0, sizeof(extract));
    extract.flush = 1;
    if (ioctl(ndpfd, PCM_EXTRACT, &extract) < 0) {
      perror("ndp10x extract flush ioctl failed");
      exit(1);
    }
  }

  extract.flush = 0;
  extract.buffer = audio_buffer;
  extract.buffer_length = rec_time * AUDIO_BUFF_LENGTH;
  for (cnt = 0; cnt < terms; cnt++) {
    printf("extract audio\n");
    if (ioctl(ndpfd, PCM_EXTRACT, &extract) < 0) {
      perror("ndp10x extract ioctl failed");
      exit(1);
    }
    printf("save data to a file\n");
    memset(buf, 0, sizeof(unsigned char) * 32);
        snprintf(buf, 32, "%s%d%s", "/intsdc/pcm_audio", (unsigned int) cnt, ".raw");
    fptr = fopen(buf, "wb");
    if (!fptr) {
      perror("Couldn't open file for saving audio data");
      exit(1);
    }
    wrt_size = fwrite(audio_buffer, 1, rec_time * AUDIO_BUFF_LENGTH, fptr);
    if (ferror(fptr)) {
      perror("PCM data write failed");
    }
    printf("%d bytes written to %s\n", wrt_size, buf);
    fclose(fptr);

#ifdef EXTRACT_WAV
        printf("save data to a wav file\n");
        memset(buf, 0, sizeof(unsigned char) * 32);
        snprintf(buf, 32, "%s%d%s", "/intsdc/pcm_audio", (unsigned int) cnt, ".wav");
        write_wav(buf, audio_buffer, sample_bytes, rec_time * AUDIO_BUFF_LENGTH);
#endif
  }

  stats(ndpfd, 1, 0, NULL);

  if (audio_buffer) {
    free(audio_buffer);
  }

  if (close(ndpfd) < 0) {
    perror("ndp10x close failed");
    exit(1);
  }

  return 0;
}
