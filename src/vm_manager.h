#ifndef VM_MANAGER_H
#define VM_MANAGER_H

int vm_mngr_instance_create(int vm_id, int pid, char *core_set);
void vm_mngr_instance_destroy(int vm_id);
void update_perf_counters(void);

#endif