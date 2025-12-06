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


def generate_pie_chart_all(data, col_value, title, filename):
    """Gera um gráfico de pizza para a métrica usando cores neutras."""
    
    plt.style.use('default')
    sns.set_context("paper", font_scale=1.2)
    
    # 1. Preparação dos dados e labels
    labels = data['Algoritmo']
    sizes = data[col_value]
    total_sum = sizes.sum()
    
    # 2. Paleta de cores (NEUTRA: Pastel1)
    palette_colors = sns.color_palette("Pastel1", n_colors=len(data))
    
    plt.figure(figsize=(9, 9))
    
    # Adiciona nota para métricas "Menor é Melhor"
    is_lower_better = col_value in ['Media_Wait', 'Media_Turnaround']
    
    if is_lower_better:
        title += " (Menor Fatia = Melhor Performance)"
    
    # Formato de porcentagem e valor absoluto
    def autopct_format(pct):
        val = (pct * total_sum) / 100.0
        return f'{pct:.1f}%\n({val:.4f})'
        
    # 3. Criação do gráfico de pizza
    wedges, texts, autotexts = plt.pie(
        sizes, 
        colors=palette_colors,
        autopct=autopct_format,
        labels=labels, 
        startangle=90, 
        wedgeprops={"edgecolor": "black", 'linewidth': 1.5, 'antialiased': True},
        textprops={'fontsize': 10, 'weight': 'bold'}
    )
    

    plt.title(title, fontsize=14, fontweight='bold', pad=20)
    plt.tight_layout()
    plt.savefig(os.path.join(OUTPUT_DIR, filename), dpi=400)
    plt.close()



generate_pie_chart_all(df_final, 'Throughput', 'Contribuição Relativa do Throughput por Algoritmo', 'fig_throughput.png')
generate_pie_chart_all(df_final, 'Utilização Média CPU (%)', 'Contribuição Relativa da Utilização Média da CPU', 'fig_cpu_util.png')
generate_pie_chart_all(df_final, 'Media_Wait', 'Contribuição Relativa do Tempo de Espera Médio', 'fig_wait_time.png')
generate_pie_chart_all(df_final, 'Media_Turnaround', 'Contribuição Relativa do Tempo de Turnaround Médio', 'fig_turnaround_time.png')

df_final.to_csv(os.path.join(OUTPUT_DIR, 'metricas_consolidadas_final.csv'), index=False)

print("\n✅ Análise concluída.")
print(f"Os Gráficos foram salvos no diretório: {OUTPUT_DIR}")