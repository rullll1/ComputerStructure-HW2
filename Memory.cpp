//
// Created by rulll on 26/12/2024.
//

#include "Memory.h"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <cstdint>
#include <iomanip>
#include <iostream>
//
// LRU::LRU(int n) {
// 	this->capacity = n;
// }
// void LRU::access(int value) {
// 	// If the number is already in the values, move it to the tail (most recent)
// 	if (values.find(value) != values.end()) {
// 		usageOrder.erase(values[value]);  // Remove the old position
// 	}
//
// 	// Add the number to the end of the list (most recently used)
// 	usageOrder.push_back(value);
// 	values[value] = --usageOrder.end();  // Update the values with the new position
// }
// int LRU::remove_least_recently_used() {
// 	int lru = usageOrder.front();  // Least recently used element
// 	usageOrder.pop_front();  // Remove it from the list
// 	values.erase(lru);  // Also remove it from the values
// 	return lru;
// }
// void LRU::remove_specific(int target) {
// 	usageOrder.erase(values[target]);  // Remove the old position
// }

LRU::LRU(int n) : capacity(n) {}

// Function to access a specific value (mark it as most recently used)
void LRU::access(int num) {
	auto it = cacheMap.find(num);
	if (it != cacheMap.end()) {
		// If the number exists in the cache, move it to the front (most recently used)
		lruList.erase(it->second);
	} else { // TODO: remove else
		// If the cache is full, remove the least recently used element
		if (lruList.size() >= capacity) {
			remove_least_recently_used();
		}
	}
	// Insert the value at the front of the list
	lruList.push_front(num);
	cacheMap[num] = lruList.begin();
}

// Function to remove the least recently used element
int LRU::remove_least_recently_used() {
	if (!lruList.empty()) {
		int lruValue = lruList.back(); // Get the least recently used value (back of the list)
		lruList.pop_back(); // Remove it from the list
		cacheMap.erase(lruValue); // Remove it from the map
		return lruValue; // Return the removed value
	}
	return -1; // Return -1 if the cache is empty
}

// Function to remove a specific value from the cache
void LRU::remove_specific(int target) {
	auto it = cacheMap.find(target);
	if (it != cacheMap.end()) {
		// If the target exists, remove it from the list and map
		lruList.erase(it->second);
		cacheMap.erase(it);
	}
}

// Helper function to print the contents of the cache (for debugging)
void LRU::printCache() const {
	std::cout << "Cache contents (most recent to least): ";
	for (const int &val : lruList) {
		std::cout << val << " ";
	}
	std::cout << std::endl;
}
Memory::Memory(unsigned int cacheSize, unsigned int BlockSize, unsigned int ways){
	this->cacheSize = cacheSize; //pow(2,cacheSize);
	this->blockSize = BlockSize; //pow(2, BlockSize);
	this ->setSize = cacheSize - BlockSize - ways;
	this->ways = pow(2, ways);
	this->tagDirectory = std::vector<std::vector<int>>(this->ways,
							   std::vector<int>(pow(2,this->setSize), -1)); // init a matrix of -1
	this->tagDirectoryDirty = std::vector<std::vector<int>>(this->ways,
							   std::vector<int>(pow(2,this->setSize), 0)); // init a matrix of 0

	for (int i=0; i < pow(2,this->setSize); i++) {
		this->_LRU.emplace_back(this->ways); // Construct each LRU with its capacity
	}
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

std::string Memory::constructAddress(int tag, int set) {
	uint32_t address = 0;

	// Reconstruct the tag part
	address |= (tag << (this->setSize + this->blockSize));

	// Reconstruct the set part
	uint32_t setMask = (1 << this->setSize) - 1; // Mask to ensure set fits in `setSize` bits
	address |= ((set & setMask) << this->blockSize);

	// Convert the reconstructed address to a hexadecimal string with "0x" prefix
	std::stringstream ss;
	ss << "0x" << std::hex << std::setw(8) << std::setfill('0') << address;
	return ss.str();
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

std::string Memory::execute_LRU(int tag, int set) {
	int way = this->_LRU[set].remove_least_recently_used();
	int old_tag = this->tagDirectory[way][set];
	std::string old_address = this->constructAddress(old_tag, set);
	this->tagDirectory[way][set] = tag;
	this->_LRU[set].access(way);
	return old_address;

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

void Memory::markDirty(int tag, int set) {
	for (int way=0; way < this->tagDirectory.size(); way++) {
		if (tagDirectory[way][set] == tag) {
			tagDirectoryDirty[way][set] = DIRTY;
		}
	}
}

void Memory::markClean(int tag, int set) {
	for (int way=0; way < this->tagDirectory.size(); way++) {
		if (tagDirectory[way][set] == tag) {
			tagDirectoryDirty[way][set] = 0;
		}
	}
}

bool Memory::isDirty(int tag, int set) {
	for (int way=0; way < this->tagDirectory.size(); way++) {
		if (tagDirectory[way][set] == tag) {
			return tagDirectoryDirty[way][set] == DIRTY;
		}
	}
    return false;
}

std::string Memory::load_data(int tag, int set) {
	bool loaded_data = false;
	bool ran_LRU = false;
	std::string address_to_remove;
	// bring data to one of the ways
	for (int way=0; way < this->tagDirectory.size(); way++) {
		if (tagDirectory[way][set] == -1) { // we found an empty set
			tagDirectory[way][set] = tag; // TODO: remove
			this->_LRU[set].access(way);
			loaded_data = true;
			break;
		}
	}
	if (!loaded_data) { // all ways on the specific set are full - need to run LRU
		address_to_remove = this->execute_LRU(tag, set);
		ran_LRU = true;
	}
	return address_to_remove; // notify that LRU was removed
}

void Memory::invalidate_data(std::string& address) {
	int set = this->extractSet(address);
	int tag = this->extractTag(address);
	// we will run this if we used lru in l2 and want to invalidate data in l1
	for (int way=0; way < this->tagDirectory.size(); way++) {
		if (tagDirectory[way][set] == tag) { // we found an empty set
			tagDirectory[way][set] = -1; // now its invalid
			this->_LRU[set].remove_specific(way);
			break;
		}
	}
}

MemoryManager::MemoryManager(unsigned int l1_cache_size,
	unsigned int l2_cache_size, unsigned int l1_block_size, unsigned int l2_block_size,
		unsigned int l1_way, unsigned int l2_way, unsigned int l1_time, unsigned int l2_time,
		unsigned int memory_time, bool write_allocate)
	: l1_cache(l1_cache_size, l1_block_size, l1_way),  // Initialize l1_cache in the initializer list
	  l2_cache(l2_cache_size, l2_block_size, l2_way)   // Initialize l2_cache in the initializer list
{
	this->l1_time = l1_time;
	this->l2_time = l2_time;
	this->memory_time = memory_time;
	this->write_allocate = write_allocate;
	this->numL1Miss = 0;
	this->numL2Miss = 0;
	this->numOperations = 0;
	this->totalAccessTime = 0;
	this->accessL2 = 0;
}

void MemoryManager::find(const std::string &addressStr) {
	this->incrementNumOperations();
	int set1 = this->l1_cache.extractSet(addressStr);
	int set2 = this->l2_cache.extractSet(addressStr);
	int tag1 = this->l1_cache.extractTag(addressStr);
	int tag2 = this->l2_cache.extractTag(addressStr);

	bool l1_found = this->l1_cache.find(tag1, set1);
	this->updateAccessTime(this->l1_time);
	bool l2_found = false;
	if (!l1_found) {
		this->incrementL1Miss();
		l2_found = this->l2_cache.find(tag2, set2);
		this->accessL2 ++;
		this->updateAccessTime(this->l2_time);
		if (!l2_found) {
			this->incrementL2Miss();
		}
	}
	if (!l1_found and !l2_found) {
		// need to bring data from memory
		std::string address_to_remove = this->l2_cache.load_data(tag2, set2);
		if (!address_to_remove.empty()) { // could not load data - need to run LRU for l2
			int set11 = this->l1_cache.extractSet(address_to_remove);
			int tag11 = this->l1_cache.extractTag(address_to_remove);
			bool is_l1_dirty = this->l1_cache.isDirty(tag11, set11);
			if (is_l1_dirty) {
				int set22 = this->l2_cache.extractSet(address_to_remove);
				int tag22 = this->l2_cache.extractTag(address_to_remove);
				this->l2_cache.load_data(tag2, set2);
				this->l1_cache.markClean(tag11, set11);
			}
			this->l1_cache.invalidate_data(address_to_remove); // we keep the caches inclusive - remove the block from l1
		}
		std::string address_to_ingest = this->l1_cache.load_data(tag1, set1);
		int set11 = this->l1_cache.extractSet(address_to_ingest);
		int tag11 = this->l1_cache.extractTag(address_to_ingest);
		bool is_l1_dirty = this->l1_cache.isDirty(tag1, set1);
		if (is_l1_dirty) {
			int set22 = this->l2_cache.extractSet(address_to_ingest);
			int tag22 = this->l2_cache.extractTag(address_to_ingest);
			this->l2_cache.load_data(tag22, set22);
			this->l1_cache.markClean(tag1, set1);
		}
		// 0x00000 -> L2
		this->updateAccessTime(this->memory_time);
	}
}

void MemoryManager::write(const std::string &addressStr) {
	this->incrementNumOperations();
	int set1 = this->l1_cache.extractSet(addressStr);
	int set2 = this->l2_cache.extractSet(addressStr);
	int tag1 = this->l1_cache.extractTag(addressStr);
	int tag2 = this->l2_cache.extractTag(addressStr);

	bool l1_found = this->l1_cache.find(tag1, set1);
	this->updateAccessTime(this->l1_time);
    if (l1_found) {
    	// Hit in L1: Written (dirty).
        this->l1_cache.markDirty(tag1, set1);
        return;
    }

    this->incrementL1Miss();
	this->accessL2 ++;
	bool l2_found = this->l2_cache.find(tag2, set2);
	this->updateAccessTime(this->l2_time);
    if (l2_found) {
    	// Hit in L2: Written (dirty).
    	this->l2_cache.markDirty(tag2, set2);
    	std::string address_to_remove = this->l1_cache.load_data(tag1, set1);
    	return;
    }

	this->incrementL2Miss();
    if (this->write_allocate) {
    	bool is_l1_dirty = this->l1_cache.isDirty(tag1, set1);
    	bool is_l2_dirty = this->l2_cache.isDirty(tag2, set2);

    	std::string address_to_remove =  this->l2_cache.load_data(tag2, set2);
    	if (!address_to_remove.empty()) {
    		this->l1_cache.invalidate_data(address_to_remove);
    		if (is_l2_dirty) {
    			this->updateAccessTime(this->memory_time);
    			this->l2_cache.markClean(tag2, set2);
    		}
    	}
    	std::string address_to_ingest =  this->l1_cache.load_data(tag1, set1);
    	if (!address_to_ingest.empty()) {
    		if (is_l1_dirty) {
    			this->updateAccessTime(this->l2_time);
    			this->l1_cache.markClean(tag1, set1);
    		}
    		this->l1_cache.invalidate_data(address_to_remove);
    	}
        this->l1_cache.markDirty(tag1, set1);
    }
	this->updateAccessTime(this->memory_time);
    return;
}
