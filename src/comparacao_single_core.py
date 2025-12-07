import matplotlib.pyplot as plt
import seaborn as sns
import pandas as pd
import numpy as np
import os


algoritmos = ['SJN', 'Priority', 'FCFS', 'Round Robin']


tempo_single = [298, 314, 314, 314] 

tempo_multi  = [193, 194, 164, 180]

# Throughput (Processos/Ciclo) - Calculado: 13 processos / Tempo
throughput_single = [13/t for t in tempo_single]
throughput_multi  = [13/t for t in tempo_multi]

# Speedup - Calculado: T_single / T_multi
speedup = [ts/tm for ts, tm in zip(tempo_single, tempo_multi)]


# Define estilo visual limpo
sns.set_theme(style="whitegrid", context="paper", font_scale=1.4)

# Configuração de Fontes (Sans-Serif)
plt.rcParams['font.family'] = 'serif'
plt.rcParams['font.serif'] = ['Times New Roman'] + plt.rcParams['font.serif']
plt.rcParams['axes.edgecolor'] = '#333333'
plt.rcParams['axes.linewidth'] = 1.2

# Paletas de cores profissionais
colors_compare = ["#34495e", "#e74c3c"] # Azul acinzentado (Single) e Vermelho (Multi)
color_speedup = "viridis"
color_throughput = "magma"

# Criar diretório de saída
output_dir = "../build/output/graficos/comparacao_single_core"
os.makedirs(output_dir, exist_ok=True)


def plot_tempo_execucao():
    # Prepara o DataFrame para o Seaborn (formato longo)
    df = pd.DataFrame({
        'Algoritmo': algoritmos * 2,
        'Tempo (Ciclos)': tempo_single + tempo_multi,
        'Arquitetura': ['Single-Core']*4 + ['Multicore (4 Cores)']*4
    })

    plt.figure(figsize=(10, 7)) # Aumentei um pouco a altura para caber a legenda
    ax = sns.barplot(x='Algoritmo', y='Tempo (Ciclos)', hue='Arquitetura', data=df, palette=colors_compare)
    
    # Adicionar valores nas barras
    for container in ax.containers:
        ax.bar_label(container, padding=3, fmt='%.0f', fontsize=11, fontweight='bold')

    plt.title('Redução do Tempo de Execução: Single-Core vs Multicore', fontsize=16, fontweight='bold', pad=20)
    plt.ylabel('Tempo Total (Ciclos)', fontweight='bold')
    plt.xlabel('Política de Escalonamento', fontweight='bold')

    
    plt.legend(
        title='', 
        loc='upper center', 
        bbox_to_anchor=(0.5, -0.15), 
        ncol=2,                      
        frameon=False, 
        fontsize=12
    )

    
    plt.tight_layout()
    sns.despine(left=True)
    
    plt.savefig(f"{output_dir}/1_tempo_execucao_final.png", dpi=300, bbox_inches='tight')
    print("Gráfico 1 salvo: Tempo de Execução")


def plot_speedup():
    plt.figure(figsize=(10, 6))
    
    # Barplot
    ax = sns.barplot(x=algoritmos, y=speedup, palette=color_speedup, hue=algoritmos, legend=False)
    
    # Linha de Amdahl (Teórico)
    plt.axhline(y=1.9, color='#c0392b', linestyle='--', linewidth=2.5, label='Speedup Teórico (Amdahl): 1.9x')
    
    # Linha de Base (1x - Sem ganho)
    plt.axhline(y=1.0, color='black', linestyle='-', linewidth=1)

    # Valores nas barras
    for i, v in enumerate(speedup):
        ax.text(i, v + 0.05, f"{v:.2f}x", ha='center', fontweight='bold', fontsize=12)

    plt.title('Speedup Obtido por Algoritmo (Multicore 4x)', fontsize=16, fontweight='bold', pad=20)
    plt.ylabel('Speedup (T_single / T_multi)', fontweight='bold')
    plt.xlabel('Política de Escalonamento', fontweight='bold')
    plt.ylim(0, 4.5)
    
    # Legenda interna (aqui não atrapalha)
    plt.legend(loc='upper right', frameon=True)
    sns.despine(left=True)

    plt.savefig(f"{output_dir}/2_speedup_amdahl_final.png", dpi=300, bbox_inches='tight')
    print("Gráfico 2 salvo: Speedup")


def plot_throughput():
    df = pd.DataFrame({
        'Algoritmo': algoritmos * 2,
        'Throughput': throughput_single + throughput_multi,
        'Arquitetura': ['Single-Core']*4 + ['Multicore (4 Cores)']*4
    })

    plt.figure(figsize=(10, 7))
    ax = sns.barplot(x='Algoritmo', y='Throughput', hue='Arquitetura', data=df, palette="magma")

    plt.title('Aumento da Vazão (Throughput) do Sistema', fontsize=16, fontweight='bold', pad=20)
    plt.ylabel('Processos Finalizados por Ciclo', fontweight='bold')
    plt.xlabel('Política de Escalonamento', fontweight='bold')
    
    
    plt.legend(
        title='', 
        loc='upper center', 
        bbox_to_anchor=(0.5, -0.15), 
        ncol=2, 
        frameon=False, 
        fontsize=12
    )
    
    plt.tight_layout()
    sns.despine(left=True)

    plt.savefig(f"{output_dir}/3_throughput_final.png", dpi=300, bbox_inches='tight')
    print("Gráfico 3 salvo: Throughput")

# Execução
if __name__ == "__main__":
    print("Gerando gráficos finais para o artigo...")
    plot_tempo_execucao()
    plot_speedup()
    plot_throughput()
    print(f"\nSucesso! Todos os gráficos salvos em: {output_dir}")