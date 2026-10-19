#include "oscillator.h"

static void recompute(lp_osc_t *o)
{
    uint32_t r;

    o->rate = (LP_PHASE_FULL << 4) / o->period_q4;
    r = o->rate * o->refractory_ms;
    o->refractory = r >= LP_PHASE_FULL ? LP_PHASE_FULL - 1 : r;
}

void lp_osc_init(lp_osc_t *o, uint16_t period_ms, uint16_t refractory_ms,
                 uint8_t coupling_q8, uint8_t epsilon_q8,
                 uint8_t adapt_shift, uint8_t adapt_range_pct)
{
    uint32_t span;

    if (period_ms < 20)
        period_ms = 20;
    o->phase = 0;
    o->period_q4 = (uint32_t)period_ms << 4;
    span = o->period_q4 / 100 * adapt_range_pct;
    o->period_min_q4 = o->period_q4 - span;
    o->period_max_q4 = o->period_q4 + span;
    o->period_nat_q4 = o->period_q4;
    o->refractory_ms = refractory_ms;
    o->coupling_q8 = coupling_q8;
    o->epsilon = (LP_PHASE_FULL >> 8) * epsilon_q8;
    o->adapt_shift = adapt_shift;
    recompute(o);
}

void lp_osc_set_phase(lp_osc_t *o, uint32_t phase)
{
    o->phase = phase % LP_PHASE_FULL;
}

/* Once per cycle, drift back toward the natural period.  Without this the
 * swarm ratchets: late pulses only ever speed nodes up, everyone ends up
 * pinned against their own (different) clamp, and the spread returns. */
static void relax(lp_osc_t *o)
{
    if (!o->adapt_shift || o->period_q4 == o->period_nat_q4)
        return;
    if (o->period_q4 > o->period_nat_q4)
        o->period_q4 -= (o->period_q4 - o->period_nat_q4 + 3) >> 2;
    else
        o->period_q4 += (o->period_nat_q4 - o->period_q4 + 3) >> 2;
    recompute(o);
}

bool lp_osc_tick(lp_osc_t *o, uint32_t dt_ms)
{
    uint32_t max_dt = 0x7FFFFFFFu / o->rate;
    uint32_t p;

    if (dt_ms > max_dt)
        dt_ms = max_dt;
    p = o->phase + dt_ms * o->rate;
    if (p >= LP_PHASE_FULL) {
        o->phase = p % LP_PHASE_FULL;
        relax(o);
        return true;
    }
    o->phase = p;
    return false;
}

bool lp_osc_pulse(lp_osc_t *o)
{
    uint32_t p;

    /* Right after our own flash we are deaf: this ignores the echo of our
     * own group and is what makes a synchronized group stable. */
    if (o->phase < o->refractory)
        return false;

    if (o->adapt_shift) {
        /* Pulse late in our cycle: the neighbour is ahead, so speed up.
         * Pulse early: the neighbour is behind, so slow down. */
        int32_t err = ((int32_t)o->phase - (int32_t)(LP_PHASE_FULL / 2)) / 4096;
        int32_t d = err * (int32_t)o->period_q4 / ((int32_t)1 << (10 + o->adapt_shift));
        int32_t np = (int32_t)o->period_q4 - d;

        if (np < (int32_t)o->period_min_q4)
            np = (int32_t)o->period_min_q4;
        if (np > (int32_t)o->period_max_q4)
            np = (int32_t)o->period_max_q4;
        if ((uint32_t)np != o->period_q4) {
            o->period_q4 = (uint32_t)np;
            recompute(o);
        }
    }

    p = o->phase + (o->phase >> 8) * o->coupling_q8 + o->epsilon;
    if (p >= LP_PHASE_FULL) {
        o->phase = 0;
        relax(o);
        return true;
    }
    o->phase = p;
    return false;
}
