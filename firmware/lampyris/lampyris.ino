/*
 * lampyris - one firefly.
 *
 * Flash a few of these near each other in a dark room and within a
 * handful of cycles they blink as one. No radio, no shared clock, no leader.
 *
 * Wiring (default: the LED is both lantern and eye)
 *
 *     PIN_ANODE  ---[ 220R ]---|>|---  PIN_CATHODE
 *                             LED
 *
 *   Shine:  anode HIGH, cathode LOW.
 *   See:    reverse-bias the LED to charge its junction capacitance, float
 *           the cathode, and time how long photocurrent takes to drain it.
 *           More light = faster discharge. (Dietz, Yerazunis & Leigh,
 *           "Very Low-Cost Sensing and Communication Using Bidirectional
 *           LEDs", MERL TR2003-35.)
 *
 * Alternative: define LAMPYRIS_SENSE_ANALOG and put a phototransistor or LDR
 * divider on PIN_SENSE. Easier to tune, one more part.
 *
 * CI builds this for Uno, Nano, Leonardo and Raspberry Pi Pico. Only portable
 * Arduino calls are used; ATtiny85 pins are pre-defined below.
 */

#include "src/firefly.h"

/* ------------------------------------------------------------ config -- */

#define SPECIES          LP_SPECIES_DEMO   /* or LP_SPECIES_PTEROPTYX / LP_SPECIES_CAROLINUS */
#define SAMPLE_MS        4                 /* control loop period            */

#if defined(__AVR_ATtiny85__)
#define PIN_ANODE        0
#define PIN_CATHODE      1
#define PIN_SENSE        A1
#define PIN_SEED         A3
#elif defined(ARDUINO_ARCH_RP2040)
#define PIN_ANODE        2
#define PIN_CATHODE      3
#define PIN_SENSE        A0
#define PIN_SEED         A2                /* leave GP28 unconnected        */
#else
#define PIN_ANODE        2
#define PIN_CATHODE      3
#define PIN_SENSE        A0
#define PIN_SEED         A5
#endif

/* LED-as-sensor tuning: give up on a discharge after this long, and the
 * constant that turns discharge time into a brightness reading. */
#define SENSE_TIMEOUT_US 3000UL
#define SENSE_SCALE      2000000UL

/* #define LAMPYRIS_SENSE_ANALOG */
/* #define LAMPYRIS_DEBUG */

/* ---------------------------------------------------------------------- */

static lp_firefly_t me;

static void ledOn(void)
{
    pinMode(PIN_ANODE, OUTPUT);
    pinMode(PIN_CATHODE, OUTPUT);
    digitalWrite(PIN_CATHODE, LOW);
    digitalWrite(PIN_ANODE, HIGH);
}

static void ledOff(void)
{
    pinMode(PIN_ANODE, OUTPUT);
    pinMode(PIN_CATHODE, OUTPUT);
    digitalWrite(PIN_ANODE, LOW);
    digitalWrite(PIN_CATHODE, LOW);
}

static uint16_t senseLight(void)
{
#ifdef LAMPYRIS_SENSE_ANALOG
    return (uint16_t)analogRead(PIN_SENSE) << 6;
#else
    uint32_t t0, dt;

    /* Reverse bias: charge the junction. */
    pinMode(PIN_ANODE, OUTPUT);
    pinMode(PIN_CATHODE, OUTPUT);
    digitalWrite(PIN_ANODE, LOW);
    digitalWrite(PIN_CATHODE, HIGH);
    delayMicroseconds(20);

    /* Float the cathode (no pull-up) and time the photocurrent discharge. */
    pinMode(PIN_CATHODE, INPUT);
    t0 = micros();
    do {
        dt = micros() - t0;
    } while (digitalRead(PIN_CATHODE) == HIGH && dt < SENSE_TIMEOUT_US);

    ledOff();
    dt = SENSE_SCALE / (dt + 1);
    return (uint16_t)(dt > 65535UL ? 65535UL : dt);
#endif
}

static uint32_t gatherSeed(void)
{
    uint32_t s = 0;
    uint8_t i;

    /* Floating-pin noise plus sensor timing jitter. Every node must start at
     * a different phase or there is nothing to watch. */
    for (i = 0; i < 32; i++) {
        s = (s << 3) ^ (s >> 29) ^ (uint32_t)analogRead(PIN_SEED) ^ micros();
        s ^= senseLight();
        delayMicroseconds(97 + (s & 63));
    }
    return s;
}

void setup(void)
{
    uint8_t i;

#ifdef LAMPYRIS_DEBUG
    Serial.begin(115200);
#endif
    ledOff();
    for (i = 0; i < 3; i++) {   /* hello */
        ledOn();
        delay(40);
        ledOff();
        delay(80);
    }
    lp_ff_init(&me, &SPECIES, gatherSeed(), SAMPLE_MS);
}

void loop(void)
{
    static uint32_t next;
    static uint32_t lastFires;
    uint32_t now = millis();
    uint16_t light = 0;
    bool led;

    if ((int32_t)(now - next) < 0)
        return;
    next = now + SAMPLE_MS;

    if (lp_ff_wants_sample(&me, now))
        light = senseLight();
    led = lp_ff_step(&me, now, light);
    if (led)
        ledOn();
    else
        ledOff();

#ifdef LAMPYRIS_DEBUG
    if (me.fires != lastFires) {
        lastFires = me.fires;
        Serial.print(F("fire t="));
        Serial.print(now);
        Serial.print(F(" period="));
        Serial.print(lp_osc_period_ms(&me.osc));
        Serial.print(F(" ambient="));
        Serial.print(lp_det_baseline(&me.det));
        Serial.print(F(" heard="));
        Serial.print(me.heard);
        Serial.print(F(" absorbed="));
        Serial.println(me.absorbed);
    }
#else
    (void)lastFires;
#endif
}
