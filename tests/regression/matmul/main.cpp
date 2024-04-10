#include <iostream>
#include <unistd.h>
#include <string.h>
#include <vector>
#include <vortex.h>
#include "common.h"
#include <VX_config.h>
//#define TYPE int16_t

#define RT_CHECK(_expr)                                         \
   do {                                                         \
     int _ret = _expr;                                          \
     if (0 == _ret)                                             \
       break;                                                   \
     printf("Error: '%s' returned %d!\n", #_expr, (int)_ret);   \
	 cleanup();			                                              \
     exit(-1);                                                  \
   } while (false)

///////////////////////////////////////////////////////////////////////////////

const char* kernel_file = "kernel.bin";
uint32_t matrix_size = 0;

vx_device_h device = nullptr;
std::vector<uint8_t> staging_buf;
kernel_arg_t kernel_arg = {};


static void show_usage() {
   std::cout << "Vortex Test." << std::endl;
   std::cout << "Usage: [-k: kernel] [-n words] [-h: help]" << std::endl;
}

static void parse_args(int argc, char **argv, uint32_t &data_size) {
  int c;
  while ((c = getopt(argc, argv, "n:k:d:h?")) != -1) {
    switch (c) {
    case 'n':
      matrix_size = atoi(optarg);
      break;
    case 'k':
      kernel_file = optarg;
      break;
    case 'd':
      data_size = atoi(optarg);
      break;  
    case 'h':
    case '?': {
      show_usage();
      exit(0);
    } break;
    default:
      show_usage();
      exit(-1);
    }
  }
}

void cleanup() {
  if (device) {
    vx_mem_free(device, kernel_arg.src0_addr);
    vx_mem_free(device, kernel_arg.src1_addr);
    vx_mem_free(device, kernel_arg.dst_addr);
    vx_dev_close(device);
  }
}

//kernel_arg, buf_size
int run_test(const kernel_arg_t& kernel_arg,
             uint32_t buf_size, 
             std::vector<int> refs) {
  // start device
  std::cout << "start device" << std::endl;
  RT_CHECK(vx_start(device));

  // wait for completion
  std::cout << "wait for completion" << std::endl;
  RT_CHECK(vx_ready_wait(device, VX_MAX_TIMEOUT));

  // download destination buffer
  std::cout << "download destination buffer" << std::endl;
  RT_CHECK(vx_copy_from_dev(device, staging_buf.data(), kernel_arg.dst_addr, buf_size));

  // verify result
  std::cout << "verify result" << std::endl;  
  {
    int errors = 0;
    auto buf_ptr = (int32_t*)staging_buf.data();
    
    int Result[matrix_size*matrix_size];
    int n_tiles = (matrix_size/TC_SIZE);
    int tc_size_f = TC_SIZE*TC_SIZE;

  //converting buf ptr (tile by tile) to CPU style linear (row by row)
  for(int k = 0; k < matrix_size/TC_SIZE; k+= 1)
  {
    for(int j = 0; j < matrix_size; j+= TC_SIZE)
    {
      for(int i =0; i < TC_SIZE*TC_SIZE; i++)
      {
        std::cout << "GPU idx = " << TC_SIZE*matrix_size*k +j+ (i/TC_SIZE)*matrix_size +i%(TC_SIZE) << "; CPU idx = " << matrix_size*TC_SIZE*k+TC_SIZE*j+i << std::endl;
        std::cout << "GPU value = " << std::hex << buf_ptr[ TC_SIZE*matrix_size*k +j+ (i/TC_SIZE)*matrix_size +i%(TC_SIZE)] << std::endl;
        //Result[ matrix_size*TC_SIZE*k+TC_SIZE*j+i]  = buf_ptr[ TC_SIZE*matrix_size*k +j+ (i/TC_SIZE)*matrix_size +i%(TC_SIZE)];
        Result[ TC_SIZE*matrix_size*k +j+ (i/TC_SIZE)*matrix_size +i%(TC_SIZE)]  = buf_ptr[matrix_size*TC_SIZE*k+TC_SIZE*j+i];
      }
    }    
  }

    for (uint32_t i = 0; i < matrix_size*matrix_size; ++i) {
      //int ref = i + i; 
      int cur = Result[i];
      if (cur != refs[i]) {

        std::cout << "index not matching " << i << std::endl;
        std::cout << "GPU : " << std::hex << cur << "; CPU : " << std::hex << refs[i] << std::endl;
        ++errors;
      }
    }
    if (errors != 0) {
      std::cout << "Found " << std::dec << errors << " errors!" << std::endl;
      std::cout << "FAILED!" << std::endl;
      return 1;  
    }
    else
    {
      std::cout << "CONDITIONALLY PASSED!" << std::endl;
    }
  }

  return 0;
}

template<typename TYPE>
class mainVariables
{
  public:
    // Constructor
    mainVariables(uint32_t bufSize, uint32_t dataSize, uint32_t matrixSize)
        : buf_size(bufSize), data_size(dataSize), matrix_size(matrixSize)
    {
        std::cout << "buf_size = " << buf_size << std::endl;
        std::cout << "data_size = " << data_size << std::endl;

        // Resize vectors to specified sizes
        src_A.resize(buf_size/data_size);
        src_B.resize(buf_size/data_size);
        refs.resize(buf_size/data_size);
        std::cout << "Inside constructor" <<std::endl;
        A_mat = (TYPE*)calloc(buf_size/data_size,sizeof(TYPE));
        B_mat = (TYPE*)calloc(buf_size/data_size,sizeof(TYPE));
    }

  void init_inputs ()
  {
    std::cout << "inside init" << std::endl;
    for (uint32_t i = 0; i < matrix_size*matrix_size; ++i) 
    {
      auto a = static_cast<float>(std::rand()) / RAND_MAX;
      auto b = static_cast<float>(std::rand()) / RAND_MAX;
      src_A[i] = static_cast<TYPE>(a * matrix_size);
      src_B[i] = static_cast<TYPE>(b * matrix_size);
      std::cout << "src_A[" << i<< "] = " << src_A[i] << std::endl;
      std::cout << "src_B[" << i<< "] = " << src_B[i] << std::endl;
    }
  }

  void matmul_cpu() 
  {
    for (uint32_t row = 0; row < matrix_size; ++row) 
    {
      for (uint32_t col = 0; col < matrix_size; ++col) 
      {
        TYPE sum(0);
        for (uint32_t e = 0; e < matrix_size; ++e) {
            sum += src_A[row * matrix_size + e] * src_B[e * matrix_size + col];
        }
        refs[row * matrix_size + col] = sum;
      }
    }
  }

  //Public variables
  std::vector<TYPE> src_A;
  std::vector<TYPE> src_B;
  std::vector<TYPE> refs;

  TYPE* A_mat;
  TYPE* B_mat;

  private:
    uint32_t buf_size;
    uint32_t data_size;
    uint32_t matrix_size;
};


int main(int argc, char *argv[]) {  
  // parse command arguments
  uint32_t data_size = 0;
  parse_args(argc, argv, data_size);
  if (matrix_size == 0) {
    matrix_size = 2;
  }

  // open device connection
  std::cout << "open device connection" << std::endl;  
  RT_CHECK(vx_dev_open(&device));

  uint64_t num_cores, num_warps, num_threads;
  RT_CHECK(vx_dev_caps(device, VX_CAPS_NUM_CORES, &num_cores));
  RT_CHECK(vx_dev_caps(device, VX_CAPS_NUM_WARPS, &num_warps));
  RT_CHECK(vx_dev_caps(device, VX_CAPS_NUM_THREADS, &num_threads));

  //TODO - can be changed
  //Number of output tiles * number of threads
  uint32_t num_tasks  = (matrix_size*matrix_size)/(TC_SIZE*TC_SIZE)*num_threads;
  
  std::cout << "DEBUG: Matrix Size: " << matrix_size << std::endl;
  std::cout << "DEBUG: TC Size: " << TC_SIZE << std::endl;
  std::cout << "DEBUG: Num Threads: " << num_threads << std::endl;
  std::cout << "DEBUG: Data Size: " << data_size << std::endl;
  //size of each operand
  //uint32_t buf_size   =  ((matrix_size*matrix_size)/(TC_SIZE*TC_SIZE))*(matrix_size/(TC_SIZE))*(TC_SIZE*TC_SIZE)*4;
  uint32_t buf_size   =  ((matrix_size*matrix_size)/(TC_SIZE*TC_SIZE))*(matrix_size/(TC_SIZE))*(TC_SIZE*TC_SIZE)*data_size;

  //256
  std::cout << "buffer size: " << buf_size << " bytes" << std::endl;

  // upload program
  std::cout << "upload program" << std::endl;  
  RT_CHECK(vx_upload_kernel_file(device, kernel_file));

  // allocate device memory
  std::cout << "allocate device memory" << std::endl;

  
  RT_CHECK(vx_mem_alloc(device, buf_size, VX_MEM_TYPE_GLOBAL, &kernel_arg.src0_addr));
  RT_CHECK(vx_mem_alloc(device, buf_size, VX_MEM_TYPE_GLOBAL, &kernel_arg.src1_addr));
  RT_CHECK(vx_mem_alloc(device, buf_size, VX_MEM_TYPE_GLOBAL, &kernel_arg.dst_addr));

  mainVariables<int> variables (buf_size, data_size, matrix_size);
  variables.init_inputs();

  //1
  std::cout << "num_tasks = " << num_tasks << std::endl;
  kernel_arg.num_tasks = num_tasks;
  kernel_arg.num_warps = num_warps;
  //1
  kernel_arg.matrix_size = matrix_size;
  kernel_arg.data_size = data_size;

  std::cout << "num_tasks = " << std::hex << kernel_arg.num_tasks << std::endl;
  std::cout << "matrix_size = " << std::hex << kernel_arg.matrix_size << std::endl;
  
  
  uint32_t offset = (matrix_size*matrix_size)/(TC_SIZE*TC_SIZE) * (matrix_size/TC_SIZE) * (TC_SIZE*TC_SIZE) * 4;
  
  //TODO - does this need to be fixed?
  uint32_t base_addr = 0x40;
  kernel_arg.src0_addr = base_addr;  
  kernel_arg.src1_addr = base_addr + offset;
  kernel_arg.dst_addr = base_addr + 2*offset;

  std::cout << "dev_src0=0x" << std::hex << kernel_arg.src0_addr << std::endl;
  std::cout << "dev_src1=0x" << std::hex << kernel_arg.src1_addr << std::endl;
  std::cout << "dev_dst=0x" << std::hex << kernel_arg.dst_addr << std::endl;

  // allocate staging buffer  
  std::cout << "allocate staging buffer" << std::endl;    
  uint32_t alloc_size = std::max<uint32_t>(buf_size, sizeof(kernel_arg_t));
  staging_buf.resize(alloc_size);
  
  // upload kernel argument
  std::cout << "upload kernel argument" << std::endl;
  memcpy(staging_buf.data(), &kernel_arg, sizeof(kernel_arg_t));
  RT_CHECK(vx_copy_to_dev(device, KERNEL_ARG_DEV_MEM_ADDR, staging_buf.data(), sizeof(kernel_arg_t)));

  uint32_t tc_size_f = TC_SIZE*TC_SIZE;
  uint32_t n_tiles = matrix_size/TC_SIZE;
  
  variables.matmul_cpu();

  //Demand matrix creation for A
    //traverse through the rows
  for(uint32_t k=0; k<n_tiles; k++)
  {
    //traverse through output tiles in a row
    for(uint32_t i=0; i<n_tiles; i++)
    {
      //traverse through tiles for one output tile
      
        for(uint32_t j=0; j< n_tiles; j++)
        {
          for(int t=0; t < TC_SIZE*TC_SIZE; t++)
          { 
            variables.A_mat[n_tiles*n_tiles*tc_size_f*k + n_tiles*tc_size_f*i+tc_size_f*j + t]   = variables.src_A[k*TC_SIZE*matrix_size+ TC_SIZE*j +(t/TC_SIZE)*matrix_size + t%TC_SIZE];
          }
        }
    }
  }

  //Demand matrix creation for B
  //traverse through the rows
  for(uint32_t k=0; k<n_tiles; k++)
  {
    //traverse through output tiles in a row
    for(uint32_t i=0; i<n_tiles; i++)
    {
      //traverse through tiles for one output tile
      for(uint32_t j=0; j< n_tiles; j++)
      {
        for(int t=0; t < TC_SIZE*TC_SIZE; t++)
        {
          variables.B_mat[n_tiles*n_tiles*tc_size_f*k + n_tiles*tc_size_f*i+tc_size_f*j + t]   = variables.src_B[i*TC_SIZE+ TC_SIZE*matrix_size*j +(t/TC_SIZE)*matrix_size + t%TC_SIZE];
        }
      }
    }
  }
  
 // upload source buffer0
  {
    staging_buf.resize(buf_size);
    std::cout << "upload source buffer0" << std::endl;
    auto buf_ptr = (int32_t*)staging_buf.data();
    
      for (uint32_t i = 0; i < buf_size/data_size; i+=data_size) {
      buf_ptr[i+0] = variables.A_mat[i];
      buf_ptr[i+1] = variables.A_mat[i+1];
      buf_ptr[i+2] = variables.A_mat[i+2];
      buf_ptr[i+3] = variables.A_mat[i+3];
    }  
    
    RT_CHECK(vx_copy_to_dev(device, kernel_arg.src0_addr, staging_buf.data(), buf_size));
  }

  // upload source buffer1
  {
    std::cout << "upload source buffer1" << std::endl;
    auto buf_ptr = (int32_t*)staging_buf.data();
    for (uint32_t i = 0; i < buf_size/data_size; i+=data_size) {
      buf_ptr[i+0] = variables.B_mat[i];
      buf_ptr[i+1] = variables.B_mat[i+1];
      buf_ptr[i+2] = variables.B_mat[i+2];
      buf_ptr[i+3] = variables.B_mat[i+3];
    }  
    RT_CHECK(vx_copy_to_dev(device, kernel_arg.src1_addr, staging_buf.data(), buf_size));
  }

  // clear destination buffer
  //TODO - what is this?
  {
    std::cout << "clear destination buffer" << std::endl;      
    auto buf_ptr = (int32_t*)staging_buf.data();
    for (uint32_t i = 0; i < buf_size/data_size; ++i) {
            buf_ptr[i] = 0xdeadbeef;
    }  
    RT_CHECK(vx_copy_to_dev(device, kernel_arg.dst_addr, staging_buf.data(), buf_size));  
  }


  // run tests
  std::cout << "run tests" << std::endl;
  RT_CHECK(run_test(kernel_arg, buf_size, variables.refs));

  // cleanup
  std::cout << "cleanup" << std::endl;  
  cleanup();

  std::cout << "PASSED!" << std::endl;

  return 0;
}