#include "../include/cache.h"
#include "../include/sim_pipe.h"
#include <iostream>
#include <math.h>
#include <stdlib.h>
#include <sstream>

#define KB 1024
#define N 1024 //array size
#define RUN_TWICE
#define VECTOR_ADD

using namespace std;

/* Test case for cache simulator */ 

/* DO NOT MODIFY */
/*When you analyze the impact of the cache size on the miss rate, you
  fix:
  write-hit policy
  write-miss policy,
  the associativity
  and block size (to a couple of
  vary:
  cache size.
*/
int main(int argc, char **argv){
	printf("Cache Size,Block Size,Associativity,Number of Memory Accesses,Miss Rate,\n");
#ifdef VECTOR_ADD
	cache *mycache = NULL;
#ifdef RUN_TWICE
	int runLimit = 16*pow(4, 1);
#endif
/*create cache at given size (loop)*/
	for(int this_cache = 16 * KB; this_cache <= runLimit * KB; this_cache = this_cache * 4){
/*create block size of cache at given size (loop)*/
		for(int this_block_size = 32; this_block_size <= 256; this_block_size = this_block_size * 2){
			unsigned i, j;
			sim_pipe *mips = new sim_pipe(N*N);

			mycache = new cache(this_cache,		//size
								1,			//associativity
								this_block_size,			//cache line size
								WRITE_BACK,		//write hit policy
								WRITE_ALLOCATE, 	//write miss policy
								5, 			//hit time
								100, 			//miss penalty
								32    		//address width
			);
//		mycache->load_trace("traces/GCC.t");

			mips->set_cache(mycache);

			//loads program in instruction memory at address 0x10000000
			mips->load_program("asm/vector_add.asm", 0x10000000);

			// initialize registers
			// Note: arrays A, B and C are laid out contiguously in memory starting from address 0xA0000
			mips->set_gp_register(1, N); 		//number of elements in array	
			mips->set_gp_register(2, 0xA0000); 	//address of array A
			mips->set_gp_register(3, 0xA0000+N*4); 	//address of array B
			mips->set_gp_register(4, 0xA0000+N*8); 	//address of array C

			//initialize data memory and prints its content (for the specified address ranges)
			for (i = 0xA0000, j=0; j<N; i+=4, j++) mips->write_memory(i,j);
			for (i = 0xA0000+N*4, j=0; j<N; i+=4, j++) mips->write_memory(i,j*2);
			//run pipeline
			mips->run();
			//print cache output
			mycache->easier_output();
			cout << "\n" ;
			//delete cache
			delete mycache;
		}
	}
#endif
#ifdef OG_VERSION
	cache *mycache = NULL;
	for(int this_cache = 16 * KB; this_cache <= 256 * KB; this_cache = this_cache * 4){
		for(int assoc = 1; assoc <= 16; ){
			for(int this_block_size = 32; this_block_size <= 256; this_block_size = this_block_size * 2){
				mycache = new cache(this_cache,		//size
									assoc,			//associativity
									this_block_size,			//cache line size
									WRITE_BACK,		//write hit policy
									WRITE_ALLOCATE, 	//write miss policy
									5, 			//hit time
									100, 			//miss penalty
									32    		//address width
				);
				mycache->load_trace("traces/GCC.t");
				mycache->run();
				mycache->easier_output();
				cout << "\n" ;
				delete mycache;
			}
			if(assoc == 1){
				assoc = assoc + 1;
			}
			else{
				assoc = assoc * assoc;
			}
		}
	}
#endif
#ifdef EIGHT_WAY
	cache *mycache = NULL;
	int assoc = 8;
	for(int this_cache = 16 * KB; this_cache <= 256 * KB; this_cache = this_cache * 4){
		for(int this_block_size = 32; this_block_size <= 256; this_block_size = this_block_size * 2){
			mycache = new cache(this_cache,		//size
								assoc,			//associativity
								this_block_size,			//cache line size
								WRITE_BACK,		//write hit policy
								WRITE_ALLOCATE, 	//write miss policy
								5, 			//hit time
								100, 			//miss penalty
								32    		//address width
			);
			mycache->load_trace("traces/GCC.t");
			mycache->run();
			mycache->easier_output();
			cout << "\n" ;
			delete mycache;
		}
		
	}
#endif

}
