#include <stdint.h>
#include <vx_intrinsics.h>
#include <vx_spawn.h>
#include "common.h"

void kernel_body(int task_id, kernel_arg_t* __UNIFORM__ arg) {
	uint32_t count    = arg->task_size;
	int32_t* src0_ptr = (int32_t*)arg->src0_addr;
	int32_t* src1_ptr = (int32_t*)arg->src1_addr;
	int32_t* dst_ptr  = (int32_t*)arg->dst_addr;
	
	unsigned a_addr = reinterpret_cast<unsigned>(src0_ptr);
	unsigned b_addr = reinterpret_cast<unsigned>(src1_ptr);
	unsigned c_addr = reinterpret_cast<unsigned>(dst_ptr);


	uint32_t offset = task_id * count;
	vx_printf("count = %d\n", count);
	for (uint32_t i = 0; i < count; ++i) {
		mload(0,a_addr);
    	//Debug
		mload(1,b_addr);
    	vx_printf("KDEBUG Starting Matmul\n");

    	mm();
    	//vx_printf("KDEBUG Finished Matmul\n");

    	ms(c_addr);
	}

	vx_fence();
}

int main() {
	kernel_arg_t* arg = (kernel_arg_t*)KERNEL_ARG_DEV_MEM_ADDR;
	vx_printf("arg->num_tasks = %d\n", arg->num_tasks);
	vx_spawn_tasks(arg->num_tasks, (vx_spawn_tasks_cb)kernel_body, arg);
	//vx_spawn_tasks(1, (vx_spawn_tasks_cb)kernel_body, arg);
	return 0;
}
