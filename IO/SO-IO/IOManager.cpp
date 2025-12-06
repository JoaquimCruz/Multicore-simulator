#include "IOManager.hpp"
#include <iostream>
#include <chrono>
#include <fstream>
#include <random> // Nova biblioteca
#include <filesystem>

// Variáveis de aleatoriedade movidas para escopo local/classe ou estático seguro
static std::mt19937 rng(std::random_device{}()); 

IOManager::IOManager() :
    printer_requesting(false),
    disk_requesting(false),
    network_requesting(false),
    shutdown_flag(false)
{
    // Removido srand(time(nullptr)); 

    // Garante que o diretório output exista antes de abrir arquivos
    std::filesystem::create_directories("output");
    
    resultFile.open("output/result.dat", std::ios::app);
    outputFile.open("output/io_metrics.dat", std::ios::app);

    if (!resultFile || !outputFile) {
        std::cerr << "Erro: não foi possível abrir arquivos de saída de IO." << std::endl;
    }

    managerThread = std::thread(&IOManager::managerLoop, this);
}

IOManager::~IOManager() {
    shutdown_flag = true;
    if (managerThread.joinable()) {
        managerThread.join();
    }
    if (resultFile.is_open()) resultFile.close();
    if (outputFile.is_open()) outputFile.close();
}

void IOManager::registerProcessWaitingForIO(PCB* process) {
    std::lock_guard<std::mutex> lock(waiting_processes_lock);
    waiting_processes.push_back(process);
}

void IOManager::addRequest(std::unique_ptr<IORequest> request) {
    std::lock_guard<std::mutex> lock(queueLock);
    requests.push_back(std::move(request));
}

void IOManager::managerLoop() {
    // Distribuições uniformes para substituir o rand() % N
    std::uniform_int_distribution<int> dist100(0, 99);
    std::uniform_int_distribution<int> dist50(0, 49);
    std::uniform_int_distribution<int> distCycle(1, 3);

    while (!shutdown_flag) {
        // ETAPA 1: Simula solicitações de dispositivos
        {
            std::lock_guard<std::mutex> lock(device_state_lock);
            if (dist100(rng) == 0) { // 1% de chance
                if (!printer_requesting) printer_requesting = true;
            }
            if (dist50(rng) == 0) { // 2% de chance
                if (!disk_requesting) disk_requesting = true;
            }
        }

        // ETAPA 2: Verifica pares Processo <-> Dispositivo
        std::unique_ptr<IORequest> new_request = nullptr;
        {
            std::lock_guard<std::mutex> wplock(waiting_processes_lock);
            if (!waiting_processes.empty()) {
                std::lock_guard<std::mutex> dslock(device_state_lock);
                
                // Prioriza quem chegou primeiro (FIFO na espera)
                PCB* process_to_service = waiting_processes.front();

                if (printer_requesting) {
                    new_request = std::make_unique<IORequest>();
                    new_request->operation = "print_job";
                    new_request->msg = "Imprimindo documento...";
                    printer_requesting = false;
                } else if (disk_requesting) {
                    new_request = std::make_unique<IORequest>();
                    new_request->operation = "read_from_disk";
                    new_request->msg = "Lendo dados do disco...";
                    disk_requesting = false;
                }
                
                if (new_request) {
                    new_request->process = process_to_service;
                    waiting_processes.erase(waiting_processes.begin());
                    // Custo entre 100ms e 300ms
                    new_request->cost_cycles = std::chrono::milliseconds(distCycle(rng) * 100);
                }
            }
        }
        
        if (new_request) {
            addRequest(std::move(new_request));
        }

        // ETAPA 3: Processa a fila de I/O (Serial)
        std::unique_ptr<IORequest> req_to_process = nullptr;
        {
            std::lock_guard<std::mutex> lock(queueLock);
            if (!requests.empty()) {
                req_to_process = std::move(requests.front());
                requests.erase(requests.begin());
            }
        }

        if (req_to_process) {
            auto start = std::chrono::steady_clock::now();
            std::this_thread::sleep_for(req_to_process->cost_cycles);
            auto end = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

            req_to_process->process->io_cycles.fetch_add(duration);

            if (resultFile.is_open())
                resultFile << "PID " << req_to_process->process->pid << " : " << req_to_process->msg << "\n";
            
            
            req_to_process->process->state = State::Ready;
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    }
}