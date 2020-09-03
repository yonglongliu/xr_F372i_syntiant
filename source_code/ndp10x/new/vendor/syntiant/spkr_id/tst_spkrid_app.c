/*HEADER******************************************************************
**************************************************************************
***
*** Copyright (c) Meeami
*** All rights reserved
***
*** This software embodies materials and concepts that are
*** confidential to Meeami and is made available solely pursuant
*** to the terms of a written license agreement with Meeami.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include "syntiant_st_speaker_id.h"
//Start of the test wrapper
int main(int argc, char *argv[])
{
  FILE *enroll_fp[10], *test_fp, *fp_model;
  void *pSpkrid_handle; //Holds the SPKRID INSTANTCE
  short *test_ipbuf, **ipbuf;
  char user_model[64], c_l;
  int cfd_scr, ww_len, *user_model_bin, i_l, user_model_sz, num_utt, utt_type = 0, ret_val = 0;
  short opt_cnt;
  unsigned int user_id;
  uint8_t snr_val;

  if (argc != 1)
  {
    printf("\n\nTST WPR :: INVALID COMMAND LINE ARUGUMENT - ./app\n\n");
    return 1;
  }

  enroll_fp[0] = fopen("enroll_1.bin", "rb");
  enroll_fp[1] = fopen("enroll_2.bin", "rb");
  enroll_fp[2] = fopen("enroll_3.bin", "rb");
  enroll_fp[3] = fopen("enroll_4.bin", "rb");
  enroll_fp[4] = fopen("enroll_5.bin", "rb");
  enroll_fp[5] = fopen("enroll_6.bin", "rb");
  enroll_fp[6] = fopen("enroll_7.bin", "rb");
  enroll_fp[7] = fopen("enroll_8.bin", "rb");
  enroll_fp[8] = fopen("enroll_9.bin", "rb");
  enroll_fp[9] = fopen("enroll_10.bin", "rb");
  test_fp = fopen("test.bin", "rb");

  num_utt = 5;
  opt_cnt = 14;
  ww_len = 33600;

  ret_val = syntiant_st_speaker_id_engine_init(5, ww_len, opt_cnt, num_utt, &pSpkrid_handle, &user_model_sz);

  user_model_bin = (int *)malloc(user_model_sz);
  if (pSpkrid_handle == NULL)
  {
    printf("TST WPR :: EERROR: UNABLE TO ALLOCATE MEM FOR SPKRID INST \n");
    return 1;
  }
  syntiant_st_speaker_id_engine_get_opt_pnt(pSpkrid_handle, &opt_cnt);
  printf("TST WPR :: Current operating point for speaker id engine: %d ret_val %d\n", opt_cnt, ret_val);

  // Initialize SPKRID
  if (ret_val != SYNTIANT_ST_SPEAKER_ID_ERROR_NONE)
  {
    printf("TST WPR :: FAIL: syntiant_st_speaker_id_sm_init\n");
    return 1;
  }
  else
    printf("TST WPR :: syntiant_st_speaker_id_sm_init Success \n");

  ipbuf = (short **)malloc(10 * sizeof(short *));

  for (i_l = 0; i_l < 10; i_l++)
  {
    if (enroll_fp[i_l] != NULL)
    {
      ipbuf[i_l] = (short *)malloc(sizeof(short) * ww_len);
      fread(ipbuf[i_l], sizeof(short), ww_len, enroll_fp[i_l]);
    }
    else
    {
      printf("TST WPR :: Please provide input files\n");
      return -1;
    }
  }
  if (test_fp != NULL)
  {
    test_ipbuf = (short *)malloc(sizeof(short) * ww_len);
    fread(&test_ipbuf[0], sizeof(short), ww_len, test_fp);
  }
  else
  {
    printf("TST WPR :: Provide the test.bin file\n");
    return -1;
  }

  printf("key\tfunction\n\'q\'\tquit\n\'t\'\ttrain user\n\'a\'\tadd user\n\'g\'\tget user\n\'v\'\tverify user\n\'r\'\tremove user\n\'s\'\tto change operating point\n\'e\'\tto validate utterance\n");

  while (1)
  {
    c_l = getchar();
    if (c_l == 'q')
      break;
    else if (c_l == 't')
    {
      printf("TST WPR :: Enter the speaker user ID: ");
      scanf("%d", &user_id);
      ret_val = syntiant_st_speaker_id_engine_train_user(pSpkrid_handle, user_id, num_utt, ww_len, ipbuf);

      if (ret_val == SYNTIANT_ST_SPEAKER_ID_ERROR_NEED_MORE_AUDIO)
        printf("TST WPR :: Need more number of utterances.\n");
      else if (ret_val == SYNTIANT_ST_SPEAKER_ID_ERROR_NEED_LONGER_AUDIO)
        printf("TST WPR :: Audio data provided is not sufficient.\n");
      else if (ret_val == SYNTIANT_ST_SPEAKER_ID_ERROR_MODEL_USER_ALREADY_EXISTS)
        printf("TST WPR :: user_id already exists.\n");
      else if (ret_val == SYNTIANT_ST_SPEAKER_ID_ERROR_MODEL_TOO_MANY_USERS)
        printf("TST WPR :: Exceeded the number of users supported.\n");
      else if (ret_val == SYNTIANT_ST_SPEAKER_ID_ERROR_NONE)
        printf("TST WPR :: Enrollment completed.\n");
    }
    else if (c_l == 'a')
    {
      printf("TST WPR :: Enter the speaker user ID: ");
      scanf("%d", &user_id);
      sprintf(user_model, "user_%d.spk", user_id);
      fp_model = fopen(user_model, "r");
      if (fp_model == NULL)
      {
        printf("TST WPR :: There is no user model with id = %s to add\n", user_model);
      }
      else
      {
        fread(user_model_bin, 1, user_model_sz, fp_model);
        fclose(fp_model);
        ret_val = syntiant_st_speaker_id_engine_add_user((void *)pSpkrid_handle, user_id, (void *)user_model_bin, user_model_sz);
        if (ret_val == SYNTIANT_ST_SPEAKER_ID_ERROR_MODEL_USER_ALREADY_EXISTS)
          printf("TST WPR :: user_id already exists.\n");
        else if (ret_val == SYNTIANT_ST_SPEAKER_ID_ERROR_MODEL_TOO_MANY_USERS)
          printf("TST WPR :: Exceeded the number of users supported.\n");
        else if (ret_val == SYNTIANT_ST_SPEAKER_ID_ERROR_INCOMPATIBLE_USER_MODEL)
          printf("TST WPR :: User model version not matching.\n");
        else if (ret_val == SYNTIANT_ST_SPEAKER_ID_ERROR_NONE)
          printf("TST WPR :: Added user successfully.\n");
      }
    }
    else if (c_l == 'v')
    {
      printf("TST WPR :: Enter the SNR value of the utterance: ");
      scanf("%" SCNu8, &snr_val);
      ret_val = syntiant_st_speaker_id_engine_identify_user(pSpkrid_handle, ww_len, test_ipbuf, &user_id, &cfd_scr, snr_val);

      utt_type = ret_val;
      printf("TST WPR :: utt_type: %x, ret_val: %x\n", utt_type, ret_val);

      if (utt_type == SYNTIANT_ST_SPEAKER_ID_ERROR_NOISY_ENV_UTT)
        printf("TST WPR :: Your are in Noisy Environment. Please Maintain Noise Levels.\n");
      else if (utt_type == SYNTIANT_ST_SPEAKER_ID_ERROR_SOFT_SPOKEN_UTT)
        printf("TST WPR :: Looks like you are too SOFT !! Can you speak bit Louder.\n");
      else if (utt_type == SYNTIANT_ST_SPEAKER_ID_ERROR_LOUD_SPOKEN_UTT)
        printf("TST WPR :: Utterance is saturated. Please keep distance from phone\n");
      else if (utt_type == SYNTIANT_ST_SPEAKER_ID_ERROR_NONE)
        printf("TST WPR :: Utterance is proper.\n");

      if (user_id == 0)
        printf("TST WPR :: No Such user is in enrolled.\n");
      else
        printf("TST WPR :: Speaker Name: %d\n", user_id);
    }
    else if (c_l == 'g')
    {
      printf("TST WPR :: Enter the speaker user ID: ");
      scanf("%d", &user_id);
      ret_val = syntiant_st_speaker_id_engine_get_user_model(pSpkrid_handle, user_id, user_model_sz, user_model_bin);
      if (ret_val == SYNTIANT_ST_SPEAKER_ID_ERROR_MODEL_NO_SUCH_USER)
        printf("TST WPR :: No Such user is in enrolled.\n");
      else
      {
        sprintf(user_model, "user_%d.spk", user_id);
        fp_model = fopen(user_model, "w");
        fwrite(user_model_bin, 1, user_model_sz, fp_model);
        fclose(fp_model);
      }
    }
    else if (c_l == 'r')
    {
      printf("TST WPR :: Enter the speaker user ID: ");
      scanf("%d", &user_id);
      ret_val = syntiant_st_speaker_id_engine_remove_user(pSpkrid_handle, user_id);
      if (ret_val == SYNTIANT_ST_SPEAKER_ID_ERROR_MODEL_NO_SUCH_USER)
        printf("TST WPR :: No Such user is in enrolled.\n");
      else if (ret_val == SYNTIANT_ST_SPEAKER_ID_ERROR_NONE)
        printf("TST WPR :: Removed the user %d sucessfully\n", user_id);
    }
    else if (c_l == 's')
    {
      int tmp;
      printf("TST WPR :: Enter the operating point to set: ");
      scanf("%d", &tmp);
      opt_cnt = tmp;
      ret_val = syntiant_st_speaker_id_engine_set_opt_pnt(pSpkrid_handle, opt_cnt);
      if (ret_val == SYNTIANT_ST_SPEAKER_ID_ERROR_INVALID_OPT_PT)
        printf("TST WPR :: Given an invalid operating point. Enter a valid operating point.\n");
      else if (ret_val == SYNTIANT_ST_SPEAKER_ID_ERROR_NONE)
        printf("TST WPR :: operating point is set successfully\n");
      syntiant_st_speaker_id_engine_get_opt_pnt(pSpkrid_handle, &opt_cnt);
      printf("TST WPR :: Current operating point for speaker id engine: %d\n", opt_cnt);
    }
    else if (c_l == 'e')
    {
      ret_val = syntiant_st_speaker_id_engine_enroll_utt_validation(test_ipbuf, ww_len);

      if (ret_val == SYNTIANT_ST_SPEAKER_ID_ERROR_NONE)
        printf("TST WPR :: Utterance is proper for Enrollment \n");
      else if (ret_val == SYNTIANT_ST_SPEAKER_ID_ERROR_NEED_LONGER_AUDIO)
        printf("TST WPR :: Length of Audio Recording is Insufficient \n");
      else if (ret_val == SYNTIANT_ST_SPEAKER_ID_ERROR_SATURATED_UTT)
        printf("TST WPR :: Utterance is mostly Saturated \n");
      else if (ret_val == SYNTIANT_ST_SPEAKER_ID_ERROR_NOISY_ENV_UTT)
        printf("TST WPR :: Your are in too Noisy Environment. Please maintain Noise Levels \n");
      else if (ret_val == SYNTIANT_ST_SPEAKER_ID_ERROR_SOFT_SPOKEN_UTT)
        printf("TST WPR :: Looks like you are too SOFT !! Can you speak bit Louder. \n");
    }
  }

  syntiant_st_speaker_id_engine_uninit(pSpkrid_handle);

  //close the file pointers
  fclose(enroll_fp[0]);
  fclose(enroll_fp[1]);
  fclose(enroll_fp[2]);
  fclose(enroll_fp[3]);
  fclose(enroll_fp[4]);
  fclose(enroll_fp[5]);
  fclose(enroll_fp[6]);
  fclose(enroll_fp[7]);
  fclose(enroll_fp[8]);
  fclose(enroll_fp[9]);
  fclose(test_fp);

  return 0;
}
