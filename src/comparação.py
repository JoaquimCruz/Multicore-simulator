import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
import re
import os
import sys

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
OUTPUT_DIR = os.path.join('..', 'build', 'output', 'graficos', 'comparacao_carga_de_trabalho') 

def setup_output_dir_specific():
    """Cria a pasta de saída (comparação_carga_de_trabalho) se ela não existir."""
    try:
        os.makedirs(OUTPUT_DIR, exist_ok=True)
        print(f"✅ Diretório de saída específico configurado: {OUTPUT_DIR}")
    except OSError:
        print(f"❌ ERRO: Não foi possível criar o diretório de saída: {OUTPUT_DIR}")
        sys.exit(1)

def parse_process_metrics(file_path, algorithm_name, target_pids):
    """Lê um arquivo .dat e extrai o Turnaround Time apenas para os PIDs alvo."""
    
    if not os.path.exists(file_path): return []
        
    with open(file_path, 'r', encoding='utf-8') as f:
        content = f.read()

    results = []
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
df_specific['Carga de Trabalho'] = df_specific['PID'].map(TARGET_PIDS)


def generate_grouped_bar_chart(data, x_col, y_col, hue_col, title, filename):
    
    plt.figure(figsize=(10, 6))
    
    palette_colors = sns.color_palette("viridis", n_colors=len(data[hue_col].unique()))
    
    ax = sns.barplot(
        x=x_col, 
        y=y_col, 
        hue=hue_col, 
        data=data, 
        palette=palette_colors,
        order=data[x_col].unique().tolist() 
    )
    

    for p in ax.patches:
        height = p.get_height()
        ax.annotate(
            f'{height:.0f}', 
            (p.get_x() + p.get_width() / 2., height),
            ha = 'center', 
            va = 'bottom', 
            fontsize=10, 
            fontweight='bold', 
            xytext = (0, 5), 
            textcoords = 'offset points'
        )

    ax.spines['right'].set_visible(False)
    ax.spines['top'].set_visible(False)
    ax.grid(axis='y', linestyle='-', alpha=0.3)
    ax.grid(axis='x', visible=False)
    
    ax.set_title(title, pad=15, weight='bold')
    ax.set_ylabel(y_col + ' (unid. de tempo)', weight='bold') 
    ax.set_xlabel('Algoritmo de Escalonamento', weight='bold')  
      
    ax.legend(
        title='Carga de Trabalho', 
        loc='upper left',
        bbox_to_anchor=(1.01, 1), 
        frameon=True,
    )

    plt.tight_layout()
    plt.savefig(os.path.join(OUTPUT_DIR, filename), dpi=300) 
    plt.close()


print("\nGerando Gráfico de Comparação de Cargas de Trabalho...")

generate_grouped_bar_chart(
    df_specific, 
    x_col='Algoritmo', 
    y_col='Turnaround', 
    hue_col='Carga de Trabalho', 
    title='Turnaround Time por Algoritmo e Tipo de Carga de Trabalho', 
    filename='comparacao_cargas_trabalho.png'
)


df_specific.to_csv(os.path.join(OUTPUT_DIR, 'comparacao_cargas_trabalho.csv'), index=False)

print("\n✅ Análise de Cargas de Trabalho concluída.")
print(f"O gráfico de comparação específica foi salvo em: {OUTPUT_DIR}")