#ifndef SCHEDULER_HPP
#define SCHEDULER_HPP

#include <deque>
#include <vector>
#include <mutex>
#include <algorithm>
#include <iostream>
#include "PCB.hpp"


enum class SchedulingPolicy {
    FCFS,       // First Come, First Served
    SJN,        // Shortest Job Next
    RR,         // Round Robin (Com Quantum e sem preempção)
    Priority    // Prioridade
};

class Scheduler {
private:
    std::deque<PCB*> ready_queue; // Fila de processamento
    std::mutex queueMutex;        // Proteção para Multicore
    SchedulingPolicy policy;
    int timeSlice;                // Quantum do sistema

    // Auxiliar para ordenar a fila (usado em SJN e Prioridade)
    void sortQueue();

public:
    Scheduler(SchedulingPolicy initialPolicy = SchedulingPolicy::RR, int quantum = 20);

    // Adiciona um processo à fila (Thread-Safe)
    void addProcess(PCB* process);

    // Retorna o próximo processo a ser executado (Thread-Safe)
    // Retorna nullptr se a fila estiver vazia
    PCB* getNextProcess();

    // Verifica se há processos prontos
    bool hasProcesses();

    // Define a política de escalonamento dinamicamente
    void setPolicy(SchedulingPolicy newPolicy);
};

#endif 