/*
 * SYNTIANT CONFIDENTIAL
 * _____________________
 *
 *   Copyright (c) 2018-2020 Syntiant Corporation
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

#include <syntiant_ilib/syntiant_portability.h>

#include <syntiant_ilib/ndp10x_regs.h>
#include <syntiant_ilib/ndp10x_spi_regs.h>
#include <syntiant_ilib/syntiant_ndp.h>
#include <syntiant_ilib/syntiant_ndp10x.h>
#include <syntiant_ilib/syntiant_ndp10x_driver.h>
#include <syntiant_ilib/syntiant_ndp10x_mailbox.h>
#include <syntiant_ilib/syntiant_ndp_driver.h>
#include <syntiant_ilib/syntiant_ndp_error.h>

#include <syntiant-firmware/ndp10x_firmware.h>
#include <syntiant-firmware/ndp10x_ph.h>
#include <syntiant-firmware/ndp10x_result.h>
#include <syntiant-firmware/ndp10x_agc.h>

enum syntiant_ndp10x_internal_constants_e {
    /* TODO merge these elsewhere */
    SYNTIANT_NDP10X_MAX_FLL_REF_CLOCK_RATE=4000000,
    SYNTIANT_NDP10X_TARGET_CLOCK_RATE=16000000,
    SYNTIANT_NDP10X_TARGET_OVERDRIVE_CLOCK_RATE=24000000,
    SYNTIANT_NDP10X_MAX_MCU_RATE=19200000,
    SYNTIANT_NDP10X_MAX_DNN_RATE=19200000,
    SYNTIANT_NDP10X_DEFAULT_TANK_SIZE = 2048,
    SYNTIANT_NDP10X_FW_POINTERS_ADDRESS = 0x1fffc0c0U,
    SYNTIANT_NDP10X_MAX_SPI_BLOCK_SIZE = 2048U
};

enum syntiant_ndp10x_posterior_constants_e {
    POSTERIOR_ENABLE_BITS=(NDP10X_FW_STATE_ENABLE_SMAX_SMOOTHER
                           | NDP10X_FW_STATE_ENABLE_POSTERIOR)
};

/*
 * state (mostly register values) values used by ndp10x_config functions
 */
struct syntiant_ndp10x_config_state_s {
    unsigned int dnn_input;
    unsigned int tank_input;
    unsigned int input_clock_rate;
    unsigned int core_clock_rate;
    unsigned int tank_max_size;
    uint32_t fw_state_addr;
    int eq_update;
    uint32_t fw_enable;
    uint32_t fw_max_adjustment_gain;
    uint32_t fw_nom_speech_quiet;
    uint32_t fw_match_per_frame;
    uint32_t dnnctl0;
    uint32_t dnnctl1;
    uint32_t dnnctl3;
    uint32_t dnnctl7;
    uint32_t smplctl;
    uint32_t smplmark;
    uint32_t smplsts;
    uint32_t tank;
    uint32_t i2sctl;
    uint32_t pdmctl;
    uint32_t pdmcfg[2];
    uint32_t clkctl0;
    uint32_t clkctl1;
    uint32_t clkctl2;
    uint32_t freqctl;
    uint32_t mem;
    uint32_t mem0;
    uint32_t mem1;
    uint32_t mem3;
    uint8_t ctl;
};

extern int
syntiant_ndp10x_parse_tag_values(struct syntiant_ndp_device_s *ndp);

int syntiant_ndp10x_poll(
    struct syntiant_ndp_device_s *ndp, uint32_t *notifications, int clear);

/**
 *@brief function to read information stored in config region
 *@param ndp device object
 *@param config config object
 *@param classes number of classes
 *@return error code
 */
int
syntiant_ndp10x_access_config(struct syntiant_ndp_device_s *ndp,
    struct syntiant_ndp_config_s *config, unsigned int *classes);

/**
 *@brief function to read config area and set pkgver_len, paramver_len, 
 *       fwver_len, pkgver_len, and labels in ndp10x.
 * @param ndp device object 
 * @return error code
*/
int
syntiant_ndp10x_restore_config(struct syntiant_ndp_device_s *ndp);

int
syntiant_ndp10x_posterior_init(
    struct syntiant_ndp_device_s *ndp, uint32_t states, uint32_t classes, uint32_t ph_type);

/**
 * @brief function to encode the posterior and its respective actions
 * @param param the paramter to save the encoded value in.
 * @param action the posterior action type (8 bit)
 * @param action_arg the action args encoded by the packager (24 bit)
*/
void
syntiant_ndp10x_posterior_config_encode_action_V2(
    uint32_t *param, uint32_t action, uint32_t action_arg);

/**
 * @brief function to decode the posterior and its respective actions
 * @param param the paramter used to extract the values from
 * @param action the posterior action type (8 bit)
 * @param action_arg the action args encoded by the packager (24 bit)
*/
void
syntiant_ndp10x_posterior_config_decode_action_V2(
    uint32_t param, uint32_t *action, uint32_t *action_arg);

int
syntiant_ndp10x_posterior_config_no_sync(struct syntiant_ndp_device_s *ndp,
    struct syntiant_ndp10x_posterior_config_s *config);

/**
 * @brief function to read length (and in some cases value) of specific tag.
 * This function checks if ndp->ndp10x includes pkgver_len, paramver_len, or
 * fwver_length, (3) read the length/value of the given tag.
 * @param ndp device object
 * @param tag_to_read tag to read its length (and value)
 * @param length_to_read length to be read
 * @param ver value to be read
 * @return error code
 */

 /* NOTE: Here is the layout of config area:
  * 4 bytes input clock rate
  * 4 bytes validity of labels, and version of fw, params, and pkg
  * 4 bytes length, and LABELS_MAX_SIZE bytes of labels
  * 4 bytes length, and VERSION_MAX_SIZE bytes fw version
  * 4 bytes length, and VERSION_MAX_SIZE bytes params version
  * 4 bytes length, and VERSION_MAX_SIZE bytes pkg version
 */

/**
 * @brief frunction to verify configuration matches with ndp10x
 * @param ndp device object
 * @return error code
 */
int
syntiant_ndp10x_verify_configuration(struct syntiant_ndp_device_s *ndp);

int
syntiant_ndp10x_config_read_state(struct syntiant_ndp_device_s *ndp,
                                  struct syntiant_ndp10x_config_state_s *st);
void
syntiant_ndp10x_config_get_input(struct syntiant_ndp10x_config_state_s *st,
                                 struct syntiant_ndp10x_config_s *config,
                                 struct syntiant_ndp10x_device_s *ndp10x);
/* Required to satisfy KCC */
void
syntiant_ndp10x_reset_mailbox_state(struct syntiant_ndp_device_s *ndp);

void
syntiant_ndp10x_tx_mb_data(
    struct syntiant_ndp10x_device_mb_state_s *mb, int bits, uint8_t *data);

void
syntiant_ndp10x_rx_mb_data(
    struct syntiant_ndp10x_device_mb_state_s *mb, int bits);

uint8_t
syntiant_ndp10x_tx_next_mb_data(
    struct syntiant_ndp10x_device_mb_state_s *mb, int *last);

void
syntiant_ndp10x_rx_next_mb_data(
    struct syntiant_ndp10x_device_mb_state_s *mb, uint8_t d, int *last);

void
syntiant_ndp10x_inc_uint8(uint8_t *p);

int
syntiant_ndp10x_mbwait(struct syntiant_ndp_device_s *ndp);

int
syntiant_ndp10x_op_size(struct syntiant_ndp_device_s *ndp, int mcu,
        unsigned int *size);

int
syntiant_ndp10x_read_block(struct syntiant_ndp_device_s *ndp, int mcu,
    uint32_t address, void *value, unsigned int count);

int
syntiant_ndp10x_write_block(struct syntiant_ndp_device_s *ndp, int mcu,
    uint32_t address, void *value, unsigned int count);

    int
syntiant_ndp10x_read(
    struct syntiant_ndp_device_s *ndp, int mcu, uint32_t address, void *value);

    int
syntiant_ndp10x_write(struct syntiant_ndp_device_s *ndp, int mcu,
    uint32_t address, uint32_t value);

    int
syntiant_ndp10x_load_fw_pointers(struct syntiant_ndp_device_s *ndp,
                                 uint32_t address);

int
syntiant_ndp10x_do_mailbox_req(struct syntiant_ndp_device_s *ndp, uint8_t req);

int
syntiant_ndp10x_do_mailbox_req0(struct syntiant_ndp_device_s *ndp, uint8_t req);

int
syntiant_ndp10x_wait_mb(struct syntiant_ndp_device_s *ndp, uint32_t address,
                        uint8_t owner, uint8_t *vp);

int
syntiant_ndp10x_reset_fix(struct syntiant_ndp_device_s *ndp);

int
syntiant_ndp10x_reset(struct syntiant_ndp_device_s *ndp);

int
syntiant_ndp10x_init(struct syntiant_ndp_device_s *ndp,
                     enum syntiant_ndp_init_mode_e init_mode);

int
syntiant_ndp10x_uninit(
    struct syntiant_ndp_device_s *ndp, enum syntiant_ndp_init_mode_e init_mode);

uint8_t
syntiant_ndp10x_do_mcu_to_host(
    struct syntiant_ndp10x_device_s *ndp10x, uint8_t mbin_resp,
    uint8_t mbin_next);

uint8_t
syntiant_ndp10x_do_host_to_mcu(struct syntiant_ndp10x_device_s *ndp10x,
    uint8_t mbin_resp, uint8_t mbin_next, int *wake);

    int
syntiant_ndp10x_do_mailboxes(struct syntiant_ndp_device_s *ndp, int *wake);

int
syntiant_ndp10x_interrupts(struct syntiant_ndp_device_s *ndp, int *causes);

int
syntiant_ndp10x_load(struct syntiant_ndp_device_s *ndp, void *chunk, int len);

char *
syntiant_ndp10x_get_config_devtype(unsigned int device_type);

int
syntiant_ndp10x_get_config(struct syntiant_ndp_device_s *ndp,
    struct syntiant_ndp_config_s *config);

int
syntiant_ndp10x_send_data(struct syntiant_ndp_device_s *ndp, uint8_t *data,
                          unsigned int len, int type, uint32_t offset);

int
syntiant_ndp10x_extract_data(struct syntiant_ndp_device_s *ndp, int type,
                             int from, uint8_t *data, unsigned int *lenp);


int
syntiant_ndp10x_config_set_tank(struct syntiant_ndp10x_config_s *config,
                                struct syntiant_ndp10x_config_state_s *st);

int
syntiant_ndp10x_config_check_tank(unsigned int dnn_input,
                                    unsigned int tank_input);

void
syntiant_ndp10x_config_get_tank(struct syntiant_ndp10x_config_state_s *st,
                                struct syntiant_ndp10x_config_s *config);

void
syntiant_ndp10x_config_get_dnn(struct syntiant_ndp10x_config_state_s *st,
                               struct syntiant_ndp10x_config_s *config);

int
syntiant_ndp10x_get_match_strength(
    struct syntiant_ndp_device_s *ndp, uint8_t *strengths, unsigned int len,
    int type);

int
syntiant_ndp10x_get_match_binary(
    struct syntiant_ndp_device_s *ndp, uint8_t *matches, unsigned int len);

int
syntiant_ndp10x_get_match_summary(struct syntiant_ndp_device_s *ndp,
                                  uint32_t *summary);

int
syntiant_ndp10x_config_set_i2s(struct syntiant_ndp10x_config_s *config,
                               struct syntiant_ndp10x_config_state_s *st);

void
syntiant_ndp10x_config_get_i2s(struct syntiant_ndp10x_config_state_s *st,
                               struct syntiant_ndp10x_config_s *config);

int
syntiant_ndp10x_config_set_pdm(struct syntiant_ndp10x_config_s *config,
                               struct syntiant_ndp10x_config_state_s *st);

void
syntiant_ndp10x_config_get_pdm(struct syntiant_ndp10x_config_state_s *st,
                               struct syntiant_ndp10x_config_s *config);

int
syntiant_ndp10x_config_set_freq(struct syntiant_ndp10x_config_s *config,
                                struct syntiant_ndp10x_config_state_s *st);

void
syntiant_ndp10x_config_get_freq(struct syntiant_ndp10x_config_state_s *st,
                                struct syntiant_ndp10x_config_s *config);

int
syntiant_ndp10x_reset(struct syntiant_ndp_device_s *ndp);

int
syntiant_ndp10x_init(
    struct syntiant_ndp_device_s *ndp,
    enum syntiant_ndp_init_mode_e init_mode);

int
syntiant_ndp10x_uninit(
    struct syntiant_ndp_device_s *ndp,
    enum syntiant_ndp_init_mode_e init_mode);

uint8_t
syntiant_ndp10x_do_mcu_to_host(
    struct syntiant_ndp10x_device_s *ndp10x, uint8_t mbin_resp,
    uint8_t mbin_next);

uint8_t
syntiant_ndp10x_do_host_to_mcu(struct syntiant_ndp10x_device_s *ndp10x,
    uint8_t mbin_resp, uint8_t mbin_next, int *wake);

int
syntiant_ndp10x_do_mailboxes(struct syntiant_ndp_device_s *ndp, int *wake);

int
syntiant_ndp10x_interrupts(struct syntiant_ndp_device_s *ndp, int *causes);

int
syntiant_ndp10x_load(struct syntiant_ndp_device_s *ndp, void *chunk, int len);

int
syntiant_ndp10x_config_get_filter_eq(struct syntiant_ndp_device_s *ndp,
                                     struct syntiant_ndp10x_config_s *config);
int
syntiant_ndp10x_config_set_filter_eq(struct syntiant_ndp_device_s *ndp,
                                     struct syntiant_ndp10x_config_s *config,
                                     struct syntiant_ndp10x_config_state_s *st);

int
syntiant_ndp10x_config_read_state(struct syntiant_ndp_device_s *ndp,
                                  struct syntiant_ndp10x_config_state_s *st);
int
syntiant_ndp10x_config_update_state(struct syntiant_ndp_device_s *ndp,
                                    struct syntiant_ndp10x_config_state_s *st0,
                                    struct syntiant_ndp10x_config_state_s *st);
int
syntiant_ndp10x_config_no_sync(struct syntiant_ndp_device_s *ndp,
                               struct syntiant_ndp10x_config_s *config);
int
syntiant_ndp10x_config(struct syntiant_ndp_device_s *ndp,
    struct syntiant_ndp10x_config_s *config);

int
syntiant_ndp10x_posterior_init(
    struct syntiant_ndp_device_s *ndp, uint32_t states, uint32_t classes, uint32_t ph_type);

int
syntiant_ndp10x_posterior_config_no_sync(struct syntiant_ndp_device_s *ndp,
    struct syntiant_ndp10x_posterior_config_s *config);

int
syntiant_ndp10x_gpio(
    struct syntiant_ndp_device_s *ndp, struct syntiant_ndp10x_gpio_s *gpio);

int
syntiant_ndp10x_status(struct syntiant_ndp_device_s *ndp,
                       struct syntiant_ndp10x_status_s *status);

int
syntiant_ndp10x_get_match_summary(
    struct syntiant_ndp_device_s *ndp, uint32_t *summary);

void
syntiant_ndp10x_config_get_dnn(struct syntiant_ndp10x_config_state_s *st,
                               struct syntiant_ndp10x_config_s *config);
int
syntiant_ndp10x_config_set_dnn(struct syntiant_ndp10x_config_s *config,
                               struct syntiant_ndp10x_config_state_s *st);
void
syntiant_ndp10x_config_get_tank(struct syntiant_ndp10x_config_state_s *st,
                                struct syntiant_ndp10x_config_s *config);
int
syntiant_ndp10x_config_set_spi(struct syntiant_ndp10x_config_s *config,
                               struct syntiant_ndp10x_config_state_s *st);

void
syntiant_ndp10x_config_get_spi(struct syntiant_ndp10x_config_state_s *st,
                               struct syntiant_ndp10x_config_s *config);

int
syntiant_ndp10x_config_set_input(struct syntiant_ndp10x_config_s *config,
                                 struct syntiant_ndp10x_config_state_s *st);

void
syntiant_ndp10x_config_get_clock(struct syntiant_ndp10x_config_state_s *st,
                                 struct syntiant_ndp10x_config_s *config);

int
syntiant_ndp10x_config_calc_mult_mult(unsigned int* core_clock_rate,
                                      unsigned int* input_clock_rate,
                                      unsigned int* clock_mult,
                                      unsigned int* clock_div);

int
syntiant_ndp10x_config_set_clock_slow_read(struct syntiant_ndp_device_s *ndp,
                                           uint32_t addr, uint32_t *vp);

int
syntiant_ndp10x_config_set_clock(struct syntiant_ndp_device_s *ndp,
                                 struct syntiant_ndp10x_config_s *config,
                                 struct syntiant_ndp10x_config_state_s *st);

void
syntiant_ndp10x_config_get_fw_state(struct syntiant_ndp10x_config_state_s *st,
                                    struct syntiant_ndp10x_config_s *config);


int
syntiant_ndp10x_config_set_fw_state(struct syntiant_ndp10x_config_s *config,
                                    struct syntiant_ndp10x_config_state_s *st);

void
syntiant_ndp10x_config_get_memory_power (
                            struct syntiant_ndp10x_config_state_s *st,
                            struct syntiant_ndp10x_config_s *config);

int
syntiant_ndp10x_config_set_memory_power(
                                    struct syntiant_ndp10x_config_s *config,
                                    struct syntiant_ndp10x_config_state_s *st);

int
syntiant_ndp10x_config_get_filter_bins(
                                    struct syntiant_ndp_device_s *ndp,
                                    struct syntiant_ndp10x_config_s *config);

enum syntiant_ndp10x_mb_constants_e {
    MB_NONE=0xff,
    NDP10X_H2M=SYNTIANT_NDP10X_DEVICE_MAILBOX_HOST_TO_MCU,
    NDP10X_M2H=SYNTIANT_NDP10X_DEVICE_MAILBOX_MCU_TO_HOST
};

static unsigned int
min_(unsigned int a, unsigned int b)
{
    if (a < b) {
        return a;
    } else {
        return b;
    }
}

int syntiant_ndp10x_mailbox_trace = 0;
uint32_t syntiant_ndp10x_mailbox_req_number[2] = { 0, 0 };
uint32_t syntiant_ndp10x_mailbox_rsp_number[2] = { 0, 0 };
uint32_t syntiant_ndp10x_mailbox_unexpected[2] = { 0, 0 };
uint32_t syntiant_ndp10x_mailbox_error[2] = { 0, 0 };

#ifdef SYNTIANT_NDP10X_MAILBOX_TRACE
char *mailbox_dir[2] = { "h2m", "m2h" };

void syntiant_ndp10x_mb_request_trace(int m2h, uint8_t r);
void
syntiant_ndp10x_mb_request_trace(int m2h, uint8_t r)
{
    static char *reqs[] = SYNTIANT_NDP10X_MB_REQUEST_DECODER;

    if (syntiant_ndp10x_mailbox_trace) {
        if (1 < syntiant_ndp10x_mailbox_trace
            || (r != SYNTIANT_NDP10X_MB_REQUEST_CONT
                   && r != SYNTIANT_NDP10X_MB_REQUEST_EXTOP
                   && (r & ~0x3) != SYNTIANT_NDP10X_MB_REQUEST_DATA)) {
            SYNTIANT_PRINTF("%s req  %d: ", mailbox_dir[m2h],
                                syntiant_ndp10x_mailbox_req_number[m2h]);
            if (r < sizeof(reqs) / sizeof(reqs[0])) {
                SYNTIANT_PRINTF("%s\n", reqs[r]);
            } else {
                SYNTIANT_PRINTF("ILLEGAL(%d)\n", r);
            }
        }
    }
    if (r < 8) {
        /*
         * only bump req count for 'primitive' (3-bit) requests
         * versus composite (extended/6-bit) requests
         */
        syntiant_ndp10x_mailbox_req_number[m2h]++;
    }
}

void syntiant_ndp10x_mb_response_trace(int m2h, uint8_t r);
void
syntiant_ndp10x_mb_response_trace(int m2h, uint8_t r)
{
    static char *rsps[] = SYNTIANT_NDP10X_MB_RESPONSE_DECODER;

    if (syntiant_ndp10x_mailbox_trace) {
        if (1 < syntiant_ndp10x_mailbox_trace
            || (r != SYNTIANT_NDP10X_MB_RESPONSE_CONT
                   && r != SYNTIANT_NDP10X_MB_RESPONSE_ERROR
                   && (r & ~0x3) != SYNTIANT_NDP10X_MB_RESPONSE_DATA)) {
            SYNTIANT_PRINTF("%s rsp  %d: ", mailbox_dir[m2h],
                syntiant_ndp10x_mailbox_rsp_number[m2h]);
            if (r < sizeof(rsps) / sizeof(rsps[0])) {
                SYNTIANT_PRINTF("%s\n", rsps[r]);
            } else {
                SYNTIANT_PRINTF("ILLEGAL(%d)\n", r);
            }
        }
    }
    syntiant_ndp10x_mailbox_rsp_number[m2h]++;
}

void syntiant_ndp10x_mb_data_trace(int m2h, uint8_t *data, int bits);
void
syntiant_ndp10x_mb_data_trace(int m2h, uint8_t *data, int bits)
{
    uint32_t d = 0;
    int i;

    if (syntiant_ndp10x_mailbox_trace) {
        SYNTIANT_PRINTF("%s data %d: ", mailbox_dir[m2h],
            syntiant_ndp10x_mailbox_rsp_number[m2h]);
        if (bits <= (int) sizeof(d) * 8) {
            memmove(&d, data, (size_t) ((bits + 7) / 8));
            SYNTIANT_PRINTF("0x%x", d);
        } else {
            for (i = 0; i < (bits + 7) / 8; i++) {
                SYNTIANT_PRINTF("0x%02x ", data[i]);
            }
        }
        SYNTIANT_PRINTF("\n");
    }
}

void syntiant_ndp10x_mb_unexpected_trace(int m2h, uint8_t r, uint8_t bits);
void
syntiant_ndp10x_mb_unexpected_trace(int m2h, uint8_t r, uint8_t bits)
{
    unsigned int n;

    if (syntiant_ndp10x_mailbox_trace) {
        n = m2h ? syntiant_ndp10x_mailbox_req_number[m2h]
                : syntiant_ndp10x_mailbox_rsp_number[m2h];
        SYNTIANT_PRINTF("%s unex %d: %d message unexpected"
                            ", %d data bits remaining\n",
                            mailbox_dir[m2h], n, r, bits);
    }
    syntiant_ndp10x_mailbox_unexpected[m2h]++;
}

void syntiant_ndp10x_mb_error_trace(int m2h, uint8_t e);
void
syntiant_ndp10x_mb_error_trace(int m2h, uint8_t e)
{
    static char *errors[] = SYNTIANT_NDP10X_MB_ERROR_DECODER;

    if (syntiant_ndp10x_mailbox_trace) {
        SYNTIANT_PRINTF("%s err  %d: ", mailbox_dir[m2h],
            syntiant_ndp10x_mailbox_rsp_number[m2h]);
        if (e < sizeof(errors) / sizeof(errors[0])) {
            SYNTIANT_PRINTF("%s\n", errors[e]);
        } else {
            SYNTIANT_PRINTF("ILLEGAL(%d)\n", e);
        }
    }
    syntiant_ndp10x_mailbox_error[m2h]++;
}
#else
#define syntiant_ndp10x_mb_request_trace(m2h, r)                               \
    do {                                                                       \
    } while (0)
#define syntiant_ndp10x_mb_response_trace(m2h, r)                              \
    do {                                                                       \
    } while (0)
#define syntiant_ndp10x_mb_data_trace(m2h, d, b)                               \
    do {                                                                       \
    } while (0)
#define syntiant_ndp10x_mb_unexpected_trace(m2h, r, bits)                      \
    do {                                                                       \
    } while (0)
#define syntiant_ndp10x_mb_error_trace(m2h, e)                                 \
    do {                                                                       \
    } while (0)
#endif

void
syntiant_ndp10x_reset_mailbox_state(struct syntiant_ndp_device_s *ndp)
{
    int i;

    for (i = 0; i < SYNTIANT_NDP10X_DEVICE_MAILBOX_DIRECTIONS; i++) {
        memset(&ndp->d.ndp10x.mb[i], 0,
            sizeof(struct syntiant_ndp10x_device_mb_state_s));
    }
    ndp->d.ndp10x.mbin = 0;
    ndp->d.ndp10x.mbin_resp = 0;
    ndp->d.ndp10x.fw_pointers_addr = 0;
    ndp->d.ndp10x.fw_state_addr = 0;
}

void
syntiant_ndp10x_tx_mb_data(
    struct syntiant_ndp10x_device_mb_state_s *mb, int bits, uint8_t *data)
{
    /* TODO: don't assert */
    assert(bits <= SYNTIANT_NDP10X_DEVICE_MAX_DATA);
    mb->out = 1;
    mb->index = 0;
    mb->bits = (uint8_t) bits;
    memmove(mb->data, data, (size_t) ((bits + 7) / 8));
}

void
syntiant_ndp10x_rx_mb_data(
    struct syntiant_ndp10x_device_mb_state_s *mb, int bits)
{
    /* TODO: don't assert */
    assert(bits <= SYNTIANT_NDP10X_DEVICE_MAX_DATA);
    mb->out = 0;
    mb->index = 0;
    mb->bits = (uint8_t) bits;
    memset(mb->data, 0, (size_t) ((bits + 7) / 8));
}

uint8_t
syntiant_ndp10x_tx_next_mb_data(
    struct syntiant_ndp10x_device_mb_state_s *mb, int *last)
{
    unsigned int di, b, s;
    uint8_t d;

    assert(mb->index < mb->bits);
    di = mb->index;
    b = di >> 3;
    s = di & 0x6;
    /* data request and response code have the same value */
    d = (uint8_t) (SYNTIANT_NDP10X_MB_REQUEST_DATA | ((mb->data[b] >> s) & 0x3));
    mb->index = (uint8_t) (di + 2);

    *last = (mb->bits == mb->index);
    return d;
}

/*
 * @return true if all expected data has been received
 */
void
syntiant_ndp10x_rx_next_mb_data(
    struct syntiant_ndp10x_device_mb_state_s *mb, uint8_t d, int *last)
{
    int di, b, s;
    uint8_t d0;

    assert(mb->index < mb->bits);

    di = mb->index;
    b = di >> 3;
    s = di & 0x6;

    d0 = mb->data[b];
    d0 = (uint8_t) ((d0 & ~(0x3 << s)) | ((d & 0x3) << s));
    mb->data[b] = d0;
    mb->index = (uint8_t) (di + 2);

    *last = (mb->bits == mb->index);
}

void
syntiant_ndp10x_inc_uint8(uint8_t *p)
{
    uint8_t v = *p;
    if (v < 0xff) v++;

    *p = v;
}

int
syntiant_ndp10x_mbwait(struct syntiant_ndp_device_s *ndp)
{
    int s;

    s = (ndp->iif.mbwait)(ndp->iif.d);
    if (s) goto error;

    s = ndp->d.ndp10x.mb[NDP10X_H2M].error;

error:
    return s;
}

int
syntiant_ndp10x_op_size(struct syntiant_ndp_device_s *ndp, int mcu,
                        unsigned int *size)
{
    ndp = ndp;
    *size = mcu ? 4 : 1;

    return SYNTIANT_NDP_ERROR_NONE;
}

int
syntiant_ndp10x_read_block(struct syntiant_ndp_device_s *ndp, int mcu,
                           uint32_t address, void *value, unsigned int count)
{
    int s;

    do {
        unsigned int max_bytes_allowed = 0;
        unsigned int bytes_to_read;
        uint32_t addr_wrap = address % SYNTIANT_NDP10X_MAX_SPI_BLOCK_SIZE;

        max_bytes_allowed = 
            SYNTIANT_NDP10X_MAX_SPI_BLOCK_SIZE - addr_wrap;

        bytes_to_read
            = (count > max_bytes_allowed) ? max_bytes_allowed : count;

        s = (ndp->iif.transfer)(
            ndp->iif.d, mcu, address, NULL, value, bytes_to_read);
        if (s) goto error;

        address += (uint32_t) bytes_to_read;
        value = (uint8_t *) value + bytes_to_read;
        count -= bytes_to_read;

    } while (count != 0);

error:
    return s;
}

int
syntiant_ndp10x_write_block(struct syntiant_ndp_device_s *ndp, int mcu,
                            uint32_t address, void *value, unsigned int count)
{
    int s;

    do {
        unsigned int max_bytes_allowed = 0;
        unsigned int bytes_to_write;
        uint32_t addr_wrap = address % SYNTIANT_NDP10X_MAX_SPI_BLOCK_SIZE;
        
        max_bytes_allowed =
            (SYNTIANT_NDP10X_MAX_SPI_BLOCK_SIZE - addr_wrap);

        bytes_to_write
            = (count > max_bytes_allowed) ? max_bytes_allowed : count;

        s = (ndp->iif.transfer)(
            ndp->iif.d, mcu, address, value, NULL, bytes_to_write);

        if (s) goto error;

        if (mcu || address != NDP10X_SPI_SAMPLE) {
            address += (uint32_t) bytes_to_write;
        }
        value = (uint8_t*)value + bytes_to_write;
        count -= bytes_to_write;

    } while (count != 0);

error:
    return s;
}

int
syntiant_ndp10x_read(
    struct syntiant_ndp_device_s *ndp, int mcu, uint32_t address, void *value)
{
    int s;

    s = syntiant_ndp10x_read_block(ndp, mcu, address, value,
                                   mcu ? 4 : 1);

    return s;
}

int
syntiant_ndp10x_write(struct syntiant_ndp_device_s *ndp, int mcu,
    uint32_t address, uint32_t value)
{
    int s;
    s = syntiant_ndp10x_write_block(ndp, mcu, address, &value,
                                    mcu ? 4 : 1);
    return s;
}

int
syntiant_ndp10x_load_fw_pointers(struct syntiant_ndp_device_s *ndp,
                                 uint32_t address)
{
    struct syntiant_ndp10x_device_s *ndp10x = &ndp->d.ndp10x;
    int s = SYNTIANT_NDP_ERROR_NONE;
    struct ndp10x_fw_state_pointers_s fwps;
    
    s = syntiant_ndp10x_read_block(ndp, 1, address, fwps.addresses,
                                   sizeof(fwps.addresses));
    if (s) {
        goto out;
    }
    ndp10x->fw_pointers_addr = address;
    ndp10x->fw_state_addr =
        fwps.addresses[NDP10X_FW_STATE_ADDRESS_INDEX_FW_STATE];
    ndp10x->fw_agc_addr =
        fwps.addresses[NDP10X_FW_STATE_ADDRESS_INDEX_GAIN_CONTROLLER];
    ndp10x->fw_posterior_state_addr =
        fwps.addresses[NDP10X_FW_STATE_ADDRESS_INDEX_POSTERIOR_STATE];
    ndp10x->fw_posterior_parameters_addr =
        fwps.addresses[NDP10X_FW_STATE_ADDRESS_INDEX_POSTERIOR_PARAMS];
    ndp10x->tank_address =
        fwps.addresses[NDP10X_FW_STATE_ADDRESS_INDEX_TANK_START];
    ndp10x->tank_max_size =
        fwps.addresses[NDP10X_FW_STATE_ADDRESS_INDEX_TANK_END]
        - ndp10x->tank_address;

    if (ndp10x->fw_state_addr) {
        address = ndp10x->fw_state_addr
            + (uint32_t) offsetof(struct ndp10x_fw_state_s, match_ring_size);
        s = syntiant_ndp10x_read(ndp, 1, address, &ndp10x->match_ring_size);
        if (s) {
            goto out;
        }
    }
    
 out:
    return s;
}

int syntiant_ndp10x_do_mailbox_req_no_sync(struct syntiant_ndp_device_s *ndp,
                                           uint8_t req);
int
syntiant_ndp10x_do_mailbox_req_no_sync(struct syntiant_ndp_device_s *ndp,
                                       uint8_t req)
{
    struct syntiant_ndp10x_device_s *ndp10x = &ndp->d.ndp10x;
    struct syntiant_ndp10x_device_mb_state_s *mb = &ndp10x->mb[NDP10X_H2M];
    int s = SYNTIANT_NDP_ERROR_FAIL;
    uint8_t mbin_next;
    uint32_t addr;

    if (mb->op != SYNTIANT_NDP10X_MB_OP_NONE) {
        /* TODO: internal error */
        goto error;
    }
    if (mb->phase != SYNTIANT_NDP10X_MB_PHASE_IDLE) {
        /* TODO: internal error */
        goto error;
    }

    switch (req) {
    case SYNTIANT_NDP10X_MB_REQUEST_NOP:
        mb->op = SYNTIANT_NDP10X_MB_OP_NOP;
        break;
    case SYNTIANT_NDP10X_MB_REQUEST_MIADDR:
        mb->op = SYNTIANT_NDP10X_MB_OP_MIADDR;
        break;
    default:
        /* TODO: internal error */
        goto error;
    }

    mb->error = SYNTIANT_NDP_ERROR_NONE;
    mb->phase = SYNTIANT_NDP10X_MB_PHASE_OP;
    if (0x7 < req) {
        syntiant_ndp10x_mb_request_trace(NDP10X_H2M, req);
        syntiant_ndp10x_tx_mb_data(mb, 6, &req);
        req = SYNTIANT_NDP10X_MB_REQUEST_EXTOP;
        mb->phase = SYNTIANT_NDP10X_MB_PHASE_EXTOP;
    }

    syntiant_ndp10x_mb_request_trace(NDP10X_H2M, req);
    mbin_next = (uint8_t)
        SYNTIANT_NDP10X_MB_HOST_TO_MCU_INSERT(ndp10x->mbin, req);

    s = syntiant_ndp10x_write(ndp, 0, NDP10X_SPI_MBIN, mbin_next);
    if (s) goto error;

    ndp10x->mbin = mbin_next;

    s = syntiant_ndp10x_mbwait(ndp);
    if (s) goto error;

    switch (mb->op) {
    case SYNTIANT_NDP10X_MB_OP_NOP:
        break;
    case SYNTIANT_NDP10X_MB_OP_MIADDR:
        if (!mb->error) {
            /* assuming little endian */
            memmove(&addr, mb->data, 4);
            s = syntiant_ndp10x_load_fw_pointers(ndp, addr);
            if (s) {
                goto error;
            }
        }
    }

    s = mb->error;

error:
    mb->op = SYNTIANT_NDP10X_MB_OP_NONE;
    mb->phase = SYNTIANT_NDP10X_MB_PHASE_IDLE;
    return s;
}

/*
 * Used by the CLI, mostly for debugging
 */
int
syntiant_ndp10x_do_mailbox_req(struct syntiant_ndp_device_s *ndp, uint8_t req)
{
    int s, s0;
    s = (ndp->iif.sync)(ndp->iif.d);
    if (s) goto error;

    s = syntiant_ndp10x_do_mailbox_req_no_sync(ndp, req);

    s0 = (ndp->iif.unsync)(ndp->iif.d);
    s = s ? s : s0;

error:
    return s;
}


int
syntiant_ndp10x_restore_config(struct syntiant_ndp_device_s *ndp)
{
    struct syntiant_ndp10x_device_s *ndp10x = &ndp->d.ndp10x;
    unsigned int classes = 0;
    int s = SYNTIANT_NDP_ERROR_NONE;

    struct syntiant_ndp_config_s config;
    memset(&config, 0, sizeof(struct syntiant_ndp_config_s));
    s = syntiant_ndp10x_access_config(ndp, &config, &classes);
    if (s) goto error;
    
    if (config.labels_len > 0) {
        ndp10x->labels_len = config.labels_len;   
        ndp10x->classes = classes;
    }

    if (config.firmware_version_len > 0) {
        ndp10x->fwver_len = config.firmware_version_len;
    }
    
    if (config.parameters_version_len > 0) {
        ndp10x->paramver_len = config.parameters_version_len;
    }
    
    if (config.pkg_version_len > 0) {
        ndp10x->pkgver_len = config.pkg_version_len;
    }

error:
    return s;
}

const long int ndp10x_reset_fix_v1_synpkg_size = 280;
const unsigned char ndp10x_reset_fix_v1_synpkg[280] = {
    0x01, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0xA1, 0xE5, 0xBD, 0x53, 0x02, 0x00, 0x00, 0x00,
    0x14, 0x00, 0x00, 0x00, 0x6E, 0x64, 0x70, 0x31, 0x30, 0x78, 0x2D, 0x72, 0x65, 0x73, 0x65, 0x74,
    0x2D, 0x66, 0x69, 0x78, 0x5F, 0x76, 0x31, 0x00, 0x08, 0x00, 0x00, 0x00, 0xDC, 0x00, 0x00, 0x00,
    0x00, 0x70, 0x01, 0x20, 0xC1, 0xC0, 0xFF, 0x1F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0xF0, 0x00, 0xF8, 0x03, 0x4B, 0x04, 0x4A, 0x19, 0x68, 0x0A, 0x40, 0x1A, 0x60, 0x19, 0x60,
    0x30, 0xBF, 0xFD, 0xE7, 0x04, 0x90, 0x00, 0x40, 0xFF, 0xFF, 0x7F, 0xFF, 0x04, 0x00, 0x00, 0x00,
    0x04, 0x00, 0x00, 0x00, 0xF2, 0x6D, 0x45, 0xAB
};

enum syntiant_ndp10x_boot_mb_const_e {
    SYNTIANT_NDP10X_BOOT_MB_OPEN_RAM_SIZE = 1024,
    SYNTIANT_NDP10X_BOOT_MB_OPEN_RAM_BASE = 0x20017c00,
    SYNTIANT_NDP10X_BOOT_MB_OWNER_MASK = 0x80,
    SYNTIANT_NDP10X_BOOT_MB_PAYLOAD_MASK = 0x7f,
    SYNTIANT_NDP10X_BOOT_MB_REQUEST_LOAD = 0x9,
    SYNTIANT_NDP10X_BOOT_MB_REQUEST_BOOTING = 0x11,
    SYNTIANT_NDP10X_BOOT_MB_RESPONSE_ERROR = 0x20
};

int
syntiant_ndp10x_wait_mb(struct syntiant_ndp_device_s *ndp, uint32_t address,
                        uint8_t owner, uint8_t *vp)
{
    int i, s;
    uint8_t v;
    const int mb_timeout = 128;
    
    for (i = 0; i < mb_timeout; i++) {
        s = syntiant_ndp10x_read(ndp, 0, address, &v);
        if (s) {
            return s;
        }
        if ((v & SYNTIANT_NDP10X_BOOT_MB_OWNER_MASK) == owner) {
            break;
        }
    }
    
    if (i == mb_timeout) {
        return SYNTIANT_NDP_ERROR_TIMEOUT;
    }

    *vp = v;
    
    return 0;
}

int
syntiant_ndp10x_reset_fix(struct syntiant_ndp_device_s *ndp)
{
    uint8_t *pbytes = (uint8_t *) ndp10x_reset_fix_v1_synpkg;
    unsigned int package_len = (unsigned int) ndp10x_reset_fix_v1_synpkg_size;
    uint8_t mbin, mbout, mbin_resp, mbout_resp, req, resp;
    unsigned int len, i, j;
    int s, s0;

    s = syntiant_ndp10x_write(ndp, 0, NDP10X_SPI_INTCTL, 0);
    if (s) {
        goto out;
    }
    
    s = syntiant_ndp10x_write(ndp, 0, NDP10X_SPI_PLL,
                              NDP10X_SPI_PLL_PLLRSTB(0));
    if (s) {
        goto out;
    }
    
    s = syntiant_ndp10x_write(ndp, 0, NDP10X_SPI_PLL,
                              NDP10X_SPI_PLL_PLLRSTB(1));
    if (s) {
        goto out;
    }
    
    s = syntiant_ndp10x_write(ndp, 0, NDP10X_SPI_CTL,
                              NDP10X_SPI_CTL_RESETN(0)
                              | NDP10X_SPI_CTL_CLKEN(1)
                              | NDP10X_SPI_CTL_EXTCLK(0)
                              | NDP10X_SPI_CTL_BOOTDISABLE(1));
    if (s) {
        goto out;
    }
    
    s = syntiant_ndp10x_write(ndp, 0, NDP10X_SPI_CTL,
                              NDP10X_SPI_CTL_RESETN(1)
                              | NDP10X_SPI_CTL_CLKEN(1)
                              | NDP10X_SPI_CTL_EXTCLK(0)
                              | NDP10X_SPI_CTL_BOOTDISABLE(0));
    if (s) {
        goto out;
    }
    
    s = syntiant_ndp10x_wait_mb(ndp, NDP10X_SPI_MBOUT,
                                SYNTIANT_NDP10X_BOOT_MB_OWNER_MASK, &mbout);
    if (s) {
        goto out;
    }
    
    req = mbout & SYNTIANT_NDP10X_BOOT_MB_PAYLOAD_MASK;
    if (req != SYNTIANT_NDP10X_BOOT_MB_REQUEST_BOOTING) {
        s = SYNTIANT_NDP_ERROR_FAIL;
        goto out;
    }
    
    mbout_resp = SYNTIANT_NDP10X_BOOT_MB_OWNER_MASK
        | SYNTIANT_NDP10X_MB_RESPONSE_SUCCESS;
    s = syntiant_ndp10x_write(ndp, 0, NDP10X_SPI_MBOUT_RESP, mbout_resp);
    if (s) {
        goto out;
    }

    mbin = SYNTIANT_NDP10X_BOOT_MB_OWNER_MASK
        | SYNTIANT_NDP10X_BOOT_MB_REQUEST_LOAD;
    s = syntiant_ndp10x_write(ndp, 0, NDP10X_SPI_MBIN, mbin);
    if (s) {
        goto out;
    }

    s = syntiant_ndp10x_wait_mb(ndp, NDP10X_SPI_MBIN_RESP,
                                SYNTIANT_NDP10X_BOOT_MB_OWNER_MASK & mbin,
                                &mbin_resp);
    if (s) {
        goto out;
    }

    resp = mbin_resp & SYNTIANT_NDP10X_BOOT_MB_PAYLOAD_MASK;
    if (resp != SYNTIANT_NDP10X_MB_RESPONSE_CONT) {
        s = SYNTIANT_NDP_ERROR_FAIL;
        goto out;
    }
    
    for (i = 0; i < package_len; i += len) {
        len = package_len - i;
        if (SYNTIANT_NDP10X_BOOT_MB_OPEN_RAM_SIZE < len) {
            len = SYNTIANT_NDP10X_BOOT_MB_OPEN_RAM_SIZE;
        }
        for (j = 0; j < len; j+= 4) {
            s = syntiant_ndp10x_write_block(ndp, 1,
                                    SYNTIANT_NDP10X_BOOT_MB_OPEN_RAM_BASE + j,
                                    &pbytes[i] + j, 4);
            if (s) {
                goto out;
            }
        }
        mbin = (uint8_t) (((mbin & SYNTIANT_NDP10X_BOOT_MB_OWNER_MASK)
                           ^ SYNTIANT_NDP10X_BOOT_MB_OWNER_MASK)
                          | SYNTIANT_NDP10X_MB_REQUEST_CONT);
        s = syntiant_ndp10x_write(ndp, 0, NDP10X_SPI_MBIN, mbin);
        if (s) {
            goto out;
        }
        
        s = syntiant_ndp10x_wait_mb(ndp, NDP10X_SPI_MBIN_RESP,
                                    SYNTIANT_NDP10X_BOOT_MB_OWNER_MASK & mbin,
                                    &mbin_resp);
        if (s) {
            goto out;
        }
        
        resp = mbin_resp & SYNTIANT_NDP10X_BOOT_MB_PAYLOAD_MASK;
        if (resp == SYNTIANT_NDP10X_MB_RESPONSE_SUCCESS) {
            break;
        }
        if (resp == SYNTIANT_NDP10X_BOOT_MB_RESPONSE_ERROR) {
            s = SYNTIANT_NDP_ERROR_PACKAGE;
            goto out;
        }
        if (resp != SYNTIANT_NDP10X_MB_RESPONSE_CONT) {
            s = SYNTIANT_NDP_ERROR_FAIL;
            goto out;
        }
    }
    
    if (resp != SYNTIANT_NDP10X_MB_RESPONSE_SUCCESS) {
        s = SYNTIANT_NDP_ERROR_PACKAGE;
    }
    
 out:
    s0 = syntiant_ndp10x_write(ndp, 0, NDP10X_SPI_INTSTS, 0x7f);
    s = s ? s : s0;
    return s;
}


int
syntiant_ndp10x_reset(struct syntiant_ndp_device_s *ndp)
{

    int s;
    uint32_t data;
    uint32_t config_address;

    /* Reset the NDP101 */
    data = NDP10X_SPI_CTL_RESETN(0) | NDP10X_SPI_CTL_CLKEN(1)
         | NDP10X_SPI_CTL_EXTCLK(0) | NDP10X_SPI_CTL_BOOTDISABLE(1);
    s = syntiant_ndp10x_write(ndp, 0, NDP10X_SPI_CTL, data);
    if (s) goto error;

    /* Clear SPI registers (not cleared by reset) */
    data = 0x0;
    s = syntiant_ndp10x_write(ndp, 0, NDP10X_SPI_INTCTL, data);
    if (s) goto error;
    s = syntiant_ndp10x_write(ndp, 0, NDP10X_SPI_MBIN, data);
    if (s) goto error;
    s = syntiant_ndp10x_write(ndp, 0, NDP10X_SPI_MBOUT_RESP, data);
    if (s) goto error;
    data = 0x7f;
    s = syntiant_ndp10x_write(ndp, 0, NDP10X_SPI_INTSTS, data);
    if (s) goto error;
    data = 0;
    s = syntiant_ndp10x_write(ndp, 0, NDP10X_SPI_INTSTS, data);
    if (s) goto error;

    /* de-assert reset */
    data = NDP10X_SPI_CTL_RESETN(1) | NDP10X_SPI_CTL_CLKEN(1)
         | NDP10X_SPI_CTL_EXTCLK(0) | NDP10X_SPI_CTL_BOOTDISABLE(0);
    s = syntiant_ndp10x_write(ndp, 0, NDP10X_SPI_CTL, data);
    if (s) goto error;
    
    /* Drive the interrupt line output active high interrupts */
    data = NDP10X_SPI_CFG_INTEN(1) | NDP10X_SPI_CFG_INTNEG(0);
    s = syntiant_ndp10x_write(ndp, 0, NDP10X_SPI_CFG, data);
    if (s) goto error;

    /*
     * Disable remapping to bootrom to address 0x0
     */
    s = syntiant_ndp10x_write(ndp, 1, NDP10X_SYSCTL, 0);
    if (s) goto error;

    /*
     * Disable the holding tank so it doesn't spew all over
     * firmware memory
     */
    s = syntiant_ndp10x_write(
        ndp, 1, NDP10X_DSP_CONFIG_TANK, NDP10X_DSP_CONFIG_TANK_ENABLE(0));
    if (s) goto error;

    /* Disable memory access [set on reset] */
    s = syntiant_ndp10x_write(ndp, 1, NDP10X_DNN_CONFIG_DNNCTL0,
                              NDP10X_DNN_CONFIG_DNNCTL0_NUMFILT
                              (SYNTIANT_NDP10X_MAX_FREQUENCY_BINS)
                              | NDP10X_DNN_CONFIG_DNNCTL0_RSTB(1));
    if (s) goto error;

    /* invalidate config regions */
    data = 0x0;
    config_address = NDP10X_ILIB_SCRATCH_ORIGIN +
        offsetof(struct syntiant_ndp10x_config_layout, valid);
    s = syntiant_ndp10x_write(ndp, 1, config_address, data);
    if (s) goto error;
    
 error:
    return s;
}

int
syntiant_ndp10x_init(struct syntiant_ndp_device_s *ndp,
                     enum syntiant_ndp_init_mode_e init_mode)
{
    int s = SYNTIANT_NDP_ERROR_NONE;
    int s0;
    uint8_t data;
    struct syntiant_ndp10x_device_s *ndp10x = &ndp->d.ndp10x;
    uint8_t mbin, mbin_resp;
    uint32_t notifications;
    struct syntiant_ndp10x_config_s config;
    struct syntiant_ndp10x_config_state_s st;
    uint32_t addr;

    memset(ndp10x, 0, sizeof(struct syntiant_ndp10x_device_s));
    ndp10x->tank_max_size = 64 * 1024;
    ndp10x->tank_address = 0x20001000;

    switch (init_mode) {
    case SYNTIANT_NDP_INIT_MODE_NO_TOUCH:
        break;

    case SYNTIANT_NDP_INIT_MODE_RESTART:
        s = syntiant_ndp10x_config_read_state(ndp, &st);
        if (s) goto error;
        /* read input clk rate from NDP */
        addr = NDP10X_ILIB_SCRATCH_ORIGIN +
            offsetof(struct syntiant_ndp10x_config_layout, input_clk);
        s = syntiant_ndp10x_read(ndp, 1, addr, &(st.input_clock_rate));
        if (s) goto error;

        ndp10x->input_clock_rate = st.input_clock_rate;

        syntiant_ndp10x_config_get_input(&st, &config, ndp10x);

        s = syntiant_ndp10x_read(ndp, 0, NDP10X_SPI_MBIN, (uint32_t *)&mbin);
        if (s) goto error;
        s = syntiant_ndp10x_read(ndp, 0, NDP10X_SPI_MBIN_RESP,
                                 (uint32_t *) &mbin_resp);
        if (s) goto error;
        ndp10x->mbin = mbin;
        ndp10x->mbin_resp = mbin_resp;
        s0 = syntiant_ndp10x_restore_config(ndp);
        if (s0) {
            if (s0 != SYNTIANT_NDP_ERROR_PACKAGE) {
                s = s0;
                goto error;
            }
        } else {
            s = syntiant_ndp10x_read(ndp, 0, NDP10X_SPI_CTL, &data);
            if (s) goto error;
            /* needed for mbwait, which will call syntiant_ndp_poll() */
            if (!(data & NDP10X_SPI_CTL_BOOTDISABLE(1))) {
                ndp->init = 1;
                /* clear any outstanding notifications */
                s = syntiant_ndp10x_poll(ndp, &notifications, 1);
                if (s) goto error;
                s = syntiant_ndp10x_load_fw_pointers
                    (ndp, SYNTIANT_NDP10X_FW_POINTERS_ADDRESS);
                if (s) {
                    goto error;
                }
                addr = ndp10x->fw_state_addr
                    + (uint32_t) offsetof(struct ndp10x_fw_state_s,
                                          match_producer);
                s = syntiant_ndp10x_read(ndp, 1, addr, &ndp10x->match_producer);
                if (s) {
                    goto error;
                }
                ndp10x->match_consumer = ndp10x->match_producer;
            }
        }
        break;

    case SYNTIANT_NDP_INIT_MODE_RESET:
        s = syntiant_ndp10x_reset(ndp);
        break;

    default:
        s = SYNTIANT_NDP_ERROR_ARG;
        break;
    }
error:
    ndp->init = 0;
    return s;
}

int
syntiant_ndp10x_uninit(
    struct syntiant_ndp_device_s *ndp, enum syntiant_ndp_init_mode_e init_mode)
{
    int s = SYNTIANT_NDP_ERROR_NONE;

    if (init_mode == SYNTIANT_NDP_INIT_MODE_RESET) {
        s = syntiant_ndp10x_reset(ndp);
    }

    return s;
}

uint8_t
syntiant_ndp10x_do_mcu_to_host(
    struct syntiant_ndp10x_device_s *ndp10x, uint8_t mbin_resp,
    uint8_t mbin_next)
{
    int last;
    uint8_t req, data, data_in, data_out, data_req, cont_req, err;
    uint8_t m2h_next_rsp = MB_NONE;
    struct syntiant_ndp10x_device_mb_state_s *mb = &ndp10x->mb[NDP10X_M2H];

    req = (uint8_t) ((mbin_resp & SYNTIANT_NDP10X_MB_MCU_TO_HOST_M)
                     >> SYNTIANT_NDP10X_MB_MCU_TO_HOST_S);
    syntiant_ndp10x_mb_request_trace(NDP10X_M2H, req);
    data = mb->index < mb->bits;
    data_in = data && !mb->out;
    data_out = data && mb->out;
    data_req = ((req & SYNTIANT_NDP10X_MB_REQUEST_DATA)
        == SYNTIANT_NDP10X_MB_REQUEST_DATA);
    cont_req = (req == SYNTIANT_NDP10X_MB_REQUEST_CONT);
    if ((data_in ^ data_req) || (data_out ^ cont_req)) {
        m2h_next_rsp = SYNTIANT_NDP10X_MB_RESPONSE_ERROR;
        err = SYNTIANT_NDP10X_MB_ERROR_UNEXPECTED;
        mb->phase = SYNTIANT_NDP10X_MB_PHASE_ERROR;
        syntiant_ndp10x_tx_mb_data(mb, 6, &err);
        syntiant_ndp10x_inc_uint8(&mb->unexpected);
        syntiant_ndp10x_mb_unexpected_trace
            (NDP10X_M2H, req, (uint8_t) (mb->bits - mb->index));
    } else {
        switch (req) {
        case SYNTIANT_NDP10X_MB_REQUEST_NOP:
            m2h_next_rsp = SYNTIANT_NDP10X_MB_RESPONSE_SUCCESS;
            break;
        case SYNTIANT_NDP10X_MB_REQUEST_CONT:
            m2h_next_rsp = syntiant_ndp10x_tx_next_mb_data(mb, &last);
            break;
        case SYNTIANT_NDP10X_MB_REQUEST_MATCH:
            ndp10x->matches++;
            m2h_next_rsp = SYNTIANT_NDP10X_MB_RESPONSE_SUCCESS;
            break;
        case SYNTIANT_NDP10X_MB_REQUEST_EXTOP:
            syntiant_ndp10x_rx_mb_data(mb, 6);
            mb->phase = SYNTIANT_NDP10X_MB_PHASE_EXTOP;
            m2h_next_rsp = SYNTIANT_NDP10X_MB_RESPONSE_CONT;
            break;
        case SYNTIANT_NDP10X_MB_REQUEST_DATA | 0:
        case SYNTIANT_NDP10X_MB_REQUEST_DATA | 1:
        case SYNTIANT_NDP10X_MB_REQUEST_DATA | 2:
        case SYNTIANT_NDP10X_MB_REQUEST_DATA | 3:
            syntiant_ndp10x_rx_next_mb_data(mb, req, &last);
            if (last) {
                syntiant_ndp10x_mb_data_trace(NDP10X_M2H, mb->data, mb->bits);
#if 0
                switch (mb->phase) {
                case SYNTIANT_NDP10X_MB_PHASE_EXTOP:
                    /* extend from here when EXTOPs are added */
                }
#endif
                mb->phase = SYNTIANT_NDP10X_MB_PHASE_IDLE;
                m2h_next_rsp = SYNTIANT_NDP10X_MB_RESPONSE_SUCCESS;
            } else {
                m2h_next_rsp = SYNTIANT_NDP10X_MB_RESPONSE_CONT;
            }
            break;
        }
    }
    syntiant_ndp10x_mb_response_trace(NDP10X_M2H, m2h_next_rsp);
    mbin_next = (uint8_t)
        SYNTIANT_NDP10X_MB_MCU_TO_HOST_INSERT(mbin_next, m2h_next_rsp);
    return mbin_next;
}

uint8_t
syntiant_ndp10x_do_host_to_mcu(struct syntiant_ndp10x_device_s *ndp10x,
    uint8_t mbin_resp, uint8_t mbin_next, int *wake)
{
    int last;
    int done = 0;
    uint8_t rsp, data, data_in, data_out, data_rsp, cont_rsp;
    uint8_t h2m_next_req = MB_NONE;
    struct syntiant_ndp10x_device_mb_state_s *mb = &ndp10x->mb[NDP10X_H2M];

    rsp = (mbin_resp & SYNTIANT_NDP10X_MB_HOST_TO_MCU_M)
        >> SYNTIANT_NDP10X_MB_HOST_TO_MCU_S;
    syntiant_ndp10x_mb_response_trace(NDP10X_H2M, rsp);
    data = mb->index < mb->bits;
    data_in = data && !mb->out;
    data_out = data && mb->out;
    data_rsp = ((rsp & SYNTIANT_NDP10X_MB_RESPONSE_DATA)
        == SYNTIANT_NDP10X_MB_RESPONSE_DATA);
    cont_rsp = (rsp == SYNTIANT_NDP10X_MB_RESPONSE_CONT);
    if ((data_in ^ data_rsp) || (data_out ^ cont_rsp)) {
        syntiant_ndp10x_mb_unexpected_trace
            (NDP10X_H2M, rsp, (uint8_t) (mb->bits - mb->index));
        syntiant_ndp10x_inc_uint8(&mb->unexpected);
        h2m_next_req = SYNTIANT_NDP10X_MB_REQUEST_NOP;
        mb->error = SYNTIANT_NDP_ERROR_FAIL;
        mb->phase = SYNTIANT_NDP10X_MB_PHASE_FLUSH;
        mb->bits = mb->index;
    } else {
        /*
         * the previous clause catches all cases of unexpected data
         * transfer sequencing (i.e. receiving or prompting for data when
         * unexpected and not receiving or prompting it when it is) so
         * no such checks are required here -- only need to consider
         * phase-to-phase sequencing
         */
        switch (rsp) {
        case SYNTIANT_NDP10X_MB_RESPONSE_SUCCESS:
            done = 1;
            break;
        case SYNTIANT_NDP10X_MB_RESPONSE_CONT:
            h2m_next_req = syntiant_ndp10x_tx_next_mb_data(mb, &last);
            if (last) {
                switch (mb->op) {
                case SYNTIANT_NDP10X_MB_OP_MIADDR:
                    mb->phase = SYNTIANT_NDP10X_MB_PHASE_ADDR;
                    syntiant_ndp10x_rx_mb_data(mb, 32);
                    break;
                }
            }
            break;
        case SYNTIANT_NDP10X_MB_RESPONSE_ERROR:
            mb->phase = SYNTIANT_NDP10X_MB_PHASE_ERROR;
            syntiant_ndp10x_rx_mb_data(mb, 6);
            h2m_next_req = SYNTIANT_NDP10X_MB_REQUEST_CONT;
            break;
        case SYNTIANT_NDP10X_MB_RESPONSE_DATA | 0:
        case SYNTIANT_NDP10X_MB_RESPONSE_DATA | 1:
        case SYNTIANT_NDP10X_MB_RESPONSE_DATA | 2:
        case SYNTIANT_NDP10X_MB_RESPONSE_DATA | 3:
            syntiant_ndp10x_rx_next_mb_data(mb, rsp, &last);
            if (last) {
                switch (mb->phase) {
                case SYNTIANT_NDP10X_MB_PHASE_ERROR:
                    /* eventually decode err, but presently only 1 outcome */
                    /* err = mb->data[0]; */
                    mb->error = SYNTIANT_NDP_ERROR_FAIL;
                    done = 1;
                    break;
                case SYNTIANT_NDP10X_MB_PHASE_FLUSH:
                    done = 1;
                    break;
                case SYNTIANT_NDP10X_MB_PHASE_ADDR:
                    syntiant_ndp10x_mb_data_trace(
                        NDP10X_H2M, mb->data, mb->bits);
                    done = 1;
                }
            } else {
                h2m_next_req = SYNTIANT_NDP10X_MB_REQUEST_CONT;
            }
            break;
        }
    }
    if (h2m_next_req != MB_NONE) {
        syntiant_ndp10x_mb_request_trace(NDP10X_H2M, h2m_next_req);
        mbin_next = (uint8_t)
            SYNTIANT_NDP10X_MB_HOST_TO_MCU_INSERT(mbin_next, h2m_next_req);
    }
    if (done) {
        mb->phase = SYNTIANT_NDP10X_MB_PHASE_IDLE;
    }
    *wake = done;
    return mbin_next;
}

int
syntiant_ndp10x_do_mailboxes(struct syntiant_ndp_device_s *ndp, int *wake)
{
    struct syntiant_ndp10x_device_s *ndp10x = &ndp->d.ndp10x;
    uint8_t mbin_resp, owns, mbin_next;
    int s;

    s = syntiant_ndp10x_read(ndp, 0, NDP10X_SPI_MBIN_RESP, &mbin_resp);
    if (s) goto error;

    owns = mbin_resp ^ ndp10x->mbin_resp;
    mbin_next = ndp10x->mbin;
    if (owns & SYNTIANT_NDP10X_MB_MCU_TO_HOST_OWNER) {
        mbin_next = syntiant_ndp10x_do_mcu_to_host(ndp10x, mbin_resp, mbin_next);
    }
    *wake = 0;
    if (owns & SYNTIANT_NDP10X_MB_HOST_TO_MCU_OWNER) {
        mbin_next
            = syntiant_ndp10x_do_host_to_mcu(ndp10x, mbin_resp, mbin_next, wake);
    }
    if (mbin_next != ndp10x->mbin) {
        s = syntiant_ndp10x_write(ndp, 0, NDP10X_SPI_MBIN, mbin_next);
        if (s) goto error;
        ndp10x->mbin = mbin_next;
    }
    ndp10x->mbin_resp = mbin_resp;

error:
    return s;
}

int
syntiant_ndp10x_interrupts(struct syntiant_ndp_device_s *ndp, int *causes)
{
    int s;
    uint8_t intctl, intctl_old;
    int cs = *causes;

    if (cs > SYNTIANT_NDP_INTERRUPT_ALL
        && cs != SYNTIANT_NDP_INTERRUPT_DEFAULT) {
        s = SYNTIANT_NDP_ERROR_ARG;
        goto error;
    }
    s = syntiant_ndp10x_read(ndp, 0, NDP10X_SPI_INTCTL, &intctl);
    if (s) goto error;

    intctl_old = intctl;

    /* cs >= 0 means "set interrupts" */
    if (cs >= 0)  {
        if (cs == SYNTIANT_NDP_INTERRUPT_DEFAULT) {
            intctl = NDP10X_SPI_INTCTL_MBIN_INTEN(1)
                | NDP10X_SPI_INTCTL_WM_INTEN(1)
                | NDP10X_SPI_INTCTL_AE_INTEN(1);
        } else {
            intctl = (uint8_t)
                (NDP10X_SPI_INTCTL_MATCH_INTEN
                 (!!(cs & SYNTIANT_NDP_INTERRUPT_MATCH))
                 | NDP10X_SPI_INTCTL_MBIN_INTEN
                 (!!(cs & SYNTIANT_NDP_INTERRUPT_MAILBOX_IN))
                 | NDP10X_SPI_INTCTL_MBOUT_INTEN
                 (!!(cs & SYNTIANT_NDP_INTERRUPT_MAILBOX_OUT))
                 | NDP10X_SPI_INTCTL_DNN_INTEN
                 (!!(cs & SYNTIANT_NDP_INTERRUPT_DNN_FRAME))
                 | NDP10X_SPI_INTCTL_FREQ_INTEN
                 (!!(cs & SYNTIANT_NDP_INTERRUPT_FEATURE))
                 | NDP10X_SPI_INTCTL_AE_INTEN
                 (!!(cs & SYNTIANT_NDP_INTERRUPT_ADDRESS_ERROR))
                 | NDP10X_SPI_INTCTL_WM_INTEN
                 (!!(cs & SYNTIANT_NDP_INTERRUPT_WATER_MARK)));
        }
        s = syntiant_ndp10x_write(ndp, 0, NDP10X_SPI_INTCTL, intctl);
        if (s) goto error;
    }

    /* return interrupts */
    cs =
        ((NDP10X_SPI_INTCTL_MATCH_INTEN(1) & intctl_old)
         ? SYNTIANT_NDP_INTERRUPT_MATCH : 0)
        | ((NDP10X_SPI_INTCTL_MBIN_INTEN(1) & intctl_old)
           ? SYNTIANT_NDP_INTERRUPT_MAILBOX_IN : 0)
        | ((NDP10X_SPI_INTCTL_MBOUT_INTEN(1) & intctl_old)
           ? SYNTIANT_NDP_INTERRUPT_MAILBOX_OUT : 0)
        | ((NDP10X_SPI_INTCTL_DNN_INTEN(1) & intctl_old)
           ? SYNTIANT_NDP_INTERRUPT_DNN_FRAME : 0)
        | ((NDP10X_SPI_INTCTL_FREQ_INTEN(1) & intctl_old)
           ? SYNTIANT_NDP_INTERRUPT_FEATURE : 0)
        | ((NDP10X_SPI_INTCTL_AE_INTEN(1) & intctl_old)
           ? SYNTIANT_NDP_INTERRUPT_ADDRESS_ERROR : 0)
        | ((NDP10X_SPI_INTCTL_WM_INTEN(1) & intctl_old)
           ? SYNTIANT_NDP_INTERRUPT_WATER_MARK : 0);

    *causes = cs;

error:
    return s;
}

int
syntiant_ndp10x_poll(struct syntiant_ndp_device_s *ndp,
                     uint32_t *notifications, int clear)
{
    struct syntiant_ndp10x_device_s *ndp10x = &ndp->d.ndp10x;
    uint32_t ns = 0;
    uint32_t addr;
    int wake;
    int s;
    uint8_t intsts;

    s = syntiant_ndp10x_read(ndp, 0, NDP10X_SPI_INTSTS, &intsts);
    if (s) goto error;
    if (clear) {
        s = syntiant_ndp10x_write(ndp, 0, NDP10X_SPI_INTSTS, intsts);
    }
    
    /* H2M response interrupt */
    if (intsts & NDP10X_SPI_INTSTS_MBIN_INT(1)) {
        s = syntiant_ndp10x_do_mailboxes(ndp, &wake);
        if (s) {
            goto error;
        }
        if (ndp10x->matches != ndp10x->prev_matches) {
            if (clear) {
                ndp10x->prev_matches = ndp10x->matches;
            }
            addr = ndp10x->fw_state_addr
                + (uint32_t) offsetof(struct ndp10x_fw_state_s, match_producer);
            s = syntiant_ndp10x_read(ndp, 1, addr, &ndp10x->match_producer);
            if (s) {
                goto error;
            }
            ns |= SYNTIANT_NDP_NOTIFICATION_DNN;
        }
        if (wake) {
            ns |= SYNTIANT_NDP_NOTIFICATION_MAILBOX_IN;
        }
    }

    if (ndp10x->match_producer != ndp10x->match_consumer) {
        ns |= SYNTIANT_NDP_NOTIFICATION_MATCH;
    }

    /* M2H request interrupt */
    if (intsts & NDP10X_SPI_INTSTS_MBOUT_INT(1)) {
        ns |= SYNTIANT_NDP_NOTIFICATION_MAILBOX_OUT;
    }

    /* M2H Frequency domain completion (filter bank) interrupt */
    if (intsts & NDP10X_SPI_INTSTS_FREQ_INT(1)) {
        ns |= SYNTIANT_NDP_NOTIFICATION_FEATURE;
    }

    if (intsts & NDP10X_SPI_INTSTS_AE_INT(1)) {
        ns |= SYNTIANT_NDP_NOTIFICATION_ERROR;
    }

    if (intsts & NDP10X_SPI_INTSTS_WM_INT(1)) {
        ns |= SYNTIANT_NDP_NOTIFICATION_WATER_MARK;
    }
    *notifications = ns;

error:
    return s;
}

int
syntiant_ndp10x_verify_configuration(struct syntiant_ndp_device_s *ndp){

    struct syntiant_ndp10x_device_s *ndp10x = &ndp->d.ndp10x;
    int s;
    struct syntiant_ndp_config_s config;
    memset(&config, 0, sizeof(struct syntiant_ndp_config_s));
    s = syntiant_ndp10x_access_config(ndp, &config, NULL);

    /* label length*/
    if (config.labels_len > 0){
        if (config.labels_len != ndp10x->labels_len){
            s = SYNTIANT_NDP_ERROR_FAIL;
            goto error;
        }
    }

    /* firmware version length */
    if (config.firmware_version_len > 0){
        if (config.firmware_version_len != ndp10x->fwver_len){
            s = SYNTIANT_NDP_ERROR_FAIL;
            goto error;
        }
    }

    /* params version length */
    if (config.parameters_version_len > 0){
        if (config.parameters_version_len != ndp10x->paramver_len){
            s = SYNTIANT_NDP_ERROR_FAIL;
            goto error;
        }
    }

    /* pkg version length */
    if (config.pkg_version_len > 0){
        if (config.pkg_version_len != ndp10x->pkgver_len){
            s = SYNTIANT_NDP_ERROR_FAIL;
            goto error;
        }
    }

error:
    return s;

}

int
syntiant_ndp10x_load(struct syntiant_ndp_device_s *ndp, void *chunk, int len)
{

    int s = SYNTIANT_NDP_ERROR_NONE;
    syntiant_pkg_parser_state_t *pstate = &ndp->pstate;
    struct syntiant_ndp10x_config_s ndp10x_config;
    struct syntiant_ndp10x_config_state_s st;
    struct syntiant_ndp10x_device_s *ndp10x = &ndp->d.ndp10x;
    unsigned int prev_st=0;
    if (len == 0) {
        
        /* check if fw or NN params are already there, not board params*/
        int is_any_pkg_loaded = (ndp10x->paramver_len || ndp10x->fwver_len);

        syntiant_pkg_parser_init(pstate);
        syntiant_pkg_parser_reset(pstate);
        /* verify configuration and ndp10x matches */
        if (is_any_pkg_loaded){
            s = syntiant_ndp10x_verify_configuration(ndp);
            if ( (s && s != SYNTIANT_NDP_ERROR_PACKAGE) ||
                (s && s == SYNTIANT_NDP_ERROR_FAIL) )  {
                goto error;
            }
        }
        s = SYNTIANT_NDP_ERROR_MORE;
        goto error;
    } else {
        syntiant_pkg_preprocess_chunk(pstate, chunk, len, 0);
        syntiant_pkg_parser_reset(pstate);

        while (pstate->ptr < pstate->open_ram_end &&
            pstate->mode != PACKAGE_MODE_DONE) {
            s = syntiant_pkg_parse_chunk(pstate, 0);
            if (s) {
                s = SYNTIANT_NDP_ERROR_PACKAGE;
                goto error;
            }
            if (pstate->mode == PACKAGE_MODE_TAG_START ||
                pstate->mode == PACKAGE_MODE_VALUE_START ||
                pstate->mode == PACKAGE_MODE_VALUE_CONT){
                if(*pstate->tag == TAG_NN_PH_PARAMETERS_V4 ||
                   *pstate->tag == TAG_NN_PH_PARAMETERS_V5) {
                    s = syntiant_ndp10x_config_read_state(ndp, &st);
                    if (s) goto error;

                    memset(&ndp10x_config, 0, sizeof(ndp10x_config));
                    syntiant_ndp10x_config_get_input(&st, &ndp10x_config, ndp10x);
                    prev_st = ndp10x_config.dnn_input;

                    ndp10x_config.set |= SYNTIANT_NDP10X_CONFIG_SET_DNN_INPUT;
                    ndp10x_config.dnn_input = SYNTIANT_NDP10X_CONFIG_DNN_INPUT_NONE;
                    s = syntiant_ndp10x_config_no_sync(ndp, &ndp10x_config);
                    if (s) goto error;
                }
                s = syntiant_ndp10x_parse_tag_values(ndp);
                if (s) goto error;
                if(prev_st) {
                    ndp10x_config.set |= SYNTIANT_NDP10X_CONFIG_SET_DNN_INPUT;
                    ndp10x_config.dnn_input = prev_st;
                    syntiant_ndp10x_config_no_sync(ndp, &ndp10x_config);
                    prev_st = 0;
                }
            }
            if (pstate->ptr == pstate->open_ram_end &&
                pstate->mode != PACKAGE_MODE_DONE){
                s = SYNTIANT_NDP_ERROR_MORE;
                goto error;
            }
        }
    }
error:
    return s;
}

char *
syntiant_ndp10x_get_config_devtype(unsigned int device_type)
{
    char *s = "unknown";

    switch (device_type) {
    case 2:
        s = "ndp100b0emu";
        break;
    case 0x13:
        s = "ndp101es:0x13";
        break;
    case 0x14:
        s = "ndp101es:0x14";
        break;
    case 0x18:
        s = "ndp100es:0x18";
        break;
    case 0x20:
        s = "ndp101b0";
        break;
    case 0x24:
        s = "ndp100b0";
        break;
    }
    return s;
}

int 
syntiant_ndp10x_access_config(struct syntiant_ndp_device_s *ndp, 
                              struct syntiant_ndp_config_s *config,
                              unsigned int *classes) {
    unsigned int config_address;
    unsigned int v;
    unsigned int length, chunk_num, len;
    unsigned int valid;
    int s;
    char *buffer = NULL;
    unsigned int labels_len, firmware_version_len, parameters_version_len,
        pkg_version_len;
    char buff[4];
    unsigned int num_classes = 0;
    uint8_t buffer_index = 0;

    /* check valid bits */
    config_address = NDP10X_ILIB_SCRATCH_ORIGIN + 
        offsetof(struct syntiant_ndp10x_config_layout, valid);
    s = syntiant_ndp10x_read(ndp, 1, config_address, &v);
    if (s) goto error;
    
    valid = v;

    if (valid & (uint32_t)(~SYNTIANT_CONFIG_VALID_MASK)) {
        valid = 0;
    }

    labels_len = config->labels_len;
    firmware_version_len = config->firmware_version_len;
    parameters_version_len = config->parameters_version_len;
    pkg_version_len = config->pkg_version_len;
    if (classes) {
        *classes = 0;
    }
    config->labels_len = 0;
    config->firmware_version_len = 0;
    config->parameters_version_len = 0;
    config->pkg_version_len = 0;
    
    if (labels_len % 4 != 0) {
        s = SYNTIANT_NDP_ERROR_ARG;
        goto error;
    }
    if (firmware_version_len % 4 != 0) {
        s = SYNTIANT_NDP_ERROR_ARG;
        goto error;
    }
    if (parameters_version_len % 4 != 0) {
        s = SYNTIANT_NDP_ERROR_ARG;
        goto error;
    }
    if (pkg_version_len % 4 != 0) {
        s = SYNTIANT_NDP_ERROR_ARG;
        goto error;
    }
    
    if (valid & SYNTIANT_CONFIG_LABELS_VALID) {
        config_address = NDP10X_ILIB_SCRATCH_ORIGIN + 
            offsetof(struct syntiant_ndp10x_config_layout, label_size);
        s = syntiant_ndp10x_read(ndp, 1, config_address, &v);
        if (s) goto error;

        length = v;
        if (length < NDP10X_ILIB_SCRATCH_LENGTH) {
            config_address = NDP10X_ILIB_SCRATCH_ORIGIN +
                offsetof(struct syntiant_ndp10x_config_layout, labels);
            if (s) goto error;
            buffer = config->labels;
            len =  min_(labels_len, length);
            if (buffer && 0 < len) {
                s = syntiant_ndp10x_read_block
                    (ndp, 1, config_address, buffer, len);
                if (s) goto error;
            }
            config->labels_len = length;
            /*set classes*/
            if (classes) {
                for (chunk_num = 0; chunk_num < length / 4; chunk_num++) {
                    s = syntiant_ndp10x_read_block(ndp, 1, config_address,
                                                   buff, 4);
                    if (s) goto error;
                    for (buffer_index = 0; buffer_index < 4;
                         buffer_index++) {
                        if (*(buff + buffer_index) == '\0') {
                            num_classes++;
                            if ((buffer_index < 3)
                                && (*(buff + buffer_index + 1) == '\0'))
                                break;
                        }
                    }
                    config_address += 4;
                }
                *classes = num_classes;
            }
        }   
    }

    /*fw version*/
    config_address = NDP10X_ILIB_SCRATCH_ORIGIN +
            offsetof(struct syntiant_ndp10x_config_layout, fw_version_size);
    if (valid & SYNTIANT_CONFIG_FW_VERSION_VALID) {
        s = syntiant_ndp10x_read(ndp, 1, config_address, &length);
        if (s) goto error;
        if (length < NDP10X_ILIB_SCRATCH_LENGTH) {
            config_address = NDP10X_ILIB_SCRATCH_ORIGIN +
                offsetof(struct syntiant_ndp10x_config_layout, fw_version);
            buffer = config->firmware_version;
            len = min_(firmware_version_len, length);
            if (buffer && 0 < len) {
                s = syntiant_ndp10x_read_block
                    (ndp, 1, config_address, buffer, len);
                if (s) goto error;
            }
            config->firmware_version_len = length;
        }
    }

    /*nn version*/
    config_address = NDP10X_ILIB_SCRATCH_ORIGIN + 
        offsetof(struct syntiant_ndp10x_config_layout, params_version_size);
    if (valid & SYNTIANT_CONFIG_NN_VERSION_VALID) {
        s = syntiant_ndp10x_read(ndp, 1, config_address, &length);
        if (s) goto error;
        if (length < NDP10X_ILIB_SCRATCH_LENGTH) {
            buffer = config->parameters_version;
            config_address = NDP10X_ILIB_SCRATCH_ORIGIN +
                offsetof(struct syntiant_ndp10x_config_layout, params_version);
            len = min_(parameters_version_len, length);
            if (buffer && 0 < len) {
                s = syntiant_ndp10x_read_block
                    (ndp, 1, config_address, buffer, len);
                if (s) goto error;
            }
            config->parameters_version_len = length;
        }
    }

    /*pkg version*/
    config_address = NDP10X_ILIB_SCRATCH_ORIGIN + 
        offsetof(struct syntiant_ndp10x_config_layout, pkg_version_size);
    if (valid & SYNTIANT_CONFIG_PKG_VERSION_VALID) {
        s = syntiant_ndp10x_read(ndp, 1, config_address, &length);
        if (s) goto error;
        if (length < NDP10X_ILIB_SCRATCH_LENGTH) {
            buffer = config->pkg_version;
            config_address = NDP10X_ILIB_SCRATCH_ORIGIN +
                offsetof(struct syntiant_ndp10x_config_layout, pkg_version);
            len = min_(pkg_version_len, length);
            if (buffer && 0 < len) {
                s = syntiant_ndp10x_read_block
                    (ndp, 1, config_address, buffer, len);
                if (s) goto error;
            }
            config->pkg_version_len = length;
        }
    }

error:
    return s;
}

int
syntiant_ndp10x_get_config(struct syntiant_ndp_device_s *ndp,
    struct syntiant_ndp_config_s *config)
{
    struct syntiant_ndp10x_device_s *ndp10x = &ndp->d.ndp10x;
    int s;

    config->device_type = syntiant_ndp10x_get_config_devtype(ndp->device_type);
    config->classes = ndp10x->classes;
    if (!config->labels_len && !config->firmware_version_len
        && !config->parameters_version_len
        && !config->pkg_version_len) {

        config->labels_len = ndp10x->labels_len;
        config->firmware_version_len = ndp10x->fwver_len;
        config->parameters_version_len = ndp10x->paramver_len;
        config->pkg_version_len = ndp10x->pkgver_len;
        s = SYNTIANT_NDP_ERROR_NONE;
        goto error;
    }

    s = syntiant_ndp10x_access_config(ndp, config, NULL);
error:
    return s;
}

int
syntiant_ndp10x_send_data(struct syntiant_ndp_device_s *ndp, uint8_t *data,
                          unsigned int len, int type, uint32_t offset)
{
    int s;

    /* boundary check for DNN static feature */
    if ((type == SYNTIANT_NDP_SEND_DATA_TYPE_FEATURE_STATIC) &&
        (((offset + (uint32_t) len) > NDP10X_DNNSTATICFEATURE_SIZE )
         || (len & 0x3))) {
        s = SYNTIANT_NDP_ERROR_ARG;
        goto error;
    }
    else if ((type == SYNTIANT_NDP_SEND_DATA_TYPE_STREAMING) && offset) {
        s = SYNTIANT_NDP_ERROR_ARG;
        goto error;
    }

    if (type == SYNTIANT_NDP_SEND_DATA_TYPE_FEATURE_STATIC) {
        s = syntiant_ndp10x_write_block
            (ndp, 1, NDP10X_DNNSTATICFEATURE + offset, data, len);

    } else {
        s = syntiant_ndp10x_write_block(ndp, 0, NDP10X_SPI_SAMPLE, data, len);
    }

error:
    return s;
}

int
syntiant_ndp10x_extract_data(struct syntiant_ndp_device_s *ndp, int type,
                             int from, uint8_t *data, unsigned int *lenp)
{
    struct syntiant_ndp10x_device_s *ndp10x = &ndp->d.ndp10x;
    uint32_t addr = ndp10x->fw_state_addr;
    unsigned int len = (unsigned int) *lenp;
    unsigned int rlen;
    uint32_t base, currptr, rcurrptr, bufsize, used, offset, tanksts1;
    uint32_t start = 0;
    int s = SYNTIANT_NDP_ERROR_NONE;

    if (type < 0
        || SYNTIANT_NDP_EXTRACT_TYPE_LAST < type
        || from < 0
        || SYNTIANT_NDP_EXTRACT_FROM_LAST < from
        || len % 4) {
        s = SYNTIANT_NDP_ERROR_ARG;
        goto error;
    }

    if (type == SYNTIANT_NDP_EXTRACT_TYPE_INPUT) {
        base = ndp10x->tank_address;
        bufsize = ndp10x->tank_size;

        s = syntiant_ndp10x_read(ndp, 1, NDP10X_DSP_CONFIG_TANKSTS1, &tanksts1);
        if (s) {
            goto error;
        }
        if (addr) {
            s = syntiant_ndp10x_read
                (ndp, 1, addr
                 + (uint32_t) offsetof(struct ndp10x_fw_state_s, tankptr),
                 &currptr);
            if (s) {
                goto error;
            }
        } else {
            currptr = NDP10X_DSP_CONFIG_TANKSTS1_CURRPTR_EXTRACT(tanksts1);
        }
        
        rcurrptr = currptr & ~0x3U;
        currptr = (currptr + 3) & ~0x3U;
        
        switch (from) {
        case SYNTIANT_NDP_EXTRACT_FROM_MATCH:
            start = ndp10x->tankptr_match;
            if (start + (start < currptr ? bufsize : 0) < currptr + len) {
                s = SYNTIANT_NDP_ERROR_ARG;
                goto error;
            }
            start = start + bufsize - len;
            break;
        case SYNTIANT_NDP_EXTRACT_FROM_UNREAD:
            start = ndp10x->tankptr_last;
            break;
        case SYNTIANT_NDP_EXTRACT_FROM_OLDEST:
            start = currptr;
            break;
        case SYNTIANT_NDP_EXTRACT_FROM_NEWEST:
            if (bufsize - (currptr - rcurrptr) < len) {
                s = SYNTIANT_NDP_ERROR_ARG;
                goto error;
            }
            start = rcurrptr + bufsize - len;
        }

        if (bufsize <= start) {
            start -= bufsize;
        }
            
        used = bufsize ? rcurrptr + bufsize - start : 0;
        if (bufsize < used
            || (from == SYNTIANT_NDP_EXTRACT_FROM_UNREAD && bufsize <= used)) {
            used -= bufsize;
        }
        
        if (!data &&
            (from == SYNTIANT_NDP_EXTRACT_FROM_MATCH
             || from == SYNTIANT_NDP_EXTRACT_FROM_NEWEST)) {
            len = 0;
        }
    } else if (type == SYNTIANT_NDP_EXTRACT_TYPE_FEATURES) {
        base = NDP10X_DSP_CONFIG_FILTOUT(0);
        bufsize = NDP10X_DSP_CONFIG_FILTOUT_COUNT * 4;
        start = 0;
        used = bufsize;
    } else if (type == SYNTIANT_NDP_EXTRACT_TYPE_FEATURE_STATIC) {
        base = NDP10X_DNNSTATICFEATURE;
        bufsize = NDP10X_DNNSTATICFEATURE_SIZE;
        start = 0;
        used = bufsize;
    } else {
        s = SYNTIANT_NDP_ERROR_ARG;
        goto error;
    }
    
    if (used < len) {
        len = used;
    }

    if (len && data) {
        offset = 0;
        if (bufsize < start + len) {
            rlen = bufsize - start;
            s = syntiant_ndp10x_read_block(ndp, 1, base + start, data, rlen);
            if (s) {
                goto error;
            }
            len -= rlen;
            offset = rlen;
            start = 0;
        }

        s = syntiant_ndp10x_read_block(ndp, 1, base + start, data + offset, len);
        if (s) {
            goto error;
        }
    }

    if (type == SYNTIANT_NDP_EXTRACT_TYPE_INPUT) {
        start = start + len;
        ndp10x->tankptr_last = start < bufsize ? start : start - bufsize;
    }
    
    *lenp = used;

 error:
    return s;
}

int
syntiant_ndp10x_get_match_summary(struct syntiant_ndp_device_s *ndp,
                                  uint32_t *summary)
{
    int s = SYNTIANT_NDP_ERROR_NONE;
    struct syntiant_ndp10x_device_s *ndp10x = &ndp->d.ndp10x;
    struct ndp10x_fw_match_s match;
    uint32_t addr = ndp10x->fw_state_addr;
    uint32_t cons;
    
    *summary = 0;
    if (addr) {
        cons = ndp10x->match_consumer;
        if (ndp10x->match_producer != cons) {
            addr += (uint32_t) offsetof(struct ndp10x_fw_state_s,
                                        match_ring);
            addr += cons * (uint32_t) sizeof(struct ndp10x_fw_match_s);
            s = syntiant_ndp10x_read_block(ndp, 1, addr, &match, sizeof(match));
            if (s) {
                goto out;
            }
            ndp10x->tankptr_match = match.tankptr & ~0x3U;
            *summary = match.summary;
            cons++;
            cons = cons == ndp10x->match_ring_size ? 0 : cons;
            ndp10x->match_consumer = cons;
        }
    } else {
        s = syntiant_ndp10x_read(ndp, 0, NDP10X_SPI_MATCH, summary);
    }

 out:
    return s;
}

int
syntiant_ndp10x_get_match_binary(
    struct syntiant_ndp_device_s *ndp, uint8_t *matches, unsigned int len)
{
    int s;
    uint32_t addr = ndp->d.ndp10x.fw_state_addr;

    if (8 < len || len % 4 != 0) {
        s = SYNTIANT_NDP_ERROR_ARG;
        goto error;
    }

    if (addr) {
        addr += (uint32_t) offsetof(struct ndp10x_fw_state_s,
                                    result.winner_one_hot);
        s = syntiant_ndp10x_read_block(ndp, 1, addr, matches, len);
    } else {
        s = syntiant_ndp10x_read_block(
            ndp, 1, NDP10X_DNN_CONFIG_DNNSTS1, matches, len);
    }

error:
    return s;
}

int
syntiant_ndp10x_get_match_strength(
    struct syntiant_ndp_device_s *ndp, uint8_t *strengths, unsigned int len,
    int type)
{
    int s;
    uint32_t addr = ndp->d.ndp10x.fw_state_addr;

    if (type == SYNTIANT_NDP_STRENGTH_RAW) {
        if (64 < len || len % 4 != 0) {
            s = SYNTIANT_NDP_ERROR_ARG;
            goto error;
        }
    } else if (type == SYNTIANT_NDP_STRENGTH_SOFTMAX) {
        if (64 * (int) sizeof(uint32_t) < len || len % 4 != 0) {
            s = SYNTIANT_NDP_ERROR_ARG;
            goto error;
        }
    } else {
        s = SYNTIANT_NDP_ERROR_ARG;
        goto error;
    }

    if (type == SYNTIANT_NDP_STRENGTH_RAW) {

        if (addr) {
            addr += (uint32_t)
                offsetof(struct ndp10x_fw_state_s, result.raw_strengths);
            s = syntiant_ndp10x_read_block(ndp, 1, addr, strengths, len);
        } else {
            s = syntiant_ndp10x_read_block(
                ndp, 1, NDP10X_DNN_CONFIG_DNNACT(0), strengths, len);
        }

    } else {
        if (addr) {
            addr += (uint32_t)
                offsetof(struct ndp10x_fw_state_s, result.softmax_strengths);
            s = syntiant_ndp10x_read_block(ndp, 1, addr, strengths, len);
        } else {
            s = SYNTIANT_NDP_ERROR_FAIL;
        }
    }

error:
    return s;
}

void
syntiant_ndp10x_config_get_dnn(struct syntiant_ndp10x_config_state_s *st,
                               struct syntiant_ndp10x_config_s *config)
{
    unsigned int smplmode, filtaudiochsel, dnnmode, packed;
    uint32_t smplctl = st->smplctl;
    uint32_t i2sctl = st->i2sctl;
    unsigned int dnn_input = SYNTIANT_NDP10X_CONFIG_DNN_INPUT_NONE;

    if (!NDP10X_DNN_CONFIG_DNNCTL0_ENABLE_EXTRACT(st->dnnctl0)) {
        dnn_input = SYNTIANT_NDP10X_CONFIG_DNN_INPUT_NONE;
        goto out;
    }

    smplmode = NDP10X_DSP_CONFIG_SMPLCTL_SMPLMODE_EXTRACT(smplctl);
    filtaudiochsel = NDP10X_DSP_CONFIG_SMPLCTL_FILTAUDIOCHSEL_EXTRACT(smplctl);
    dnnmode = NDP10X_DSP_CONFIG_SMPLCTL_DNNMODE_EXTRACT(smplctl);
    packed = NDP10X_DSP_CONFIG_I2SCTL_PACKED_EXTRACT(i2sctl);

    switch (smplmode) {
    case NDP10X_DSP_CONFIG_SMPLCTL_SMPLMODE_PDM:
        switch (filtaudiochsel) {
        case NDP10X_DSP_CONFIG_SMPLCTL_FILTAUDIOCHSEL_NEGEDGE:
            dnn_input = SYNTIANT_NDP10X_CONFIG_DNN_INPUT_PDM0;
            break;
        case NDP10X_DSP_CONFIG_SMPLCTL_FILTAUDIOCHSEL_POSEDGE:
            dnn_input = SYNTIANT_NDP10X_CONFIG_DNN_INPUT_PDM1;
            break;
        case NDP10X_DSP_CONFIG_SMPLCTL_FILTAUDIOCHSEL_SUM:
            dnn_input = SYNTIANT_NDP10X_CONFIG_DNN_INPUT_PDM_SUM;
            break;
        }
        break;
    case NDP10X_DSP_CONFIG_SMPLCTL_SMPLMODE_I2S:
        switch (dnnmode) {
        case NDP10X_DSP_CONFIG_SMPLCTL_DNNMODE_I2S:
            dnn_input = SYNTIANT_NDP10X_CONFIG_DNN_INPUT_I2S_DIRECT;
            break;
        case NDP10X_DSP_CONFIG_SMPLCTL_DNNMODE_FREQ:
            switch (filtaudiochsel) {
            case NDP10X_DSP_CONFIG_SMPLCTL_FILTAUDIOCHSEL_NEGEDGE:
                dnn_input = packed
                    ? SYNTIANT_NDP10X_CONFIG_DNN_INPUT_I2S_MONO
                    : SYNTIANT_NDP10X_CONFIG_DNN_INPUT_I2S_LEFT;
                break;
            case NDP10X_DSP_CONFIG_SMPLCTL_FILTAUDIOCHSEL_POSEDGE:
                dnn_input = SYNTIANT_NDP10X_CONFIG_DNN_INPUT_I2S_RIGHT;
                break;
            case NDP10X_DSP_CONFIG_SMPLCTL_FILTAUDIOCHSEL_SUM:
                dnn_input = SYNTIANT_NDP10X_CONFIG_DNN_INPUT_I2S_SUM;
                break;
            }
            break;
        }
        break;
    case NDP10X_DSP_CONFIG_SMPLCTL_SMPLMODE_SPI:
        switch (dnnmode) {
        case NDP10X_DSP_CONFIG_SMPLCTL_DNNMODE_SPI:
            dnn_input = SYNTIANT_NDP10X_CONFIG_DNN_INPUT_SPI_DIRECT;
            break;
        case NDP10X_DSP_CONFIG_SMPLCTL_DNNMODE_FREQ:
            dnn_input = SYNTIANT_NDP10X_CONFIG_DNN_INPUT_SPI;
            break;
        }
        break;
    }

 out:
    st->dnn_input = dnn_input;
    if (config) {
        config->dnn_frame_size =
            NDP10X_DNN_CONFIG_DNNCTL7_NUMFRAMEINPUTS_EXTRACT(st->dnnctl7);
        config->dnn_signed =
            NDP10X_DNN_CONFIG_DNNCTL0_SIGNEDINPUT_EXTRACT(st->dnnctl0);
        config->dnn_minimum_threshold =
            NDP10X_DNN_CONFIG_DNNCTL3_MINTHRESH_EXTRACT(st->dnnctl3);
        config->dnn_run_threshold =
            NDP10X_DNN_CONFIG_DNNCTL3_RUNTHRESH_EXTRACT(st->dnnctl3);
        config->dnn_inputs =
            NDP10X_DNN_CONFIG_DNNCTL1_NUMINPUTS_EXTRACT(st->dnnctl1);
        config->dnn_static_inputs =
            NDP10X_DNN_CONFIG_DNNCTL7_NUMSTATICINPUTS_EXTRACT(st->dnnctl7);
        config->dnn_outputs =
            NDP10X_DNN_CONFIG_DNNCTL7_NUMOUTPUTS_EXTRACT(st->dnnctl7);
    }
}



int
syntiant_ndp10x_config_set_dnn(struct syntiant_ndp10x_config_s *config,
                               struct syntiant_ndp10x_config_state_s *st)
{
    uint32_t v;
    uint32_t dnnctl0 = st->dnnctl0;
    uint32_t dnnctl1 = st->dnnctl1;
    uint32_t dnnctl3 = st->dnnctl3;
    uint32_t dnnctl7 = st->dnnctl7;
    uint32_t smplctl = st->smplctl;
    unsigned int dnn_input = config->dnn_input;
    unsigned int enable;
    int s = SYNTIANT_NDP_ERROR_NONE;

    if (config->set & SYNTIANT_NDP10X_CONFIG_SET_FREQ_FRAME_SIZE) {
        dnnctl0 = NDP10X_DNN_CONFIG_DNNCTL0_NUMFILT_MASK_INSERT
            (dnnctl0, config->freq_frame_size);
    }

    if (config->set & SYNTIANT_NDP10X_CONFIG_SET_DNN_SIGNED) {
        dnnctl0 = NDP10X_DNN_CONFIG_DNNCTL0_SIGNEDINPUT_MASK_INSERT
            (dnnctl0, (unsigned int) !!config->dnn_signed);
    }

    if (config->set & SYNTIANT_NDP10X_CONFIG_SET_DNN_MINIMUM_THRESHOLD) {
        if ((NDP10X_DNN_CONFIG_DNNCTL3_MINTHRESH_MASK
             >> NDP10X_DNN_CONFIG_DNNCTL3_MINTHRESH_SHIFT)
            < config->dnn_minimum_threshold) {
            s = SYNTIANT_NDP_ERROR_ARG;
            goto out;
        }
        dnnctl3 = NDP10X_DNN_CONFIG_DNNCTL3_MINTHRESH_MASK_INSERT
            (dnnctl3, config->dnn_minimum_threshold);
    }

    if (config->set & SYNTIANT_NDP10X_CONFIG_SET_DNN_RUN_THRESHOLD) {
        if ((NDP10X_DNN_CONFIG_DNNCTL3_RUNTHRESH_MASK
             >> NDP10X_DNN_CONFIG_DNNCTL3_RUNTHRESH_SHIFT)
            < config->dnn_run_threshold) {
            s = SYNTIANT_NDP_ERROR_ARG;
            goto out;
        }
        dnnctl3 = NDP10X_DNN_CONFIG_DNNCTL3_RUNTHRESH_MASK_INSERT
            (dnnctl3, config->dnn_run_threshold);
    }

    if (config->set & SYNTIANT_NDP10X_CONFIG_SET_DNN_FRAME_SIZE) {
        if ((NDP10X_DNN_CONFIG_DNNCTL7_NUMFRAMEINPUTS_MASK
             >> NDP10X_DNN_CONFIG_DNNCTL7_NUMFRAMEINPUTS_SHIFT)
            < config->dnn_frame_size) {
            s = SYNTIANT_NDP_ERROR_ARG;
            goto out;
        }
        dnnctl7 = NDP10X_DNN_CONFIG_DNNCTL7_NUMFRAMEINPUTS_MASK_INSERT
            (dnnctl7, config->dnn_frame_size);
    }

    if (config->set & SYNTIANT_NDP10X_CONFIG_SET_DNN_INPUTS) {
        if ((NDP10X_DNN_CONFIG_DNNCTL1_NUMINPUTS_MASK
             >> NDP10X_DNN_CONFIG_DNNCTL1_NUMINPUTS_SHIFT)
            < config->dnn_inputs) {
            s = SYNTIANT_NDP_ERROR_ARG;
            goto out;
        }
        dnnctl1 = NDP10X_DNN_CONFIG_DNNCTL1_NUMINPUTS_MASK_INSERT
            (dnnctl1, config->dnn_inputs);
    }

    if (config->set & SYNTIANT_NDP10X_CONFIG_SET_DNN_STATIC_INPUTS) {
        if ((NDP10X_DNN_CONFIG_DNNCTL7_NUMSTATICINPUTS_MASK
             >> NDP10X_DNN_CONFIG_DNNCTL7_NUMSTATICINPUTS_SHIFT)
            < config->dnn_static_inputs) {
            s = SYNTIANT_NDP_ERROR_ARG;
            goto out;
        }
        dnnctl7 = NDP10X_DNN_CONFIG_DNNCTL7_NUMSTATICINPUTS_MASK_INSERT
            (dnnctl7, config->dnn_static_inputs);
    }

    if (config->set & SYNTIANT_NDP10X_CONFIG_SET_DNN_OUTPUTS) {
        if ((NDP10X_DNN_CONFIG_DNNCTL7_NUMOUTPUTS_MASK
             >> NDP10X_DNN_CONFIG_DNNCTL7_NUMOUTPUTS_SHIFT)
            < config->dnn_outputs) {
            s = SYNTIANT_NDP_ERROR_ARG;
            goto out;
        }
        dnnctl7 = NDP10X_DNN_CONFIG_DNNCTL7_NUMOUTPUTS_MASK_INSERT
            (dnnctl7, config->dnn_outputs);
    }

    if (!(config->set & SYNTIANT_NDP10X_CONFIG_SET_DNN_INPUT)) {
        st->dnnctl0 = dnnctl0;
        st->dnnctl1 = dnnctl1;
        st->dnnctl3 = dnnctl3;
        st->dnnctl7 = dnnctl7;
        syntiant_ndp10x_config_get_dnn(st, NULL);
        goto out;
    }

    if (SYNTIANT_NDP10X_CONFIG_DNN_INPUT_MAX < dnn_input) {
        s = SYNTIANT_NDP_ERROR_ARG;
        goto out;
    }

    switch (dnn_input) {
    case SYNTIANT_NDP10X_CONFIG_DNN_INPUT_PDM0:
    case SYNTIANT_NDP10X_CONFIG_DNN_INPUT_PDM1:
    case SYNTIANT_NDP10X_CONFIG_DNN_INPUT_PDM_SUM:
        v = NDP10X_DSP_CONFIG_SMPLCTL_SMPLMODE_PDM;
        break;
    case SYNTIANT_NDP10X_CONFIG_DNN_INPUT_I2S_LEFT:
    case SYNTIANT_NDP10X_CONFIG_DNN_INPUT_I2S_RIGHT:
    case SYNTIANT_NDP10X_CONFIG_DNN_INPUT_I2S_SUM:
        v = NDP10X_DSP_CONFIG_SMPLCTL_SMPLMODE_I2S;
        break;
    case SYNTIANT_NDP10X_CONFIG_DNN_INPUT_I2S_MONO:
    case SYNTIANT_NDP10X_CONFIG_DNN_INPUT_I2S_DIRECT:
        v = NDP10X_DSP_CONFIG_SMPLCTL_SMPLMODE_I2S;
        break;
    case SYNTIANT_NDP10X_CONFIG_DNN_INPUT_SPI:
    case SYNTIANT_NDP10X_CONFIG_DNN_INPUT_SPI_DIRECT:
    case SYNTIANT_NDP10X_CONFIG_DNN_INPUT_NONE:
        v = NDP10X_DSP_CONFIG_SMPLCTL_SMPLMODE_SPI;
        break;
    default:
        /* missing case bug -- arg was already range checked */
        s = SYNTIANT_NDP_ERROR_ARG;
        goto out;
    }
    smplctl =
        NDP10X_DSP_CONFIG_SMPLCTL_SMPLMODE_MASK_INSERT(smplctl, v);

    if (dnn_input != SYNTIANT_NDP10X_CONFIG_DNN_INPUT_I2S_DIRECT
        && dnn_input != SYNTIANT_NDP10X_CONFIG_DNN_INPUT_SPI
        && dnn_input != SYNTIANT_NDP10X_CONFIG_DNN_INPUT_SPI_DIRECT
        && dnn_input != SYNTIANT_NDP10X_CONFIG_DNN_INPUT_NONE) {
        switch (dnn_input) {
        case SYNTIANT_NDP10X_CONFIG_DNN_INPUT_PDM0:
            v = NDP10X_DSP_CONFIG_SMPLCTL_FILTAUDIOCHSEL_NEGEDGE;
            break;
        case SYNTIANT_NDP10X_CONFIG_DNN_INPUT_PDM1:
            v = NDP10X_DSP_CONFIG_SMPLCTL_FILTAUDIOCHSEL_POSEDGE;
            break;
        case SYNTIANT_NDP10X_CONFIG_DNN_INPUT_PDM_SUM:
            v = NDP10X_DSP_CONFIG_SMPLCTL_FILTAUDIOCHSEL_SUM;
            break;
        case SYNTIANT_NDP10X_CONFIG_DNN_INPUT_I2S_LEFT:
        case SYNTIANT_NDP10X_CONFIG_DNN_INPUT_I2S_MONO:
            v = NDP10X_DSP_CONFIG_SMPLCTL_FILTAUDIOCHSEL_NEGEDGE;
            break;
        case SYNTIANT_NDP10X_CONFIG_DNN_INPUT_I2S_RIGHT:
            v = NDP10X_DSP_CONFIG_SMPLCTL_FILTAUDIOCHSEL_POSEDGE;
            break;
        case SYNTIANT_NDP10X_CONFIG_DNN_INPUT_I2S_SUM:
            v = NDP10X_DSP_CONFIG_SMPLCTL_FILTAUDIOCHSEL_SUM;
            break;
        default:
            /* missing case bug -- arg was already range checked */
            s = SYNTIANT_NDP_ERROR_ARG;
            goto out;
        }
        smplctl =
            NDP10X_DSP_CONFIG_SMPLCTL_FILTAUDIOCHSEL_MASK_INSERT(smplctl, v);
    }

    switch (dnn_input) {
    case SYNTIANT_NDP10X_CONFIG_DNN_INPUT_I2S_DIRECT:
        v = NDP10X_DSP_CONFIG_SMPLCTL_DNNMODE_I2S;
        break;
    case SYNTIANT_NDP10X_CONFIG_DNN_INPUT_SPI_DIRECT:
    case SYNTIANT_NDP10X_CONFIG_DNN_INPUT_NONE:
        v = NDP10X_DSP_CONFIG_SMPLCTL_DNNMODE_SPI;
        break;
    default:
        v = NDP10X_DSP_CONFIG_SMPLCTL_DNNMODE_FREQ;
    }
    smplctl = NDP10X_DSP_CONFIG_SMPLCTL_DNNMODE_MASK_INSERT(smplctl, v);

    enable = dnn_input != SYNTIANT_NDP10X_CONFIG_DNN_INPUT_NONE;
    dnnctl0 = NDP10X_DNN_CONFIG_DNNCTL0_ENABLE_MASK_INSERT(dnnctl0, enable);
    dnnctl0 = NDP10X_DNN_CONFIG_DNNCTL0_ENABLESAMPLES_MASK_INSERT
        (dnnctl0, enable);

    st->dnnctl0 = dnnctl0;
    st->dnnctl1 = dnnctl1;
    st->dnnctl3 = dnnctl3;
    st->dnnctl7 = dnnctl7;
    st->smplctl = smplctl;
    st->dnn_input = dnn_input;

 out:
    return s;
}

void
syntiant_ndp10x_config_get_tank(struct syntiant_ndp10x_config_state_s *st,
                                struct syntiant_ndp10x_config_s *config)
{
    uint32_t input;
    uint32_t smplmode;
    unsigned int pdm, packed;
    uint32_t smplctl = st->smplctl;
    uint32_t smplmark = st->smplmark;
    uint32_t tank = st->tank;
    uint32_t i2sctl = st->i2sctl;
    uint32_t tank_input = SYNTIANT_NDP10X_CONFIG_TANK_INPUT_NONE;

    input = NDP10X_DSP_CONFIG_SMPLCTL_TANKAUDIOCHSEL_EXTRACT(smplctl);
    smplmode = NDP10X_DSP_CONFIG_SMPLCTL_SMPLMODE_EXTRACT(smplctl);
    pdm = (smplmode == NDP10X_DSP_CONFIG_SMPLCTL_SMPLMODE_PDM);
    packed = NDP10X_DSP_CONFIG_I2SCTL_PACKED_EXTRACT(i2sctl);

    if (!NDP10X_DSP_CONFIG_TANK_ENABLE_EXTRACT(tank)) {
        tank_input = SYNTIANT_NDP10X_CONFIG_TANK_INPUT_NONE;
    } else if (NDP10X_DSP_CONFIG_TANK_INPUTSEL_EXTRACT(tank)) {
        tank_input = SYNTIANT_NDP10X_CONFIG_TANK_INPUT_FILTER_BANK;
    } else if (smplmode == NDP10X_DSP_CONFIG_SMPLCTL_SMPLMODE_SPI) {
        tank_input = SYNTIANT_NDP10X_CONFIG_TANK_INPUT_SPI;
    } else {
        switch (input) {
        case NDP10X_DSP_CONFIG_SMPLCTL_TANKAUDIOCHSEL_NEGEDGE:
            tank_input = pdm
                ? SYNTIANT_NDP10X_CONFIG_TANK_INPUT_PDM0
                : (packed
                   ? SYNTIANT_NDP10X_CONFIG_TANK_INPUT_I2S_MONO
                   : SYNTIANT_NDP10X_CONFIG_TANK_INPUT_I2S_LEFT);
            break;
        case NDP10X_DSP_CONFIG_SMPLCTL_TANKAUDIOCHSEL_POSEDGE:
            tank_input = pdm
                ? SYNTIANT_NDP10X_CONFIG_TANK_INPUT_PDM1
                : SYNTIANT_NDP10X_CONFIG_TANK_INPUT_I2S_RIGHT;
            break;
        case NDP10X_DSP_CONFIG_SMPLCTL_TANKAUDIOCHSEL_SUM:
            tank_input = pdm
                ? SYNTIANT_NDP10X_CONFIG_TANK_INPUT_PDM_SUM
                : SYNTIANT_NDP10X_CONFIG_TANK_INPUT_I2S_SUM;
            break;
        case NDP10X_DSP_CONFIG_SMPLCTL_TANKAUDIOCHSEL_BOTH:
            tank_input = pdm
                ? SYNTIANT_NDP10X_CONFIG_TANK_INPUT_PDM_BOTH
                : SYNTIANT_NDP10X_CONFIG_TANK_INPUT_I2S_BOTH;
            break;
        case NDP10X_DSP_CONFIG_SMPLCTL_TANKAUDIOCHSEL_NEGPDMIN:
            tank_input = SYNTIANT_NDP10X_CONFIG_TANK_INPUT_PDM0_RAW;
            break;
        case NDP10X_DSP_CONFIG_SMPLCTL_TANKAUDIOCHSEL_POSPDMIN:
            tank_input = SYNTIANT_NDP10X_CONFIG_TANK_INPUT_PDM1_RAW;
            break;
        case NDP10X_DSP_CONFIG_SMPLCTL_TANKAUDIOCHSEL_BOTHPDMIN:
            tank_input = SYNTIANT_NDP10X_CONFIG_TANK_INPUT_PDM_BOTH_RAW;
            break;
        }
    }

    st->tank_input = tank_input;
    if (config) {
        config->audio_frame_size =
            (NDP10X_DSP_CONFIG_FREQCTL_FRAMESTEP_MASK
             >> NDP10X_DSP_CONFIG_FREQCTL_FRAMESTEP_SHIFT) + 1;
        config->audio_buffer_used =
            NDP10X_DSP_CONFIG_SMPLSTS_NUM_SAMPLES_EXTRACT(st->smplsts);
        if (!config->audio_buffer_used
            && (tank_input == SYNTIANT_NDP10X_CONFIG_TANK_INPUT_SPI
                || st->dnn_input == SYNTIANT_NDP10X_CONFIG_DNN_INPUT_SPI)) {
            config->audio_buffer_used =
               config->audio_frame_size
                - NDP10X_DSP_CONFIG_FREQCTL_FRAMESTEP_EXTRACT(st->freqctl);
        }
        config->water_mark_on =
            NDP10X_DSP_CONFIG_SMPLMARK_ENABLE_EXTRACT(smplmark);
        config->tank_bits =
            NDP10X_DSP_CONFIG_TANK_FORMATSEL_EXTRACT(tank) ? 8 : 16;
        config->tank_size = NDP10X_DSP_CONFIG_TANK_SIZE_EXTRACT(tank);
        config->tank_max_size = st->tank_max_size;
    }
}

int
syntiant_ndp10x_config_check_tank(unsigned int dnn_input,
                                  unsigned int tank_input)
{
    int s = SYNTIANT_NDP_ERROR_NONE;

    /* TODO: pick default tank when dnn_input is set and tank_input is not */

    if (SYNTIANT_NDP10X_CONFIG_TANK_INPUT_MAX < tank_input) {
        s = SYNTIANT_NDP_ERROR_ARG;
        goto out;
    }

    switch (tank_input) {
    case SYNTIANT_NDP10X_CONFIG_TANK_INPUT_NONE:
        break;
    case SYNTIANT_NDP10X_CONFIG_TANK_INPUT_PDM0:
    case SYNTIANT_NDP10X_CONFIG_TANK_INPUT_PDM1:
    case SYNTIANT_NDP10X_CONFIG_TANK_INPUT_PDM_SUM:
    case SYNTIANT_NDP10X_CONFIG_TANK_INPUT_PDM_BOTH:
    case SYNTIANT_NDP10X_CONFIG_TANK_INPUT_PDM0_RAW:
    case SYNTIANT_NDP10X_CONFIG_TANK_INPUT_PDM1_RAW:
    case SYNTIANT_NDP10X_CONFIG_TANK_INPUT_PDM_BOTH_RAW:
        switch (dnn_input) {
        case SYNTIANT_NDP10X_CONFIG_DNN_INPUT_NONE:
        case SYNTIANT_NDP10X_CONFIG_DNN_INPUT_PDM0:
        case SYNTIANT_NDP10X_CONFIG_DNN_INPUT_PDM1:
        case SYNTIANT_NDP10X_CONFIG_DNN_INPUT_PDM_SUM:
            break;
        default:
            s = SYNTIANT_NDP_ERROR_ARG;
            goto out;
        }
        break;
    case SYNTIANT_NDP10X_CONFIG_TANK_INPUT_I2S_LEFT:
    case SYNTIANT_NDP10X_CONFIG_TANK_INPUT_I2S_RIGHT:
    case SYNTIANT_NDP10X_CONFIG_TANK_INPUT_I2S_SUM:
    case SYNTIANT_NDP10X_CONFIG_TANK_INPUT_I2S_BOTH:
        switch (dnn_input) {
        case SYNTIANT_NDP10X_CONFIG_DNN_INPUT_NONE:
        case SYNTIANT_NDP10X_CONFIG_DNN_INPUT_I2S_LEFT:
        case SYNTIANT_NDP10X_CONFIG_DNN_INPUT_I2S_RIGHT:
        case SYNTIANT_NDP10X_CONFIG_DNN_INPUT_I2S_SUM:
            break;
        default:
            s = SYNTIANT_NDP_ERROR_ARG;
            goto out;
        }
        break;
    case SYNTIANT_NDP10X_CONFIG_TANK_INPUT_I2S_MONO:
        switch (dnn_input) {
        case SYNTIANT_NDP10X_CONFIG_DNN_INPUT_NONE:
        case SYNTIANT_NDP10X_CONFIG_DNN_INPUT_I2S_MONO:
        case SYNTIANT_NDP10X_CONFIG_DNN_INPUT_I2S_DIRECT:
            break;
        default:
            s = SYNTIANT_NDP_ERROR_ARG;
            goto out;
        }
        break;
    case SYNTIANT_NDP10X_CONFIG_TANK_INPUT_SPI:
        switch (dnn_input) {
        case SYNTIANT_NDP10X_CONFIG_DNN_INPUT_NONE:
        case SYNTIANT_NDP10X_CONFIG_DNN_INPUT_SPI:
        case SYNTIANT_NDP10X_CONFIG_DNN_INPUT_SPI_DIRECT:
            break;
        default:
            s = SYNTIANT_NDP_ERROR_ARG;
            goto out;
        }
        break;
    case SYNTIANT_NDP10X_CONFIG_TANK_INPUT_FILTER_BANK:
        switch (dnn_input) {
        case SYNTIANT_NDP10X_CONFIG_DNN_INPUT_NONE:
        case SYNTIANT_NDP10X_CONFIG_DNN_INPUT_I2S_DIRECT:
        case SYNTIANT_NDP10X_CONFIG_DNN_INPUT_SPI_DIRECT:
            s = SYNTIANT_NDP_ERROR_ARG;
            goto out;
        }
        break;
    case SYNTIANT_NDP10X_CONFIG_TANK_INPUT_DNN:
        switch (dnn_input) {
        case SYNTIANT_NDP10X_CONFIG_DNN_INPUT_I2S_DIRECT:
        case SYNTIANT_NDP10X_CONFIG_DNN_INPUT_SPI_DIRECT:
            s = SYNTIANT_NDP_ERROR_ARG;
            goto out;
        }
        break;
    }

 out:
    return s;
}

int
syntiant_ndp10x_config_set_tank(struct syntiant_ndp10x_config_s *config,
                                struct syntiant_ndp10x_config_state_s *st)
{

    unsigned int enable;
    unsigned int tank_input = config->tank_input;
    uint32_t smplctl = st->smplctl;
    uint32_t smplmark = st->smplmark;
    uint32_t tank = st->tank;
    int s = SYNTIANT_NDP_ERROR_NONE;
    uint32_t v = 0;

    if (config->set & SYNTIANT_NDP10X_CONFIG_SET_TANK_BITS) {
        if (config->tank_bits != 8 && config->tank_bits != 16) {
            s = SYNTIANT_NDP_ERROR_ARG;
            goto out;
        }
        tank = NDP10X_DSP_CONFIG_TANK_FORMATSEL_MASK_INSERT
            (tank, (unsigned int) (config->tank_bits == 8));
    }

    if (config->set & SYNTIANT_NDP10X_CONFIG_SET_TANK_SIZE) {
        if (st->tank_max_size < config->tank_size) {
            s = SYNTIANT_NDP_ERROR_ARG;
            goto out;
        }
        tank = NDP10X_DSP_CONFIG_TANK_SIZE_MASK_INSERT(tank, config->tank_size);
    }

    if (config->set & SYNTIANT_NDP10X_CONFIG_SET_WATER_MARK_ON) {
        smplmark = NDP10X_DSP_CONFIG_SMPLMARK_HI_MARK_MASK_INSERT
            (smplmark, NDP10X_DSP_CONFIG_SMPLMARK_HI_MARK_MASK
             >> NDP10X_DSP_CONFIG_SMPLMARK_HI_MARK_SHIFT);
        smplmark = NDP10X_DSP_CONFIG_SMPLMARK_ENABLE_MASK_INSERT
            (smplmark, (unsigned int) !!config->water_mark_on);
    }

    if (!(config->set & SYNTIANT_NDP10X_CONFIG_SET_TANK_INPUT)) {
        st->tank = tank;
        st->smplmark = smplmark;
        syntiant_ndp10x_config_get_tank(st, NULL);
        goto out;
    }

    if (tank_input == SYNTIANT_NDP10X_CONFIG_TANK_INPUT_DNN) {
        switch (st->dnn_input) {
        case SYNTIANT_NDP10X_CONFIG_DNN_INPUT_NONE:
            tank_input = SYNTIANT_NDP10X_CONFIG_TANK_INPUT_NONE;
            break;
        case SYNTIANT_NDP10X_CONFIG_DNN_INPUT_PDM0:
            tank_input = SYNTIANT_NDP10X_CONFIG_TANK_INPUT_PDM0;
            break;
        case SYNTIANT_NDP10X_CONFIG_DNN_INPUT_PDM1:
            tank_input = SYNTIANT_NDP10X_CONFIG_TANK_INPUT_PDM1;
            break;
        case SYNTIANT_NDP10X_CONFIG_DNN_INPUT_PDM_SUM:
            tank_input = SYNTIANT_NDP10X_CONFIG_TANK_INPUT_PDM_SUM;
            break;
        case SYNTIANT_NDP10X_CONFIG_DNN_INPUT_I2S_LEFT:
            tank_input = SYNTIANT_NDP10X_CONFIG_TANK_INPUT_I2S_LEFT;
            break;
        case SYNTIANT_NDP10X_CONFIG_DNN_INPUT_I2S_RIGHT:
            tank_input = SYNTIANT_NDP10X_CONFIG_TANK_INPUT_I2S_RIGHT;
            break;
        case SYNTIANT_NDP10X_CONFIG_DNN_INPUT_I2S_SUM:
            tank_input = SYNTIANT_NDP10X_CONFIG_TANK_INPUT_I2S_SUM;
            break;
        case SYNTIANT_NDP10X_CONFIG_DNN_INPUT_I2S_MONO:
        case SYNTIANT_NDP10X_CONFIG_DNN_INPUT_I2S_DIRECT:
            tank_input = SYNTIANT_NDP10X_CONFIG_TANK_INPUT_I2S_MONO;
            break;
        case SYNTIANT_NDP10X_CONFIG_DNN_INPUT_SPI:
        case SYNTIANT_NDP10X_CONFIG_DNN_INPUT_SPI_DIRECT:
            tank_input = SYNTIANT_NDP10X_CONFIG_TANK_INPUT_SPI;
            break;
        }
    }

    s = syntiant_ndp10x_config_check_tank(st->dnn_input, tank_input);
    if (s)
        goto out;

    /* TODO: handle the dnn_input == NONE case */

    tank = NDP10X_DSP_CONFIG_TANK_INPUTSEL_MASK_INSERT
        (tank, (unsigned int)
         (tank_input == SYNTIANT_NDP10X_CONFIG_TANK_INPUT_FILTER_BANK));

    switch (tank_input) {
    case SYNTIANT_NDP10X_CONFIG_TANK_INPUT_PDM0:
    case SYNTIANT_NDP10X_CONFIG_TANK_INPUT_I2S_LEFT:
    case SYNTIANT_NDP10X_CONFIG_TANK_INPUT_I2S_MONO:
        v = NDP10X_DSP_CONFIG_SMPLCTL_TANKAUDIOCHSEL_NEGEDGE;
        break;
    case SYNTIANT_NDP10X_CONFIG_TANK_INPUT_PDM1:
    case SYNTIANT_NDP10X_CONFIG_TANK_INPUT_I2S_RIGHT:
        v = NDP10X_DSP_CONFIG_SMPLCTL_TANKAUDIOCHSEL_POSEDGE;
        break;
    case SYNTIANT_NDP10X_CONFIG_TANK_INPUT_PDM_SUM:
    case SYNTIANT_NDP10X_CONFIG_TANK_INPUT_I2S_SUM:
        v = NDP10X_DSP_CONFIG_SMPLCTL_TANKAUDIOCHSEL_SUM;
        break;
    case SYNTIANT_NDP10X_CONFIG_TANK_INPUT_PDM_BOTH:
    case SYNTIANT_NDP10X_CONFIG_TANK_INPUT_I2S_BOTH:
        v = NDP10X_DSP_CONFIG_SMPLCTL_TANKAUDIOCHSEL_BOTH;
        break;
    case SYNTIANT_NDP10X_CONFIG_TANK_INPUT_PDM0_RAW:
        v = NDP10X_DSP_CONFIG_SMPLCTL_TANKAUDIOCHSEL_NEGPDMIN;
        break;
    case SYNTIANT_NDP10X_CONFIG_TANK_INPUT_PDM1_RAW:
        v = NDP10X_DSP_CONFIG_SMPLCTL_TANKAUDIOCHSEL_POSPDMIN;
        break;
    case SYNTIANT_NDP10X_CONFIG_TANK_INPUT_PDM_BOTH_RAW:
        v = NDP10X_DSP_CONFIG_SMPLCTL_TANKAUDIOCHSEL_BOTHPDMIN;
        break;
    case SYNTIANT_NDP10X_CONFIG_TANK_INPUT_SPI:
    case SYNTIANT_NDP10X_CONFIG_TANK_INPUT_NONE:
    case SYNTIANT_NDP10X_CONFIG_TANK_INPUT_FILTER_BANK:
        v = NDP10X_DSP_CONFIG_SMPLCTL_TANKAUDIOCHSEL_EXTRACT(smplctl);
        break;
    default:
        /* logic error, missing case */
        s = SYNTIANT_NDP_ERROR_ARG;
    }

    smplctl = NDP10X_DSP_CONFIG_SMPLCTL_TANKAUDIOCHSEL_MASK_INSERT(smplctl, v);

    enable = tank_input != SYNTIANT_NDP10X_CONFIG_TANK_INPUT_NONE;
    tank = NDP10X_DSP_CONFIG_TANK_ENABLE_MASK_INSERT(tank, enable);

    if (!(config->set & SYNTIANT_NDP10X_CONFIG_SET_TANK_SIZE)
        && tank_input != SYNTIANT_NDP10X_CONFIG_TANK_INPUT_NONE
        && !NDP10X_DSP_CONFIG_TANK_SIZE_EXTRACT(tank)) {
        tank = NDP10X_DSP_CONFIG_TANK_SIZE_MASK_INSERT
            (tank, SYNTIANT_NDP10X_DEFAULT_TANK_SIZE);
    }

    st->smplctl = smplctl;
    st->smplmark = smplmark;
    st->tank = tank;
    st->tank_input = tank_input;

 out:
    return s;
}

void
syntiant_ndp10x_config_get_freq(struct syntiant_ndp10x_config_state_s *st,
                                struct syntiant_ndp10x_config_s *config)
{
    uint32_t freqctl = st->freqctl;

    config->preemphasis_exponent =
        NDP10X_DSP_CONFIG_FREQCTL_PREEMPHCOEFEXP_EXTRACT(freqctl);
    config->power_offset =
        NDP10X_DSP_CONFIG_FREQCTL_POWEROFFSET_EXTRACT(freqctl);
    config->power_scale_exponent =
        NDP10X_DSP_CONFIG_FREQCTL_POWERATTENEXP_EXTRACT(freqctl);
    config->audio_frame_step =
        NDP10X_DSP_CONFIG_FREQCTL_FRAMESTEP_EXTRACT(freqctl);
    config->freq_frame_size =
        NDP10X_DSP_CONFIG_FREQCTL_NUMFILT_EXTRACT(freqctl);
}

int
syntiant_ndp10x_config_set_freq(struct syntiant_ndp10x_config_s *config,
                                struct syntiant_ndp10x_config_state_s *st)
{
    uint32_t freqctl = st->freqctl;
    unsigned int enable = 1;
    int s = SYNTIANT_NDP_ERROR_NONE;

    if (config->set & SYNTIANT_NDP10X_CONFIG_SET_DNN_INPUT) {
        switch (st->dnn_input) {
        case SYNTIANT_NDP10X_CONFIG_DNN_INPUT_SPI_DIRECT:
        case SYNTIANT_NDP10X_CONFIG_DNN_INPUT_I2S_DIRECT:
            enable = 0;
            break;
        case SYNTIANT_NDP10X_CONFIG_DNN_INPUT_NONE:
            enable = (st->tank_input
                      == SYNTIANT_NDP10X_CONFIG_TANK_INPUT_FILTER_BANK);
            break;
        }

        freqctl = NDP10X_DSP_CONFIG_FREQCTL_ENABLE_MASK_INSERT(freqctl, enable);
        freqctl = NDP10X_DSP_CONFIG_FREQCTL_ENABLESAMPLES_MASK_INSERT
            (freqctl, enable);
    }

    if (config->set & SYNTIANT_NDP10X_CONFIG_SET_PREEMPHASIS_EXPONENT) {
        if ((NDP10X_DSP_CONFIG_FREQCTL_PREEMPHCOEFEXP_MASK
             >> NDP10X_DSP_CONFIG_FREQCTL_PREEMPHCOEFEXP_SHIFT)
            < config->preemphasis_exponent) {
            s = SYNTIANT_NDP_ERROR_ARG;
            goto out;
        }
        freqctl = NDP10X_DSP_CONFIG_FREQCTL_PREEMPHCOEFEXP_MASK_INSERT
            (freqctl, config->preemphasis_exponent);
    }

    if (config->set & SYNTIANT_NDP10X_CONFIG_SET_POWER_OFFSET) {
        if ((NDP10X_DSP_CONFIG_FREQCTL_POWEROFFSET_MASK
             >> NDP10X_DSP_CONFIG_FREQCTL_POWEROFFSET_SHIFT)
            < config->power_offset) {
            s = SYNTIANT_NDP_ERROR_ARG;
            goto out;
        }
        freqctl = NDP10X_DSP_CONFIG_FREQCTL_POWEROFFSET_MASK_INSERT(
            freqctl, config->power_offset);
    }

    if (config->set & SYNTIANT_NDP10X_CONFIG_SET_POWER_SCALE_EXPONENT) {
        if ((NDP10X_DSP_CONFIG_FREQCTL_POWERATTENEXP_MASK
             >> NDP10X_DSP_CONFIG_FREQCTL_POWERATTENEXP_SHIFT)
            < config->power_scale_exponent) {
            s = SYNTIANT_NDP_ERROR_ARG;
            goto out;
        }
        freqctl = NDP10X_DSP_CONFIG_FREQCTL_POWERATTENEXP_MASK_INSERT
            (freqctl, config->power_scale_exponent);
    }

    if (config->set & SYNTIANT_NDP10X_CONFIG_SET_FREQ_FRAME_SIZE) {
        if (SYNTIANT_NDP10X_MAX_FREQUENCY_BINS < config->freq_frame_size) {
            s = SYNTIANT_NDP_ERROR_ARG;
            goto out;
        }
        freqctl = NDP10X_DSP_CONFIG_FREQCTL_NUMFILT_MASK_INSERT
            (freqctl, config->freq_frame_size);
    }

    st->freqctl = freqctl;

out:
    return s;
}

void
syntiant_ndp10x_config_get_pdm(struct syntiant_ndp10x_config_state_s *st,
                               struct syntiant_ndp10x_config_s *config)
{
    int i, neg;
    uint32_t pdmcfg, v;

    for (i = 0; i < 2; i++) {
        pdmcfg = st->pdmcfg[i];
        config->pdm_out_shift[i] =
            NDP10X_DSP_CONFIG_PDMCFG_OUTSHIFT_EXTRACT(pdmcfg);
        config->pdm_in_shift[i] =
            NDP10X_DSP_CONFIG_PDMCFG_INSHIFT_EXTRACT(pdmcfg);
        v = NDP10X_DSP_CONFIG_PDMCFG_DCOFFSET_EXTRACT(pdmcfg);
        neg = !!(v & ((NDP10X_DSP_CONFIG_PDMCFG_DCOFFSET_MASK
                       >> (NDP10X_DSP_CONFIG_PDMCFG_DCOFFSET_SHIFT + 1)) + 1));
        v &= (NDP10X_DSP_CONFIG_PDMCFG_DCOFFSET_MASK
              >> (NDP10X_DSP_CONFIG_PDMCFG_DCOFFSET_SHIFT + 1));
        config->pdm_dc_offset[i] = neg ? -((int) v) : (int) v;
    }

    /* pdm0 and pdm1 should be configured with the same decimation value */
    config->pdm_clock_rate = SYNTIANT_NDP10X_AUDIO_FREQUENCY
        * NDP10X_DSP_CONFIG_PDMCFG_DECIMATION_EXTRACT(st->pdmcfg[0]);

    config->pdm_clock_ndp =
        NDP10X_CHIP_CONFIG_CLKCTL2_PDMCLKOUTENABLE_EXTRACT(st->clkctl2);
}

int
syntiant_ndp10x_config_set_pdm(struct syntiant_ndp10x_config_s *config,
                               struct syntiant_ndp10x_config_state_s *st)
{
    int i;
    unsigned int active;
    uint32_t decimation;
    uint32_t divisor;
    uint32_t pdmcfg;
    unsigned int pdm_clk_out;
    unsigned int pdm0 = 0;
    unsigned int pdm1 = 0;
    unsigned int pdm_capture[2] = {0, 0};
    uint32_t pdmctl = st->pdmctl;
    uint32_t clkctl2 = st->clkctl2;
    int s = SYNTIANT_NDP_ERROR_NONE;

    switch (st->dnn_input) {
    case SYNTIANT_NDP10X_CONFIG_DNN_INPUT_PDM0:
        pdm0 = 1;
        break;
    case SYNTIANT_NDP10X_CONFIG_DNN_INPUT_PDM1:
        pdm1 = 1;
        break;
    case SYNTIANT_NDP10X_CONFIG_DNN_INPUT_PDM_SUM:
        pdm0 = 1;
        pdm1 = 1;
    }

    switch (st->tank_input) {
    case SYNTIANT_NDP10X_CONFIG_TANK_INPUT_PDM0:
        pdm0 = 1;
        break;
    case SYNTIANT_NDP10X_CONFIG_TANK_INPUT_PDM0_RAW:
        pdm0 = 1;
        pdm_capture[0] = 1;
        break;
    case SYNTIANT_NDP10X_CONFIG_TANK_INPUT_PDM1:
        pdm1 = 1;
        break;
    case SYNTIANT_NDP10X_CONFIG_TANK_INPUT_PDM1_RAW:
        pdm1 = 1;
        pdm_capture[1] = 1;
        break;
    case SYNTIANT_NDP10X_CONFIG_TANK_INPUT_PDM_SUM:
    case SYNTIANT_NDP10X_CONFIG_TANK_INPUT_PDM_BOTH:
        pdm0 = 1;
        pdm1 = 1;
        break;
    case SYNTIANT_NDP10X_CONFIG_TANK_INPUT_PDM_BOTH_RAW:
        pdm0 = 1;
        pdm1 = 1;
        pdm_capture[0] = 1;
        pdm_capture[1] = 1;
        break;
    }

    /* B0 requires pdm1 clock to be enabled if either PDM interface is in use */
    pdm1 = pdm0 ? 1 : pdm1;
    active = pdm0 || pdm1;

    pdmctl =
        NDP10X_DSP_CONFIG_PDMCTL_NEGEDGEENABLE_MASK_INSERT(pdmctl, pdm0);
    pdmctl =
        NDP10X_DSP_CONFIG_PDMCTL_POSEDGEENABLE_MASK_INSERT(pdmctl, pdm1);
    pdmctl =
        NDP10X_DSP_CONFIG_PDMCTL_ENABLE_MASK_INSERT(pdmctl, active);

    if (config->set & SYNTIANT_NDP10X_CONFIG_SET_PDM_CLOCK_NDP) {
        clkctl2 = NDP10X_CHIP_CONFIG_CLKCTL2_PDMCLKOUTENABLE_MASK_INSERT
            (clkctl2, config->pdm_clock_ndp);
        clkctl2 = NDP10X_CHIP_CONFIG_CLKCTL2_PDMCLKOUTNEEDED_MASK_INSERT
            (clkctl2, config->pdm_clock_ndp);
    }

    pdm_clk_out = NDP10X_CHIP_CONFIG_CLKCTL2_PDMCLKOUTENABLE_EXTRACT(clkctl2);

    /* Compute the real PDM divisor */
    if (config->set & SYNTIANT_NDP10X_CONFIG_SET_PDM_CLOCK_RATE) {
        if (config->pdm_clock_rate <= 0) {
            s = SYNTIANT_NDP_ERROR_ARG;
            goto out;
        }

        if (!st->core_clock_rate) {
            s = SYNTIANT_NDP_ERROR_FAIL;
            goto out;
        }

        /* get PDM clock and round and account for div-2 in final clock */
        divisor = (st->core_clock_rate / config->pdm_clock_rate);
        divisor = (divisor >> 1) + (divisor & 0x1);
        clkctl2 = NDP10X_CHIP_CONFIG_CLKCTL2_PDMCLKOUTDIV_MASK_INSERT
            (clkctl2, divisor);
    }

    divisor = NDP10X_CHIP_CONFIG_CLKCTL2_PDMCLKOUTDIV_EXTRACT(clkctl2);
    /* this first computation is * 2 with which to round up */
    decimation = pdm_clk_out
        ? st->core_clock_rate / divisor
        : config->pdm_clock_rate * 2;
    decimation = decimation / SYNTIANT_NDP10X_AUDIO_FREQUENCY;
    decimation = (decimation / 2) + (decimation & 0x1);

    for (i = 0; i < 2; i++) {
        pdmcfg = st->pdmcfg[i];
        pdmcfg = NDP10X_DSP_CONFIG_PDMCFG_ENABLECAPTURE_MASK_INSERT
            (pdmcfg, pdm_capture[i]);
        if (config->set & SYNTIANT_NDP10X_CONFIG_SET_PDM_OUT_SHIFT) {
            pdmcfg = NDP10X_DSP_CONFIG_PDMCFG_OUTSHIFT_MASK_INSERT
                (pdmcfg, config->pdm_out_shift[i]);
        }
        if (config->set & SYNTIANT_NDP10X_CONFIG_SET_PDM_IN_SHIFT) {
            pdmcfg = NDP10X_DSP_CONFIG_PDMCFG_INSHIFT_MASK_INSERT
                (pdmcfg, config->pdm_in_shift[i]);
        }
        if (config->set & SYNTIANT_NDP10X_CONFIG_SET_PDM_DC_OFFSET) {
            pdmcfg = NDP10X_DSP_CONFIG_PDMCFG_DCOFFSET_MASK_INSERT
                (pdmcfg, (unsigned int) config->pdm_dc_offset[i]);
        }
        if (pdm_clk_out ||
            (config->set & SYNTIANT_NDP10X_CONFIG_SET_PDM_CLOCK_RATE)) {
            pdmcfg = NDP10X_DSP_CONFIG_PDMCFG_DECIMATION_MASK_INSERT
                (pdmcfg, decimation);
        }
        st->pdmcfg[i] = pdmcfg;
    }

    st->pdmctl = pdmctl;
    st->clkctl2 = clkctl2;

 out:
    return s;
}

void
syntiant_ndp10x_config_get_i2s(struct syntiant_ndp10x_config_state_s *st,
                               struct syntiant_ndp10x_config_s *config)
{
    uint32_t i2sctl = st->i2sctl;

    config->i2s_frame_size =
        NDP10X_DSP_CONFIG_I2SCTL_FRAMESIZE_EXTRACT(i2sctl) + 1;
    config->i2s_sample_size =
        NDP10X_DSP_CONFIG_I2SCTL_SAMPLESIZE_EXTRACT(i2sctl) + 1;
    config->i2s_sample_msbit =
        NDP10X_DSP_CONFIG_I2SCTL_MSBINDEX_EXTRACT(i2sctl);
}

int
syntiant_ndp10x_config_set_i2s(struct syntiant_ndp10x_config_s *config,
                               struct syntiant_ndp10x_config_state_s *st)
{
    unsigned int left = 0;
    unsigned int right = 0;
    unsigned int packed;
    unsigned int burst = 0;
    unsigned int active;
    uint32_t v;
    uint32_t i2sctl = st->i2sctl;
    int s = SYNTIANT_NDP_ERROR_NONE;

    packed = NDP10X_DSP_CONFIG_I2SCTL_PACKED_EXTRACT(i2sctl);

    switch (st->dnn_input) {
    case SYNTIANT_NDP10X_CONFIG_DNN_INPUT_I2S_LEFT:
        left = 1;
        packed = 0;
        break;
    case SYNTIANT_NDP10X_CONFIG_DNN_INPUT_I2S_RIGHT:
        right = 1;
        packed = 0;
        break;
    case SYNTIANT_NDP10X_CONFIG_DNN_INPUT_I2S_SUM:
        left = 1;
        right = 1;
        packed = 0;
        break;
    case SYNTIANT_NDP10X_CONFIG_DNN_INPUT_I2S_MONO:
        left = 1;
        packed = 1;
        break;
    case SYNTIANT_NDP10X_CONFIG_DNN_INPUT_I2S_DIRECT:
        left = 1;
        packed = 1;
        burst = 1;
    }

    switch (st->tank_input) {
    case SYNTIANT_NDP10X_CONFIG_TANK_INPUT_I2S_LEFT:
        left = 1;
        packed = 0;
        break;
    case SYNTIANT_NDP10X_CONFIG_TANK_INPUT_I2S_RIGHT:
        right = 1;
        packed = 0;
        break;
    case SYNTIANT_NDP10X_CONFIG_TANK_INPUT_I2S_SUM:
    case SYNTIANT_NDP10X_CONFIG_TANK_INPUT_I2S_BOTH:
        left = 1;
        right = 1;
        packed = 0;
        break;
    case SYNTIANT_NDP10X_CONFIG_TANK_INPUT_I2S_MONO:
        left = 1;
        packed = 1;
        break;
    }

    /* TODO: configure framesize, samplesize, msbindex */

    active = left || right;

    i2sctl = NDP10X_DSP_CONFIG_I2SCTL_PACKED_MASK_INSERT(i2sctl, packed);
    i2sctl = NDP10X_DSP_CONFIG_I2SCTL_LEFTCHENABLE_MASK_INSERT(i2sctl, left);
    i2sctl = NDP10X_DSP_CONFIG_I2SCTL_RIGHTCHENABLE_MASK_INSERT(i2sctl, right);
    i2sctl = NDP10X_DSP_CONFIG_I2SCTL_MODE_MASK_INSERT(i2sctl, burst);
    i2sctl = NDP10X_DSP_CONFIG_I2SCTL_ENABLE_MASK_INSERT(i2sctl, active);

    if (config->set & SYNTIANT_NDP10X_CONFIG_SET_I2S_FRAME_SIZE) {
        v = config->i2s_frame_size - 1;
        if ((NDP10X_DSP_CONFIG_I2SCTL_FRAMESIZE_MASK
             >> NDP10X_DSP_CONFIG_I2SCTL_FRAMESIZE_SHIFT) < v) {
            s = SYNTIANT_NDP_ERROR_ARG;
            goto out;
        }
        i2sctl = NDP10X_DSP_CONFIG_I2SCTL_FRAMESIZE_MASK_INSERT(i2sctl, v);
    }

    if (config->set & SYNTIANT_NDP10X_CONFIG_SET_I2S_SAMPLE_SIZE) {
        v = config->i2s_sample_size - 1;
        if ((NDP10X_DSP_CONFIG_I2SCTL_SAMPLESIZE_MASK
             >> NDP10X_DSP_CONFIG_I2SCTL_SAMPLESIZE_SHIFT) < v) {
            s = SYNTIANT_NDP_ERROR_ARG;
            goto out;
        }
        i2sctl = NDP10X_DSP_CONFIG_I2SCTL_SAMPLESIZE_MASK_INSERT(i2sctl, v);
    }

    if (config->set & SYNTIANT_NDP10X_CONFIG_SET_I2S_SAMPLE_MSBIT) {
        if ((NDP10X_DSP_CONFIG_I2SCTL_MSBINDEX_MASK
             >> NDP10X_DSP_CONFIG_I2SCTL_MSBINDEX_SHIFT)
            < config->i2s_sample_msbit) {
            s = SYNTIANT_NDP_ERROR_ARG;
            goto out;
        }
        i2sctl = NDP10X_DSP_CONFIG_I2SCTL_MSBINDEX_MASK_INSERT
            (i2sctl, config->i2s_sample_msbit);
    }

    st->i2sctl = i2sctl;

 out:
    return s;
}

void
syntiant_ndp10x_config_get_spi(struct syntiant_ndp10x_config_state_s *st,
                               struct syntiant_ndp10x_config_s *config)
{
    if (!config) return;

    config->spi_word_bits =
        NDP10X_DSP_CONFIG_SMPLCTL_SPIFORMAT_EXTRACT(st->smplctl) ? 8 : 16;
}

int
syntiant_ndp10x_config_set_spi(struct syntiant_ndp10x_config_s *config,
                               struct syntiant_ndp10x_config_state_s *st)
{
    unsigned int v;
    int s = SYNTIANT_NDP_ERROR_NONE;

    if (config->set & SYNTIANT_NDP10X_CONFIG_SET_SPI_WORD_BITS) {
        v = config->spi_word_bits;
        if (v != 8 && v != 16) {
            s = SYNTIANT_NDP_ERROR_ARG;
            goto out;
        }
        st->smplctl = NDP10X_DSP_CONFIG_SMPLCTL_SPIFORMAT_MASK_INSERT
            (st->smplctl, (unsigned int) (v == 8));
    }

 out:
    return s;
}

void
syntiant_ndp10x_config_get_input(struct syntiant_ndp10x_config_state_s *st,
                                 struct syntiant_ndp10x_config_s *config,
                                 struct syntiant_ndp10x_device_s *ndp10x)
{
    syntiant_ndp10x_config_get_dnn(st, config);
    config->dnn_input = st->dnn_input;

    syntiant_ndp10x_config_get_tank(st, config);
    config->tank_input = st->tank_input;

    syntiant_ndp10x_config_get_freq(st, config);

    syntiant_ndp10x_config_get_pdm(st, config);

    syntiant_ndp10x_config_get_i2s(st, config);

    syntiant_ndp10x_config_get_spi(st, config);

    ndp10x->dnn_input = config->dnn_input;
    ndp10x->tank_bits = config->tank_bits;
    ndp10x->tank_input = config->tank_input;
    ndp10x->tank_size = config->tank_size;
    ndp10x->audio_frame_size = config->audio_frame_size;
    ndp10x->audio_frame_step = config->audio_frame_step;
    ndp10x->freq_frame_size = config->freq_frame_size;
    ndp10x->dnn_frame_size = config->dnn_frame_size;
}

int
syntiant_ndp10x_config_set_input(struct syntiant_ndp10x_config_s *config,
                                 struct syntiant_ndp10x_config_state_s *st)
{
    int s;

    s = syntiant_ndp10x_config_set_dnn(config, st);
    if (s)
        goto out;

    s = syntiant_ndp10x_config_set_tank(config, st);
    if (s)
        goto out;

    s = syntiant_ndp10x_config_set_freq(config, st);
    if (s)
        goto out;

    s = syntiant_ndp10x_config_set_pdm(config, st);
    if (s)
        goto out;

    s = syntiant_ndp10x_config_set_i2s(config, st);
    if (s)
        goto out;

    s = syntiant_ndp10x_config_set_spi(config, st);
    if (s)
        goto out;

out:
    return s;
}

void
syntiant_ndp10x_config_get_clock(struct syntiant_ndp10x_config_state_s *st,
                                 struct syntiant_ndp10x_config_s *config)
{
    unsigned int mcu_clock_div;
    unsigned int freq_clock_div;
    unsigned int dnn_clock_div;

    mcu_clock_div = NDP10X_CHIP_CONFIG_CLKCTL0_MCUCLKDIV_EXTRACT(st->clkctl0);
    freq_clock_div = NDP10X_CHIP_CONFIG_CLKCTL1_FFTCLKDIV_EXTRACT(st->clkctl1);
    dnn_clock_div = NDP10X_CHIP_CONFIG_CLKCTL1_DNNCLKDIV_EXTRACT(st->clkctl1);

    /* Compute clocks */
    config->input_clock_rate = st->input_clock_rate;
    config->core_clock_rate = st->core_clock_rate;
    config->mcu_clock_rate =
        mcu_clock_div ? st->core_clock_rate / mcu_clock_div : 0;
    config->freq_clock_rate =
        freq_clock_div ? st->core_clock_rate / freq_clock_div : 0;
    config->spi_max_pcm_input_rate =
        config->freq_clock_rate ? config->freq_clock_rate * 2 : 0;
    config->dnn_clock_rate =
        dnn_clock_div ? st->core_clock_rate / dnn_clock_div : 0;
}

int
syntiant_ndp10x_config_calc_mult_mult(unsigned int* core_clock_rate,
                                      unsigned int* input_clock_rate,
                                      unsigned int* clock_mult,
                                      unsigned int* clock_div)
{
    int s = SYNTIANT_NDP_ERROR_NONE;


    /*
     * compute clock multiplier with 3 frac bits, but note
     * the FLL target is actually core_clock_rate / 2
     * numerator factor is / 2 for core_clock_rate / 2,
     * and * 2 for 1 fractional bit with which to round up
     */
    *clock_mult = (*core_clock_rate << (3 - 1 + 1))
        / *input_clock_rate;
    /* round up */
    *clock_mult = (*clock_mult / 2) + (*clock_mult & 0x1);

    /*
     * compute reciprocal of clock multiplier with (all) 14 frac bits
     * for tracking.
     * A simple multiplication by 2^14 would overflow the clock rate
     * in the worst case (4MHz), so we multiply by the maximum
     * 2^10 and divide the core clock by 2^4(+1-1) to ensure no overflow
     * again note the FLL target is actually core_clock_rate / 2
     * denominator factor is / 2 for core_clock_rate / 2,
     * and * 2 for 1 fractional bit with which to round up
     */
    *clock_div  = (*input_clock_rate << 10)
        / (*core_clock_rate >> (4 + 1 + 1));
    /* round up */
    *clock_div = (*clock_div / 2) + (*clock_div & 0x1);

    /*
     * Update core clock with real divisor
     * * 2 for core_clock_rate / 2,
     * * 2  for 1 fractional bit with which to round up
     */
    *core_clock_rate = (*input_clock_rate * (*clock_mult))
        >> (3 - 1 - 1);
    *core_clock_rate = (*core_clock_rate / 2) + (*core_clock_rate & 0x1);

    return s;
}

int
syntiant_ndp10x_config_set_clock_slow_read(struct syntiant_ndp_device_s *ndp,
                                           uint32_t addr, uint32_t *vp)
{
    int s;
    uint32_t b[2];
    
    s = syntiant_ndp10x_write_block(ndp, 0, NDP10X_SPI_MADDR(0), &addr, 4);
    if (s) {
        goto out;
    }
    
    s = syntiant_ndp10x_read_block(ndp, 0, NDP10X_SPI_MADDR(0), b, 8);
    if (s) {
        goto out;
    }

    *vp = b[1];

 out:
    return s;
    
}

int
syntiant_ndp10x_config_set_clock(struct syntiant_ndp_device_s *ndp,
                                 struct syntiant_ndp10x_config_s *config,
                                 struct syntiant_ndp10x_config_state_s *st)
{
    int s = SYNTIANT_NDP_ERROR_NONE;
    int use_fll;
    uint8_t ctl = st->ctl;
    unsigned int core_clock_rate = 0;
    unsigned int clock_mult;
    unsigned int clock_div;
    unsigned int pdm_clk_div = 1;
    int i;
    uint32_t fllctl0;
    uint32_t fllctl1;
    uint32_t fllctl2;
    uint32_t fllsts0;
    uint32_t fllsts1;
    uint32_t v, v0;
    unsigned int module_clock_div;
    uint32_t config_address;

    if (config->set & SYNTIANT_NDP10X_CONFIG_SET_INPUT_CLOCK_RATE) {
        st->input_clock_rate = config->input_clock_rate;

        /* Determine internal clock rate */
        use_fll = config->input_clock_rate
            < SYNTIANT_NDP10X_MAX_FLL_REF_CLOCK_RATE;
        core_clock_rate = config->input_clock_rate;

        /* write input clock rate into the config region*/
        config_address = NDP10X_ILIB_SCRATCH_ORIGIN;
        s = syntiant_ndp10x_write(ndp, 1, config_address, core_clock_rate);
        if (s) goto out;
        /* -------------------------------------------------------
         * Configures the internal FLL
         * ------------------------------------------------------- */
        if (use_fll) {

            /* find out if external clock is already on and turn it off */
            if (NDP10X_SPI_CTL_EXTCLK_EXTRACT(ctl)) {
                ctl = (uint8_t) NDP10X_SPI_CTL_EXTCLK_MASK_INSERT(ctl, 0);
                s = syntiant_ndp10x_write(ndp, 0, NDP10X_SPI_CTL, ctl);
                if (s)
                    goto out;
            }

            /* setup nominal target */

            config->pdm_clock_rate = ( config->pdm_clock_rate % 64000)
                ? (config->pdm_clock_rate + 32000) / 64000 * 64000
                : config->pdm_clock_rate;

   	    pdm_clk_div =  (config->pdm_clock_rate > 0)
                ? ((SYNTIANT_NDP10X_TARGET_CLOCK_RATE + 
                   ( config->pdm_clock_rate >> 1) ) / config->pdm_clock_rate )
                : 1;

            pdm_clk_div = (pdm_clk_div > 1 && pdm_clk_div % 2) ?
                pdm_clk_div + 1 : pdm_clk_div;

            core_clock_rate = (config->pdm_clock_rate > 0)
                ? ((pdm_clk_div * config->pdm_clock_rate <
                    SYNTIANT_NDP10X_TARGET_CLOCK_RATE)
                   ? (pdm_clk_div + 2 ) * config->pdm_clock_rate
                   : pdm_clk_div * config->pdm_clock_rate)
                : SYNTIANT_NDP10X_TARGET_CLOCK_RATE;
            
            syntiant_ndp10x_config_calc_mult_mult(&core_clock_rate,
                                                  &(config->input_clock_rate),
                                                  &clock_mult,
                                                  &clock_div);

            /* Forward write (no read/modify/write til clock rate is high */
            fllctl1 = NDP10X_CHIP_CONFIG_FLLCTL1_DEFAULT;
            fllctl1 = NDP10X_CHIP_CONFIG_FLLCTL1_FREQMULT_MASK_INSERT
                (fllctl1, clock_mult);
            fllctl1 = NDP10X_CHIP_CONFIG_FLLCTL1_FREQDIV_MASK_INSERT
                (fllctl1, clock_div);
            s = syntiant_ndp10x_write(ndp, 1, NDP10X_CHIP_CONFIG_FLLCTL1,
                                      fllctl1);
            if (s)
                goto out;

            /* fix for #108 [HLS - ClockMult] FLL loop gain bug */
            fllctl2 = NDP10X_CHIP_CONFIG_FLLCTL2_DEFAULT;
            fllctl2 = NDP10X_CHIP_CONFIG_FLLCTL2_LOOPGAIN1_MASK_INSERT
                (fllctl2, 0x7); 
	    s = syntiant_ndp10x_write(ndp, 1, NDP10X_CHIP_CONFIG_FLLCTL2,
                                      fllctl2);
            if (s)
                goto out; 

            /* Turn on tracking */
            fllctl0 = NDP10X_CHIP_CONFIG_FLLCTL0_DEFAULT;
            fllctl0 = NDP10X_CHIP_CONFIG_FLLCTL0_ENABLETRACKING_MASK_INSERT
                (fllctl0, 1);
            s = syntiant_ndp10x_write(ndp, 1, NDP10X_CHIP_CONFIG_FLLCTL0,
                                      fllctl0);
            if (s)
                goto out;

            /* loop until locked */
            /* loop count ~ 1mbps SPI / 80 bits/read * 0.02 sec wait -> 250 */
            s = SYNTIANT_NDP_ERROR_TIMEOUT;
            for (i = 0; i < 250; i++) {
                /* read FLL status */
                s = syntiant_ndp10x_read(ndp, 1, NDP10X_CHIP_CONFIG_FLLSTS0,
                                         &fllsts0);
                if (s)
                    goto out;

                /* break out of lock */
                if (NDP10X_CHIP_CONFIG_FLLSTS0_MODE_EXTRACT(fllsts0)
                    == NDP10X_CHIP_CONFIG_FLLSTS0_MODE_LOCKED) {
                    s = SYNTIANT_NDP_ERROR_NONE;
                    break;
                /* Check the tracking mode */
                } else if (NDP10X_CHIP_CONFIG_FLLSTS0_MODE_EXTRACT(fllsts0)
                    == NDP10X_CHIP_CONFIG_FLLSTS0_MODE_TRACKINITIAL) {

                    /* read the integrator */
                    s = syntiant_ndp10x_read(ndp, 1, NDP10X_CHIP_CONFIG_FLLSTS1,
                                         &fllsts1);
                    if (s)
                        goto out;


                    /* Trap when oscillator is too slow */
                    if ((NDP10X_CHIP_CONFIG_FLLSTS1_INTEGRATOR_EXTRACT(fllsts1)
                         == 0xfffff)
                       && (NDP10X_CHIP_CONFIG_FLLSTS0_DCOCSEL_EXTRACT(fllsts0)
                           == 0x7)) {

                        /* Push clock rate to OVERDRIVE rate */
                        core_clock_rate =
                            SYNTIANT_NDP10X_TARGET_OVERDRIVE_CLOCK_RATE;
                        syntiant_ndp10x_config_calc_mult_mult
                            (&core_clock_rate, &(config->input_clock_rate),
                             &clock_mult, &clock_div);

                        /*
                         * Forward write (no read/modify/write) til clock
                         * rate is high
                         */
                        fllctl1 = NDP10X_CHIP_CONFIG_FLLCTL1_DEFAULT;
                        fllctl1 =
                            NDP10X_CHIP_CONFIG_FLLCTL1_FREQMULT_MASK_INSERT
                            (fllctl1, clock_mult);
                        fllctl1 = NDP10X_CHIP_CONFIG_FLLCTL1_FREQDIV_MASK_INSERT
                                (fllctl1, clock_div);
                        s = syntiant_ndp10x_write(ndp, 1,
                                                  NDP10X_CHIP_CONFIG_FLLCTL1,
                                                  fllctl1);
                    }

                    s = SYNTIANT_NDP_ERROR_NONE;
                    break;
                }

            }
            if (s)
                goto out;

        /* -------------------------------------------------------
         * Turn on external clock if needed *
         * ------------------------------------------------------- */
        } else if (NDP10X_SPI_CTL_EXTCLK_EXTRACT(ctl) == 0) {

            /*
             * do a 'good' read before enabling the external clock
             */
            s = syntiant_ndp10x_config_set_clock_slow_read
                (ndp, NDP10X_BOOTROM, &v);
            if (s)
                goto out;

            s = SYNTIANT_NDP_ERROR_TIMEOUT;
            for (i = 0; i < 128; i++) {
                /* Turn on the clock */
                ctl = NDP10X_SPI_CTL_EXTCLK_MASK_INSERT(ctl, 1);
                s = syntiant_ndp10x_write(ndp, 0, NDP10X_SPI_CTL, ctl);
                if (s)
                    goto out;
                
                /* Read a location with a known different value */
                s = syntiant_ndp10x_config_set_clock_slow_read
                    (ndp, NDP10X_BOOTROM + 4, &v0);
                if (s)
                    goto out;

                if (v != v0) {
                    s = SYNTIANT_NDP_ERROR_NONE;
                    break;
                }
                /* if it reads as the same, new data was not read internally */

                /* turn off external clock again */
                ctl = (uint8_t) NDP10X_SPI_CTL_EXTCLK_MASK_INSERT(ctl, 0);
                s = syntiant_ndp10x_write(ndp, 0, NDP10X_SPI_CTL, ctl);
                if (s)
                    goto out;

            }
            /* If we hit 128 tries and no clock, it's a timeout */
            if (i == 128) {
                s = SYNTIANT_NDP_ERROR_TIMEOUT;
            }
            if (s)
                goto out;

        }
        st->ctl = ctl;

        /* -------------------------------------------------------
         *    This sections updates internal clock dividers
         * ------------------------------------------------------- */

        module_clock_div = 1;
        if (core_clock_rate > SYNTIANT_NDP10X_MAX_MCU_RATE) {
            module_clock_div = 2;
        }
        st->clkctl0 = NDP10X_CHIP_CONFIG_CLKCTL0_MCUCLKDIV_MASK_INSERT
            (st->clkctl0, module_clock_div);

        st->clkctl1 = NDP10X_CHIP_CONFIG_CLKCTL1_DNNCLKDIV_MASK_INSERT
            (st->clkctl1, module_clock_div);
    }

    if (NDP10X_SPI_CTL_EXTCLK_EXTRACT(ctl)) {
        st->core_clock_rate = st->input_clock_rate;
    } else {
        if (!(config->set & SYNTIANT_NDP10X_CONFIG_SET_INPUT_CLOCK_RATE)) {
            s = syntiant_ndp10x_read(ndp, 1, NDP10X_CHIP_CONFIG_FLLCTL1,
                                     &fllctl1);
            if (s)
                goto out;
            clock_mult = NDP10X_CHIP_CONFIG_FLLCTL1_FREQMULT_EXTRACT(fllctl1);
            /*
             * clock_mult has 3 fractional bits,
             * * 2 for core_clock_rate / 2,
             * * 2  for 1 fractional bit with which to round up
             */
            core_clock_rate = (st->input_clock_rate * clock_mult)
                >> (3 - 1 - 1);
            core_clock_rate = (core_clock_rate / 2) + (core_clock_rate & 0x1);
        }
        st->core_clock_rate = core_clock_rate;
    }

    if (config->set & SYNTIANT_NDP10X_CONFIG_SET_MCU_CLOCK_RATE) {
        if (config->mcu_clock_rate > SYNTIANT_NDP10X_MAX_MCU_RATE) {
            s = SYNTIANT_NDP_ERROR_ARG;
            goto out;
        }
        module_clock_div = st->core_clock_rate / config->mcu_clock_rate;
        st->clkctl0 = NDP10X_CHIP_CONFIG_CLKCTL0_MCUCLKDIV_MASK_INSERT
            (st->clkctl0, module_clock_div);
    }
    
    if (config->set & SYNTIANT_NDP10X_CONFIG_SET_DNN_CLOCK_RATE) {
        if (config->dnn_clock_rate > SYNTIANT_NDP10X_MAX_DNN_RATE) {
            s = SYNTIANT_NDP_ERROR_ARG;
            goto out;
        }
        module_clock_div = st->core_clock_rate / config->dnn_clock_rate;
        st->clkctl1 = NDP10X_CHIP_CONFIG_CLKCTL1_DNNCLKDIV_MASK_INSERT
            (st->clkctl1, module_clock_div);
    }
    
    if (config->set & SYNTIANT_NDP10X_CONFIG_SET_FREQ_CLOCK_RATE) {
        module_clock_div = st->core_clock_rate / config->freq_clock_rate;
        st->clkctl1 = NDP10X_CHIP_CONFIG_CLKCTL1_FFTCLKDIV_MASK_INSERT
            (st->clkctl1, module_clock_div);
    }

out:
    return s;
}

void
syntiant_ndp10x_config_get_fw_state(struct syntiant_ndp10x_config_state_s *st,
                                    struct syntiant_ndp10x_config_s *config)

{
    config->agc_on = !!(st->fw_enable & NDP10X_FW_STATE_ENABLE_AGC);
    config->agc_max_adj[0] = st->fw_max_adjustment_gain >> 8;
    config->agc_max_adj[1] = st->fw_max_adjustment_gain & 0xf;
    config->agc_nom_speech_quiet = st->fw_nom_speech_quiet >> 22;
    config->match_per_frame_on = st->fw_match_per_frame;
}

int
syntiant_ndp10x_config_set_fw_state(struct syntiant_ndp10x_config_s *config,
                                    struct syntiant_ndp10x_config_state_s *st)

{
    int s = SYNTIANT_NDP_ERROR_NONE;

    if (config->set & SYNTIANT_NDP10X_CONFIG_SET_AGC_ON) {
        if (!st->fw_state_addr) {
            s = SYNTIANT_NDP_ERROR_UNINIT;
            goto error;
        }
        if (config->agc_on) {
            st->fw_enable |= NDP10X_FW_STATE_ENABLE_AGC;
        } else {
            st->fw_enable &= (unsigned int) ~NDP10X_FW_STATE_ENABLE_AGC;
        }
    }

    if (config->set1 & SYNTIANT_NDP10X_CONFIG_SET1_AGC_MAX_ADJ) {
        if (!st->fw_state_addr) {
            s = SYNTIANT_NDP_ERROR_UNINIT;
            goto error;
        }
        if (15 < config->agc_max_adj[0] || 15 < config->agc_max_adj[1]) {
            s = SYNTIANT_NDP_ERROR_ARG;
            goto error;
        }
        st->fw_max_adjustment_gain =
            config->agc_max_adj[0] << 8 | config->agc_max_adj[1];
    }
    
    if (config->set1 & SYNTIANT_NDP10X_CONFIG_SET1_AGC_NOM_SPEECH_TARGET) {
        if (!st->fw_state_addr) {
            s = SYNTIANT_NDP_ERROR_UNINIT;
            goto error;
        }
        if (1023 < config->agc_nom_speech_quiet) {
            s = SYNTIANT_NDP_ERROR_ARG;
            goto error;
        }

        st->fw_nom_speech_quiet = config->agc_nom_speech_quiet << 22;
    }

    if (config->set & SYNTIANT_NDP10X_CONFIG_SET_MATCH_PER_FRAME_ON) {
        if (!st->fw_state_addr) {
            s = SYNTIANT_NDP_ERROR_UNINIT;
            goto error;
        }
        if (1 < config->match_per_frame_on) {
            s = SYNTIANT_NDP_ERROR_ARG;
            goto error;
        }

        st->fw_match_per_frame = config->match_per_frame_on;
    }

 error:
    return s;
}
    
void
syntiant_ndp10x_config_get_memory_power
(struct syntiant_ndp10x_config_state_s *st,
 struct syntiant_ndp10x_config_s *config)
{
    uint32_t v, m;
    int i;
    unsigned int mp;
    
    for (i = 0; i <= SYNTIANT_NDP10X_CONFIG_MEMORY_MAX; i++) {
        if (i == SYNTIANT_NDP10X_CONFIG_MEMORY_BOOTROM) {
            v = NDP10X_CHIP_CONFIG_MEM_PG_ROM_EXTRACT(st->mem);
            mp =
                v == NDP10X_CHIP_CONFIG_MEM_PG_ROM_POWERDOWN
                ? SYNTIANT_NDP10X_CONFIG_MEMORY_POWER_OFF
                : SYNTIANT_NDP10X_CONFIG_MEMORY_POWER_ON;
        } else if (SYNTIANT_NDP10X_CONFIG_MEMORY_DNN00 <= i) {
            m = 1U << (i - SYNTIANT_NDP10X_CONFIG_MEMORY_DNN00);
            mp =  st->mem1 & m
                ? (st->mem3 & m
                   ? SYNTIANT_NDP10X_CONFIG_MEMORY_POWER_OFF
                   : SYNTIANT_NDP10X_CONFIG_MEMORY_POWER_RETAIN)
                : SYNTIANT_NDP10X_CONFIG_MEMORY_POWER_ON;
        } else {
            v = i == SYNTIANT_NDP10X_CONFIG_MEMORY_RAM
                ? NDP10X_CHIP_CONFIG_MEM_PG_RAM_EXTRACT(st->mem)
                : (i == SYNTIANT_NDP10X_CONFIG_MEMORY_BOOTRAM
                   ? NDP10X_CHIP_CONFIG_MEM_PG_BOOTRAM_EXTRACT(st->mem)
                   : NDP10X_DNN_CONFIG_MEM0_PG_INRAM_EXTRACT(st->mem0));
            mp = v == NDP10X_CHIP_CONFIG_MEM_PG_RAM_POWERDOWN
                ? SYNTIANT_NDP10X_CONFIG_MEMORY_POWER_OFF
                : (v == NDP10X_CHIP_CONFIG_MEM_PG_RAM_ON
                   ? SYNTIANT_NDP10X_CONFIG_MEMORY_POWER_ON
                   : SYNTIANT_NDP10X_CONFIG_MEMORY_POWER_RETAIN);
        }
        config->memory_power[i] = (unsigned char) mp;
    }
}

int
syntiant_ndp10x_config_set_memory_power(struct syntiant_ndp10x_config_s *config,
                                        struct syntiant_ndp10x_config_state_s
                                        *st)

{
    int s = SYNTIANT_NDP_ERROR_NONE;
    int mp;
    uint32_t v, m, mem1, mem3;
    int i, shift;
    
    if (config->set1 & SYNTIANT_NDP10X_CONFIG_SET1_MEMORY_POWER) {
        for (i = 0; i <= SYNTIANT_NDP10X_CONFIG_MEMORY_MAX; i++) {
            if (config->memory_power[i]
                == SYNTIANT_NDP10X_CONFIG_MEMORY_POWER_NO_CHANGE) {
                continue;
            }
            if (SYNTIANT_NDP10X_CONFIG_MEMORY_POWER_MAX
                < config->memory_power[i]) {
                s = SYNTIANT_NDP_ERROR_ARG;
                goto out;
            }
            mp = config->memory_power[i];
            if (i == SYNTIANT_NDP10X_CONFIG_MEMORY_BOOTROM) {
                if (mp == SYNTIANT_NDP10X_CONFIG_MEMORY_POWER_RETAIN) {
                    s = SYNTIANT_NDP_ERROR_ARG;
                    goto out;
                }
                v = mp == SYNTIANT_NDP10X_CONFIG_MEMORY_POWER_OFF
                    ? NDP10X_CHIP_CONFIG_MEM_PG_ROM_POWERDOWN
                    : NDP10X_CHIP_CONFIG_MEM_PG_ROM_ON;
                st->mem = NDP10X_CHIP_CONFIG_MEM_PG_ROM_MASK_INSERT
                    (st->mem, v);
            } else if (SYNTIANT_NDP10X_CONFIG_MEMORY_DNN00 <= i) {
                shift = i - SYNTIANT_NDP10X_CONFIG_MEMORY_DNN00;
                m = ~(1U << shift);
                mem1 = mp != SYNTIANT_NDP10X_CONFIG_MEMORY_POWER_ON;
                st->mem1 = (mem1 << shift) | (st->mem1 & m);
                mem3 = mp != SYNTIANT_NDP10X_CONFIG_MEMORY_POWER_RETAIN;
                st->mem3 = (mem3 << shift) | (st->mem3 & m);
            } else {
                v = mp == SYNTIANT_NDP10X_CONFIG_MEMORY_POWER_OFF
                    ?  NDP10X_CHIP_CONFIG_MEM_PG_RAM_POWERDOWN
                    : (mp == SYNTIANT_NDP10X_CONFIG_MEMORY_POWER_ON
                       ? NDP10X_CHIP_CONFIG_MEM_PG_RAM_ON
                       : NDP10X_CHIP_CONFIG_MEM_PG_RAM_RET2);
                if (i == SYNTIANT_NDP10X_CONFIG_MEMORY_RAM) {
                    st->mem = NDP10X_CHIP_CONFIG_MEM_PG_RAM_MASK_INSERT
                        (st->mem, v);
                } else if (i == SYNTIANT_NDP10X_CONFIG_MEMORY_BOOTRAM) {
                    st->mem = NDP10X_CHIP_CONFIG_MEM_PG_BOOTRAM_MASK_INSERT
                        (st->mem, v);
                } else {
                    st->mem0 = NDP10X_DNN_CONFIG_MEM0_PG_INRAM_MASK_INSERT
                        (st->mem0, v);
                }
            }
        }
    }
    
 out:
    return s;
}

int
syntiant_ndp10x_config_get_filter_bins(struct syntiant_ndp_device_s *ndp,
                                       struct syntiant_ndp10x_config_s *config)
{
    int i, bins;
    uint32_t v, addr;
    int s = SYNTIANT_NDP_ERROR_NONE;

    addr = NDP10X_DSP_CONFIG_MELBINS0;
    bins = (int) (config->freq_frame_size + 1);
    for (i = 0; i < bins; i++) {
        s = syntiant_ndp10x_read(ndp, 1, addr, &v);
        if (s) goto out;
        if (0 < i) {
            config->filter_frequency[i - 1]
                = NDP10X_DSP_CONFIG_MELBINS0_FILTBIN00_EXTRACT(v) * 2;
        }
        i += 1;
        if (i < bins) {
            config->filter_frequency[i - 1]
                = NDP10X_DSP_CONFIG_MELBINS0_FILTBIN01_EXTRACT(v) * 2;
        }
        i += 1;
        if (i < bins) {
            config->filter_frequency[i - 1]
                = NDP10X_DSP_CONFIG_MELBINS0_FILTBIN02_EXTRACT(v) * 2;
        }
        addr += 4;
    }

 out:
    return s;
}

int
syntiant_ndp10x_config_get_filter_eq(struct syntiant_ndp_device_s *ndp,
                                     struct syntiant_ndp10x_config_s *config)
{
    int s = SYNTIANT_NDP_ERROR_NONE;
    uint32_t v = 0;
    uint8_t f;
    unsigned int i;

    for (i = 0; i < SYNTIANT_NDP10X_MAX_FREQUENCY_BINS; i++) {
        if (i % 4 == 0) {
            s = syntiant_ndp10x_read(ndp, 1, NDP10X_DSP_CONFIG_EQ(i >> 2), &v);
            if (s)
                goto out;
        }
        f = (uint8_t) NDP10X_DSP_CONFIG_EQ_TAP_EXTRACT(v, i & 3);
        config->filter_eq[i] = (int8_t) f;
    }

 out:
    return s;
}

int
syntiant_ndp10x_config_set_filter_eq(struct syntiant_ndp_device_s *ndp,
                                     struct syntiant_ndp10x_config_s *config,
                                     struct syntiant_ndp10x_config_state_s *st)
{
    int s = SYNTIANT_NDP_ERROR_NONE;
    uint32_t v = 0;
    uint8_t f;
    unsigned int i;
    
    if (!(config->set1 & SYNTIANT_NDP10X_CONFIG_SET1_FILTER_EQ)) {
        goto out;
    }


    for (i = 0; i < SYNTIANT_NDP10X_MAX_FREQUENCY_BINS; i++) {
        if (config->filter_eq[i] < -128 || 127 < config->filter_eq[i]) {
            s = SYNTIANT_NDP_ERROR_ARG;
            goto out;
        }
    }

    for (i = 0; i < SYNTIANT_NDP10X_MAX_FREQUENCY_BINS; i++) {
        f = (uint8_t) config->filter_eq[i];
        v = NDP10X_DSP_CONFIG_EQ_TAP_MASK_INSERT(v, i & 3, (unsigned int) f);
        if (i % 4 == 3) {
            s = syntiant_ndp10x_write(ndp, 1, NDP10X_DSP_CONFIG_EQ(i >> 2), v);
            if (s)
                goto out;
        }
    }

    st->eq_update = 1;
 out:
    return s;
}

int
syntiant_ndp10x_config_read_state(struct syntiant_ndp_device_s *ndp,
                                  struct syntiant_ndp10x_config_state_s *st)
{
    int s;
    unsigned int i;
    struct syntiant_ndp10x_device_s *ndp10x = &ndp->d.ndp10x;
    uint32_t addr;
    
    if (ndp10x->fw_state_addr) {
        addr = ndp10x->fw_state_addr
            + (uint32_t) offsetof(struct ndp10x_fw_state_s, enable);
        s = syntiant_ndp10x_read(ndp, 1, addr, &st->fw_enable);
        if (s)
            goto out;
        
        addr = ndp10x->fw_agc_addr
            + (uint32_t) offsetof(struct ndp10x_agc_state_s,
                                  max_adjustment_gain);
        s = syntiant_ndp10x_read(ndp, 1, addr, &st->fw_max_adjustment_gain);
        if (s)
            goto out;
        
        addr = ndp10x->fw_agc_addr
            + (uint32_t) offsetof(struct ndp10x_agc_state_s, nom_speech_quiet);
        s = syntiant_ndp10x_read(ndp, 1, addr, &st->fw_nom_speech_quiet);
        if (s)
            goto out;

        addr = ndp10x->fw_state_addr
            + (uint32_t) offsetof(struct ndp10x_fw_state_s,
                                  mb_state.enable_match_for_every_frame);
        s = syntiant_ndp10x_read(ndp, 1, addr, &st->fw_match_per_frame);
        if (s)
            goto out;
    } else {
        st->fw_enable = 0;
        st->fw_max_adjustment_gain = 0;
        st->fw_nom_speech_quiet = 0;
        st->fw_match_per_frame = 0;
    }

    s = syntiant_ndp10x_read(ndp, 1, NDP10X_DNN_CONFIG_DNNCTL0, &st->dnnctl0);
    if (s)
        goto out;
    s = syntiant_ndp10x_read(ndp, 1, NDP10X_DNN_CONFIG_DNNCTL1, &st->dnnctl1);
    if (s)
        goto out;
    s = syntiant_ndp10x_read(ndp, 1, NDP10X_DNN_CONFIG_DNNCTL3, &st->dnnctl3);
    if (s)
        goto out;
    s = syntiant_ndp10x_read(ndp, 1, NDP10X_DNN_CONFIG_DNNCTL7, &st->dnnctl7);
    if (s)
        goto out;
    s = syntiant_ndp10x_read(ndp, 1, NDP10X_DSP_CONFIG_SMPLCTL, &st->smplctl);
    if (s)
        goto out;
    s = syntiant_ndp10x_read(ndp, 1, NDP10X_DSP_CONFIG_SMPLMARK, &st->smplmark);
    if (s)
        goto out;
    s = syntiant_ndp10x_read(ndp, 1, NDP10X_DSP_CONFIG_SMPLSTS, &st->smplsts);
    if (s)
        goto out;
    s = syntiant_ndp10x_read(ndp, 1, NDP10X_DSP_CONFIG_TANK, &st->tank);
    if (s)
        goto out;
    s = syntiant_ndp10x_read(ndp, 1, NDP10X_DSP_CONFIG_I2SCTL, &st->i2sctl);
    if (s)
        goto out;
    s = syntiant_ndp10x_read(ndp, 1, NDP10X_DSP_CONFIG_PDMCTL, &st->pdmctl);
    if (s)
        goto out;
    for (i = 0; i < 2; i++) {
        s = syntiant_ndp10x_read(ndp, 1, NDP10X_DSP_CONFIG_PDMCFG(i),
                                 &st->pdmcfg[i]);
        if (s)
            goto out;
    }
    s = syntiant_ndp10x_read(ndp, 1, NDP10X_CHIP_CONFIG_CLKCTL0, &st->clkctl0);
    if (s)
        goto out;
    s = syntiant_ndp10x_read(ndp, 1, NDP10X_CHIP_CONFIG_CLKCTL1, &st->clkctl1);
    if (s)
        goto out;
    s = syntiant_ndp10x_read(ndp, 1, NDP10X_CHIP_CONFIG_CLKCTL2, &st->clkctl2);
    if (s)
        goto out;
    s = syntiant_ndp10x_read(ndp, 1, NDP10X_DSP_CONFIG_FREQCTL, &st->freqctl);
    if (s)
        goto out;
    s = syntiant_ndp10x_read(ndp, 1, NDP10X_CHIP_CONFIG_MEM, &st->mem);
    if (s)
        goto out;
    s = syntiant_ndp10x_read(ndp, 1, NDP10X_DNN_CONFIG_MEM0, &st->mem0);
    if (s)
        goto out;
    s = syntiant_ndp10x_read(ndp, 1, NDP10X_DNN_CONFIG_MEM1, &st->mem1);
    if (s)
        goto out;
    s = syntiant_ndp10x_read(ndp, 1, NDP10X_DNN_CONFIG_MEM3, &st->mem3);
    if (s)
        goto out;
    s = syntiant_ndp10x_read(ndp, 0, NDP10X_SPI_CTL, &st->ctl);
    if (s)
        goto out;

 out:
    return s;
}

int
syntiant_ndp10x_config_update_state(struct syntiant_ndp_device_s *ndp,
                                    struct syntiant_ndp10x_config_state_s *st0,
                                    struct syntiant_ndp10x_config_state_s *st)
{
    struct syntiant_ndp10x_device_s *ndp10x = &ndp->d.ndp10x;
    uint32_t addr;
    int s = SYNTIANT_NDP_ERROR_NONE;
    int do_reset;
    unsigned int i;

    do_reset = st0->dnnctl0 != st->dnnctl0 || st0->dnnctl1 != st->dnnctl1
        || st0->dnnctl3 != st->dnnctl3 || st0->dnnctl7 != st->dnnctl7
        || st0->freqctl != st->freqctl || st0->smplctl != st->smplctl
        || st0->smplmark != st->smplmark
        || st->eq_update;

    if (do_reset) {
        st0->pdmctl = NDP10X_DSP_CONFIG_PDMCTL_RSTB_MASK_INSERT
            (st0->pdmctl, 0);
        s = syntiant_ndp10x_write(ndp, 1, NDP10X_DSP_CONFIG_PDMCTL,
                                  st0->pdmctl);
        if (s)
            goto out;

        st0->freqctl = NDP10X_DSP_CONFIG_FREQCTL_RSTB_MASK_INSERT
            (st0->freqctl, 0);
        s = syntiant_ndp10x_write(ndp, 1, NDP10X_DSP_CONFIG_FREQCTL,
                                  st0->freqctl);
        if (s)
            goto out;

        st0->dnnctl0 = NDP10X_DNN_CONFIG_DNNCTL0_RSTB_MASK_INSERT
            (st0->dnnctl0, 0);
        s = syntiant_ndp10x_write(ndp, 1, NDP10X_DNN_CONFIG_DNNCTL0,
                                  st0->dnnctl0);
        if (s)
            goto out;
    }

    if (st0->tank != st->tank) {
        /* reset the tank every time configuration changes */
        s = syntiant_ndp10x_write(ndp, 1, NDP10X_DSP_CONFIG_TANK,
                                  NDP10X_DSP_CONFIG_TANK_INIT(1)
                                  | NDP10X_DSP_CONFIG_TANK_ENABLE(0));
        if (s)
            goto out;
        if (ndp10x->fw_state_addr) {
            /* notify firmware to reset tank pointer */
            addr = ndp10x->fw_state_addr
                + (uint32_t) offsetof(struct ndp10x_fw_state_s, tankptr);
            s = syntiant_ndp10x_write(ndp, 1, addr, 0);
            if (s)
                goto out;
            addr = ndp10x->fw_state_addr
                + (uint32_t) offsetof(struct ndp10x_fw_state_s, tank_reset);
            s = syntiant_ndp10x_write(ndp, 1, addr, 1);
            if (s)
                goto out;
        }
        ndp10x->tankptr_last = 0;
        s = syntiant_ndp10x_write(ndp, 1, NDP10X_DSP_CONFIG_TANK, st->tank);
        if (s)
            goto out;
    }

    if (st0->mem != st->mem) {
        s = syntiant_ndp10x_write(ndp, 1, NDP10X_CHIP_CONFIG_MEM, st->mem);
        if (s)
            goto out;
    }
    
    if (st0->mem0 != st->mem0) {
        s = syntiant_ndp10x_write(ndp, 1, NDP10X_DNN_CONFIG_MEM0, st->mem0);
        if (s)
            goto out;
    }
    
    if (st0->mem3 != st->mem3) {
        s = syntiant_ndp10x_write(ndp, 1, NDP10X_DNN_CONFIG_MEM3, st->mem3);
        if (s)
            goto out;
    }
    
    if (st0->mem1 != st->mem1) {
        s = syntiant_ndp10x_write(ndp, 1, NDP10X_DNN_CONFIG_MEM1, st->mem1);
        if (s)
            goto out;
    }
    
    if (st0->fw_enable != st->fw_enable) {
        addr = ndp10x->fw_state_addr
            + (uint32_t) offsetof(struct ndp10x_fw_state_s, enable);
        s = syntiant_ndp10x_write(ndp, 1, addr, st->fw_enable);
        if (s)
            goto out;
        
    }
        
    if (st0->fw_max_adjustment_gain != st->fw_max_adjustment_gain) {
        addr = ndp10x->fw_agc_addr
            + (uint32_t) offsetof(struct ndp10x_agc_state_s,
                                  max_adjustment_gain);
        s = syntiant_ndp10x_write(ndp, 1, addr, st->fw_max_adjustment_gain);
        if (s)
            goto out;
    }
    
    if (st0->fw_nom_speech_quiet != st->fw_nom_speech_quiet) {
        addr = ndp10x->fw_agc_addr
            + (uint32_t) offsetof(struct ndp10x_agc_state_s, nom_speech_quiet);
        s = syntiant_ndp10x_write(ndp, 1, addr, st->fw_nom_speech_quiet);
        if (s)
            goto out;
    }
    
    if (st0->fw_match_per_frame != st->fw_match_per_frame) {
        addr = ndp10x->fw_state_addr
            + (uint32_t) offsetof(struct ndp10x_fw_state_s,
                                  mb_state.enable_match_for_every_frame);
        s = syntiant_ndp10x_write(ndp, 1, addr, st->fw_match_per_frame);
        if (s)
            goto out;
    }

    if (st0->dnnctl0 != st->dnnctl0) {
        s = syntiant_ndp10x_write(ndp, 1, NDP10X_DNN_CONFIG_DNNCTL0,
                                  st->dnnctl0);
        if (s)
            goto out;
    }

    if (st0->dnnctl1 != st->dnnctl1) {
        s = syntiant_ndp10x_write(ndp, 1, NDP10X_DNN_CONFIG_DNNCTL1,
                                  st->dnnctl1);
        if (s)
            goto out;
    }

    if (st0->dnnctl3 != st->dnnctl3) {
        s = syntiant_ndp10x_write(ndp, 1, NDP10X_DNN_CONFIG_DNNCTL3,
                                  st->dnnctl3);
        if (s)
            goto out;
    }

    if (st0->dnnctl7 != st->dnnctl7) {
        s = syntiant_ndp10x_write(ndp, 1, NDP10X_DNN_CONFIG_DNNCTL7,
                                  st->dnnctl7);
        if (s)
            goto out;
    }

    if (st0->freqctl != st->freqctl) {
        s = syntiant_ndp10x_write(ndp, 1, NDP10X_DSP_CONFIG_FREQCTL,
                                  st->freqctl);
        if (s)
            goto out;
    }

    if (st0->pdmctl != st->pdmctl) {
        s = syntiant_ndp10x_write(ndp, 1, NDP10X_DSP_CONFIG_PDMCTL, st->pdmctl);
        if (s)
            goto out;
    }

    for(i = 0; i < 2; i++) {
        if (st0->pdmcfg[i] != st->pdmcfg[i]) {
            s = syntiant_ndp10x_write(ndp, 1, NDP10X_DSP_CONFIG_PDMCFG(i),
                                      st->pdmcfg[i]);
            if (s)
                goto out;
        }
    }

    if (st0->i2sctl != st->i2sctl) {
        s = syntiant_ndp10x_write(ndp, 1, NDP10X_DSP_CONFIG_I2SCTL, st->i2sctl);
        if (s)
            goto out;
    }

    if (st0->smplctl != st->smplctl) {
        s = syntiant_ndp10x_write(ndp, 1, NDP10X_DSP_CONFIG_SMPLCTL,
                                  st->smplctl);
        if (s)
            goto out;
    }

    if (st0->smplmark != st->smplmark) {
        s = syntiant_ndp10x_write(ndp, 1, NDP10X_DSP_CONFIG_SMPLMARK,
                                  st->smplmark);
        if (s)
            goto out;
    }

    if (st0->clkctl0 != st->clkctl0) {
        s = syntiant_ndp10x_write(ndp, 1, NDP10X_CHIP_CONFIG_CLKCTL0,
                                  st->clkctl0);
        if (s)
            goto out;
    }

    if (st0->clkctl1 != st->clkctl1) {
        s = syntiant_ndp10x_write(ndp, 1, NDP10X_CHIP_CONFIG_CLKCTL1,
                                  st->clkctl1);
        if (s)
            goto out;
    }

    if (st0->clkctl2 != st->clkctl2) {
        s = syntiant_ndp10x_write(ndp, 1, NDP10X_CHIP_CONFIG_CLKCTL2,
                                  st->clkctl2);
        if (s)
            goto out;
    }

    if (do_reset) {
        s = syntiant_ndp10x_write(ndp, 0, NDP10X_SPI_INTSTS,
                                  NDP10X_SPI_INTSTS_MATCH_INT(1)
                                  | NDP10X_SPI_INTSTS_DNN_INT(1)
                                  | NDP10X_SPI_INTSTS_FREQ_INT(1)
                                  | NDP10X_SPI_INTSTS_AE_INT(1)
                                  | NDP10X_SPI_INTSTS_WM_INT(1));
        if (s) goto out;
    }

 out:
    return s;
}


int
syntiant_ndp10x_config_no_sync(struct syntiant_ndp_device_s *ndp,
                               struct syntiant_ndp10x_config_s *config)
{

    struct syntiant_ndp10x_config_state_s st, st0;
    struct syntiant_ndp10x_device_s *ndp10x = &ndp->d.ndp10x;

    int s = SYNTIANT_NDP_ERROR_NONE;

    if (config->set & (unsigned int) ~SYNTIANT_NDP10X_CONFIG_SET_ALL_M) {
        s = SYNTIANT_NDP_ERROR_ARG;
        goto error;
    }

    if (!config->set && !config->get_all) {
        config->dnn_input = ndp10x->dnn_input;
        config->tank_bits = ndp10x->tank_bits;
        config->tank_input = ndp10x->tank_input;
        config->tank_size = ndp10x->tank_size;
        config->audio_frame_size = ndp10x->audio_frame_size;
        config->audio_frame_step = ndp10x->audio_frame_step;
        config->freq_frame_size = ndp10x->freq_frame_size;
        config->dnn_frame_size = ndp10x->dnn_frame_size;
        goto error;
    }

    s = syntiant_ndp10x_config_read_state(ndp, &st);
    if (s)
        goto error;

    st.tank_max_size = ndp10x->tank_max_size;
    st.fw_state_addr = ndp10x->fw_state_addr;
    st.input_clock_rate = ndp10x->input_clock_rate;
    st.eq_update = 0;

    st0 = st;

    s = syntiant_ndp10x_config_set_clock(ndp, config, &st);
    if (s)
        goto error;

    s = syntiant_ndp10x_config_set_input(config, &st);
    if (s)
        goto error;

    s = syntiant_ndp10x_config_set_fw_state(config, &st);
    if (s)
        goto error;
    
    s = syntiant_ndp10x_config_set_memory_power(config, &st);
    if (s)
        goto error;

    s = syntiant_ndp10x_config_set_filter_eq(ndp, config, &st);
    if (s)
        goto error;

    s = syntiant_ndp10x_config_update_state(ndp, &st0, &st);
    if (s)
       goto error;

    syntiant_ndp10x_config_get_memory_power(&st, config);
    
    syntiant_ndp10x_config_get_fw_state(&st, config);
    
    syntiant_ndp10x_config_get_input(&st, config, ndp10x);

    syntiant_ndp10x_config_get_clock(&st, config);
    ndp10x->input_clock_rate = st.input_clock_rate;

    config->fw_pointers_addr = ndp10x->fw_pointers_addr;

    if (config->get_all) {
        s = syntiant_ndp10x_config_get_filter_bins(ndp, config);
        if (s)
            goto error;
        s = syntiant_ndp10x_config_get_filter_eq(ndp, config);
        if (s)
            goto error;
    }

 error:
    return s;
}

int
syntiant_ndp10x_config(struct syntiant_ndp_device_s *ndp,
    struct syntiant_ndp10x_config_s *config)
{
    int s, s0;

    if (!ndp || !ndp->init) {
        s = SYNTIANT_NDP_ERROR_UNINIT;
        goto error;
    }

    s = (ndp->iif.sync)(ndp->iif.d);
    if (s) goto error;

    s = syntiant_ndp10x_config_no_sync(ndp, config);

    s0 = (ndp->iif.unsync)(ndp->iif.d);
    s = s ? s : s0;
error:
    return s;
}

int
syntiant_ndp10x_posterior_init(
    struct syntiant_ndp_device_s *ndp, uint32_t states, uint32_t classes, uint32_t ph_type)
{
    uint32_t ph_params_addr = ndp->d.ndp10x.fw_posterior_parameters_addr;
    struct ndp10x_ph_state_params_s sparams;
    struct ndp10x_ph_class_params_s cparams;
    uint32_t class_base_for_state_offset;
    uint32_t params_base_addr, saddr, caddr;

    unsigned int i, j;
    int s;
    uint32_t offset = 0x0;

    if (!ph_params_addr) {
        s = SYNTIANT_NDP_ERROR_FAIL;
        goto error;
    }

    sparams.timeout = 0;
    sparams.timeout_action = 0;

    cparams.window = 0;
    cparams.threshold = 65535;
    cparams.backoff = 0;
    cparams.smoothing_queue_size = 1;

    offset = offsetof(struct ndp10x_ph_params_s, params_memory);

    params_base_addr = ph_params_addr + offset;

    class_base_for_state_offset
        = states
        * ((unsigned int) sizeof(struct ndp10x_ph_state_params_s))
        / ((unsigned int) sizeof(uint32_t));

    for (i = 0; i < states; i++) {

        class_base_for_state_offset
            += i * (classes * ((unsigned int) sizeof(cparams))
                    / ((unsigned int) sizeof(uint32_t)));
        sparams.class_params_offset = class_base_for_state_offset;
        saddr = params_base_addr + i * ((unsigned int) sizeof(sparams));

        s = syntiant_ndp10x_write_block(
            ndp, 1, saddr, &sparams, sizeof(sparams));
        if (s) goto error;

        for (j = 0; j < classes; j++) {
            cparams.action = NDP10X_PH_ACTION_MATCH_M | j;
            caddr = params_base_addr + class_base_for_state_offset
                * ((unsigned int) sizeof(uint32_t))
                + j * ((unsigned int) sizeof(cparams));
            s = syntiant_ndp10x_write_block(
                ndp, 1, caddr, &cparams, sizeof(cparams));
            if (s) goto error;
        }
    }

    s = syntiant_ndp10x_write(ndp, 1,
        ph_params_addr + offsetof(struct ndp10x_ph_params_s, num_classes),
        classes);
    if (s) goto error;

    s = syntiant_ndp10x_write
        (ndp, 1, ph_params_addr
         + ((unsigned int) offsetof(struct ndp10x_ph_params_s, num_states)),
        states);
    if (s) goto error;

    s = syntiant_ndp10x_write(
        ndp, 1,
        ph_params_addr
            + (unsigned int) offsetof(struct ndp10x_ph_params_s, ph_type),
        ph_type);
    if (s) goto error;

error:
    return s;
}

int syntiant_ndp10x_posterior_config_encode_action(int timeout,
                                                   unsigned int states,
                                                   unsigned int atype,
                                                   unsigned int match,
                                                   unsigned int state,
                                                   uint32_t *action);
int
syntiant_ndp10x_posterior_config_encode_action(int timeout,
                                               unsigned int states,
                                               unsigned int atype,
                                               unsigned int match,
                                               unsigned int state,
                                               uint32_t *action)
{
    int s = SYNTIANT_NDP_ERROR_NONE;
    uint32_t a;
    
    switch (atype) {
    case SYNTIANT_NDP10X_POSTERIOR_CONFIG_ACTION_TYPE_MATCH:
        if (63 < match) {
            s = SYNTIANT_NDP_ERROR_ARG;
            goto error;
        }
        a = NDP10X_PH_ACTION_MATCH_M | match;
        break;
    case SYNTIANT_NDP10X_POSTERIOR_CONFIG_ACTION_TYPE_STATE:
        if (states <= state) {
            s = SYNTIANT_NDP_ERROR_ARG;
            goto error;
        }
        a = state;
        break;
    case SYNTIANT_NDP10X_POSTERIOR_CONFIG_ACTION_TYPE_STAY:
        if (timeout) {
            s = SYNTIANT_NDP_ERROR_ARG;
            goto error;
        }
        a = NDP10X_PH_ACTION_STAY;
        break;
    default:
        s = SYNTIANT_NDP_ERROR_ARG;
        goto error;
    }
    
    *action = a;
    
 error:
    return s;
}

void syntiant_ndp10x_posterior_config_decode_action(uint32_t action,
                                                    unsigned int *atype,
                                                    unsigned int *match,
                                                    unsigned int *state);
void
syntiant_ndp10x_posterior_config_decode_action(uint32_t action,
                                               unsigned int *atype,
                                               unsigned int *match,
                                               unsigned int *state)
{
    unsigned int at = 0;
    unsigned int m = 0;
    unsigned int st = 0;
    
    if (action & NDP10X_PH_ACTION_MATCH_M) {
        at = SYNTIANT_NDP10X_POSTERIOR_CONFIG_ACTION_TYPE_MATCH;
        m = action & ~((unsigned int) NDP10X_PH_ACTION_MATCH_M);
    } else if (action == NDP10X_PH_ACTION_STAY) {
        at = SYNTIANT_NDP10X_POSTERIOR_CONFIG_ACTION_TYPE_STAY;
    } else {
        at = SYNTIANT_NDP10X_POSTERIOR_CONFIG_ACTION_TYPE_STATE;
        st = action;
    }
    *atype = at;
    *match = m;
    *state = st;
}

void
syntiant_ndp10x_posterior_config_encode_action_V2(
    uint32_t *param, uint32_t action, uint32_t action_arg)
{
    uint32_t action_mask = 0x000000ff;
    uint32_t action_arg_mask = 0xffffff00;
    *param = ((action & action_mask) | (action_arg & action_arg_mask));
}

void
syntiant_ndp10x_posterior_config_decode_action_V2(
    uint32_t param, uint32_t *action, uint32_t *action_arg)
{
    uint32_t action_mask = 0x000000ff;
    uint32_t action_arg_mask = 0xffffff00;
    *action = param & action_mask;
    *action_arg = param & action_arg_mask;
}

int
syntiant_ndp10x_posterior_config_no_sync(struct syntiant_ndp_device_s *ndp,
    struct syntiant_ndp10x_posterior_config_s *config)
{
    struct syntiant_ndp10x_device_s *ndp10x = &ndp->d.ndp10x;
    uint32_t ph_params_addr = ndp10x->fw_posterior_parameters_addr;
    uint32_t states, classes, ph_type;
    uint32_t state_addr, params_addr;
    struct ndp10x_ph_state_params_s sparams;
    struct ndp10x_ph_class_params_s cparams;
    int s;
    uint32_t offset=0x0;

    if (config->set
        & ((unsigned int) ~SYNTIANT_NDP10X_POSTERIOR_CONFIG_SET_ALL_M)) {
        s = SYNTIANT_NDP_ERROR_ARG;
        goto error;
    }

    if ((config->set & (SYNTIANT_NDP10X_POSTERIOR_CONFIG_SET_STATES
                        | SYNTIANT_NDP10X_POSTERIOR_CONFIG_SET_CLASSES
                        | SYNTIANT_NDP10X_POSTERIOR_CONFIG_SET_PH_TYPE))
        && (config->set
            & ~((unsigned int)
                (SYNTIANT_NDP10X_POSTERIOR_CONFIG_SET_STATES
                 | SYNTIANT_NDP10X_POSTERIOR_CONFIG_SET_CLASSES
                 | SYNTIANT_NDP10X_POSTERIOR_CONFIG_SET_PH_TYPE)))) {
        s = SYNTIANT_NDP_ERROR_ARG;
        goto error;
    }

    if ((config->set & (SYNTIANT_NDP10X_POSTERIOR_CONFIG_SET_TIMEOUT
                        | SYNTIANT_NDP10X_POSTERIOR_CONFIG_SET_TIMEOUT_ACTION
                        | SYNTIANT_NDP10X_POSTERIOR_CONFIG_SET_TIMEOUT_ACTION_V2))
        && (config->set
            & ~((unsigned int)
                (SYNTIANT_NDP10X_POSTERIOR_CONFIG_SET_TIMEOUT
                 | SYNTIANT_NDP10X_POSTERIOR_CONFIG_SET_TIMEOUT_ACTION
                 | SYNTIANT_NDP10X_POSTERIOR_CONFIG_SET_TIMEOUT_ACTION_V2)))) {
        s = SYNTIANT_NDP_ERROR_ARG;
        goto error;
    }

    if (!ph_params_addr) {
        s = SYNTIANT_NDP_ERROR_FAIL;
        goto error;
    }

    offset = offsetof(struct ndp10x_ph_params_s, num_states);
    s = syntiant_ndp10x_read(ndp, 1, ph_params_addr + offset, &states);
    if (s) goto error;

    if (config->set & SYNTIANT_NDP10X_POSTERIOR_CONFIG_SET_STATES) {
        states = config->states;
    }

    offset = offsetof(struct ndp10x_ph_params_s, num_classes);
    s = syntiant_ndp10x_read(ndp, 1, ph_params_addr + offset, &classes);
    if (s) goto error;

    if (config->set & SYNTIANT_NDP10X_POSTERIOR_CONFIG_SET_CLASSES) {
        classes = config->classes;
    }

    offset = offsetof(struct ndp10x_ph_params_s, ph_type);
    s = syntiant_ndp10x_read(ndp, 1, ph_params_addr + offset, &ph_type);
    if (s) goto error;

    if (config->set & SYNTIANT_NDP10X_POSTERIOR_CONFIG_SET_PH_TYPE) {
        ph_type = config->ph_type;
    }

    if (config->set
        & (SYNTIANT_NDP10X_POSTERIOR_CONFIG_SET_STATES
              | SYNTIANT_NDP10X_POSTERIOR_CONFIG_SET_CLASSES
              | SYNTIANT_NDP10X_POSTERIOR_CONFIG_SET_PH_TYPE)) {
        s = syntiant_ndp10x_posterior_init(ndp, states, classes, ph_type);
        if (s) goto error;
        goto got_structure;
    }

    if (!config->set && !states) {
        goto got_structure;
    }

    if (states <= config->state) {
        s = SYNTIANT_NDP_ERROR_ARG;
        goto error;
    }

    offset = offsetof(struct ndp10x_ph_params_s, params_memory);
    state_addr = ph_params_addr + offset;
    state_addr += config->state * ((unsigned int) sizeof(sparams));
    s = syntiant_ndp10x_read_block(ndp, 1, state_addr, &sparams,
                                   sizeof(sparams));
    if (s) goto error;

    if (config->set & SYNTIANT_NDP10X_POSTERIOR_CONFIG_SET_TIMEOUT) {
        sparams.timeout = config->timeout;
    }

    if (config->set & SYNTIANT_NDP10X_POSTERIOR_CONFIG_SET_TIMEOUT_ACTION) {
        s = syntiant_ndp10x_posterior_config_encode_action
            (1, states, config->timeout_action_type,
             config->timeout_action_match, config->timeout_action_state,
             &sparams.timeout_action);
        if (s) goto error;
    }

    if (config->set & SYNTIANT_NDP10X_POSTERIOR_CONFIG_SET_TIMEOUT_ACTION_V2){
        syntiant_ndp10x_posterior_config_encode_action_V2(
            &sparams.timeout_action, config->timeout_action_type,
            config->timeout_action_arg);
    }

    if (!(config->set
          & ~(unsigned int)
          (SYNTIANT_NDP10X_POSTERIOR_CONFIG_SET_TIMEOUT
           | SYNTIANT_NDP10X_POSTERIOR_CONFIG_SET_TIMEOUT_ACTION))
        && !classes) {
        goto got_state;
    }

    if (classes <= config->class_index) {
        s = SYNTIANT_NDP_ERROR_ARG;
        goto error;
    }

    offset = offsetof(struct ndp10x_ph_params_s, params_memory);

    params_addr = ph_params_addr + offset
        + sparams.class_params_offset * ((unsigned int) sizeof(uint32_t))
        + config->class_index * ((unsigned int) sizeof(cparams));
    s = syntiant_ndp10x_read_block(
        ndp, 1, params_addr, &cparams, sizeof(cparams));
    if (s) goto error;

    if (config->set & SYNTIANT_NDP10X_POSTERIOR_CONFIG_SET_THRESHOLD) {
        cparams.threshold = config->threshold;
    }

    if (config->set & SYNTIANT_NDP10X_POSTERIOR_CONFIG_SET_WINDOW) {
        cparams.window = config->window;
    }

    if (config->set & SYNTIANT_NDP10X_POSTERIOR_CONFIG_SET_BACKOFF) {
        cparams.backoff = config->backoff;
    }

    if (config->set & SYNTIANT_NDP10X_POSTERIOR_CONFIG_SET_SM_QUEUE_SIZE) {
        if (NDP10X_RESULT_SOFTMAX_SMOOTHER_MAX_QUEUE_SIZE
            < config->smoothing_queue_size) {
            s = SYNTIANT_NDP_ERROR_ARG;
            goto error;
        }
        cparams.smoothing_queue_size = config->smoothing_queue_size;
    }

    if (config->set & SYNTIANT_NDP10X_POSTERIOR_CONFIG_SET_ACTION) {
        s = syntiant_ndp10x_posterior_config_encode_action
            (0, states, config->action_type, config->action_match,
             config->action_state, &cparams.action);
        if (s) goto error;
    }

    if (config->set & SYNTIANT_NDP10X_POSTERIOR_CONFIG_SET_ACTION_V2) {
        syntiant_ndp10x_posterior_config_encode_action_V2(
            &cparams.action, config->action_type,
            config->action_arg);
    }

    if (config->set & (SYNTIANT_NDP10X_POSTERIOR_CONFIG_SET_THRESHOLD
                          | SYNTIANT_NDP10X_POSTERIOR_CONFIG_SET_WINDOW
                          | SYNTIANT_NDP10X_POSTERIOR_CONFIG_SET_BACKOFF
                          | SYNTIANT_NDP10X_POSTERIOR_CONFIG_SET_ACTION
                          | SYNTIANT_NDP10X_POSTERIOR_CONFIG_SET_ACTION_V2
                          | SYNTIANT_NDP10X_POSTERIOR_CONFIG_SET_SM_QUEUE_SIZE))
    {
        s = syntiant_ndp10x_write_block(
            ndp, 1, params_addr, &cparams, sizeof(cparams));
        if (s) goto error;
    }

    config->threshold = cparams.threshold;
    config->window = cparams.window;
    config->backoff = cparams.backoff;
    config->smoothing_queue_size = cparams.smoothing_queue_size;
    
    if (ph_type == 1) {
        syntiant_ndp10x_posterior_config_decode_action_V2(
            cparams.action, &config->action_type, &config->action_arg);
            /* set unused params to garbage */
            config->action_match = 0xffffffff;
            config->action_state = 0xffffffff;
    } else {
        syntiant_ndp10x_posterior_config_decode_action(cparams.action,
                                                   &config->action_type,
                                                   &config->action_match,
                                                   &config->action_state);
        config->action_arg = 0xffffffff;
    }

                                                   
got_state:
    if (config->set
        & (SYNTIANT_NDP10X_POSTERIOR_CONFIG_SET_TIMEOUT
           | SYNTIANT_NDP10X_POSTERIOR_CONFIG_SET_TIMEOUT_ACTION
           | SYNTIANT_NDP10X_POSTERIOR_CONFIG_SET_TIMEOUT_ACTION_V2)) {
        s = syntiant_ndp10x_write_block(
            ndp, 1, state_addr, &sparams, sizeof(sparams));
        if (s) goto error;
    }
    config->timeout = sparams.timeout;
    if (ph_type == 1) {
        syntiant_ndp10x_posterior_config_decode_action_V2(
            sparams.timeout_action, &config->timeout_action_type,
            &config->timeout_action_arg);
        config->timeout_action_state = 0xffffffff;
        config->timeout_action_match = 0xffffffff;
    } else {
        syntiant_ndp10x_posterior_config_decode_action(
            sparams.timeout_action, &config->timeout_action_type,
            &config->timeout_action_match, &config->timeout_action_state);
        config->timeout_action_arg = 0xffffffff;
    }

got_structure:
    config->states = states;
    config->classes = classes;
    config->ph_type = ph_type;

error:
    return s;
}

int
syntiant_ndp10x_posterior_config(struct syntiant_ndp_device_s *ndp,
    struct syntiant_ndp10x_posterior_config_s *config)
{
    int s, s0;

    if (!ndp || !ndp->init) {
        s = SYNTIANT_NDP_ERROR_UNINIT;
        goto error;
    }

    s = (ndp->iif.sync)(ndp->iif.d);
    if (s) goto error;

    s = syntiant_ndp10x_posterior_config_no_sync(ndp, config);

    s0 = (ndp->iif.unsync)(ndp->iif.d);
    s = s ? s : s0;
error:
    return s;
}

int
syntiant_ndp10x_gpio(
    struct syntiant_ndp_device_s *ndp, struct syntiant_ndp10x_gpio_s *gpio)
{
    uint32_t reg, reg_val, old_reg_val;
    int s = SYNTIANT_NDP_ERROR_NONE;

    if (gpio->set & ~((unsigned int) SYNTIANT_NDP10X_GPIO_SET_ALL_M)) {
        s = SYNTIANT_NDP_ERROR_ARG;
        goto error;
    }

    if (gpio->gpio > 7) {
        s = SYNTIANT_NDP_ERROR_ARG;
        goto error;
    }

    s = syntiant_ndp_read(ndp, 1, NDP10X_CHIP_CONFIG_GPIO, &reg_val);
    if (s)
        goto error;
    if (gpio->set & SYNTIANT_NDP10X_GPIO_SET_INPUT_ENABLE) {
        reg_val = NDP10X_CHIP_CONFIG_GPIO_GPIOIE_MASK_INSERT
            (reg_val, (unsigned int) !!gpio->input_enable);
        s = syntiant_ndp_write(ndp, 1, NDP10X_CHIP_CONFIG_GPIO, reg_val);
        if (s)
            goto error;
    }
    gpio->input_enable = NDP10X_CHIP_CONFIG_GPIO_GPIOIE_EXTRACT(reg_val);

    if (gpio->set & SYNTIANT_NDP10X_GPIO_SET_VALUE) {
        reg = NDP10X_GPIO_DATAOUT;
        s = syntiant_ndp_read(ndp, 1, reg, &reg_val);
        if (s) goto error;

        old_reg_val = reg_val;
        if (gpio->value) {
            reg_val |= (1U << gpio->gpio);
        } else {
            reg_val &= ~(1U << gpio->gpio);
        }

        if (reg_val != old_reg_val) {
            s = syntiant_ndp_write(ndp, 1, reg, reg_val);
            if (s) goto error;
        }
    }

    if (gpio->set & SYNTIANT_NDP10X_GPIO_SET_MODE) {
        if (gpio->mode > SYNTIANT_NDP10X_GPIO_MODE_MAX) {
            s = SYNTIANT_NDP_ERROR_ARG;
            goto error;
        }

        if (gpio->mode == SYNTIANT_NDP10X_GPIO_MODE_OUTPUT) {
            reg = NDP10X_GPIO_OUTSET;
        } else {
            reg = NDP10X_GPIO_OUTCLR;
        }
        reg_val = 1U << gpio->gpio;
        s = syntiant_ndp_write(ndp, 1, reg, reg_val);
        if (s) goto error;
    } else {
        /* return current I/O mode */
        reg = NDP10X_GPIO_OUTSET;
        s = syntiant_ndp_read(ndp, 1, reg, &reg_val);
        if (s) goto error;

        if (reg_val & (1U << gpio->gpio)) {
            gpio->mode = 1;
        } else {
            gpio->mode = 0;
        }
    }

    if (!(gpio->set & SYNTIANT_NDP10X_GPIO_SET_VALUE)) {
        s = syntiant_ndp_read(ndp, 1, NDP10X_GPIO_DATA, &reg_val);
        if (s) goto error;

        if (reg_val & (1U << gpio->gpio)) {
            gpio->value = 1;
        } else {
            gpio->value = 0;
        }
    }

error:
    return s;
}

int
syntiant_ndp10x_status(struct syntiant_ndp_device_s *ndp,
                       struct syntiant_ndp10x_status_s *status)
{
    struct syntiant_ndp10x_device_s *ndp10x = &ndp->d.ndp10x;
    uint32_t a, v;
    int s = SYNTIANT_NDP_ERROR_NONE;

    if (status->set & ~((unsigned int) SYNTIANT_NDP10X_STATUS_SET_ALL_M)) {
        s = SYNTIANT_NDP_ERROR_ARG;
        goto error;
    }

    a = ndp10x->fw_state_addr
        + (unsigned int) offsetof(struct ndp10x_fw_state_s,
                                  mb_state.m2h_match_skipped);

    if (status->set & SYNTIANT_NDP10X_STATUS_SET_MAILBOX_TRACE) {
        syntiant_ndp10x_mailbox_trace = status->mailbox_trace;
    }

    status->mailbox_trace = syntiant_ndp10x_mailbox_trace;
    status->h2m_mailbox_req
        = syntiant_ndp10x_mailbox_req_number[NDP10X_H2M];
    status->h2m_mailbox_rsp
        = syntiant_ndp10x_mailbox_rsp_number[NDP10X_H2M];
    status->h2m_mailbox_unexpected
        = syntiant_ndp10x_mailbox_unexpected[NDP10X_H2M];
    status->h2m_mailbox_error
        = syntiant_ndp10x_mailbox_error[NDP10X_H2M];
    status->m2h_mailbox_req
        = syntiant_ndp10x_mailbox_req_number[NDP10X_M2H];
    status->m2h_mailbox_rsp
        = syntiant_ndp10x_mailbox_rsp_number[NDP10X_M2H];
    status->m2h_mailbox_unexpected
        = syntiant_ndp10x_mailbox_unexpected[NDP10X_M2H];
    status->m2h_mailbox_error
        = syntiant_ndp10x_mailbox_error[NDP10X_M2H];

    if (ndp10x->fw_state_addr) {
        s = syntiant_ndp_read(ndp, 1, a, &v);
        if (s)
            goto error;
        status->missed_frames = v;
    } else {
        status->missed_frames = 0;
    }

    if (status->set & SYNTIANT_NDP10X_STATUS_SET_CLEAR) {
        memset(syntiant_ndp10x_mailbox_req_number, 0,
               sizeof(syntiant_ndp10x_mailbox_req_number));
        memset(syntiant_ndp10x_mailbox_rsp_number, 0,
               sizeof(syntiant_ndp10x_mailbox_rsp_number));
        memset(syntiant_ndp10x_mailbox_unexpected, 0,
               sizeof(syntiant_ndp10x_mailbox_unexpected));
        memset(syntiant_ndp10x_mailbox_error, 0,
               sizeof(syntiant_ndp10x_mailbox_error));
        if (ndp10x->fw_state_addr) {
            s = syntiant_ndp_write(ndp, 1, a, 0);
            if (s)
                goto error;
        }
    }

 error:
    return s;
}

uint32_t syntiant_ndp10x_device_types[] = {
    0x13, /* internal NDP101-ES */
    0x14, /* internal NDP101-ES */
    0x18, /* internal NDP100-ES */
    0x20, /* NDP101B0 QFN 32 */
    0x24, /* NDP100B0 WLBGA12 */
    0x2, /* test/simulation */
    0x0  /* end of list */
};

struct syntiant_ndp_driver_s syntiant_ndp10x_driver = {
    syntiant_ndp10x_device_types, syntiant_ndp10x_init, syntiant_ndp10x_uninit,
    syntiant_ndp10x_op_size, syntiant_ndp10x_interrupts, syntiant_ndp10x_poll,
    syntiant_ndp10x_load, syntiant_ndp10x_get_config, syntiant_ndp10x_send_data,
    syntiant_ndp10x_extract_data, syntiant_ndp10x_get_match_summary,
    syntiant_ndp10x_get_match_binary, syntiant_ndp10x_get_match_strength,
    syntiant_ndp10x_read_block, syntiant_ndp10x_write_block,
};

int
syntiant_ndp10x_debug_extract(
    struct syntiant_ndp_device_s *ndp, int type, void *data, unsigned int *len)
{

    struct syntiant_ndp10x_device_s *ndp10x = &ndp->d.ndp10x;
    unsigned int l = *len;
    uint32_t addr;
    int s = SYNTIANT_NDP_ERROR_ARG;

    if (!ndp10x->fw_state_addr) {
        s = SYNTIANT_NDP_ERROR_UNINIT;
        goto error;
    }

    switch (type) {
    case SYNTIANT_NDP10X_DEBUG_EXTRACT_TYPE_FW_STATE:
        if (l != sizeof(struct ndp10x_fw_state_s)) {
            goto error;
        }
        addr = ndp10x->fw_state_addr;
        break;
    case SYNTIANT_NDP10X_DEBUG_EXTRACT_TYPE_AGC_STATE:
        if (l != sizeof(struct ndp10x_agc_state_s)) {
            goto error;
        }
        addr = ndp10x->fw_agc_addr;
        break;
    case SYNTIANT_NDP10X_DEBUG_EXTRACT_TYPE_PH_STATE:
        if (l != sizeof(struct ndp10x_ph_state_s)) {
            goto error;
        }
        addr = ndp10x->fw_posterior_state_addr;
        break;
    default:
        goto error;
    }

    s = syntiant_ndp_read_block(ndp, 1, addr, data, l);

error:
    return s;
}

struct syntiant_ndp_driver_s* syntiant_ndp_get_driver(void)
{
    return &syntiant_ndp10x_driver;
}
