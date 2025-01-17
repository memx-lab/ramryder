import pandas as pd
import matplotlib.pyplot as plt

data = pd.read_csv('data/filtered_machine_1936_usage.csv', header=None)

data.columns = [
    'machine_id', 'time_stamp', 'cpu_util_percent', 'mem_util_percent',
    'mem_gps', 'mkpi', 'net_in', 'net_out', 'disk_io_percent'
]

start_time = data['time_stamp'].min()
data['elapsed_time'] = (data['time_stamp'] - start_time) / 3600

elapsed_time = data['elapsed_time']
cpu_util = data['cpu_util_percent']
mem_util = data['mem_util_percent']
mem_gps = data['mem_gps']

plt.figure(figsize=(6, 3.5))
plt.plot(elapsed_time, cpu_util, label='CPU')
plt.plot(elapsed_time, mem_util, label='Memory Capacity')
plt.plot(elapsed_time, mem_gps, label='Memory Bandwidth')

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
plt.savefig('figures/machine_usage_alibaba.pdf', format='pdf', dpi=300)
plt.show()
