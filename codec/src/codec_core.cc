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
#include "codec_core.h"

#include "color_space_transforms.h"
#include "img_proc_utils.h"
#include "core_analysis_utils.h"
#include "context.h"
#include "ANS_coder.h"

#include <array>
#include <iomanip>
#include <fstream>

int prediction_errors[512]={0};

/*
*##################   Encoder  ########################
*/
  int UQ(int error, int delta, int near){
    if(error > 0){
      error = (near + error)/delta;
    }else{
      error = -(near - error)/delta;
    }
    return error;
  }

  int predict(int a, int b, int c){
    int min_ab = std::min(b,a);
    int max_ab = std::max(b,a);

    if(c >= max_ab){
      return min_ab;
    }else if(c <= min_ab){
      return max_ab;
    }else{
      return a + b - c;
    }
  }

  size_t image_scanner(const cv::Mat& src,uint8_t* binary_file,int near, int  &geometric_coder_iters, bool analysis_enabled = false){
    //set run parameters
      const int delta = 2*near +1;
      const int alpha = near ==0?MAXVAL + 1 :
                       (MAXVAL + 2 * near) / delta + 1;
      #if ERROR_REDUCTION
        const int MIN_REDUCT_ERROR = -near;
        const int MAX_REDUCT_ERROR =  MAXVAL + near;
        const int DECO_RANGE = alpha * delta;               
        const int MIN_ERROR = -std::floor(alpha/2.0);
        const int MAX_ERROR =  std::ceil(alpha/2.0) -1;
      #endif
      const int remainder_reduct_bits = std::floor(std::log2(float(delta)));
      const int chn=0;

    //variable init
      for(auto & val: ctx_cnt){val=ctx_initial_cnt;}
      for(auto & val: ctx_acc){val=0;}
      for(auto & val: ctx_mean){val=0;}
      int ctx_initial_p_idx = std::max(CTX_NT_HALF_IDX>>1 ,CTX_NT_HALF_IDX -2 -near);
      for(auto & val: ctx_p_idx){val=ctx_initial_p_idx;}
      for(auto & val: ctx_Nt){val=ctx_initial_Nt;} 

      const int ctx_initial_St = std::max(2, ((alpha + 32) >> 6 ))<<CTX_ST_PRECISION;
      for(auto & val: ctx_St){val=ctx_initial_St;}  

    Symbol_Coder symbol_coder(binary_file);

    RowBuffer row_buffer(src.cols);
    
    //analysis
      theoretical_bits = 0;
      theoretical_entropy = 0;

    // store first px 
      unsigned char channel_value =get_value(src,0,0,chn); 
      symbol_coder.store_pixel(channel_value);
      #if ANALYSIS_CODE
        theoretical_bits +=INPUT_BPP;
        theoretical_entropy +=INPUT_BPP;
      #endif
      row_buffer.update(channel_value,0);


    int init_col = 1;
    for (int row = 0; row < src.rows; ++row){
      row_buffer.start_row();
      for (int col = init_col; col < src.cols; ++col){
        channel_value =get_value(src,row,col,chn); 
        int a,b,c,d;
        row_buffer.get_teplate(col,a,b,c,d);

        Context_t context = map_gradients_to_int(d - b, b - c, c - a);
        int fixed_prediction = predict(a, b, c);

        int prediction  = clamp(get_context_bias(context) + fixed_prediction);
        int error = int(channel_value) - prediction;
        error *=context.sign;
        error = (ctx_acc[context.id] >0)?- error:error; //sign flip
        // error = (ctx_acc[context.id] >0 && context.id != CTX_0)?- error:error; //sign flip
        error = UQ(error, delta,near ); // quantize
        #if ERROR_REDUCTION
          if(unlikely(error < MIN_ERROR)){
            error += alpha;
          }else if(unlikely(error > MAX_ERROR)){
            error -= alpha;
          }
        #endif

        ee_symb_data symbol;
          symbol.y = error <0? 1:0;
          symbol.z = abs(error)-symbol.y;
          symbol.theta_id = get_context_theta_idx(context);
          symbol.p_id = ctx_p_idx[context.id];
          symbol.remainder_reduct_bits = remainder_reduct_bits;
        

        // get decoded value
        // int q_error = (ctx_acc[context.id] >0 && context.id != CTX_0)?- delta*error: delta*error;
        int q_error = (ctx_acc[context.id] >0 )? -delta*error: delta*error;
        int q_channel_value = (prediction + q_error*context.sign);

        #if ERROR_REDUCTION
          if(unlikely(q_channel_value < MIN_REDUCT_ERROR)){
            q_channel_value += DECO_RANGE;
          }else if(unlikely(q_channel_value > MAX_REDUCT_ERROR)){
            q_channel_value -= DECO_RANGE;
          }
        #endif 

        q_channel_value = clamp(q_channel_value);
        assert( !(near ==0) || (q_channel_value == channel_value));

        row_buffer.update(q_channel_value,col);


        // entropy encoding
        symbol_coder.push_symbol(symbol);

          #if ANALYSIS_CODE
          if(unlikely(analysis_enabled)) {
            estimate_entropy(symbol,context,near);
            estimate_code_length(symbol,context);
          }
          #endif
          update_context(context, q_error,symbol.z,symbol.y);
    
      }
      init_col = 0;
      row_buffer.end_row();
    }

    symbol_coder.code_symbol_buffer(); // encode and insert contents of buffers in file
    geometric_coder_iters = symbol_coder.get_geometric_coder_iters();
    return symbol_coder.get_out_file_size();
  }

  uint32_t encode_core(const cv::Mat& src,cv::Mat & quant_img,uint8_t* binary_file, char chroma_mode,
    char _fixed_prediction_alg, int near, char encoder_mode,std::string csv_prefix){
    // param setting and init

      assert(chroma_mode == CHROMA_MODE_GRAY); //Other modes are not currently supported 

      if((get_num_of_symbs(src.rows,src.cols,chroma_mode) % EE_BUFFER_SIZE) != 0) {
        std::cerr<<"Warning: possible codification inefficiency due to codification block misalign \n";
      }


    //encode
      cv::Mat encoder_input_img;
      if(src.channels()== 3 ){ //I assume it's RGB image 
        rgb2yuv(src, encoder_input_img,chroma_mode);
      }else{
        encoder_input_img = src; //shallow copy
      }

      bool analysis_enabled = (encoder_mode !=0) ;
      int geometric_coder_iters;

      uint32_t file_size = image_scanner(encoder_input_img,binary_file,near,geometric_coder_iters,analysis_enabled);

    //output analysis
    #if ANALYSIS_CODE
    if(analysis_enabled) {
      float num_symb = get_num_of_symbs(src.rows,src.cols,chroma_mode);

      printf("E= %.4lf , Estim. bpp= %.4lf , bpp= %.4lf , Avg iters= %.4lf ,\n",
                                  theoretical_entropy/num_symb, 
                                  theoretical_bits/num_symb,
                                  file_size*8.0/num_symb,
                                  float(geometric_coder_iters)/num_symb);
    }
    #endif
    return file_size;
  }




/*
*##################   Decoder  ########################
*/

  void binary_scanner(unsigned char* block_binary,cv::Mat& decoded_img,int near){
    //set run parameters
      const int delta = 2*near +1;
      const int alpha = near ==0?MAXVAL + 1 :
                       (MAXVAL + 2 * near) / delta + 1;
      const  uint bit_reduction = std::floor(std::log2(delta));
      const uint escape_bits = EE_REMAINDER_SIZE - bit_reduction; 
      const int chn = 0 ;

      #if ERROR_REDUCTION
        const int DECO_RANGE = alpha * delta;
        const int MIN_REDUCT_ERROR = -near;
        const int MAX_REDUCT_ERROR =  MAXVAL + near;
      #endif 

    int num_of_symbols = get_num_of_symbs(decoded_img.rows,decoded_img.cols,CHROMA_MODE_GRAY);
    Binary_Decoder bin_decoder(block_binary,num_of_symbols);
    RowBuffer row_buffer(decoded_img.cols);

    //variable init 
      for(auto & val: ctx_cnt){val=ctx_initial_cnt;}
      for(auto & val: ctx_acc){val=0;}
      for(auto & val: ctx_mean){val=0;}

      int ctx_initial_p_idx = std::max(CTX_NT_HALF_IDX>>1 ,CTX_NT_HALF_IDX -2 -near);
      for(auto & val: ctx_p_idx){val=ctx_initial_p_idx;}
      for(auto & val: ctx_Nt){val=ctx_initial_Nt;} 

      const int ctx_initial_St = std::max(2, ((alpha + 32) >> 6 ))<<CTX_ST_PRECISION;

      for(auto & val: ctx_St){val=ctx_initial_St;}


    for (int row = 0; row < decoded_img.rows; ++row){
      row_buffer.start_row();
      for (int col = 0; col < decoded_img.cols; ++col){
          if (unlikely(row==0 )){
            if (unlikely(col==0 )){
              int channel_value = bin_decoder.retrive_pixel();
              row_buffer.update(channel_value,col);
              decoded_img.data[chn] = channel_value; // just .data[chn] cause row=col=0
              continue;
            }
          } 
          int a,b,c,d;
          row_buffer.get_teplate(col,a,b,c,d);
          Context_t context = map_gradients_to_int(d - b, b - c, c - a);
          int fixed_prediction = predict(a, b, c);
          int prediction = clamp(get_context_bias(context) + fixed_prediction);


           // entropy decoding
          int z,y,q_error;
          bin_decoder.retrive_TSG_symbol(get_context_theta_idx(context),ctx_p_idx[context.id],escape_bits,z,y);

          int error = y ==1? -z -1:z;
          q_error = error*delta;
          q_error = (ctx_acc[context.id] >0)?-q_error:q_error;
          // q_error = (ctx_acc[context.id] >0 && context.id != CTX_0)?-q_error:q_error;
          int deco_val = (prediction + q_error*context.sign);
        
          #if ERROR_REDUCTION 
            if(unlikely(deco_val < MIN_REDUCT_ERROR)){
              deco_val += DECO_RANGE;
            }else if(unlikely(deco_val > MAX_REDUCT_ERROR)){
              deco_val -= DECO_RANGE;
            }
          #endif 

        int q_channel_value = clamp(deco_val);


        row_buffer.update(q_channel_value,col);
        update_context(context, q_error,z,y);
        
        // store in output image
        decoded_img.data[decoded_img.step[0]*row+decoded_img.step[1]*col+chn] = q_channel_value;
      }
      row_buffer.end_row();
    }

  }





  void decode_core(unsigned char* in_file ,cv::Mat& decode_img,char chroma_mode,
                        char _fixed_prediction_alg , int near , uint ee_buffer_size, 
                        char encoder_mode){

    assert(chroma_mode == CHROMA_MODE_GRAY); //Other modes are not currently supported 
    int num_chn= chroma_mode == CHROMA_MODE_GRAY? 1:decode_img.channels();
    
    binary_scanner(in_file,decode_img,near);

    if (num_chn==3 && encoder_mode!=1) 
    {
      #if DEBUG_DEC
        {
          cv::namedWindow("yuv",cv::WINDOW_NORMAL);
          cv::imshow("yuv",yuv_img);
        }
      #endif
         
      yuv2rgb(decode_img,decode_img);
    }
  }



