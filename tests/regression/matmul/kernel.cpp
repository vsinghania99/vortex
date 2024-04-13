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

	//TODO - check if okay to send base address like this?
	//TODO - make flexible for data types
	
	unsigned num_threads = arg->num_tasks / ((arg->matrix_size*arg->matrix_size)/(TC_SIZE*TC_SIZE));
	int num_warps = arg->num_warps;
	uint32_t matrix_size = arg->matrix_size;
	uint32_t tc_size = TC_SIZE;
	int n_tiles = matrix_size/tc_size;
	int num_output_tiles = (matrix_size*matrix_size)/(tc_size*tc_size);
	

	int num_tasks = arg->num_tasks;

	int num_warps_actual = MIN(num_output_tiles, num_warps);

	int num_tasks_per_thread = MAX (1, (num_tasks/(num_threads*num_warps_actual)));
	int num_tasks_per_warp = MAX (1, num_tasks/num_warps_actual);
	
	//int num_data_per_thread = MAX(1, (n_tiles*tc_size*tc_size)/(num_threads))
	int addr_shift;
	if (((tc_size*tc_size*n_tiles)/(num_threads)) > 1)
		addr_shift = (tc_size*tc_size*n_tiles)/(num_threads);
	else
		addr_shift = 1;
	int num_data_per_op_tile = tc_size*tc_size*n_tiles;
	int num_data_per_warp = num_data_per_op_tile*(MAX(1, (num_output_tiles/num_warps_actual)));

	//Offset for 1st warp
	int task_id_first_warp = task_id%num_tasks_per_warp;
	int offset = ((task_id_first_warp/num_tasks_per_thread)*addr_shift) + ((task_id_first_warp%num_tasks_per_thread)*num_data_per_op_tile);

	//TODO :: enable this if bigger csr space available and lesser #transfers to be enabled
	//int offset = ((task_id/num_tasks_per_thread)*num_data_per_thread) + ((task_id%num_tasks_per_thread)*num_data_per_op_tile)
	//Generalisation for all warps
	offset = offset + (num_data_per_warp*(task_id/num_tasks_per_warp));

	int addr_shift_c;
	if (((tc_size*tc_size)/(num_threads)) > 1)
		addr_shift_c = tc_size;
	else
		addr_shift_c = 1;
	int num_data_per_op_tile_c = tc_size*tc_size;

	int num_data_per_warp_c = num_data_per_warp/n_tiles;
	int offset_c = ((task_id_first_warp/num_tasks_per_thread)*addr_shift_c) + ((task_id_first_warp%num_tasks_per_thread)*num_data_per_op_tile_c);
	offset_c = offset_c + (num_data_per_warp_c*(task_id/num_tasks_per_warp));

	//unsigned task_id_max = arg->num_tasks;	
	//unsigned offset = (TC_SIZE*TC_SIZE*n_tiles)*((task_id)%(arg->num_tasks/(num_threads))) + (TC_SIZE*TC_SIZE/num_threads)*((task_id)/(arg->num_tasks/(num_threads)));
	//unsigned offset_c = (TC_SIZE*TC_SIZE)*((task_id)%(arg->num_tasks/num_threads)) + (TC_SIZE*TC_SIZE/num_threads)*((task_id)/(arg->num_tasks/(num_threads)));

	//TODO : change this during thread optimization
	//int task_id_max = MIN(arg->num_tasks, num_data_per_op_tile*num_output_tiles);
	int task_id_max = MIN(num_tasks_per_thread, num_output_tiles);
	
	int xyz = MIN(num_threads,tc_size*tc_size*n_tiles);
	int xyz_c = MIN(num_threads,tc_size*tc_size);
	
	
	//unsigned c_addr_base = c_addr + (((task_id*matrix_size)/arg->num_tasks)*4) ; //Fix this
	
	if (((task_id%num_tasks_per_warp)/num_tasks_per_thread) < xyz)
	{	
		unsigned a_addr_base = a_addr + offset*4;
		unsigned b_addr_base = b_addr + offset*4;
		unsigned c_addr_base = c_addr + offset_c*4;
		
		csr_write(VX_MAT_MUL_SIZE,n_tiles);
		mload (0, a_addr_base);
		mload (1, b_addr_base);
		//In case of multiple threads - sync load
		vx_fence();


		mm();   //Assuming padding to ensure matrix size is a multiple of TC_SIZE
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
