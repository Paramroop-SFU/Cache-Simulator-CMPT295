#include "cache.h"
#include "dogfault.h"
#include <assert.h>
#include <ctype.h>
#include <getopt.h>
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
//NEED TO FIX probe_cache!!!!!!
// DO NOT MODIFY THIS FILE. INVOKE AFTER EACH ACCESS FROM runTrace
void print_result(result r)
{
  if (r.status == CACHE_EVICT)
    printf(" [status: %d victim_block: 0x%llx insert_block: 0x%llx]", r.status,
           r.victim_block, r.insert_block);
  if (r.status == CACHE_HIT)
    printf(" [status: %d]", r.status);
  if (r.status == CACHE_MISS)
    printf(" [status: %d insert_block: 0x%llx]", r.status, r.insert_block);
}

// HELPER FUNCTIONS USEFUL FOR IMPLEMENTING THE CACHE
// Convert address to block address. 0s out the bottom block bits.
unsigned long long address_to_block(const unsigned long long address, const Cache *cache)
{
  unsigned long long zero_out = address >> cache->blockBits;
  zero_out = zero_out << cache->blockBits;
  return zero_out;

}

// Access the cache after successful probing.
void access_cache(const unsigned long long address, const Cache *cache)
{
}

// Calculate the tag of the address. 0s out the bottom set bits and the bottom block bits.
unsigned long long cache_tag(const unsigned long long address,const Cache *cache)                            
{
  unsigned long long tag = address;
  tag = tag >> cache->setBits >> cache ->blockBits;
  tag = tag << cache->setBits << cache ->blockBits;

  return tag;
}

// Calculate the set of the address. 0s out the bottom block bits, 0s out the tag bits, and then shift the set bits to the right.
unsigned long long cache_set(const unsigned long long address,const Cache *cache)                            
{
    unsigned long long set = address;
    int tag = 64 - (cache->blockBits + cache->setBits);
    set = set << tag >> tag >> cache->blockBits;
    return set;
}

// Check if the address is found in the cache. If so, return true. else return false.
bool probe_cache(const unsigned long long address, const Cache *cache)
{
  int set = pow(2,cache->setBits);
  unsigned long long block = address_to_block(address,cache);

  for (int i = 0; i < set; i++)
  {
    for (int p = 0; p < cache->linesPerSet; p++)
    {
        if (cache->sets[i].lines[p].valid == true && cache->sets[i].lines[p].block_addr == block)
        {
          cache->sets[i].recentRate++;
          cache->sets[i].lines[p].r_rate = cache->sets[i].recentRate;
          return true;
        }
    }
  }
  return false;
}

// Allocate an entry for the address. If the cache is full, evict an entry to create space. This method will not fail. When method runs there should have already been space created.
void allocate_cache(const unsigned long long address, const Cache *cache)
{
  unsigned long long block = address_to_block(address,cache);
  unsigned long long set_position = cache_set(address,cache);
  Set cache_pos = cache->sets[set_position];
  for (int i = 0; i < cache->linesPerSet; i++ )
  {
    if (cache_pos.lines[i].valid == 0)
    {
      //printf("\n\n\n\n\n %lld \n\n\n\n\n",block);
      cache_pos.lines[i].block_addr = block;
       cache_pos.lines[i].tag = cache_tag(address,cache);
        cache_pos.lines[i].valid = 1;
        cache_pos.recentRate++;
        cache_pos.lines[i].r_rate = cache_pos.recentRate;
       return;
    }
  }
  
}

// Is there space available in the set corresponding to the address?
bool avail_cache(const unsigned long long address, const Cache *cache)
{
  // calculate tag and set values
  //unsigned long long tagbit = cache_tag(address,cache);
  unsigned long long setvalue = cache_set(address,cache);
  Set cache_pos = cache->sets[setvalue];
  for (int i = 0; i < cache->linesPerSet; i++ )
  {
    if (cache_pos.lines[i].valid == 0)
    {
      return true;
    }
  }
  return false;
}

// If the cache is full, evict an entry to create space. This method figures out which entry to evict. Depends on the policy.
unsigned long long victim_cache(const unsigned long long address,
                                Cache *cache)
{
  unsigned long long setvalue = cache_set(address,cache);
  Set cache_pos = cache->sets[setvalue];
  int rate = cache_pos.lines[0].r_rate;
  
  unsigned long long index = 0;
  for (int i = 0;i < cache->linesPerSet; i++ )
  {
    if (cache_pos.lines[i].r_rate < rate)
    {
      rate = cache_pos.lines[i].r_rate;
      index = i;
    }

  }
  return index;
}

// Set can be determined by the address. Way is determined by policy and set by the operate cache.
void evict_cache(const unsigned long long address, int way, Cache *cache)
{
  unsigned long long setvalue = cache_set(address,cache);
  Set cache_pos = cache->sets[setvalue];
  cache_pos.lines[way].valid = 0;
  cache_pos.lines[way].r_rate = 0;
}

// Given a block address, find it in the cache and when found remove it.
// If not found don't remove it. Useful when implementing 2-level policies.
// and triggering evictions from other caches.
void flush_cache(const unsigned long long block_address, Cache *cache)
{
 int set = pow(2,cache->setBits);
  

  for (int i = 0; i < set; i++)
  {
    for (int p = 0; p < cache->linesPerSet; p++)
    {
        if (cache->sets[i].lines[p].valid == true && cache->sets[i].lines[p].block_addr == block_address)
        {
          cache->sets[i].lines[p].valid = 0;
          cache->sets[i].lines[p].r_rate = 0;
          
        }
    }
  }
  
}
// checks if the address is in the cache, if not and if the cache is full
// evicts an address
result operateCache(const unsigned long long address, Cache *cache)
{
   result r;
   unsigned long long block = address_to_block(address,cache);
   unsigned long long set_num = cache_set(address,cache);
   r.insert_block = block;
   bool found = probe_cache(address,cache);
   if (found == true)
   {
    printf(" %s hit ",cache->name);
    r.status = 1; 
    return r;
   }
   bool space = avail_cache(address,cache);
   if (space == true)
   {
    printf(" %s insert ",cache->name);
    allocate_cache(address,cache);
    r.status = 0; 
   }
   else
   {
    printf(" %s insert + eviction ",cache->name);
      unsigned long long victim = victim_cache(address,cache);
      r.victim_block = cache->sets[set_num].lines[victim].block_addr;
      evict_cache(address,victim,cache);
      allocate_cache(address,cache);
       r.status = 2; 
       cache->eviction_count++;
      
   }


  // Hit
  // Miss
  // Evict
  return r;
}

// initialize the cache and allocate space for it
void cacheSetUp(Cache *cache, char *name)
{
  int num_sets = pow(2,cache->setBits);
  int E = cache->linesPerSet;
  cache->sets = (Set*)malloc(sizeof(Set)*num_sets);

  for (int i = 0; i < num_sets; i++)
  {
    cache->sets[i].lines = (Line*)malloc(sizeof(Line)*E);
    cache->sets[i].recentRate = 0;


    for (int j = 0; j < E; j++)
    {
      cache->sets[i].lines[j].valid = false;
      cache->sets[i].lines[j].r_rate = 0;
    }
  }

  cache->name = name;
  cache->hit_count = 0;
  cache->miss_count = 0;
  cache->eviction_count = 0;
}

// deallocate memory
void deallocate(Cache *cache)
{
  int setnumber = pow(2,cache->setBits);
  for (int i = 0; i < setnumber; i++ )
  {
    free(cache->sets[i].lines);
  }
  free(cache->sets);
}

void printSummary(const Cache *cache)
{
  printf("\n%s hits:%d misses:%d evictions:%d", cache->name, cache->hit_count,
         cache->miss_count, cache->eviction_count);
}

