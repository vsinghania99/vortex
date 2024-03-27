#include <iostream>
#include <unistd.h>
#include <string.h>
#include <vector>
#include <vortex.h>
#include "common.h"
#include <VX_config.h>
//#define TC_SIZE     2


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

static void parse_args(int argc, char **argv) {
  int c;
  while ((c = getopt(argc, argv, "n:k:h?")) != -1) {
    switch (c) {
    case 'n':
      matrix_size = atoi(optarg);
      break;
    case 'k':
      kernel_file = optarg;
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

//kernel_arg, buf_size, num_points
int run_test(const kernel_arg_t& kernel_arg,
             uint32_t buf_size, 
             uint32_t num_points) {
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
    uint32_t Ans[MATRIX_SIZE*MATRIX_SIZE] = {4,8,8,16,12,16,24,32,12,24,16,32,36,48,48,64};
    
    for (uint32_t i = 0; i < matrix_size*matrix_size; ++i) {
      //int ref = i + i; 
      int cur = buf_ptr[i];
      std::cout << "Res " << i << " : " << cur << std::endl;
      if (cur != Ans[i]) {
        //std::cout << "error at result #" << std::dec << i;
        //          << std::hex << ": actual 0x" << cur << ", expected 0x" << ref << std::endl;
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

int main(int argc, char *argv[]) {  
  // parse command arguments
  parse_args(argc, argv);
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

  //Number of tiles * threads
  //uint32_t num_tasks  = num_cores * num_warps * num_threads;
  //TODO - fix this
  std::cout << "DEBUG: Matrix Size: " << MATRIX_SIZE << std::endl;
  //uint32_t num_tasks  = ((matrix_size*matrix_size)/(TC_SIZE*TC_SIZE))*(matrix_size/(TC_SIZE))*num_threads;
  uint32_t num_tasks  = (MATRIX_SIZE*MATRIX_SIZE)/(TC_SIZE*TC_SIZE)*num_threads;
  
  //4*1*1
  std::cout << "DEBUG: TC Size: " << TC_SIZE << std::endl;
  std::cout << "DEBUG: Num Threads: " << num_threads << std::endl;

  uint32_t num_points = TC_SIZE * TC_SIZE;
  //size of each operand
  //uint32_t buf_size   = num_points * sizeof(int32_t);
  uint32_t buf_size   =  ((MATRIX_SIZE*MATRIX_SIZE)/(TC_SIZE*TC_SIZE))*(MATRIX_SIZE/(TC_SIZE))*(TC_SIZE*TC_SIZE)*4;

  //64
  std::cout << "number of points: " << num_points << std::endl;
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

  //1
  std::cout << "num_tasks = " << num_tasks << std::endl;
  kernel_arg.num_tasks = num_tasks;
  //1
  kernel_arg.matrix_size = MATRIX_SIZE;

  std::cout << "dev_src0=0x" << std::hex << kernel_arg.src0_addr << std::endl;
  std::cout << "dev_src1=0x" << std::hex << kernel_arg.src1_addr << std::endl;
  std::cout << "dev_dst=0x" << std::hex << kernel_arg.dst_addr << std::endl;
  std::cout << "num_tasks = " << std::hex << kernel_arg.num_tasks << std::endl;
  std::cout << "matrix_size = " << std::hex << kernel_arg.matrix_size << std::endl;
  
  kernel_arg.src0_addr = 0x40;
  kernel_arg.src1_addr = 0x240;
  kernel_arg.dst_addr = 0x440;

  // allocate staging buffer  
  std::cout << "allocate staging buffer" << std::endl;    
  uint32_t alloc_size = std::max<uint32_t>(buf_size, sizeof(kernel_arg_t));
  staging_buf.resize(alloc_size);
  
  // upload kernel argument
  std::cout << "upload kernel argument" << std::endl;
  memcpy(staging_buf.data(), &kernel_arg, sizeof(kernel_arg_t));
  RT_CHECK(vx_copy_to_dev(device, KERNEL_ARG_DEV_MEM_ADDR, staging_buf.data(), sizeof(kernel_arg_t)));


  /*
  int A_mat[] = {1,1,1,1,
                 2,2,2,2,
                 3,3,3,3,
                 4,4,4,4};

  int B_mat[] = {1,2,3,4,
                 1,2,3,4,
                 1,2,3,4,
                 1,2,3,4};
  */
  
  int A_mat[] = {1,1,2,2,1,1,2,2,1,1,2,2,1,1,2,2,3,3,4,4,3,3,4,4,3,3,4,4,3,3,4,4};
  int B_mat[] = {1,2,1,2,1,2,1,2,3,4,3,4,3,4,3,4,1,2,1,2,1,2,1,2,3,4,3,4,3,4,3,4};
  
  kernel_arg.num_warps = num_warps;
  // upload source buffer0
  {
    staging_buf.resize(buf_size);
    std::cout << "upload source buffer0" << std::endl;
    auto buf_ptr = (int32_t*)staging_buf.data();
    //for (uint32_t i = 0; i < num_points; i+=4) {
    for (uint32_t i = 0; i < buf_size/4; i+=4) {
      buf_ptr[i+0] = A_mat[i];
      buf_ptr[i+1] = A_mat[i+1];
      buf_ptr[i+2] = A_mat[i+2];
      buf_ptr[i+3] = A_mat[i+3];
      std::cout << "DEBUG: i=" << i << ", buf_value=" << buf_ptr[i+0] << std::endl;
      std::cout << "DEBUG: i=" << i << ", buf_value=" << buf_ptr[i+1] << std::endl;
      std::cout << "DEBUG: i=" << i << ", buf_value=" << buf_ptr[i+2] << std::endl;
      std::cout << "DEBUG: i=" << i << ", buf_value=" << buf_ptr[i+3] << std::endl;
    }  
    RT_CHECK(vx_copy_to_dev(device, kernel_arg.src0_addr, staging_buf.data(), buf_size));
  }

  // upload source buffer1
  {
    std::cout << "upload source buffer1" << std::endl;
    auto buf_ptr = (int32_t*)staging_buf.data();
    //for (uint32_t i = 0; i < num_points; i+=4) {
    for (uint32_t i = 0; i < buf_size/4; i+=4) {
      buf_ptr[i+0] = B_mat[i];
      buf_ptr[i+1] = B_mat[i+1];
      buf_ptr[i+2] = B_mat[i+2];
      buf_ptr[i+3] = B_mat[i+3];
      std::cout << "DEBUG: i=" << i << ", buf_value=" << buf_ptr[i+0] << std::endl;
      std::cout << "DEBUG: i=" << i << ", buf_value=" << buf_ptr[i+1] << std::endl;
      std::cout << "DEBUG: i=" << i << ", buf_value=" << buf_ptr[i+2] << std::endl;
      std::cout << "DEBUG: i=" << i << ", buf_value=" << buf_ptr[i+3] << std::endl;
    }  
    RT_CHECK(vx_copy_to_dev(device, kernel_arg.src1_addr, staging_buf.data(), buf_size));
  }

  // clear destination buffer
  //TODO - what is this?
  {
    std::cout << "clear destination buffer" << std::endl;      
    auto buf_ptr = (int32_t*)staging_buf.data();
    for (uint32_t i = 0; i < buf_size/4; ++i) {
      buf_ptr[i] = 0xdeadbeef;
    }  
    RT_CHECK(vx_copy_to_dev(device, kernel_arg.dst_addr, staging_buf.data(), buf_size));  
  }

  // run tests
  std::cout << "run tests" << std::endl;
  RT_CHECK(run_test(kernel_arg, buf_size, num_points));

  // cleanup
  std::cout << "cleanup" << std::endl;  
  cleanup();

  std::cout << "PASSED!" << std::endl;

  return 0;
}