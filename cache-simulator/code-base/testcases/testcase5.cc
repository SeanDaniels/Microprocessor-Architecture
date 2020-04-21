#include "cache.h"
#include <iostream>
#include <stdlib.h>
#include <sstream>

#define KB 1024

using namespace std;

/* Test case for cache simulator */ 

/* DO NOT MODIFY */

int main(int argc, char **argv){

	cache *mycache = NULL;

	for (unsigned b=32; b<=256; b=b*2){

	cout << "BLOCK SIZE = " << dec << b << endl;
	cout << "===================" << endl << endl;

	mycache = new cache(     16*KB,			//size
				  2,			//associativity
				  b,			//cache line size
				  WRITE_BACK,		//write hit policy
				  WRITE_ALLOCATE, 	//write miss policy
				  5, 			//hit time
				  100, 			//miss penalty
				  48    		//address width
				  );     

	mycache->print_configuration();

	mycache->load_trace("traces/GCC.t");

	mycache->run();	

	cout << endl;

	mycache->print_statistics();

	cout << endl;
	
	delete mycache;

	}

}
