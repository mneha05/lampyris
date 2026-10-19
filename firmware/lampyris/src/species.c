#include "species.h"

#include <string.h>

/*                                    name         period refr  c  eps adapt range  pattern        */
const lp_species_t LP_SPECIES_DEMO      = { "demo",      1000,  300, 64, 3, 0,  0, { 120, 1,   0 } };
const lp_species_t LP_SPECIES_PTEROPTYX = { "pteroptyx",  560,  170, 96, 3, 6, 15, {  90, 1,   0 } };
const lp_species_t LP_SPECIES_CAROLINUS = { "carolinus", 8000, 3200, 64, 2, 6, 10, {  90, 6, 450 } };

const lp_species_t *const lp_species_all[] = {
    &LP_SPECIES_DEMO,
    &LP_SPECIES_PTEROPTYX,
    &LP_SPECIES_CAROLINUS,
    0
};

const lp_species_t *lp_species_find(const char *name)
{
    int i;

    for (i = 0; lp_species_all[i]; i++)
        if (strcmp(lp_species_all[i]->name, name) == 0)
            return lp_species_all[i];
    return 0;
}

bool lp_pattern_led(const lp_pattern_t *p, uint32_t since_fire_ms)
{
    uint32_t slot;

    if (p->count == 0)
        return false;
    if (p->count == 1 || p->spacing_ms == 0)
        return since_fire_ms < p->flash_ms;
    slot = since_fire_ms / p->spacing_ms;
    if (slot >= p->count)
        return false;
    return since_fire_ms % p->spacing_ms < p->flash_ms;
}

uint32_t lp_pattern_length(const lp_pattern_t *p)
{
    if (p->count <= 1 || p->spacing_ms == 0)
        return p->flash_ms;
    return (uint32_t)(p->count - 1) * p->spacing_ms + p->flash_ms;
}
