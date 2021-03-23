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

#ifndef CODEC_H
#define CODEC_H

#include "codec_core.h"
#include <opencv2/imgproc/imgproc.hpp> //cv::Mat


#include <stdint.h>
#include <iostream>
#include <fstream>

//#include <omp.h>

//int threads = 3;


struct global_header {
  uint8_t predictor:2;
  uint8_t color_profile:4;
  uint8_t version :2;

  uint8_t ee_buffer_exp:3; // buffer_size = 32* 2^ee_buffer_exp
  uint8_t NEAR:5;

  uint16_t blk_height; //(block height)
  uint16_t blk_width;  //(block width)

  uint16_t blk_rows; // img height = blk_height * blk_rows
  uint16_t blk_cols; // img width = blk_width * blk_cols
}__attribute__((packed));

struct block_header {
  uint32_t size; //in bytes
}__attribute__((packed));



int encoder(const cv::Mat& src_img,char* out_file,int block_width=128,
                    int block_height=8, int chroma_samp=0 , char prediction = ENCODER_PRED_LOCO, 
                      int NEAR = 0,
                      char encoder_mode = ENCODER_MODE_ENCODE, 
                      std::string csv_prefix= std::string());
//int encoder(const cv::Mat& src_img,char* out_file,int block_width=128,int block_height=8, char chroma_samp=0);


int decoder(char* in_file,cv::Mat &dst_img);

#endif /* CODEC_H */
