#include "bus.h"
#include "setAssociativeCache.h"
#include "lru.h"

#include <list> 
#include <unordered_map> 
#include <iostream>
#include <list>
#include <cstring>

void bus::addListener(setAssociativeCache* c){
    listeners.push_back(setAssociativeCache); //object exists -> pass by reference 
}

uint8_t* bus::readBus(uint32_t address, int core_id){
    //core_id sents request to all other L3, all L3 call sets[set].contains 
}