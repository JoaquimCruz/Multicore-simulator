/*
  test_memory.cpp
  Arquivo de teste para a memória principal (RAM) e secundária (Disco).

  Como funciona:
  1.  Testa a escrita e leitura na memória principal com valores diferentes.
  2.  Testa a escrita e leitura na memória secundária (assumindo um endereço alto).
  3.  Usa registradores para sinalizar o sucesso de cada teste.
      - $t5 = 1 se o teste da RAM passou.
      - $t6 = 1 se o teste do Disco passou.
  4.  Ao final, o simulador pode inspecionar os valores de $t5 e $t6 para
      verificar se as memórias estão funcionando corretamente.
*/
#include <iostream>
#include <vector>
#include <memory>
#include <cstdint>

// Headers do projeto
#include "cpu/pcb_loader.hpp"
#include "cpu/PCB.hpp"
#include "cpu/CONTROL_UNIT.hpp"
#include "memory/MAIN_MEMORY.hpp" // Incluindo MAIN_MEMORY
//#include "ioRequest.hpp" 

// Sentinel de fim de programa (o mesmo de CONTROL_UNIT.cpp)
static constexpr uint32_t END_SENTINEL = 0b11111100000000000000000000000000u;

// --- Helpers para Montagem de Instruções (do test_cpu_metrics.cpp) ---
// R-type (add, sub, etc.)
static uint32_t makeR(uint8_t rs, uint8_t rt, uint8_t rd, uint8_t funct) {
    return (0u << 26) | (static_cast<uint32_t>(rs) << 21) | (static_cast<uint32_t>(rt) << 16) |
           (static_cast<uint32_t>(rd) << 11) | (0u << 6) | (funct & 0x3Fu);
}
// I-type (lw, sw, li, beq, etc.)
static uint32_t makeI(uint8_t opcode, uint8_t rs, uint8_t rt, uint16_t imm) {
    return (static_cast<uint32_t>(opcode & 0x3F) << 26) | (static_cast<uint32_t>(rs) << 21) |
           (static_cast<uint32_t>(rt) << 16) | imm;
}

int main() {
    // --- Configuração Inicial ---
    PCB pcb{};
    load_pcb_from_json("process1.json", pcb); // Carrega configurações do processo

    // Usa a implementação de MAIN_MEMORY do seu projeto
    // O construtor espera um tamanho, vamos definir um tamanho padrão.
    MAIN_MEMORY ram(1024); 

    // Mapeamento de registradores (baseado em HASH_REGISTER.hpp e test_cpu_metrics.cpp)
    // t0=8, t1=9, t2=10, t3=11, t4=12, t5=13, t6=14
    uint8_t r_zero = 0;
    uint8_t r_t0 = 8;  // Para o valor 0xFFFFFFFF
    uint8_t r_t1 = 9;  // Para o valor 42
    uint8_t r_t2 = 10; // Para ler de volta da RAM
    uint8_t r_t3 = 11; // Para o valor 1337 (disco)
    uint8_t r_t4 = 12; // Para ler de volta do Disco
    uint8_t r_t5 = 13; // Sinalizador de sucesso da RAM (1 = sucesso)
    uint8_t r_t6 = 14; // Sinalizador de sucesso do Disco (1 = sucesso)

    // Opcodes baseados no seu CONTROL_UNIT.hpp
    const uint8_t op_li = 0x0E; // 001110
    const uint8_t op_sw = 0x0D; // 001101
    const uint8_t op_lw = 0x0C; // 001100
    const uint8_t op_beq = 0x05; // 000101
    const uint8_t op_addi = 0x08; // Não está no mapa, mas é padrão MIPS (usado para `li`)
                                  // No seu caso, `li` tem um opcode próprio.

    // Endereços de memória para os testes
    const uint16_t ram_addr1 = 100;
    const uint16_t ram_addr2 = 104;
    const uint16_t disk_addr = 1024; // Endereço alto para simular disco

    // --- Programa Assembly de Teste ---
    // Será escrito na memória instrução por instrução.
    uint32_t program[] = {
        // --- Teste 1: Memória Principal (RAM) - Escrita/Leitura Simples ---
        /* 0*/ makeI(op_li, r_zero, r_t1, 42),          // LI $t1, 42
        /* 1*/ makeI(op_sw, r_t1, r_zero, ram_addr1),   // SW $t1, 100($zero)
        /* 2*/ makeI(op_lw, r_zero, r_t2, ram_addr1),   // LW $t2, 100($zero)
        
        // --- Teste 2: Memória Principal (RAM) - Padrão de Bits ---
        /* 3*/ makeI(op_li, r_zero, r_t0, 0xFFFF),      // LI $t0, -1 (que vira 0xFFFFFFFF)
        /* 4*/ makeI(op_sw, r_t0, r_zero, ram_addr2),   // SW $t0, 104($zero)
        /* 5*/ makeI(op_lw, r_zero, r_t0, ram_addr2),   // LW $t0, 104($zero) (lê de volta para o mesmo reg)

        // --- Verificação RAM ---
        // Se $t1 == $t2 E $t0 tiver o valor -1, o teste passou.
        // BEQ pula 2 instruções se $t1 != $t2 (falha)
        /* 6*/ makeI(op_beq, r_t1, r_t2, 2),            // BEQ $t1, $t2, PULA_FALHA_RAM
        /* 7*/ makeI(op_li, r_zero, r_t5, 1),           // LI $t5, 1 (SUCESSO RAM!)
        /* 8*/ makeI(0x0B, 0, 0, 10),                   // JUMP para o teste de disco
        /* 9*/ makeI(op_li, r_zero, r_t5, 0),           // FALHA_RAM: LI $t5, 0

        // --- Teste 3: Memória Secundária (Disco) ---
        /*10*/ makeI(op_li, r_zero, r_t3, 1337),        // LI $t3, 1337
        /*11*/ makeI(op_sw, r_t3, r_zero, disk_addr),   // SW $t3, 1024($zero)
        /*12*/ makeI(op_lw, r_zero, r_t4, disk_addr),   // LW $t4, 1024($zero)
        
        // --- Verificação Disco ---
        /*13*/ makeI(op_beq, r_t3, r_t4, 2),            // BEQ $t3, $t4, PULA_FALHA_DISCO
        /*14*/ makeI(op_li, r_zero, r_t6, 1),           // LI $t6, 1 (SUCESSO DISCO!)
        /*15*/ makeI(0x0B, 0, 0, 17),                   // JUMP para o fim
        /*16*/ makeI(op_li, r_zero, r_t6, 0),           // FALHA_DISCO: LI $t6, 0

        // --- Fim do Programa ---
        /*17*/ END_SENTINEL
    };

    // Escreve o programa na memória
    for (size_t i = 0; i < sizeof(program) / sizeof(uint32_t); ++i) {
        ram.WriteMem(i, program[i]);
    }

    // Inicializa os endereços de memória para garantir que não contenham lixo
    ram.WriteMem(ram_addr1, 0);
    ram.WriteMem(ram_addr2, 0);
    ram.WriteMem(disk_addr, 0);

    // --- Execução ---
    std::vector<std::unique_ptr<ioRequest>> ioRequests;
    bool printLock = false;
    Core(ram, pcb, &ioRequests, printLock); // Executa o simulador

    // --- Verificação dos Resultados ---
    std::cout << "========================================\n";
    std::cout << "======= RESULTADOS DO TESTE DE MEMORIA =======\n";
    std::cout << "========================================\n";

    uint32_t ram_success_flag = pcb.regBank.readRegister("t5");
    uint32_t disk_success_flag = pcb.regBank.readRegister("t6");

    std::cout << "Flag de Sucesso da RAM ($t5): " << ram_success_flag << "\n";
    if (ram_success_flag == 1) {
        std::cout << "  -> ✅ SUCESSO: A memoria principal passou no teste.\n";
    } else {
        std::cout << "  -> ❌ FALHA: A memoria principal falhou no teste.\n";
    }

    std::cout << "\nFlag de Sucesso do Disco ($t6): " << disk_success_flag << "\n";
    if (disk_success_flag == 1) {
        std::cout << "  -> ✅ SUCESSO: A memoria secundaria passou no teste.\n";
    } else {
        std::cout << "  -> ❌ FALHA: A memoria secundaria falhou no teste.\n";
    }
    
    std::cout << "========================================\n";
    std::cout << "Valores Finais dos Registradores de Teste:\n";
    std::cout << "$t1 (valor escrito RAM): " << pcb.regBank.readRegister("t1") << " (esperado: 42)\n";
    std::cout << "$t2 (valor lido RAM):    " << pcb.regBank.readRegister("t2") << " (esperado: 42)\n";
    std::cout << "$t3 (valor escrito Disco): " << pcb.regBank.readRegister("t3") << " (esperado: 1337)\n";
    std::cout << "$t4 (valor lido Disco):    " << pcb.regBank.readRegister("t4") << " (esperado: 1337)\n";
    std::cout << "========================================\n";

    return 0;
}