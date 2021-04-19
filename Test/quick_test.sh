#!/bin/bash

RED="\e[31m"
GREEN="\e[32m"
NC="\e[0m"

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

REPO_ROOT=$(git rev-parse --show-toplevel)
CODEC="${REPO_ROOT}/codec/loco_ans_codec"

if ! [[ -f $CODEC ]]; then
  echo "Codec binary not present. Run 'make' in the codec folder"
  exit 1
fi

DATASET_DIR="${REPO_ROOT}/image_dataset/gray8bit"
EXE_PEAK_ERROR="${REPO_ROOT}/tools/get_peak_error/get_peak_error"
WORKING_DIR="/tmp/loco_ans_test"

if ! [[ -d "$WORKING_DIR" ]]; then
  mkdir $WORKING_DIR 2>/dev/null #needed by encode_n_deco_blocks

  if ! [[ "$?" -eq "0" ]]; then
    Print_Err " ERROR: Can't create codec tmp folder . Quiting"
    exit 1
  fi
fi

image="hdr"
min_error=0
max_error=5

if ! [[ -z $1 ]]; then
  image=$1
fi
src_img="${DATASET_DIR}/${image}.pgm"

if ! [[ -f $src_img ]]; then
  echo "Test image does not exist. Path provided: \n $src_img "
  exit 2
fi


if ! [[ -z $2 ]]; then
  min_error=$2
fi

if ! [[ -z $3 ]]; then
  max_error=$3
fi




encoded="${WORKING_DIR}/encoded.jls_ans"
rx_img="${WORKING_DIR}/rx_img.pgm"


for error in $(seq $min_error $max_error)
 do echo -n "$error : " 
 out_string=$( $CODEC 0 $src_img $encoded $error 1)
 bpp_encoder_analysis=$(echo "$out_string" | awk -F " " '/E=/{ print $0}')

  get_file_size=1
  file_bpp=""
  if [[ $get_file_size -ne 0 ]]; then
    #statements
    file_size=$(du -b $encoded|awk '{print $1}')
    rows=$(identify -format "%h" $src_img)
    cols=$(identify -format "%w" $src_img)
    pixels=$(echo "$rows * $cols" |bc)
    bpp=$(echo "scale=4;${file_size} * 8 /${pixels}" |bc )
    file_bpp="File bpp= $bpp"

    deco_out=$( $CODEC 1 $encoded $rx_img)
    peak_error=$($EXE_PEAK_ERROR $src_img $rx_img)

    deco_result="$GREEN OK ($peak_error)$NC"
    if [[ $peak_error -gt $error ]]; then
      deco_result="$RED Error ($peak_error)$NC"
    fi

  fi

  echo -e  "$bpp_encoder_analysis | $file_bpp | $deco_result "

done
