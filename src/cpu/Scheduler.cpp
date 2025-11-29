#include "Scheduler.hpp"

Scheduler::Scheduler(SchedulingPolicy initialPolicy, int quantum)
    : policy(initialPolicy), timeSlice(quantum) {}

void Scheduler::addProcess(PCB* process) {
    std::lock_guard<std::mutex> lock(queueMutex);
    
    // Define o estado como Ready
    process->state = State::Ready;

    // Em Round Robin e FCFS, apenas adiciona ao final
    ready_queue.push_back(process);

    // Se for Prioridade ou SJN, reordena a fila
    if (policy == SchedulingPolicy::Priority || policy == SchedulingPolicy::SJN) {
        sortQueue();
    }
}

PCB* Scheduler::getNextProcess() {
    std::lock_guard<std::mutex> lock(queueMutex);

    if (ready_queue.empty()) {
        return nullptr;
    }

    PCB* next = ready_queue.front();
    ready_queue.pop_front();
    
    return next;
}

bool Scheduler::hasProcesses() {
    std::lock_guard<std::mutex> lock(queueMutex);
    return !ready_queue.empty();
}

void Scheduler::setPolicy(SchedulingPolicy newPolicy) {
    std::lock_guard<std::mutex> lock(queueMutex);
    policy = newPolicy;
    // Reordena imediatamente se mudarmos para uma política que exige ordem
    if (policy == SchedulingPolicy::Priority || policy == SchedulingPolicy::SJN) {
        sortQueue();
    }
}

void Scheduler::sortQueue() {
    // Lógica de Ordenação
    if (policy == SchedulingPolicy::Priority) {
        // Ordena por prioridade (Maior valor = Maior prioridade)
        std::sort(ready_queue.begin(), ready_queue.end(), 
            [](PCB* a, PCB* b) {
                return a->priority > b->priority; 
            });
    } 
    else if (policy == SchedulingPolicy::SJN) {
        // Shortest Job Next: Ordena por tamanho do programa (aprox.)
        // Usamos mem_writes/reads como estimativa ou o PC final se soubessemos
        // Por enquanto, vamos ordenar por quem tem menos ciclos executados (simplificação)
        // Ou pelo tamanho do tasks.json se tivéssemos essa métrica fácil.
        // Vamos manter a ordem de prioridade como placeholder para SJN por enquanto.
        std::sort(ready_queue.begin(), ready_queue.end(), 
            [](PCB* a, PCB* b) {
                return a->burst_time < b->burst_time; 
            });
    }
}


//Verifica se a política é preemptiva - RR e Prioruty são preemptivos, mas FCFS não
bool Scheduler::isPreemptive() const {
    return (policy == SchedulingPolicy::RR);
}

void Scheduler::pushFront(PCB* process) {
    std::lock_guard<std::mutex> lock(queueMutex);
    ready_queue.push_front(process);
}
