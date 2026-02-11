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
        setAssociativeCache* nextLevel; 
        setAssociativeCache* prevLevel; 

        static unordered_map<uint32_t, uint8_t> RAM; //address, data -> static bec 1 copy of RAM shared across L1,L2,L3, and all cores
    
    public:
        
        setAssociativeCache(int numSets, int setSize, setAssociativeCache* prevLevel, setAssociativeCache* nextLevel): numSets(numSets), way(setSize),prevLevel(prevLevel),nextLevel(nextLevel){
            for(int i=0; i<numSets; i++){
                sets.push_back(LRUCache(setSize));
            }

            for (uint32_t addr = 0; addr < 0x20000; addr ++) {  // Every byte; byte addresssable
                RAM[addr] = rand() % 256;  // Random "garbage" 2^8 = 256 0-255
            }
        }

        void setPrevLevel(setAssociativeCache* prev) {
        prevLevel = prev;
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
            if(sets[set].isFull()){//current set is full
                CacheLine temp  = sets[set].peek(); 
                uint32_t victimTag = temp.tag;
                uint32_t victimAddress = ((victimTag)<<(bitsOffsets + bitSets)) | (set << bitsOffsets);
            
                //upstream check
                if(prevLevel != nullptr){
                    prevLevel->backInvalidate(victimAddress);   
                }                               
            
                CacheLine* victim = sets[set].get(victimTag); //update victim; cannot call evict/peek-> backinvalidate calls .put -> put in front; tag doesn't change
                              
                
                //downstream check
                if(nextLevel != nullptr){
                    if(victim->dirty){
                        nextLevel->writeBlock(victimAddress, victim->data); 
                        cout << "Evicting dirty line to Next Level\n";
                    }
                }else{
                    // L3 to RAM
                    if (victim->dirty) {
                        for (int i = 0; i < 64; i++) {
                            RAM[victimAddress + i] = victim->data[i];
                        }
                        cout << "Evicting dirty line to RAM\n";
                    }
                }
                sets[set].remove(victimTag);
            }
        }

        void writeBlock(uint32_t address, uint8_t* dataptr){
            int setIdx = set(address);
            int tagValue = tag(address);
            
            if(sets[setIdx].contains(tagValue)){
                CacheLine* line = sets[setIdx].get(tagValue);
                memcpy(line->data, dataptr, 64);
                line->dirty = true; 
            }else{//miss, add, and update
                evictIfNeeded(setIdx); 
                sets[setIdx].put(tagValue, dataptr, true);
            }

        }

        
        void write (uint32_t address, uint32_t data){// I always write to L1
            if (address % 4 != 0) {
                cout << "ERROR: Unaligned access! Address " << hex << address << " is not a multiple of 4.\n";
                return; // Stop
            
            }
            int tagValue = tag(address); 
            int setIdx = set(address);
            int offsetId = offset(address);

            //only L1 should call this; recursively brings cache line up 
            readBlock(address);
           
            if(sets[setIdx].contains(tagValue)){
                CacheLine* line = sets[setIdx].get(tagValue);
                
                //unpacketize
                for(int i =0; i<4; i++){
                    //little endian write
                    line->data[i+offsetId] =  (data >> (i * 8)) & 0xFF; //modifies in place
                }
                line-> dirty = true; 
                cout<<"L1 HIT: Wrote 64 bytes data "<<" to address "<<address<<"\n";
            }
        }

        uint8_t* readBlock (uint32_t address){//fetch the block to L1
            int tagValue = tag(address); 
            int setIdx = set(address); 

            if(sets[setIdx].contains(tagValue)){//hit 
                CacheLine* line = sets[setIdx].get(tagValue); 
                return line->data; 
            }

            uint8_t* newData;
            std::vector<uint8_t> list;
            if(nextLevel !=nullptr){
                newData = nextLevel->readBlock(address);
            }else{//I am L3, look for ram 
                list = ramRead(address);
                newData = list.data(); //newData is the pointer to the data
            }

            //miss, evict, and add
            evictIfNeeded(setIdx); 

            sets[setIdx].put(tagValue,newData, false); 
            return sets[setIdx].get(tagValue)->data; //new dadta
        }

        int read (uint32_t address){ //read the offset
            int offsetId = offset(address);

            uint8_t* block = readBlock(address); 

            int data = 0; 
            for(int i =0; i<4; i++){
                data |= ((block[offsetId+i])<<i*8); 
            }
            cout << "CPU Read: " << hex << data << " from " << address << dec << "\n";
            return data;            
           
        }

        void backInvalidate(uint32_t address){//look upward, copy downstream if dirty
            int tagValue = tag(address); 
            int setIdx = set(address); 

            if(prevLevel != nullptr){//not in L1 yet
                prevLevel->backInvalidate(address);
            }

            //In prevLevel (start with L1)
            if(sets[setIdx].contains(tagValue)){
                CacheLine* line = sets[setIdx].get(tagValue); 

                if(line->dirty && nextLevel !=nullptr){//pass down
                    nextLevel->writeBlock(address,line->data); 
                }

                sets[setIdx].remove(tagValue); 
            //bool isDirty; 
            }
            
            
        }

};

unordered_map<uint32_t, uint8_t> setAssociativeCache::RAM;

int main() {
    cout << "========================================\n";
    cout << "MULTI-LEVEL CACHE HIERARCHY TEST\n";
    cout << "========================================\n";
    
    // Create 3-level hierarchy
    setAssociativeCache L3(32, 8, nullptr, nullptr);  // Largest, slowest
    setAssociativeCache L2(16, 4, nullptr, &L3);
    setAssociativeCache L1(8, 2, nullptr, &L2);
    
    L2.setPrevLevel(&L1);
    L3.setPrevLevel(&L2);
    
    cout << "\n========================================\n";
    cout << "TEST 1: Simple Write and Read (L1 Miss → L2 Miss → L3 Miss → RAM)\n";
    cout << "========================================\n";
    
    L1.write(0x1000, 0xDEADBEEF);
    int val = L1.read(0x1000);
    cout << "Read back: 0x" << hex << val << dec << "\n";
    cout << "Expected: 0xDEADBEEF\n";
    
    cout << "\n========================================\n";
    cout << "TEST 2: Spatial Locality (Multiple words in same block)\n";
    cout << "========================================\n";
    
    L1.write(0x2000, 100);
    L1.write(0x2004, 200);  // Same block, should hit in L1
    L1.write(0x2008, 300);
    L1.write(0x200C, 400);
    
    cout << "Reading back all 4 values...\n";
    cout << "0x2000: " << L1.read(0x2000) << " (expect 100)\n";
    cout << "0x2004: " << L1.read(0x2004) << " (expect 200)\n";
    cout << "0x2008: " << L1.read(0x2008) << " (expect 300)\n";
    cout << "0x200C: " << L1.read(0x200C) << " (expect 400)\n";
    
    cout << "\n========================================\n";
    cout << "TEST 3: L1 → L2 Dirty Writeback\n";
    cout << "========================================\n";
    
    L1.write(0x3000, 555);  // Allocate in L1 (dirty)
    L1.write(0x3800, 666);  // Same set, different tag
    L1.write(0x4000, 777);  // Force L1 eviction of 0x3000 → L2
    
    cout << "\nReading 0x3000 (should hit in L2)...\n";
    int val3 = L1.read(0x3000);  // L1 miss, L2 hit
    cout << "Value: " << val3 << " (expect 555)\n";
    
    cout << "\n========================================\n";
    cout << "TEST 4: L2 → L3 Dirty Writeback\n";
    cout << "========================================\n";
    
    // Fill L1 and L2 to force cascading evictions
    for (int i = 0; i < 20; i++) {
        L1.write(0x5000 + i * 0x800, i * 111);
    }
    
    cout << "\nReading early value (should be in L3)...\n";
    int early = L1.read(0x5000);
    cout << "Value: " << early << " (expect 0)\n";
    
    cout << "\n========================================\n";
    cout << "TEST 5: Back-Invalidation (L3 evicts → L2 and L1 invalidate)\n";
    cout << "========================================\n";
    
    L1.write(0x6000, 888);  // Allocate in all 3 levels
    cout << "\nValue is now in L1, L2, L3...\n";
    
    // Fill L3 to force eviction of 0x6000
    for (int i = 0; i < 300; i++) {
        L1.write(0x7000 + i * 0x1000, i);
    }
    
    cout << "\nChecking if 0x6000 was invalidated from L1/L2...\n";
    cout << "(Should see miss → miss → miss → RAM fetch)\n";
    int val6 = L1.read(0x6000);
    cout << "Value: " << val6 << "\n";
    
    cout << "\n========================================\n";
    cout << "TEST 6: Dirty Data During Back-Invalidation\n";
    cout << "========================================\n";
    
    L1.write(0x8000, 999);  // Dirty in L1
    L1.read(0x8000);        // Confirm it's there
    
    // Force L2 to evict 0x8000 (which should trigger L1 back-invalidate)
    // L1 must writeback dirty data to L2 before invalidating
    for (int i = 0; i < 50; i++) {
        L1.write(0x9000 + i * 0x800, i);
    }
    
    cout << "\nReading 0x8000 after invalidation...\n";
    int val8 = L1.read(0x8000);
    cout << "Value: " << val8 << " (expect 999 - data preserved)\n";
    
    cout << "\n========================================\n";
    cout << "TEST 7: Cross-Block Verification\n";
    cout << "========================================\n";
    
    // Write multiple blocks, verify isolation
    L1.write(0xA000, 10);
    L1.write(0xA040, 20);  // Different block
    L1.write(0xA080, 30);
    
    // Modify first block
    L1.write(0xA000, 11);
    
    // Verify others unchanged
    cout << "0xA000: " << L1.read(0xA000) << " (expect 11)\n";
    cout << "0xA040: " << L1.read(0xA040) << " (expect 20)\n";
    cout << "0xA080: " << L1.read(0xA080) << " (expect 30)\n";
    
    cout << "\n========================================\n";
    cout << "TEST 8: Unaligned Access Detection\n";
    cout << "========================================\n";
    
    L1.write(0xB001, 123);  // Should error (not multiple of 4)
    
    cout << "\n========================================\n";
    cout << "TEST 9: Stress Test - Rapid Writes\n";
    cout << "========================================\n";
    
    for (int i = 0; i < 100; i++) {
        L1.write(0xC000 + i * 4, i);
    }
    
    cout << "Verifying random samples...\n";
    cout << "0xC000: " << L1.read(0xC000) << " (expect 0)\n";
    cout << "0xC064: " << L1.read(0xC064) << " (expect 25)\n";
    cout << "0xC188: " << L1.read(0xC188) << " (expect 98)\n";
    
    cout << "\n========================================\n";
    cout << "ALL TESTS COMPLETE\n";
    cout << "========================================\n";
    
    return 0;
}