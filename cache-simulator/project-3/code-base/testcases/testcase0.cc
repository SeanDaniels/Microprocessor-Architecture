#include "cache.h"
#include <iostream>
#include <stdlib.h>
#include <sstream>

#define KB 1024

using namespace std;

/* Test case for cache simulator */ 

/* DO NOT MODIFY */

int main(int argc, char **argv){

	//WRITE-BACK, WRITE-ALLOCATE

	cache *mycache = new cache(128*KB,		//size
				  2,			//associativity
				  256,			//cache line size
				  WRITE_BACK,		//write hit policy
				  WRITE_ALLOCATE, 	//write miss policy
				  5, 			//hit time
				  100, 			//miss penalty
				  32    		//address width
				  );     

	cout << "FIRST CONFIGURATION" << endl << "------------------------" << endl << endl;

	mycache->print_configuration();
	cout << endl;

	mycache->load_trace("traces/simple.t");

	cout << "ONE MEMORY ACCESS AT A TIME" << endl;
	cout << "===========================" << endl;
	for (int i=0; i<12; i++){
                cout << "MEMORY ACCESS #" << dec << i << endl;
		mycache->run(1);
		mycache->print_tag_array();
		cout << endl;
	}

	mycache->print_statistics();
	cout << endl;

	delete mycache;

	//WRITE-THROUGH, NO-WRITE-ALLOCATE

	cout << "=================================" << endl  << endl;
	cout << "SECOND CONFIGURATION" << endl << "------------------------" << endl << endl;
        
	mycache = new cache(128*KB,              //size
                                  2,                    //associativity
                                  256,                  //cache line size
                                  WRITE_THROUGH,           //write hit policy
                                  NO_WRITE_ALLOCATE,       //write miss policy
                                  5,                    //hit time
                                  100,                  //miss penalty
                                  32                    //address width
                                  );

        mycache->print_configuration();
	cout << endl;

        mycache->load_trace("traces/simple.t");

	cout << "ONE MEMORY ACCESS AT A TIME" << endl;
        for (int i=0; i<12; i++){
                cout << "MEMORY ACCESS #" << dec << i << endl;
		mycache->run(1);
		mycache->print_tag_array();
		cout << endl;
        }

        mycache->print_statistics();
	cout << endl;

        delete mycache;
}
