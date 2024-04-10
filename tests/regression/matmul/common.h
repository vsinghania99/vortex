#ifndef _COMMON_H_
#define _COMMON_H_

#define KERNEL_ARG_DEV_MEM_ADDR 0x7ffff000
#define TC_SIZE     2

//TODO - check if passing #work/thread is okay?
typedef struct {
  uint32_t num_tasks;
  uint32_t num_warps;
  uint32_t matrix_size;
  uint32_t data_size;
  uint64_t src0_addr;
  uint64_t src1_addr;
  uint64_t dst_addr;
} kernel_arg_t;

#endif