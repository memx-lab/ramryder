#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <pcm/pcm_c_public.h>
#include "uncore_agent.h"

int uncore_agent_init(void)
{
    if (pcm_c_init() != 0) {
        perror("Filed to init  PCM\n");
        return -1;
    }
 
    /* Start to record uncore counter usage */
    pcm_c_start();

    return 0;
}

void uncore_agent_cleanup(void)
{
    pcm_c_cleanup();
}

void uncore_agent_get_bandwidth(memdata_t *md, bool output)
{
    pcm_c_get_bandwidth(md, output);
}