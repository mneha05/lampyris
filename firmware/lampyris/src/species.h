/*
 * Species presets: timing, coupling and flash pattern.  Modelled loosely on
 * real synchronous fireflies, scaled where needed so a swarm is fun to watch.
 */
#ifndef LAMPYRIS_SPECIES_H
#define LAMPYRIS_SPECIES_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint16_t flash_ms;   /* LED on-time per flash                 */
    uint8_t  count;      /* flashes per burst                     */
    uint16_t spacing_ms; /* start-to-start spacing inside a burst */
} lp_pattern_t;

typedef struct {
    const char  *name;
    uint16_t     period_ms;
    uint16_t     refractory_ms;
    uint8_t      coupling_q8;
    uint8_t      epsilon_q8;
    uint8_t      adapt_shift;
    uint8_t      adapt_range_pct;
    lp_pattern_t pattern;
} lp_species_t;

/* One steady 1 Hz blink. Easiest to debug and to watch converge. */
extern const lp_species_t LP_SPECIES_DEMO;
/* Mangrove fireflies of Southeast Asia: fast single flashes, roughly twice
 * a second, with frequency adaptation switched on. */
extern const lp_species_t LP_SPECIES_PTEROPTYX;
/* Great Smoky Mountains: a burst of six quick flashes, then darkness. */
extern const lp_species_t LP_SPECIES_CAROLINUS;

/* NULL-terminated list, for CLIs and menus. */
extern const lp_species_t *const lp_species_all[];

const lp_species_t *lp_species_find(const char *name);

/* Is the LED on, given milliseconds since the last fire? */
bool lp_pattern_led(const lp_pattern_t *p, uint32_t since_fire_ms);

/* Length of a full burst in ms. */
uint32_t lp_pattern_length(const lp_pattern_t *p);

#ifdef __cplusplus
}
#endif

#endif
