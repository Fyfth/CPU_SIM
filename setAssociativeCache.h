#ifndef SET_ASSOCIATIVE_CACHE_H 
#define SET_ASSOCIATIVE_CACHE_H

#include <vector>
#include <unordered_map>
#include <iostream>
#include <cstdint>
#include <cmath>
#include "lru.h" 


class setAssociativeCache {
    private: 
        std::vector<LRUCache> sets; 
        int numSets;
        int bitsOffsets = 6;
        int way;
        setAssociativeCache* nextLevel; 
        setAssociativeCache* prevLevel; 
        int bitSets; 

        
        static std::unordered_map<uint32_t, uint8_t> RAM; 
    
    public: 
        setAssociativeCache(int numSets, int setSize, setAssociativeCache* prevLevel, setAssociativeCache* nextLevel); 
        
        void setPrevLevel(setAssociativeCache* prev); 
        int set(uint32_t address); 
        int offset(uint32_t address); 
        int tag(uint32_t address); 
        bool contains(uint32_t address); 
        
        void ramWrite(uint32_t address, uint32_t data); 
        std::vector<uint8_t> ramRead(uint32_t address); 
        
        void evictIfNeeded(int setIdx); 
        void writeBlock(uint32_t address, uint8_t* dataptr); 
        void write(uint32_t address, uint32_t data); 
        uint8_t* readBlock(uint32_t address); 
        int read(uint32_t address); 
        void backInvalidate(uint32_t address); 
}; 

#endif