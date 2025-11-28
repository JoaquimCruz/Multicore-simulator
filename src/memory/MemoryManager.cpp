#include "MemoryManager.hpp"
#include <iostream>

MemoryManager::MemoryManager(size_t mainMemorySize, size_t secondaryMemorySize) 
    : mainMemoryLimit(mainMemorySize) // Inicializa o limite
{
    mainMemory = std::make_unique<MAIN_MEMORY>(mainMemorySize);
    secondaryMemory = std::make_unique<SECONDARY_MEMORY>(secondaryMemorySize);
    L1_cache = std::make_unique<Cache>();
    
    // Inicializa o mapa de frames (Total RAM / Tamanho Página)
    size_t numFrames = mainMemorySize / PAGE_SIZE;
    framesMap.resize(numFrames, false); // Todos livres
}

// Encontra o primeiro frame livre na RAM
int MemoryManager::allocateFrame() {
    for (size_t i = 0; i < framesMap.size(); ++i) {
        if (!framesMap[i]) {
            framesMap[i] = true; // Marca frame como ocupado
            return i;
        }
    }
    return -1; // Memória cheia
}

// MMU: Traduz Virtual -> Físico
uint32_t MemoryManager::translateAddress(uint32_t virtualAddress, PCB& process, bool isWrite) {
    int pageNumber = virtualAddress / PAGE_SIZE;
    int offset = virtualAddress % PAGE_SIZE;

    // Verifica se a página existe na Tabela de Páginas do Processo
    if (process.pageTable.find(pageNumber) == process.pageTable.end()) {
        // Page Fault (Página não mapeada)
        
        if (isWrite) {
            // Lazy Allocation: Aloca frame apenas na primeira escrita
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

    // Calcula Endereço Físico
    int frameNumber = process.pageTable[pageNumber];
    uint32_t physicalAddress = (frameNumber * PAGE_SIZE) + offset;
    
    return physicalAddress;
}

uint32_t MemoryManager::read(uint32_t virtualAddress, PCB& process) {
    // Lock Recursivo: Permite que este método seja chamado, e se a cache chamar writeToFile, não trava.
    std::lock_guard<std::recursive_mutex> lock(memMutex);

    process.mem_accesses_total.fetch_add(1);
    process.mem_reads.fetch_add(1);

    // 1. Tradução de Endereço (MMU)
    uint32_t physicalAddress = translateAddress(virtualAddress, process, false);

    if (physicalAddress == MEMORY_ACCESS_ERROR) {
        std::cerr << "ERRO: Leitura inválida (Pagina não mapeada) PID " << process.pid << "\n";
        return 0; 
    }

    // 2. Tenta ler da Cache L1 (Usei 'get' conforme seu cache.hpp)
    size_t cache_data = L1_cache->get(physicalAddress);

    if (cache_data != CACHE_MISS) { 
        // CACHE HIT
        process.cache_mem_accesses.fetch_add(1);
        process.memory_cycles.fetch_add(process.memWeights.cache);
        contabiliza_cache(process, true);
        return static_cast<uint32_t>(cache_data);
    }

    // 3. CACHE MISS: Busca na RAM
    contabiliza_cache(process, false);
    
    uint32_t data_from_mem = 0;
    
    if (physicalAddress < mainMemoryLimit) {
        process.primary_mem_accesses.fetch_add(1);
        process.memory_cycles.fetch_add(process.memWeights.primary);
        data_from_mem = mainMemory->ReadMem(physicalAddress);
    } else {
        // Fallback para memória secundária (não usado sem swap, mas mantido)
        process.secondary_mem_accesses.fetch_add(1);
        process.memory_cycles.fetch_add(process.memWeights.secondary);
        uint32_t secondaryAddress = physicalAddress - mainMemoryLimit;
        data_from_mem = secondaryMemory->ReadMem(secondaryAddress);
    }

    // 4. Atualiza Cache (put exige 'this' para write-back)
    L1_cache->put(physicalAddress, data_from_mem, this);

    return data_from_mem;
}

void MemoryManager::write(uint32_t virtualAddress, uint32_t data, PCB& process) {
    std::lock_guard<std::recursive_mutex> lock(memMutex);

    process.mem_accesses_total.fetch_add(1);
    process.mem_writes.fetch_add(1);

    // 1. Tradução (isWrite = true para alocar se precisar)
    uint32_t physicalAddress = translateAddress(virtualAddress, process, true);

    if (physicalAddress == MEMORY_ACCESS_ERROR) return;

    // 2. Escrita na Memória Principal (Write-Through simplificado)
    if (physicalAddress < mainMemoryLimit) {
        process.primary_mem_accesses.fetch_add(1);
        process.memory_cycles.fetch_add(process.memWeights.primary);
        mainMemory->WriteMem(physicalAddress, data);
    } else {
        uint32_t secondaryAddress = physicalAddress - mainMemoryLimit;
        secondaryMemory->WriteMem(secondaryAddress, data);
    }
    
    // 3. Atualiza Cache se existir (ou insere)
    // Se estiver na cache, update marca como dirty. Se não, put insere.
    size_t cacheCheck = L1_cache->get(physicalAddress);
    if (cacheCheck != CACHE_MISS) {
        L1_cache->update(physicalAddress, data);
        contabiliza_cache(process, true); // Consideramos Hit se estava lá para update
    } else {
        // Se não estava, insere (Write-Allocate)
        L1_cache->put(physicalAddress, data, this);
        contabiliza_cache(process, false);
    }
    
    process.cache_mem_accesses.fetch_add(1);
    process.memory_cycles.fetch_add(process.memWeights.cache);
}

// Callback chamado pela Cache quando precisa salvar uma linha suja (Eviction)
void MemoryManager::writeToFile(uint32_t address, uint32_t data) {
    // O lock já é detido pelo método 'read' ou 'write' que chamou a cache.
    // Como é recursive_mutex, podemos travar de novo ou apenas executar.
    
    // O endereço vindo da cache JÁ É FÍSICO.
    if (address < mainMemoryLimit) {
        mainMemory->WriteMem(address, data);
    } else {
        uint32_t secondaryAddress = address - mainMemoryLimit;
        secondaryMemory->WriteMem(secondaryAddress, data);
    }
}