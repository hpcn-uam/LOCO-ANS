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

#ifndef ANS_CODER_H
#define ANS_CODER_H

#include "codec_core.h"
#include "context.h"
#include "img_proc_utils.h"
#include "coder_config.h"

// #define EE_BUFFER_SIZE (2048)
#define STACK_SIZE (EE_BUFFER_SIZE* int(MAX_SUPPORTED_BPP/8))
#define STACK_PTR_INIT (STACK_SIZE-1)
#define CTX_NT_HALF_IDX (1<<(CTX_NT_PRECISION-1))
#define CTX_NT_QUANT_BINS (1<<(CTX_NT_PRECISION))
// #define NUM_ANS_THETA_MODES 32//16 // supported theta_modes
// #define NUM_ANS_P_MODES 16 //16 //32 // supported theta_modes
// #define NUM_ANS_STATES 32//32//64 // NUM_ANS_STATES only considers I range

#define ANS_I_RANGE_START NUM_ANS_STATES

#define ANS_NEXT_STATE 0
#define ANS_NUM_OF_BITS 1
const int tANS_STATE_SIZE = std::log2(NUM_ANS_STATES) ; 
const int tANS_STATE_MASK = (0xFFFF >> (16-tANS_STATE_SIZE));

typedef uint32_t bit_buffer_t;
typedef uint8_t binary_stack_t;

struct tANS_ROM_element_t {
  int8_t next_state;
  int8_t bits;
} ;
typedef tANS_ROM_element_t tANS_table_t;




class Symbol_Coder
{
  /** It encodes the module using tANS 
 * It does it iteratively as the exponential distribution allow it
 * It's encoded from the last conditional prob to the first since 
 * ANS is decoded in reverse order and the decoder needs to see the
 * conditional probabilities in order
 * 
 * The last iteration (first symbol) is encoded with a ANS mode that
 * takes into account the URURQ configuration. The rest symbols are 
 * encoded as if the module was quantized with an uniform quantizer
 * since URURQ is uniform except for the central range
 *
 */

  private:
    uint symbols_in_buffer ;
    std::vector<ee_symb_data> entropy_encoder_buffer;

    // ANS
    uint ANS_encoder_state ; // = ANS_I_RANGE_START & tANS_STATE_MASK
    size_t geometric_coder_iters;

    // binary writer 
    uint8_t* out_file;
    long file_size;

    int stack_ptr ;
    binary_stack_t encoded_bit_stack[STACK_SIZE];
    bit_buffer_t bit_buffer;
    uint bit_ptr ;
    const uint BIT_BUFFER_SIZE = 8*sizeof(binary_stack_t);
    
    
  public:
  // if using the default constructor, set_out_bitfile needs to be called before 
  // coding
  Symbol_Coder():symbols_in_buffer(0),ANS_encoder_state(0),geometric_coder_iters(0)
            ,file_size(0) , stack_ptr(STACK_PTR_INIT),bit_buffer(0),bit_ptr(0){
    entropy_encoder_buffer.reserve(EE_BUFFER_SIZE);
  }

  Symbol_Coder(uint8_t *_out_file):symbols_in_buffer(0),ANS_encoder_state(0),geometric_coder_iters(0)
            ,out_file(_out_file),file_size(0) , stack_ptr(STACK_PTR_INIT),bit_buffer(0),bit_ptr(0){
    entropy_encoder_buffer.reserve(EE_BUFFER_SIZE);


   // TODO: support mac iterations as argument
    //get max iterations and compute the max_module_per_cardinality_table

  }

  ~Symbol_Coder(){
    if(symbols_in_buffer != 0) {
      std::cerr<<"Error: Ending codification with symbols in the buffer." <<std::endl;
      std::cerr<<"    Call code_symbol_buffer() before quiting." <<std::endl;
    }

    if(stack_ptr != STACK_PTR_INIT) {
      std::cerr<<"Error: Ending codification with bits in binary stack." <<std::endl;
      std::cerr<<"    Call code_symbol_buffer() before quiting." <<std::endl;
    }
  }

  void set_out_bitfile(uint8_t *_out_file){
    out_file = _out_file;
  }

  size_t get_out_file_size() const {
    return file_size;
  }

  size_t get_geometric_coder_iters() const {
    return geometric_coder_iters;
  }
  

  void store_pixel(unsigned char pixel){
    symbols_in_buffer ++; // count in order to have always the same encode block size
    write_byte_in_binary(pixel);
    if(unlikely(symbols_in_buffer >= EE_BUFFER_SIZE)) {
      std::cerr<<"Warning: Coder symbol buffer is full after storing pixel value" <<std::endl;
    }
  }

   
  void push_symbol(ee_symb_data symbol ){
    symbols_in_buffer ++;
    entropy_encoder_buffer.push_back(symbol);

    if(unlikely(symbols_in_buffer >= EE_BUFFER_SIZE)) {
      code_symbol_buffer();
    }

  }

  void code_symbol_buffer(){
    if(likely( symbols_in_buffer != 0)) { // OPT !entropy_encoder_buffer.empty() does the job
      while(!entropy_encoder_buffer.empty()){
        ee_symb_data symb_data = entropy_encoder_buffer.back();
        entropy_encoder_buffer.pop_back();
        Bernoulli_coder(symb_data);// encode y
        geometric_coder(symb_data); // encode z 
      }

      tANS_store_state();
      store_binary_stack(); //go to next byte
      symbols_in_buffer = 0;

    }

  }

private:
  void Bernoulli_coder(ee_symb_data symbol){
      static const tANS_table_t tANS_y_encode_table[NUM_ANS_P_MODES][NUM_ANS_STATES][2]{
      #include "ANS_tables/tANS_y_encoder_table.dat"
    }; // [0] number of bits, [1] next state

    if(unlikely(symbol.p_id >= CTX_NT_HALF_IDX)) {
      #if CTX_NT_CENTERED_QUANT
        if(symbol.p_id == CTX_NT_HALF_IDX) {
          push_bits_to_binary_stack( symbol.y,1);
          return;
        }
        symbol.p_id = CTX_NT_QUANT_BINS - symbol.p_id;
      #else
        symbol.p_id = CTX_NT_QUANT_BINS-1 - symbol.p_id;
      #endif
      symbol.y = symbol.y == 0?1:0;
    }
    
    auto current_y_ANS_table = tANS_y_encode_table[symbol.p_id];
    tANS_encode_bernulli(current_y_ANS_table, symbol.y);

  }

  void geometric_coder(ee_symb_data symbol){
      static const tANS_table_t tANS_encode_table[NUM_ANS_THETA_MODES][NUM_ANS_STATES][ANS_MAX_SRC_CARDINALITY]{
      #include "ANS_tables/tANS_z_encoder_table.dat"
    }; 

    const auto max_allowed_module = max_module_per_cardinality_table[symbol.theta_id ];
    const auto encoder_cardinality = tANS_cardinality_table[symbol.theta_id ];
    int module_reminder = symbol.z;
    auto current_ANS_table = tANS_encode_table[symbol.theta_id];

    assert(encoder_cardinality>0);
    int ans_symb = module_reminder & (encoder_cardinality-1);
    // int ans_symb = module_reminder % encoder_cardinality;
    //iteration limitation
    if (unlikely(symbol.z >= max_allowed_module)){ 
      // exceeds max iterations
      uint enc_bits = EE_REMAINDER_SIZE - symbol.remainder_reduct_bits;
      push_bits_to_binary_stack( symbol.z ,enc_bits);
   
      //use geometric_coder to code escape symbol
      module_reminder = max_allowed_module; 
      ans_symb = encoder_cardinality;
    }

    do {
      module_reminder -= ans_symb;
      tANS_encode(current_ANS_table, ans_symb);
      ans_symb = encoder_cardinality;

    }while(module_reminder > 0);

    assert(module_reminder==0);
  }


  // ---------------------------------------------
  // ---------------   ANS coder   --------------- 
  // ---------------------------------------------


  void inline tANS_store_state(){

    #if tANS_STATE_SIZE+1 > 8
    #error The bit buffer stores upto 8 bits. binary stack should use a type with more bits, but just changing the type causes endianness problems and/or others
    #else
    push_bits_to_binary_stack(ANS_encoder_state +NUM_ANS_STATES ,tANS_STATE_SIZE+1);
    #endif
    ANS_encoder_state = 0; // = ANS_I_RANGE_START & tANS_STATE_MASK
  }

  void inline tANS_encode_bernulli(const tANS_table_t tANS_encode_table[NUM_ANS_STATES][2], uint symbol){
    const int num_of_bits = tANS_encode_table[ANS_encoder_state][symbol].bits;
    assert(num_of_bits >= 0);

    const uint BIT_MASK = (1<<num_of_bits)-1;
    push_bits_to_binary_stack( ANS_encoder_state & BIT_MASK,num_of_bits);
    ANS_encoder_state = tANS_encode_table[ANS_encoder_state][symbol].next_state;
  }


  void inline tANS_encode(const tANS_table_t tANS_encode_table[NUM_ANS_STATES][ANS_MAX_SRC_CARDINALITY], uint symbol){
    #if ANALYSIS_CODE
      geometric_coder_iters++; // analysis
    #endif

    const int num_of_bits = tANS_encode_table[ANS_encoder_state][symbol].bits;

    assert(num_of_bits >= 0);

    const uint BIT_MASK = (1<<num_of_bits)-1;
    // uint BIT_MASK = 0xFF >> (8-num_of_bits);
    push_bits_to_binary_stack( ANS_encoder_state & BIT_MASK,num_of_bits);
    ANS_encoder_state = tANS_encode_table[ANS_encoder_state][symbol].next_state;

  }


  // ---------------------------------------------
  // --------------- Binary Writer --------------- 
  // ---------------------------------------------


  void push_single_bit_to_binary_stack( uint32_t bit ){
    bit_buffer |= bit << bit_ptr;
    bit_ptr += 1;

    if (bit_ptr >= BIT_BUFFER_SIZE){
      // encoded_bit_stack[stack_ptr] =  binary_stack_t((bit_buffer & 0xff) << 8) | ((bit_buffer & 0xff00) >> 8);
      encoded_bit_stack[stack_ptr] =  binary_stack_t(bit_buffer);
      // encoded_bit_stack[stack_ptr] =  uint8_t(bit_buffer &0xFF);
      bit_ptr = 0;
      bit_buffer =  0;
      stack_ptr-=sizeof(binary_stack_t);
      // stack_ptr--;

      if(stack_ptr < 0) {
        std::cerr<<DBG_INFO<<"ERROR: Stack overflow. MAX_SUPPORTED_BPP ("<<MAX_SUPPORTED_BPP<<
              ") it's not enough. Can't fix this, quiting"<<std::endl;
        throw 1;
      }
    }
  }

  void push_bits_to_binary_stack( uint32_t symbol ,uint num_of_bits ){
    bit_buffer |= symbol << bit_ptr;
    bit_ptr += num_of_bits;

    if (bit_ptr >= BIT_BUFFER_SIZE){
      encoded_bit_stack[stack_ptr] =  binary_stack_t(bit_buffer);
      bit_ptr -= BIT_BUFFER_SIZE;
      bit_buffer >>=BIT_BUFFER_SIZE;
      stack_ptr-=sizeof(binary_stack_t);
      if(stack_ptr < 0) {
        std::cerr<<DBG_INFO<<"ERROR: Stack overflow. MAX_SUPPORTED_BPP ("<<MAX_SUPPORTED_BPP<<
              ") it's not enough. Can't fix this, quiting"<<std::endl;
        throw 1;
      }
    }
  } 

  void  write_byte_in_binary(uint32_t byte  ){
    out_file[file_size] = byte;
    file_size++;
  }

  void store_binary_stack(){
    if(bit_ptr != 0) {
      // encoded_bit_stack[stack_ptr] = binary_stack_t((bit_buffer & 0xff) << 8) | ((bit_buffer & 0xff00) >> 8);
      encoded_bit_stack[stack_ptr] = binary_stack_t(bit_buffer) ;
      bit_buffer = 0;
      bit_ptr = 0;

      BUILD_BUG_ON(sizeof(binary_stack_t) > 1);
      //   Currently sizeof(binary_stack_t) > 1 is not supported
      //   need to check how many bytes are added here 
      //   Also, endianness should be taken into account
    }else{
      stack_ptr++;
    }

    const int bytes_in_stack = (STACK_PTR_INIT - stack_ptr +1)*sizeof(binary_stack_t);
    memcpy(out_file+file_size,encoded_bit_stack + stack_ptr,bytes_in_stack);

    file_size += bytes_in_stack;
    stack_ptr = STACK_PTR_INIT;
  }


};


/***********************
 *Decoder side functions  
***********************/

/*
When calling any of :
- retrive_pixel
- retrive_TSG_symbol

it's assumed that a symbol was retrieved (except in retrive_TSG_symbol 
when the escape symbol is encountered ). So the falling clean up code is used 
before returning:

```
blk_rem_symbols--;
check_update_block();
```

*/

  #define ANS_SYMBOL 0
  #define ANS_PREV_STATE 1
class Binary_Decoder
{

  uint remaining_symbols; // remaining symbols excluding those in the current block
  uint blk_rem_symbols;  // remaining symbols in current decoding block

  unsigned char* block_binary;
  size_t binary_file_ptr;
  int bit_ptr;
  #define bit_ptr_init (7)


public:
  Binary_Decoder(unsigned char* _block_binary,uint total_symbs):
      block_binary(_block_binary),binary_file_ptr(0),bit_ptr(bit_ptr_init),
      is_ANS_ready(false),ANS_decoder_state(0){
    // TODO should be using the buffer size stated in the global header, not EE_BUFFER_SIZE
    blk_rem_symbols = total_symbs < EE_BUFFER_SIZE? total_symbs : EE_BUFFER_SIZE;
    remaining_symbols = total_symbs - blk_rem_symbols;
  }

  ~Binary_Decoder(){};
  
  unsigned char retrive_pixel(){
    unsigned char symbol = get_byte();
    blk_rem_symbols--;
    check_update_block();
    return symbol;
  }

  int retrive_bits(int num_of_bits ){
    int bit_buffer = 0;
    for(int i = 0; i < num_of_bits; ++i) {
      bit_buffer *=2;
      bit_buffer |= get_bit();
    }

    // blk_rem_symbols--;
    // check_update_block();
    return bit_buffer;
    
  }

  int retrive_TSG_symbol(int theta_id, int p_id, uint escape_bits, int &z, int &y){  
    // z first and y second
    z = Geometric_decoder(theta_id,escape_bits);
    y = Bernoulli_decoder(p_id);
    blk_rem_symbols--;
    check_update_block();
    return 0;
  }

private:

  // operations on binary file
    inline unsigned char get_byte(){
      if(unlikely(bit_ptr != bit_ptr_init)) {
        std::cerr<<DBG_INFO<<" Warning: previous byte was being read. bit_ptr: "<<bit_ptr<<std::endl;
        binary_file_ptr ++; //go to next byte, discarding current
      }
      unsigned char symbol = block_binary[binary_file_ptr];
      binary_file_ptr ++;
      return symbol;
    }

    inline unsigned char get_bit(){
      unsigned char bit = block_binary[binary_file_ptr] & (1 << bit_ptr);
      bit = bit != 0? 1 : 0;
      bit_ptr--;
      if(bit_ptr < 0) {
        bit_ptr = bit_ptr_init;
        binary_file_ptr++;
      }
      return bit;
    }



  bool is_ANS_ready;
  uint ANS_decoder_state;


  inline void check_update_block(){
    if(unlikely(blk_rem_symbols == 0)) {
      tANS_finish_block();
      blk_rem_symbols = remaining_symbols < EE_BUFFER_SIZE? remaining_symbols : EE_BUFFER_SIZE;
      remaining_symbols -= blk_rem_symbols;
    }
  }

  auto tANS_z_decoder(uint mode){
    static const int32_t tANS_z_decode_table[NUM_ANS_THETA_MODES][NUM_ANS_STATES][2]{
       #include "ANS_tables/tANS_z_decoder_table.dat"
      }; // [0] number of bits, [1] next state
    assert((ANS_decoder_state >= ANS_I_RANGE_START));
    uint tANS_table_idx = ANS_decoder_state & tANS_STATE_MASK;
    auto symbol = tANS_z_decode_table[mode][tANS_table_idx][ANS_SYMBOL];
    ANS_decoder_state = tANS_z_decode_table[mode][tANS_table_idx][ANS_PREV_STATE];
    assert((symbol >= 0));

    while(ANS_decoder_state < ANS_I_RANGE_START) {
      ANS_decoder_state *=2; // OPT: left shift
      ANS_decoder_state |=  get_bit();
    }

    return symbol;
  }

  auto tANS_y_decoder(uint mode){
    static const int32_t tANS_y_decode_table[NUM_ANS_P_MODES][NUM_ANS_STATES][2]{
        #include "ANS_tables/tANS_y_decoder_table.dat"
      }; // [0] number of bits, [1] next state

    assert((ANS_decoder_state >= ANS_I_RANGE_START));
    uint tANS_table_idx = ANS_decoder_state & tANS_STATE_MASK;
    auto symbol = tANS_y_decode_table[mode][tANS_table_idx][ANS_SYMBOL];
    ANS_decoder_state = tANS_y_decode_table[mode][tANS_table_idx][ANS_PREV_STATE];
    assert((symbol >= 0));

    while(ANS_decoder_state < ANS_I_RANGE_START) {
      ANS_decoder_state *=2; // OPT: left shift
      ANS_decoder_state |=  get_bit();
    }

    return symbol;
  }

  void init_ANS(){
    while(get_bit() == 0); // remove bit padding
    ANS_decoder_state = 1; // 1 because i have just removed the bit marker
    for(unsigned i = 0; i < tANS_STATE_SIZE; ++i) {
      ANS_decoder_state *=2;
      ANS_decoder_state |= get_bit();
    }

    is_ANS_ready = true;
  }

  void tANS_finish_block(){
    if(unlikely((ANS_decoder_state != ANS_I_RANGE_START))){
      std::cerr<<"Error: ANS decoder state("<<ANS_decoder_state 
            <<") should be zero at the end of the block"<<std::endl;
    }
    ANS_decoder_state = 0;
    is_ANS_ready = false;
  }

  int Bernoulli_decoder(uint p_id){
    bool invert_y = false;
    #if !HALF_Y_CODER
    if(unlikely(p_id >= CTX_NT_HALF_IDX)) {
      invert_y = true;
      #if CTX_NT_CENTERED_QUANT
        if(p_id == CTX_NT_HALF_IDX) {
          return get_bit();
        }

        p_id = CTX_NT_QUANT_BINS - p_id;
      #else
        p_id = CTX_NT_QUANT_BINS-1 - p_id;
      #endif
    }
    #endif

    int ans_symb = tANS_y_decoder( p_id);
    int y = ans_symb;
    #if !HALF_Y_CODER
      if(unlikely(invert_y)) {
        y = y == 0?1:0;
      }
    #endif
    return y;
  }
  int Geometric_decoder(uint theta_id, uint escape_bits){
    if(unlikely(!is_ANS_ready)) {init_ANS(); }
    
    const auto encoder_cardinality = tANS_cardinality_table[theta_id ];

    // read first symbol using 
    auto ans_symb = tANS_z_decoder(theta_id);
    int module = ans_symb;

    int it = 1;
    while(ans_symb >= encoder_cardinality){
      if(it >= EE_MAX_ITERATIONS) {
        module = retrive_bits(escape_bits);
        break;
      }

      ans_symb = tANS_z_decoder(theta_id);
      module += ans_symb;
      it++;
    }
    

    return module;
  }


};



#endif // ANS_CODER_H
