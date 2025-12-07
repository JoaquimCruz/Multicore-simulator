import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
import re
import os
import sys
import numpy as np


METRICS_BASE_DIR = os.path.join('..', 'build', 'output', 'metricas')
OUTPUT_DIR = os.path.join('..', 'build', 'output', 'graficos') 

def setup_output_dir():
    """Cria a pasta de saída (../build/output/graficos/) se ela não existir."""
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
    print("ERRO: Nenhum arquivo de métricas pôde ser carregado. Encerrando.")
    sys.exit(1)

df_global = pd.DataFrame(global_dfs)
df_process = pd.concat(process_dfs, ignore_index=True)

df_avg_process = df_process.groupby('Algoritmo').agg(
    Media_Wait=('Wait', 'mean'),
    Media_Turnaround=('Turnaround', 'mean'),
).reset_index()

df_final = pd.merge(df_global, df_avg_process, on='Algoritmo')


def generate_plot(data, y_col, title, filename, ascending=False):
    """Função auxiliar para gerar gráficos de barras e salvar no diretório correto."""
    plt.figure(figsize=(9, 5))
    sns.barplot(
        x='Algoritmo', 
        y=y_col, 
        data=data.sort_values(by=y_col, ascending=ascending), 
        palette='coolwarm'
    )
    plt.title(title, fontsize=14)
    plt.ylabel(y_col, fontsize=12)
    plt.xlabel('Algoritmo', fontsize=12)
    plt.grid(axis='y', linestyle='--')
    plt.tight_layout()

    plt.savefig(os.path.join(OUTPUT_DIR, filename))
    plt.close()

sns.set_style("whitegrid")
generate_plot(df_final, 'Throughput', 'Throughput Global por Algoritmo', 'comparacao_throughput.png', ascending=False)
generate_plot(df_final, 'Utilização Média CPU (%)', 'Utilização Média da CPU', 'comparacao_utilizacao_cpu.png', ascending=False)
generate_plot(df_final, 'Media_Wait', 'Tempo de Espera Médio', 'comparacao_wait_time.png', ascending=True)
generate_plot(df_final, 'Media_Turnaround', 'Tempo de Turnaround Médio', 'comparacao_turnaround_time.png', ascending=True)

df_final.to_csv(os.path.join(OUTPUT_DIR, 'metricas_consolidadas_final.csv'), index=False)

print("\n✅ Análise concluída.")
print(f"Os Gráficos foram salvos no diretório: {OUTPUT_DIR}")