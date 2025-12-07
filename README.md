# Simulador de Arquitetura Multicore: Escalonamento e Gerenciamento de Memória

<div style="display: inline-block;">
<img align="center" height="20px" width="60px" src="https://img.shields.io/badge/C%2B%2B-00599C?style=for-the-badge&logo=c%2B%2B&logoColor=white"/> 
<img align="center" height="20px" width="60px" src="https://img.shields.io/badge/Python-3776AB?style=for-the-badge&logo=python&logoColor=white"/> 
<img align="center" height="20px" width="80px" src="https://img.shields.io/badge/Made%20for-VSCode-1f425f.svg"/> 
</div>

---

## Introdução

Este projeto consiste no desenvolvimento de um **Simulador de Arquitetura Multicore**, criado como Trabalho Final da disciplina de **Sistemas Operacionais**, ministrada pelo docente Michel Pires (<a href="https://github.com/mpiress" target="_blank">mpiress</a>) do Centro Federal de Educação Tecnológica de Minas Gerais (CEFET-MG).

Desenvolvido por alunos do **6º período de Engenharia de Computação**, o simulador tem como objetivo explorar e validar conceitos fundamentais de sistemas operacionais modernos, incluindo:
* Execução paralela em arquitetura Quad-Core.
* Políticas de escalonamento de processos (Preemptivas e Não-Preemptivas).
* Gerenciamento de Memória Virtual com Paginação e *Swap*.
* Simulação de hierarquia de memória (Cache L1, RAM e Disco).
* Tratamento de *Hazards* de dados e controle de fluxo em nível de instrução.

O sistema permite a carga de lotes de processos heterogêneos (*CPU-Bound*, *I/O-Bound* e *Memory-Bound*) e gera métricas detalhadas para análise de desempenho.

---

##  Sumário

- [Estruturação do Projeto](#-estruturação-do-projeto)
- [Métricas Analisadas](#métricas-analisadas)
- [Como Adicionar Novos Programas](#como-adicionar-novos-programas)
- [Como Rodar o Código](#como-rodar-o-código)
- [Visualização de Dados](#visualização-de-dados)

---

##  Estruturação do Projeto

```text
.
├── batch.json
├── build
│   ├── batch.json
│   ├── CMakeCache.txt
│   ├── CMakeFiles
│   ├── cmake_install.cmake
│   ├── Makefile
│   ├── output
│   ├── output.dat
│   ├── processos
│   ├── result.dat
│   ├── simulador
│   ├── src
│   ├── test_bank
│   ├── test_hash
│   ├── test_metrics
│   └── test_ula
├── CMakeLists.txt
├── IO
│   └── SO-IO
├── LICENSE
├── Makefile
├── md
│   └── HASH_REGISTER_IMPROVEMENTS.md
├── parser_json
│   ├── CMakeLists.txt
│   ├── include
│   ├── src
│   └── tasks
├── processos
│   ├── cpu_bound
│   ├── io_bound
│   ├── memory_bound
│   └── process1.json
├── README.md
├── src
│   ├── analise_graficos.py
│   ├── comparação.py
│   ├── comparacao_single_core.py
│   ├── cpu
│   ├── IO
│   ├── main.cpp
│   ├── memory
│   ├── nlohmann
│   ├── parser_json
│   ├── tasks
│   ├── test
│   ├── teste.cpp
│   ├── test_hash_register.cpp
│   └── test_register_bank.cpp
├── test
│   ├── test_cpu_metrics.cpp
│   └── test_memory.cpp
├── teste
└── teste.exe

```

## Métricas Analisadas

O simulador coleta dados em tempo real e gera relatórios finais (`.dat`) para avaliar o desempenho das políticas de escalonamento (*FCFS, SJN, Round Robin e Priority*). As principais métricas incluem:

- **Tempo Total de Execução**: O tempo total necessário para concluir todo o lote de processos. `/build/output/metricas`
- **Throughput (Vazão):** Quantidade de processos finalizados por unidade de tempo (ciclo). `/build/output/metricas`
- **Utilização da CPU:** Porcentagem de tempo em que os núcleos estiveram ocupados executando instruções úteis versus ociosidade. `/build/output/metricas`
- **Tempo de Espera (Waiting Time):** Tempo total que um processo permaneceu na fila de prontos aguardando execução. `/build/output/metricas`
- **Turnaround Time:** Tempo total desde a chegada do processo até sua finalização. `/build/output/metricas`
- **Estatísticas de Memória:** Contagem de Page Faults e Cache Hits/Misses para cada processo. `/build/output/resultados/resultados.dat`


## Como Adicionar Novos Programas 

Para adicionar um novo processo ao simulador, são necessários dois arquivos JSON: um com o código do programa (tarefa) e outro com a definição do processo para o SO.

### Arquivo de Definição do Processo (`processos/`)

Este arquivo define o PID, prioridade e aponta para o código fonte simulado. O local de salvamento dele irá depender das caracterísicas do processo, ele podendo ser cpu/memory/IO bound. A estruturação das pastas e o exemplo do json para definição do processo estão citados abaixo. 

````Markdown
├── processos                   # Definições dos processos (PID, Prioridade)
│   ├── cpu_bound
│   ├── io_bound
│   ├── memory_bound
│   └── process1.json

````

````JSON
{
  "pid": 10, 
  "name": "Nome do Processo",
  "priority": 5,
  "program_path": "src/tasks/PASTA/arquivo_tarefa.json"
}

````

### Arquivo de Tarefa/Código (`src/tasks/`)

Este arquivo contém as instruções Assembly MIPS simuladas e a seção de dados. A pasta de salvamento irá depender novamente das características do processo, ele podendo ser cpu/memory/IO bound. A estruturação das pastas e o exemplo do json para definição do processo estão citados abaixo. 

````Markdown
├── src
│   └── tasks                   # Códigos Assembly MIPS simulados
│       ├── CPUBOUND
│       ├── IOBOUND
│       └── MEMORYBOUND
````

````JSON
{

  "metadata": { "task_id": "exemplo", "description": "Descricao" },
  "data": { "var1": 10 },
  "program": [
    { "instruction": "lw", "rt": "$t0", "offset": 0, "base": "var1" },
    { "instruction": "addi", "rt": "$t0", "rs": "$t0", "immediate": 1 },
    { "instruction": "end" }
  ]
}

````


É necessário também adicionar o caminho do novo arquivo de processo (passo 1) no arquivo `batch.json` para que ele seja carregado na inicialização. 


````JSON
{
  "processes": [
    "processos/process1.json",
    "processos/cpu_bound/process2.json",
    "processos/cpu_bound/process3.json",
    "processos/io_bound/process5.json",
    "processos/cpu_bound/process6.json",
    "processos/io_bound/proc_io_1.json",
    "processos/io_bound/proc_io_2.json",
    "processos/io_bound/proc_io_3.json",
    "processos/memory_bound/proc_mem_1.json",
    "processos/memory_bound/proc_mem_2.json",
    "processos/memory_bound/proc_mem_3.json",
    -> COLOQUE AQUI O SEU NOVO PROCESSO
  ]
}

````

## Como rodar o código

### Pré-requesitos

- Compilador C++ com suporte a C++17 (ex: g++);
- `CMake` (versão 3.10 ou superior);
- `Make`.

### Compilação e execução

1. Clone o repositório e navegue para a pasta raiz do projeto.

```` BASH  
$ git clone git@github.com:JoaquimCruz/Multicore-simulator.git

$ cd Multicore-simulator
````

2. Crie o diretório build e compile o projeto

```` BASH  
mkdir build
cd build
cmake ..
make
````

3. Execute o simulador: 

```` BASH  
# Recomendado: Via Make (garante cópia atualizada dos JSONs)
make run
````

4. No menu interativo, selecione o algoritmo de escalonamento desejado: 


```` BASH  
0 - FCFS
1 - SJN
2 - Round Robin
3 - Priority
````

As métricas e resultados são salvas em `build/output`

Caso precise alterar o código, para compilar e rodar após alterações será necessário seguir os comandos: 

```` BASH  
# Entre na pasta build
cd build

# Para remover todo o conteúdo da pasta build
rm -rf * 

cmake ..
make
make run
````

## Visualização de Dados

O projeto inclui scripts em Python para gerar gráficos comparativos baseados nos logs gerados pelo simulador.

Os gráficos são gerados de maneira automática, exceto aqueles gerados pelo arquivo `comparacao_single_core.py`, em que os valores são definidos a mão. Tais gráficos são armazenados em ` build/graficos/comparacao_single_core/`. 

### Pré-requisitos Python

Certifique-se de ter o Python 3 instalado e as seguintes bibliotecas:

```` BASH  
pip install matplotlib seaborn pandas
````

### Gerando os gráficos

Após rodar as simulações e gerar os arquivos de métricas na pasta `build/output/metricas`, execute o script dentro da pasta `src/`:

```` BASH 

# na raiz do projeto, entre na pasta src
cd src

# Comparação entre quad-core vs. single-core
python3 comparacao_single_core.py

# Comparação carga de trabalho
python3 comparação.py

# Métricas de desempenho
python analise_graficos.py
````

Os gráficos  serão salvos automaticamente na pasta: `build/output/graficos/`


# Autores

<p>
  João Francisco Teles da Silva - Graduando em Engenharia da Computação pelo <a href="https://www.cefetmg.br" target="_blank">CEFET-MG</a>. Contato: (<a href="mailto:joao.silva.05@aluno.cefetmg.br">joao.silva.05@aluno.cefetmg.br</a>)
</p>

<p>
  Joaquim Cézar Santana da Cruz - Graduando em Engenharia da Computação pelo <a href="https://www.cefetmg.br" target="_blank">CEFET-MG</a>. Contato: (<a href="mailto:joaquim.cruz@aluno.cefetmg.br">joaquim.cruz@aluno.cefetmg.br</a>)
</p>

<p>
  Lucas Cerqueira Portela - Graduando em Engenharia da Computação pelo <a href="https://www.cefetmg.br" target="_blank">CEFET-MG</a>. Contato: (<a href="mailto:lucas.portela@aluno.cefetmg.br">lucas.portela@aluno.cefetmg.br</a>)
</p>



<p>
  Maíra Beatriz de Almeida Lacerda - Graduando em Engenharia da Computação pelo <a href="https://www.cefetmg.br" target="_blank">CEFET-MG</a>. Contato: (<a href="mailto:maira@aluno.cefetmg.br">maira@aluno.cefetmg.br</a>)
</p>


