#include <stdint.h>
#include <vx_intrinsics.h>
#include <vx_spawn.h>
#include "common.h"
#include <VX_config.h>

void kernel_body(int task_id, kernel_arg_t* __UNIFORM__ arg) {
	//uint32_t tile_num    = arg->task_size;
	int32_t* src0_ptr = (int32_t*)arg->src0_addr;
	int32_t* src1_ptr = (int32_t*)arg->src1_addr;
	int32_t* dst_ptr  = (int32_t*)arg->dst_addr;
	unsigned a_addr = reinterpret_cast<unsigned>(src0_ptr);
	unsigned b_addr = reinterpret_cast<unsigned>(src1_ptr);
	unsigned c_addr = reinterpret_cast<unsigned>(dst_ptr);

	uint32_t tc_size = arg->tc_size;
	int TC_per_warp = arg->TC_per_warp;
	unsigned num_threads = arg->num_threads;
	int num_warps = arg->num_warps;
	uint32_t matrix_size = arg->matrix_size;
	
	int n_tiles = matrix_size/tc_size;
	int num_output_tiles = (matrix_size*matrix_size)/(tc_size*tc_size);
	
	int num_tasks = arg->num_tasks;

	//Assuming matrix size always > tensor core size
	int warps_actual;
	if (TC_per_warp > num_output_tiles)
		warps_actual = 1;
	else 
		warps_actual = num_output_tiles/TC_per_warp;

	int num_warps_actual = MIN(warps_actual, num_warps);
	int num_threads_per_tc = MAX(1, num_threads/TC_per_warp);

	int num_tasks_per_thread = MAX (1, (num_tasks/(num_threads*num_warps_actual)));
	int num_tasks_per_warp = MAX (1, num_tasks/num_warps_actual);
	int task_id_first_warp = task_id%num_tasks_per_warp;

	//A&B
	int num_data_per_op_tile = tc_size*tc_size*n_tiles;
	int num_data_per_warp = num_data_per_op_tile*(MAX(1, (num_output_tiles/num_warps_actual)));
	
	int addr_shift;
	if (((tc_size*tc_size*n_tiles)/(num_threads)) > 1)
		addr_shift = (tc_size*tc_size*n_tiles)/(num_threads);
	else
		addr_shift = 1;
	//Offset for 1st warp
	int offset = ((task_id_first_warp/num_tasks_per_thread)*addr_shift) + ((task_id_first_warp%num_tasks_per_thread)*num_data_per_op_tile);
	offset = offset + (num_data_per_warp*(task_id/num_tasks_per_warp));

	//C
	int num_data_per_op_tile_c = tc_size*tc_size;
	int num_data_per_warp_c = num_data_per_warp/n_tiles;
	
	int addr_shift_c;
	if (((tc_size*tc_size)/(num_threads)) > 1)
		addr_shift_c = tc_size;
	else
		addr_shift_c = 1;
	//Offset for 1st warp
	int offset_c = ((task_id_first_warp/num_tasks_per_thread)*addr_shift_c) + ((task_id_first_warp%num_tasks_per_thread)*num_data_per_op_tile_c);
	offset_c = offset_c + (num_data_per_warp_c*(task_id/num_tasks_per_warp));
	
	//TODO : change this during thread optimization
	
	int xyz = MIN(num_threads,tc_size*tc_size*n_tiles*TC_per_warp);
	int xyz_c = MIN(num_threads,tc_size*tc_size);
		
	//unsigned c_addr_base = c_addr + (((task_id*matrix_size)/arg->num_tasks)*4) ; //Fix this
	if (((task_id%num_tasks_per_warp)/num_tasks_per_thread) < xyz)
	{	
		unsigned a_addr_base = a_addr + offset*arg->data_size;
		unsigned b_addr_base = b_addr + offset*arg->data_size;
		unsigned c_addr_base = c_addr + offset_c*arg->data_size;
		csr_write(VX_MAT_MUL_SIZE,n_tiles);
		mload (0, a_addr_base);
		mload (1, b_addr_base);
		//In case of multiple threads - sync load
		vx_fence();


		mm();   //Assuming padding to ensure matrix size is a multiple of tc_size
		vx_fence();
		if (((task_id%num_tasks_per_warp)/num_tasks_per_thread) < xyz_c)
			ms(c_addr_base);
		//In case of multiple threads - sync store
		vx_fence();
	}
}

int main() {
	kernel_arg_t* arg = (kernel_arg_t*)KERNEL_ARG_DEV_MEM_ADDR;
	vx_printf("arg->num_tasks = %d\n", arg->num_tasks);
	vx_spawn_tasks(arg->num_tasks, (vx_spawn_tasks_cb)kernel_body, arg);
	return 0;
}
