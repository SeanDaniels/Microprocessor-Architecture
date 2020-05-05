#include "../include/cache.h"
#include <iostream>
#include <stdlib.h>
#include <sstream>

#define KB 1024

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

  cout << "cache_size" << ", " << "cache_line_size" << ", " << "associativity" << ", ";
  cout << "number_memory_accesses" << ", " <<  "read_count" << ", " << "read_misses" << ", " << "write_count" << ", " << "write_misses";
  cout << ", " << "eviction_count" << ", " << "number_memory_writes" << ", " << endl;

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

}
