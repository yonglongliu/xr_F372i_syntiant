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
#ifndef _SYNGUP_H_
#define _SYNGUP_H_

#include <openssl/md5.h>

#define UUID_LENGTH (16)

#define TAG_SYNPKG_FILE (44)
#define TAG_TFLITE_MODEL_FILE (45)
#define TAG_POSTERIOR_FILE (46)
#define TAG_TFLITE_FB_FILE (47)
#define TAG_COMPOSITE_SOUND_CONTENT (48)

#define SYN_SOUND_TRIGGER_MAX_LOCALE_LEN (6)
#define SYN_SOUND_TRIGGER_MAX_USERS (10)
#define SYN_SOUND_TRIGGER_MAX_PHRASES (10)
#define SYN_SOUND_TRIGGER_MAX_STRING_LEN (64)

/* Metadata structure for synpkg blob
 */

struct phrase {
  uint8_t id;
  uint8_t recog_mode;
  uint8_t num_users;
  char locale[SYN_SOUND_TRIGGER_MAX_LOCALE_LEN];
  /* includes null terminator */
  char text[SYN_SOUND_TRIGGER_MAX_STRING_LEN];
  /* includes null terminator */
  uint32_t users[SYN_SOUND_TRIGGER_MAX_USERS];
};

struct synpkg_metadata {
  uint8_t type;
  uint8_t num_phrases;
  uint8_t version[126];
  /* includes null terminator */
  uint8_t param_version[32];
  /* includes null terminator */
  uint8_t firmware_version[32];
  /* includes null terminator */
  uint8_t vendor_uuid[UUID_LENGTH];
  uint8_t model_uuid[UUID_LENGTH];
  uint8_t phrases[0];
  /* maps variable number of phrases to struct phrase */
};

/* A link representing a blob
 * @param data: binary data blob
 * @param tag: tag identifier for a blob
 * @param digest: MD5 digest of the blob
 */
struct syn_pkg {
  uint32_t tag;
  uint32_t size;
  uint8_t digest[MD5_DIGEST_LENGTH];
  uint8_t* data;
  uint8_t persist;
  uint8_t loaded;
  uint32_t metadata_size;
  struct synpkg_metadata* mdata;
  struct syn_pkg* next;
};

/* Syngup package
 * @cookie: identifier of a syngup package
 * @crc: crc32 computed of the whole package
 * @num_pkgs: number of blobs in the syngup package
 * @inflated_len: uncompressed length of the conglomerate
 * @head: head of the list of packages
 */
struct syngup_package {
  uint32_t cookie;
  uint32_t crc;
  uint32_t num_pkgs;
  uint32_t inflated_len;
  struct syn_pkg* head;
};

struct ndp10x_ph_class_params_s;
struct ndp10x_ph_state_params_s;

#ifdef __cplusplus
extern "C" {
#endif

/* Get a synpkg blob from syngup
 * @syngup: pointer to syngup package handle
 * @pkg_ptr: pointer to the blob data (in terms of syn_pkg)
 */
int syngup_get_synpkg(struct syngup_package* syngup, struct syn_pkg** pkg_ptr);

/* Get a third party/dynamically generated blob from syngup
 * @syngup: pointer to syngup package handle
 * @pkg_ptr: pointer to the blob data (in terms of syn_pkg)
 */
int syngup_get_sound_package(struct syngup_package* syngup, struct syn_pkg** pkg_ptr);

/* Add a new blob to the syngup package
 * pkg_head: Head of the blob list
 * model_data: blob
 * size: size of the blob
 * metadata: 64 bytes of blob metadata
 */
int syngup_add_component(struct syngup_package* pkg, void* model_data, uint32_t size,
                         uint8_t* metadata, uint32_t mdata_size);

/* Delete a blob from syngup
 * @pkg: Pacakge handle
 * @data: data buffer pointing to the beginning of the syngup data
 * @size: size of syngup data (file size)
 * @model_uuid: model identifier
 */
int syngup_del_blob(struct syngup_package* pkg, uint8_t* data, uint32_t size, uint8_t* model_uuid);

/* Delete a component (blob)
 * head: head of the pkg list
 * model_uuid: UUID of the blob
 */
int syngup_del_component(struct syngup_package* pkg, uint8_t* model_uuid);

/* Pack and compress all the components in a syngup package
 * Returns the size of the syngup package, 0 if compression fails.
 */
uint32_t syngup_pack_blobs(struct syngup_package* pkg, uint8_t* buffer);

/* Compute the total length of all the blobs
 */
uint32_t syngup_get_total_size(struct syngup_package* pkg, uint32_t blob_size,
                               uint32_t metadata_size);

/*  Extract the blobs from the given memory buffer
 *  @pkg: syngup package ptr
 *  @bytes, memory buffer mapping the gup file
 *  @data_size, size of memory buffer
 */
int syngup_extract_blobs(struct syngup_package* pkg, void* begin_ptr, uint32_t size);

/* Get number of posterior states and classes
 */
int syngup_get_posterior_data(struct syngup_package * pkgs, struct syn_pkg** pkg, uint32_t* num_states,
                              uint32_t* num_classes);

/* Get a blob from syngup given a tag
 */
int syngup_get_component(struct syn_pkg* pkg_list, struct syn_pkg** pkg, uint32_t tag);

/* Get posterior data of states and classes
 */
void syngup_posterior_setting(struct syn_pkg* pkg, struct ndp10x_ph_state_params_s* states,
                              struct ndp10x_ph_class_params_s* classes, uint32_t num_states,
                              uint32_t num_classes);

/* Clean up syngup resources
 */
void syngup_cleanup(struct syngup_package* pkg);

#ifdef __cplusplus
}
#endif

#endif /* _SYNGUP_H */
