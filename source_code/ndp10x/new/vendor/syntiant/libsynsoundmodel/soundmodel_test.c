/*
 * SYNTIANT CONFIDENTIAL
 * _____________________
 *
 *   Copyright (c) 2020 Syntiant Corporation
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
#include <inttypes.h>
#include <unistd.h>
#include <arpa/inet.h>

#include <hardware/sound_trigger.h>
#include <system/sound_trigger.h>

#include "syntiant_soundmodel.h"

void print_sound_model(struct sound_trigger_phrase_sound_model* model, char* extra_text) {
  unsigned int i, j;
  printf("Parsed %smodel:\n", extra_text);
  printf("  Vendor UUID: ");
  printf("%08x-%04x-%04x-%04x-%02x%02x%02x%02x%02x%02x\n", ntohl(model->common.vendor_uuid.timeLow),
         ntohs(model->common.vendor_uuid.timeMid), ntohs(model->common.vendor_uuid.timeHiAndVersion),
         ntohs(model->common.vendor_uuid.clockSeq), model->common.vendor_uuid.node[0], model->common.vendor_uuid.node[1],
         model->common.vendor_uuid.node[2], model->common.vendor_uuid.node[3], model->common.vendor_uuid.node[4],
         model->common.vendor_uuid.node[5]);
  printf("  Model UUID:  ");
  printf("%08x-%04x-%04x-%04x-%02x%02x%02x%02x%02x%02x\n", ntohl(model->common.uuid.timeLow),
         ntohs(model->common.uuid.timeMid), ntohs(model->common.uuid.timeHiAndVersion),
         ntohs(model->common.uuid.clockSeq), model->common.uuid.node[0], model->common.uuid.node[1],
         model->common.uuid.node[2], model->common.uuid.node[3], model->common.uuid.node[4],
         model->common.uuid.node[5]);
  printf("  Num Phrases: %d\n", model->num_phrases);
  for (i = 0; i < model->num_phrases; i++) {
    printf("    Text : %s\n", model->phrases[i].text);
    printf("    Locale : %s\n", model->phrases[i].locale);
    printf("    Recognition mode : %d\n", model->phrases[i].recognition_mode);
    printf("    Num Users : %d {", model->phrases[i].num_users);
    if (model->phrases[i].num_users) {
      for (j = 0; j < model->phrases[i].num_users; j++) {
        printf(" %d ", model->phrases[i].users[j]);
      }
    }
    printf("}\n");
  }
}

int main(int argc, char** argv)
{
  char* package_name = NULL;
  struct sound_trigger_phrase_sound_model *model;
  struct stat file_stat;
  void* pkg;
  int pfd;
  int r;
  int c;
  unsigned int i;
  unsigned int alloc_size;
  int extend = 0;
  struct syntiant_user_sound_model_s* dest_model = NULL;
  int user_id = 0;

  if (argc < 2) {
    printf("Need package argument as %s -p <package>\n", argv[0]);
    return -1;
  }

  while ((c = getopt(argc, argv, "e:p:")) != -1) {
    switch (c) {
      case 'e':
        extend = 1;
        user_id = atoi(optarg);
        break;

      case 'p':
        package_name = optarg;
        break;

      default:
        printf("Need package argument as %s -p <package>\n", argv[0]);
        return 1;
    }
  }

  if (stat(package_name, &file_stat) < 0) {
    perror("unable to stat file");
    exit(1);
  }

  printf("File Size: %lld bytes\n", file_stat.st_size);

  pkg = malloc(file_stat.st_size);
  pfd = open(package_name, O_RDONLY);

  if (pfd < 0) {
    perror("Unable to open file");
    free(pkg);
    return -1;
  }

  r = read(pfd, pkg, file_stat.st_size);
  if (r < 0) {
    perror("Unable to read file");
    close(pfd);
    free(pkg);
    return -1;
  }

  if (r < file_stat.st_size) {
    perror("Truncated file read");
    close(pfd);
    free(pkg);
    return -1;
  }

  alloc_size = syntiant_get_keyphrase_sound_model_size(pkg, file_stat.st_size);
  model = malloc(alloc_size);
  printf("Allocated %u bytes for sound model\n", alloc_size);

  r = syntiant_parse_sound_model(model, pkg, file_stat.st_size);
  if (r) {
    printf("Parse sound model error\n");
    close(pfd);
    free(pkg);
    return -1;
  }

  print_sound_model(model, "");

  if (extend) {
    int16_t *samples[SYNTIANT_SPEAKER_ID_NUM_ENROLLMENT_SAMPLES];
    int res;
    struct sound_trigger_phrase_sound_model *ext_model;
    /* for the purpose of the test, just use zero-samples */
    for (i = 0; i < 5; i++) {
      samples[i] = calloc(SYNTIANT_SPEAKER_ID_WW_LEN * sizeof(int16_t), sizeof(uint8_t));
    }
    res = ExtendSoundModel_Samples(model, samples, &dest_model, user_id);
    printf("\nExtendSoundModel_Samples result: %d\n", res);
    if (res == SYNTIANT_ERROR_NONE) {
      printf("ExtendSoundModel_Samples dest_model bin size: %d\n", dest_model->model_size);

      alloc_size = syntiant_get_keyphrase_sound_model_size(dest_model->model, dest_model->model_size);
      ext_model = malloc(alloc_size);
      printf("Allocated %u bytes for Ext sound model\n", alloc_size);
      res = syntiant_parse_sound_model(ext_model, dest_model->model, dest_model->model_size);

      print_sound_model(ext_model, "Extended ");

      free(ext_model);
    }
  }

  free(model);
  free(dest_model);
  return r;
}
