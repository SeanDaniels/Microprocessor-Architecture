#include "cache.h"
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <iomanip>

using namespace std;

cache::cache(unsigned size, 
      unsigned associativity,
      unsigned line_size,
      write_policy_t wr_hit_policy,
      write_policy_t wr_miss_policy,
      unsigned hit_time,
      unsigned miss_penalty,
      unsigned address_width
){
	/* edit here */
}

void cache::print_configuration(){
	/* edit here */
}

cache::~cache(){
	/* edit here */
}

void cache::load_trace(const char *filename){
   stream.open(filename);
}

void cache::run(unsigned num_entries){

   unsigned first_access = number_memory_accesses;
   string line;
   unsigned line_nr=0;

   while (getline(stream,line)){

	line_nr++;
        char *str = const_cast<char*>(line.c_str());
	
        // tokenize the instruction
        char *op = strtok (str," ");
	char *addr = strtok (NULL, " ");
	address_t address = strtoul(addr, NULL, 16);

	/* 
		edit here:
		insert the code to process read and write operations
	   	using the read() and write() functions below

	*/

	number_memory_accesses++;
	if (num_entries!=0 && (number_memory_accesses-first_access)==num_entries)
		break;
   }
}

void cache::print_statistics(){
	cout << "STATISTICS" << endl;
	/* edit here */
}

/* update the hit/miss statistics here */
access_type_t cache::read(address_t address){
	/* edit here */
	return MISS;
}

/* update the hit/miss statistics here */
access_type_t cache::write(address_t address){
	/* edit here */
        return MISS;
}

void cache::print_tag_array(){
	cout << "TAG ARRAY" << endl;
	/* edit here */
}

unsigned cache::evict(unsigned index){
	/* edit here */
	return 0;
}

