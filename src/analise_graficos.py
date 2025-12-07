import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
import re
import os
import sys
import numpy as np

plt.style.use("default")
sns.set_theme(style="whitegrid")

plt.rcParams.update({
    "figure.dpi": 300,           
    "savefig.dpi": 300,
    "font.family": "serif",       
    "axes.titlesize": 16,        
    "axes.labelsize": 13,         
    "xtick.labelsize": 11,
    "ytick.labelsize": 11,
    "legend.fontsize": 11,
})


METRICS_BASE_DIR = os.path.join('..', 'build', 'output', 'metricas')
OUTPUT_DIR = os.path.join('..', 'build', 'output', 'graficos', 'comparacao_metricas') 

Y_LABELS = {
    'Throughput': 'Throughput (processos/unidade de tempo)',
    'Utilização Média CPU (%)': 'Utilização Média da CPU (%)',
    'Media_Wait': 'Tempo Médio de Espera (unid. de tempo)',
    'Media_Turnaround': 'Tempo Médio de Turnaround (unid. de tempo)',
}


def setup_output_dir():
    """Cria a pasta de saída se ela não existir."""
    try:
        os.makedirs(OUTPUT_DIR, exist_ok=True)
        print(f"✅ Diretório de saída configurado: {OUTPUT_DIR}")
    except OSError:
        print(f"❌ ERRO: Não foi possível criar o diretório de saída: {OUTPUT_DIR}")
        sys.exit(1)

def parse_metrics(file_path, algorithm_name):
    """Lê um arquivo .dat e extrai métricas globais e por processo."""
    if not os.path.exists(file_path): return None, None
    try:
        with open(file_path, 'r', encoding='utf-8') as f: content = f.read()
    except Exception: return None, None
    
    global_metrics = {'Algoritmo': algorithm_name}
    
    match_time = re.search(r"Tempo total simulação:\s+(\d+)", content)
    global_metrics['Tempo Total Simulação'] = int(match_time.group(1)) if match_time else None
    
    match_cpu = re.search(r"Utilização média da CPU:\s+([\d\.]+)%", content)
    global_metrics['Utilização Média CPU (%)'] = float(match_cpu.group(1)) if match_cpu else None
    
    match_throughput = re.search(r"Throughput global:\s+([\d\.]+)", content)
    global_metrics['Throughput'] = float(match_throughput.group(1)) if match_throughput else None
    
    process_metrics = []
    pattern = re.compile(r"PID (\d+) \| Wait=(\d+) \| Turnaround=(\d+) \| CPU=(\d+)", re.DOTALL)
    for match in pattern.finditer(content):
        pid, wait, turnaround, cpu = map(int, match.groups())
        process_metrics.append({'Algoritmo': algorithm_name, 'PID': pid, 'Wait': wait, 'Turnaround': turnaround, 'CPU_Burst': cpu})
        
    return global_metrics, pd.DataFrame(process_metrics)


def generate_plot(data, y_col, title, filename, palette, ascending=False):
    """
    Gera um gráfico de barras otimizado usando Seaborn com linha de média.
    """
    data_sorted = data.sort_values(by=y_col, ascending=ascending).copy()
    
    data_sorted['Algoritmo'] = pd.Categorical(data_sorted['Algoritmo'], categories=data_sorted['Algoritmo'].unique(), ordered=True)
    
    overall_mean = data_sorted[y_col].mean()

    fig, ax = plt.subplots(figsize=(9, 5.5)) 

    sns.barplot(
        x="Algoritmo",
        y=y_col,
        data=data_sorted,
        ax=ax,
        palette=palette
    )

    ax.axhline(
        overall_mean, 
        color='#b30000', 
        linestyle='--', 
        linewidth=1.5, 
        alpha=0.7,
        label=f'Média Geral: {overall_mean:.2f}'
    )
    
    ax.legend(
        loc='upper left', 
        bbox_to_anchor=(1.05, 1), 
        frameon=True, 
        fontsize=10
    )


    ax.set_title(title, pad=15, weight='bold', fontsize=16)
    ax.set_xlabel("Algoritmo de Escalonamento", labelpad=10, weight='semibold')
    ax.set_ylabel(Y_LABELS.get(y_col, y_col), labelpad=10, weight='semibold')

    ax.grid(axis='y', linestyle='-', alpha=0.3)
    ax.grid(axis='x', visible=False)

    ax.spines['top'].set_visible(False)
    ax.spines['right'].set_visible(False)
    ax.spines['bottom'].set_linewidth(0.8)
    ax.spines['left'].set_linewidth(0.8)

    for p in ax.patches:
        height = p.get_height()
        ax.text(
            p.get_x() + p.get_width()/2,
            height + ax.get_ylim()[1] * 0.015,
            f"{height:.2f}",
            ha='center', va='bottom',
            fontsize=10,
            color='black',
            fontweight='bold'
        )

    plt.tight_layout()
    plt.savefig(os.path.join(OUTPUT_DIR, filename), bbox_inches='tight')
    plt.close()



setup_output_dir()
metrics_dir = METRICS_BASE_DIR 

FILE_PATHS = {
    'FCFS': os.path.join(metrics_dir, 'metricas_FCFS.dat'),
    'PRIORITY': os.path.join(metrics_dir, 'metricas_PRIORITY.dat'),
    'RR': os.path.join(metrics_dir, 'metricas_RR.dat'),
    'SJN': os.path.join(metrics_dir, 'metricas_SJN.dat')
}

global_dfs = []
process_dfs = []
for algo, path in FILE_PATHS.items():
    global_data, process_data = parse_metrics(path, algo)
    if global_data is not None:
        global_dfs.append(global_data)
        process_dfs.append(process_data)

if not global_dfs:
    print("ERRO: Nenhum arquivo de métricas pôde ser carregado. Verifique o caminho.")
    sys.exit(1)

df_global = pd.DataFrame(global_dfs)
df_process = pd.concat(process_dfs, ignore_index=True)

df_avg_process = df_process.groupby('Algoritmo').agg(
    Media_Wait=('Wait', 'mean'),
    Media_Turnaround=('Turnaround', 'mean'),
).reset_index()

df_final = pd.merge(df_global, df_avg_process, on='Algoritmo')

ALGORITHM_PALETTE = sns.color_palette("viridis", len(df_final['Algoritmo'].unique()))


print("\n Gerando Gráficos de Comparação...")
generate_plot(df_final, 'Throughput', 'Throughput Global por Algoritmo', 'comparacao_throughput.png', palette=ALGORITHM_PALETTE, ascending=False)
generate_plot(df_final, 'Utilização Média CPU (%)', 'Utilização Média da CPU', 'comparacao_utilizacao_cpu.png', palette=ALGORITHM_PALETTE, ascending=False)
generate_plot(df_final, 'Media_Wait', 'Tempo de Espera Médio', 'comparacao_wait_time.png', palette=ALGORITHM_PALETTE, ascending=True)
generate_plot(df_final, 'Media_Turnaround', 'Tempo de Turnaround Médio', 'comparacao_turnaround_time.png', palette=ALGORITHM_PALETTE, ascending=True)

df_final.to_csv(os.path.join(OUTPUT_DIR, 'metricas_consolidadas_final_viridis.csv'), index=False)

print("\n✅ Análise concluída.")
print(f"Os Gráficos e o CSV consolidado foram salvos no diretório: {OUTPUT_DIR}")