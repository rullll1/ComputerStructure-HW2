//
// Created by rulll on 26/12/2024.
//


#ifndef MEMORY_H
#define MEMORY_H
#define WRITE_ALLOCATE 1
#define NO_WRITE_ALLOCATE 0
#include <iostream>
#include <list>
#include <map>
#include <vector>
#include <unordered_map>

using namespace std;





class LRU {
private:
    int capacity{};
    list<int> usageOrder;  // Doubly linked list to maintain order of usage
    std::unordered_map<int, std::list<int>::iterator> values;
public:
    explicit LRU(int n);
    void access(int num);
    int remove_least_recently_used();
    void remove_specific(int target);

};

class Memory {
private:
    int cacheSize;          // Total size of the cache (in bytes)
    int blockSize;          // Size of each cache block (in bytes)
    int ways;
    int setSize;
    // int tagSize;

    // int numLines{};           // Number of cache lines
    // int numBlocks{};          // Number of blocks in the cache
    // int cacheHits{};          // Number of cache hits
    // int cacheMisses{};        // Number of cache misses
    // int* cache;      // Simulated cache (simple vector for storing blocks)
    // int** tagDirectory;   // Pointer to hold the dynamically allocated array tagDirectory;
    std::vector<std::vector<int>> tagDirectory;
    std::vector<LRU> _LRU;



public:
    Memory(int cacheSize, int BlockSize, int Ways);
    bool find(int tag, int set);
    bool load_data(int tag, int set);
    void invalidate_data(int tag, int set);
    int extractSet(const std::string &addressStr);
    int extractTag(const std::string &addressStr);
    void execute_LRU(int tag, int set);

};

class Memory_Manager {
private:
    Memory l1_cache;
    Memory l2_cache;
    int l1_time;
    int l2_time;
    int memory_time;
    int writing_policy;

public:
    Memory_Manager(int l1_cache_size, int l2_cache_size, int l1_block_size, int l2_block_size,
        int l1_way, int l2_way, int l1_time, int l2_time, int memory_time, int writing_policy);
    void find(const std::string &addressStr);
    int extractSet(const std::string &addressStr);
    int extractTag(const std::string &addressStr);

};
#endif //MEMORY_H
