#include <iostream>
#include <vector> 
#include <cassert>

using namespace std;

struct Cacheline{
    uint32_t tag = 0; 
    bool valid; 
    bool dirty; 
    int data =0; 
    int lru_counter =0;

    void print() const{
        if(!valid){
            cout<"INVALID";
            return;
        }

        cout<<"Tag: "<<hex<<tag<<dec
        <<", Data = "<<data
        <<", Dirty? = "<<(dirty?"Yes":"No")
        <<", LRU Counter = "<<lru_counter; 
    }

}

class setAssociativeCache{
    private: 
        int const static num_sets = 8; 
        int const static way =2; 
        static const int offset_bits = 4;
        static const int set_bits = 3;

        //32bits, 3 bits set, 4 bits offset, 32-7 = 25 bit tag 

        Cacheline cache[num_sets][way]; //creates a matrix where each cell is a cacheline

        int hit =0; 
        int miss =0; 
        int writeback =0; 

        int getset(uint32_t address){

            int set = 0; 

            // for(int i =0; i<set_bits; i++){
            //     set +=(address >>offset_bits)&(1<<i);       //mask 1 by 1 
            // }                                               // if set_bits =3, then 2^3 = 8 that is 1000 but 2^3 -1 = 7
                                                               // 7 = 111 mask all at once -> this hold for all 
            set = (address >> offset_bits) & ((1 << set_bits) - 1); //
            return set;             
        }

        int gettag(uint32_t address){
            return address >> (set_bits+ offset_bits);
        }
        
            
}






