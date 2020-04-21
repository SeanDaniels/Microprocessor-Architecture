#include "cache.h"
#include <iostream>
#include <stdlib.h>
#include <sstream>

#define KB 1024

using namespace std;

/* Test case for cache simulator */ 

/* DO NOT MODIFY */

int main(int argc, char **argv){

	cache *mycache = new cache(32*KB,		//size
				  4,			//associativity
				  32,			//cache line size
				  WRITE_BACK,		//write hit policy
				  WRITE_ALLOCATE, 	//write miss policy
				  5, 			//hit time
				  100, 			//miss penalty
				  48    		//address width
				  );     

	mycache->print_configuration();
	cout << endl;

	mycache->load_trace("traces/GCC.t");

	cout << endl << "Run 50 memory accesses..." << endl;
	mycache->run(50);
	mycache->print_tag_array();
	cout << endl;

	cout << endl << "Run 1000 memory accesses..." << endl;
	mycache->run(1000);
	mycache->print_tag_array();
	cout << endl;
	mycache->print_statistics();
	cout << endl;

	cout << endl << "Complete execution..." << endl;
	mycache->run();
	mycache->print_statistics();
}
