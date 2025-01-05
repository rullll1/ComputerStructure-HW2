//
// Created by rulll on 26/12/2024.
//

#ifndef MEMORY_H
#define MEMORY_H
#define WRITE_ALLOCATE 1
#define NO_WRITE_ALLOCATE 0
#include <list>
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
    unsigned int cacheSize;          // Total size of the cache (in bytes)
    unsigned int blockSize;          // Size of each cache block (in bytes)
    unsigned int ways;
    unsigned int setSize;
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
    Memory(unsigned int cacheSize, unsigned int BlockSize, unsigned int Ways);
    bool find(int tag, int set);
    bool load_data(int tag, int set);
    void invalidate_data(int tag, int set);
    int extractSet(const std::string &addressStr);
    int extractTag(const std::string &addressStr);
    void execute_LRU(int tag, int set);

};

class MemoryManager {
private:
    Memory l1_cache;
    Memory l2_cache;
    unsigned int l1_time;
    unsigned int l2_time;
    unsigned int memory_time;
    bool write_allocate;
    unsigned int numL1Miss;
    unsigned int numL2Miss;
    unsigned int totalAccessTime;
    unsigned int numOperations;
    void incrementL1Miss() { ++numL1Miss; }
    void incrementL2Miss() { ++numL2Miss; }
    void incrementNumOperations() { ++numOperations; }
    void updateAccessTime(const unsigned int newTime) { totalAccessTime += newTime; }

public:
    MemoryManager(unsigned int l1_cache_size, unsigned int l2_cache_size, unsigned int l1_block_size, unsigned int l2_block_size,
        unsigned int l1_way, unsigned int l2_way, unsigned int l1_time, unsigned int l2_time, unsigned int memory_time, bool write_allocate);
    void find(const std::string &addressStr);
    void write(const std::string &addressStr);
    double getL1MissRate() const { return static_cast<double>(numL1Miss) / static_cast<double>(numOperations); }
    double getL2MissRate() const { return static_cast<double>(numL2Miss) / static_cast<double>(numOperations); }
    double getAverageAccessTime() const {return static_cast<double>(totalAccessTime) / static_cast<double>(numOperations); }

};
#endif //MEMORY_H
