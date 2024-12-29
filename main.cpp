#include <iostream>
#include "Memory.h"
int main()
{
    std::cout << "Hello, World!" << std::endl;
    int set = 0;
    Memory l2 = Memory(6, 3, 0);
    Memory l1 = Memory(4, 3, 1);
    std::string address = "0x1231114";
    int set2 = l2.extractSet(address);
    int set1 = l1.extractSet(address);
    int tag1 = l1.extractTag(address);
    int tag2 = l2.extractTag(address);


    // std::cout << l1.extractSet("0x11111111");
    return 0;



}
