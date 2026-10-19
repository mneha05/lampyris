#include "firefly.h"

#define LP_BLIND_GUARD_MS   25u   /* LED-as-sensor needs a moment to recover */
#define LP_DET_MIN_DELTA    40u
#define LP_DET_REL_SHIFT    2u    /* flash must beat ambient by 25%          */
#define LP_DET_TAU_MS       2048u /* ambient baseline time constant       */
#define LP_DET_FAST_MS      8u    /* recent-level time constant           */

static uint8_t shift_for(uint32_t tau_ms, uint16_t sample_ms)
{
    uint8_t s = 0;

    while (((uint32_t)sample_ms << (s + 1)) <= tau_ms && s < 15)
        s++;
    return s;
}

uint32_t lp_rng_next(uint32_t *state)
{
    uint32_t x = *state ? *state : 0x9E3779B9u;

    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

void lp_ff_init(lp_firefly_t *f, const lp_species_t *sp, uint32_t seed,
                uint16_t sample_ms)
{
    if (sample_ms == 0)
        sample_ms = 1;

    f->sp = sp;
    lp_osc_init(&f->osc, sp->period_ms, sp->refractory_ms, sp->coupling_q8,
                sp->epsilon_q8, sp->adapt_shift, sp->adapt_range_pct);
    lp_osc_set_phase(&f->osc, lp_rng_next(&seed) % LP_PHASE_FULL);
    lp_det_init(&f->det, LP_DET_MIN_DELTA, LP_DET_REL_SHIFT,
                shift_for(LP_DET_TAU_MS, sample_ms),
                shift_for(LP_DET_FAST_MS, sample_ms));
    f->now = 0;
    f->fire_at = 0;
    f->led_off_at = 0;
    f->fires = 0;
    f->heard = 0;
    f->absorbed = 0;
    f->blind_ms = LP_BLIND_GUARD_MS;
    f->led = false;
    f->fired_once = false;
    f->started = false;
}

static void fire(lp_firefly_t *f)
{
    f->fire_at = f->now;
    f->fired_once = true;
    f->fires++;
}

bool lp_ff_wants_sample(const lp_firefly_t *f, uint32_t now_ms)
{
    return !f->led && (uint32_t)(now_ms - f->led_off_at) >= f->blind_ms;
}

bool lp_ff_step(lp_firefly_t *f, uint32_t now_ms, uint16_t light)
{
    uint32_t dt = f->started ? (uint32_t)(now_ms - f->now) : 0;
    bool led;

    f->now = now_ms;
    f->started = true;

    if (lp_osc_tick(&f->osc, dt))
        fire(f);

    if (lp_ff_wants_sample(f, now_ms) && lp_det_feed(&f->det, light)) {
        f->heard++;
        if (lp_osc_pulse(&f->osc)) {
            f->absorbed++;
            fire(f);
        }
    }

    led = f->fired_once &&
          lp_pattern_led(&f->sp->pattern, (uint32_t)(now_ms - f->fire_at));
    if (f->led && !led)
        f->led_off_at = now_ms;
    f->led = led;
    return led;
}
