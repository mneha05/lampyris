/*
 * swarm: watch a field of lampyris nodes fall into step.
 *
 *   swarm                          49 nodes, demo species, summary + raster
 *   swarm --live                   animate the field in the terminal
 *   swarm -s carolinus -n 64 -t 300 --svg docs/carolinus.svg
 */
#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 199309L
#endif

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#endif

#include "world.h"

#define R_BIN_MS   100u
#define LED_BIN_MS 20u

typedef struct {
    world_cfg_t cfg;
    double      seconds;
    int         live;
    int         quiet;
    const char *svg;
    const char *csv;
} opts_t;

static void usage(void)
{
    int i;

    puts("usage: swarm [options]\n"
         "  -n N               number of fireflies (1..400, default 49)\n"
         "  -s SPECIES         species preset (default demo)\n"
         "  -t SECONDS         simulated time (default 60)\n"
         "  --seed S           random seed (default 1)\n"
         "  --jitter PCT       +- clock mismatch between nodes (default 3)\n"
         "  --layout L         grid | scatter | ring (default grid)\n"
         "  --spacing D        distance between neighbours (default 1.0)\n"
         "  --gain G           flash brightness at the source (default 600)\n"
         "  --dusk SECONDS     start in daylight and fade to night\n"
         "  --noise N          +- sensor noise in counts (default 8)\n"
         "  --live             animate in the terminal (real time)\n"
         "  --svg FILE         write a raster plot\n"
         "  --csv FILE         write t_ms,order_parameter\n"
         "  -q                 only print the summary line\n"
         "species:");
    for (i = 0; lp_species_all[i]; i++)
        printf("  %-10s period %5u ms, %u flash%s\n", lp_species_all[i]->name,
               lp_species_all[i]->period_ms, lp_species_all[i]->pattern.count,
               lp_species_all[i]->pattern.count == 1 ? "" : "es");
}

static int parse(int argc, char **argv, opts_t *o)
{
    int i;

    world_cfg_default(&o->cfg);
    o->seconds = 60.0;
    o->live = 0;
    o->quiet = 0;
    o->svg = NULL;
    o->csv = NULL;

    for (i = 1; i < argc; i++) {
        const char *a = argv[i];
        const char *v = i + 1 < argc ? argv[i + 1] : NULL;

#define NEEDV() do { if (!v) { fprintf(stderr, "%s needs a value\n", a); return -1; } i++; } while (0)
        if (!strcmp(a, "-h") || !strcmp(a, "--help")) {
            usage();
            exit(0);
        } else if (!strcmp(a, "-n")) {
            NEEDV(); o->cfg.n = atoi(v);
        } else if (!strcmp(a, "-s") || !strcmp(a, "--species")) {
            NEEDV();
            o->cfg.species = lp_species_find(v);
            if (!o->cfg.species) {
                fprintf(stderr, "unknown species '%s'\n", v);
                return -1;
            }
        } else if (!strcmp(a, "-t") || !strcmp(a, "--seconds")) {
            NEEDV(); o->seconds = atof(v);
        } else if (!strcmp(a, "--seed")) {
            NEEDV(); o->cfg.seed = (uint32_t)strtoul(v, NULL, 10);
        } else if (!strcmp(a, "--jitter")) {
            NEEDV(); o->cfg.jitter_pct = atof(v);
        } else if (!strcmp(a, "--layout")) {
            NEEDV();
            if (!strcmp(v, "grid")) o->cfg.layout = LAYOUT_GRID;
            else if (!strcmp(v, "scatter")) o->cfg.layout = LAYOUT_SCATTER;
            else if (!strcmp(v, "ring")) o->cfg.layout = LAYOUT_RING;
            else { fprintf(stderr, "unknown layout '%s'\n", v); return -1; }
        } else if (!strcmp(a, "--spacing")) {
            NEEDV(); o->cfg.spacing = atof(v);
        } else if (!strcmp(a, "--gain")) {
            NEEDV(); o->cfg.flash_gain = atof(v);
        } else if (!strcmp(a, "--dusk")) {
            NEEDV();
            o->cfg.dusk_ms = (uint32_t)(atof(v) * 1000.0);
            o->cfg.ambient_start = 6000;
        } else if (!strcmp(a, "--noise")) {
            NEEDV(); o->cfg.noise = (uint16_t)atoi(v);
        } else if (!strcmp(a, "--live")) {
            o->live = 1;
        } else if (!strcmp(a, "--svg")) {
            NEEDV(); o->svg = v;
        } else if (!strcmp(a, "--csv")) {
            NEEDV(); o->csv = v;
        } else if (!strcmp(a, "-q")) {
            o->quiet = 1;
        } else {
            fprintf(stderr, "unknown option '%s' (try --help)\n", a);
            return -1;
        }
#undef NEEDV
    }
    if (o->cfg.n < 1 || o->cfg.n > WORLD_MAX || o->seconds <= 0.0) {
        fprintf(stderr, "bad -n or -t\n");
        return -1;
    }
    return 0;
}

static void sleep_ms(unsigned ms)
{
#ifdef _WIN32
    Sleep(ms);
#else
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (long)(ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
#endif
}

static void draw_live(const world_t *w, double r)
{
    enum { COLS = 64, ROWS = 24 };
    static char cell[ROWS][COLS];
    double maxx = 1e-9, maxy = 1e-9;
    int i, row, col;

    for (i = 0; i < w->cfg.n; i++) {
        if (w->x[i] > maxx) maxx = w->x[i];
        if (w->y[i] > maxy) maxy = w->y[i];
    }
    memset(cell, ' ', sizeof cell);
    for (i = 0; i < w->cfg.n; i++) {
        col = (int)(w->x[i] / maxx * (COLS - 2) + 0.5);
        row = (int)(w->y[i] / maxy * (ROWS - 1) + 0.5);
        if (cell[row][col] != '@')
            cell[row][col] = w->led[i] ? '@' : '.';
    }
    fputs("\x1b[H", stdout);
    for (row = 0; row < ROWS; row++) {
        for (col = 0; col < COLS; col++) {
            if (cell[row][col] == '@')
                fputs("\x1b[1;93m@\x1b[0m", stdout);
            else if (cell[row][col] == '.')
                fputs("\x1b[2;32m.\x1b[0m", stdout);
            else
                putchar(' ');
        }
        putchar('\n');
    }
    printf("\x1b[K t = %6.1f s   sync r = %.3f   ambient %5u   (ctrl-c to quit)\n",
           w->t / 1000.0, r, world_ambient(w));
    fflush(stdout);
}

static int pick_row(int k, int rows, int n)
{
    return rows >= n ? k : (int)((long)k * (n - 1) / (rows - 1));
}

static void print_raster(const uint8_t *bits, size_t nbins, int n,
                         uint32_t from_ms, uint32_t to_ms, const char *title)
{
    enum { WIDTH = 72, ROWS = 16 };
    int rows = n < ROWS ? n : ROWS, k, c;
    double ms_per_col = (double)(to_ms - from_ms) / WIDTH;

    printf("\n  %s  (%.1f s .. %.1f s, %d of %d fireflies)\n", title,
           from_ms / 1000.0, to_ms / 1000.0, rows, n);
    for (k = 0; k < rows; k++) {
        int i = pick_row(k, rows, n);

        printf("  %3d |", i);
        for (c = 0; c < WIDTH; c++) {
            size_t b0 = (size_t)((from_ms + c * ms_per_col) / LED_BIN_MS);
            size_t b1 = (size_t)((from_ms + (c + 1) * ms_per_col) / LED_BIN_MS);
            int lit = 0;
            size_t b;

            for (b = b0; b <= b1 && b < nbins; b++)
                lit |= bits[b * (size_t)n + (size_t)i];
            putchar(lit ? '#' : ' ');
        }
        puts("|");
    }
}

static void write_svg(const char *path, const uint8_t *bits, size_t nbins,
                      const double *r, size_t nr, const opts_t *o)
{
    const int W = 960, left = 40, right = 16, top = 34, rowh = 6, rplot = 90;
    int n = o->cfg.n, rows = n < 64 ? n : 64, H, k;
    double pw = W - left - right, dur = nbins * (double)LED_BIN_MS;
    FILE *fp = fopen(path, "w");
    size_t b, j;

    if (!fp) {
        perror(path);
        return;
    }
    H = top + rows * rowh + 24 + rplot + 30;
    fprintf(fp, "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"%d\" height=\"%d\" "
                "viewBox=\"0 0 %d %d\" font-family=\"ui-monospace,Menlo,Consolas,monospace\" font-size=\"11\">\n",
            W, H, W, H);
    fprintf(fp, "<rect width=\"100%%\" height=\"100%%\" fill=\"#0b1020\"/>\n");
    fprintf(fp, "<text x=\"%d\" y=\"20\" fill=\"#cfd8e3\" font-size=\"13\">lampyris  |  %d x %s  |  "
                "clock spread +-%.1f%%  |  seed %u</text>\n",
            left, n, o->cfg.species->name, o->cfg.jitter_pct, o->cfg.seed);

    for (k = 0; k < rows; k++) {
        int i = pick_row(k, rows, n);
        double y = top + k * rowh;

        for (b = 0; b < nbins; b++) {
            size_t e;

            if (!bits[b * (size_t)n + (size_t)i])
                continue;
            for (e = b; e + 1 < nbins && bits[(e + 1) * (size_t)n + (size_t)i]; e++)
                ;
            fprintf(fp, "<rect x=\"%.1f\" y=\"%.1f\" width=\"%.2f\" height=\"%d\" fill=\"#d9ff4f\"/>\n",
                    left + pw * b / nbins, y, fmax(pw * (e - b + 1) / nbins, 0.8), rowh - 2);
            b = e;
        }
    }
    {
        double y0 = top + rows * rowh + 24, y1 = y0 + rplot;

        fprintf(fp, "<text x=\"4\" y=\"%.0f\" fill=\"#7f8ea3\">r=1</text>\n", y0 + 4);
        fprintf(fp, "<text x=\"4\" y=\"%.0f\" fill=\"#7f8ea3\">r=0</text>\n", y1);
        fprintf(fp, "<line x1=\"%d\" y1=\"%.1f\" x2=\"%.1f\" y2=\"%.1f\" stroke=\"#26304a\"/>\n",
                left, y1, left + pw, y1);
        fprintf(fp, "<polyline fill=\"none\" stroke=\"#4fd1ff\" stroke-width=\"1.5\" points=\"");
        for (j = 0; j < nr; j++)
            fprintf(fp, "%.1f,%.1f ", left + pw * (j * (double)R_BIN_MS) / dur, y1 - r[j] * rplot);
        fprintf(fp, "\"/>\n");
        fprintf(fp, "<text x=\"%d\" y=\"%d\" fill=\"#7f8ea3\">0 s</text>\n", left, H - 8);
        fprintf(fp, "<text x=\"%.0f\" y=\"%d\" fill=\"#7f8ea3\" text-anchor=\"end\">%.0f s</text>\n",
                left + pw, H - 8, dur / 1000.0);
        fprintf(fp, "<text x=\"%.0f\" y=\"%d\" fill=\"#7f8ea3\" text-anchor=\"middle\">"
                    "flashes per firefly (top)  /  Kuramoto order parameter r (bottom)</text>\n",
                left + pw / 2, H - 8);
    }
    fputs("</svg>\n", fp);
    fclose(fp);
}

int main(int argc, char **argv)
{
    static world_t w;
    opts_t o;
    uint32_t total_ms, t;
    size_t nbins, nr, j;
    uint8_t *bits;
    double *r;
    long lock_at = -1;
    uint32_t heard = 0, absorbed = 0;
    int i;

    if (parse(argc, argv, &o))
        return 2;
    if (world_init(&w, &o.cfg)) {
        fputs("out of memory\n", stderr);
        return 1;
    }

    total_ms = (uint32_t)(o.seconds * 1000.0);
    nbins = total_ms / LED_BIN_MS + 1;
    nr = (total_ms - 1) / R_BIN_MS + 1;
    bits = calloc(nbins * (size_t)o.cfg.n, 1);
    r = calloc(nr, sizeof *r);
    if (!bits || !r) {
        fputs("out of memory\n", stderr);
        return 1;
    }

    if (o.live)
        fputs("\x1b[2J", stdout);
    for (t = 0; t < total_ms; t++) {
        world_step(&w);
        for (i = 0; i < o.cfg.n; i++)
            if (w.led[i])
                bits[(t / LED_BIN_MS) * (size_t)o.cfg.n + (size_t)i] = 1;
        if (t % R_BIN_MS == 0)
            r[t / R_BIN_MS] = world_order(&w);
        if (o.live && t % 33 == 0) {
            draw_live(&w, world_order(&w));
            sleep_ms(33);
        }
    }

    /* Locked = r stays above 0.9 from some point on, having reached 0.95. */
    for (j = nr; j-- > 0;) {
        if (r[j] < 0.90)
            break;
        if (r[j] >= 0.95)
            lock_at = (long)(j * R_BIN_MS);
    }
    for (i = 0; i < o.cfg.n; i++) {
        heard += w.ff[i].heard;
        absorbed += w.ff[i].absorbed;
    }

    if (!o.quiet) {
        uint32_t period = o.cfg.species->period_ms;
        uint32_t win = period * 6 < 8000 ? 8000 : period * 6;
        const char *ramp = " .:-=+*#%@";
        int cols = 60, c;

        if (win > total_ms / 2)
            win = total_ms / 2;
        printf("lampyris swarm: %d x %s, %s layout, clock spread +-%.1f%%, seed %u\n",
               o.cfg.n, o.cfg.species->name,
               o.cfg.layout == LAYOUT_GRID ? "grid" : o.cfg.layout == LAYOUT_RING ? "ring" : "scatter",
               o.cfg.jitter_pct, o.cfg.seed);
        print_raster(bits, nbins, o.cfg.n, 0, win, "dusk: everyone for themselves");
        print_raster(bits, nbins, o.cfg.n, total_ms - win, total_ms, "later: one organism");

        printf("\n  sync r(t) |");
        for (c = 0; c < cols; c++) {
            size_t a = (size_t)c * nr / cols, e = (size_t)(c + 1) * nr / cols;
            double s = 0.0;
            size_t k;

            if (e <= a)
                e = a + 1;
            for (k = a; k < e && k < nr; k++)
                s += r[k];
            s /= (double)(e - a);
            putchar(ramp[(int)(s * 9.0 + 0.5) > 9 ? 9 : (int)(s * 9.0 + 0.5)]);
        }
        printf("|  0 .. %.0f s\n\n", o.seconds);
    }

    if (lock_at >= 0)
        printf("locked after %.1f s (%.1f cycles); final r = %.3f; %u flashes seen, %u absorptions\n",
               lock_at / 1000.0, lock_at / (double)o.cfg.species->period_ms, r[nr - 1],
               heard, absorbed);
    else
        printf("not locked after %.1f s; final r = %.3f; %u flashes seen, %u absorptions\n",
               o.seconds, r[nr - 1], heard, absorbed);

    {
        uint32_t lo = 0xFFFFFFFFu, hi = 0;

        for (i = 0; i < o.cfg.n; i++) {
            uint32_t p = w.ff[i].osc.period_q4;

            if (p < lo) lo = p;
            if (p > hi) hi = p;
        }
        printf("free-running periods now span %.1f .. %.1f ms\n", lo / 16.0, hi / 16.0);
    }

    if (o.svg)
        write_svg(o.svg, bits, nbins, r, nr, &o);
    if (o.csv) {
        FILE *fp = fopen(o.csv, "w");

        if (fp) {
            fputs("t_ms,r\n", fp);
            for (j = 0; j < nr; j++)
                fprintf(fp, "%lu,%.4f\n", (unsigned long)(j * R_BIN_MS), r[j]);
            fclose(fp);
        } else {
            perror(o.csv);
        }
    }

    free(bits);
    free(r);
    world_free(&w);
    return lock_at >= 0 ? 0 : 3;
}
