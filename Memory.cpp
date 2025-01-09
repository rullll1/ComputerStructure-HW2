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



LRU::LRU(int n) : capacity(n) {}

void LRU::access(int num) {
	auto it = cacheMap.find(num);
	if (it != cacheMap.end()) {
		lruList.erase(it->second);
	} else {
		if (lruList.size() >= capacity) {
			remove_least_recently_used();
		}
	}
	lruList.push_front(num);
	cacheMap[num] = lruList.begin();
}

int LRU::remove_least_recently_used() {
	if (!lruList.empty()) {
		int lruValue = lruList.back();
		lruList.pop_back();
		cacheMap.erase(lruValue);
		return lruValue;
	}
	return -1;
}

void LRU::remove_specific(int target) {
	auto it = cacheMap.find(target);
	if (it != cacheMap.end()) {
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
	this->cacheSize = cacheSize;
	this->blockSize = BlockSize;
	this ->setSize = cacheSize - BlockSize - ways;
	this->ways = pow(2, ways);
	this->tagDirectory = std::vector<std::vector<int>>(this->ways,
							   std::vector<int>(pow(2,this->setSize), -1));
	this->tagDirectoryDirty = std::vector<std::vector<int>>(this->ways,
							   std::vector<int>(pow(2,this->setSize), 0));

	for (int i=0; i < pow(2,this->setSize); i++) {
		this->_LRU.emplace_back(this->ways);
	}


}

int Memory::extractSet(const std::string &addressStr) {
	int set;
	uint32_t address;
	std::stringstream ss;
	ss << std::hex << addressStr;
	ss >> address;

	uint32_t setMask = (1 << this->setSize) - 1;  // Mask to extract index bits
	set = (address >> this->blockSize) & setMask;
	return set;
}

std::string Memory::constructAddress(int tag, int set) {
	uint32_t address = 0;

	address |= (tag << (this->setSize + this->blockSize));

	uint32_t setMask = (1 << this->setSize) - 1; // Mask to ensure set fits in `setSize` bits
	address |= ((set & setMask) << this->blockSize);

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
	for (int way=0; way < this->tagDirectory.size(); way++) {
        if (tagDirectory[way][set] == tag) {
          loaded_data = true;
          break;
        }
		if (tagDirectory[way][set] == -1) { // we found an empty set
			tagDirectory[way][set] = tag;
			this->_LRU[set].access(way);
			loaded_data = true;
			break;
		}
	}
	if (!loaded_data) {
		address_to_remove = this->execute_LRU(tag, set);
		ran_LRU = true;
	}
	return address_to_remove;
}

MemoryManager::MemoryManager(unsigned int l1_cache_size,
	unsigned int l2_cache_size, unsigned int l1_block_size, unsigned int l2_block_size,
		unsigned int l1_way, unsigned int l2_way, unsigned int l1_time, unsigned int l2_time,
		unsigned int memory_time, bool write_allocate)
	: l1_cache(l1_cache_size, l1_block_size, l1_way),
	  l2_cache(l2_cache_size, l2_block_size, l2_way)
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
    if (l2_found) {
    	std::string address_to_ingest = this->l1_cache.load_data(tag1, set1);
    	if (!address_to_ingest.empty()) {
    		bool is_l1_dirty = this->l1_cache.isDirty(tag1, set1);
    		if (is_l1_dirty) {
    			int set22 = this->l2_cache.extractSet(address_to_ingest);
    			int tag22 = this->l2_cache.extractTag(address_to_ingest);
    			this->l2_cache.load_data(tag22, set22);
    			this->l2_cache.markDirty(tag22, set22);
    			this->l1_cache.markClean(tag1, set1);
    		}
    	}
//    	this->l1_cache.markDirty(tag1, set1);
    }
	if (!l1_found and !l2_found) {
		// need to bring data from memory
		std::string address_to_remove = this->l2_cache.load_data(tag2, set2);
		if (!address_to_remove.empty()) { // could not load data - need to run LRU for l2
			bool is_l1_dirty = this->l1_cache.isDirty(tag1, set1);
			if (is_l1_dirty) {
				int set22 = this->l2_cache.extractSet(address_to_remove);
				int tag22 = this->l2_cache.extractTag(address_to_remove);
				this->l2_cache.load_data(tag22, set22);
				this->l1_cache.markClean(tag1, set1);
			}
		}
		std::string address_to_ingest = this->l1_cache.load_data(tag1, set1);
        if (!address_to_ingest.empty()) {
			bool is_l1_dirty = this->l1_cache.isDirty(tag1, set1);
			if (is_l1_dirty) {
				int set22 = this->l2_cache.extractSet(address_to_ingest);
				int tag22 = this->l2_cache.extractTag(address_to_ingest);
				this->l2_cache.load_data(tag22, set22);
				this->l1_cache.markClean(tag1, set1);
			}
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
    	if (this->write_allocate) {
    		std::string address_to_ingest = this->l1_cache.load_data(tag1, set1);
    		if (!address_to_ingest.empty()) {
    			bool is_l1_dirty = this->l1_cache.isDirty(tag1, set1);
    			if (is_l1_dirty) {
    				int set22 = this->l2_cache.extractSet(address_to_ingest);
    				int tag22 = this->l2_cache.extractTag(address_to_ingest);
    				this->l2_cache.load_data(tag22, set22);
                    this->l2_cache.markDirty(tag22, set22);
    				this->l1_cache.markClean(tag1, set1);
    			}
    		}
    		this->l1_cache.markDirty(tag1, set1);
    	}
    	return;
    }

	this->incrementL2Miss();
    if (this->write_allocate) {
    	std::string address_to_remove =  this->l2_cache.load_data(tag2, set2);
    	if (!address_to_remove.empty()) {
    		this->l2_cache.markClean(tag2, set2);
    		bool is_l1_dirty = this->l1_cache.isDirty(tag1, set1);
    		if (is_l1_dirty) {
    			int set22 = this->l2_cache.extractSet(address_to_remove);
    			int tag22 = this->l2_cache.extractTag(address_to_remove);
    			this->l2_cache.load_data(tag22, set22);
    			this->l2_cache.markDirty(tag22, set22);
    			this->l1_cache.markClean(tag1, set1);
    		}
    	}
    	std::string address_to_ingest =  this->l1_cache.load_data(tag1, set1);
    	if (!address_to_ingest.empty()) {
    		bool is_l1_dirty = this->l1_cache.isDirty(tag1, set1);
    		if (is_l1_dirty) {
    			this->l1_cache.markClean(tag1, set1);
    		}
    	}
        this->l1_cache.markDirty(tag1, set1);
    }
	this->updateAccessTime(this->memory_time);
    return;
}
