/*
 * Flash detector: turns a raw light reading into "a neighbour just flashed"
 * events.
 *
 * It is an onset (edge) detector, not a level detector: it fires when the
 * light jumps above its own recent level.  A level detector goes deaf in a
 * busy swarm, where some neighbour is almost always glowing; an onset
 * detector still catches each new flash stacked on top of the others.
 * A slow ambient baseline scales the threshold so the same node works at
 * dusk, under a porch light, or in a dark room.
 */
#ifndef LAMPYRIS_DETECTOR_H
#define LAMPYRIS_DETECTOR_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t base_q4;     /* slow ambient baseline, 1/16 counts           */
    uint32_t level_q4;    /* fast-following recent level, 1/16 counts     */
    uint16_t min_delta;   /* absolute threshold floor                     */
    uint8_t  rel_shift;   /* relative threshold = baseline >> rel_shift   */
    uint8_t  tau_shift;   /* baseline time constant = 2^tau_shift samples */
    uint8_t  fast_shift;  /* level time constant = 2^fast_shift samples   */
    bool     armed;
    bool     primed;
} lp_det_t;

void lp_det_init(lp_det_t *d, uint16_t min_delta, uint8_t rel_shift,
                 uint8_t tau_shift, uint8_t fast_shift);

/* Feed one sample. Returns true on the onset of a flash. */
bool lp_det_feed(lp_det_t *d, uint16_t sample);

static inline uint16_t lp_det_baseline(const lp_det_t *d) { return (uint16_t)(d->base_q4 >> 4); }

#ifdef __cplusplus
}
#endif

#endif
