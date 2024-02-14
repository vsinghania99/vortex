#include <stdio.h>
#include <vx_print.h>
#include <vx_intrinsics.h>

//TODO - fix this
#define SIZE 2

uint32_t A[SIZE][SIZE] =
{
    {3,5},
    {7,9}
};

uint32_t B[SIZE][SIZE] =
{
    {11,13},
    {15,17}
};



uint32_t Ans[SIZE][SIZE] =
{
    {108,124},
    {212,244}
};

int main() {

	int errors = 0;
	vx_printf("KDEBUG Initializing output matrix\n");
    uint32_t C[SIZE][SIZE] =
    {
        {0,0},
        {0,0}
    };

	//Debug
    //vx_printf("KDEBUG A addr = %x\n", &(A[0][0]));
	unsigned a_addr = reinterpret_cast<unsigned>(&(A[0][0]));
	unsigned b_addr = reinterpret_cast<unsigned>(&(B[0][0]));
	unsigned c_addr = reinterpret_cast<unsigned>(&(C[0][0]));

    //Debug
	//vx_printf("KDEBUG a_addr value = %x\n", a_addr);
	vx_printf("KDEBUG Done Initializing output matrix\n");

    ml(0,a_addr);
    //Debug
	//vx_printf("After reinterpret (%x x %d)\n", A[0][0], B[0][0]);
	ml(1,b_addr);
    vx_printf("KDEBUG Starting Matmul\n");

    mm();
    vx_printf("KDEBUG Finished Matmul\n");

    ms(c_addr);
	
    //Debug
	//vx_printf("KDEBUG Result of mul C[0][0] = %d\n", C[0][0]);
	//vx_printf("KDEBUG Result of mul C[0][0] = %d\n", C[0][1]);
	//vx_printf("KDEBUG Result of mul C[0][0] = %d\n", C[1][0]);
	//vx_printf("KDEBUG Result of mul C[0][0] = %d\n", C[1][1]);
    	//comparison
	vx_printf("KDEBUG Starting Comparison\n");
    bool flag = true;
    for(int i = 0; i < SIZE; i++)
    {
        for(int j = 0; j < SIZE; j++)
        {
            if(C[i][j] != Ans[i][j])
            {
                flag = false;
                break;
            }
        }
    }

	vx_printf("KDEBUG Finished Comparison\n");

	if (flag) {
		vx_printf("Passed!\n");
	} else {
		vx_printf("Failed!");
		errors = 1;
	}

	return errors;
}
