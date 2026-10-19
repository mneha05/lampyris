#include "world.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void world_cfg_default(world_cfg_t *c)
{
    memset(c, 0, sizeof *c);
    c->n = 49;
    c->species = &LP_SPECIES_DEMO;
    c->seed = 1;
    c->jitter_pct = 3.0;
    c->layout = LAYOUT_GRID;
    c->spacing = 1.0;
    c->flash_gain = 600.0;
    c->r0 = 1.0;
    c->ambient_start = 200;
    c->ambient_end = 200;
    c->dusk_ms = 0;
    c->noise = 8;
    c->sample_ms = 1;
}

static double frand(uint32_t *s)
{
    return (lp_rng_next(s) >> 8) / 16777216.0;
}

int world_init(world_t *w, const world_cfg_t *c)
{
    int i, j, side;

    if (c->n < 1 || c->n > WORLD_MAX)
        return -1;
    memset(w, 0, sizeof *w);
    w->cfg = *c;
    w->rng = c->seed * 2654435761u + 1u;
    w->gain = calloc((size_t)c->n * (size_t)c->n, sizeof *w->gain);
    if (!w->gain)
        return -1;

    side = (int)ceil(sqrt((double)c->n));
    for (i = 0; i < c->n; i++) {
        switch (c->layout) {
        case LAYOUT_SCATTER:
            w->x[i] = frand(&w->rng) * side * c->spacing;
            w->y[i] = frand(&w->rng) * side * c->spacing;
            break;
        case LAYOUT_RING: {
            double a = 2.0 * M_PI * i / c->n;
            double rad = c->n * c->spacing / (2.0 * M_PI);
            w->x[i] = rad * (1.0 + cos(a));
            w->y[i] = rad * (1.0 + sin(a));
            break;
        }
        default:
            w->x[i] = (i % side) * c->spacing;
            w->y[i] = (i / side) * c->spacing;
            break;
        }
    }

    for (i = 0; i < c->n; i++) {
        double skew = (frand(&w->rng) * 2.0 - 1.0) * c->jitter_pct / 100.0;
        long period = lround(c->species->period_ms * (1.0 + skew));

        w->sp[i] = *c->species;
        w->sp[i].period_ms = (uint16_t)(period < 20 ? 20 : period > 65535 ? 65535 : period);
        lp_ff_init(&w->ff[i], &w->sp[i], lp_rng_next(&w->rng), c->sample_ms);
    }

    for (i = 0; i < c->n; i++) {
        for (j = 0; j < c->n; j++) {
            double dx = w->x[i] - w->x[j], dy = w->y[i] - w->y[j];
            double g = i == j ? 0.0 : c->flash_gain / (1.0 + (dx * dx + dy * dy) / (c->r0 * c->r0));
            w->gain[(size_t)i * c->n + j] = (uint16_t)(g > 65535.0 ? 65535.0 : g);
        }
    }
    return 0;
}

void world_free(world_t *w)
{
    free(w->gain);
    w->gain = NULL;
}

uint16_t world_ambient(const world_t *w)
{
    const world_cfg_t *c = &w->cfg;
    double k;

    if (c->dusk_ms == 0 || w->t >= c->dusk_ms)
        return c->ambient_end;
    k = (double)w->t / c->dusk_ms;
    return (uint16_t)(c->ambient_start + (c->ambient_end - (double)c->ambient_start) * k);
}

void world_step(world_t *w)
{
    static uint8_t next[WORLD_MAX];
    int n = w->cfg.n, i, j;
    uint16_t amb = world_ambient(w);

    for (i = 0; i < n; i++) {
        lp_firefly_t *f = &w->ff[i];
        uint16_t light = 0;

        if (w->t % w->cfg.sample_ms == 0 && lp_ff_wants_sample(f, w->t)) {
            const uint16_t *row = &w->gain[(size_t)i * n];
            int32_t v = amb;

            if (w->cfg.noise)
                v += (int32_t)(lp_rng_next(&w->rng) % (2u * w->cfg.noise + 1u)) - w->cfg.noise;
            for (j = 0; j < n; j++)
                if (w->led[j])
                    v += row[j];
            light = (uint16_t)(v < 0 ? 0 : v > 65535 ? 65535 : v);
        }
        if (w->t % w->cfg.sample_ms == 0)
            next[i] = lp_ff_step(f, w->t, light);
        else
            next[i] = w->led[i];
    }
    memcpy(w->led, next, (size_t)n);
    w->t++;
}

double world_order(const world_t *w)
{
    double sx = 0.0, sy = 0.0;
    int i;

    for (i = 0; i < w->cfg.n; i++) {
        double a = 2.0 * M_PI * lp_ff_phase(&w->ff[i]) / LP_PHASE_FULL;
        sx += cos(a);
        sy += sin(a);
    }
    return sqrt(sx * sx + sy * sy) / w->cfg.n;
}
