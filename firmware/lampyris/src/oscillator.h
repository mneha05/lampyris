/*
 * Pulse-coupled oscillator (Mirollo-Strogatz style) with optional
 * Ermentrout-style frequency adaptation.  Integer-only, no allocation,
 * safe on 8-bit AVR.
 *
 * Phase runs 0 .. LP_PHASE_FULL.  When it reaches the top the oscillator
 * "fires".  When a neighbour's flash is seen, phase is pushed forward by a
 * multiplicative + additive kick; if the kick crosses the top, we fire on
 * the spot and are absorbed into the neighbour's group.
 */
#ifndef LAMPYRIS_OSCILLATOR_H
#define LAMPYRIS_OSCILLATOR_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LP_PHASE_BITS 22
#define LP_PHASE_FULL ((uint32_t)1 << LP_PHASE_BITS)

typedef struct {
    uint32_t phase;         /* 0 .. LP_PHASE_FULL-1                        */
    uint32_t rate;          /* phase units per millisecond                 */
    uint32_t period_q4;     /* current period, 1/16 ms                     */
    uint32_t period_min_q4;
    uint32_t period_max_q4;
    uint32_t period_nat_q4; /* natural period; adaptation relaxes to it    */
    uint32_t refractory;    /* phase below which pulses are ignored        */
    uint32_t epsilon;       /* additive kick, phase units                  */
    uint16_t refractory_ms;
    uint8_t  coupling_q8;   /* multiplicative kick: phase *= 1 + c/256     */
    uint8_t  adapt_shift;   /* frequency adaptation gain 2^-shift, 0 = off */
} lp_osc_t;

void lp_osc_init(lp_osc_t *o, uint16_t period_ms, uint16_t refractory_ms,
                 uint8_t coupling_q8, uint8_t epsilon_q8,
                 uint8_t adapt_shift, uint8_t adapt_range_pct);

/* Advance by dt_ms. Returns true if the oscillator fired. */
bool lp_osc_tick(lp_osc_t *o, uint32_t dt_ms);

/* A neighbour flash was seen. Returns true if it made us fire now. */
bool lp_osc_pulse(lp_osc_t *o);

void lp_osc_set_phase(lp_osc_t *o, uint32_t phase);

static inline uint32_t lp_osc_period_ms(const lp_osc_t *o) { return o->period_q4 >> 4; }

#ifdef __cplusplus
}
#endif

#endif
