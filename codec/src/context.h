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

#ifndef CONTEXT_H
#define CONTEXT_H

#include "codec_core.h"
#include <array>
#include <fstream>
#include <iomanip>
#include <math.h>
#include "coder_config.h"


#define ADD_GRAD_4 false // deprecated

#define MU_estim_like_original false

#define CTX_BINS_PER_DIM 9//7 //5  // deprecated 
const int CTX_DIM_OFFSET = (CTX_BINS_PER_DIM-1)/2;

#define CTX_0 (4*9+4)
#define CTX_DIMS 3
//const int CTX_GRAD_BINS = pow(CTX_BINS_PER_DIM,CTX_DIMS);

#if ADD_GRAD_4
  #define CTX_GRAD_BINS (729*3) //343 // 729
#else
  #define CTX_GRAD_BINS (729) //343 // 729
#endif

#define CTX_BINS (CTX_GRAD_BINS ) //125





// Context state variables
// TODO replace std::arrays for normal arrays
std::array<long, CTX_BINS> ctx_cnt={0};

std::array<long, CTX_BINS> ctx_acc={0};
std::array<long, CTX_BINS> ctx_mean={0};

std::array<long, CTX_BINS> ctx_Nt={0};
std::array<long, CTX_BINS> ctx_p_idx={0};

std::array<long, CTX_BINS> ctx_St={0};


#define CTX_MU_PRECISION 0  // number of fractional bits
#define CTX_MU_ACC_BITS 11  // number of bits
const int CTX_MU_FACTOR = int(pow(2,CTX_MU_PRECISION));
const int CTX_ACC_MAX_VAL = int(pow(2,CTX_MU_ACC_BITS-1)-1); // CTX_MU_ACC_BITS-1 because is a signed accumulator

#define CTX_NT_HALF_IDX (1<<(CTX_NT_PRECISION-1))

// #define MAX_Nt_IDX  CTX_NT_HALF_IDX-1//((1<<(CTX_NT_PRECISION))-1)// CTX_NT_HALF_IDX-1
const uint CTX_NT_FACTOR = uint(pow(2,CTX_NT_PRECISION)); // number of fractional bits


#define CTX_ST_BITS (11 +CTX_ST_PRECISION ) // number of bits

const uint CTX_ST_FACTOR = uint(pow(2,CTX_ST_PRECISION));
const uint CTX_ST_MAX_VAL = uint(pow(2,CTX_ST_BITS)-1); 


// initial params
#define CTX_ADJUST_CNT 64
const int ctx_initial_cnt = 1; 
// const int ctx_initial_p_idx = CTX_NT_HALF_IDX -3; //.5 //if using original mu estimation
const int ctx_initial_Nt = 0;



int gradient_quantizer(int g){
  int sign = 1;
  if(g < 0){
    sign = -1;
    g = -g;
  }

  int q;
  // check from high to low probability
  if(g == 0){
    q = 0;
  }else if (g < 3){
    q = 1*sign;
  }else if (g < 7){
    q = 2*sign;
  }else if (g < 21){
    q = 3*sign;
  }else{
    q = 4*sign;
  }
  return q;
}

Context_t map_gradients_to_int(int g1, int g2, int g3){
  int q1 = gradient_quantizer(g1);
  int q2 = gradient_quantizer(g2);
  int q3 = gradient_quantizer(g3);

  int sign = 1;
  if(q1<0 || (q1==0 && q2<0) || (q1==0 && q2==0 && q3<0)){
    sign = -1;
    q1 *= -1;
    q2 *= -1;
    q3 *= -1;
  }

  int context_id = (q1*CTX_BINS_PER_DIM + (q2+CTX_DIM_OFFSET) )*CTX_BINS_PER_DIM 
                                                      + (q3+CTX_DIM_OFFSET);
  assert(!(q1==0 && q2==0 && q3==0) ||(CTX_0 ==context_id ));

  return Context_t(context_id,sign);
}

int get_context_bias(Context_t context){
  // return context.sign * int(round_to_fixed_bits(ctx_mean[context.id]/float(CTX_MU_FACTOR),0));
  #if CTX_MU_PRECISION == 0
    return context.sign *ctx_mean[context.id];
  #else
    const int abs_bias = (CTX_MU_FACTOR/2)-1;
    int bias = ctx_mean[context.id]>0? abs_bias:-abs_bias;
    return context.sign * ((ctx_mean[context.id]+bias)/CTX_MU_FACTOR);
  #endif
}



int get_context_theta_idx(Context_t context){
  int idx;
  #if CTX_ST_FINER_QUANT
    int e, l = ctx_cnt[context.id];
    for(e = 0; ctx_St[context.id] > l; l<<=1,e+=2) {;}
    // idx = e<<1;
    idx = e;
    if(ctx_St[context.id]> l-((l+2)>>2)){
      idx++;
    }
  #else
    for(idx = 0; ctx_St[context.id] > (ctx_cnt[context.id]<<(idx)); ++idx) {;}
  #endif

  idx = idx>MAX_ST_IDX?MAX_ST_IDX:idx;
  return idx;
}



void update_context(Context_t ctx, int prediction_error,int z, int y){ 
  int context = ctx.id;

  //update accumulators and counters
    ctx_acc[context] += context == CTX_0 ? 0: prediction_error<<CTX_MU_PRECISION; // CTX_0 shouldn't have bias
    ctx_Nt[context] += y<<CTX_NT_PRECISION;
    ctx_St[context] += (z<<CTX_ST_PRECISION );
    // ctx_St[context] += (z<<CTX_ST_PRECISION )*alpha ;
    ctx_cnt[context]++;

  //update_context_divisions
    //mu
    { 
      #if MU_estim_like_original
        int Li =-ctx_cnt[context]; //ceil(cnt/2)
        int Ls = 0; // floor(cnt/2)
      #else // rounding range [-1/2, +1/2)
        int Li =-((ctx_cnt[context]+1)>>1); //ceil(cnt/2) // OPT: the +1 probably has no practical effect on compression
        int Ls = ((ctx_cnt[context])>>1); // floor(cnt/2)
      #endif

      //TODO limit mu values to decide it's range, and in consequence it's bits
      if (unlikely(Li >= ctx_acc[context])){
        ctx_mean[context]--;
        ctx_acc[context] += ctx_cnt[context];
        if(unlikely(Li >= ctx_acc[context])){ctx_acc[context]= Li+1; }
      } else if (unlikely(ctx_acc[context] > Ls)){
        ctx_mean[context]++;
        ctx_acc[context] -= ctx_cnt[context] ;
        if (unlikely(ctx_acc[context] > Ls)){ ctx_acc[context]= Ls;}
      }
    }

    //Nt
    {
      
      #if CTX_NT_CENTERED_QUANT
        const int Li =-((ctx_cnt[context]+1)>>1); //ceil(cnt/2) // OPT: the +1 probably has no practical effect on compression
        const int Ls = ((ctx_cnt[context])>>1); // floor(cnt/2)
      #else
        const int Li = 0 ;
        const int Ls = ctx_cnt[context];
      #endif
      ctx_Nt[context] -= ctx_p_idx[context];
      if (unlikely(Li > ctx_Nt[context])){
        ctx_p_idx[context]--;
        ctx_Nt[context] += ctx_cnt[context];
      } else if (unlikely(ctx_Nt[context] >= Ls)){
        #if HALF_Y_CODER
          #define MAX_Nt_IDX  CTX_NT_HALF_IDX-1//((1<<(CTX_NT_PRECISION))-1)// CTX_NT_HALF_IDX-1
          if (unlikely(ctx_p_idx[context] <MAX_Nt_IDX)) {
            ctx_p_idx[context]++;
            ctx_Nt[context] -= ctx_cnt[context] ;
          }
        #else
          ctx_p_idx[context]++;
          ctx_Nt[context] -= ctx_cnt[context] ;
        #endif
      }

      assert(ctx_p_idx[context]>=0);
      #if CTX_NT_CENTERED_QUANT
        assert(ctx_p_idx[context]<=(1<<CTX_NT_PRECISION));
      #else
        assert(ctx_p_idx[context]<(1<<CTX_NT_PRECISION));
      #endif
    }
    
  
  // adjust
    if(unlikely(ctx_cnt[context] >= CTX_ADJUST_CNT )) { 
      ctx_cnt[context] >>=1;
      ctx_acc[context] /=2;  
      ctx_Nt[context] /=2;
      ctx_St[context] /=2;

    }    
}



#endif // CONTEXT_H
