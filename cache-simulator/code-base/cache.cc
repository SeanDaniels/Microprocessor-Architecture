#include "cache.h"
#include <cstdlib>
#include <cstdio>
#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <iomanip>
#include <cmath>
#include <bitset>
#include <sstream>
//////////////
// dbg defs //
//////////////
//#define INIT_DBG
//#define ADDR_DBG
//#define BLOCK_BIT_DBG
//#define READ_DBG
#define SET_HIERARCHY
//#define META_BLOCK_DBG

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
    size_of_cache =  size;
    assoc_of_cache = associativity;
    ln_sz_of_cache = line_size;
    hit_pol_of_cache = wr_hit_policy;
    miss_pol_of_cache = wr_miss_policy;
    hit_time_of_cache = hit_time;
    miss_penalty_of_cache = miss_penalty;
    add_width_of_cache = address_width;
    bit_block_offset = log2(line_size);
    set_number_blocks(size, line_size);
    set_number_sets(number_of_blocks, assoc_of_cache);
    bit_index= log2(number_of_sets);
    bit_tag = add_width_of_cache - (bit_index+ bit_block_offset);
#ifdef INIT_DBG
    cout << "Number of bits for block offset: " << bit_block_offset << endl;
    cout << "Number of bits for index: " << bit_index << endl;
    cout << "Number of bits for tag: " << bit_tag << endl;
    cout << "Size of long long: " << sizeof(long long) << endl;
    cout << "Size of unsigned long long: " << sizeof(unsigned long long) << endl;
    #endif
    //create a set array
#ifdef SET_HIERARCHY
    cache_sets.num_ways = assoc_of_cache;
    cache_sets.sets_per_way = number_of_sets;
    cache_sets.tag_arrays = new tag_array_t[assoc_of_cache];
    for(unsigned i = 0; i< assoc_of_cache; i++){
        cache_sets.tag_arrays[i].num_entries = number_of_sets;
        cache_sets.tag_arrays[i].entries = new tag_array_entry[number_of_sets];
        unsigned j;
        for(j = 0; j<number_of_sets; j++){
            cache_sets.tag_arrays[i].entries[j].tag = 0;
            cache_sets.tag_arrays[i].entries[j].dirty = 0;
            cache_sets.tag_arrays[i].entries[j].valid = 0;
        }
        #ifdef META_BLOCK_DBG
        cout << "Created way " << i << " with " << j << " blocks." << endl;
        cout << "Cache set info: " << endl;
        cout << "Number of ways: " << cache_sets.num_ways << endl;
        cout << "Number of sets per way: " << cache_sets.sets_per_way << endl;
        cout << "Number of sets in tag array 1: " << cache_sets.tag_arrays[0].num_entries << endl;
        cout << "Number of sets in tag array 2: " << cache_sets.tag_arrays[1].num_entries << endl;
        #endif
    }
#else
    //create the tag array
        tag_array.entries = new tag_array_entry[number_of_blocks];
    //init tag array
        for(unsigned i = 0; i<number_of_blocks; i++){
            tag_array.entries[i].tag = 0;
            tag_array.entries[i].dirty = 0;
            tag_array.entries[i].valid = 0;
        }
#endif

    number_memory_accesses = 0;
    num_mem_wr = 0;
    num_instructions_rd = 0;
    num_instructions_wr = 0;
    num_rd_miss = 0;
    num_wr_miss = 0;
    num_evictions = 0;
    cycles_ran = 0;

}

void cache::print_configuration(){
    /* edit here */
    cout << "CACHE CONFIGURATION" << endl;
    cout << "size = ";
    if(size_of_cache>=1024){
        cout << size_of_cache/1024 << " KB" << endl;
    }
    if(size_of_cache<1024){
        cout << size_of_cache << " B" << endl;
    }
    cout << "associativity = " << assoc_of_cache << "-way"<< endl;
    cout << "cache line size = " << ln_sz_of_cache <<" B"<< endl;
    cout << "write hit policy = ";
    if(hit_pol_of_cache==WRITE_BACK){
        cout << "write-back" << endl;
    }
    if(hit_pol_of_cache==WRITE_THROUGH){
        cout << "write-through" << endl;
    }
    cout << "write miss policy = ";
    if(miss_pol_of_cache == WRITE_ALLOCATE){
        cout << "write-allocate" << endl;
    }
    if(miss_pol_of_cache == NO_WRITE_ALLOCATE){
        cout << "no-write-allocate" << endl;
    }

    cout << "cache hit time = " << hit_time_of_cache << " CLK"<< endl;
    cout << "cache miss penalty = " << miss_penalty_of_cache << " CLK"<< endl;
    cout << "memory address width = " << add_width_of_cache << " bits"<< endl;
}

cache::~cache(){
    /* edit here */
    delete[] cache_sets.tag_arrays;
    number_memory_accesses = 0;
    num_mem_wr = 0;
    num_instructions_rd = 0;
    num_instructions_wr = 0;
    num_rd_miss = 0;
    num_wr_miss = 0;
    num_evictions = 0;
    number_of_sets = 0;
    number_of_blocks = 0;
    cycles_ran = 0;
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
        // r or w
        char *op = strtok (str," ");
        //address as string
        char *addr = strtok (NULL, " ");
        //string converted to unsigned long
        address_t address = strtoul(addr, NULL, 16);
#ifdef INIT_DBG
        printf("%s\n",op);
        printf("%s\n", addr);
        printf("%lld\n",address);
#endif
        if(*op==READ){
            read(address);
        }
        else if(*op==WRITE){
            write(address);
        }
        else{
            cout << "ERROR: NOT READ OR WRITE" << endl;
        }
        cycles_ran++;
        number_memory_accesses++;
        if (num_entries!=0 && (number_memory_accesses-first_access)==num_entries)
            break;
    }
}

void cache::print_statistics(){
    //total hits for mem access time
    total_hits = num_instructions_rd + num_instructions_wr;
    //total miss for mem access time
    total_miss = num_rd_miss + num_wr_miss;
    //calculate current average
    float num = total_hits*hit_time_of_cache + total_miss*miss_penalty_of_cache;
    float den = number_memory_accesses;
    
    current_average = num/den;
    //output statistics
    cout << "STATISTICS" << endl;
    cout << "memory accesses = " << dec << number_memory_accesses << endl;
    cout << "read = " << dec << num_instructions_rd << endl;
    cout << "read misses = " << dec <<num_rd_miss << endl;
    cout << "write = " << dec << num_instructions_wr << endl;
    cout << "write misses = " << dec << num_wr_miss << endl;
    cout << "evictions = " << dec << num_evictions << endl;
    cout << "memory writes = " << dec<< num_mem_wr << endl;
    //output average w defined precision
    cout << "average memory access time = " << setprecision(6) << current_average << endl;;
    /* edit here */
}

/* update the hit/miss statistics here */
access_type_t cache::read(address_t address){
    /* edit here */
    bool allEmpty = false;
    parsed_address_t tempParsedAddress;
    parse_address(address, &tempParsedAddress);
    //compare tags, if found in correct set, return the hit
    for(unsigned i = 0; i<cache_sets.num_ways;i++){
        //check if empty
        //if empty, record miss
        //if empty, place tag in this entry, exit loop
        //if not empty,check next way
        //if way 2 empty, handle like empty
        //if way 2 not empty, record miss
        //if not empty, compare tag
        if(tempParsedAddress.tag == cache_sets.tag_arrays[i].entries[tempParsedAddress.index].tag){
            num_instructions_rd++; 
            cache_sets.tag_arrays[i].entries[tempParsedAddress.index].cycle_last_used = cycles_ran;
            return HIT;
        }
    }
    //if there is an available set position in the cache, return the miss
    //update the cache at this position
    for(unsigned i = 0; i<cache_sets.num_ways;i++){
        if(cache_sets.tag_arrays[i].entries[tempParsedAddress.index].valid == 0){
            num_instructions_rd++;
            num_rd_miss++;
            cache_sets.tag_arrays[i].entries[tempParsedAddress.index].valid = 1; 
            cache_sets.tag_arrays[i].entries[tempParsedAddress.index].dirty = 0; 
            cache_sets.tag_arrays[i].entries[tempParsedAddress.index].tag = tempParsedAddress.tag; 
            cache_sets.tag_arrays[i].entries[tempParsedAddress.index].cycle_last_used = cycles_ran; 
            return MISS;
        }
    }
    //if made it here, both ways are populated, need to evict one of the way sets
    unsigned wayCleared = evict_cache_entry(tempParsedAddress.index);
    //insert new cache entry
    cache_sets.tag_arrays[wayCleared].entries[tempParsedAddress.index].dirty = 0;
    cache_sets.tag_arrays[wayCleared].entries[tempParsedAddress.index].tag = tempParsedAddress.tag;
    cache_sets.tag_arrays[wayCleared].entries[tempParsedAddress.index].valid = 1;
    cache_sets.tag_arrays[wayCleared].entries[tempParsedAddress.index].cycle_last_used = cycles_ran;
    num_instructions_rd++;
    num_rd_miss++;
    //return the miss 
    #ifdef READ_DBG
    cout << "Reading." << endl;
    cout << "Parsed address tag: " << tempParsedAddress.tag << endl;
    cout << "Parsed address index: " << tempParsedAddress.index << endl;
    cout << "Parsed address block offset: " << tempParsedAddress.block_offset << endl;
    #endif
    return MISS;
}

/* update the hit/miss statistics here */
access_type_t cache::write(address_t address){
    //increment write number
    num_instructions_wr++;
    //break address down
    parsed_address_t tempParsedAddress;
    parse_address(address, &tempParsedAddress);
    //check all ways 
    for(unsigned i = 0; i < cache_sets.num_ways; i++){
        tag_array_entry* tempEntry = &cache_sets.tag_arrays[i].entries[tempParsedAddress.index];
        //compare tags
        if(tempParsedAddress.tag == tempEntry->tag){
            //if same, update count for LRU
            tempEntry->cycle_last_used=cycles_ran; 
            //if wb, set dirty bit
            if(hit_pol_of_cache==WRITE_BACK){
                tempEntry->dirty=1;
            }
            //if not, increment write count, make sure dirty bit is clear
            else{
                num_mem_wr++;
                tempEntry->dirty=0;
            }
            //retunr cache hit
            return HIT;
        }
        
    }
    //didn't hit, increment miss count
    num_wr_miss++;
    //two cases, write allocate, no-write-allocate
    if(miss_pol_of_cache==WRITE_ALLOCATE){
        //check both ways
        for(unsigned i = 0; i < cache_sets.num_ways; i++){
            tag_array_entry* tempEntry = &cache_sets.tag_arrays[i].entries[tempParsedAddress.index];
            if(tempEntry->valid==0){
            //update tag entry valid bit
                tempEntry->valid=1;
                if(hit_pol_of_cache==WRITE_BACK){
                    tempEntry->dirty=1;
                }
                else{
                    tempEntry->dirty=0;
                    //increment number of writes
                    num_mem_wr++;
                }
                //set stag
                tempEntry->tag=tempParsedAddress.tag;
                //update lru counter
                tempEntry->cycle_last_used=cycles_ran;
                return MISS;
            }
        }
        //no empty blocks, trigger eviction
        unsigned wayCleared = evict_cache_entry(tempParsedAddress.index);

        tag_array_entry* evictedTagEntry = &cache_sets.tag_arrays[wayCleared].entries[tempParsedAddress.index];
        evictedTagEntry->valid = 1;
        if(hit_pol_of_cache==WRITE_BACK){
            evictedTagEntry->dirty = 1;
        }
        else{
            num_mem_wr++;
            evictedTagEntry->dirty = 0;
        }
        //set tag
        evictedTagEntry->tag = tempParsedAddress.tag;
        //update lru counter
        evictedTagEntry->cycle_last_used = cycles_ran;
       return MISS; 
    }
    if((hit_pol_of_cache==WRITE_THROUGH)&&(miss_pol_of_cache==NO_WRITE_ALLOCATE)){
        num_mem_wr++;
        return MISS;
    }
    return MISS;
}

void cache::print_tag_array(){
	cout << "TAG ARRAY" << endl;
    for(unsigned i = 0; i < cache_sets.num_ways; i++){
        cout << "BLOCKS " << i << endl;
        if(miss_pol_of_cache == WRITE_ALLOCATE){
            cout << setfill(' ') << setw(7) << "index" << setw(6) << "dirty" << setw(4 + bit_tag/4) << "tag" << endl;
            for(unsigned j = 0; j < cache_sets.sets_per_way; j++){
                if(cache_sets.tag_arrays[i].entries[j].valid!=0){
                    cout << setfill(' ') << setw(7) << dec << j << setw(6) << cache_sets.tag_arrays[i].entries[j].dirty << setw(4) << "0x"  << setw(bit_tag/4) << hex << cache_sets.tag_arrays[i].entries[j].tag << endl;
                }
            }
        }
        else{
            cout << setfill(' ') << setw(7) << "index" << setw(4 + bit_tag/4) << "tag" << endl;
            for(unsigned j = 0; j < cache_sets.sets_per_way; j++){
                if(cache_sets.tag_arrays[i].entries[j].valid!=0){
                    cout << setfill(' ') << setw(7) << dec << j << setw(4) <<  "0x"  << setw(bit_tag/4) << hex << cache_sets.tag_arrays[i].entries[j].tag << endl;
                }
            }
        }
    }
	/* edit here */
}

unsigned cache::evict(unsigned index){
	/* edit here */
	return 0;
}

void cache::set_number_blocks(unsigned thisCacheSize, unsigned thisLineSize){
#ifdef INIT_DBG
    cout << "Setting number of blocks" << endl;
    cout << "Cache size: " << thisCacheSize << endl;
    cout << "Line size: " << thisLineSize << endl;
#endif
    number_of_blocks = thisCacheSize/thisLineSize;
#ifdef INIT_DBG
    cout << "Number of blocks: " << number_of_blocks << endl;
#endif
}

void cache::set_number_sets(unsigned thisNumberOfBlocks, unsigned thisAssociativity){
#ifdef INIT_DBG
    cout << "Setting number of sets" << endl;
    cout << "Number of blocks: " << thisNumberOfBlocks << endl;
    cout << "Assoc.: " << thisAssociativity << endl;
    #endif
    number_of_sets = thisNumberOfBlocks/thisAssociativity;
#ifdef INIT_DBG
    cout << "Number of sets: " << number_of_sets << endl;
    #endif

}



    

void cache::parse_address(address_t thisAddress, parsed_address_t* thisParsedAddress){
    //set masks for segments
    unsigned blockOffsetMask = pow(2,bit_block_offset) - 1;
    unsigned indexMask = pow(2,bit_index) - 1;
    unsigned tagMask = pow(2,bit_tag) - 1;
    //get block offset
    unsigned blockOffset = thisAddress&blockOffsetMask;
    //shift address bits
    long long shiftedAddress = thisAddress >> bit_block_offset;
    //get index
    unsigned index = shiftedAddress&indexMask;
    //shift address bits
    long long tagAddress = shiftedAddress >> bit_index;
    //isolate tag
    unsigned long long tag = tagAddress&tagMask;
    thisParsedAddress->block_offset= blockOffset;
    thisParsedAddress->index = index;
    thisParsedAddress->tag = tagAddress;

    
#ifdef BLOCK_BIT_DBG
    cout << "Number of of block offset bits = " << bit_block_offset << endl;
    cout << "The block offset mask is " << hex << blockOffsetMask;
    cout << "The result of address & blockOffsetMask is " << blockOffset << endl;
    cout << "The result of address & blockOffsetMask in hex is " << hex <<blockOffset << endl;
    cout << "Address is " << thisAddress << endl;
    cout << "Address in hex is " << hex << thisAddress << endl;
    cout << "The 'block offset' shifted address is " << shiftedAddress << endl;
    cout << "The 'block offset' shifted address in hex is " << hex << shiftedAddress << endl;
    cout << "The result of shifted address & index mask is " << index << endl;
    cout << "The result of address & index mask in hex is " << hex << index << endl;
    cout << "The 'index' shifted address is " << tagAddress << endl;
    cout << "The 'index' shifted address in hex is " << hex << tagAddress << endl;
    cout << "The result of index shifted address & index mask is " << tag << endl;
    cout << "The result of index shifted address & index mask in hex is " << hex << tag << endl;
    #endif
    
}



unsigned cache::evict_cache_entry(unsigned thisIndex){
    //check populated set locations
    //compare for least recently used
    //return index of least recently used
    unsigned furthestClockCycle = cache_sets.tag_arrays[0].entries[thisIndex].cycle_last_used;
    unsigned returnIndex = 0;
    //check entries in the ways, return the way that houses the least recently used
    for(unsigned i = 0; i<cache_sets.num_ways; i++){
        if(cache_sets.tag_arrays[i].entries[thisIndex].cycle_last_used<furthestClockCycle){
            furthestClockCycle = cache_sets.tag_arrays[i].entries[thisIndex].cycle_last_used;
            returnIndex = i;
        }
    }
    //clear dirty bit if necessary, remove valid flag
    //update memory writes?
    if(cache_sets.tag_arrays[returnIndex].entries[thisIndex].dirty==1){
        num_mem_wr++;
        cache_sets.tag_arrays[returnIndex].entries[thisIndex].dirty=0;
        cache_sets.tag_arrays[returnIndex].entries[thisIndex].valid=0;
    }
    
    num_evictions++;
    return returnIndex; 
}
