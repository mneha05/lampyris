/*
 * One firefly: oscillator + eye + lantern.
 *
 *   light sample --> detector --> oscillator --> flash pattern --> LED
 *                       ^                                      |
 *                       +------- blinded while we glow --------+
 *
 * The node is fully described by this struct (~60 bytes), so an ATtiny85
 * with 512 B of RAM has room to spare.
 */
#ifndef LAMPYRIS_FIREFLY_H
#define LAMPYRIS_FIREFLY_H

#include <stdint.h>
#include <stdbool.h>

#include "detector.h"
#include "oscillator.h"
#include "species.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    lp_osc_t            osc;
    lp_det_t            det;
    const lp_species_t *sp;
    uint32_t            now;
    uint32_t            fire_at;
    uint32_t            led_off_at;
    uint32_t            fires;
    uint32_t            heard;      /* flashes detected                  */
    uint32_t            absorbed;   /* times a flash made us fire early   */
    uint16_t            blind_ms;   /* stay deaf this long after glowing */
    bool                led;
    bool                fired_once;
    bool                started;
} lp_firefly_t;

/* sample_ms: how often lp_ff_step() is called (sets detector time scales).
 * seed: any per-node random value; picks the starting phase. */
void lp_ff_init(lp_firefly_t *f, const lp_species_t *sp, uint32_t seed,
                uint16_t sample_ms);

/* Is the eye open right now? Lets hardware skip a slow light measurement
 * (and with LED-as-sensor, the LED cannot see while it shines anyway). */
bool lp_ff_wants_sample(const lp_firefly_t *f, uint32_t now_ms);

/* Run one step. light is ignored while blind. Returns LED state. */
bool lp_ff_step(lp_firefly_t *f, uint32_t now_ms, uint16_t light);

static inline uint32_t lp_ff_phase(const lp_firefly_t *f) { return f->osc.phase; }

uint32_t lp_rng_next(uint32_t *state);

#ifdef __cplusplus
}
#endif

#endif
