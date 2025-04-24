#ifndef GUEST_AGENT_H
#define GUEST_AGENT_H

int guest_agent_init(int vm_id);
void guest_agent_cleanup(int vm_id);
char *guest_agent_get_meminfo(int vm_id);

#endif // GUEST_AGENT_H
