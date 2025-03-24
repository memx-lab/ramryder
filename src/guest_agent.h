#ifndef GUEST_AGENT_H
#define GUEST_AGENT_H

int guest_agent_init(void);
void guest_agent_cleanup(void);
char *query_vm_status(void);

#endif // GUEST_AGENT_H
