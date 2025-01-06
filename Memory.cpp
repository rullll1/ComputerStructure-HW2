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

Memory::Memory(unsigned int cacheSize, unsigned int BlockSize, unsigned int ways){
	this->cacheSize = cacheSize; //pow(2,cacheSize);
	this->blockSize = BlockSize; //pow(2, BlockSize);
	this ->setSize = cacheSize - BlockSize - ways;
	this->ways = pow(2, ways);
	this->tagDirectory = std::vector<std::vector<int>>(this->ways,
							   std::vector<int>(pow(2,this->setSize), -1)); // init a matrix of -1
//	this->tagDirectoryDirty = std::vector<std::vector<int>>(this->ways,
//							   std::vector<int>(pow(2,this->setSize), 0)); // init a matrix of 0

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

//void Memory::markDirty(int tag, int set) {
//	for (int way=0; way < this->tagDirectory.size(); way++) {
//		if (tagDirectory[way][set] == tag) { // we have a hit
//			tagDirectoryDirty[way][set] = DIRTY;
//		}
//	}
//}

//void Memory::markClean(int tag, int set) {
//	for (int way=0; way < this->tagDirectory.size(); way++) {
//		if (tagDirectory[way][set] == tag) { // we have a hit
//			tagDirectoryDirty[way][set] = 0;
//		}
//	}
//}

//bool Memory::isDirty(int tag, int set) {
//	for (int way=0; way < this->tagDirectory.size(); way++) {
//        if (tagDirectory[way][set] == tag && tagDirectoryDirty[way][set] == DIRTY) {
//			return true;
//        }
//	}
//    return false;
//}

//int Memory::getWay(int tag, int set) {
//	for (int way=0; way < this->tagDirectory.size(); way++) {
//        if (tagDirectory[way][set] == tag) {
//			return way;
//        }
//	}
//    return -1;
//}

bool Memory::load_data(int tag, int set) {
	bool loaded_data = false;
	bool ran_LRU = false;
	// bring data to one of the ways
	for (int way=0; way < this->tagDirectory.size(); way++) {
		if (this->tagDirectory[way][set] == -1) { // we found an empty set
			this->tagDirectory[way][set] = tag;
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
		this->updateAccessTime(this->l2_time);
		if (!l2_found) {
			this->incrementL2Miss();
		}
	}
	if (!l1_found and !l2_found) {
		// need to bring data from memory
		bool ran_lru = this->l2_cache.load_data(tag2, set2);
		if (ran_lru) { // could not load data - need to run LRU for l2
//            int way2 = this->l2_cache.getWay(tag2, set2); // TODO: need the tag of the evacuated data.
//            if (this->l2_cache.isDirty(way2, set2)) {
//                this->updateAccessTime(this->memory_time); // update memory when l2 is dirty and evacuated
//            	this->l2_cache.markClean(way2, set2);
//            }
//            this->l2_cache.execute_LRU(tag2, set2);
            this->l1_cache.invalidate_data(tag1, set1); // we keep the caches inclusive - remove the block from l1
		}
		ran_lru = this->l1_cache.load_data(tag1, set1);
        if (ran_lru) { // could not load data - need to run LRU for l1
//            if (this->l1_cache.isDirty(tag1, set1)) {
//                this->updateAccessTime(this->l2_time); // update l2 when l1 is dirty and evacuated
//            	this->l1_cache.markClean(tag2, set2);
//            }
		}
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
    bool l2_found = false;

    // START WRITE-BACK IF HIT
    if (l1_found) {
//    	this->l1_cache.markDirty(tag1, set1);
    } else {
        this->incrementL1Miss();
    	l2_found = this->l2_cache.find(tag2, set2);
		this->updateAccessTime(this->l2_time);
        if (l2_found) {
//        	this->l2_cache.markDirty(tag1, set1);
        }
    }
    // END WRITE-BACK IF HIT

    if (!l1_found && !l2_found) {
        this->incrementL2Miss();
        this->updateAccessTime(this->memory_time);
    	if (this->write_allocate) {
        	//Block fetched from memory to L2, then to L1 (dirty).
			bool ran_lru = this->l2_cache.load_data(tag2, set2);
			if (ran_lru) {
				this->l1_cache.invalidate_data(tag1, set1);
//            	if (this->l2_cache.isDirty(tag2, set2)) {
//                	this->updateAccessTime(this->memory_time); // update memory when l2 is dirty and evacuated
//            		this->l2_cache.markClean(tag2, set2);
//            	}
            }
    	}
	}
}



