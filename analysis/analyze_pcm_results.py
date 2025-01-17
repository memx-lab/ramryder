import pandas as pd
import matplotlib.pyplot as plt
from datetime import datetime

file_path = 'data/mlrm-128mnb-60s.csv'
header = pd.read_csv(file_path, nrows=2, header=None)
data = pd.read_csv(file_path, skiprows=2, header=None)
data.columns = pd.MultiIndex.from_tuples(list(zip(header.iloc[0], header.iloc[1])))

combined_time = data[('System', 'Date')] + ' ' + data[('System', 'Time')]
data['Timestamp'] = pd.to_datetime(combined_time, format='%Y-%m-%d %H:%M:%S.%f')
start_time = data['Timestamp'].min()
data['elapsed_time'] = 1.5 * (data['Timestamp'] - start_time).dt.total_seconds() / 3600

# sampling every three minutes
data = data.iloc[::3, :]

elapsed_time = data['elapsed_time']
mem_util = data[('System', 'MemUtil')].str.rstrip('%').astype(float)
cpu_util = data[('System', 'CpuUtil')].str.rstrip('%').astype(float)
sys_mem_total_bw = data[('System', 'SysMem Total (MB/s)')]

mem_util = (mem_util) * (180.0 / 80)
cpu_util = (cpu_util) * (80 / 64)
# assume total BW is 240GB/s
sys_mem_bw_util = (sys_mem_total_bw / (210 * 1024)) * 100

plt.figure(figsize=(6, 3.5))
plt.plot(elapsed_time, cpu_util, label='CPU')
plt.plot(elapsed_time, mem_util, label='Memory Capacity')
plt.plot(elapsed_time, sys_mem_bw_util, label='Memory Bandwidth')

plt.xlabel('Time (hours)', fontsize=12)
plt.ylabel('Resource Utilization (%)', fontsize=12)
plt.xticks(fontsize=11)
plt.yticks(fontsize=11)

ticks = range(0, int(elapsed_time.max()) + 1, 6)
plt.xticks(ticks)

plt.xlim(0, min(elapsed_time.max(), 36))
plt.ylim(0, 100)

plt.legend(fontsize=10.5, loc='upper center', bbox_to_anchor=(0.5, 1.15), ncol=3)
plt.grid(True)

plt.tight_layout()
plt.savefig('figures/machine_usage_mlrm.pdf', format='pdf', dpi=300)
plt.show()
