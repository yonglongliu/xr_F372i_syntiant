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

#define LOG_TAG "Syngup"
#define LOG_NDEBUG 0

#include <assert.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syntiant-firmware/ndp10x_ph.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <zlib.h>

#include <cutils/log.h>

#include "syngup.h"

#define SYNGUP_COOKIE (0x53594E54u)

#define BLOB_LOADED (1)
#define BLOB_PERSIST (1)
#define UPDATE_SIZE (1024)

#ifdef __ANDROID__
//#define ALOGV(stuff, ...)
#else
#define ALOGV printf
#endif

#include "syntiant_defs.h"

/*
 * @tag identifier for a blob/package
 * @length length of the blob/package including 16 bytes of md5 digest
 * @data place holder for blob/package data
 */
typedef struct header {
  uint32_t tag;
  uint32_t length;
  uint8_t data[0];
} header_t;

struct class_params_s {
  uint32_t window;
  uint32_t threshold;
  uint32_t backoff;
  uint32_t action;
  uint32_t smoothing_queue_size;
  uint32_t action_arg;
};

/**
 * @brief parameters for each state
 *
 */
struct state_params_s {
  uint32_t timeout;
};

static int valid_tag(uint32_t tag) {
  if (tag == TAG_SYNPKG_FILE || tag == TAG_TFLITE_MODEL_FILE || tag == TAG_TFLITE_FB_FILE ||
      tag == TAG_POSTERIOR_FILE || tag == TAG_COMPOSITE_SOUND_CONTENT) {
    return 1;
  } else {
    return 0;
  }
}

static uLong compute_crc(Byte* zptr, uint32_t inflated_len) {
  uint32_t chunk_size = 1024;

  /* compute crc */
  uLong crc = crc32(0L, Z_NULL, 0);
  while (inflated_len) {
    if (inflated_len < chunk_size) {
      chunk_size = inflated_len;
    }
    crc = crc32(crc, zptr, chunk_size);
    inflated_len -= chunk_size;
    zptr += chunk_size;
  }
  return crc;
}

static int syngup_decompress(void* bytes, uint32_t length, struct syngup_package* hdr,
                             uint8_t** d_ptr) {
  int ret = 0;
  uLong crc;
  uint32_t compr_len, hdr_size;

  Byte* zdata = NULL;
  hdr_size = offsetof(struct syngup_package, head);

  memcpy(hdr, bytes, hdr_size);
  if (hdr->cookie != SYNGUP_COOKIE) {
    ALOGV("%s: Invalid syncookie: 0x%x\n", __func__, hdr->cookie);
    return -1;
  }
  compr_len = length - hdr_size;

  zdata = (Byte*)malloc(hdr->inflated_len * sizeof(Byte));
  if (!zdata) {
    goto err;
  }
  bytes = (uint8_t*)bytes + hdr_size;
  ret =
      uncompress((Bytef*)zdata, (uLongf*)&hdr->inflated_len, (const Bytef*)bytes, (uLong)compr_len);
  if (ret) {
    ALOGV("%s: err from uncompress: %d\n", __func__, ret);
    goto err;
  }
  /* validate crc32 */
  crc = compute_crc(zdata, hdr->inflated_len);
  if (hdr->crc != crc) {
    ALOGV("%s: CRC validation failed: crc:0x%x, computed:0x%x ret:%d\n", __func__, hdr->crc, (uint32_t)crc, ret);
    goto err;
  }
  *d_ptr = (uint8_t*)zdata;

  return ret;
err:
  if (zdata) {
    free(zdata);
    ret = -1;
  }
  return ret;
}

/* Compress data and copy it to over to the caller provided buffer
 * @param bytes: caller allocated buffer
 */
static int syngup_compress(uint8_t* bytes, struct syngup_package* hdr, uint32_t* deflated_size) {
  int ret;
  Byte* zdata;
  uLong compr_len = 0;
  uint32_t inflated_len = hdr->inflated_len;

  compr_len = compressBound(inflated_len);
  zdata = (Bytef*)calloc(compr_len * sizeof(Byte), 1);
  if (!zdata) {
    return -1;
  }
  hdr->crc = compute_crc((Byte*)bytes, inflated_len);
  /* compress the blobs */
  ret = compress((Bytef*)zdata, (uLongf*)&compr_len, (const Bytef*)bytes, inflated_len);
  if (ret) {
    ALOGV("%s: error from compression: %d\n", __func__, ret);
  } else {
    *deflated_size = compr_len;
    memcpy(bytes, zdata, *deflated_size);
  }
  free(zdata);

  return ret;
}

/* Compute MD5 digest of a blob
 * @param pkg: Input syngup package
 * @params digest: Computed MD5 digest
 */
static void _compute_digest(uint8_t* data, uint32_t size, unsigned char* digest) {
  uint32_t update_size;
  MD5_CTX mdContext;

  /* compute md5 digest of the blob/package */
  MD5_Init(&mdContext);
  while (size) {
    update_size = size > UPDATE_SIZE ? UPDATE_SIZE : size;
    MD5_Update(&mdContext, data, update_size);
    data += update_size;
    size -= update_size;
  }
  MD5_Final(digest, &mdContext);
}

static void syngup_compute_digest(header_t* pkg, unsigned char* digest) {
  _compute_digest(pkg->data, pkg->length, digest);
}

static int syngup_parse_metadata(struct syn_pkg* pkg, uint32_t size, uint8_t* bytes) {
  pkg->mdata = malloc(size);
  if (!pkg->mdata) {
    return -1;
  }
  memcpy(pkg->mdata, bytes, size);
  pkg->metadata_size = size;
  return 0;
}

static int syngup_extract(void* bytes, uint32_t uncompr_len, struct syn_pkg* pkg_ptr) {
  int i, ret = 0;
  void* ref_ptr;
  header_t* header;
  unsigned char digest[MD5_DIGEST_LENGTH], cdigest[MD5_DIGEST_LENGTH];
  uint32_t tag, tot_length, blob_length;
  struct syn_pkg* head = pkg_ptr;
  uint32_t metadata_len = 0;

  ref_ptr = bytes;
  while (uncompr_len) {
    header = (header_t*)bytes;
    tag = header->tag;
    blob_length = header->length;
    assert(blob_length > MD5_DIGEST_LENGTH);

    if (!valid_tag(tag)) {
      goto skip_package;
    }

    syngup_compute_digest(header, cdigest);
    memcpy(digest, header->data + blob_length, MD5_DIGEST_LENGTH);

    /* validate md5 digest */
    if (memcmp(cdigest, digest, MD5_DIGEST_LENGTH)) {
      ALOGV("%s: Invalid md5 digest\n", __func__);
      ret = -1;
      ALOGV("\n cdigest\n");
      for (i = 0; i < 16; i++) {
        ALOGV("%x ", cdigest[i]);
      }
      ALOGV("\n digest\n");
      for (i = 0; i < 16; i++) {
        ALOGV("%x ", digest[i]);
      }
      break;
    } else {
      /* setup this blob/package metadata */
      pkg_ptr->data = malloc(sizeof(uint8_t) * blob_length);
      if (!pkg_ptr->data) {
        goto cleanup;
      }
      memcpy(pkg_ptr->data, header->data, blob_length);
      /* extract metadata */
      memcpy(&metadata_len, header->data + blob_length + MD5_DIGEST_LENGTH, sizeof(metadata_len));
      if (metadata_len) {
        ret = syngup_parse_metadata(
            pkg_ptr, metadata_len,
            header->data + blob_length + MD5_DIGEST_LENGTH + sizeof(metadata_len));
        if (ret) {
          goto cleanup;
        }
      }
      pkg_ptr->size = blob_length;
      pkg_ptr->tag = tag;
      pkg_ptr->loaded = !BLOB_LOADED;
      pkg_ptr->persist = BLOB_PERSIST;
      pkg_ptr = pkg_ptr->next;
    }
  skip_package:
    /* skip the bytes to the start of next blob/package */
    tot_length = offsetof(header_t, data) + blob_length + MD5_DIGEST_LENGTH + sizeof(metadata_len) +
                 metadata_len;
    bytes = (uint8_t*)bytes + tot_length;
    uncompr_len -= tot_length;
  }
  bytes = ref_ptr;
  return ret;

cleanup:
  while (head != pkg_ptr) {
    free(head->data);
    head->data = NULL;
    if (head->mdata) {
      free(head->mdata);
    }
    head = head->next;
  }
  ALOGV("%s: Couldn't allocate memory for blob:%d\n", __func__, tag);
  ;
  return -1;
}

/* Callers must call syngup_cleanup() after using this function
 * @tag: tag identifying the blob
 * @data: buffer containing entire syngup data
 * @size: size of the buffer
 * @pkg_ptr: pointer to the blob
 */
int syngup_get_synpkg(struct syngup_package* syngup, struct syn_pkg** pkg_ptr) {
  return syngup_get_component(syngup->head, pkg_ptr, TAG_SYNPKG_FILE);
}

int syngup_get_sound_package(struct syngup_package* syngup, struct syn_pkg** pkg_ptr) {
  return syngup_get_component(syngup->head, pkg_ptr, TAG_COMPOSITE_SOUND_CONTENT);
}

/* Compute the total length of all the blobs including their headers
 */
uint32_t syngup_get_total_size(struct syngup_package* pkg, uint32_t blob_size,
                               uint32_t metadata_size) {
  uint32_t total = 0;

  struct syn_pkg* head = pkg->head;

  total += sizeof(head->tag) + sizeof(head->size) + sizeof(head->metadata_size) + blob_size +
           metadata_size;

  while (head) {
    total += head->size + sizeof(head->tag) + sizeof(head->size) + MD5_DIGEST_LENGTH +
             sizeof(head->metadata_size) + head->metadata_size;
    head = head->next;
  }
  return total;
}

/* Cleanup syngup resources
 */
void syngup_cleanup(struct syngup_package* pkg) {
  struct syn_pkg *tmp, *head;

  head = pkg->head;
  while (head) {
    if (head->data) {
      free(head->data);
    }
    if (head->mdata) {
      free(head->mdata);
    }
    tmp = head->next;
    free(head);
    head = tmp;
    pkg->num_pkgs--;
  }
  pkg->head = NULL;
}

/* Pack and compress all the components in a syngup package
 * Returns the size of the syngup package, 0 if compression fails.
 * @pkg: syngup package pointer
 * @buffer: user supplied buffer for copying package data
*/

uint32_t syngup_pack_blobs(struct syngup_package* pkg, uint8_t* buffer) {
  uint32_t deflated_len, hdr_size;
  unsigned char digest[MD5_DIGEST_LENGTH];
  uint8_t* buffer_begin = buffer;
  struct syn_pkg* head = pkg->head;

  hdr_size = offsetof(struct syngup_package, head);
  buffer += hdr_size;
  pkg->inflated_len = 0;

  if (!pkg->num_pkgs) {
    ALOGV("Nothing to pack! Number of blobs: %d\n", pkg->num_pkgs);
    return 0;
  }
  while (head) {
    ALOGV("pack: %d\n", head->tag);
    /* compute md5 digest of the blob/package */
    _compute_digest(head->data, head->size, digest);
    /* pack the blob with its header */
    memcpy(buffer, &head->tag, sizeof(head->tag));
    buffer += sizeof(head->tag);
    memcpy(buffer, &head->size, sizeof(head->size));
    buffer += sizeof(head->size);
    memcpy(buffer, head->data, head->size);
    buffer += head->size;
    memcpy(buffer, digest, MD5_DIGEST_LENGTH);
    buffer += MD5_DIGEST_LENGTH;
    memcpy(buffer, &head->metadata_size, sizeof(head->metadata_size));
    buffer += sizeof(head->metadata_size);
    pkg->inflated_len += sizeof(head->tag) + sizeof(head->size) + head->size + MD5_DIGEST_LENGTH +
                         sizeof(head->metadata_size);
    if (head->metadata_size) {
      memcpy(buffer, head->mdata, head->metadata_size);
      buffer += head->metadata_size;
      pkg->inflated_len += head->metadata_size;
    }
    head = head->next;
  }
  buffer = buffer_begin + hdr_size;
  /* compress the conglomerate */
  if (syngup_compress(buffer, pkg, &deflated_len)) {
    ALOGV("%s: Couldn't compress the blob\n", __func__);
    return 0;
  }
  /* add syngup header */
  memcpy(buffer_begin, pkg, hdr_size);
  return deflated_len + hdr_size;
}

/* Decompress and extract blobs from a syngup package.
 * @param pkg_head: head of the list containing different blobs
 * @param begin_ptr: starting point in the memory containing syngup object
 * @param size: size of the syngup object in memory
*/
int syngup_extract_blobs(struct syngup_package* pkg_head, void* begin_ptr, uint32_t size) {
  int num_pkgs;
  int error_code = 0;
  uint8_t* bytes = NULL;
  struct syn_pkg **head = NULL, *pkgs = NULL;

  ALOGV("%s: libsyngup version:%s\n", __func__, syntiant_internal_version);

  error_code = syngup_decompress(begin_ptr, size, pkg_head, &bytes);
  if (error_code) {
    ALOGV("%s: ndp10x decompress failed\n", __func__);
    goto error;
  }
  head = &pkg_head->head;
  num_pkgs = pkg_head->num_pkgs;
  while (num_pkgs) {
    pkgs = (struct syn_pkg*)malloc(sizeof(*pkgs));
    if (!pkgs) {
      ALOGV("%s: ndp10x malloc for pkg_ptrs failed\n", __func__);
      goto error;
    }
    memset(pkgs, 0, sizeof(*pkgs));
    *head = pkgs;
    head = &((*head)->next);
    num_pkgs--;
  }

  error_code = syngup_extract((void*)bytes, pkg_head->inflated_len, pkg_head->head);
  if (error_code) {
    ALOGV("%s: synpkg package decompression and parsing failed: %d\n", __func__, error_code);
    goto error;
  }

error:
  if (bytes) {
    /* free memory allocated during decompression */
    free(bytes);
  }
  return error_code;
}

/* Add a new blob to the syngup package
 * pkg_head: Head of the blob list
 * model_data: blob
 * size: size of the blob
 * metadata: blob metadata
 * meta_size: size of metadata
 */
int syngup_add_component(struct syngup_package* pkg, void* model_data, uint32_t size,
                         uint8_t* metadata, uint32_t meta_size) {
  int error_code = 0;
  struct syn_pkg* head = pkg->head;
  struct syn_pkg** blob;

  if (!size) {
    ALOGV("Can't add 0 sized data to syngup\n");
    return error_code;
  }
  if (!head->next) {
    ALOGV("empty list! %d\n", head->tag);
  }
  while (head->next) {
    ALOGV("tag: %d\n", head->next->tag);
    head = head->next;
  }
  blob = &head->next;
  *blob = malloc(sizeof(*head));
  if (!*blob) {
    ALOGV("Couldn't allocate memory for this component\n");
    error_code = -1;
    goto error;
  }

  memset(*blob, 0, sizeof(struct syn_pkg));
  (*blob)->data = malloc(sizeof(uint8_t) * size);
  if (!(*blob)->data) {
    ALOGV("Couldn't allocate data memory for this component\n");
    error_code = -1;
    free(*blob);
    *blob = NULL;
    goto error;
  }
  memcpy((*blob)->data, model_data, size);
  (*blob)->size = size;

  if (metadata && meta_size) {
    (*blob)->mdata = malloc(sizeof(uint8_t) * meta_size);
    if (!(*blob)->mdata) {
      error_code = -1;
      goto error;
    }
    memcpy((*blob)->mdata, metadata, meta_size);
    (*blob)->metadata_size = meta_size;
  }
  (*blob)->tag = TAG_COMPOSITE_SOUND_CONTENT;
  (*blob)->persist = BLOB_PERSIST;
  (*blob)->loaded = BLOB_LOADED;
  pkg->num_pkgs++;

  head = pkg->head;
  while (head) {
    ALOGV("after addition, blob:%d\n", head->tag);
    head = head->next;
  }
  return error_code;

error:
  if (*blob) {
    if ((*blob)->data) free((*blob)->data);
    if ((*blob)->mdata) free((*blob)->mdata);
    free(*blob);
    *blob = NULL;
  }
  return error_code;
}

int syngup_del_component(struct syngup_package* pkg, uint8_t* model_uuid) {
  uint8_t ret = 1;
  uint8_t* mdata;
  struct syn_pkg* tmp = NULL;
  struct syn_pkg** cursor;

  cursor = &pkg->head;

  while (*cursor) {
    if ((*cursor)->mdata != NULL) {
      mdata = (*cursor)->mdata->model_uuid;
      if (!memcmp(mdata, model_uuid, UUID_LENGTH)) {
        break;
      }
    }
    cursor = &(*cursor)->next;
  }
  if (*cursor) {
    tmp = *cursor;
    *cursor = (*cursor)->next;
    if (tmp->data) {
      free(tmp->data);
    }
    if (tmp->mdata) {
      free(tmp->mdata);
    }
    free(tmp);
    tmp = NULL;
    ret = 0;
    pkg->num_pkgs--;
  }
  return ret;
}

/* Delete a blob
 * head: head of the pkg list
 * model_uuid: UUID of the blob
 */
int syngup_del_blob(struct syngup_package* pkg, uint8_t* data, uint32_t size, uint8_t* model_uuid) {
  uint8_t ret = 1;

  ret = syngup_extract_blobs(pkg, (void*)data, size);
  if (ret) {
    ALOGV("Error(%d) while extract blobs\n", ret);
    return ret;
  }
  return syngup_del_component(pkg, model_uuid);
}

/* Find the blob with the id == tag
 */
int syngup_get_component(struct syn_pkg* pkg_list, struct syn_pkg** pkg, uint32_t tag) {
  while (pkg_list) {
    if (pkg_list->tag == tag) {
      *pkg = pkg_list;
      break;
    }
    pkg_list = pkg_list->next;
  }
  if (!*pkg) {
    ALOGV("%s: pkg not found\n", __func__);
    return -1;
  }
  return 0;
}

/* Get posterior settings from the posterior blob
 */
int syngup_get_posterior_data(struct syngup_package* pkgs, struct syn_pkg** pkg, uint32_t* num_states,
                              uint32_t* num_classes) {
  int error_code;
  uint8_t* bytes = NULL;

  error_code = syngup_get_component(pkgs->head, pkg, TAG_POSTERIOR_FILE);
  if (error_code) {
    ALOGV("%d not found\n", TAG_POSTERIOR_FILE);
    return error_code;
  }
  bytes = (*pkg)->data;
  memcpy(num_states, bytes, sizeof(*num_states));
  bytes = bytes + sizeof(*num_states);

  memcpy(num_classes, bytes, sizeof(*num_classes));

  return error_code;
}

/* Fill in the caller passed states and classes arrays. Before making this
 * call, caller must call syngup_get_posterior_data to know the num_classes
 * and num_states.
 */
void syngup_posterior_setting(struct syn_pkg* pkg, struct ndp10x_ph_state_params_s* states,
                              struct ndp10x_ph_class_params_s* classes, uint32_t num_states,
                              uint32_t num_classes) {
  uint32_t i;
  uint32_t s;
  uint8_t* bytes = pkg->data + sizeof(num_classes) + sizeof(num_states);

  /* parse the byte stream and create the data structures from it */
  for (s = 0; s < num_states; s++) {
    memcpy(&states[s], bytes, sizeof(struct state_params_s));
    bytes += sizeof(struct state_params_s);
    for (i = 0; i < num_classes; i++) {
      memcpy(&classes[s * num_classes + i], bytes, sizeof(*classes));
      bytes += sizeof(struct class_params_s);
    }
  }
}
