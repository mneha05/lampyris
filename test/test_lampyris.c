/* Unit and swarm-level tests. No framework: build and run, exit code 0 = pass. */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "world.h"

static int failures, checks;

#define CHECK(cond, ...) do {                                         \
        checks++;                                                     \
        if (!(cond)) {                                                \
            failures++;                                               \
            printf("  FAIL %s:%d: ", __FILE__, __LINE__);             \
            printf(__VA_ARGS__);                                      \
            putchar('\n');                                            \
        }                                                             \
    } while (0)

#define FRAC(x) ((uint32_t)((x) * LP_PHASE_FULL))

/* ---- oscillator ------------------------------------------------------ */

static void test_free_running_period(void)
{
    lp_osc_t o;
    uint32_t t, last = 0, fires = 0;

    lp_osc_init(&o, 1000, 300, 64, 3, 0, 0);
    for (t = 1; t <= 10050; t++) {
        if (lp_osc_tick(&o, 1)) {
            if (fires)
                CHECK(t - last >= 999 && t - last <= 1001, "period %lu", (unsigned long)(t - last));
            last = t;
            fires++;
        }
    }
    CHECK(fires == 10, "fired %lu times in 10 s", (unsigned long)fires);
}

static void test_refractory_ignores_pulse(void)
{
    lp_osc_t o;

    lp_osc_init(&o, 1000, 300, 64, 3, 0, 0);
    lp_osc_set_phase(&o, FRAC(0.10));
    CHECK(!lp_osc_pulse(&o), "fired during refractory");
    CHECK(o.phase == FRAC(0.10), "phase moved during refractory");
}

static void test_pulse_advances_and_absorbs(void)
{
    lp_osc_t o;
    uint32_t before;

    lp_osc_init(&o, 1000, 300, 64, 3, 0, 0);
    lp_osc_set_phase(&o, FRAC(0.50));
    before = o.phase;
    CHECK(!lp_osc_pulse(&o), "mid-cycle pulse fired");
    CHECK(o.phase > before + FRAC(0.10), "advance too small: %f",
          (o.phase - before) / (double)LP_PHASE_FULL);

    lp_osc_set_phase(&o, FRAC(0.85));
    CHECK(lp_osc_pulse(&o), "late pulse did not absorb");
    CHECK(o.phase == 0, "phase not reset after absorption");
}

static void test_frequency_adaptation(void)
{
    lp_osc_t o;
    uint32_t nat, i;

    lp_osc_init(&o, 1000, 100, 64, 3, 6, 15);
    nat = o.period_q4;

    lp_osc_set_phase(&o, FRAC(0.70));
    lp_osc_pulse(&o);
    CHECK(o.period_q4 < nat, "late pulse should shorten period");

    lp_osc_init(&o, 1000, 100, 64, 3, 6, 15);
    lp_osc_set_phase(&o, FRAC(0.20));
    lp_osc_pulse(&o);
    CHECK(o.period_q4 > nat, "early pulse should lengthen period");

    /* 0.6 is late but not late enough to absorb (absorbing would relax). */
    for (i = 0; i < 200; i++) {
        lp_osc_set_phase(&o, FRAC(0.60));
        lp_osc_pulse(&o);
    }
    CHECK(o.period_q4 >= o.period_min_q4, "period below clamp");
    CHECK(o.period_q4 <= nat * 86 / 100 + 16, "clamp not reached: %lu", (unsigned long)o.period_q4);

    for (i = 0; i < 40; i++)
        lp_osc_tick(&o, 1000);
    CHECK(o.period_q4 == nat, "did not relax back to natural period: %lu vs %lu",
          (unsigned long)o.period_q4, (unsigned long)nat);
}

/* ---- detector -------------------------------------------------------- */

static int count_onsets(lp_det_t *d, const uint16_t *s, int n)
{
    int i, k = 0;

    for (i = 0; i < n; i++)
        k += lp_det_feed(d, s[i]);
    return k;
}

static void test_detector_step_and_stack(void)
{
    static uint16_t s[3000];
    lp_det_t d;
    int i;

    for (i = 0; i < 3000; i++)
        s[i] = 200;
    for (i = 1000; i < 1120; i++)
        s[i] += 300;              /* neighbour A flashes          */
    for (i = 1050; i < 1170; i++)
        s[i] += 300;              /* B starts while A still glows */
    lp_det_init(&d, 40, 2, 9, 3);
    CHECK(count_onsets(&d, s, 3000) == 2, "expected two onsets for stacked flashes");
}

static void test_detector_ignores_dusk_and_noise(void)
{
    lp_det_t d;
    uint32_t rng = 7;
    int i, k = 0;

    lp_det_init(&d, 40, 2, 9, 3);
    for (i = 0; i < 30000; i++) {
        double amb = 6000.0 - 5800.0 * i / 30000.0;
        int noise = (int)(lp_rng_next(&rng) % 17) - 8;

        k += lp_det_feed(&d, (uint16_t)(amb + noise));
    }
    CHECK(k == 0, "%d false onsets during dusk", k);
}

static void test_detector_porch_light(void)
{
    lp_det_t d;
    int i, k = 0, later = 0;

    lp_det_init(&d, 40, 2, 9, 3);
    for (i = 0; i < 1000; i++)
        k += lp_det_feed(&d, 200);
    for (i = 0; i < 5000; i++)
        k += lp_det_feed(&d, 3000);      /* someone switches a light on */
    CHECK(k == 1, "light switch should cost exactly one onset, got %d", k);
    for (i = 0; i < 3000; i++)
        later += lp_det_feed(&d, (uint16_t)(3000 + ((i % 1000) < 100 ? 1500 : 0)));
    CHECK(later == 3, "should still see flashes under the light, got %d", later);
}

/* ---- pattern / firefly ----------------------------------------------- */

static void test_carolinus_burst(void)
{
    uint32_t t;
    int edges = 0, prev = 0;

    for (t = 0; t < 8000; t++) {
        int on = lp_pattern_led(&LP_SPECIES_CAROLINUS.pattern, t);

        edges += on && !prev;
        prev = on;
    }
    CHECK(edges == 6, "carolinus burst has %d flashes", edges);
    CHECK(lp_pattern_length(&LP_SPECIES_CAROLINUS.pattern) < LP_SPECIES_CAROLINUS.refractory_ms,
          "burst must fit inside refractory or a node would hear its own group");
}

static void test_firefly_is_blind_while_glowing(void)
{
    lp_firefly_t f;
    uint32_t t;
    int saw_led = 0;

    lp_ff_init(&f, &LP_SPECIES_DEMO, 123, 1);
    for (t = 0; t < 3000; t++) {
        bool led = lp_ff_step(&f, t, 200);

        if (led) {
            saw_led = 1;
            CHECK(!lp_ff_wants_sample(&f, t), "sampling while LED on at t=%lu", (unsigned long)t);
        }
    }
    CHECK(saw_led, "never flashed");
    CHECK(f.heard == 0, "heard %lu flashes in a dark empty field", (unsigned long)f.heard);
}

/* ---- swarm ----------------------------------------------------------- */

static int run_locks(world_cfg_t *c, uint32_t ms, double *final_r)
{
    static world_t w;
    uint32_t t;
    double min_tail = 1.0, r = 0.0;

    if (world_init(&w, c))
        return 0;
    for (t = 0; t < ms; t++) {
        world_step(&w);
        if (t >= ms * 4 / 5 && t % 50 == 0) {
            r = world_order(&w);
            if (r < min_tail)
                min_tail = r;
        }
    }
    world_free(&w);
    if (final_r)
        *final_r = r;
    return min_tail >= 0.90;
}

static void swarm_case(const char *label, const lp_species_t *sp, int n, int layout,
                       double jitter, uint32_t ms, int seeds, int need)
{
    world_cfg_t c;
    int s, ok = 0;

    for (s = 1; s <= seeds; s++) {
        world_cfg_default(&c);
        c.species = sp;
        c.n = n;
        c.layout = layout;
        c.jitter_pct = jitter;
        c.seed = (uint32_t)s;
        ok += run_locks(&c, ms, NULL);
    }
    printf("  swarm %-34s %2d/%d locked\n", label, ok, seeds);
    CHECK(ok >= need, "%s: only %d/%d seeds locked (need %d)", label, ok, seeds, need);
}

static void test_swarms(void)
{
    swarm_case("demo 49 grid +-3%", &LP_SPECIES_DEMO, 49, LAYOUT_GRID, 3.0, 30000, 20, 20);
    swarm_case("demo 100 scatter +-5%", &LP_SPECIES_DEMO, 100, LAYOUT_SCATTER, 5.0, 40000, 10, 10);
    swarm_case("pteroptyx 49 grid +-12%", &LP_SPECIES_PTEROPTYX, 49, LAYOUT_GRID, 12.0, 30000, 20, 19);
    swarm_case("carolinus 36 grid +-6%", &LP_SPECIES_CAROLINUS, 36, LAYOUT_GRID, 6.0, 300000, 10, 8);
}

static void test_no_light_no_sync(void)
{
    world_cfg_t c;
    double r;

    world_cfg_default(&c);
    c.flash_gain = 0.0;     /* blindfold everyone */
    c.seed = 5;
    CHECK(!run_locks(&c, 30000, &r), "synchronized without seeing each other (r=%.3f)", r);
}

static void test_deterministic(void)
{
    world_cfg_t c;
    double a, b;

    world_cfg_default(&c);
    c.seed = 42;
    run_locks(&c, 5000, &a);
    run_locks(&c, 5000, &b);
    CHECK(a == b, "same seed gave r=%.6f then %.6f", a, b);
}

int main(void)
{
    puts("lampyris tests");
    test_free_running_period();
    test_refractory_ignores_pulse();
    test_pulse_advances_and_absorbs();
    test_frequency_adaptation();
    test_detector_step_and_stack();
    test_detector_ignores_dusk_and_noise();
    test_detector_porch_light();
    test_carolinus_burst();
    test_firefly_is_blind_while_glowing();
    test_no_light_no_sync();
    test_deterministic();
    test_swarms();
    printf("%d checks, %d failed\n", checks, failures);
    return failures ? 1 : 0;
}
