#ifndef CACHE_H_
#define CACHE_H_

#include <fstream>
#include <iostream>
#include <sstream>
#include <stdbool.h>
#include <stdio.h>
#include <string>
#include <cstring>

using namespace std;

#define CACHE_UNDEFINED 0xFFFFFFFFFFFFFFFF // constant used for initialization
#define READ 'r'
#define WRITE 'w'

typedef enum {
WRITE_BACK,
WRITE_THROUGH,
WRITE_ALLOCATE,
NO_WRITE_ALLOCATE
} write_policy_t;

typedef enum { HIT, MISS } access_type_t;

typedef struct{
  long long tag;
  unsigned dirty;
  unsigned valid;
  unsigned cycle_last_used;
}tag_array_entry;

typedef struct{
  unsigned num_entries;
  tag_array_entry* entries;
}tag_array_t;

typedef struct{
  unsigned num_ways;
  unsigned sets_per_way;
  tag_array_t* tag_arrays;
}cache_sets_t;

typedef struct{
  unsigned index;
  unsigned block_offset;
  long long tag;
}parsed_address_t;

typedef long long address_t; // memory address type

class cache {

  /* Add the data members required by your simulator's implementation here */
  unsigned size_of_cache;
  unsigned assoc_of_cache;
  unsigned ln_sz_of_cache;
  unsigned hit_time_of_cache;
  unsigned miss_penalty_of_cache;
  unsigned add_width_of_cache;
  write_policy_t hit_pol_of_cache;
  write_policy_t miss_pol_of_cache;
  unsigned number_of_sets;
  unsigned number_of_blocks;
  unsigned bit_index;
  unsigned bit_block_offset;
  unsigned bit_tag;
  //read count
  unsigned num_instructions_rd;
  //write count
  unsigned num_instructions_wr;
  //read misses
  unsigned num_rd_miss;
  //write misses
  unsigned num_wr_miss;
  //evictions
  unsigned num_evictions;
  //number of memory writes
  unsigned num_mem_wr;
  //number of total hits
  unsigned total_hits;
  //number of total misses
  unsigned total_miss;
  //value for average memory access time
  float current_average;

  cache_sets_t cache_sets;

  /* number of memory accesses processed */
  unsigned number_memory_accesses;

  /* trace file input stream */
  ifstream stream;

  public:
    unsigned cycles_ran = 0;
    /*
     * Instantiates the cache simulator
     */
    cache(unsigned cache_size,          // cache size (in bytes)
          unsigned cache_associativity, // cache associativity (fully-associative
          // caches not considered)
          unsigned cache_line_size,     // cache block size (in bytes)
          write_policy_t write_hit_policy,  // write-back or write-through
          write_policy_t write_miss_policy, // write-allocate or no-write-allocate
          unsigned cache_hit_time,          // cache hit time (in clock cycles)
          unsigned cache_miss_penalty, // cache miss penalty (in clock cycles)
          unsigned address_width       // number of bits in memory address
    );

    // de-allocates the cache simulator
    ~cache();

    // loads the trace file (with name "filename") so that it can be used by the
    // "run" function
    void load_trace(const char *filename);

    // processes "num_memory_accesses" memory accesses (i.e., entries) from the
    // input trace if "num_memory_accesses=0" (default), then it processes the
    // trace to completion
    void run(unsigned num_memory_accesses = 0);

    // processes a read operation and returns hit/miss
    access_type_t read(address_t address);

    // processes a write operation and returns hit/miss
    access_type_t write(address_t address);

    // returns the next block to be evicted from the cache
    unsigned evict(unsigned index);

    // prints the cache configuration
    void print_configuration();

    // prints the execution statistics
    void print_statistics();

    // prints the metadata information (including "dirty" bit, when applicable)
    // for all valid cache entries
    void print_tag_array();
//determine number of sets in cache
    void set_number_sets(unsigned thisNumberOfBlocks, unsigned thisAssociativity);
//determine number of blocks in cache
    void set_number_blocks(unsigned thisCacheSize, unsigned thisLineSize);
//return tag bits
    long long get_tag_bits(address_t thisAddress);
    //return index bits
    long long get_index_bits(address_t thisAddress);
    //return block offset bits
    long long get_block_offset_bits(address_t thisAddress);

    void parse_address(address_t thisAddress, parsed_address_t* thisParsedAddress);

    bool block_empty(unsigned thisIndex,unsigned thisWay);

    unsigned evict_cache_entry(unsigned thisIndex);
};

#endif /*CACHE_H_*/
