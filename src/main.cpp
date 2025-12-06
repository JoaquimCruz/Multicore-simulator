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
namespace fs = std::filesystem;

void* Core(MemoryManager &memoryManager, PCB &process, std::vector<std::unique_ptr<IORequest>>* ioRequests, bool &printLock);

const int SYSTEM_QUANTUM = 20; 
const int NUM_CORES=4;


// Cada núcleo tem seu próprio tempo. O tempo global é o máximo entre eles.
std::array<std::atomic<uint64_t>, NUM_CORES> g_core_clock{}; 
std::array<std::atomic<uint64_t>, NUM_CORES> g_core_busy{};

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

    fs::create_directories("output/resultados");
    fs::create_directories("output/trace_logs");
    
    std::ofstream resultados("output/resultados/resultados.dat", std::ios::app);
    std::ofstream output("output/resultados/output_" + std::to_string(pcb.pid) + ".dat");

    if (resultados.is_open()) {
        resultados << "=== Resultados de Execução (PID: " << pcb.pid << ") ===\n";
        resultados << "Nome: " << pcb.name << "\n";
        resultados << "Quantum: " << pcb.quantum << "\n";
        resultados << "Prioridade: " << pcb.priority << "\n";
        resultados << "Ciclos de Pipeline: " << pcb.pipeline_cycles << "\n";
        resultados << "Ciclos de Memória: " << pcb.memory_cycles << "\n";
        resultados << "Cache Hits: " << pcb.cache_hits << "\n";
        resultados << "Cache Misses: " << pcb.cache_misses << "\n";
       // resultados << "Ciclos de IO: " << pcb.io_cycles << "\n";
        resultados << "--------------------------------\n";
    }

    if (output.is_open()) {
        output << "=== Saída Lógica do Programa ===\n";
        output << "Registradores principais:\n";
        output << pcb.regBank.get_registers_as_string() << "\n";
        output << "\n=== Operações Executadas ===\n";

        std::string temp_filename = "output/trace_logs/temp_" + std::to_string(pcb.pid) + ".log";
        
        if (fs::exists(temp_filename)) { 
            std::ifstream temp_file(temp_filename);
            if (temp_file.is_open()) {
                output << temp_file.rdbuf();
                temp_file.close();
            }
        } else {
            output << "(Nenhuma operação registrada ou falha de log na UC)\n";
        }
        output << "\n=== Fim das Operações Registradas ===\n";
    }
}

void print_system_metrics(const std::vector<std::unique_ptr<PCB>> &process_list, const std::string &policyName)
{
    std::cout << "\n\n===== MÉTRICAS FINAIS DO SISTEMA (" << policyName << ") =====\n";

    uint64_t total_waiting = 0;
    uint64_t total_turnaround = 0;
    uint64_t total_cpu_time = 0;
    uint64_t max_finish_time = 0;

    int process_count = process_list.size();

    // Encontrar o maior tempo de término real
    for (const auto &ptr : process_list) {
        const PCB* p = ptr.get();
        
        uint64_t turnaround = p->finish_time - p->arrival_time;
        
        
        // Wait = Turnaround - CPU - IO
        int64_t derived_wait = turnaround - p->cpu_time - p->io_cycles;
        if (derived_wait < 0) derived_wait = 0; // Proteção contra clocks muito desajustados

        total_waiting   += derived_wait; // Usamos o valor corrigido
        total_turnaround+= turnaround;
        total_cpu_time  += p->cpu_time;

        if (p->finish_time > max_finish_time)
            max_finish_time = p->finish_time;

        std::cout << "\n--- Processo PID " << p->pid << " ---\n";
        std::cout << "Tempo de espera: " << derived_wait << " (Corrigido)\n";
        std::cout << "Turnaround:      " << turnaround << "\n";
        std::cout << "CPU Time:        " << p->cpu_time << "\n";
        std::cout << "IO Time:         " << p->io_cycles << "\n";
        std::cout << "Fim:             " << p->finish_time << "\n";
    }

    
    if (max_finish_time == 0) {
        for(int i=0; i<NUM_CORES; i++) {
            if (g_core_clock[i].load() > max_finish_time) 
                max_finish_time = g_core_clock[i].load();
        }
    }

    uint64_t total_core_busy = 0;
    for (int i = 0; i < NUM_CORES; i++)
        total_core_busy += g_core_busy[i].load();

    double avg_waiting    = (double) total_waiting    / process_count;
    double avg_turnaround = (double) total_turnaround / process_count;
    double cpu_util       = (max_finish_time > 0) ? (double) total_core_busy  / (max_finish_time * NUM_CORES) : 0;
    double throughput     = (max_finish_time > 0) ? (double) process_count    / max_finish_time : 0;
    double ideal_time     = (double) total_cpu_time   / NUM_CORES;
    double efficiency     = (max_finish_time > 0) ? ideal_time / max_finish_time : 0;

    // Prints no Console
    std::cout << "\n======================================\n";
    std::cout << "========= RESUMO DO SISTEMA ==========\n";
    std::cout << "======================================\n";
    std::cout << "Tempo total simulação:    " << max_finish_time << "\n";
    std::cout << "Tempo médio de espera:    " << avg_waiting << "\n";
    std::cout << "Turnaround médio:         " << avg_turnaround << "\n";
    std::cout << "Utilização média da CPU:  " << cpu_util * 100 << "%\n";
    std::cout << "Throughput global:        " << throughput << "\n";
    std::cout << "Eficiência:               " << efficiency * 100 << "%\n";
    std::cout << "======================================\n\n";
    
    // Escrita no Arquivo
    fs::create_directories("output/metricas");
    std::string filename = "output/metricas/metricas_" + policyName + ".dat";
    std::ofstream file(filename);

    if (!file.is_open()) {
        std::cerr << "Erro arquivo: " << filename << "\n";
        return;
    }

    file << "==== MÉTRICAS DA POLÍTICA " << policyName << " ====\n\n";
    file << "Tempo total simulação:    " << max_finish_time << "\n";
    file << "Utilização média da CPU:  " << cpu_util * 100 << "%\n";
    file << "Throughput global:        " << throughput << "\n\n";
    
    file << "---- Métricas por processo ----\n";
    for (const auto &ptr : process_list) {
        const PCB* p = ptr.get();
        uint64_t turnaround = p->finish_time - p->arrival_time;
        // Recalcula pro arquivo também
        int64_t derived_wait = turnaround - p->cpu_time - p->io_cycles;
        if (derived_wait < 0) derived_wait = 0;

        file << "PID " << p->pid 
             << " | Wait=" << derived_wait
             << " | Turnaround=" << turnaround 
             << " | CPU=" << p->cpu_time 
             << "\n";
    }
    file.close();
    std::cout << "Arquivo gerado: " << filename << "\n";
}

void ioWorker(Scheduler &scheduler, std::vector<PCB*> &blocked_list, 
              std::mutex &blocked_mutex, std::atomic<int> &finished_processes, 
              const int total_processes) 
{
    std::cout << "[IOM] Thread de IO Iniciada.\n";
    while (finished_processes.load() < total_processes) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        std::lock_guard<std::mutex> lock(blocked_mutex);
        for (auto it = blocked_list.begin(); it != blocked_list.end(); ) {
            if ((*it)->state == State::Ready) { 
                 // Reinsere com prioridade ou tempo 0 (ajuste conforme sua politica)
                 scheduler.addProcess(*it, 0); 
                 it = blocked_list.erase(it);
            } else {
                ++it;
            }
        }
    }
}

void coreWorker(int coreId, Scheduler& scheduler, MemoryManager& memManager, IOManager& ioManager, 
                std::vector<PCB*>& blocked_list, std::mutex& blocked_mutex, 
                std::atomic<int>& finished_processes, int total_processes)
{
    bool print_lock = true;
    std::vector<std::unique_ptr<IORequest>> io_requests;

    while (finished_processes.load() < total_processes || scheduler.hasProcesses()) {
    
        // CORREÇÃO: Usa o relógio DESTE core para pedir processo
        PCB* current_process = scheduler.getNextProcess(g_core_clock[coreId].load());

        if(current_process == nullptr){
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        std::cout << "\n[Core " << coreId << "] Executando PID " << current_process->pid << "\n";

        current_process->state = State::Running;
        io_requests.clear();

        // Medição de ciclos
        uint64_t before = current_process->pipeline_cycles.load();
        Core(memManager, *current_process, &io_requests, print_lock);
        uint64_t after = current_process->pipeline_cycles.load();
        
        uint64_t used = (after > before ? after - before : 0);
        
        current_process->cpu_time += used;
        
        
        g_core_busy[coreId] += used;
        g_core_clock[coreId] += used; 

        switch (current_process->state) {
            case State::Blocked:
                ioManager.registerProcessWaitingForIO(current_process);
                {
                    std::lock_guard<std::mutex> lock(blocked_mutex);
                    blocked_list.push_back(current_process);
                }
                break;

            case State::Finished:
                // Salva o tempo de término baseado no relógio deste core
                current_process->finish_time = g_core_clock[coreId].load();
                
                std::cout << "[Core " << coreId << "] PID " << current_process->pid 
                          << " FINALIZADO em T=" << current_process->finish_time << "\n";
                          
                print_metrics(*current_process);
                finished_processes.fetch_add(1);
                break;

            default: // Preemptado
                if (scheduler.isPreemptive()) {
                    current_process->state = State::Ready;
                    // Devolve com o tempo atual deste core
                    scheduler.addProcess(current_process, g_core_clock[coreId].load());
                } else {
                    current_process->state = State::Running;
                    scheduler.pushFront(current_process);
                }
                break;
        }
    }
}

void run_simulation_with_policy(SchedulingPolicy policy, const std::string &policyName)
{
    std::cout << "=== Inicializando o Simulador (" << policyName << ") - Fase 1: Limpeza ===\n";

    // Limpeza robusta de logs antigos
    try {
        if (fs::exists("output/trace_logs")) {
            for (const auto& entry : fs::directory_iterator("output/trace_logs")) fs::remove_all(entry.path());
        } else {
            fs::create_directories("output/trace_logs");
        }
        if (fs::exists("output/resultados")) {
            for (const auto& entry : fs::directory_iterator("output/resultados")) fs::remove_all(entry.path());
        } else {
            fs::create_directories("output/resultados");
        }
        if (fs::exists("output/resultados.dat")) fs::remove("output/resultados.dat");
    } catch (const std::exception& e) {
        std::cerr << "Aviso limpeza: " << e.what() << "\n";
    }

    // Reset dos relógios globais
    for (int i = 0; i < NUM_CORES; ++i) {
        g_core_clock[i].store(0);
        g_core_busy[i].store(0);
    }

    // Defina o tamanho da memória aqui (ex: 320, 512, 1024)
    MemoryManager memManager(512, 8192); 
    IOManager ioManager;
    Scheduler scheduler(policy, SYSTEM_QUANTUM);
    std::vector<std::unique_ptr<PCB>> process_list;
    std::vector<PCB*> blocked_list;

    std::ifstream batchFile("batch.json");
    if (!batchFile.is_open()) return;

    try {
        json batch;
        batchFile >> batch;
        for (const auto& procFile : batch["processes"]) {
            std::string filename = procFile.get<std::string>();
            auto newPcb = std::make_unique<PCB>();
            if (load_pcb_from_json(filename, *newPcb)) {
                newPcb->quantum = SYSTEM_QUANTUM;
                newPcb->arrival_time = 0;
                if (!newPcb->program_path.empty()) {
                    loadJsonProgram(newPcb->program_path, memManager, *newPcb, 0);
                    // Chegada no tempo 0
                    scheduler.addProcess(newPcb.get(), 0);
                    process_list.push_back(std::move(newPcb));
                }
            }
        }
    } catch (...) {}

    int total_processes = process_list.size();
    if (total_processes == 0) return;

    std::atomic<int> finished_processes{0};
    std::mutex blocked_mutex;
    std::vector<std::thread> core_threads;
    core_threads.reserve(NUM_CORES);

    std::thread io_thread(ioWorker, std::ref(scheduler), std::ref(blocked_list), std::ref(blocked_mutex), std::ref(finished_processes), total_processes);

    for (int i = 0; i < NUM_CORES; ++i) {
        core_threads.emplace_back(coreWorker, i, std::ref(scheduler), std::ref(memManager), std::ref(ioManager), std::ref(blocked_list), std::ref(blocked_mutex), std::ref(finished_processes), total_processes);
    }

    for (auto &t : core_threads) if (t.joinable()) t.join();
    if (io_thread.joinable()) io_thread.join();

    std::cout << "\n=== Simulador Encerrado ===\n";
    print_system_metrics(process_list, policyName);
}

int main() {
    while (true) {
        std::cout << "\n=== MENU DO ESCALONADOR MULTICORE ===\n";
        std::cout << "0 - FCFS\n1 - SJN\n2 - Round Robin\n3 - Priority\n9 - Sair\nOpcao: ";
        int opcao;
        if (!(std::cin >> opcao)) {
            std::cin.clear(); std::cin.ignore(10000, '\n'); continue;
        }
        if (opcao == 9) break;
        switch (opcao) {
            case 0: run_simulation_with_policy(SchedulingPolicy::FCFS, "FCFS"); break;
            case 1: run_simulation_with_policy(SchedulingPolicy::SJN, "SJN"); break;
            case 2: run_simulation_with_policy(SchedulingPolicy::RR, "RR"); break;
            case 3: run_simulation_with_policy(SchedulingPolicy::Priority, "PRIORITY"); break;
            default: std::cout << "Opcao invalida!\n"; continue;
        }
    }
    return 0;
}