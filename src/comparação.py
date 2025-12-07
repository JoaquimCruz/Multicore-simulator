import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
import re
import os
import sys


METRICS_BASE_DIR = os.path.join('..', 'build', 'output', 'metricas')
OUTPUT_DIR_SPECIFIC = os.path.join('..', 'build', 'output', 'comparacao_especifica') # Novo diretório

def setup_output_dir_specific():
    """Cria a pasta de saída (comparacao_especifica) se ela não existir."""
    try:
        os.makedirs(OUTPUT_DIR_SPECIFIC, exist_ok=True)
        print(f"✅ Diretório de saída específico configurado: {OUTPUT_DIR_SPECIFIC}")
    except OSError:
        sys.exit(1)

def parse_process_metrics(file_path, algorithm_name, target_pids):
    """Lê um arquivo .dat e extrai o Turnaround Time apenas para os PIDs alvo."""
    
    if not os.path.exists(file_path): return []
        
    with open(file_path, 'r', encoding='utf-8') as f:
        content = f.read()

    results = []
    # Regex para capturar PID e Turnaround Time
    pattern = re.compile(r"PID (\d+) \| Wait=\d+ \| Turnaround=(\d+) \| CPU=\d+")
    
    for match in pattern.finditer(content):
        pid, turnaround = map(int, match.groups())
        if pid in target_pids:
            results.append({
                'Algoritmo': algorithm_name,
                'PID': pid,
                'Turnaround': turnaround
            })
    return results


setup_output_dir_specific()

TARGET_PIDS = {
    1: 'CPU-Bound (164c)',
    5: 'Memory-Bound (24c)',
    7: 'IO-Bound (12c)'
}
FILE_PATHS = {
    'FCFS': os.path.join(METRICS_BASE_DIR, 'metricas_FCFS.dat'),
    'PRIORITY': os.path.join(METRICS_BASE_DIR, 'metricas_PRIORITY.dat'),
    'RR': os.path.join(METRICS_BASE_DIR, 'metricas_RR.dat'),
    'SJN': os.path.join(METRICS_BASE_DIR, 'metricas_SJN.dat')
}

all_process_data = []
for algo, path in FILE_PATHS.items():
    all_process_data.extend(parse_process_metrics(path, algo, TARGET_PIDS.keys()))

if not all_process_data:
    print("ERRO: Nenhuma métrica de processo foi carregada. Verifique os caminhos e arquivos.")
    sys.exit(1)

df_specific = pd.DataFrame(all_process_data)
# Mapeia o PID para o tipo de carga de trabalho para melhor visualização
df_specific['Carga de Trabalho'] = df_specific['PID'].map(TARGET_PIDS)



def generate_grouped_bar_chart(data, x_col, y_col, hue_col, title, filename):
    
    plt.style.use('default') 
    sns.set_context("paper", font_scale=1.2)
    sns.set_style("white")
    
    plt.figure(figsize=(10, 6))
    
    palette_colors = sns.color_palette("Pastel1", n_colors=len(data[hue_col].unique()))
    
    ax = sns.barplot(
        x=x_col, 
        y=y_col, 
        hue=hue_col, 
        data=data, 
        palette=palette_colors,
        order=data[x_col].unique().tolist() 
    )
    
    for p in ax.patches:
        ax.annotate(f'{p.get_height():.0f}', 
                    (p.get_x() + p.get_width() / 2., p.get_height()),
                    ha = 'center', 
                    va = 'bottom', 
                    fontsize=9,
                    xytext = (0, 3),
                    textcoords = 'offset points')


    ax.spines['right'].set_visible(False)
    ax.spines['top'].set_visible(False)
    plt.title(title, fontsize=14, fontweight='bold', pad=15)
    plt.ylabel(y_col + ' (Ciclos)', fontsize=11)
    plt.xlabel('Algoritmo de Escalonamento', fontsize=11)
    plt.legend(title='Carga de Trabalho', loc='upper left')
    plt.grid(axis='y', linestyle=':', alpha=0.6)
    plt.tight_layout()
    plt.savefig(os.path.join(OUTPUT_DIR_SPECIFIC, filename), dpi=400)
    plt.close()



generate_grouped_bar_chart(
    df_specific, 
    x_col='Algoritmo', 
    y_col='Turnaround', 
    hue_col='Carga de Trabalho', 
    title='Turnaround Time por Algoritmo e Carga de Trabalho', 
    filename='comparacao_cargas_trabalho.png'
)

df_specific.to_csv(os.path.join(OUTPUT_DIR_SPECIFIC, 'comparacao_cargas_trabalho.csv'), index=False)

print("\n✅ Análise de Cargas de Trabalho concluída.")
print(f"O gráfico de comparação específica foi salvo em: {OUTPUT_DIR_SPECIFIC}")