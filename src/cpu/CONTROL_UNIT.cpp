#include "pcb_loader.hpp"
#include <fstream>
#include "CONTROL_UNIT.hpp"
#include "../memory/MemoryManager.hpp"
#include "PCB.hpp"
#include "../IO/IOManager.hpp"
#include <bitset>
#include <cmath>
#include <stdexcept>
#include <iostream>
#include <algorithm>
#include <sstream>
#include <vector>
#include <fstream>
#include <mutex>

static std::mutex log_mutex;
using namespace std;



//Implementação thread-safe de log_operation (Recebe e usa o PID)
void Control_Unit::log_operation(const std::string &msg, int pid) {
    std::lock_guard<std::mutex> lock(log_mutex);
    
    // Usa o PID real para criar um arquivo de log único por processo
    std::ostringstream oss;
    oss << "output/trace_logs/temp_" << pid << ".log"; 

    std::ofstream fout(oss.str(), std::ios::app);
    if (fout.is_open()) {
        fout << msg << " [PID:" << pid << "]\n";
    }
}

static uint32_t binaryStringToUint(const std::string &bin) {
    if (bin.empty()) return 0;
    uint32_t value = 0;
    for (char c : bin) {
        value <<= 1;
        if (c == '1') value |= 1u;
        else if (c != '0') throw std::invalid_argument("binary");
    }
    return value;
}

static int32_t signExtend16(uint16_t v) {
    if (v & 0x8000) return (int32_t)(0xFFFF0000u | v);
    else return (int32_t)(v & 0x0000FFFFu);
}

static std::string regIndexToBitString(uint32_t idx) {
    std::string s(5, '0');
    for (int i = 4; i >= 0; --i) s[4 - i] = ((idx >> i) & 1) ? '1' : '0';
    return s;
}

static std::string toBinStr(uint32_t v, int width) {
    std::string s(width, '0');
    for (int i = 0; i < width; ++i) s[width - 1 - i] = ((v >> i) & 1) ? '1' : '0';
    return s;
}

static inline void account_pipeline_cycle(PCB &p) { p.pipeline_cycles.fetch_add(1); }
static inline void account_stage(PCB &p) { p.stage_invocations.fetch_add(1); }

string Control_Unit::Get_immediate(const uint32_t instruction) {
    uint16_t imm = static_cast<uint16_t>(instruction & 0xFFFFu);
    return std::bitset<16>(imm).to_string();
}

string Control_Unit::Get_destination_Register(const uint32_t instruction) {
    uint32_t rd = (instruction >> 11) & 0x1Fu;
    return regIndexToBitString(rd);
}

string Control_Unit::Get_target_Register(const uint32_t instruction) {
    uint32_t rt = (instruction >> 16) & 0x1Fu;
    return regIndexToBitString(rt);
}

string Control_Unit::Get_source_Register(const uint32_t instruction) {
    uint32_t rs = (instruction >> 21) & 0x1Fu;
    return regIndexToBitString(rs);
}

string Control_Unit::Identificacao_instrucao(uint32_t instruction, hw::REGISTER_BANK &registers) {
    (void)registers; 
    uint32_t opcode = (instruction >> 26) & 0x3Fu;
    
    switch (opcode) {
        case 0x00: { // R-type
            uint32_t funct = instruction & 0x3Fu;
            if (funct == 0x20) return "ADD";
            if (funct == 0x22) return "SUB";
            if (funct == 0x18) return "MULT";
            if (funct == 0x1A) return "DIV";
            return ""; // NOP
        }
        case 0x02: return "J";        
        case 0x03: return "JAL";
        case 0x04: return "BEQ";
        case 0x05: return "BNE";
        case 0x08: return "ADDI";     
        case 0x09: return "ADDIU";    
        case 0x0F: return "LUI";      
        case 0x0C: return "ANDI";     
        case 0x0A: return "SLTI";     
        case 0x23: return "LW";       
        case 0x2B: return "SW";       
        case 0x0E: return "LI";       
        case 0x10: return "PRINT";    
        case 0x3F: return "END";
        case 0x07: return "BGT";
        case 0x01: return "BLT"; 
        default: return ""; 
    }
}

static string get_dest_reg_for_hazard(const Instruction_Data& instr) {
    if (instr.op == "ADD" || instr.op == "SUB" || instr.op == "MULT" || instr.op == "DIV") return instr.destination_register;
    if (instr.op == "ADDI" || instr.op == "ADDIU" || instr.op == "LW" || instr.op == "LI" || instr.op == "LUI" || instr.op == "SLTI" || instr.op == "LA") return instr.target_register;
    return "";
}

void Control_Unit::Fetch(ControlContext &context) {
    account_stage(context.process);
    context.registers.mar.write(context.registers.pc.value);
    uint32_t instr = context.memManager.read(context.registers.mar.read(), context.process);
    context.registers.ir.write(instr);

    if (instr == 0 && context.registers.pc.value > 10000) {
        std::cerr << "[CPU] ERRO FATAL: PC desviou para área vazia (" 
                  << context.registers.pc.value << "). Encerrando PID " 
                  << context.process.pid << "\n";
        context.endProgram = true; // Força o fim do processo
        return;
    }
    
    const uint32_t END_SENTINEL = 0b11111100000000000000000000000000u;
    if (instr == END_SENTINEL) {
        context.endProgram = true;
        return;
    }
    context.registers.pc.write(context.registers.pc.value + 4);
}

void Control_Unit::Decode(hw::REGISTER_BANK &registers, Instruction_Data &data) {
    uint32_t instruction = registers.ir.read();
    data.rawInstruction = instruction;
    data.op = Identificacao_instrucao(instruction, registers);

    // Log para debug, sei que não é a melhor maneira, porém estou com preguiça de debugar.
    std::cout << "[DECODE] PC-4: " << (registers.pc.read()-4) << " Raw: " << std::hex << instruction << " OP: " << data.op << std::dec << "\n";

    if (data.op == "BUBBLE" || data.op == "") return;

    if (data.op == "ADD" || data.op == "SUB" || data.op == "MULT" || data.op == "DIV") {
        data.source_register = Get_source_Register(instruction);
        data.target_register = Get_target_Register(instruction);
        data.destination_register = Get_destination_Register(instruction);
    } else if (data.op == "ADDI" || data.op == "ADDIU" ||
               data.op == "LI" || data.op == "LW" || data.op == "LA" || data.op == "SW" ||
               data.op == "BGTI" || data.op == "BLTI" || data.op == "BEQ" || data.op == "BNE" ||
               data.op == "BGT"  || data.op == "BLT" || data.op == "SLTI" || data.op == "LUI") {
        data.source_register = Get_source_Register(instruction);   
        data.target_register = Get_target_Register(instruction);   
        data.addressRAMResult = Get_immediate(instruction);
        uint16_t imm16 = static_cast<uint16_t>(instruction & 0xFFFFu);
        data.immediate = signExtend16(imm16);
    } else if (data.op == "J") {
        uint32_t instr26 = instruction & 0x03FFFFFFu;
        data.addressRAMResult = std::bitset<26>(instr26).to_string();
        data.immediate = static_cast<int32_t>(instr26);
    } else if (data.op == "PRINT") {
        data.target_register = Get_target_Register(instruction);
        std::string imm = Get_immediate(instruction);
        bool allZero = true; for(char c : imm) if(c!='0') allZero=false;
        if (!allZero) {
            data.addressRAMResult = imm;
            uint16_t imm16 = static_cast<uint16_t>(binaryStringToUint(imm));
            data.immediate = signExtend16(imm16);
        } else {
            data.addressRAMResult.clear();
            data.immediate = 0;
        }
    }

    // Detecção de hazards simples (RAW) - insere bolha se necessário
    std::string read_reg1 = "";
    std::string read_reg2 = "";

    if (data.op == "ADD" || data.op == "SUB" || data.op == "MULT" || data.op == "DIV" || 
        data.op == "BEQ" || data.op == "BNE" || data.op == "BGT" || data.op == "BLT" || data.op == "SW") {
        read_reg1 = data.source_register;
        read_reg2 = data.target_register;
    }
    else if (data.op == "ADDI" || data.op == "ADDIU" || data.op == "LW" || data.op == "SLTI") {
        read_reg1 = data.source_register;
    }
    else if (data.op == "PRINT") {
        read_reg1 = data.target_register;
    }

    int current_idx = this->data.size() - 1;
    
    
    if (current_idx - 1 >= 0) {
        Instruction_Data& exec_instr = this->data[current_idx - 1];
        if (exec_instr.op != "BUBBLE" && !exec_instr.op.empty()) {
            string dest = get_dest_reg_for_hazard(exec_instr);
            if (!dest.empty() && dest != "00000") {
                if ((!read_reg1.empty() && read_reg1 == dest) || (!read_reg2.empty() && read_reg2 == dest)) {
                    data.op = "BUBBLE";
                    data.rawInstruction = 0;
                    registers.pc.write(registers.pc.read() - 4);
                    return;
                }
            }
        }
    }

    if (current_idx - 2 >= 0) {
        Instruction_Data& mem_instr = this->data[current_idx - 2];
        if (mem_instr.op != "BUBBLE" && !mem_instr.op.empty()) {
            string dest = get_dest_reg_for_hazard(mem_instr);
            if (!dest.empty() && dest != "00000") {
                if ((!read_reg1.empty() && read_reg1 == dest) || (!read_reg2.empty() && read_reg2 == dest)) {
                    data.op = "BUBBLE";
                    data.rawInstruction = 0;
                    registers.pc.write(registers.pc.read() - 4);
                    return;
                }
            }
        }
    }
}


void Control_Unit::Execute_Immediate_Operation(ControlContext &context, Instruction_Data &data) {
    if (data.op.empty() || data.op == "BUBBLE") return;
    hw::REGISTER_BANK &registers = context.registers; // Acessa os registradores via contexto
    
    std::string name_rs = this->map.getRegisterName(binaryStringToUint(data.source_register));
    std::string name_rt = this->map.getRegisterName(binaryStringToUint(data.target_register));
    int32_t val_rs = registers.readRegister(name_rs);
    int32_t imm = data.immediate; 
    std::ostringstream ss;
    
    if (data.op == "ADDI" || data.op == "ADDIU") {
        ALU alu; alu.A = val_rs; alu.B = imm; alu.op = ADD; alu.calculate();
        registers.writeRegister(name_rt, alu.result);
        ss << "[IMM] " << data.op << " " << name_rt << " = " << name_rs << "(" << val_rs << ") + " << imm << " -> " << alu.result;
        log_operation(ss.str(), context.process.pid); return; 
    }
    if (data.op == "SLTI") {
        int32_t res = (val_rs < imm) ? 1 : 0; registers.writeRegister(name_rt, res);
        ss << "[IMM] SLTI " << name_rt << " = (" << name_rs << "(" << val_rs << ") < " << imm << ") ? 1 : 0 -> " << res;
        log_operation(ss.str(), context.process.pid); return; 
    }
    if (data.op == "LUI") {
        uint32_t uimm = static_cast<uint32_t>(static_cast<uint16_t>(imm));
        int32_t val = static_cast<int32_t>(uimm << 16); registers.writeRegister(name_rt, val);
        ss << "[IMM] LUI " << name_rt << " = (0x" << std::hex << imm << " << 16) -> 0x" << val << std::dec;
        log_operation(ss.str(), context.process.pid); return; 
    }
    if (data.op == "LI") {
        registers.writeRegister(name_rt, imm);
        ss << "[IMM] LI " << name_rt << " = " << imm;
        log_operation(ss.str(), context.process.pid); return; 
    }
}

void Control_Unit::Execute_Aritmetic_Operation(ControlContext &context, Instruction_Data &data) {
    if (data.op.empty() || data.op == "BUBBLE") return;
    hw::REGISTER_BANK &registers = context.registers; // Acessa os registradores via contexto
    
    std::string name_rs = this->map.getRegisterName(binaryStringToUint(data.source_register));
    std::string name_rt = this->map.getRegisterName(binaryStringToUint(data.target_register));
    std::string name_rd = this->map.getRegisterName(binaryStringToUint(data.destination_register));
    int32_t val_rs = registers.readRegister(name_rs);
    int32_t val_rt = registers.readRegister(name_rt);
    ALU alu; alu.A = val_rs; alu.B = val_rt;
    if (data.op == "ADD") alu.op = ADD;
    else if (data.op == "SUB") alu.op = SUB;
    else if (data.op == "MULT") alu.op = MUL;
    else if (data.op == "DIV") alu.op = DIV;
    else return;
    alu.calculate(); registers.writeRegister(name_rd, alu.result);
    std::ostringstream ss;
    ss << "[ARIT] " << data.op << " " << name_rd << " = " << name_rs << "(" << val_rs << ") " << data.op << " " << name_rt << "(" << val_rt << ") = " << alu.result;
    log_operation(ss.str(), context.process.pid); 
}


void Control_Unit::Execute_Operation(Instruction_Data &data, ControlContext &context) {
    if (data.op == "PRINT") {
        if (!data.target_register.empty()) {
            string name = this->map.getRegisterName(binaryStringToUint(data.target_register));
            int value = context.registers.readRegister(name);
            auto req = std::make_unique<IORequest>();
            req->msg = std::to_string(value);
            req->process = &context.process;
            context.ioRequests.push_back(std::move(req));
            std::cout << "[PRINT-REQ] PRINT REG " << name << " value=" << value << " (pid=" << context.process.pid << ")\n";
            if (context.printLock) {
                context.process.state = State::Blocked;
                context.endExecution = true;
            }
        }
    }
}

void Control_Unit::Execute_Loop_Operation(hw::REGISTER_BANK &registers, Instruction_Data &data,
                                          int &counter, int &counterForEnd, bool &programEnd,
                                          MemoryManager &memManager, PCB &process) {
    if (data.op.empty() || data.op == "BUBBLE") return;

    string name_rs = this->map.getRegisterName(binaryStringToUint(data.source_register));
    string name_rt = this->map.getRegisterName(binaryStringToUint(data.target_register));
    ALU alu;
    alu.A = registers.readRegister(name_rs);
    alu.B = registers.readRegister(name_rt);
    
    bool jump = false;
    if (data.op == "BEQ") { alu.op = BEQ; alu.calculate(); if (alu.result == 1) jump = true; }
    else if (data.op == "BNE") { alu.op = BNE; alu.calculate(); if (alu.result == 1) jump = true; }
    else if (data.op == "J") { jump = true; }
    else if (data.op == "BLT") { alu.op = BLT; alu.calculate(); if (alu.result == 1) jump = true; }
    else if (data.op == "BGT") { alu.op = BGT; alu.calculate(); if (alu.result == 1) jump = true; }

    if (jump) {
        uint32_t targetAddr = static_cast<uint32_t>(data.immediate);
        std::cout << "[BRANCH] OP=" << data.op << " tomado. PC Antigo=" << registers.pc.read() << " -> Novo PC=" << targetAddr << "\n";
        registers.pc.write(targetAddr);
        
        if (counter - 1 >= 0 && counter - 1 < this->data.size()) {
            this->data[counter - 1].op = "BUBBLE";
        }
        registers.ir.write(0);
    }
}



//Atualizar chamadas para passar o ControlContext
void Control_Unit::Execute(Instruction_Data &data, ControlContext &context) {
    account_stage(context.process);
    if (data.op.empty() || data.op == "BUBBLE") return;

    if (data.op == "ADDI" || data.op == "ADDIU" || data.op == "SLTI" || data.op == "LUI" || data.op == "LI") {
        Execute_Immediate_Operation(context, data); return; 
    }
    if (data.op == "ADD" || data.op == "SUB" || data.op == "MULT" || data.op == "DIV") {
        Execute_Aritmetic_Operation(context, data); return; 
    } else if (data.op == "BEQ" || data.op == "J" || data.op == "BNE" || data.op == "BGT" || data.op == "BGTI" || data.op == "BLT" || data.op == "BLTI") {
        Execute_Loop_Operation(context.registers, data, context.counter, context.counterForEnd, context.endProgram, context.memManager, context.process); return;
    } else if (data.op == "PRINT") {
        Execute_Operation(data, context); return;
    }
}

// Funcao que realiza a etapa de acesso a memoria por meio de diferentes instrucoes
void Control_Unit::Memory_Acess(Instruction_Data &data, ControlContext &context) {
    account_stage(context.process);
    if (data.op.empty() || data.op == "BUBBLE") return;

    string name_rt = this->map.getRegisterName(binaryStringToUint(data.target_register));

    // transferencia de uma palavra de 4 bits para o registrador ( string name_rt )
    if (data.op == "LW") {

        uint32_t addr = binaryStringToUint(data.addressRAMResult);
        int value = context.memManager.read(addr, context.process);
        context.registers.writeRegister(name_rt, value);
        std::cout << "[MEMORY] LW addr=" << addr << " value=" << value << " -> " << name_rt << "\n";
    
    // LA e LI carregam um valor imediato para o registrador, sendo o LI podendo ser de 32 ou 16 bits
    } else if (data.op == "LA" || data.op == "LI") {

        uint32_t val = binaryStringToUint(data.addressRAMResult);
        context.registers.writeRegister(name_rt, static_cast<int>(val));
        std::cout << "[MEMORY] " << data.op << " -> " << name_rt << " value=" << static_cast<int>(val) << "\n";

    } else if (data.op == "PRINT" && data.target_register.empty()) {

        uint32_t addr = binaryStringToUint(data.addressRAMResult);
        int value = context.memManager.read(addr, context.process);
        auto req = std::make_unique<IORequest>();
        req->msg = std::to_string(value);
        req->process = &context.process;
        context.ioRequests.push_back(std::move(req));
        std::cout << "[PRINT-REQ] PRINT MEM addr=" << addr << " value=" << value << " (pid=" << context.process.pid << "\n";

        if (context.printLock) {
            context.process.state = State::Blocked;
            context.endExecution = true;
        }

    }
}

// Funcao que realiza a etapa de escrita de volta ao banco de registradores ou memoria
void Control_Unit::Write_Back(Instruction_Data &data, ControlContext &context) {
    account_stage(context.process);
    if (data.op.empty() || data.op == "BUBBLE") return;

    if (data.op == "SW") {
        uint32_t addr = binaryStringToUint(data.addressRAMResult);
        string name_rt = this->map.getRegisterName(binaryStringToUint(data.target_register));
        int value = context.registers.readRegister(name_rt);
        context.memManager.write(addr, value, context.process);
        std::cout << "[WRITE-BACK] SW addr=" << addr << " value=" << value << " from reg " << name_rt << "\n";
    }
}

void* Core(MemoryManager &memoryManager, PCB &process, vector<unique_ptr<IORequest>>* ioRequests, bool &printLock) {
    Control_Unit UC;
    Instruction_Data data;
    int clock = 0;
    int counterForEnd = 5;
    int counter = 0;
    bool endProgram = false;
    bool endExecution = false;

    ControlContext context{ process.regBank, memoryManager, *ioRequests, printLock, process, counter, counterForEnd, endProgram, endExecution };

    while (context.counterForEnd > 0) {
        if (context.counter >= 4 && context.counterForEnd >= 1) {
            UC.Write_Back(UC.data[context.counter - 4], context);
        }
        if (context.counter >= 3 && context.counterForEnd >= 2) {
            UC.Memory_Acess(UC.data[context.counter - 3], context);
        }
        if (context.counter >= 2 && context.counterForEnd >= 3) {
            UC.Execute(UC.data[context.counter - 2], context);
        }
        if (context.counter >= 1 && context.counterForEnd >= 4) {
            account_stage(process);
            UC.Decode(context.registers, UC.data[context.counter - 1]);
        }
        if (context.counter >= 0 && context.counterForEnd == 5) {
            UC.data.push_back(data);
            UC.Fetch(context);
        }

        context.counter += 1;
        clock += 1;
        account_pipeline_cycle(process);

        if (clock >= process.quantum || context.endProgram == true) {
            context.endExecution = true;
        }
        if (context.endExecution == true) {
            context.counterForEnd -= 1;
        }
    }

    if (context.endProgram) {
        process.state = State::Finished;
    }
    return nullptr;
}