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
#include "nlohmann/json.hpp"

#include "cpu/PCB.hpp"
#include "cpu/pcb_loader.hpp"
#include "cpu/CONTROL_UNIT.hpp"
#include "memory/MemoryManager.hpp"
#include "parser_json/parser_json.hpp"
#include "IO/IOManager.hpp"

using json = nlohmann::json;


/*
    O projeto já implementa o multicore com 4 cores e um quantum de 20 ciclos para cada processo. 
    Já temos o Io Worker para controlar a espera ociosa por I/O.
    A função Core() foi adaptada para ser thread-safe e cada core possui sua própria fila de requisições de I/O.
    Falta agora fazer o swap para quando a memória RAM estiver cheia.

*/
void* Core(MemoryManager &memoryManager, PCB &process, std::vector<std::unique_ptr<IORequest>>* ioRequests, bool &printLock);

const int SYSTEM_QUANTUM = 20; 
const int NUM_CORES=4;


//relógio lógico global da simulação (em "ciclos" de CPU)
std::atomic<uint64_t> g_sim_time{0};

//tempo ocupado de cada core
std::array<std::atomic<uint64_t>, NUM_CORES> g_core_busy{};


// Função para imprimir as métricas de um processo 
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

        std::string temp_filename = "output/temp_" + std::to_string(pcb.pid) + ".log";
        
        
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



void print_system_metrics(const std::vector<std::unique_ptr<PCB>> &process_list,
                          const std::string &policyName)
{
    std::cout << "\n\n===== MÉTRICAS FINAIS DO SISTEMA (" << policyName << ") =====\n";

    uint64_t total_waiting = 0;
    uint64_t total_turnaround = 0;
    uint64_t total_cpu_time = 0;
    uint64_t max_finish_time = 0;

    int process_count = process_list.size();

    // ===== PROCESSO POR PROCESSO =====
    for (const auto &ptr : process_list) {
        const PCB* p = ptr.get();

        uint64_t turnaround = p->finish_time - p->arrival_time;

        total_waiting   += p->waiting_time;
        total_turnaround+= turnaround;
        total_cpu_time  += p->cpu_time;

        if (p->finish_time > max_finish_time)
            max_finish_time = p->finish_time;

        std::cout << "\n--- Processo PID " << p->pid << " ---\n";
        std::cout << "Tempo de espera: " << p->waiting_time << "\n";
        std::cout << "Turnaround:      " << turnaround << "\n";
        std::cout << "CPU Time:        " << p->cpu_time << "\n";
        std::cout << "Início:          " << p->first_start_time << "\n";
        std::cout << "Fim:             " << p->finish_time << "\n";
    }

    // ===== MÉTRICAS AGREGADAS =====
    uint64_t total_core_busy = 0;
    for (int i = 0; i < NUM_CORES; i++)
        total_core_busy += g_core_busy[i].load();

    double avg_waiting    = (double) total_waiting    / process_count;
    double avg_turnaround = (double) total_turnaround / process_count;
    double cpu_util       = (double) total_core_busy  / (max_finish_time * NUM_CORES);
    double throughput     = (double) process_count    / max_finish_time;
    double ideal_time     = (double) total_cpu_time   / NUM_CORES;
    double efficiency     = ideal_time / max_finish_time;

    // ===== PRINT NO CONSOLE =====
    std::cout << "\n======================================\n";
    std::cout << "========= RESUMO DO SISTEMA ==========\n";
    std::cout << "======================================\n\n";

    std::cout << "Número de processos:      " << process_count << "\n";
    std::cout << "Tempo total simulação:    " << max_finish_time << "\n\n";

    std::cout << "Tempo médio de espera:    " << avg_waiting << "\n";
    std::cout << "Turnaround médio:         " << avg_turnaround << "\n\n";

    std::cout << "Utilização média da CPU:  " << cpu_util * 100 << "%\n";
    std::cout << "Throughput global:        " << throughput << " processos/ciclo\n";
    std::cout << "Eficiência:               " << efficiency * 100 << "%\n";

    std::cout << "======================================\n\n";

    // ===== SALVAR EM ARQUIVO =====
    std::filesystem::create_directory("output");

    std::string filename = "../output/metricas_" + policyName + ".dat";
    std::ofstream file(filename);

    if (!file.is_open()) {
        std::cerr << "Erro ao criar arquivo de métricas: " << filename << "\n";
        return;
    }

    // ===== CABEÇALHO DO ARQUIVO =====
    file << "==== MÉTRICAS DA POLÍTICA " << policyName << " ====\n\n";
    file << "Número de processos:      " << process_count << "\n";
    file << "Tempo total simulação:    " << max_finish_time << "\n";
    file << "Tempo médio de espera:    " << avg_waiting << "\n";
    file << "Turnaround médio:         " << avg_turnaround << "\n";
    file << "Utilização média da CPU:  " << cpu_util * 100 << "%\n";
    file << "Throughput global:        " << throughput << " processos/ciclo\n";
    file << "Eficiência:               " << efficiency * 100 << "%\n\n";

    // ===== BLOCO DE PROCESSOS =====
    file << "---- Métricas por processo ----\n";
    for (const auto &ptr : process_list) {
        const PCB* p = ptr.get();
        uint64_t turnaround = p->finish_time - p->arrival_time;

        file << "PID " << p->pid
             << " | Wait=" << p->waiting_time
             << " | Turnaround=" << turnaround
             << " | CPU=" << p->cpu_time
             << " | Start=" << p->first_start_time
             << " | End=" << p->finish_time
             << "\n";
    }

    file.close();

    std::cout << "Arquivo gerado: " << filename << "\n";
}


/*

    Opa, Michel, blz?
    
    A função coreWorker abaixo ela usa std::this_thread::sleep_for(std::chrono::milliseconds(5))
    para evitar espera ociosa que consome CPU desnecessariamente. Nós decidimos não usar nenhuma variável
    de condição para simplificar a implementação e evitar complexidade adicional. Por isso, usamos 
    a função da biblioteca thread para pausar a execução da thread por um curto período. 

*/

//Funcao dedicada para gerenciar o desbloqueio de IO 
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
                 scheduler.addProcess(*it,g_sim_time.load()); // Devolve para a fila de prontos
                 it = blocked_list.erase(it);
            } else {
                ++it;
            }
        }
    }
    std::cout << "[IOM] Thread de IO Finalizada.\n";
}


// coreWorker 
void coreWorker(int coreId, Scheduler& scheduler, MemoryManager& memManager,IOManager& ioManager, std::vector<PCB*>& blocked_list, std::mutex& blocked_mutex,std::atomic<int>& finished_processes, int total_processes){
    
    bool print_lock = true;
    std::vector<std::unique_ptr<IORequest>> io_requests;

    
    while (finished_processes.load() < total_processes || scheduler.hasProcesses()) {
    
        PCB* current_process = scheduler.getNextProcess(g_sim_time.load());

        if(current_process == nullptr){
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }


        std::cout << "\n[Core " << coreId << "][Scheduler] Executando PID "
                      << current_process->pid << " (" << current_process->name
                      << ") - Quantum: " << current_process->quantum << "\n";

        current_process->state = State::Running;
        io_requests.clear();

        // ===== MEDIR CICLOS ANTES DE EXECUTAR ====
        uint64_t before = current_process->pipeline_cycles.load();

        
        Core(memManager, *current_process, &io_requests, print_lock);


        // ===== MEDIR CICLOS APÓS EXECUTAR =====
        uint64_t after = current_process->pipeline_cycles.load();
        uint64_t used = (after > before ? after - before : 0);

        // soma tempo de CPU gasto neste quantum
        current_process->cpu_time += used;

        // soma no core
        g_core_busy[coreId] += used;

        // avança o relógio global
        g_sim_time += used;



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

                current_process->finish_time = g_sim_time.load();

                print_metrics(*current_process);
                finished_processes.fetch_add(1);
                break;

            default:
                if (scheduler.isPreemptive()) {
                    std::cout << "[Core " << coreId << "][Scheduler] PID "
                              << current_process->pid
                              << " fim de quantum (preemptivo) -> Ready Queue.\n";
                    current_process->state = State::Ready;
                    scheduler.addProcess(current_process,g_sim_time.load());
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


void run_simulation_with_policy(SchedulingPolicy policy, const std::string &policyName)
{
    // ====== INÍCIO DO SEU MAIN (copiado exatamente) ======

    std::cout << "=== Inicializando o Simulador Multicore (Fase 1: Limpeza) ===\n";

    // reset de variáveis globais
    g_sim_time.store(0);
    for (int i = 0; i < NUM_CORES; ++i) g_core_busy[i].store(0);

    MemoryManager memManager(192, 8192);
    IOManager ioManager;

    Scheduler scheduler(policy, SYSTEM_QUANTUM);
    std::vector<std::unique_ptr<PCB>> process_list;
    std::vector<PCB*> blocked_list;

    // ======= Carregamento do BATCH =======
    std::ifstream batchFile("batch.json");
    if (!batchFile.is_open()) {
        std::cerr << "Erro fatal: Arquivo 'batch.json' nao encontrado!\n";
        return;
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
                
                newPcb->quantum = SYSTEM_QUANTUM;
                newPcb->arrival_time = 0;
                newPcb->last_ready_in = 0;

                if (!newPcb->program_path.empty()) {
                    
                    loadJsonProgram(newPcb->program_path, memManager, *newPcb, 0);
                    std::cout << "OK! (Programa: " << newPcb->program_path << ")\n";

                    scheduler.addProcess(newPcb.get(), g_sim_time.load());
                    process_list.push_back(std::move(newPcb));
                }
                else {
                    std::cout << "FALHA (program_path vazio)\n";
                }
            }
            else {
                std::cout << "FALHA (Erro no JSON)\n";
            }
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Erro no parsing do batch.json: " << e.what() << "\n";
        return;
    }

    int total_processes = process_list.size();
    if (total_processes == 0) {
        std::cerr << "Nenhum processo carregado. Encerrando.\n";
        return;
    }

    std::cout << "\n[System] Total de processos prontos: " << total_processes << "\n";

    // ======== Execução MULTICORE ========
    std::cout << "\nIniciando execucao MULTICORE com " << NUM_CORES << " nucleos...\n";

    std::atomic<int> finished_processes{0};
    std::mutex blocked_mutex;
    std::vector<std::thread> core_threads;
    core_threads.reserve(NUM_CORES);

    // Thread de IO
    std::thread io_thread(ioWorker,
                          std::ref(scheduler),
                          std::ref(blocked_list),
                          std::ref(blocked_mutex),
                          std::ref(finished_processes),
                          total_processes);

    // Threads dos núcleos
    for (int i = 0; i < NUM_CORES; ++i) {
        core_threads.emplace_back(
            coreWorker,
            i,
            std::ref(scheduler),
            std::ref(memManager),
            std::ref(ioManager),
            std::ref(blocked_list),
            std::ref(blocked_mutex),
            std::ref(finished_processes),
            total_processes
        );
    }

    // Esperar cores
    for (auto &t : core_threads)
        if (t.joinable()) t.join();

    // Esperar IO
    if (io_thread.joinable())
        io_thread.join();

    std::cout << "\n=== Todos os processos finalizados. Politica "
              << policyName << " encerrada. ===\n";

    // ======= CHAMA SUAS MÉTRICAS =======
    print_system_metrics(process_list, policyName);
}



int main() {

    while (true) {

        std::cout << "\n=== MENU DO ESCALONADOR MULTICORE ===\n";
        std::cout << "Escolha a politica de escalonamento:\n";
        std::cout << "0 - FCFS\n";
        std::cout << "1 - SJN\n";
        std::cout << "2 - Round Robin\n";
        std::cout << "3 - Priority\n";
        std::cout << "9 - Sair\n";
        std::cout << "Opcao: ";

        int opcao;
        std::cin >> opcao;

        if (opcao == 9) {
            std::cout << "Encerrando simulador.\n";
            return 0;
        }

        switch (opcao) {
            case 0:
                run_simulation_with_policy(SchedulingPolicy::FCFS, "FCFS");
                break;

            case 1:
                run_simulation_with_policy(SchedulingPolicy::SJN, "SJN");
                break;

            case 2:
                run_simulation_with_policy(SchedulingPolicy::RR, "RR");
                break;

            case 3:
                run_simulation_with_policy(SchedulingPolicy::Priority, "PRIORITY");
                break;

            default:
                std::cout << "Opcao invalida! Tente novamente.\n";
                continue;
        }

        std::cout << "\nDeseja testar outra politica? (1 = sim, 0 = sair): ";
        int again;
        std::cin >> again;

        if (again == 0) {
            std::cout << "Saindo.\n";
            return 0;
        }
    }

    return 0;
}

