#include "lru.h"
using namespace std;


LRUCache::LRUCache(int cap):capacity(cap){
    cout<<"Created LRU Cache with capacity "<<cap<<"\n\n";
}

CacheLine* LRUCache::get(uint32_t key){
    cout<<"Get("<<key<<"): ";

    //not found
    if(cacheMap.find(key)== cacheMap.end()){
        cout<<"MISS: not found in cache\n"; 
        return nullptr; 

    }

    //foundf
    auto it = cacheMap[key];// it is a pointer to cacheline ; so *it is cacheline; 
    //int value = it-> data; 

    cout<<"HIT (value = [DATABLOCK]) ";

    items.splice(items.begin(), items, it);
    

    return &(*it); //this really simplifies but it is an iterator not a pointer so need to deference first
}

void LRUCache::put(uint32_t key, uint8_t value[64], bool dirty){


    //update case
    cout<<"PUT([DATABLOCK]): "; 
    if (cacheMap.find(key)!=cacheMap.end()){
        auto it = cacheMap[key];
        items.splice(items.begin(), items, it);
        memcpy(it->data, value, 64);
        it->dirty = true;

        
        return; 
    }
    cout<<"new key, inserting"; 

    if (items.size() == capacity){
        //Eviction time -> someone in the set gotta go 
        cout << "WARNING: Internal eviction triggered in put()! Possible data loss if dirty.\n";
        uint32_t evictKey = items.back().tag; 
        
        bool evictDirty = items.back().dirty; 

        cout<<"-> Eviciting ("<<evictKey<<" )";

        //kill the node
        items.pop_back();
        //erase from map 
        cacheMap.erase(evictKey);

    }
    

    //general case add to front 

    cout<<"-> add items to front\n";

    items.push_front(CacheLine(key, value, dirty));

    //add to map 
    cacheMap[key]= items.begin();


    
    }

    bool LRUCache::contains(uint32_t key){
        return cacheMap.find(key) !=cacheMap.end();
    }
    int LRUCache::size(){
        return items.size();
    }
    bool LRUCache::isFull(){
        return items.size() == capacity;
    }

    CacheLine LRUCache::eviction(){
        if (items.empty()) {
            uint8_t data[64] = {0};
        return CacheLine(-1, data, false);
        }
        CacheLine victim = items.back();
        uint32_t key = victim.tag; 
        items.pop_back(); 
        cacheMap.erase(key); 

        return victim; 
    }
    void LRUCache::remove(int tag){
        auto mapIt = cacheMap.find(tag); 
        if(mapIt == cacheMap.end()) return; 
        auto it = mapIt->second; 
        cacheMap.erase(mapIt); 
        items.erase(it); 
    }
    
    CacheLine LRUCache::peek(){
        if (items.empty()) {
            uint8_t data[64] = {0};
        return CacheLine(-1, data, false);
        }
        CacheLine victim = items.back();
    
        return victim; 
    }




//     int main() {
//     cout << "========================================\n";
//     cout << "TEST 1: Basic Operations\n";
//     cout << "========================================\n";
//     LRUCache cache(3);
    
//     cache.put(1, 100);  // Cache: [1]
//     cache.put(2, 200);  // Cache: [2, 1]
//     cache.put(3, 300);  // Cache: [3, 2, 1]
    
//     cout << "\n--- Reading key 1 (should move to front) ---\n";
//     cache.get(1);       // Cache: [1, 3, 2]
    
//     cout << "\n--- Adding key 4 (should evict 2) ---\n";
//     cache.put(4, 400);  // Cache: [4, 1, 3], evicted 2
    
//     cout << "\n--- Trying to read evicted key 2 ---\n";
//     cache.get(2);       // Should be -1 (miss)
    
//     cout << "\n========================================\n";
//     cout << "TEST 2: Update Existing Key\n";
//     cout << "========================================\n";
    
//     cout << "--- Updating key 1 (should move to front with new value) ---\n";
//     cache.put(1, 999);  // Cache: [1, 4, 3], 1's value now 999
    
//     cache.get(1);       // Should return 999
    
//     cout << "\n========================================\n";
//     cout << "TEST 3: Eviction Order\n";
//     cout << "========================================\n";
//     LRUCache cache2(2);
    
//     cache2.put(1, 10);   // Cache: [1]
//     cache2.put(2, 20);   // Cache: [2, 1]
//     cache2.get(1);       // Cache: [1, 2] - 1 moved to front
//     cache2.put(3, 30);   // Cache: [3, 1] - 2 evicted (was oldest)
    
//     cout << "\n--- Should miss on 2, hit on 1 ---\n";
//     cache2.get(2);       // Miss
//     cache2.get(1);       // Hit
    
//     cout << "\n========================================\n";
//     cout << "TEST 4: Rapid Access Pattern\n";
//     cout << "========================================\n";
//     LRUCache cache3(3);
    
//     cache3.put(1, 1);
//     cache3.put(2, 2);
//     cache3.put(3, 3);
//     cache3.get(2);       // 2 is now most recent
//     cache3.get(3);       // 3 is now most recent
//     cache3.put(4, 4);    // 1 should be evicted (oldest)
    
//     cout << "\n--- Final verification ---\n";
//     cache3.get(1);       // Should miss
//     cache3.get(2);       // Should hit
//     cache3.get(3);       // Should hit
//     cache3.get(4);       // Should hit
    
//     return 0;
// }


    

