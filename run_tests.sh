#!/bin/bash

DATA_SETS[0]="Rawzor/rgb8bit"
NUM_OF_DATA_SETS=1

SCRIPT=$(readlink -f "$0")
SCRIPTPATH=$(dirname "$SCRIPT")
CODEC_PATH=$(dirname $SCRIPTPATH)

REPO_ROOT=$(git rev-parse --show-toplevel)
BASE_PATH="${REPO_ROOT}/Test_Img"

CODEC_BIN=${CODEC_PATH}/Release/opt_jpegls_codec
EXE_PEAK_ERROR="${REPO_ROOT}/tools/get_peak_error/get_peak_error"
MSE_BINARY="${REPO_ROOT}/tools/get_mse/get_mse"

CSV_FILE="${CODEC_PATH}/Tests/jpeg_ans_data_test_complete_test.csv"
LS_MAX_ERROR=20

WORKING_DIR="/tmp/LOCO_ANS"

TABLE_HEADER="db,img num,blk rows,blk cols,chroma,row,col,codec,codec_version,config,estim_entropy,estim_bpp,bpp,av_ee_iter,error_range,mse,mssim,ssim,note"


Test_tags=( v0.2_Ntc4_Stcg5_ANS4_NI7 v0.2_Ntc5_Stcg6_ANS5_NI7 v0.2_Ntc5_Stfg6_ANS5_NI7 v0.2_Ntc6_Stcg7_ANS6_NI7 v0.2_Ntc6_Stcg8_ANS7_NI7 v0.2_Ntc6_Stfg8_ANS7_NI7 v0.2_Ntc4_Stcg5_ANS4_NIinf v0.2_Ntc5_Stcg6_ANS5_NIinf v0.2_Ntc5_Stfg6_ANS5_NIinf v0.2_Ntc6_Stcg7_ANS6_NIinf v0.2_Ntc6_Stcg8_ANS7_NIinf v0.2_Ntc6_Stfg8_ANS7_NIinf )
# Test_tags=( v0.2_Ntc4_Stcg5_ANS4_NI7 v0.2_Ntc5_Stcg6_ANS5_NI7  )
CODEC_BUILD_DIR=${CODEC_PATH}/Release

note=""
if ! [[ -z $1 ]]; then
  note="$1"
fi

###########  FUNCTION DEFINITION ###################
## --------------------------------------------

  DEF_COLOR="\e[39m"
  MSJ_COLOR="\e[32m"
  WAR_COLOR="\e[33m"
  ERR_COLOR="\e[31m"
  Print_Msg(){
    time=$(date|awk '{print $4}')
    echo -e " ${time}: ${MSJ_COLOR} $@ ${DEF_COLOR}"
  }

  Print_War(){
   time=$(date|awk '{print $4}')
    echo -e " ${time}: ${WAR_COLOR} $@ ${DEF_COLOR}"
  }

  Print_Err(){
    time=$(date|awk '{print $4}')
    echo -e " ${time}: ${ERR_COLOR} $@ ${DEF_COLOR}"
  }

  test_data_set(){ # $data_base 
    database=${1}
    img_num=0
    src_img="${WORKING_DIR}/loco_src_img.pgm"
    encoded="${WORKING_DIR}/encoded.jls"
    decoded="${WORKING_DIR}/decoded_img.pgm"

    for filename in ${BASE_PATH}/${database}/*.p*; do
        
      ffmpeg -i $filename $src_img -hide_banner -loglevel error -y
      image_name=$( basename $filename)
      for (( error = 0; error <= $LS_MAX_ERROR ; error++ )); do
        alpha=$((1+2*error))
        echo -ne "\r Testing JPEG-LS $filename with error $error "
        rows=$(identify -format "%h" $src_img)
        cols=$(identify -format "%w" $src_img)
        
        encoder_out=$($CODEC_BIN 0 $src_img $encoded $alpha $rows $cols 1)

        blk_rows="0"
        blk_cols="0"
        chroma="GRAY"
        codec="JPEG_LS_ANS"
        version=$codec_version
        config=$(printf '%02d'  $error)
        estim_entropy=$(echo "$encoder_out" |awk -F " " '/E=/{print $2 }' )
        estim_bpp=$(echo "$encoder_out" |awk -F " " '/E=/{print $6 }' )
        av_ee_iter=$(echo "$encoder_out" |awk -F " " '/E=/{print $13}' )
        err_range=$(( 1 + 2* $error))

        pixels=$(echo "$rows * $cols" |bc)
        file_size=$(du -b $encoded|awk '{print $1}')
        bpp=$(echo "scale=4;${file_size} * 8 /${pixels}" |bc )


        decoder_out=$($CODEC_BIN 1 $encoded $decoded)
        
        mse=$($MSE_BINARY $src_img $decoded )
        mssim=$(ssimdiff -CC 6 -lin $src_img $decoded)
        ssim=$(ssimdiff -CC 6 -nowav -lin $src_img $decoded)

        peak_error=$($EXE_PEAK_ERROR $src_img $decoded)
        if [[ $peak_error -gt $error ]]; then
          Print_Err " Error ($peak_error)"
          note+="| max error fail ($peak_error)"
        fi

        to_csv_file="${database},${image_name},${blk_rows},${blk_cols},${chroma}"
        to_csv_file+=",${rows},${cols},${codec},$version,$config,${estim_entropy}"
        to_csv_file+=",${estim_bpp},${bpp},${av_ee_iter},${err_range},${mse},${mssim},${ssim},${note}"
      
        echo -e $to_csv_file >> $CSV_FILE
      done
      (( img_num++ ))
      ((test_counter++))
    done
    Print_War "End of Dataset"
  }




###########  Start of Script ###################
## --------------------------------------------
test_counter=0

current_codec_version=$(git tag --points-at HEAD)
if [[ $current_codec_version == '' ]]; then
  # no tag, use commit instead
  current_codec_version=$(git rev-parse HEAD)
fi

if [[ $(git diff --stat -- . ':!Tests') != '' ]]; then
  current_codec_version+="_dirty"

  Print_War "Working tree is dirty. Type 'y' to continue with the test"  
  read ans
  if ! [[ $ans == "y" ]]; then
    exit 1
  fi
fi


if  [[ -f "$CSV_FILE" ]]; then
  Print_War "Warning: out CSV file exist"   
  Print_War "Append(A/Default) or Overwrite(Ov)?"
  read ans
  if [[ $ans == "Ov" ]]; then
    #Delete content and save header
    echo $TABLE_HEADER >$CSV_FILE
  fi
else
  #Create and save header
  echo $TABLE_HEADER >$CSV_FILE
fi



if ! [[ -d "$WORKING_DIR" ]]; then
  mkdir $WORKING_DIR 2>/dev/null #needed by encode_n_deco_blocks

  if ! [[ "$?" -eq "0" ]]; then
    Print_Err " ERROR: Can't create codec tmp folder . Quiting"
    exit 1
  fi
fi


for version in ${Test_tags[@]}; do
  Print_Msg "************ Testing  $version ************ "
  git checkout $version > /dev/null

  codec_version=$(git tag --points-at HEAD)
  if [[ $codec_version == '' ]]; then
    # no tag, use commit instead
    codec_version=$(git rev-parse HEAD)
  fi

  cd $CODEC_BUILD_DIR
  make clean >/dev/null; make all >/dev/null

  if ! [[ -f $CODEC_BIN ]]; then
    Print_Err "Compilation failed"
    exit 2
  fi
  cd $CODEC_PATH
  for (( data_set_idx = 0; data_set_idx < $NUM_OF_DATA_SETS; data_set_idx++ )); do
    Print_Msg "****  Start Testing Dataset: ${DATA_SETS[$data_set_idx]} *****"
    test_data_set ${DATA_SETS[$data_set_idx]}

  done
done

git checkout $current_codec_version


Print_Msg "number of tests: ${test_counter}"

if [[ "$warning_cnt" -ne 0 ]]; then
  Print_War Number of warnings: $warning_cnt
fi



