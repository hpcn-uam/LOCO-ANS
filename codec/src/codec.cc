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

#include "codec.h"
#include <libgen.h>

void interpol_yuv440(const cv::Mat& src,cv::Mat&  dst){
  cv::Mat tmp=src.clone();
  for (int row = 0; row < src.rows; row+=2){
    for (int col = 0; col < src.cols; ++col){
      tmp.at<cv::Vec3b>(row+1,col)[1]= src.at<cv::Vec3b>(row,col)[1] ; //u
      tmp.at<cv::Vec3b>(row+1,col)[2]= src.at<cv::Vec3b>(row,col)[2] ; //v
    }
  }

  dst=tmp.clone();
}

int encoder(const cv::Mat& src_img,char* out_file,int block_width,int block_height, 
            int chroma_mode, char prediction,int NEAR, char encoder_mode ,std::string csv_prefix){

  if(NEAR > 31) {
    std::cerr<<" The header used in this version does not support NEAR > 31"<<std::endl;
    return 1;
  }
  bool save_to_file = true;


  //get number of image blocks
    uint16_t blk_rows,blk_cols; //image height and width in number of blocks

    if(src_img.rows%block_height==0){
      blk_rows=src_img.rows/block_height;
    }else{
      std::cerr<<"encoder: unsupported image height("<<src_img.rows<<"). It must be multiple of block height ("
               <<block_height<<")"<<std::endl;
      return -2;
    }

    if(src_img.cols%block_width==0){
      blk_cols=src_img.cols/block_width;
    }else{
      std::cerr<<"encoder :unsupported image width("<<src_img.cols<<"). It must be multiple of block width ("
               <<block_width<<")"<<std::endl;
      return -2;
    }

      
    //save file header
      struct global_header header;
      header.color_profile= chroma_mode;
      header.version=0 ;        
      header.predictor = prediction;
      header.ee_buffer_exp = uint(std::log2(2096/32));
      header.NEAR = NEAR ;        
      header.blk_height = block_height;
      header.blk_width = block_width;
      header.blk_rows =blk_rows; 
      header.blk_cols = blk_cols;

      std::ofstream binary_out_file(out_file,std::ios::binary); //out file
      binary_out_file.write((char*)&(header),sizeof(header));
    

  //generate blocks
    cv::Mat block, codec_input_img;
    
    if(src_img.channels()== 3 && chroma_mode == CHROMA_MODE_GRAY){ //I assume it's RGB image 
      rgb2yuv(src_img, codec_input_img,CHROMA_MODE_GRAY);
    }else{
      codec_input_img = src_img;
    }

    int compress_img_size=sizeof(global_header);
    uint8_t* block_buffer;
    uint32_t max_output_size=((block_height*block_width*3)/(sizeof (*block_buffer))+1); //for no chroma sub-sampling
    block_buffer = new uint8_t[max_output_size];

    for (uint16_t blk_row = 0; blk_row < blk_rows; ++blk_row) {
      for (uint16_t blk_col = 0; blk_col < blk_cols; ++blk_col) {

        block=codec_input_img(cv::Range(blk_row*block_height,(blk_row+1)*block_height),
                      cv::Range(blk_col*block_width,(blk_col+1)*block_width)); //map a portion of the input image to a block
        cv::Mat quant_block;
        uint32_t out_file_size;

        std::string csv_prefix_updated(csv_prefix+","+std::to_string(blk_row)
                                          +","+std::to_string(blk_col));

        if(block_height==1 && chroma_mode==CHROMA_MODE_YUV420){
          if (blk_row%2!=0) {
            out_file_size=encode_core(block,quant_block,block_buffer,CHROMA_MODE_GRAY,
                                prediction,NEAR, encoder_mode,csv_prefix_updated);
          }else{
            out_file_size=encode_core(block,quant_block,block_buffer,CHROMA_MODE_YUV422,
                                prediction,NEAR, encoder_mode,csv_prefix_updated);
          }
        }else{
         out_file_size=encode_core(block,quant_block,block_buffer,chroma_mode,
                            prediction,NEAR, encoder_mode,csv_prefix_updated);
        }

        compress_img_size+= (int)out_file_size;
        compress_img_size+= sizeof(block_header);
        if(save_to_file) {
          //store block header
            struct block_header block_header;
            block_header.size = out_file_size;
            binary_out_file.write((char*)&(block_header),sizeof(block_header));

          binary_out_file.write((char*)block_buffer,out_file_size); //store block data
        }
      }
    }


    binary_out_file.close();
  

    delete[] block_buffer;
    return compress_img_size;
}



int decoder(char* in_file,cv::Mat &dst_img){
  //open LOCO-ANS-coded image
    std::ifstream binary_in_file(in_file, std::ios::binary );

  //extract file header
    struct global_header header;
    binary_in_file.read((char*)&header,sizeof(header));

    int format_version = header.version;
    if(format_version != 0 ) {
      std::cerr<<"Compressed imaged format not supported. version != 0"<<std::endl;
      throw 1;
    }
    
    uint32_t blk_height=header.blk_height;
    uint32_t blk_width=header.blk_width;
    uint32_t blk_rows= header.blk_rows;
    uint32_t blk_cols= header.blk_cols;

    uint32_t img_height = blk_height * blk_rows;
    uint32_t img_width  = blk_width * blk_cols;
    int chroma_mode = header.color_profile;
    uint ee_buffer_size = 32 * pow(2,header.ee_buffer_exp);
    int fix_predictor = header.predictor;
    int NEAR = (int)header.NEAR ;

    std::cout<<" Encoded image configuration ";
    std::cout<<"| NEAR: "<<NEAR; 
    std::cout<<"| blk_height: "<<blk_height; 
    std::cout<<"| blk_width: "<<blk_width; 
    std::cout<<"| img_height: "<<img_height; 
    std::cout<<"| img_width: "<<img_width; 
    std::cout<< std::endl;

    int num_of_channels;
    switch(chroma_mode){
      case CHROMA_MODE_YUV420 :
      case CHROMA_MODE_YUV422 :
      case CHROMA_MODE_YUV444 :
        num_of_channels = 3;
        break;
      case CHROMA_MODE_BAYER :
      case CHROMA_MODE_GRAY :
        num_of_channels = 1;
        break;
      default:
        std::cerr<<"Unknown chroma mode. Quitting"<<std::endl;
        throw 1;
    }

  //variable initiation for decoder loop
    char* block_binary_data;
    size_t max_block_data_size=(blk_height*blk_width*num_of_channels*
                            (1+int(MAX_SUPPORTED_BPP/8)/sizeof(*block_binary_data)));
    block_binary_data = new char[max_block_data_size];

  //loop to decode every block
    if (header.color_profile==CHROMA_MODE_GRAY){
      dst_img=cv::Mat::zeros(img_height,img_width,CV_MAKETYPE(CV_8U,1));
    }else{
      dst_img=cv::Mat::zeros(img_height,img_width,CV_MAKETYPE(CV_8U,3));
    }
    
    cv::Mat block;
    char codec_mode= (chroma_mode==CHROMA_MODE_YUV420 && blk_height==1)? 1 : 0;
    
    //omp_set_num_threads(threads);
    for (uint16_t blk_row = 0; blk_row < blk_rows; ++blk_row) {
    //#pragma omp parallel for

      for (uint16_t blk_col = 0; blk_col < blk_cols; ++blk_col) {
        //map a range of full image to block (they share image data)
          block=dst_img(cv::Range(blk_row*blk_height,(blk_row+1)*blk_height),
                           cv::Range(blk_col*blk_width,(blk_col+1)*blk_width));

        //get block id and length of the generated binary
          struct block_header block_header;
          binary_in_file.read((char*)&(block_header),sizeof(block_header));


        //get binary
          binary_in_file.read(block_binary_data, block_header.size);

        if(blk_height==1 && chroma_mode==CHROMA_MODE_YUV420 && blk_row%2!=0) {
          decode_core((unsigned char*)block_binary_data,block,CHROMA_MODE_GRAY,
                                    fix_predictor, NEAR,ee_buffer_size,codec_mode);
        }else{
          decode_core((unsigned char*)block_binary_data,block,chroma_mode,
                                    fix_predictor, NEAR,ee_buffer_size,codec_mode);
        }


      }
    }

    if(chroma_mode==CHROMA_MODE_YUV420 && blk_height==1) {
      //interpol
      interpol_yuv440(dst_img,dst_img);
      yuv2rgb(dst_img,dst_img);
    }

  delete[] block_binary_data;
  return 0;
}

