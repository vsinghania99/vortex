#include <stdint.h>
#include <vx_intrinsics.h>
#include <vx_spawn.h>
#include "common.h"

void kernel_body(int task_id, kernel_arg_t* __UNIFORM__ arg) {
	//uint32_t tile_num    = arg->task_size;
	int32_t* src0_ptr = (int32_t*)arg->src0_addr;
	int32_t* src1_ptr = (int32_t*)arg->src1_addr;
	int32_t* dst_ptr  = (int32_t*)arg->dst_addr;
	
	unsigned a_addr = reinterpret_cast<unsigned>(src0_ptr);
	unsigned b_addr = reinterpret_cast<unsigned>(src1_ptr);
	unsigned c_addr = reinterpret_cast<unsigned>(dst_ptr);

	//TODO - check if okay to send base address like this?
	//TODO - make flexible for data types
	unsigned a_addr_base = a_addr + (((task_id*arg->matrix_size)/arg->num_tasks)*4) ;
	unsigned b_addr_base = b_addr + (((task_id*arg->matrix_size)/arg->num_tasks)*4) ;
	unsigned c_addr_base = c_addr + (((task_id*arg->matrix_size)/arg->num_tasks)*4) ;
	
	mload (0, a_addr_base);
	mload (1, b_addr_base);
	//In case of multiple threads - sync load
	vx_fence();

    mm();

	ms(c_addr_base);
	//In case of multiple threads - sync store
	vx_fence();
}

int main() {
	kernel_arg_t* arg = (kernel_arg_t*)KERNEL_ARG_DEV_MEM_ADDR;
	vx_printf("arg->num_tasks = %d\n", arg->num_tasks);
	vx_spawn_tasks(arg->num_tasks, (vx_spawn_tasks_cb)kernel_body, arg);
	//vx_spawn_tasks(1, (vx_spawn_tasks_cb)kernel_body, arg);
	return 0;
}
