#ifndef PCB_HPP
#define PCB_HPP
/*
  PCB.hpp
  Definição do bloco de controle de processo (PCB) usado pelo simulador da CPU.
  Centraliza: identificação do processo, prioridade, quantum, pesos de memória e
  contadores de instrumentação de pipeline/memória.
*/
#include <string>
#include <atomic>
#include <cstdint>
#include <unordered_map>
#include "memory/cache.hpp"
#include "REGISTER_BANK.hpp" // necessidade de objeto completo dentro do PCB


// Estados possíveis do processo (simplificado)
enum class State {
    Ready,
    Running,
    Blocked,
    Finished
};




struct MemWeights {
    uint64_t cache = 1;   // custo por acesso à memória cache
    uint64_t primary = 5; // custo por acesso à memória primária
    uint64_t secondary = 10; // custo por acesso à memória secundária
};

struct PCB {
    int pid = 0;
    std::string name;
    std::string program_path;
    int quantum = 0;
    int priority = 0;
    //Isso aqui vai ser usado principalmente no SJF
    int burst_time = 0; // número total de instruções carregadas, seria como o tempo estimado de cada processo para usar no SJF

    State state = State::Ready;
    hw::REGISTER_BANK regBank;

    // tabela de páginas, faz o mapeamento Página virtual -> Frame físico
    std::unordered_map<int, int> pageTable;



    //Métricas de Tempo / escalonamento
    uint64_t arrival_time =0; // momento em que entrou no sistema
    uint64_t first_start_time =0; //primeira vez que foi escalonado
    uint64_t finish_time =0; //momento em que terminou

    uint64_t waiting_time = 0; //tempo total em fila Ready
    uint64_t last_ready_in =0; //instante em que entrou em ready pela última vez
    uint64_t cpu_time =0; //total de "ciclos de cpu" efetivamente rodando

    // Contadores de acesso à memória
    std::atomic<uint64_t> primary_mem_accesses{0};
    std::atomic<uint64_t> secondary_mem_accesses{0};
    std::atomic<uint64_t> memory_cycles{0};
    std::atomic<uint64_t> mem_accesses_total{0};
    std::atomic<uint64_t> extra_cycles{0};
    std::atomic<uint64_t> cache_mem_accesses{0};

    // Instrumentação detalhada
    std::atomic<uint64_t> pipeline_cycles{0};
    std::atomic<uint64_t> stage_invocations{0};
    std::atomic<uint64_t> mem_reads{0};
    std::atomic<uint64_t> mem_writes{0};

    // Novos contadores
    std::atomic<uint64_t> cache_hits{0};
    std::atomic<uint64_t> cache_misses{0};
    std::atomic<uint64_t> io_cycles{1};

    MemWeights memWeights;
};

// Contabilizar cache
inline void contabiliza_cache(PCB &pcb, bool hit) {
    if (hit) {
        pcb.cache_hits++;
    } else {
        pcb.cache_misses++;
    }
}

#endif // PCB_HPP