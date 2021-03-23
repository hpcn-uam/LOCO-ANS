/*
  Copyright 2021 Tobías Alonso, Autonomous University of Madrid

  This file is part of LOCO-ANS.

  LOCO-ANS is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  LOCO-ANS is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with LOCO-ANS.  If not, see <https://www.gnu.org/licenses/>.


 */

#ifndef CODEC_CORE_H
#define CODEC_CORE_H

// #define NDEBUG
#include <assert.h>

#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui.hpp>

#include <stdint.h>
#include <iostream>
#include <vector>
#include <algorithm>
#include <functional> 
#include <cmath>
#include <cstring> //memcopy

#define ANALYSIS_CODE true
#define ERROR_REDUCTION true
#define Orig_CTX_Quant false
#define MAX_SUPPORTED_BPP 16 // has to be mult of 8
#define INPUT_BPP 8
const int MAXVAL = pow(2,INPUT_BPP)-1;

#if ERROR_REDUCTION
  #define EE_REMAINDER_SIZE (INPUT_BPP-1)
#else
  #define EE_REMAINDER_SIZE (INPUT_BPP)
#endif

#define DEBUG_ENC false
#define DEBUG_DEC false

#define CHROMA_MODE_GRAY 0
#define CHROMA_MODE_YUV420 1
#define CHROMA_MODE_YUV422 2
#define CHROMA_MODE_YUV444 3
#define CHROMA_MODE_BAYER 4

#define QUANT_IDX_OFFSET 128
#define DISCARTED_IDX (2*QUANT_IDX_OFFSET-1)
#define CHN_TO_ANALIZE 0


#define DBG_INFO "FILE: "<<__FILE__<<". FUNC: "<<__func__<<". LINE: "<<__LINE__<<". "
#define BUILD_BUG_ON(condition) ((void)sizeof(char[1 - 2*!!(condition)]))

#define likely(x) __builtin_expect ((x), 1)
#define unlikely(x) __builtin_expect ((x), 0)

#define ENCODER_MODE_ENCODE 0
#define ENCODER_MODE_SYSTEM_TEST 1

#define ENCODER_PRED_LHE 0 
#define ENCODER_PRED_LOCO 1
#define ENCODER_PRED_GAP   2
#define ENCODER_PRED_GAP_2 3
#define ENCODER_PRED_SIMPLE_GAP 4


uint32_t encode_core(const cv::Mat& src,
                          cv::Mat & quant_img, 
                          uint8_t* binary_file,
                          char chroma_mode=CHROMA_MODE_YUV444,
                          char _fixed_prediction_alg = ENCODER_PRED_LOCO, // not currently in use
                          int near = 1, 
                          char encoder_mode=0, 
                          std::string csv_prefix= std::string());

void decode_core(unsigned char* in_file ,cv::Mat& decode_img,
                        char chroma_mode=CHROMA_MODE_YUV444, 
                        char _fixed_prediction_alg = ENCODER_PRED_LOCO, // not currently in use
                        int near = 1,  
                        uint ee_buffer_size = 2096,
                        char mode =0);

void rgb2yuv(const cv::Mat& src,cv::Mat&  dst,char chroma_mode =CHROMA_MODE_YUV444);

void yuv2rgb(const cv::Mat src, cv::Mat& dst,char chroma_mode =CHROMA_MODE_YUV444);


struct Context_t{
  int32_t id;
  int sign;
  Context_t():id(0),sign(0){}
  Context_t(int _id, int _sign):id(_id),sign(_sign){}
};

class RowBuffer
  {
  public:
    int cols;
    int curr_row_idx;
    unsigned char * prev_row;
    unsigned char * current_row;
    RowBuffer(int _cols):cols(_cols),curr_row_idx(0){
      prev_row = new unsigned char[_cols+2];
      current_row = new unsigned char[_cols+2];
      for(int i = 0; i < _cols+2; ++i) {
        prev_row[i]= 0;
        current_row[i]= 0;
      }
    };
    inline unsigned char retrieve(int row,int col) const{
      int row_idx= row&0x1;
      if(row_idx == curr_row_idx) {
        return current_row[col+1];
      }else{
        return prev_row[col+1];
      }
    }
    inline void update(unsigned char px,int col){
      current_row[col+1] =px; 
      current_row[col+2] =px; 
    }
    void start_row(){
      current_row[0]= prev_row[1];
    }

    void end_row(){
      unsigned char * aux = prev_row;
      prev_row = current_row;
      current_row = aux;
      curr_row_idx = curr_row_idx?1:0;
    }
    void get_teplate(int col,
                    int &a,int &b,int &c,int &d) const{
      a = current_row[col];
      b = prev_row[col+1];
      c = prev_row[col];
      d = prev_row[col+2];
    }

    ~RowBuffer(){
      delete [] prev_row;
      delete [] current_row;
    };
    
  };



struct ee_symb_data {
  ee_symb_data():z(0),y(0),remainder_reduct_bits(0),theta_id(0),p_id(0){}
  int32_t z;
  int32_t y;
  int32_t remainder_reduct_bits; //reminder_bits =  EE_REMAINDER_SIZE - remainder_reduct_bits
  int32_t theta_id;
  int32_t p_id;

}__attribute__((packed));



#endif /* CODEC_CORE_H */


