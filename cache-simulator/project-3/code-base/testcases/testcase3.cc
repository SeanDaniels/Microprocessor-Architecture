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

	cout << "WRITE-BACK/WRITE-ALLOCATE" << endl;
	cout << "=========================" << endl << endl;

	mycache = new cache(     64*KB,			//size
				  1,			//associativity
				  64,			//cache line size
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

	cout << "WRITE-THROUGH/NO-WRITE-ALLOCATE" << endl;
	cout << "===============================" << endl << endl;

	mycache = new cache(     64*KB,			//size
				  1,			//associativity
				  64,			//cache line size
				  WRITE_THROUGH,	//write hit policy
				  NO_WRITE_ALLOCATE, 	//write miss policy
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
	
	
	cout << "WRITE-BACK/NO-WRITE-ALLOCATE" << endl;
	cout << "============================" << endl << endl;
	
	mycache = new cache(     64*KB,			//size
				  1,			//associativity
				  64,			//cache line size
				  WRITE_BACK,		//write hit policy
				  NO_WRITE_ALLOCATE, 	//write miss policy
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


	cout << "WRITE-THROUGH/WRITE-ALLOCATE" << endl;
	cout << "============================" << endl << endl;

	mycache = new cache(     64*KB,			//size
				  1,			//associativity
				  64,			//cache line size
				  WRITE_THROUGH,	//write hit policy
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
