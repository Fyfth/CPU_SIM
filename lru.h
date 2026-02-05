#ifndef LRU_H
#define LRU_H


//word granularity -> real cache offset 

#include <list> 
#include <unordered_map> 
#include <iostream>
#include <list>
#include <cstring>
struct CacheLine{
    uint32_t tag; 
    uint8_t data[64]; //64 bytes per line 
    bool dirty; 

    CacheLine(uint32_t t = 0, uint8_t* d = nullptr, bool dir = false) : tag(t), dirty(dir) {
        if (d != nullptr) {
            memcpy(data, d, 64);
        } else {
            memset(data, 0, 64);
        }
    }
};


class LRUCache{
    private: 
    std::list<CacheLine> items; 
    std::unordered_map<uint32_t, std::list<CacheLine>::iterator> cacheMap; 
    int capacity;
    //void printCache();

    public: 
    LRUCache(int cap);           // Just declare, don't implement here
    CacheLine* get(uint32_t key);
    void put(uint32_t key, uint8_t value[64], bool dirty = false);
    bool contains(uint32_t key);
    int size();
    bool isFull();
    CacheLine eviction();
};
#endif
