#include <iostream>
#include <vector>
#include <deque>
#include <memory>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include "cpu/Scheduler.hpp"
#include <atomic>
#include <mutex>
// Inclua a biblioteca JSON (verifique se o caminho está correto no seu projeto)
#include "nlohmann/json.hpp"

#include "cpu/PCB.hpp"
#include "cpu/pcb_loader.hpp"
#include "cpu/CONTROL_UNIT.hpp"
#include "memory/MemoryManager.hpp"
#include "parser_json/parser_json.hpp"
#include "IO/IOManager.hpp"

using json = nlohmann::json;

/*
    Por enquanto, implementei um Scheduler simples baseado em Round-Robin (RR)
    com quantum fixo definido pelo sistema operacional.
    O loop principal do escalonador seleciona o próximo processo da fila de prontos,
    executa-o por um quantum ou até que ele bloqueie/termine, e então reavalia a fila.
    Processos que solicitam I/O são movidos para uma fila de bloqueados e retornam quando o I/O é concluído.
    O escalonador continua até que todos os processos tenham terminado sua execução.    
    
    Temos que expandir isso para suportar múltiplos núcleos e políticas adicionais depois.

*/
void* Core(MemoryManager &memoryManager, PCB &process, std::vector<std::unique_ptr<IORequest>>* ioRequests, bool &printLock);

const int SYSTEM_QUANTUM = 20; 
const int NUM_CORES=4;

// Função para imprimir as métricas de um processo (MANTIDA IGUAL)
// void print_metrics(const PCB& pcb) {
//     std::cout << "\n--- METRICAS FINAIS DO PROCESSO " << pcb.pid << " ---\n";
//     std::cout << "Nome do Processo:       " << pcb.name << "\n";
//     std::cout << "Estado Final:           " << (pcb.state == State::Finished ? "Finished" : "Incomplete") << "\n";
//     std::cout << "Ciclos de Pipeline:     " << pcb.pipeline_cycles.load() << "\n";
//     std::cout << "Total de Acessos a Mem: " << pcb.mem_accesses_total.load() << "\n";
//     std::cout << "  - Leituras:             " << pcb.mem_reads.load() << "\n";
//     std::cout << "  - Escritas:             " << pcb.mem_writes.load() << "\n";
//     std::cout << "Acessos a Cache L1:     " << pcb.cache_mem_accesses.load() << "\n";
//     std::cout << "Acessos a Mem Principal:" << pcb.primary_mem_accesses.load() << "\n";
//     std::cout << "Acessos a Mem Secundaria:" << pcb.secondary_mem_accesses.load() << "\n";
//     std::cout << "Ciclos Totais de MemoriA: " << pcb.memory_cycles.load() << "\n";
//     std::cout << "------------------------------------------\n";

//     std::filesystem::create_directory("output");
//     std::ofstream resultados("output/resultados.dat", std::ios::app); // Append para não sobrescrever se houver vários
//     std::ofstream output("output/output_" + std::to_string(pcb.pid) + ".dat"); // Arquivo separado por PID

//     if (resultados.is_open()) {
//         resultados << "=== Resultados de Execução (PID: " << pcb.pid << ") ===\n";
//         resultados << "Nome: " << pcb.name << "\n";
//         resultados << "Quantum: " << pcb.quantum << "\n";
//         resultados << "Prioridade: " << pcb.priority << "\n";
//         resultados << "Ciclos de Pipeline: " << pcb.pipeline_cycles << "\n";
//         resultados << "Ciclos de Memória: " << pcb.memory_cycles << "\n";
//         resultados << "Cache Hits: " << pcb.cache_hits << "\n";
//         resultados << "Cache Misses: " << pcb.cache_misses << "\n";
//         resultados << "Ciclos de IO: " << pcb.io_cycles << "\n";
//         resultados << "--------------------------------\n";
//     }

//     if (output.is_open()) {
//         output << "=== Saída Lógica do Programa ===\n";
//         output << "Registradores principais:\n";
//         output << pcb.regBank.get_registers_as_string() << "\n";
//         output << "\n=== Operações Executadas ===\n";

//         // Lê o arquivo temporário com operações
//         std::string temp_filename = "output/temp_" + std::to_string(pcb.pid) + ".log"; // Temp específico por PID (futuro)
//         // OBS: Atualmente seu código gera temp_1.log fixo, precisaremos mudar isso depois para multicore
//         if (std::filesystem::exists("output/temp_1.log")) { 
//             std::ifstream temp_file("output/temp_1.log");
//             if (temp_file.is_open()) {
//                 std::string line;
//                 while (std::getline(temp_file, line)) {
//                     output << line << "\n";
//                 }
//                 temp_file.close();
//             }
//             // Não removi ainda pois em multicore pode dar conflito, limpar no final
//         } else {
//             output << "(Nenhuma operação registrada)\n";
//         }
//         output << "\n=== Fim das Operações Registradas ===\n";
//     }
// }

// void coreWorker(int coreId, Scheduler& scheduler, MemoryManager& memManager,IOManager& ioManager, std::vector<PCB*>& blocked_list, std::mutex& blocked_mutex,std::atomic<int>& finished_processes, int total_processes){
    
//     bool print_lock = true;
//     // cada core tem seu vetor de IO request agora
//     std::vector<std::unique_ptr<IORequest>> io_requests;


//     while (finished_processes.load() < total_processes) {
//         // Verifica desbloqueios de IO
//     {
//         // isso garante que só um core mexa por vez na lista de processos bloqueados
//         std::lock_guard<std::mutex> lock(blocked_mutex);
//         for (auto it = blocked_list.begin(); it != blocked_list.end(); ) {
//             if ((*it)->state == State::Ready) { // IOManager liberou
//                  std::cout << "[Core " << coreId << "] Processo " << (*it)->pid 
//                               << " fim de IO -> Scheduler.\n";
//                 scheduler.addProcess(*it); // Devolve pro Scheduler
//                 it = blocked_list.erase(it);
//             } else {
//                 ++it;
//             }
//         }
//     }

//     PCB* current_process = scheduler.getNextProcess();

//     if(current_process == nullptr){
//         std::this_thread::sleep_for(std::chrono::milliseconds(1));
//         continue;
//     }


//     std::cout << "\n[Core " << coreId << "][Scheduler] Executando PID "
//                   << current_process->pid << " (" << current_process->name
//                   << ") - Quantum: " << current_process->quantum << "\n";

//     current_process->state = State::Running;
//     io_requests.clear();
//     //todos os cores acessam a memória de forma segura
//     Core(memManager, *current_process, &io_requests, print_lock);


//     switch (current_process->state) {
//             case State::Blocked:
//                 std::cout << "[Core " << coreId << "][Scheduler] PID "
//                           << current_process->pid << " solicitou I/O -> Bloqueado.\n";
//                 ioManager.registerProcessWaitingForIO(current_process);
//                 {
//                     std::lock_guard<std::mutex> lock(blocked_mutex);
//                     blocked_list.push_back(current_process);
//                 }
//                 break;

//             case State::Finished:
//                 std::cout << "[Core " << coreId << "][Scheduler] PID "
//                           << current_process->pid << " FINALIZADO.\n";
//                 print_metrics(*current_process);
//                 finished_processes.fetch_add(1);
//                 break;

//             default:
//                 if (scheduler.isPreemptive()) {
//                     std::cout << "[Core " << coreId << "][Scheduler] PID "
//                               << current_process->pid
//                               << " fim de quantum (preemptivo) -> Ready Queue.\n";
//                     current_process->state = State::Ready;
//                     scheduler.addProcess(current_process);
//                 } else {
//                     std::cout << "[Core " << coreId 
//                               << "][Scheduler] Política não preemptiva: "
//                               << "continuando execução do PID "
//                               << current_process->pid << "\n";
//                     current_process->state = State::Running;
//                     scheduler.pushFront(current_process);
//                 }
//                 break;
//         }
//     }

//     std::cout << "[Core " << coreId << "] Finalizando: todos os processos já terminados.\n";



// }

// int main() {
//     // 1. Inicialização dos Módulos Principais
//     std::cout << "=== Inicializando o Simulador Multicore (Fase 1: Limpeza) ===\n";
//     MemoryManager memManager(1024, 8192);
//     IOManager ioManager;

//     // Estruturas de Processos
//     int policyOption = 2;
//     SchedulingPolicy policy = SchedulingPolicy::Priority;
//     switch (policyOption){
//         case 0: policy = SchedulingPolicy::FCFS; break;
//         case 1: policy = SchedulingPolicy::SJN; break;
//         case 2: policy = SchedulingPolicy::RR; break;
//         case 3: policy = SchedulingPolicy::Priority; break;

//     }

//     Scheduler scheduler(policy,SYSTEM_QUANTUM);
//     std::vector<std::unique_ptr<PCB>> process_list; // Dono dos ponteiros
//     std::vector<PCB*> blocked_list;                 // Fila de bloqueados

//     // 2. Carregamento do Lote (BATCH)
//     std::ifstream batchFile("batch.json");
//     if (!batchFile.is_open()) {
//         std::cerr << "Erro fatal: Arquivo 'batch.json' nao encontrado!\n";
//         return 1;
//     }

//     try {
//         json batch;
//         batchFile >> batch;

//         std::cout << "[System] Lendo lote de processos...\n";

//         for (const auto& procFile : batch["processes"]) {
//             std::string filename = procFile.get<std::string>();
//             auto newPcb = std::make_unique<PCB>();

//             std::cout << "[System] Carregando arquivo de definição: " << filename << "... ";
            
//             if (load_pcb_from_json(filename, *newPcb)) {
//                 // Definição do Quantum pelo SO
//                 newPcb->quantum = SYSTEM_QUANTUM; // pus 20, mas se quiserem só mudar para testar fecho

//                 // Carrega o programa (código assembly) na memória
//                 if (!newPcb->program_path.empty()) {
//                     // O '0' é o endereço base físico. Com a paginação isso mudará
//                     loadJsonProgram(newPcb->program_path, memManager, *newPcb, 0);
//                     std::cout << "OK! (Programa: " << newPcb->program_path << ")\n";
                    
//                     // Adiciona ao Scheduler em vez de uma fila manual que estava
//                     scheduler.addProcess(newPcb.get());
//                     process_list.push_back(std::move(newPcb));
//                 } else {
//                     std::cout << "FALHA (program_path vazio)\n";
//                 }
//             } else {
//                 std::cout << "FALHA (Erro no JSON)\n";
//             }
//         }
//     } catch (const std::exception& e) {
//         std::cerr << "Erro no parsing do batch.json: " << e.what() << "\n";
//         return 1;
//     }

//     int total_processes = process_list.size();
//     if (total_processes == 0) {
//         std::cerr << "Nenhum processo carregado. Encerrando.\n";
//         return 0;
//     }

//     std::cout << "\n[System] Total de processos prontos: " << total_processes << "\n";

//     // 3. Loop Principal do Escalonador (Mantido Single-Core por enquanto)
//     std::cout << "\nIniciando execucao com Scheduler...\n";
    
//     //contador atômico compartilhado
//     std::atomic<int> finished_processes{0};
//     //mutex para lista de bloqueados
//     std::mutex blocked_mutex;

//     //cria um vetor de threads, os núcleos
//     std::vector<std::thread> core_threads;
//     core_threads.reserve(NUM_CORES);


//     std::cout << "\nIniciando execucao MULTICORE com " << NUM_CORES << " nucleos...\n";

//     for (int i = 0; i < NUM_CORES; ++i) {
//     core_threads.emplace_back(
//         coreWorker,
//         i,                      // coreId
//         std::ref(scheduler),
//         std::ref(memManager),
//         std::ref(ioManager),
//         std::ref(blocked_list),
//         std::ref(blocked_mutex),
//         std::ref(finished_processes),
//         total_processes
//     );
//     }

//     for (auto &t : core_threads) {
//     if (t.joinable()) t.join();
//     }

//     std::cout << "\n=== Todos os processos finalizados. Simulador MULTICORE encerrado. ===\n";

//     return 0;

    
//     /*while (finished_processes < total_processes) {
//         // Verifica desbloqueios de IO
//         for (auto it = blocked_list.begin(); it != blocked_list.end(); ) {
//             if ((*it)->state == State::Ready) { // IOManager liberou
//                 std::cout << "[SO] Processo " << (*it)->pid << " fim de IO -> Scheduler.\n";
//                 scheduler.addProcess(*it); // Devolve pro Scheduler
//                 it = blocked_list.erase(it);
//             } else {
//                 ++it;
//             }
//         }
        

//         PCB* current_process = scheduler.getNextProcess();

//          if (current_process == nullptr) {

//             if (finished_processes < total_processes) {
//                 std::this_thread::sleep_for(std::chrono::milliseconds(10));
//                 continue;
//             } else {
//                 break;
//             }
//         }

        

//         std::cout << "\n[Scheduler] Executando PID " << current_process->pid 
//                   << " (" << current_process->name << ") - Quantum: " << current_process->quantum << "\n";
        
//         current_process->state = State::Running;

//         std::vector<std::unique_ptr<IORequest>> io_requests;
//         bool print_lock = true; // Simulação de lock de print

//         // Execução do core
//         // Na Parte 4 que eu mandei para vocês, isso vai para uma Thread separada
//         // Por enquanto, temos apenas um core rodando sequencialmente
//         Core(memManager, *current_process, &io_requests, print_lock);

//         // Avalia o resultado da execução quando o quantum acaba, decide o que fazer com ele, se for não preemptivo continua rodando
//         switch (current_process->state) {
//             case State::Blocked:
//                 std::cout << "[Scheduler] PID " << current_process->pid << " solicitou I/O -> Bloqueado.\n";
//                 ioManager.registerProcessWaitingForIO(current_process);
//                 blocked_list.push_back(current_process);
//                 break;

//             case State::Finished:
//                 std::cout << "[Scheduler] PID " << current_process->pid << " FINALIZADO.\n";
//                 print_metrics(*current_process);
//                 finished_processes++;
//                 break;

//             default:
//                 if((scheduler.isPreemptive())) {
//                     std::cout << "[Scheduler] PID" << current_process->pid << "fim de quantum (preemptivo) -> Ready Queue.\n";
//                     current_process->state=State::Ready;
//                     scheduler.addProcess(current_process);
//                 }

//                 else{
//                     // Aqui é caso ele for não preemptivo e continua rodando até terminar
//                     std::cout << "[Scheduler] Política não preemptiva: continuando execução do PID "<< current_process->pid << "\n";
//                     current_process->state=State::Running;
//                     scheduler.pushFront(current_process);
//                 }
//                 break;
//         }
//     }

//     std::cout << "\n=== Todos os processos finalizados. Simulador encerrado. ===\n";
//     return 0; */
// }


// Função para imprimir as métricas de um processo (CORRIGIDA PARA MULTICORE)
void print_metrics(const PCB& pcb) {
    std::cout << "\n--- METRICAS FINAIS DO PROCESSO " << pcb.pid << " ---\n";
    std::cout << "Nome do Processo:       " << pcb.name << "\n";
    std::cout << "Estado Final:           " << (pcb.state == State::Finished ? "Finished" : "Incomplete") << "\n";
    std::cout << "Ciclos de Pipeline:     " << pcb.pipeline_cycles.load() << "\n";
    std::cout << "Total de Acessos a Mem: " << pcb.mem_accesses_total.load() << "\n";
    std::cout << "  - Leituras:             " << pcb.mem_reads.load() << "\n";
    std::cout << "  - Escritas:             " << pcb.mem_writes.load() << "\n";
    std::cout << "Acessos a Cache L1:     " << pcb.cache_mem_accesses.load() << "\n";
    std::cout << "Acessos a Mem Principal:" << pcb.primary_mem_accesses.load() << "\n";
    std::cout << "Acessos a Mem Secundaria:" << pcb.secondary_mem_accesses.load() << "\n";
    std::cout << "Ciclos Totais de MemoriA: " << pcb.memory_cycles.load() << "\n";
    std::cout << "------------------------------------------\n";

    std::filesystem::create_directory("output");
    std::ofstream resultados("output/resultados.dat", std::ios::app);
    std::ofstream output("output/output_" + std::to_string(pcb.pid) + ".dat");

    if (resultados.is_open()) {
        resultados << "=== Resultados de Execução (PID: " << pcb.pid << ") ===\n";
        resultados << "Nome: " << pcb.name << "\n";
        resultados << "Quantum: " << pcb.quantum << "\n";
        resultados << "Prioridade: " << pcb.priority << "\n";
        resultados << "Ciclos de Pipeline: " << pcb.pipeline_cycles << "\n";
        resultados << "Ciclos de Memória: " << pcb.memory_cycles << "\n";
        resultados << "Cache Hits: " << pcb.cache_hits << "\n";
        resultados << "Cache Misses: " << pcb.cache_misses << "\n";
        resultados << "Ciclos de IO: " << pcb.io_cycles << "\n";
        resultados << "--------------------------------\n";
    }

    if (output.is_open()) {
        output << "=== Saída Lógica do Programa ===\n";
        output << "Registradores principais:\n";
        output << pcb.regBank.get_registers_as_string() << "\n";
        output << "\n=== Operações Executadas ===\n";

        // CORREÇÃO CRÍTICA DO LOG: Lê apenas o arquivo temporário específico do PID
        std::string temp_filename = "output/temp_" + std::to_string(pcb.pid) + ".log";
        
        // Removemos o fallback para temp_1.log
        if (std::filesystem::exists(temp_filename)) { 
            std::ifstream temp_file(temp_filename);
            if (temp_file.is_open()) {
                std::string line;
                while (std::getline(temp_file, line)) {
                    output << line << "\n";
                }
                temp_file.close();
            }
        } else {
            output << "(Nenhuma operação registrada ou falha de log na UC)\n";
        }
        output << "\n=== Fim das Operações Registradas ===\n";
    }
}

//Funcao dedicada para gerenciar o desbloqueio de IO (Responsabilidade Única)
void ioWorker(Scheduler &scheduler, std::vector<PCB*> &blocked_list, 
              std::mutex &blocked_mutex, std::atomic<int> &finished_processes, 
              const int total_processes) 
{
    std::cout << "[IOM] Thread de IO Iniciada (Responsavel por desbloqueios).\n";
    
    while (finished_processes.load() < total_processes) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));

        // Esta thread é a unica a lidar com a fila de bloqueados (blocked_list)
        std::lock_guard<std::mutex> lock(blocked_mutex);
        
        for (auto it = blocked_list.begin(); it != blocked_list.end(); ) {
            if ((*it)->state == State::Ready) { // IOManager liberou (mudou o estado no IOManager.cpp)
                 std::cout << "[IOM] Processo " << (*it)->pid << " fim de IO -> Scheduler.\n";
                 scheduler.addProcess(*it); // Devolve para a fila de prontos
                 it = blocked_list.erase(it);
            } else {
                ++it;
            }
        }
    }
    std::cout << "[IOM] Thread de IO Finalizada.\n";
}


// coreWorker CORRIGIDO: Foco exclusivo em Escalonamento e Execução
void coreWorker(int coreId, Scheduler& scheduler, MemoryManager& memManager,IOManager& ioManager, std::vector<PCB*>& blocked_list, std::mutex& blocked_mutex,std::atomic<int>& finished_processes, int total_processes){
    
    bool print_lock = true;
    std::vector<std::unique_ptr<IORequest>> io_requests;

    // Loop corrigido: Continua rodando enquanto houver trabalho
    while (finished_processes.load() < total_processes || scheduler.hasProcesses()) {
        
        // O BLOCO DE VERIFICAÇÃO DE I/O FOI REMOVIDO DAQUI
        // A lógica de desbloqueio agora está na thread ioWorker.

        PCB* current_process = scheduler.getNextProcess();

        if(current_process == nullptr){
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }


        std::cout << "\n[Core " << coreId << "][Scheduler] Executando PID "
                      << current_process->pid << " (" << current_process->name
                      << ") - Quantum: " << current_process->quantum << "\n";

        current_process->state = State::Running;
        io_requests.clear();
        
        Core(memManager, *current_process, &io_requests, print_lock);


        // Avalia o resultado (I/O, Finalizado ou Quantum End)
        switch (current_process->state) {
            case State::Blocked:
                std::cout << "[Core " << coreId << "][Scheduler] PID "
                          << current_process->pid << " solicitou I/O -> Bloqueado.\n";
                ioManager.registerProcessWaitingForIO(current_process);
                {
                    std::lock_guard<std::mutex> lock(blocked_mutex);
                    blocked_list.push_back(current_process); // Adiciona na lista para o ioWorker monitorar
                }
                break;

            case State::Finished:
                std::cout << "[Core " << coreId << "][Scheduler] PID "
                          << current_process->pid << " FINALIZADO.\n";
                print_metrics(*current_process);
                finished_processes.fetch_add(1);
                break;

            default:
                if (scheduler.isPreemptive()) {
                    std::cout << "[Core " << coreId << "][Scheduler] PID "
                              << current_process->pid
                              << " fim de quantum (preemptivo) -> Ready Queue.\n";
                    current_process->state = State::Ready;
                    scheduler.addProcess(current_process);
                } else {
                    std::cout << "[Core " << coreId 
                              << "][Scheduler] Política não preemptiva: "
                              << "continuando execução do PID "
                              << current_process->pid << "\n";
                    current_process->state = State::Running;
                    scheduler.pushFront(current_process);
                }
                break;
        }
    }

    std::cout << "[Core " << coreId << "] Finalizando: todos os processos já terminaram ou estao bloqueados.\n";
}


int main() {
    // 1. Inicialização dos Módulos Principais
    std::cout << "=== Inicializando o Simulador Multicore (Fase 1: Limpeza) ===\n";
    MemoryManager memManager(1024, 8192);
    IOManager ioManager;

    // Estruturas de Processos
    int policyOption = 2; // Exemplo: Round-Robin
    SchedulingPolicy policy = SchedulingPolicy::RR;
    switch (policyOption){
        case 0: policy = SchedulingPolicy::FCFS; break;
        case 1: policy = SchedulingPolicy::SJN; break;
        case 2: policy = SchedulingPolicy::RR; break;
        case 3: policy = SchedulingPolicy::Priority; break;

    }

    Scheduler scheduler(policy,SYSTEM_QUANTUM);
    std::vector<std::unique_ptr<PCB>> process_list; // Dono dos ponteiros
    std::vector<PCB*> blocked_list;                 // Fila de bloqueados

    // 2. Carregamento do Lote (BATCH)
    std::ifstream batchFile("batch.json");
    if (!batchFile.is_open()) {
        std::cerr << "Erro fatal: Arquivo 'batch.json' nao encontrado!\n";
        return 1;
    }

    try {
        json batch;
        batchFile >> batch;

        std::cout << "[System] Lendo lote de processos...\n";

        for (const auto& procFile : batch["processes"]) {
            std::string filename = procFile.get<std::string>();
            auto newPcb = std::make_unique<PCB>();

            std::cout << "[System] Carregando arquivo de definição: " << filename << "... ";
            
            if (load_pcb_from_json(filename, *newPcb)) {
                // Definição do Quantum pelo SO
                newPcb->quantum = SYSTEM_QUANTUM;

                // Carrega o programa (código assembly) na memória
                if (!newPcb->program_path.empty()) {
                    loadJsonProgram(newPcb->program_path, memManager, *newPcb, 0);
                    std::cout << "OK! (Programa: " << newPcb->program_path << ")\n";
                    
                    scheduler.addProcess(newPcb.get());
                    process_list.push_back(std::move(newPcb));
                } else {
                    std::cout << "FALHA (program_path vazio)\n";
                }
            } else {
                std::cout << "FALHA (Erro no JSON)\n";
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Erro no parsing do batch.json: " << e.what() << "\n";
        return 1;
    }

    int total_processes = process_list.size();
    if (total_processes == 0) {
        std::cerr << "Nenhum processo carregado. Encerrando.\n";
        return 0;
    }

    std::cout << "\n[System] Total de processos prontos: " << total_processes << "\n";

    // 3. Execução MULTICORE
    std::cout << "\nIniciando execucao MULTICORE com " << NUM_CORES << " nucleos...\n";
    
    // contador atômico compartilhado
    std::atomic<int> finished_processes{0};
    // mutex para lista de bloqueados
    std::mutex blocked_mutex;
    std::vector<std::thread> core_threads;
    core_threads.reserve(NUM_CORES);

    // 1. Inicia a thread dedicada para IO (DESBLOQUEIO)
    std::thread io_thread(ioWorker, 
                          std::ref(scheduler), 
                          std::ref(blocked_list), 
                          std::ref(blocked_mutex), 
                          std::ref(finished_processes), 
                          total_processes);


    // 2. Inicia as threads dos cores
    for (int i = 0; i < NUM_CORES; ++i) {
        core_threads.emplace_back(
            coreWorker,
            i,                      // coreId
            std::ref(scheduler),
            std::ref(memManager),
            std::ref(ioManager),
            std::ref(blocked_list),
            std::ref(blocked_mutex),
            std::ref(finished_processes),
            total_processes
        );
    }

    // 3. Espera por todas as threads
    for (auto &t : core_threads) {
        if (t.joinable()) t.join();
    }
    
    // CRUCIAL: Espera pela thread de IO para garantir que todos os processos se completem
    if (io_thread.joinable()) {
        io_thread.join();
    }


    std::cout << "\n=== Todos os processos finalizados. Simulador MULTICORE encerrado. ===\n";

    return 0;
}