#include "lru.h"
#include <vector>
#include <unordered_map>
#include <cmath>
#include <cstring>

using namespace std;


//must fetch on miss (write)!

class setAssociativeCache{//little-endian
    private: 
        vector<LRUCache> sets; 
        int numSets;
        int bitsOffsets =4;
        int way; 

        unordered_map<uint32_t, uint8_t> RAM; //address, data
    
    public:
        
        setAssociativeCache(int numSets, int setSize): numSets(numSets), way(setSize){
            for(int i=0; i<numSets; i++){
                sets.push_back(LRUCache(setSize));
            }

            for (uint32_t addr = 0; addr < 0x20000; addr ++) {  // Every byte; byte addresssable
                RAM[addr] = rand() % 256;  // Random "garbage" 2^8 = 256 0-255
            }
        }
        int bitSets = log2(numSets);
        int set(uint32_t address){
            int setIdx = (address >>bitsOffsets) & (numSets -1);
            return setIdx;
        }

        int offset(uint32_t address){
            int offsetValue = address & ((1<<bitsOffsets)-1);
            return offsetValue;

        }

        int tag(uint32_t address){
            
            int tagValue  = (address) >>(bitsOffsets + bitSets);
            return tagValue; 
        }

        bool contains(uint32_t address){
            int setIdx = set(address); 
            int tagValue = tag(address);
            

            return sets[setIdx].contains(tagValue); 
        }

        void ramWrite(uint32_t address, uint32_t data){
            if (RAM.find(address) == RAM.end()){
                cout<<"RAM Miss; Allocating new address "<<address<<"\n";
            }

            for(int i=0; i<4; i++){
                RAM[address + i] = (data >> (i * 8)) & 0xFF;
            }

            cout<<"Wrote data "<<data<<" to RAM address "<<address<<" - "<<address+3<<"\n";
        }
      

        std::vector<uint8_t> ramRead(uint32_t address){
            uint32_t blockStart = address & ~0x3F;  // Faster than modulo
            std::vector<uint8_t> blockData(64);  // Pre-allocate size
    
            for (int i = 0; i < 64; i++) {
                uint32_t currentAddr = blockStart + i;
                if (RAM.find(currentAddr) != RAM.end()) {
                    blockData[i] = RAM[currentAddr]; 
                } else {
                    blockData[i] = 0;
                }
            }
            return blockData;
        }

        

        void evictIfNeeded(int set){
            if(sets[set].isFull()){
                CacheLine victim = sets[set].eviction();

                uint32_t victimAddress = ((victim.tag)<<(bitsOffsets + bitSets)) | (set << bitsOffsets);
            
                //now I need to check the ram, BUT I ONLY WRITEBACK IF DIRTY
                if(victim.dirty){//evict dirty
                    for (int i = 0; i < 64; i++) {
                        RAM[victimAddress + i] = victim.data[i];
                    }
                    cout << "Evicting dirty line, wrote 64 bytes to RAM\n";                    
                }            
            }
            
        }
        
        void write (uint32_t address, uint32_t data){
            int tagValue = tag(address); 
            int setIdx = set(address);
            int offsetId = offset(address);
            // first start with L1 

            //case 1 -> hit in L1 & eviction needed & evict dirty
            //case 2 -> hit in L1 & eviction not needed 
            if (address % 4 != 0) {
                cout << "ERROR: Unaligned access! Address " << hex << address << " is not a multiple of 4.\n";
                return; // Stop. Do not pass Go.
            
             }
            if(sets[setIdx].contains(tagValue)){
                CacheLine* line = sets[setIdx].get(tagValue);
                
                //unpacketize
                for(int i =0; i<4; i++){
                    line->data[i+offsetId] =  (data >> (i * 8)) & 0xFF; //modifies in place
                }
                line-> dirty = true; 
                cout<<"L1 HIT: Wrote 64 bytes data "<<" to address "<<address<<"\n";
                }else{//miss in L1

                    std::vector<uint8_t> curr_data= ramRead(address);

                    evictIfNeeded(setIdx);

                    for(int i =0; i<4; i++){
                        curr_data[i+offsetId] = (data >> (i * 8)) & 0xFF;
                    }
                    
                    sets[setIdx].put(tagValue, curr_data.data(), true); //now write the new data and mark dirty
                    cout<<"Wrote new data "<<" to address "<<address<<"\n";
                }
        }

        int read (uint32_t address){

            int tagValue = tag(address);
            int setIdx = set(address);
            int offsetId = offset(address);
            
            //case 1 -> hit in L1
            if(sets[setIdx].contains(tagValue)){
                CacheLine* line = sets[setIdx].get(tagValue);
                int data=0; 

                for(int i=0; i<4; i++){
                    data |= (line->data[offsetId+i]<<i*8);
                }
                
                cout<<"L1 HIT: Read data "<<data<<" from address "<<address<<"\n";
                return data;
            }else{
                std::vector<uint8_t> fetchedData = ramRead(address);
                cout<<"L1 MISS: Fetched data from RAM address "<<address<<"\n";
                evictIfNeeded(setIdx);
                sets[setIdx].put(tagValue, fetchedData.data(), false); //mark clean -> locality 

                CacheLine* line = sets[setIdx].get(tagValue);
                int data=0; 

                for(int i=0; i<4; i++){
                    data |= (line->data[offsetId+i]<<i*8);
                }
                return data;
            }
        }




};

int main() {
    cout << "========================================\n";
    cout << "TEST 1: Basic Write and Read\n";
    cout << "========================================\n";
    setAssociativeCache cache(8, 2);  // 8 sets, 2-way
    
    cache.write(0x100, 42);
    cache.read(0x100);  // Should hit
    
    cout << "\n========================================\n";
    cout << "TEST 2: Write Miss (Fetch from RAM)\n";
    cout << "========================================\n";
    
    cache.write(0x200, 99);  // Miss, fetch garbage from RAM, overwrite
    cache.read(0x200);       // Should hit with 99
    
    cout << "\n========================================\n";
    cout << "TEST 3: Eviction (Fill Set, Force Evict)\n";
    cout << "========================================\n";
    
    // These all map to same set (set 0 if addresses are 0x_0_)
    cache.write(0x100, 111);  // Set 0, way 0
    cache.write(0x900, 222);  // Set 0, way 1 (different tag)
    cache.write(0x1100, 333); // Set 0 - should evict 0x100 (LRU)
    
    cache.read(0x100);   // Should miss (evicted)
    cache.read(0x900);   // Should hit
    cache.read(0x1100);  // Should hit
    
    cout << "\n========================================\n";
    cout << "TEST 4: Dirty Eviction (Writeback)\n";
    cout << "========================================\n";
    
    cache.write(0x2000, 777);  // Write (dirty)
    cache.write(0xA000, 888);  // Same set, different tag
    cache.write(0x12000, 999); // Forces eviction of 0x2000 (should writeback)
    
    // Verify writeback happened by reading from "RAM" via miss
    // (In real test, you'd expose RAM or check output logs)
    
    cout << "\n========================================\n";
    cout << "TEST 5: Read Miss (Allocate Clean)\n";
    cout << "========================================\n";
    
    int data = cache.read(0x3000);  // Miss, fetch from RAM, allocate clean
    cout << "Read data: " << data << " (should be garbage from RAM)\n";
    cache.read(0x3000);  // Should hit now
    
    cout << "\n========================================\n";
    cout << "TEST 6: Temporal Locality\n";
    cout << "========================================\n";
    
    cache.write(0x4000, 100);
    cache.write(0x4004, 200);
    cache.write(0x4008, 300);
    
    // Access in reverse order (tests LRU)
    cache.read(0x4008);  // Most recent
    cache.read(0x4004);  
    cache.read(0x4000);  // Least recent
    
    // Write to same set, should evict 0x4008 (oldest after reordering)
    cache.write(0xC000, 400);
    cache.write(0x14000, 500);
    
    cout << "\n========================================\n";
    cout << "TESTS COMPLETE\n";
    cout << "========================================\n";
    
    return 0;
}