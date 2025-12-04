#include "MemoryManager.hpp"
#include <iostream>

MemoryManager::MemoryManager(size_t mainMemorySize, size_t secondaryMemorySize) 
    : mainMemoryLimit(mainMemorySize), nextSwapAddress(0), victimFramePtr(0)
{
    mainMemory = std::make_unique<MAIN_MEMORY>(mainMemorySize);
    secondaryMemory = std::make_unique<SECONDARY_MEMORY>(secondaryMemorySize);
    L1_cache = std::make_unique<Cache>();
    
    // Calcula frames. Se mainMemorySize=192 e PAGE_SIZE=32, temos 6 frames.
    numFrames = mainMemorySize / PAGE_SIZE;
    framesMap.resize(numFrames, false); 
    frameOwnerTable.resize(numFrames);
}

int MemoryManager::swapOut() {
    int victimIndex = victimFramePtr;
    victimFramePtr = (victimFramePtr + 1) % numFrames; 

    FrameInfo& victimInfo = frameOwnerTable[victimIndex];
    
    // Se o frame estiver vazio (erro de consistência), apenas retorna ele
    if (victimInfo.ownerProcess == nullptr) {
        return victimIndex;
    }

    PCB* victimPCB = victimInfo.ownerProcess;
    int victimPage = victimInfo.virtualPageNumber;
    
    // O disco é um vetor de palavras. 
    // Uma página de 32 bytes tem 8 palavras (32 / 4).
    uint32_t wordsPerPage = PAGE_SIZE / 4; 

    uint32_t diskAddr = nextSwapAddress;
    nextSwapAddress += wordsPerPage; // Avança apenas 8 posições no disco

    // Calcula onde começa o frame na RAM (vetor de palavras)
    // Frame 0 = índice 0. Frame 1 = índice 8. Frame 2 = índice 16...
    uint32_t ramBaseIndex = victimIndex * wordsPerPage;

    // Loop ajustado: Itera 8 vezes (palavras), não 32.
    for (size_t i = 0; i < wordsPerPage; i++) {
        uint32_t data = mainMemory->ReadMem(ramBaseIndex + i);
        secondaryMemory->WriteMem(diskAddr + i, data);
    }

    swapTable[{victimPCB->pid, victimPage}] = diskAddr;
    victimPCB->pageTable.erase(victimPage);

    std::cout << "[SWAP-OUT] Frame " << victimIndex 
              << " (PID " << victimPCB->pid << ", Pag " << victimPage << ")"
              << " -> Disco @" << diskAddr << "\n";

    return victimIndex;
}

void MemoryManager::swapIn(int frameIndex, PCB& process, int virtualPage, uint32_t diskAddress) {
    uint32_t wordsPerPage = PAGE_SIZE / 4; // 8 palavras
    uint32_t ramBaseIndex = frameIndex * wordsPerPage;

    // Loop ajustado: Itera 8 vezes
    for (size_t i = 0; i < wordsPerPage; i++) {
        uint32_t data = secondaryMemory->ReadMem(diskAddress + i);
        mainMemory->WriteMem(ramBaseIndex + i, data);
    }

    swapTable.erase({process.pid, virtualPage});

    std::cout << "[SWAP-IN]  PID " << process.pid << ", Pag " << virtualPage 
              << " (Disco @" << diskAddress << ") -> Frame " << frameIndex << "\n";
}

int MemoryManager::allocateFrame(PCB& process, int virtualPage) {
    for (size_t i = 0; i < framesMap.size(); ++i) {
        if (!framesMap[i]) {
            framesMap[i] = true; 
            frameOwnerTable[i] = {&process, virtualPage};
            return i;
        }
    }
    // Memória cheia -> Swap Out
    int freedFrame = swapOut();
    frameOwnerTable[freedFrame] = {&process, virtualPage};
    return freedFrame;
}

uint32_t MemoryManager::translateAddress(uint32_t virtualAddress, PCB& process, bool isWrite) {
    int pageNumber = virtualAddress / PAGE_SIZE;
    int offset = virtualAddress % PAGE_SIZE;

    // 1. RAM Hit
    if (process.pageTable.find(pageNumber) != process.pageTable.end()) {
        int frameNumber = process.pageTable[pageNumber];
        return (frameNumber * PAGE_SIZE) + offset;
    }

    // 2. Swap Hit (Está no disco)
    auto swapEntry = swapTable.find({process.pid, pageNumber});
    if (swapEntry != swapTable.end()) {
        uint32_t diskAddr = swapEntry->second;
        int newFrame = allocateFrame(process, pageNumber);
        swapIn(newFrame, process, pageNumber, diskAddr);
        process.pageTable[pageNumber] = newFrame;
        return (newFrame * PAGE_SIZE) + offset;
    }

    // 3. Nova Alocação
    if (isWrite) {
        int newFrame = allocateFrame(process, pageNumber);
        process.pageTable[pageNumber] = newFrame;
        std::cout << "[MMU] PID " << process.pid << ": Alocado Frame " << newFrame << " para Pagina " << pageNumber << "\n";
        return (newFrame * PAGE_SIZE) + offset;
    } else {
        return MEMORY_ACCESS_ERROR;
    }
}

uint32_t MemoryManager::read(uint32_t virtualAddress, PCB& process) {
    std::lock_guard<std::recursive_mutex> lock(memMutex);

    process.mem_accesses_total.fetch_add(1);
    process.mem_reads.fetch_add(1);

    uint32_t physicalAddress = translateAddress(virtualAddress, process, false);

    if (physicalAddress == MEMORY_ACCESS_ERROR) {
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
        // Endereço Físico (bytes) / 4 = Índice do Vetor (palavras)
        data_from_mem = mainMemory->ReadMem(physicalAddress / 4);
    } else {
        process.secondary_mem_accesses.fetch_add(1);
        process.memory_cycles.fetch_add(process.memWeights.secondary);
        uint32_t secondaryAddress = physicalAddress - mainMemoryLimit;
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
        // Endereço Físico (bytes) / 4 = Índice do Vetor (palavras)
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