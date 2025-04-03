#ifndef GUEST_AGENT_H
#define GUEST_AGENT_H

#define MAX_NUM_VM 16

int guest_agent_init(int vm_id);
void guest_agent_cleanup(int vm_id);
char *guest_agent_get_meminfo(int vm_id);
int guest_agent_get_num_vm(void);

#endif // GUEST_AGENT_H
