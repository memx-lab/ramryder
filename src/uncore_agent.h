#ifndef UNCORE_AGENT_H
#define UNCORE_AGENT

#include <unistd.h>
#include <pcm/pcm_c_public.h>

int uncore_agent_init(void);
void uncore_agent_cleanup(void);
void uncore_agent_get_bandwidth(memdata_t *md, bool output);

#endif // UNCORE_AGENT