#ifndef PARSER_JSON_HPP
#define PARSER_JSON_HPP

#include <string>
#include <nlohmann/json.hpp>
#include "../memory/MemoryManager.hpp" // Inclui o MemoryManager correto
#include "../cpu/PCB.hpp" 

// Carrega um programa JSON completo para a memória usando o MemoryManager
int loadJsonProgram(const std::string &filename, MemoryManager &memManager, PCB &pcb, int startAddr);

// Faz o parsing da seção de dados
int parseData(const nlohmann::json &dataJson, MemoryManager &memManager, PCB &pcb, int startAddr);

// Faz o parsing das instruções
int parseProgram(const nlohmann::json &programJson, MemoryManager &memManager, PCB &pcb, int startAddr);

// Função auxiliar (pode ser usada externamente se necessário)
uint32_t parseInstruction(const nlohmann::json &instrJson, int currentInstrIndex, int startAddr);

#endif // PARSER_JSON_HPP