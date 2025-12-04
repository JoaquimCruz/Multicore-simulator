#ifndef MEMORY_MANAGER_HPP
#define MEMORY_MANAGER_HPP

#include <memory>
#include <stdexcept>
#include <mutex> 
#include <vector>
#include <algorithm>
#include <map>
#include "MAIN_MEMORY.hpp"
#include "SECONDARY_MEMORY.hpp"
#include "cache.hpp" 
#include "../cpu/PCB.hpp" 

// 32 palavras por página, não sei se o tamanho é esse.
const size_t PAGE_SIZE = 32;

struct FrameInfo {
    PCB* ownerProcess = nullptr;
    int virtualPageNumber = -1;
};



class MemoryManager {
public:
    MemoryManager(size_t mainMemorySize, size_t secondaryMemorySize);

    // Agora o endereço recebido é virtual 
    uint32_t read(uint32_t virtualAddress, PCB& process);
    void write(uint32_t virtualAddress, uint32_t data, PCB& process);
    
    // Função auxiliar para o write-back da cache
    void writeToFile(uint32_t address, uint32_t data);

private:
    std::unique_ptr<MAIN_MEMORY> mainMemory;
    std::unique_ptr<SECONDARY_MEMORY> secondaryMemory;
    std::unique_ptr<Cache> L1_cache; // Adiciona a Cache L1

    /*
        Nessa parte aqui eu usei o recursive_mutex porque a função de tradução de endereço
        pode ser chamada dentro de read/write, que já travam o mutex. Assim evitamos deadlock.
        Não consegui fazer usando só mutex, tive que pedir ajuda ao gemini kkkk.
    */
    std::recursive_mutex memMutex; 

    size_t mainMemoryLimit;
    size_t numFrames;


    std::vector<bool>framesMap;
    std::vector<FrameInfo> frameOwnerTable;

    // Tabela de Swap: {PID, PáginaVirtual} -> EndereçoFísicoNoDisco
    std::map<std::pair<int, int>, uint32_t> swapTable;

    uint32_t nextSwapAddress;

    // Ponteiro para algoritmo FIFO/Clock
    int victimFramePtr;

    // Métodos da MMU
    uint32_t translateAddress(uint32_t virtualAddress, PCB& process, bool isWrite);
    int allocateFrame(PCB& process, int virtualPage);

    // Remove uma página da RAM para o Disco (retorna o índice do frame liberado)
    int swapOut();

    // Traz uma página do Disco para a RAM
    void swapIn(int frameIndex, PCB& process, int virtualPage, uint32_t diskAddress);
};

#endif 