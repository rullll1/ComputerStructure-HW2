#include <cstdlib>
#include <iostream>
#include <fstream>
#include <sstream>
#include <list>
#include <unordered_map>

using std::FILE;
using std::string;
using std::cout;
using std::endl;
using std::cerr;
using std::ifstream;
using std::stringstream;
using std::list;
using std::unordered_map;

class Tag
{
public:
	string tag;
	bool dirty;
	Tag(string t, bool d) : tag(t), dirty(d) {}
};

// Functor to compare Tag objects based on the tag string
class TagComparator {
    string tag;
public:
    TagComparator(const string& t) : tag(t) {}
    bool operator()(const Tag& t) const {
        return t.tag == tag;
    }
};

class Set {
public:
    std::string set_id; // String identifier (binary representation)
    int num_ways; // Number of ways
    std::list<Tag> lru_queue; // LRU queue
	bool in_l1;
	int block_size;

    Set(const std::string& id, int ways, bool in_l1, int block_size) : set_id(id), num_ways(ways), in_l1(in_l1), block_size(block_size) {}

    // Check if the set is full
    bool is_full() const {
        return lru_queue.size() >= num_ways;
    }

    // Access the set (check for a hit)
    bool access(string tag, bool write) {
        auto it = std::find_if(lru_queue.begin(), lru_queue.end(), TagComparator(tag));
        if (it != lru_queue.end()) {
            // Move to front (recently used)
			int prev_write = it->dirty;
			write = write || prev_write;
            
			lru_queue.erase(it);
            lru_queue.push_front(Tag(tag, write));
            return true; // Hit
        }
        return false; // Miss
    }

    // Add a tag to the set
    string add(string tag, bool write) {
		string to_return = "";
        if (is_full()) {
			Tag tag_to_evict = lru_queue.back();
			if (tag_to_evict.dirty && in_l1) {
				string tag_to_evict_as_string = tag_to_evict.tag;
				string address_to_evacuate = tag_to_evict_as_string + set_id + string(std::ceil(std::log2(block_size)), '0');
				to_return = address_to_evacuate;
			}
            lru_queue.pop_back(); // Evict LRU
        }
        lru_queue.push_front(Tag(tag, write));
		return to_return;
    }

	// // Print all elements in the set
    // void print_elements() const {
    //     cout << " Elements: ";
    //     for (const auto& tag : lru_queue) {
    //         cout << tag.tag << " ";
    //     }
    //     cout << endl;
    // }

    // Remove a tag from the set
    void remove_tag(const string& tag) {
        auto it = std::find_if(lru_queue.begin(), lru_queue.end(), TagComparator(tag));
        if (it != lru_queue.end()) {
            lru_queue.erase(it);
        }
    }
};

class Cache {
public:
    unordered_map<std::string, Set> l1_cache; // Map of binary set IDs to sets
    unordered_map<std::string, Set> l2_cache;
	int num_hits_l1 = 0;
    int num_misses_l1 = 0;
	int num_hits_l2 = 0;
	int num_misses_l2 = 0;
	int total_access_time = 0;
    int block_size;
    int num_sets_l1;
	int num_sets_l2;
	bool write_allocate;
	int l1_cyc;
	int l2_cyc;
	int memory_cyc;

    // Constructor computes num_sets internally and assigns binary string IDs
    Cache(int l1_s, int l2_s, int n_ways_l1, int n_ways_l2, int l1_cyc, int l2_cyc, int memory_cyc, int b_size, bool write_allocate)
        : l1_cyc(l1_cyc), l2_cyc(l2_cyc), memory_cyc(memory_cyc), write_allocate(write_allocate) {
        
		block_size = pow(2, b_size);
		int num_ways_l1 = pow(2, n_ways_l1);
		int num_ways_l2 = pow(2, n_ways_l2);
		int l1_size = pow(2, l1_s);
		int l2_size = pow(2, l2_s);

		num_sets_l1 = compute_num_sets(l1_size, num_ways_l1, block_size);
		num_sets_l2 = compute_num_sets(l2_size, num_ways_l2, block_size);
		
		int num_bits_l1 = std::ceil(std::log2(num_sets_l1)); // Number of bits needed for binary IDs
		int num_bits_l2 = std::ceil(std::log2(num_sets_l2)); // Number of bits needed for binary IDs

		for (int i = 0; i < num_sets_l1; ++i) {
			std::string binary_id = to_binary_string(i, num_bits_l1);
			l1_cache.insert(std::make_pair(binary_id, Set(binary_id, num_ways_l1, true, block_size)));
		}

        for (int i = 0; i < num_sets_l2; ++i) {
            std::string binary_id = to_binary_string(i, num_bits_l2);
			l2_cache.insert(std::make_pair(binary_id, Set(binary_id, num_ways_l2, false, block_size)));
		}
    }

    // Compute the number of sets
    static int compute_num_sets(int cache_size, int num_ways, int block_size) {
        if (block_size == 0 || num_ways == 0) {
            throw std::invalid_argument("Block size and number of ways must be greater than zero.");
        }
        int num_blocks = cache_size / block_size;
        return num_blocks / num_ways;
    }

    // Convert an integer to a binary string of specified length
    static std::string to_binary_string(int value, int length) {
        std::string binary = "";
        while (value > 0) {
            binary = (value % 2 == 0 ? "0" : "1") + binary;
            value /= 2;
        }
        while (binary.length() < length) {
            binary = "0" + binary;
        }
        return binary;
    }
	
	void calc_set_and_tag(const std::string& address, int num_sets, string& set_id, string& tag) {
		int block_offset_bits = std::ceil(std::log2(block_size));
		int set_bits = std::ceil(std::log2(num_sets));
		int tag_bits = address.length() - (block_offset_bits + set_bits);

		// Extract tag and set ID
		tag = address.substr(0, tag_bits);
		set_id = address.substr(tag_bits, set_bits);
	}

	void l1_cache_evacuate(const std::string& address) {
		string set_id;
		string tag;
		calc_set_and_tag(address, num_sets_l1, set_id, tag);
		auto& l1_set = l1_cache.at(set_id);
		l1_set.remove_tag(tag);
	}

	bool l1_read_access(const std::string& address, bool dirty = false) {
		string set_id;
		string tag;
		calc_set_and_tag(address, num_sets_l1, set_id, tag);
        auto& l1_set = l1_cache.at(set_id);
        if (l1_set.access(tag, dirty)) {
            num_hits_l1++;
			total_access_time += l1_cyc;
            return true; // Hit
        } else if (l2_read_access(address)) {
			num_misses_l1++;
			total_access_time += l1_cyc + l2_cyc;
			string address_to_evacuate = l1_set.add(tag, dirty);
			if (address_to_evacuate != "") {
				l2_write_access(address_to_evacuate, false);
			}
			return false; // Miss
		} else {
			num_misses_l1++;
			total_access_time += l1_cyc + l2_cyc + memory_cyc;
			string address_to_evacuate = l1_set.add(tag, dirty); //L2 already added it in l2_read_access.
			if (address_to_evacuate != "") {
				l2_write_access(address_to_evacuate, false);
			}
			return false; // Miss
		}
    }

	bool l2_read_access(const std::string& address) {
		// TODOO - If we evacuate a line from L2 cache, need to check if it exists in L1 and if so evacuate it from L1
		string set_id;
		string tag;
		calc_set_and_tag(address, num_sets_l2, set_id, tag);
        auto& l2_set = l2_cache.at(set_id);

        if (l2_set.access(tag, false)) {
            num_hits_l2++;
            return true; // Hit
        } else {
            num_misses_l2++;
			if(l2_set.is_full()) {
				// Evacuate the line from L1 cache before removing from L2 cache
				string l2_tag_to_evacuate = l2_set.lru_queue.back().tag;
				string address_to_evacuate = l2_tag_to_evacuate + set_id + string(std::ceil(std::log2(block_size)), '0');
				l1_cache_evacuate(address_to_evacuate);
			}
            l2_set.add(tag, false);
            return false; // Miss
        }
    }

	bool l1_write_access(const std::string& address) {
		if (write_allocate)
		{
			return l1_read_access(address, true);
		}
		else
		{
			string set_id;
			string tag;
			calc_set_and_tag(address, num_sets_l1, set_id, tag);
			auto& l1_set = l1_cache.at(set_id);
			if (l1_set.access(tag, true)) {
				num_hits_l1++;
				total_access_time += l1_cyc;
				return true; // Hit
			}
			else {
				num_misses_l1++;
				if (l2_write_access(address)) {
					total_access_time += l1_cyc + l2_cyc;
					return false; // Miss
				}
				else {
					total_access_time += l1_cyc + l2_cyc + memory_cyc;
					return false; // Miss
				}
			}
		}
	}

	bool l2_write_access(const std::string& address, bool count_hit_miss = true, bool is_write_back=false) {
		string set_id;
		string tag;
		calc_set_and_tag(address, num_sets_l2, set_id, tag);
		auto& l2_set = l2_cache.at(set_id);
		if (l2_set.access(tag, true)) {
			num_hits_l2 += count_hit_miss;
			return true; // Hit
		}
		else {
			num_misses_l2 += count_hit_miss;
			return false; // Miss
		}
	}

	double calc_average_access_time() {
		int num_accesses = num_hits_l1 + num_misses_l1;
		return (double) total_access_time / num_accesses;
	}

	double calc_l1_miss_rate() {
		int num_accesses = num_hits_l1 + num_misses_l1;
		return (double) num_misses_l1 / num_accesses;
	}

	double calc_l2_miss_rate() {
		int num_accesses = num_hits_l2 + num_misses_l2;
		return (double) num_misses_l2 / num_accesses;
	}
};

// Function to convert a hexadecimal address to a binary string
string hex_to_binary(const string& hex_address) {
    unsigned long num = strtoul(hex_address.c_str(), nullptr, 16); // Convert hex to decimal
    string binary = std::bitset<32>(num).to_string();             // Convert decimal to 32-bit binary string
    return binary;
}

int main(int argc, char **argv) {

	if (argc < 19) {
		cerr << "Not enough arguments" << endl;
		return 0;
	}

	// Get input arguments

	// File
	// Assuming it is the first argument
	char* fileString = argv[1];
	ifstream file(fileString); //input file stream
	string line;
	if (!file || !file.good()) {
		// File doesn't exist or some other error
		cerr << "File not found" << endl;
		return 0;
	}

	unsigned MemCyc = 0, BSize = 0, L1Size = 0, L2Size = 0, L1Assoc = 0,
			L2Assoc = 0, L1Cyc = 0, L2Cyc = 0, WrAlloc = 0;

	for (int i = 2; i < 19; i += 2) {
		string s(argv[i]);
		if (s == "--mem-cyc") {
			MemCyc = atoi(argv[i + 1]);
		} else if (s == "--bsize") {
			BSize = atoi(argv[i + 1]);
		} else if (s == "--l1-size") {
			L1Size = atoi(argv[i + 1]);
		} else if (s == "--l2-size") {
			L2Size = atoi(argv[i + 1]);
		} else if (s == "--l1-cyc") {
			L1Cyc = atoi(argv[i + 1]);
		} else if (s == "--l2-cyc") {
			L2Cyc = atoi(argv[i + 1]);
		} else if (s == "--l1-assoc") {
			L1Assoc = atoi(argv[i + 1]);
		} else if (s == "--l2-assoc") {
			L2Assoc = atoi(argv[i + 1]);
		} else if (s == "--wr-alloc") {
			WrAlloc = atoi(argv[i + 1]);
		} else {
			cerr << "Error in arguments" << endl;
			return 0;
		}
	}

	Cache cache = Cache(L1Size, L2Size, L1Assoc, L2Assoc, L1Cyc, L2Cyc, MemCyc, BSize, WrAlloc);

	while (getline(file, line)) {

		stringstream ss(line);
		string address;
		char operation = 0; // read (R) or write (W)
		if (!(ss >> operation >> address)) {
			// Operation appears in an Invalid format
			cout << "Command Format error" << endl;
			return 0;
		}

		string cutAddress = address.substr(2); // Removing the "0x" part of the address

		unsigned long int num = 0;
		string address_binary = hex_to_binary(cutAddress);
		
		if (operation == 'r') {
			cache.l1_read_access(address_binary);
		} else if (operation == 'w') {
			cache.l1_write_access(address_binary);
		} else {
			cerr << "Invalid operation" << endl;
			return 0;
		}
	}

	double L1MissRate = cache.calc_l1_miss_rate();
	double L2MissRate = cache.calc_l2_miss_rate();
	double avgAccTime = cache.calc_average_access_time();

	printf("L1miss=%.03f ", L1MissRate);
	printf("L2miss=%.03f ", L2MissRate);
	printf("AccTimeAvg=%.03f\n", avgAccTime);

	return 0;
}
