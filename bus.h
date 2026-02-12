#ifndef BUS_H 
#define BUS_H

#include <vector>
#include <unordered_map>
#include <iostream>
#include <cstdint>
#include <cmath>
#include "lru.h" 
#include "setAssociativeCache.cpp"

class setAssociativeCache;

class bus{
    private: 
        //this is a vector of setAssociativeCache pointers 
        std:vector<setAssociativeCache*> listeners; //all L3's
    public: 
        void addListener(setAssociativeCache* c);
        uint8_t* readBus(uint32_t address, int core_id);
        void writeInvalidate(uint32_t address, int core_id);
}; 



    
    
        


#endif