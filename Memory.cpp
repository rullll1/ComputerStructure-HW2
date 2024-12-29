//
// Created by rulll on 26/12/2024.
//

#include "Memory.h"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <cstdint>

LRU::LRU(int n) {
	this->capacity = n;
}
void LRU::access(int value) {
	// If the number is already in the values, move it to the tail (most recent)
	if (values.find(value) != values.end()) {
		usageOrder.erase(values[value]);  // Remove the old position
	}
        
	// Add the number to the end of the list (most recently used)
	usageOrder.push_back(value);
	values[value] = --usageOrder.end();  // Update the values with the new position
}
int LRU::remove_least_recently_used() {
	int lru = usageOrder.front();  // Least recently used element
	usageOrder.pop_front();  // Remove it from the list
	values.erase(lru);  // Also remove it from the values
	return lru;
}
void LRU::remove_specific(int target) {
	usageOrder.erase(values[target]);  // Remove the old position
}

Memory::Memory(int cacheSize, int BlockSize, int ways){
	this->cacheSize = cacheSize; //pow(2,cacheSize);
	this->blockSize = BlockSize; //pow(2, BlockSize);
	this ->setSize = cacheSize - BlockSize - ways;
	this->ways = pow(2, ways);
	this->tagDirectory = std::vector<std::vector<int>>(this->ways,
							   std::vector<int>(pow(2,this->cacheSize), -1)); // init a matrix of -1

	// this->cache = new int[pow(2, this->setSize)];  // Dynamically allocate the array


}

int Memory::extractSet(const std::string &addressStr) {
	// Convert the string to an integer (assuming a hexadecimal format)
	int set;
	uint32_t address;
	std::stringstream ss;
	ss << std::hex << addressStr;
	ss >> address;

	// Extract the set index (middle 11 bits)
	uint32_t setMask = (1 << this->setSize) - 1;  // Mask to extract index bits
	set = (address >> this->blockSize) & setMask;
	return set;
}

int Memory::extractTag(const std::string &addressStr) {
	int tag;
	uint32_t address;
	std::stringstream ss;
	ss << std::hex << addressStr;
	ss >> address;
	tag = address >> (this->setSize + this->blockSize);
	return tag;
}

void Memory::execute_LRU(int tag, int set) {

	int way = this->_LRU[set].remove_least_recently_used();
	this->tagDirectory[way][set] = tag;
	this->_LRU[set].access(way);

}
bool Memory::find(int tag, int set) {
	for (int way=0; way < this->tagDirectory.size(); way++) {
		if (tagDirectory[way][set] == tag) { // we have a hit
			// do something
			this->_LRU[set].access(way);
			return true;
		}
	}
	return false;
}

bool Memory::load_data(int tag, int set) {
	bool loaded_data = false;
	bool ran_LRU = false;
	// bring data to one of the ways
	for (int way=0; way < this->tagDirectory.size(); way++) {
		if (tagDirectory[way][set] == -1) { // we found an empty set
			tagDirectory[way][set] = tag;
			this->_LRU[set].access(way);
			loaded_data = true;
			break;
		}
	}
	if (!loaded_data) { // all ways on the specific set are full - need to run LRU
		this->execute_LRU(tag, set);
		ran_LRU = true;
	}
	return ran_LRU; // notify that LRU was removed

}

void Memory::invalidate_data(int tag, int set) {
	// we will run this if we used lru in l2 and want to invalidate data in l1
	for (int way=0; way < this->tagDirectory.size(); way++) {
		if (tagDirectory[way][set] == tag) { // we found an empty set
			tagDirectory[way][set] = -1; // now its invalid
			this->_LRU[set].remove_specific(way);
			break;
		}
	}
}



Memory_Manager::Memory_Manager(int l1_cache_size, int l1_block_size, int l1_way,
						 int l2_cache_size, int l2_block_size, int l2_way, int l1_time,
						 int l2_time, int memory_time, int writing_policy)
	: l1_cache(l1_cache_size, l1_block_size, l1_way),  // Initialize l1_cache in the initializer list
	  l2_cache(l2_cache_size, l2_block_size, l2_way)   // Initialize l2_cache in the initializer list
{
	this->l1_time = l1_time;
	this->l2_time = l2_time;
	this->memory_time = memory_time;
	this->writing_policy = writing_policy;
}

void Memory_Manager::find(const std::string &addressStr) {
	int set1 = this->l1_cache.extractSet(addressStr);
	int set2 = this->l2_cache.extractSet(addressStr);
	int tag1 = this->l1_cache.extractTag(addressStr);
	int tag2 = this->l2_cache.extractTag(addressStr);

	bool l1_found = this->l1_cache.find(tag1, set1);
	bool l2_found = false;
	if (!l1_found) {
		l2_found = this->l2_cache.find(tag2, set2);
	}
	if (!l1_found and !l2_found) {
		// need to bring data from memory
		bool ran_lru = l2_cache.load_data(tag2, set2);
		if (ran_lru) { // could not load data - need to run LRU for l2
			l1_cache.invalidate_data(tag1, set1); // we keep the caches inclusive - remove the block from l1
		}
		l1_cache.load_data(tag1, set1);
	}

}



