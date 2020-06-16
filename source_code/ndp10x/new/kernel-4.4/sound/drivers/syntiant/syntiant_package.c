/*
 SYNTIANT CONFIDENTIAL
 _____________________

 Copyright (c) 2017-2020 Syntiant Corporation
 All Rights Reserved.

 NOTICE:  All information contained herein is, and remains the property of
 Syntiant Corporation and its suppliers, if any.  The intellectual and
 technical concepts contained herein are proprietary to Syntiant Corporation 
and its suppliers and may be covered by U.S. and Foreign Patents, patents in
 process, and are protected by trade secret or copyright law.  Dissemination of
 this information or reproduction of this material is strictly forbidden unless
 prior written permission is obtained from Syntiant Corporation.
 */

#include <syntiant_ilib/syntiant_portability.h>

#include <syntiant_packager/syntiant_package.h>
#include <syntiant_packager/syntiant_package_consts.h>
#include <syntiant_packager/syntiant_package_tags.h>

#define maximum(a,b) (a < b) ? a : b

void
syntiant_pkg_parser_init(syntiant_pkg_parser_state_t *pstate)
{
    pstate->magic_header_found = 0;
    pstate->ptr = pstate->open_ram_begin;
    pstate->calc_crc = crc32_no_lib_init();
    pstate->expected_bytes = 0;
    pstate->mode = PACKAGE_MODE_TAG_START;
    pstate->partially_read_length = 0;
    pstate->is_current_params_package = 0;
    pstate->is_current_fw_package = 0;
    pstate->is_current_dsp_fw_package = 0;
}

void
syntiant_pkg_parser_reset(syntiant_pkg_parser_state_t *pstate)
{
    pstate->ptr = pstate->open_ram_begin;
}

int
syntiant_pkg_check_header_tlv(uint8_t *tlv)
{
    uint8_t* index = tlv;
    if ( *((uint32_t*)index) != TAG_HEADER) return 0;
    index += sizeof(uint32_t);

    if (*((uint32_t*)index) != sizeof(uint32_t)) return 0;
    index += sizeof(uint32_t);

    if (*((uint32_t*)index) != MAGIC_VALUE) return 0;
    return 1;
}

unsigned int
syntiant_pkg_read_chunk_data(syntiant_pkg_parser_state_t *pstate,
                            uint8_t* dest, uint32_t max_length, int fake, int
                            calc_crc, int copy)
{
    uint8_t* ptr = pstate->ptr;
    uint8_t* end = pstate->open_ram_end;
    unsigned int available_bytes, bytes_to_copy;

    available_bytes = (unsigned int) (end - ptr);

    switch (pstate->mode) {
    case PACKAGE_MODE_TAG_START:
    case PACKAGE_MODE_LENGTH_START:
    case PACKAGE_MODE_VALUE_START:
        bytes_to_copy = max_length;
        break;
    default:
        bytes_to_copy = pstate->expected_bytes;
        dest =  dest + (max_length - bytes_to_copy);
    }

    if (bytes_to_copy > available_bytes) {
        pstate->expected_bytes = bytes_to_copy - available_bytes;
        bytes_to_copy = available_bytes;
    } else {
        pstate->expected_bytes = 0x0;
    }

    /* First do the crc to avoid changing data in case of encrypted data*/
    if (calc_crc) {
        pstate->calc_crc =
            crc32_no_lib_update(pstate->calc_crc, ptr, (int) bytes_to_copy);
    }

    if (!fake) {
        if (copy){
            memcpy(dest, ptr, bytes_to_copy);
        }
        else{
            dest = ptr;
        }
    }
    ptr = ptr + bytes_to_copy;
    pstate->ptr = ptr;
    return bytes_to_copy;
}

int 
syntiant_pkg_read_tag(
    syntiant_pkg_parser_state_t *pstate){
    int s = SYNTIANT_PACKAGE_ERROR_NONE;

    if ( !(pstate->mode == PACKAGE_MODE_TAG_START || 
          pstate->mode == PACKAGE_MODE_TAG_CONT)){
        s = SYNTIANT_PACKAGE_INCREMENTALLY_PARSING_ERROR;
        goto error;
    }
    
    syntiant_pkg_read_chunk_data(pstate,
                     pstate->tag, 4, 0, 1, 1);

    if (pstate->expected_bytes == 0) {
        pstate->mode = PACKAGE_MODE_LENGTH_START;
    } else {
        pstate->mode = PACKAGE_MODE_TAG_CONT;
    }
error:
    return s;
}

int 
syntiant_pkg_read_length(
    syntiant_pkg_parser_state_t *pstate){
    
    int s = SYNTIANT_PACKAGE_ERROR_NONE;
    
    if ( !(pstate->mode == PACKAGE_MODE_LENGTH_START
           || pstate->mode == PACKAGE_MODE_LENGTH_CONT) ){
        s = SYNTIANT_PACKAGE_INCREMENTALLY_PARSING_ERROR;
        goto error;
    }
    
    syntiant_pkg_read_chunk_data(pstate,
                     pstate->length, 4, 0, 1, 1);

    if (pstate->expected_bytes == 0) {
        pstate->mode = PACKAGE_MODE_VALUE_START;
        pstate->value_mode = PACKAGE_MODE_NO_PARTIAL_VALUE;
    } else {
        pstate->mode = PACKAGE_MODE_LENGTH_CONT;
    }
error:
    return s;
}

int 
syntiant_pkg_parse_checksum_value(syntiant_pkg_parser_state_t *pstate){
    int s = SYNTIANT_PACKAGE_ERROR_NONE;
    unsigned int byte_read = 0;
    
    byte_read = syntiant_pkg_read_chunk_data
        (pstate, pstate->exp_crc, *((uint32_t *) pstate->length), 0, 0, 1); 
    pstate->partially_read_length += byte_read;
    if (pstate->expected_bytes == 0) {
        pstate->mode = PACKAGE_MODE_DONE;  
        pstate->calc_crc = crc32_no_lib_finalize(pstate->calc_crc);
        if (pstate->calc_crc != *(uint32_t*)pstate->exp_crc){
            s = SYNTIANT_PACKAGE_ERROR_CHECKSUM;
            goto error;
        }
    } 
    else {
        pstate->mode = PACKAGE_MODE_VALUE_CONT;
        pstate->value_mode = PACKAGE_MODE_NO_PARTIAL_VALUE;
    }
error:
    return s;
}

int 
syntiant_pkg_parse_header_value(syntiant_pkg_parser_state_t *pstate){
    int s = SYNTIANT_PACKAGE_ERROR_NONE;

    int header_tlv_size = sizeof(pstate->tag)+ sizeof(pstate->length)+ 
                sizeof(pstate->header_val);
        
    if (syntiant_pkg_check_header_tlv(pstate->ptr)){
        memcpy(pstate->tag, pstate->ptr, TAG_SIZE);
        memcpy(pstate->length, pstate->ptr + TAG_SIZE, LENGTH_SIZE);
        pstate->header_val = *(uint32_t*)(pstate->ptr+TAG_SIZE+LENGTH_SIZE);
        pstate->magic_header_found = 1;
        pstate->mode = PACKAGE_MODE_TAG_START;
            
        pstate->calc_crc = crc32_no_lib_update(
                pstate->calc_crc, pstate->ptr, header_tlv_size);
        pstate->ptr += header_tlv_size;
    } else{
        s = SYNTIANT_PACKAGE_ERROR_HEADER;
        goto error;
    }
error:
    return s;
}

int 
syntiant_pkg_print_stored_params(syntiant_pkg_parser_state_t *pstate){
    int aligned_value = 0;
    int i = 0;
    uint8_t tmp_8;
    int s = SYNTIANT_PACKAGE_ERROR_NONE;
    /* prints the details if log should be collected*/
    switch (*(uint32_t*)pstate->tag){
        case TAG_FIRMWARE_VERSION_STRING_V1:
            SYNTIANT_PRINTF("\n--------------------------\n");
            pstate->data.fw_version[*(uint32_t*)pstate->length] = '\0';
            SYNTIANT_PRINTF("\n- Firmware version: %s\n", 
                pstate->data.fw_version);
            break;  

        case TAG_NN_VERSION_STRING_V1:
            SYNTIANT_PRINTF("\n--------------------------\n");
            pstate->data.params_version[*(uint32_t*)pstate->length] = '\0';
            SYNTIANT_PRINTF("\n- NN params version: %s\n", 
            pstate->data.params_version);
            break;  

        case TAG_BOARD_CALIBRATION_PARAMS_V1:
        case TAG_BOARD_CALIBRATION_PARAMS_V2:
            aligned_value = (*(uint32_t*)pstate->tag) == TAG_BOARD_CALIBRATION_PARAMS_V2 ?
                 1 : 0;
            SYNTIANT_PRINTF("\n--------------------------\n");
            SYNTIANT_PRINTF("\n- Board params: \n");
            SYNTIANT_PRINTF("\t- DNN Input: %u\n", 
                aligned_value ? 
                pstate->data.board_params.board_params_v2.dnn_input :
                pstate->data.board_params.board_params_v1.dnn_input);

            SYNTIANT_PRINTF("\t- Input clk rate: %u\n",
                aligned_value ?  
                pstate->data.board_params.board_params_v2.input_clock_rate:
                pstate->data.board_params.board_params_v1.input_clock_rate);

            SYNTIANT_PRINTF("\t- PDM clk rate: %u\n", 
                aligned_value ? 
                pstate->data.board_params.board_params_v2.pdm_clock_rate:
                pstate->data.board_params.board_params_v1.pdm_clock_rate);


            SYNTIANT_PRINTF("\t- PDM clk ndp: %u\n", 
                aligned_value ? 
                pstate->data.board_params.board_params_v2.pdm_clock_ndp:
                pstate->data.board_params.board_params_v1.pdm_clock_ndp);

            SYNTIANT_PRINTF("\t- Power offset: %u\n", 
                aligned_value ? 
                pstate->data.board_params.board_params_v2.power_offset:
                pstate->data.board_params.board_params_v1.power_offset);

            SYNTIANT_PRINTF("\t- Preemphasis exponent: %u\n", 
                aligned_value ? 
                pstate->data.board_params.board_params_v2.preemphasis_exponent:
                pstate->data.board_params.board_params_v1.preemphasis_exponent);


            SYNTIANT_PRINTF("\t- Power scale exponent: %u\n", 
                aligned_value ? 
                pstate->data.board_params.board_params_v2.power_scale_exponent:
                pstate->data.board_params.board_params_v1.power_scale_exponent);

            SYNTIANT_PRINTF("\t- Agc: %u\n", 
                aligned_value ?
                pstate->data.board_params.board_params_v1.agc:
                pstate->data.board_params.board_params_v2.agc);

            SYNTIANT_PRINTF("\t- Equalizer: \n");
                for (i = 0; i < 40 ; i++){
                    SYNTIANT_PRINTF("\t \t %d\n", 
                        aligned_value ?
                        pstate->data.board_params.board_params_v2.equalizer[i]:
                        pstate->data.board_params.board_params_v1.equalizer[i]);    
                }
            break;
        case TAG_BOARD_CALIBRATION_PARAMS_V3:
            SYNTIANT_PRINTF("\n--------------------------\n");
            SYNTIANT_PRINTF("\n- Board params: \n");
            SYNTIANT_PRINTF("\t- DNN Input: %u\n",
                pstate->data.board_params.board_params_v3.dnn_input);

            SYNTIANT_PRINTF("\t- Input clk rate: %u\n",
                pstate->data.board_params.board_params_v3.input_clock_rate);

            SYNTIANT_PRINTF("\t- PDM clk rate: %u\n",
                pstate->data.board_params.board_params_v3.pdm_clock_rate);


            SYNTIANT_PRINTF("\t- PDM clk ndp: %u\n",
                pstate->data.board_params.board_params_v3.pdm_clock_ndp);

            SYNTIANT_PRINTF("\t- Power offset: %u\n",
                pstate->data.board_params.board_params_v3.power_offset);

            SYNTIANT_PRINTF("\t- Preemphasis exponent: %u\n",
                pstate->data.board_params.board_params_v3.preemphasis_exponent);


            SYNTIANT_PRINTF("\t- Power scale exponent: %u\n",
                pstate->data.board_params.board_params_v3.power_scale_exponent);

            SYNTIANT_PRINTF("\t- Agc: %u\n",
                pstate->data.board_params.board_params_v3.agc);

            SYNTIANT_PRINTF("\t- Equalizer: \n");
                for (i = 0; i < 40 ; i++){
                    SYNTIANT_PRINTF("\t \t %d\n",
                        pstate->data.board_params.board_params_v3.equalizer[i]);
                }
            SYNTIANT_PRINTF("\t- AGC Maximum adjustment level: \n");
            for (i = 0; i < 2 ; i++){
                SYNTIANT_PRINTF("\t %u\n",
                 pstate->data.board_params.board_params_v3.agc_max_adj[i]);
            }
            SYNTIANT_PRINTF("\t- AGC Reference quiet level: \n");
            SYNTIANT_PRINTF("\t %u\n",
                 pstate->data.board_params.board_params_v3.agc_ref_lvl);
            break;
        case TAG_PACKAGE_VERSION_STRING:
            SYNTIANT_PRINTF("\n--------------------------\n");
            pstate->data.pkg_version[*(uint32_t*)pstate->length] = '\0';
            SYNTIANT_PRINTF("\n- Package version: %s\n", 
            pstate->data.pkg_version);
            break;  

        case TAG_NDP10X_HW_CONFIG_V2:
            SYNTIANT_PRINTF("\n--------------------------\n");
            SYNTIANT_PRINTF("\n- HW params: \n") ;
            SYNTIANT_PRINTF("\t- Sample frequency: %u\n", 
                pstate->data.hw_params.sample_frequency_hz);
            SYNTIANT_PRINTF("\t- Frame size: %u\n", 
                pstate->data.hw_params.frame_size);
            SYNTIANT_PRINTF("\t- Frame step: %u\n", 
                pstate->data.hw_params.frame_step);
            SYNTIANT_PRINTF("\t- Num of filter banks: %u\n", 
                pstate->data.hw_params.num_filter_banks);
            SYNTIANT_PRINTF("\t- Num of activation used: %u\n", 
                pstate->data.hw_params.num_activations_used);
            SYNTIANT_PRINTF("\t- Length of melbin: %u\n", 
                pstate->data.hw_params.mel_bins_length);
            break;

        case TAG_NDP10X_B0_NN_CONFIG_V2:
        case TAG_NDP10X_B0_NN_CONFIG_V3:
            aligned_value = (*(uint32_t*)pstate->tag) == TAG_NDP10X_B0_NN_CONFIG_V3 ?
                 1 : 0;  
            SYNTIANT_PRINTF("\n--------------------------\n");
            SYNTIANT_PRINTF("\n- NN config params: \n") ; 
            SYNTIANT_PRINTF("\t- Num of features: %u\n", 
                aligned_value ? 
                pstate->data.fc_params.fc_params_v3.num_features:
                pstate->data.fc_params.fc_params_v2.num_features);
            SYNTIANT_PRINTF("\t- Num of static features: %u\n", 
                aligned_value ? 
                pstate->data.fc_params.fc_params_v3.num_static_features:
                pstate->data.fc_params.fc_params_v2.num_static_features);
            SYNTIANT_PRINTF("\t- DNN frame size: %u\n", 
                aligned_value ? 
                pstate->data.fc_params.fc_params_v3.dnn_frame_size:
                pstate->data.fc_params.fc_params_v2.dnn_frame_size);
            SYNTIANT_PRINTF("\t- Input clipping threshold: %u\n", 
                aligned_value ? 
                pstate->data.fc_params.fc_params_v3.input_clipping_threshold:
                pstate->data.fc_params.fc_params_v2.input_clipping_threshold);
            SYNTIANT_PRINTF("\t- DNN run threshold: %u\n", 
                aligned_value ? 
                pstate->data.fc_params.fc_params_v3.dnn_run_threshold:
                pstate->data.fc_params.fc_params_v2.dnn_run_threshold);
            SYNTIANT_PRINTF("\t- Quantization schemes for Layers: ");
            for (i=0; i<4;i++){
                tmp_8 = aligned_value ? pstate->data.fc_params.fc_params_v3.quantization_scheme[i]:
                    pstate->data.fc_params.fc_params_v2.quantization_scheme[i];
                if (tmp_8)
                    SYNTIANT_PRINTF("sqrt_49 ");
                else
                    SYNTIANT_PRINTF("uniform ");
            }
            SYNTIANT_PRINTF("\n \t- Num of nodes in layers: ");
            for (i=0; i<4; i++){
                SYNTIANT_PRINTF("\t %u", 
                    aligned_value ? pstate->data.fc_params.fc_params_v3.num_nodes[i]: 
                        pstate->data.fc_params.fc_params_v2.num_nodes[i]);
            }
            SYNTIANT_PRINTF("\n \t- Num of biases in layers: ");
            for (i=0; i<4; i++){
                SYNTIANT_PRINTF("\t %u", 
                    aligned_value ? pstate->data.fc_params.fc_params_v3.num_biases[i]: 
                        pstate->data.fc_params.fc_params_v2.num_biases[i]);
            }
            SYNTIANT_PRINTF("\n \t- Scale factor in layers: ");
            for (i=0; i<4; i++){
                SYNTIANT_PRINTF("\t %u", 
                    aligned_value ? pstate->data.fc_params.fc_params_v3.scale_factor[i]: 
                        pstate->data.fc_params.fc_params_v2.scale_factor[i]);
            }
            SYNTIANT_PRINTF("\n \t- Outshift in layers: ");
            for (i=0; i<4; i++){
                SYNTIANT_PRINTF("\t %u", 
                    aligned_value ? pstate->data.fc_params.fc_params_v3.out_shift[i]: 
                        pstate->data.fc_params.fc_params_v2.out_shift[i]);
            }
            break;
        default:
            s = SYNTIANT_PACKAGE_ERROR_UNKNOWN_TLV;
            break;
        }
    return s;
}

int
syntiant_pkg_parse_stored_params(syntiant_pkg_parser_state_t *pstate, 
    int collect_log){
    int s = SYNTIANT_PACKAGE_ERROR_NONE;
    unsigned int byte_read = 0;
    int aligned_value = 0;
    int i = 0;

    /*sanity check on the length*/
    if (pstate->mode == PACKAGE_MODE_VALUE_START){
        switch (*(uint32_t*)pstate->tag){
        case TAG_FIRMWARE_VERSION_STRING_V1:
            if (*(uint32_t*) pstate->length > VERSION_MAX_SIZE){
                s = SYNTIANT_PACKAGE_ERROR_FIRMWARE_VERSION_STRING_V2;
                goto error;
            }
            break;
        case TAG_NN_VERSION_STRING_V1:
            if (*(uint32_t*) pstate->length > VERSION_MAX_SIZE){
                s = SYNTIANT_PACKAGE_ERROR_NN_VERSION_STRING_V2;
                goto error;
            }
            break;
       
        case TAG_PACKAGE_VERSION_STRING:
            if (*(uint32_t*) pstate->length > VERSION_MAX_SIZE){
                s = SYNTIANT_PACKAGE_ERROR_PACKAGE_VERSION_STRING;
                goto error;
            }
            break;
        case TAG_NDP10X_HW_CONFIG_V2:
            if (*(uint32_t*) pstate->length != HW_PARAMS_V2_SIZE){
                s = SYNTIANT_PACKAGE_ERROR_NDP10X_HW_CONFIG_V2;
                goto error;
            }
            break;

        case TAG_BOARD_CALIBRATION_PARAMS_V1:
            if (*(uint32_t*) pstate->length != BOARD_PARAMS_V1_SIZE){
                s = SYNTIANT_PACKAGE_ERROR_BOARD_CALIBRATION_PARAMS_V1;
                goto error;
            }
            break;
        case TAG_NDP10X_B0_NN_CONFIG_V2:
            if (*(uint32_t*) pstate->length != FC_CONFIG_PARAMS_V2_SIZE){
                s = SYNTIANT_PACKAGE_ERROR_NDP10X_B0_NN_CONFIG_V2;
                goto error;
            }
            break;
        case TAG_BOARD_CALIBRATION_PARAMS_V2:
            if (*(uint32_t*) pstate->length != BOARD_PARAMS_V2_SIZE){
               s = SYNTIANT_PACKAGE_ERROR_BOARD_CALIBRATION_PARAMS_V2;
                goto error;
            }
            break;
        case TAG_BOARD_CALIBRATION_PARAMS_V3:
            if ((*(uint32_t*) pstate->length != BOARD_PARAMS_V3_SIZE) &&
                (*(uint32_t*) pstate->length != BOARD_PARAMS_V2_SIZE)) {
               s = SYNTIANT_PACKAGE_ERROR_BOARD_CALIBRATION_PARAMS_V3;
                goto error;
            }
            break;
        case TAG_NDP10X_B0_NN_CONFIG_V3:
            if (*(uint32_t*) pstate->length != FC_CONFIG_PARAMS_V3_SIZE){
                s = SYNTIANT_PACKAGE_ERROR_NDP10X_B0_NN_CONFIG_V3;
                goto error;
            }
            break;
        }
    }

    switch (*(uint32_t*)pstate->tag){
    case TAG_FIRMWARE_VERSION_STRING_V1:
        byte_read += syntiant_pkg_read_chunk_data(pstate,
            pstate->data.fw_version, *(uint32_t*) pstate->length, 0, 1, 1);
        pstate->partially_read_length += byte_read;
        pstate->is_current_fw_package = 1;
        break;

    case TAG_NN_VERSION_STRING_V1:
        byte_read += syntiant_pkg_read_chunk_data(pstate,
            pstate->data.params_version, *(uint32_t*) pstate->length, 0, 1, 1);  
        pstate->partially_read_length += byte_read;
        pstate->is_current_params_package = 1;
        break;

    case TAG_PACKAGE_VERSION_STRING:
        byte_read += syntiant_pkg_read_chunk_data(pstate,
            pstate->data.pkg_version, *(uint32_t*) pstate->length, 0, 1, 1);  
        pstate->partially_read_length += byte_read;
        break;

    case TAG_NDP10X_HW_CONFIG_V2:
        byte_read += syntiant_pkg_read_chunk_data(pstate,
            (uint8_t*)&(pstate->data.hw_params),
            *(uint32_t*) pstate->length, 0, 1, 1);
        pstate->partially_read_length += byte_read;
        break;

    case TAG_BOARD_CALIBRATION_PARAMS_V1:
        byte_read += syntiant_pkg_read_chunk_data(pstate,
            (uint8_t*)&(pstate->data.board_params.board_params_v1),
            *(uint32_t*) pstate->length, 0, 1, 1);
        pstate->partially_read_length += byte_read;
        break;
    case TAG_BOARD_CALIBRATION_PARAMS_V2:
        byte_read += syntiant_pkg_read_chunk_data(pstate,
            (uint8_t*)&(pstate->data.board_params.board_params_v2),
            *(uint32_t*) pstate->length, 0, 1, 1);
        pstate->partially_read_length += byte_read;
        break;
    case TAG_BOARD_CALIBRATION_PARAMS_V3:
        byte_read += syntiant_pkg_read_chunk_data(pstate,
            (uint8_t*)&(pstate->data.board_params.board_params_v3),
            *(uint32_t*) pstate->length, 0, 1, 1);
        pstate->partially_read_length += byte_read;
        break;

    case TAG_NDP10X_B0_NN_CONFIG_V2:
    case TAG_NDP10X_B0_NN_CONFIG_V3:
        aligned_value = (*(uint32_t*)pstate->tag) ==
                TAG_NDP10X_B0_NN_CONFIG_V3 ? 1 : 0;
        if (aligned_value){
            byte_read += syntiant_pkg_read_chunk_data(pstate,
                (uint8_t*)&(pstate->data.fc_params.fc_params_v3),
                *(uint32_t*) pstate->length, 0, 1, 1);
        }
        else{
            byte_read += syntiant_pkg_read_chunk_data(pstate,
                (uint8_t*)&(pstate->data.fc_params.fc_params_v2),
                *(uint32_t*) pstate->length, 0, 1, 1);
        }
        pstate->partially_read_length += byte_read;
        break;

    default:
        s = SYNTIANT_PACKAGE_ERROR_UNKNOWN_TLV;
        goto error;
    }

    pstate->mode = (pstate->expected_bytes == 0) ? 
            PACKAGE_MODE_TAG_START : PACKAGE_MODE_VALUE_CONT; 
    pstate->value_mode = (pstate->expected_bytes == 0) ? 
            PACKAGE_MODE_VALID_PARTIAL_VALUE : PACKAGE_MODE_NO_PARTIAL_VALUE; 

    /*prepare metadata to be used for parsing NN params*/
    if(pstate->expected_bytes == 0 &&
       (*(uint32_t*)pstate->tag == TAG_NDP10X_B0_NN_CONFIG_V2 ||
                *(uint32_t*)pstate->tag == TAG_NDP10X_B0_NN_CONFIG_V3)){
        aligned_value = (*(uint32_t*)pstate->tag) ==
                TAG_NDP10X_B0_NN_CONFIG_V3 ? 1 : 0;
        if ( aligned_value ?
             pstate->data.fc_params.fc_params_v3.num_static_features >
                    pstate->data.fc_params.fc_params_v3.num_features :
             pstate->data.fc_params.fc_params_v2.num_static_features >
                    pstate->data.fc_params.fc_params_v2.num_features){
            s = aligned_value ? SYNTIANT_PACKAGE_ERROR_NDP10X_B0_NN_CONFIG_V3:
                SYNTIANT_PACKAGE_ERROR_NDP10X_B0_NN_CONFIG_V2;
            goto error;
        }
        pstate->metadata.fc_metadata.num_features = aligned_value ?
                pstate->data.fc_params.fc_params_v3.num_features:
                pstate->data.fc_params.fc_params_v2.num_features;

        pstate->metadata.fc_metadata.num_static_features = aligned_value ?
            pstate->data.fc_params.fc_params_v3.num_static_features:
            pstate->data.fc_params.fc_params_v2.num_static_features;

        for (i = 0; i < 4; i++){
            pstate->metadata.fc_metadata.num_nodes[i] = aligned_value ?
                pstate->data.fc_params.fc_params_v3.num_nodes[i]:
                pstate->data.fc_params.fc_params_v2.num_nodes[i];
            pstate->metadata.fc_metadata.num_biases[i] = aligned_value ?
                pstate->data.fc_params.fc_params_v3.num_biases[i]:
                pstate->data.fc_params.fc_params_v2.num_biases[i];
        }
        /*sanity check on num_features, and num_nodes to be multiple of 8*/
        if ( (aligned_value ?
            pstate->data.fc_params.fc_params_v3.num_features %8 != 0 :
            pstate->data.fc_params.fc_params_v2.num_features %8 != 0) ||
            (pstate->metadata.fc_metadata.num_nodes[0]+1) % 8 != 0
            || (pstate->metadata.fc_metadata.num_nodes[1]+1) % 8 != 0
            || (pstate->metadata.fc_metadata.num_nodes[2]+1) % 8 != 0){
                s = aligned_value ?
                    SYNTIANT_PACKAGE_ERROR_NDP10X_B0_NN_CONFIG_V3 :
                    SYNTIANT_PACKAGE_ERROR_NDP10X_B0_NN_CONFIG_V2;
                goto error;
        }

    }
    /*print log information in case requested*/
    if (collect_log && pstate->expected_bytes == 0){
        syntiant_pkg_print_stored_params(pstate);
    }

error:
    return s;
}

int 
syntiant_pkg_parse_partially_stored_params(syntiant_pkg_parser_state_t *pstate, 
    int collect_log){
    int s = SYNTIANT_PACKAGE_ERROR_NONE;
    unsigned int byte_read = 0;
    unsigned int index = 0;
    switch (*(uint32_t*)pstate->tag){

     case TAG_NN_LABELS_V1:
        if (pstate->mode == PACKAGE_MODE_VALUE_START){
            /*sanity check on labels size*/
            if (*(uint32_t*)pstate->length > LABELS_MAX_SIZE){
                s = SYNTIANT_PACKAGE_ERROR_NN_LABELS_V1;
                goto error;
            }
            pstate->metadata.labels_metadata = 
            maximum(PARSER_SCRATCH_SIZE,*(uint32_t*)pstate->length); 
            if (collect_log){
                SYNTIANT_PRINTF("\n--------------------------\n");
                SYNTIANT_PRINTF("\n - Labels length: %u\n", 
                    *(uint32_t*)pstate->length);   
                SYNTIANT_PRINTF(" - Labels: "); 
            }
        }
        byte_read = syntiant_pkg_read_chunk_data(pstate,
            (uint8_t*)(&(pstate->data.labels)), 
            pstate->metadata.labels_metadata, 0, 1, 1); 
        pstate->partially_read_length += byte_read;
        if (pstate->partially_read_length == *(uint32_t*)pstate->length){
            if(collect_log){
                for (index = 0; index < pstate->metadata.labels_metadata ; 
                    index++){
                    if (pstate->data.labels[index] == '\0')
                        SYNTIANT_PRINTF("%c", ' ');
                    else
                        SYNTIANT_PRINTF("%c", pstate->data.labels[index]);
                } 
            }
            pstate->mode = PACKAGE_MODE_TAG_START;
            pstate->value_mode = PACKAGE_MODE_VALID_PARTIAL_VALUE;
            if (pstate->expected_bytes != 0){
                s = SYNTIANT_PACKAGE_INCREMENTALLY_PARSING_ERROR;
                goto error;
            }
        }
        else{ 
            if (pstate->expected_bytes){
                pstate->mode = PACKAGE_MODE_VALUE_CONT;
                pstate->value_mode = PACKAGE_MODE_NO_PARTIAL_VALUE;
            }
            else{
                pstate->mode = PACKAGE_MODE_VALUE_CONT;
                pstate->value_mode = PACKAGE_MODE_VALID_PARTIAL_VALUE;
                if(collect_log){
                    for (index = 0; index < pstate->metadata.labels_metadata ; 
                        index++){
                        if (pstate->data.labels[index] == '\0')
                            SYNTIANT_PRINTF("%c", ' ');
                        else
                            SYNTIANT_PRINTF("%c", pstate->data.labels[index]);
                    } 
                }
                pstate->metadata.labels_metadata = 
                maximum(PARSER_SCRATCH_SIZE,*(uint32_t*)pstate->length - 
                pstate->partially_read_length);
                pstate->expected_bytes = pstate->metadata.labels_metadata;
                
            }
        }
        break;

    case TAG_FIRMWARE:
        if (pstate->mode == PACKAGE_MODE_VALUE_START){
            /*sanity check on fw size*/
            if (*(uint32_t*)pstate->length > FW_MAX_SIZE){
                s = SYNTIANT_PACKAGE_ERROR_FIRMWARE;
                goto error;
            }
            pstate->metadata.fw_metadata = 
            maximum(PARSER_SCRATCH_SIZE,*(uint32_t*)pstate->length); 
            if (collect_log){
                SYNTIANT_PRINTF("\n--------------------------\n");
                SYNTIANT_PRINTF("\n - Firmware length: %u\n", 
                    *(uint32_t*)pstate->length);    
            }
        }
        byte_read = syntiant_pkg_read_chunk_data(pstate,
            (uint8_t*)(&(pstate->data.fw)), 
            pstate->metadata.fw_metadata, 0, 1, 1); 
        pstate->partially_read_length += byte_read;
        if (pstate->partially_read_length == *(uint32_t*)pstate->length){
            pstate->mode = PACKAGE_MODE_TAG_START;
            pstate->value_mode = PACKAGE_MODE_VALID_PARTIAL_VALUE;
            if (pstate->expected_bytes != 0){
                s = SYNTIANT_PACKAGE_INCREMENTALLY_PARSING_ERROR;
                goto error;
            }
        }
        else{ 
            if (pstate->expected_bytes){
                pstate->mode = PACKAGE_MODE_VALUE_CONT;
                pstate->value_mode = PACKAGE_MODE_NO_PARTIAL_VALUE;
            }
            else{
                pstate->mode = PACKAGE_MODE_VALUE_CONT;
                pstate->value_mode = PACKAGE_MODE_VALID_PARTIAL_VALUE;
                pstate->metadata.fw_metadata = 
                maximum(PARSER_SCRATCH_SIZE,*(uint32_t*)pstate->length - 
                pstate->partially_read_length);
                pstate->expected_bytes = pstate->metadata.fw_metadata;
            }
        }
        break;

    case TAG_NDP10X_NN_PARAMETERS_V3:
        if (pstate->mode == PACKAGE_MODE_VALUE_START){
            pstate->metadata.fc_metadata.params_to_read = 
            maximum(PARSER_SCRATCH_SIZE,*(uint32_t*)pstate->length);

            /*sanity check*/
            if (*(uint32_t*)pstate->length != ( 
            (pstate->metadata.fc_metadata.num_features * 
                (pstate->metadata.fc_metadata.num_nodes[0]+1U))
            +
            ( (pstate->metadata.fc_metadata.num_nodes[0]+1U) *
                (pstate->metadata.fc_metadata.num_nodes[1]+1U))
            +
            ( (pstate->metadata.fc_metadata.num_nodes[1]+1U) * 
                (pstate->metadata.fc_metadata.num_nodes[2]+1U) )
            +
            ( (pstate->metadata.fc_metadata.num_nodes[2]+1U) * 
                (pstate->metadata.fc_metadata.num_nodes[3]+1U))
            )/2
            + (pstate->metadata.fc_metadata.num_biases[0] * 
                    (pstate->metadata.fc_metadata.num_nodes[0]+1U) +
                pstate->metadata.fc_metadata.num_biases[1] * 
                    (pstate->metadata.fc_metadata.num_nodes[1]+1U) +
                pstate->metadata.fc_metadata.num_biases[2] * 
                    (pstate->metadata.fc_metadata.num_nodes[2]+1U)+
                pstate->metadata.fc_metadata.num_biases[3] *
                    (pstate->metadata.fc_metadata.num_nodes[3]+1U))/2 
                ){
                s = SYNTIANT_PACKAGE_ERROR_NDP10X_NN_PARAMETERS_V3;
                goto error;
            }

            if (collect_log){
                SYNTIANT_PRINTF("\n--------------------------\n");
                SYNTIANT_PRINTF("\n - DNN params size: %u\n", 
                    *(uint32_t*)pstate->length);    
            }
        }

        byte_read = syntiant_pkg_read_chunk_data(pstate,
            (uint8_t*)(&(pstate->data.packed_params)), 
            pstate->metadata.fc_metadata.params_to_read, 0, 1, 1); 
        pstate->partially_read_length += byte_read;

        if (pstate->partially_read_length == *(uint32_t*)pstate->length){
            pstate->mode = PACKAGE_MODE_TAG_START;
            pstate->value_mode = PACKAGE_MODE_VALID_PARTIAL_VALUE;
            if (pstate->expected_bytes != 0){
                s = SYNTIANT_PACKAGE_INCREMENTALLY_PARSING_ERROR;
                goto error;
            }
        }
        else{
            if (pstate->expected_bytes){
                pstate->mode = PACKAGE_MODE_VALUE_CONT;
                pstate->value_mode = PACKAGE_MODE_NO_PARTIAL_VALUE;
            }
            else{
                pstate->metadata.fc_metadata.params_to_read = 
                maximum(PARSER_SCRATCH_SIZE,
                    *(uint32_t*)pstate->length - pstate->partially_read_length);
                pstate->expected_bytes = 
                pstate->metadata.fc_metadata.params_to_read;
                pstate->mode = PACKAGE_MODE_VALUE_CONT;
                pstate->value_mode = PACKAGE_MODE_VALID_PARTIAL_VALUE;  
            }   
        }

        break;
    }

error:
    return s;
}

int 
syntiant_pkg_parse_posterior_params(syntiant_pkg_parser_state_t *pstate, 
    int collect_log) {
    int s = SYNTIANT_PACKAGE_ERROR_NONE;
    unsigned int byte_read = 0;
    switch (*(uint32_t*)pstate->tag){
    
        case TAG_NN_PH_PARAMETERS_V4:
            if (pstate->partially_read_length < (
                sizeof(pstate->metadata.ph_metadata.v1) - 
                sizeof(pstate->metadata.ph_metadata.v1.cur_state) -
                sizeof(pstate->metadata.ph_metadata.v1.cur_class))){

                byte_read = syntiant_pkg_read_chunk_data(pstate,
                    (uint8_t*)(&(pstate->metadata.ph_metadata.v1)), 
                    (sizeof(pstate->metadata.ph_metadata.v1) - 
                    sizeof(pstate->metadata.ph_metadata.v1.cur_state) -
                    sizeof(pstate->metadata.ph_metadata.v1.cur_class)), 0, 1, 1);  

                pstate->partially_read_length += byte_read; 
                /* cur_class of 1, means the first class, and cur_state of 1 means 
                the first state. indexing start from 0 */
                if (pstate->expected_bytes == 0){
                    pstate->metadata.ph_metadata.v1.cur_class = 0;
                    pstate->metadata.ph_metadata.v1.cur_state = 1;
                } 
            }
            else{
                /*sanity check*/
                if (pstate->partially_read_length == (
                    sizeof(pstate->metadata.ph_metadata.v1) - 
                    sizeof(pstate->metadata.ph_metadata.v1.cur_state) -
                    sizeof(pstate->metadata.ph_metadata.v1.cur_class))){

                    if (pstate->partially_read_length + 
                        (pstate->metadata.ph_metadata.v1.num_states - 1) *
                        sizeof(pstate->metadata.ph_metadata.v1.timeout) + 
                        pstate->metadata.ph_metadata.v1.num_states *
                        pstate->metadata.ph_metadata.v1.num_classes * 
                        sizeof(pstate->data.ph_params)  != 
                        *(uint32_t*)pstate->length) {
                        s = SYNTIANT_PACKAGE_ERROR_NN_PH_PARAMETERS_V4;
                        goto error;
                    }

                    if (collect_log){
                        SYNTIANT_PRINTF("\n--------------------------\n");
                        SYNTIANT_PRINTF("\n- Posterior params: \n") ;
                        SYNTIANT_PRINTF("\t - Num of states: %u\n", 
                            pstate->metadata.ph_metadata.v1.num_states);
                        SYNTIANT_PRINTF("\t - Num of classes: %u\n",
                            pstate->metadata.ph_metadata.v1.num_classes );
                        SYNTIANT_PRINTF("\t - State 1 timeout: %u\n",
                            pstate->metadata.ph_metadata.v1.timeout );

                    }  
                }

                /*read one class info*/
                if (pstate->metadata.ph_metadata.v1.cur_class < 
                    pstate->metadata.ph_metadata.v1.num_classes){
                    pstate->expected_bytes = 
                        (pstate->expected_bytes > 0) ? 
                        pstate->expected_bytes : 
                        sizeof(pstate->data.ph_params);

                    byte_read = syntiant_pkg_read_chunk_data(pstate,
                        (uint8_t*)(&(pstate->data.ph_params)), 
                        sizeof(pstate->data.ph_params), 0, 1, 1); 

                    pstate->partially_read_length += byte_read;
                    if (pstate->expected_bytes == 0){
                        pstate->metadata.ph_metadata.v1.cur_class++;

                        /*sanity check*/
                        if (pstate->metadata.ph_metadata.v1.cur_class > 
                            pstate->metadata.ph_metadata.v1.num_classes){
                            s = SYNTIANT_PACKAGE_INCREMENTALLY_PARSING_ERROR;
                            goto error;
                        }

                        if(collect_log){
                            SYNTIANT_PRINTF("\t \t - Class %u\n",
                            pstate->metadata.ph_metadata.v1.cur_class ); 

                            SYNTIANT_PRINTF("\t \t \t - Win: %u, Th: %u, Bkoff: %u, SQ: %u\n",
                            pstate->data.ph_params.phwin, pstate->data.ph_params.phth,
                            pstate->data.ph_params.phbackoff, 
                            pstate->data.ph_params.phqueuesize);    
                        }
                    }
                }

                /*read timeout for the next state*/
                else{
                    pstate->expected_bytes = 
                        (pstate->expected_bytes > 0) ? pstate->expected_bytes: 
                        sizeof(pstate->metadata.ph_metadata.v1.timeout);

                    byte_read = syntiant_pkg_read_chunk_data(pstate,
                        (uint8_t*)(&(pstate->metadata.ph_metadata.v1.timeout)), 
                        sizeof(pstate->metadata.ph_metadata.v1.timeout), 0, 1, 1);
                    pstate->partially_read_length += byte_read;
                    if (pstate->expected_bytes == 0){
                        pstate->metadata.ph_metadata.v1.cur_class = 0;
                        pstate->metadata.ph_metadata.v1.cur_state++;

                        /*sanity check*/
                        if (pstate->metadata.ph_metadata.v1.cur_state > 
                            pstate->metadata.ph_metadata.v1.num_states){
                            s = SYNTIANT_PACKAGE_INCREMENTALLY_PARSING_ERROR;
                            goto error;
                        }

                        if(collect_log){
                            SYNTIANT_PRINTF("\t - State %u timeout: %u\n",
                            pstate->metadata.ph_metadata.v1.cur_state, 
                            pstate->metadata.ph_metadata.v1.timeout);
                        }
                    }
                }
            }
            pstate->value_mode = 
                (pstate->expected_bytes == 0) ? PACKAGE_MODE_VALID_PARTIAL_VALUE 
                : PACKAGE_MODE_NO_PARTIAL_VALUE;
            pstate->mode = 
                (pstate->partially_read_length < *(uint32_t*)pstate->length) ?
                PACKAGE_MODE_VALUE_CONT : PACKAGE_MODE_TAG_START;

            if (pstate->mode == PACKAGE_MODE_TAG_START){
                if ( pstate->expected_bytes ||
                    !(pstate->metadata.ph_metadata.v1.cur_class == 
                        pstate->metadata.ph_metadata.v1.num_classes &&
                    pstate->metadata.ph_metadata.v1.cur_state == 
                        pstate->metadata.ph_metadata.v1.num_states  
                        )){            
                    s =  SYNTIANT_PACKAGE_ERROR_NN_PH_PARAMETERS_V4;
                        goto error;
                }
            }
            break;

        case TAG_NN_PH_PARAMETERS_V5:
            if (pstate->partially_read_length < (
                sizeof(pstate->metadata.ph_metadata.v2) - 
                sizeof(pstate->metadata.ph_metadata.v2.cur_state) -
                sizeof(pstate->metadata.ph_metadata.v2.cur_class))){

                byte_read = syntiant_pkg_read_chunk_data(pstate,
                    (uint8_t*)(&(pstate->metadata.ph_metadata.v2)), 
                    (sizeof(pstate->metadata.ph_metadata.v2) - 
                    sizeof(pstate->metadata.ph_metadata.v2.cur_state) -
                    sizeof(pstate->metadata.ph_metadata.v2.cur_class)), 0, 1, 1);  

                pstate->partially_read_length += byte_read; 
                /* cur_class of 1, means the first class, and cur_state of 1 means 
                the first state. indexing start from 0 */
                if (pstate->expected_bytes == 0){
                    pstate->metadata.ph_metadata.v2.cur_class = 0;
                    pstate->metadata.ph_metadata.v2.cur_state = 1;
                } 
            } else {
                /*sanity check*/
                if (pstate->partially_read_length == (
                    sizeof(pstate->metadata.ph_metadata.v2) - 
                    sizeof(pstate->metadata.ph_metadata.v2.cur_state) -
                    sizeof(pstate->metadata.ph_metadata.v2.cur_class))){

                    if (pstate->partially_read_length +
                        (pstate->metadata.ph_metadata.v2.num_states - 1) *
                         (sizeof(pstate->metadata.ph_metadata.v2.timeout) +
                          sizeof(pstate->metadata.ph_metadata.v2.timeout_action) +
                          sizeof(pstate->metadata.ph_metadata.v2.timeout_action_arg)) + 
                        pstate->metadata.ph_metadata.v2.num_states *
                        pstate->metadata.ph_metadata.v2.num_classes * 
                        sizeof(pstate->data.ph_params)  != 
                        *(uint32_t*)pstate->length) {
                        s = SYNTIANT_PACKAGE_ERROR_NN_PH_PARAMETERS_V5;
                        goto error;
                    }

                    if (collect_log){
                        SYNTIANT_PRINTF("\n--------------------------\n");
                        SYNTIANT_PRINTF("\n- Posterior params: \n") ;
                        SYNTIANT_PRINTF("\t - PH type: %d\n", 
                            pstate->metadata.ph_metadata.v2.ph_type);
                        SYNTIANT_PRINTF("\t - Num of states: %u\n", 
                            pstate->metadata.ph_metadata.v2.num_states);
                        SYNTIANT_PRINTF("\t - Num of classes: %u\n",
                            pstate->metadata.ph_metadata.v2.num_classes );
                        SYNTIANT_PRINTF(
                            "\t - State 1 timeout: %u,",
                            pstate->metadata.ph_metadata.v2.timeout);
                        SYNTIANT_PRINTF(
                            " timeout_action: %u",
                            pstate->metadata.ph_metadata.v2.timeout_action);
                        SYNTIANT_PRINTF(
                            " timeout_action_arg: %u\n",
                            pstate->metadata.ph_metadata.v2.timeout_action_arg);
                    }  
                }

                /*read one class info*/
                if (pstate->metadata.ph_metadata.v2.cur_class < 
                    pstate->metadata.ph_metadata.v2.num_classes){
                    pstate->expected_bytes = 
                        (pstate->expected_bytes > 0) ? 
                        pstate->expected_bytes : 
                        sizeof(pstate->data.ph_params);

                    byte_read = syntiant_pkg_read_chunk_data(pstate,
                        (uint8_t*)(&(pstate->data.ph_params)), 
                        sizeof(pstate->data.ph_params), 0, 1, 1); 

                    pstate->partially_read_length += byte_read;
                    if (pstate->expected_bytes == 0){
                        pstate->metadata.ph_metadata.v2.cur_class++;

                        /*sanity check*/
                        if (pstate->metadata.ph_metadata.v2.cur_class > 
                            pstate->metadata.ph_metadata.v2.num_classes){
                            s = SYNTIANT_PACKAGE_INCREMENTALLY_PARSING_ERROR;
                            goto error;
                        }

                        if(collect_log){
                            SYNTIANT_PRINTF("\t \t - Class %u\n",
                            pstate->metadata.ph_metadata.v2.cur_class ); 

                            SYNTIANT_PRINTF("\t \t \t - Win: %u, Th: %u, Bkoff: %u, SQ: %u\n",
                            pstate->data.ph_params.phwin, pstate->data.ph_params.phth,
                            pstate->data.ph_params.phbackoff, 
                            pstate->data.ph_params.phqueuesize);    
                        }
                    }
                } else {
                    /*read timeout for the next state*/
                    pstate->expected_bytes = 
                        (pstate->expected_bytes > 0) ? pstate->expected_bytes: 
                        sizeof(pstate->metadata.ph_metadata.v2.timeout);
                    byte_read = syntiant_pkg_read_chunk_data(pstate,
                        (uint8_t*)(&(pstate->metadata.ph_metadata.v2.timeout)), 
                        sizeof(pstate->metadata.ph_metadata.v2.timeout), 0, 1, 1);
                    pstate->partially_read_length += byte_read;
                    /*read timeout action for the next state*/
                    pstate->expected_bytes = 
                        (pstate->expected_bytes > 0) ? pstate->expected_bytes: 
                        sizeof(pstate->metadata.ph_metadata.v2.timeout_action);
                    byte_read = syntiant_pkg_read_chunk_data(pstate,
                        (uint8_t*)(&(pstate->metadata.ph_metadata.v2.timeout_action)), 
                        sizeof(pstate->metadata.ph_metadata.v2.timeout_action), 0, 1, 1);
                    pstate->partially_read_length += byte_read;
                    /*read timeout action args for the next state*/
                    pstate->expected_bytes = 
                        (pstate->expected_bytes > 0) ? pstate->expected_bytes: 
                        sizeof(pstate->metadata.ph_metadata.v2.timeout_action_arg);
                    byte_read = syntiant_pkg_read_chunk_data(pstate,
                        (uint8_t*)(&(pstate->metadata.ph_metadata.v2.timeout_action_arg)), 
                        sizeof(pstate->metadata.ph_metadata.v2.timeout_action_arg), 0, 1, 1);
                    pstate->partially_read_length += byte_read;

                    if (pstate->expected_bytes == 0){
                        pstate->metadata.ph_metadata.v2.cur_class = 0;
                        pstate->metadata.ph_metadata.v2.cur_state++;

                        /*sanity check*/
                        if (pstate->metadata.ph_metadata.v2.cur_state > 
                            pstate->metadata.ph_metadata.v2.num_states){
                            s = SYNTIANT_PACKAGE_INCREMENTALLY_PARSING_ERROR;
                            goto error;
                        }

                        if(collect_log){
                            SYNTIANT_PRINTF(
                                "\t - State %u timeout: %u timeout_action:%u timeout_action_arg%u\n",
                                pstate->metadata.ph_metadata.v2.cur_state, 
                                pstate->metadata.ph_metadata.v2.timeout,
                                pstate->metadata.ph_metadata.v2.timeout_action,
                                pstate->metadata.ph_metadata.v2.timeout_action_arg);
                        }
                    }
                }
            }
            pstate->value_mode = 
                (pstate->expected_bytes == 0) ? PACKAGE_MODE_VALID_PARTIAL_VALUE 
                : PACKAGE_MODE_NO_PARTIAL_VALUE;
            pstate->mode = 
                (pstate->partially_read_length < *(uint32_t*)pstate->length) ?
                PACKAGE_MODE_VALUE_CONT : PACKAGE_MODE_TAG_START;

            if (pstate->mode == PACKAGE_MODE_TAG_START){
                if ( pstate->expected_bytes ||
                    !(pstate->metadata.ph_metadata.v2.cur_class == 
                        pstate->metadata.ph_metadata.v2.num_classes &&
                    pstate->metadata.ph_metadata.v2.cur_state == 
                        pstate->metadata.ph_metadata.v2.num_states  
                        )){            
                    s =  SYNTIANT_PACKAGE_ERROR_NN_PH_PARAMETERS_V5;
                        goto error;
                }
            }
            break;
    }

error:
    return s;
}

int 
syntiant_pkg_read_value(
    syntiant_pkg_parser_state_t *pstate, int collect_log){
    int s = SYNTIANT_PACKAGE_ERROR_NONE;

    if ( !(pstate->mode == PACKAGE_MODE_VALUE_START
           || pstate->mode == PACKAGE_MODE_VALUE_CONT) ){
        s = SYNTIANT_PACKAGE_INCREMENTALLY_PARSING_ERROR;
        goto error;
    }

    /* reset mode and partially_read_length when start parsing a new value */
    if (pstate->mode == PACKAGE_MODE_VALUE_START 
        || pstate->mode == PACKAGE_MODE_TAG_START 
        || pstate->mode == PACKAGE_MODE_LENGTH_START){
        pstate->value_mode = PACKAGE_MODE_NO_PARTIAL_VALUE;
        pstate->partially_read_length = 0;
    }

    switch (*(uint32_t*)pstate->tag) {
    case TAG_CHECKSUM:
        s = syntiant_pkg_parse_checksum_value(pstate);
        break;

    case TAG_FIRMWARE_VERSION_STRING_V1:
    case TAG_NN_VERSION_STRING_V1:
    /*case TAG_BOARD_CALIBRATION_PARAMS:*/
    case TAG_PACKAGE_VERSION_STRING:
    case TAG_NDP10X_HW_CONFIG_V2:
    case TAG_BOARD_CALIBRATION_PARAMS_V1:
    case TAG_NDP10X_B0_NN_CONFIG_V2:
    case TAG_BOARD_CALIBRATION_PARAMS_V2:
    case TAG_BOARD_CALIBRATION_PARAMS_V3:
    case TAG_NDP10X_B0_NN_CONFIG_V3:
        s = syntiant_pkg_parse_stored_params(pstate, collect_log);
        break;

    case TAG_NN_LABELS_V1:
    case TAG_FIRMWARE:
    case TAG_NDP10X_NN_PARAMETERS_V3:
        s = syntiant_pkg_parse_partially_stored_params(pstate, collect_log);
        break;

    case TAG_NN_PH_PARAMETERS_V4:
        s = syntiant_pkg_parse_posterior_params(pstate, collect_log);
        break;
    
    case TAG_NN_PH_PARAMETERS_V5:
        s = syntiant_pkg_parse_posterior_params(pstate, collect_log);
        break;


    /*unkonwn tags*/
    default:

        s = SYNTIANT_PACKAGE_ERROR_UNKNOWN_TLV;
        break;
    }
error:
    /* fprintf(stderr, "s is %d, tag is %d", s,*(uint32_t*)pstate->tag ); */
    return s;
}


int 
syntiant_pkg_parse_chunk(syntiant_pkg_parser_state_t *pstate, int collect_log){
    int s = SYNTIANT_PACKAGE_ERROR_NONE;
    if (!pstate->magic_header_found) {
        s = syntiant_pkg_parse_header_value(pstate);
    }
    else {
        switch (pstate->mode) {
        case PACKAGE_MODE_TAG_START:
        case PACKAGE_MODE_TAG_CONT:
            s = syntiant_pkg_read_tag(pstate);
            if (s) goto error;
            break;
        case PACKAGE_MODE_LENGTH_START:
        case PACKAGE_MODE_LENGTH_CONT:
            s = syntiant_pkg_read_length(pstate);
            if (s) goto error;
            break;
        case PACKAGE_MODE_VALUE_START:
        case PACKAGE_MODE_VALUE_CONT:
            s = syntiant_pkg_read_value(pstate, collect_log);
            if (s) goto error;
            break;
        case PACKAGE_MODE_DONE:
            goto error;
        }
    }

error:
    return s;  
}

void
syntiant_pkg_preprocess_chunk(
    syntiant_pkg_parser_state_t *pstate, uint8_t* chunk, int length, int copy){
    if (copy){
        memcpy(pstate->open_ram_begin, chunk, (size_t) length);
    }
    else{
        pstate->open_ram_begin = chunk;
    }
    pstate->open_ram_end = pstate->open_ram_begin + length;
}
