#ifndef GUEST_MONITOR_H
#define GUEST_MONITOR_H

void guest_monitor_server_stop(void);
int guest_monitor_server_start(const char* config_file);
int guest_monitor_remove_vm(int vm_id);
int guest_monitor_add_vm(int vm_id);

#endif // GUEST_MONITOR_H
