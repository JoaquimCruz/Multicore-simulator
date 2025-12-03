#include "MemoryManager.hpp"
#include <iostream>

MemoryManager::MemoryManager(size_t mainMemorySize, size_t secondaryMemorySize) 
    : mainMemoryLimit(mainMemorySize) 
{
    mainMemory = std::make_unique<MAIN_MEMORY>(mainMemorySize);
    secondaryMemory = std::make_unique<SECONDARY_MEMORY>(secondaryMemorySize);
    L1_cache = std::make_unique<Cache>();
    
    size_t numFrames = mainMemorySize / PAGE_SIZE;
    framesMap.resize(numFrames, false); 
}

int MemoryManager::allocateFrame() {
    for (size_t i = 0; i < framesMap.size(); ++i) {
        if (!framesMap[i]) {
            framesMap[i] = true; 
            return i;
        }
    }
    return -1; 
}

uint32_t MemoryManager::translateAddress(uint32_t virtualAddress, PCB& process, bool isWrite) {
    int pageNumber = virtualAddress / PAGE_SIZE;
    int offset = virtualAddress % PAGE_SIZE;

    if (process.pageTable.find(pageNumber) == process.pageTable.end()) {
        if (isWrite) {
            int newFrame = allocateFrame();
            if (newFrame == -1) {
                std::cerr << "ERRO FATAL: Memória cheia! (PID " << process.pid << ")\n";
                return MEMORY_ACCESS_ERROR;
            }
            process.pageTable[pageNumber] = newFrame;
            std::cout << "[MMU] PID " << process.pid << ": Alocado Frame " << newFrame << " para Pagina " << pageNumber << "\n";
        } else {
            return MEMORY_ACCESS_ERROR;
        }
    }

    int frameNumber = process.pageTable[pageNumber];
    uint32_t physicalAddress = (frameNumber * PAGE_SIZE) + offset;
    return physicalAddress;
}

uint32_t MemoryManager::read(uint32_t virtualAddress, PCB& process) {
    std::lock_guard<std::recursive_mutex> lock(memMutex);

    process.mem_accesses_total.fetch_add(1);
    process.mem_reads.fetch_add(1);

    uint32_t physicalAddress = translateAddress(virtualAddress, process, false);

    if (physicalAddress == MEMORY_ACCESS_ERROR) {
        std::cerr << "ERRO: Leitura inválida no endereço " << virtualAddress 
                  << " (Pagina " << virtualAddress / PAGE_SIZE << " não mapeada) PID " << process.pid << "\n";
        return 0; 
    }

    size_t cache_data = L1_cache->get(physicalAddress);

    if (cache_data != CACHE_MISS) { 
        process.cache_mem_accesses.fetch_add(1);
        process.memory_cycles.fetch_add(process.memWeights.cache);
        contabiliza_cache(process, true);
        return static_cast<uint32_t>(cache_data);
    }

    contabiliza_cache(process, false);
    uint32_t data_from_mem = 0;
    
    if (physicalAddress < mainMemoryLimit) {
        process.primary_mem_accesses.fetch_add(1);
        process.memory_cycles.fetch_add(process.memWeights.primary);
        
        // CORREÇÃO: DIVIDIR POR 4 PARA ACESSAR INDICE DO VETOR
        data_from_mem = mainMemory->ReadMem(physicalAddress / 4);
        
    } else {
        process.secondary_mem_accesses.fetch_add(1);
        process.memory_cycles.fetch_add(process.memWeights.secondary);
        uint32_t secondaryAddress = physicalAddress - mainMemoryLimit;
        // Assume Secundária também é vetor de palavras
        data_from_mem = secondaryMemory->ReadMem(secondaryAddress / 4);
    }

    L1_cache->put(physicalAddress, data_from_mem, this);
    return data_from_mem;
}

void MemoryManager::write(uint32_t virtualAddress, uint32_t data, PCB& process) {
    std::lock_guard<std::recursive_mutex> lock(memMutex);

    process.mem_accesses_total.fetch_add(1);
    process.mem_writes.fetch_add(1);

    uint32_t physicalAddress = translateAddress(virtualAddress, process, true);
    if (physicalAddress == MEMORY_ACCESS_ERROR) return;

    if (physicalAddress < mainMemoryLimit) {
        process.primary_mem_accesses.fetch_add(1);
        process.memory_cycles.fetch_add(process.memWeights.primary);
        
        // CORREÇÃO: DIVIDIR POR 4
        mainMemory->WriteMem(physicalAddress / 4, data);
        
    } else {
        uint32_t secondaryAddress = physicalAddress - mainMemoryLimit;
        secondaryMemory->WriteMem(secondaryAddress / 4, data);
    }
    
    size_t cacheCheck = L1_cache->get(physicalAddress);
    if (cacheCheck != CACHE_MISS) {
        L1_cache->update(physicalAddress, data);
        contabiliza_cache(process, true); 
    } else {
        L1_cache->put(physicalAddress, data, this);
        contabiliza_cache(process, false);
    }
    
    process.cache_mem_accesses.fetch_add(1);
    process.memory_cycles.fetch_add(process.memWeights.cache);
}

void MemoryManager::writeToFile(uint32_t address, uint32_t data) {
    if (address < mainMemoryLimit) {
        mainMemory->WriteMem(address / 4, data);
    } else {
        uint32_t secondaryAddress = address - mainMemoryLimit;
        secondaryMemory->WriteMem(secondaryAddress / 4, data);
    }
}