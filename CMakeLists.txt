# Versão mínima do CMake e nome do projeto
cmake_minimum_required(VERSION 3.10)
project(VonNeumannSimulator)

# Define o padrão C++17 como obrigatório
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Habilita a compilação com informações de debug por padrão
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

# Adiciona o diretório 'src' para que os #includes funcionem
include_directories(src)

# --- LISTA DE ARQUIVOS FONTE PARA O SIMULADOR PRINCIPAL ---
set(SIMULATOR_SOURCES
    src/main.cpp
    src/cpu/CONTROL_UNIT.cpp
    src/cpu/pcb_loader.cpp
    src/cpu/Scheduler.cpp
    src/cpu/REGISTER_BANK.cpp
    src/cpu/ULA.cpp
    src/IO/IOManager.cpp
    src/memory/cache.cpp
    src/memory/cachePolicy.cpp
    src/memory/MAIN_MEMORY.cpp
    src/memory/MemoryManager.cpp
    src/memory/SECONDARY_MEMORY.cpp
    src/parser_json/parser_json.cpp
)

# --- ALVOS PRINCIPAIS (EXECUTÁVEIS) ---
add_executable(simulador ${SIMULATOR_SOURCES})
target_link_libraries(simulador PRIVATE pthread)

# --- ALVOS DE TESTE ---
add_executable(test_hash src/test/test_hash_register.cpp)
add_executable(test_bank src/test/test_register_bank.cpp src/cpu/REGISTER_BANK.cpp)
add_executable(test_ula src/test/teste_alu.cpp src/cpu/ULA.cpp)
add_executable(test_metrics 
    src/test/test_cpu_metrics.cpp 
    src/cpu/CONTROL_UNIT.cpp 
    src/cpu/pcb_loader.cpp 
    src/cpu/ULA.cpp 
    src/cpu/REGISTER_BANK.cpp
    src/memory/MemoryManager.cpp
    src/memory/MAIN_MEMORY.cpp
    src/memory/SECONDARY_MEMORY.cpp
    src/memory/cache.cpp
    src/memory/cachePolicy.cpp
    src/IO/IOManager.cpp
    src/parser_json/parser_json.cpp
)
target_link_libraries(test_metrics PRIVATE pthread)

# --- COPIAR ARQUIVOS NECESSÁRIOS PARA A PASTA BUILD ---

# 1. Copia o batch.json para a raiz do build
file(COPY ${CMAKE_SOURCE_DIR}/batch.json DESTINATION ${CMAKE_BINARY_DIR})

# 2. Copia a PASTA 'processos' inteira
# Isso cria 'build/processos/cpu_bound/...', mantendo a estrutura que o batch.json exige
file(COPY ${CMAKE_SOURCE_DIR}/processos DESTINATION ${CMAKE_BINARY_DIR})

# 3. Cria a estrutura da pasta 'src/tasks' dentro do build (caso precise acessar localmente)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/src)
file(COPY ${CMAKE_SOURCE_DIR}/src/tasks DESTINATION ${CMAKE_BINARY_DIR}/src)

# 4. Garante que a pasta output exista para os logs
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/output)

# --- CUSTOM TARGETS (RUN, CHECK, AJUDA) ---

# Alvo para executar o simulador
add_custom_target(run
    COMMAND ${CMAKE_BINARY_DIR}/simulador
    DEPENDS simulador
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "🚀 Executando o simulador..."
)

add_custom_target(test-all
    DEPENDS test_hash test_bank test_ula test_metrics
    COMMAND ${CMAKE_BINARY_DIR}/test_hash
    COMMAND ${CMAKE_BINARY_DIR}/test_bank
    COMMAND ${CMAKE_BINARY_DIR}/test_ula
    COMMAND ${CMAKE_BINARY_DIR}/test_metrics
    COMMENT "🧪 Executando todos os testes..."
    VERBATIM
)

add_custom_target(check
    DEPENDS simulador test_hash test_bank test_ula test_metrics
    COMMAND bash -c "'${CMAKE_BINARY_DIR}/simulador > /dev/null 2>&1 && echo \"  Simulador principal: ✅ PASSOU\" || echo \"  Simulador principal: ❌ FALHOU\"'"
    COMMAND bash -c "'${CMAKE_BINARY_DIR}/test_hash > /dev/null 2>&1 && echo \"  Teste hash register: ✅ PASSOU\" || echo \"  Teste hash register: ❌ FALHOU\"'"
    COMMAND bash -c "'${CMAKE_BINARY_DIR}/test_bank > /dev/null 2>&1 && echo \"  Teste register bank: ✅ PASSOU\" || echo \"  Teste register bank: ❌ FALHOU\"'"
    COMMAND bash -c "'${CMAKE_BINARY_DIR}/test_ula > /dev/null 2>&1 && echo \"  Teste ULA: ✅ PASSOU\" || echo \"  Teste ULA: ❌ FALHOU\"'"
    COMMAND bash -c "'${CMAKE_BINARY_DIR}/test_metrics > /dev/null 2>&1 && echo \"  Teste de Métricas: ✅ PASSOU\" || echo \"  Teste de Métricas: ❌ FALHOU\"'"
    COMMENT "🎯 Executando verificações rápidas..."
    VERBATIM
)

add_custom_target(ajuda
    COMMAND ${CMAKE_COMMAND} -E echo "📋 SO-SimuladorVonNeumann - Comandos Disponíveis:"
    COMMAND ${CMAKE_COMMAND} -E echo "  make run               - Compila e executa o simulador"
    VERBATIM
)