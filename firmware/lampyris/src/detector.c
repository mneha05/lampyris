#include "detector.h"

void lp_det_init(lp_det_t *d, uint16_t min_delta, uint8_t rel_shift,
                 uint8_t tau_shift, uint8_t fast_shift)
{
    d->base_q4 = 0;
    d->level_q4 = 0;
    d->min_delta = min_delta;
    d->rel_shift = rel_shift;
    d->tau_shift = tau_shift;
    d->fast_shift = fast_shift;
    d->armed = true;
    d->primed = false;
}

static void ema(uint32_t *acc, uint32_t xq, uint8_t shift)
{
    if (xq > *acc)
        *acc += (xq - *acc) >> shift;
    else
        *acc -= (*acc - xq) >> shift;
}

bool lp_det_feed(lp_det_t *d, uint16_t sample)
{
    uint32_t xq = (uint32_t)sample << 4;
    uint32_t level, thr;
    bool onset = false;

    if (!d->primed) {
        d->base_q4 = xq;
        d->level_q4 = xq;
        d->primed = true;
        return false;
    }

    level = d->level_q4 >> 4;
    thr = (d->base_q4 >> 4) >> d->rel_shift;
    if (thr < d->min_delta)
        thr = d->min_delta;

    if (d->armed && sample > level + thr) {
        d->armed = false;
        onset = true;
    } else if (!d->armed && sample < level + thr / 2) {
        /* The level has caught up with the new brightness: ready for the
         * next flash, even if this one is still glowing. */
        d->armed = true;
    }

    ema(&d->level_q4, xq, d->fast_shift);
    ema(&d->base_q4, xq, d->tau_shift);
    return onset;
}
