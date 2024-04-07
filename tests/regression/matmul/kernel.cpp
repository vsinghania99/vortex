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
	
	//int32_t* src0_ptr = (int32_t*)0x40;
	//int32_t* src1_ptr = (int32_t*)0x;
	//int32_t* dst_ptr  = (int32_t*)arg->dst_addr;
	
	unsigned a_addr = reinterpret_cast<unsigned>(src0_ptr);
	unsigned b_addr = reinterpret_cast<unsigned>(src1_ptr);
	unsigned c_addr = reinterpret_cast<unsigned>(dst_ptr);

	//TODO - check if okay to send base address like this?
	//TODO - make flexible for data types
	//unsigned num_threads = arg->num_tasks / ((arg->matrix_size*arg->matrix_size)/(TC_SIZE*TC_SIZE));
	unsigned num_warps = arg->num_warps;

	uint32_t matrix_size = arg->matrix_size * arg->matrix_size;
	int n_tiles = arg->matrix_size/TC_SIZE;
	//uint32_t num_tiles = matrix_size/(TC_SIZE*TC_SIZE);
	//unsigned a_addr_base = a_addr + (((task_id*matrix_size)/arg->num_tasks)*(arg->matrix_size/(TC_SIZE*num_threads))*4) ;
	//unsigned b_addr_base = b_addr + (((task_id*matrix_size)/arg->num_tasks)*(arg->matrix_size/(TC_SIZE*num_threads))*4) ;
	//unsigned c_addr_base = c_addr + ((( (task_id % num_tiles) ) *(matrix_size)/arg->num_tasks)*(arg->matrix_size/TC_SIZE)*4) ;
	//unsigned offset = 4*((task_id%num_threads) + (task_id/num_threads)*(n_tiles*TC_SIZE*TC_SIZE));
	int TC_per_warp = 2;

	unsigned task_id_max = arg->num_tasks;	
	unsigned offset = ((TC_SIZE*TC_SIZE*n_tiles))*((task_id)%(arg->num_tasks/(arg->num_threads/TC_per_warp))) + ((TC_SIZE*TC_SIZE*n_tiles)/(arg->num_threads/TC_per_warp))*((task_id)/(arg->num_tasks/(arg->num_threads/TC_per_warp)));
	unsigned offset_c = (TC_SIZE*TC_SIZE)*((task_id)%(arg->num_tasks));

	/*if(num_warps >= 2)
	{	//not braining!!
		//Warp Offset + Thread Offset + Time Offset
		//offset = (TC_SIZE*TC_SIZE*n_tiles)*(matrix_size/(TC_SIZE*TC_SIZE)/num_warps)*(task_id/(arg->num_tasks/num_warps)) + (task_id%( ((arg->num_tasks/num_warps)/num_threads) ))*(TC_SIZE*TC_SIZE*n_tiles) + ((task_id%(arg->num_tasks/num_warps))/((arg->num_tasks/num_warps)/num_threads))*(TC_SIZE*TC_SIZE/num_threads) ;
		int size_per_way = TC_SIZE*TC_SIZE*n_tiles; //4
		int tasks_per_warp = MAX(1,arg->num_tasks/(num_warps)); //1
		int tasks_per_wt = MAX(1,tasks_per_warp/num_threads); //1
		int tid_warpid		= (task_id)%(tasks_per_warp); //taskid % 1 = 0
		int tc_size = TC_SIZE*TC_SIZE; //
		int first = (size_per_way*tasks_per_wt*((task_id)/(tasks_per_warp)));   //4*1(0/1) = 0, 4
		int second = ((size_per_way)*(tid_warpid%tasks_per_wt));
		int third = ((tc_size/num_threads)*((tid_warpid)/tasks_per_wt));
		vx_printf("offset 1st term = %x; offset 2nd term = %x; offset 3rd term = %x\n",first, second, third);

		offset = first + second + third;
		offset_c = (TC_SIZE*TC_SIZE*(MAX(1,arg->num_tasks/(num_warps*num_threads))))*(task_id/MAX(1,(arg->num_tasks/num_warps))) + (TC_SIZE*TC_SIZE)*(((task_id)%(MAX(1,arg->num_tasks/(num_warps))))%(MAX(1,arg->num_tasks/(num_threads*num_warps)))) + (TC_SIZE*TC_SIZE/num_threads)*(((task_id)%(MAX(1,arg->num_tasks/num_warps)))/(MAX(1,arg->num_tasks/(num_threads*num_warps))));
	}

	if(num_threads > TC_SIZE*TC_SIZE)
	{  
		task_id_max = TC_SIZE*TC_SIZE;
		offset = (TC_SIZE*TC_SIZE*n_tiles)*((task_id)%(arg->num_tasks/(num_threads))) + ((task_id)/(arg->num_tasks/(num_threads)));
		offset_c = (TC_SIZE*TC_SIZE)*((task_id)%(arg->num_tasks/(num_threads))) + ((task_id)/(arg->num_tasks/(num_threads)));

	}*/

	unsigned a_addr_base = a_addr + offset*4;
	unsigned b_addr_base = b_addr + offset*4;
	unsigned c_addr_base = c_addr + offset_c*4;
	//unsigned c_addr_base = c_addr + (((task_id*matrix_size)/arg->num_tasks)*4) ; //Fix this
	
	//vx_printf("NUM_THREADS = %d\n",NUM_THREADS);
	//vx_printf("num_threads = %d\n",num_threads);
	//vx_printf("task ID = %d: a addr offset = %d\n",task_id,offset);

	//vx_printf("task ID = %d: a addr = %d\n",task_id,a_addr_base);
	//vx_printf("task ID = %d: b addr = %d\n",task_id,b_addr_base);
	//vx_printf("task ID = %d: c addr = %d\n",task_id,c_addr_base);
	
	//for(int i=0; i<32; i++)
	//{
	//	vx_printf("Value at %d=%d\n",src0_ptr+i,*(src0_ptr+i));
	//}

	//vx_printf("N_TILES: %d\n", n_tiles);

	if( (task_id/(TC_SIZE*TC_SIZE)) < task_id_max)
	{	
		csr_write(VX_MAT_MUL_SIZE,n_tiles);
		mload (0, a_addr_base);
		mload (1, b_addr_base);
		//In case of multiple threads - sync load
		vx_fence();


		mm();   //Assuming padding to ensure matrix size is a multiple of TC_SIZE
		vx_fence();

		ms(c_addr_base);
		//In case of multiple threads - sync store
		vx_fence();
	}
}

int main() {
	kernel_arg_t* arg = (kernel_arg_t*)KERNEL_ARG_DEV_MEM_ADDR;
	vx_printf("arg->num_tasks = %d\n", arg->num_tasks);

	//vx_printf("buffer 0\n");
	//for(int i=0; i<32; i++)
	//{
	//	vx_printf("arg->%d=%d\n",arg->src0_addr+i*4,*((int32_t*)arg->src0_addr+i*4));
	//}
	
	vx_spawn_tasks(arg->num_tasks, (vx_spawn_tasks_cb)kernel_body, arg);
	//vx_spawn_tasks(16, (vx_spawn_tasks_cb)kernel_body, arg);
	return 0;
}
