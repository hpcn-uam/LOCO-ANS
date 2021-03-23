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

#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui.hpp>

#include <iostream>
#include <string>
#include <fstream>
#include <algorithm>
#include <iomanip>
#include <cstdlib>
#include <libgen.h>
//#include <filesystem>

#include "codec.h"

#define DEBUG false
#define DECODE true
#define SHOW false

#include <sys/time.h>

using namespace std;

int main(int arg, char** argv ){
  
  int chroma_mode=CHROMA_MODE_GRAY; //default. (chroma_mode!=CHROMA_MODE_GRAY) not currently supported
  int encode_mode=ENCODER_MODE_ENCODE; //default
  int encode_prediction =ENCODER_PRED_LOCO; //default (deprecated)
  int NEAR =0; //default lossless

  if( arg < 3) {
    printf("Args: encode(0)/decode(1) args \n");
    printf("Encode args: 0 src_img_path out_compressed_img_path [NEAR] [encode_mode] [blk_height]  [blk_width]   \n");
    printf("Decode args: 1 compressed_img_path path_to_out_image  \n");
    return 1;
  }


  bool decode= atoi(argv[1]);

  if( ! decode) {
    char * img_path= argv[2];
    char * out_file= argv[3];
  
    cv::Mat img_orig = cv::imread( img_path ,cv::IMREAD_UNCHANGED);
    if(img_orig.empty()){
      std::cerr<<"Empty image file"<<std::endl;
      return 2;
    }

    int blk_height=img_orig.rows;
    int blk_width=img_orig.cols;

    if (arg>4) {
      NEAR = atoi(argv[4]);
    }

    if (arg>5) {
  		encode_mode= atoi(argv[5]);
  	}

    if (arg>6) {
      blk_height = atoi(argv[6]);
    }

    if (arg>7) {
      blk_width = atoi(argv[7]);
    }


    /*if (arg>8) {
      encode_prediction= atoi(argv[8]);
    }

    if (arg>9) {
	 	 chroma_mode= atoi(argv[9]); 
    }*/

    std::cout<<" Encoder configuration ";
    std::cout<<"| NEAR: "<<NEAR; 
    std::cout<<"| blk_height: "<<blk_height; 
    std::cout<<"| blk_width: "<<blk_width; 
    if(encode_mode != ENCODER_MODE_ENCODE) {
      std::cout<<"| encode_mode: "<<encode_mode;
    }
    // std::cout<<"| encode_prediction: "<<encode_prediction;
    std::cout<< std::endl;


 
    if(NEAR < 0) {
      std::cerr<<" Error NEAR should be >= 0"<<std::endl;
      return 1;
    }



    std::string csv_prefix;

    if(img_orig.channels()== 3 && chroma_mode == CHROMA_MODE_GRAY){ //I assume it's RGB image 
      std::cout<<"Warning: Selected chrominance mode: Gray, but source image has 3 channels. Converting to 1 channel image"<<std::endl;
      rgb2yuv(img_orig, img_orig,CHROMA_MODE_GRAY);
    }
    
  //encode
    struct timeval fin,ini;
    gettimeofday(&ini,NULL);
    int compress_img_size=encoder(img_orig,out_file,blk_width,blk_height,
                    chroma_mode,encode_prediction,NEAR,
                                        encode_mode ,csv_prefix);
    gettimeofday(&fin,NULL);

    printf("Encoder time: %.3f | ", ((fin.tv_sec*1000000+fin.tv_usec)-(ini.tv_sec*1000000+ini.tv_usec))*1.0/1000000.0);
    printf(" Achieved bpp: %.3f \n",float(compress_img_size*8)/(img_orig.cols*img_orig.rows));
    if (compress_img_size<0){
      std::cerr<<"there's been an error in trying to encode the image"<<std::endl;
      return -1;
    }

  }else{
    char * compressed_img= argv[2];
    char * out_path= argv[3];
    std::cout<<"Compressed image:"<<compressed_img<<std::endl;
    std::cout<<"Out decoded image path: "<<out_path<<std::endl;

    cv::Mat decode_img;
    struct timeval fin,ini;
    gettimeofday(&ini,NULL);
    int deco_status = decoder(compressed_img,decode_img);
    gettimeofday(&fin,NULL);
    printf("Decoder time: %.3f\n", ((fin.tv_sec*1000000+fin.tv_usec)-(ini.tv_sec*1000000+ini.tv_usec))*1.0/1000000.0);

    if (deco_status)
    {
      std::cerr<<"there's been an error in trying to decode the image"<<std::endl;
      return deco_status;
    }else{
      cv::imwrite(out_path,decode_img);
    }

  }

  return 0;
}


