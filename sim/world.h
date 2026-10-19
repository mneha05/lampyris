/*
 * A field of simulated fireflies running the real firmware core.
 *
 * Light physics: every node sees ambient light (with an optional dusk ramp)
 * plus sensor noise plus the flashes of its neighbours, attenuated as
 * gain / (1 + (d/r0)^2).  Nodes only "see" each other through this channel;
 * nothing else is shared.
 */
#ifndef LAMPYRIS_WORLD_H
#define LAMPYRIS_WORLD_H

#include <stdint.h>

#include "firefly.h"

#define WORLD_MAX 400

enum { LAYOUT_GRID, LAYOUT_SCATTER, LAYOUT_RING };

typedef struct {
    int                 n;
    const lp_species_t *species;
    uint32_t            seed;
    double              jitter_pct;   /* +- spread of each node's clock     */
    int                 layout;
    double              spacing;      /* distance between grid neighbours   */
    double              flash_gain;   /* reading from a flash at d = 0      */
    double              r0;           /* falloff radius                     */
    uint16_t            ambient_start;
    uint16_t            ambient_end;
    uint32_t            dusk_ms;      /* ramp ambient start -> end          */
    uint16_t            noise;        /* +- uniform sensor noise            */
    uint16_t            sample_ms;
} world_cfg_t;

typedef struct {
    world_cfg_t   cfg;
    uint32_t      t;
    uint32_t      rng;
    lp_species_t  sp[WORLD_MAX];
    lp_firefly_t  ff[WORLD_MAX];
    double        x[WORLD_MAX];
    double        y[WORLD_MAX];
    uint8_t       led[WORLD_MAX];
    uint16_t     *gain;               /* n*n, row = observer                */
} world_t;

void     world_cfg_default(world_cfg_t *c);
int      world_init(world_t *w, const world_cfg_t *c);
void     world_free(world_t *w);
void     world_step(world_t *w);
uint16_t world_ambient(const world_t *w);

/* Kuramoto order parameter r in [0,1]: 1 = perfect unison. */
double   world_order(const world_t *w);

#endif
