/*
 * barcode.c
 * Reads Code 39 barcodes with an MH-sensor.
 *
 * A both-edge GPIO interrupt timestamps every DO change (black <-> white).
 * The main loop turns the timestamps into widths: bar, gap, bar, ... and,
 * once no edge has arrived for BARCODE_END_GAP_MS, decodes the pass.
 *
 * Code 39: every character is 9 elements (5 bars, 4 gaps), 3 of them thick,
 * with a thin white gap between characters. The 3 widest of each 9 are
 * marked thick, so the result does not depend on how fast the car moves.
 */

#include "barcode.h"

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"

#define QUEUE_SIZE        64
#define CHAR_ELEMENTS     9
#define THICK_PER_CHAR    3
#define MIN_RATIO_X10     15      // thick must be >= 1.5 x thin

// ---------------------------------------------------------------------------
// Code 39 table: 9 elements, first element = left-most bit, 1 = thick
// ---------------------------------------------------------------------------
typedef struct
{
    char     symbol;
    uint16_t pattern;
} code39_t;

static const code39_t code39[] =
{
    {'0', 0x034}, {'1', 0x121}, {'2', 0x061}, {'3', 0x160}, {'4', 0x031},
    {'5', 0x130}, {'6', 0x070}, {'7', 0x025}, {'8', 0x124}, {'9', 0x064},
    {'A', 0x109}, {'B', 0x049}, {'C', 0x148}, {'D', 0x019}, {'E', 0x118},
    {'F', 0x058}, {'G', 0x00D}, {'H', 0x10C}, {'I', 0x04C}, {'J', 0x01C},
    {'K', 0x103}, {'L', 0x043}, {'M', 0x142}, {'N', 0x013}, {'O', 0x112},
    {'P', 0x052}, {'Q', 0x007}, {'R', 0x106}, {'S', 0x046}, {'T', 0x016},
    {'U', 0x181}, {'V', 0x0C1}, {'W', 0x1C0}, {'X', 0x091}, {'Y', 0x190},
    {'Z', 0x0D0}, {'-', 0x085}, {'.', 0x184}, {' ', 0x0C4}, {'$', 0x0A8},
    {'/', 0x0A2}, {'+', 0x08A}, {'%', 0x02A}, {'*', 0x094}
};

#define CODE39_COUNT  (sizeof(code39) / sizeof(code39[0]))

// ---------------------------------------------------------------------------
// Interrupt -> main loop queue
// ---------------------------------------------------------------------------
static volatile uint32_t edge_time_us[QUEUE_SIZE];
static volatile uint8_t  edge_level[QUEUE_SIZE];
static volatile uint32_t head = 0;
static volatile uint32_t tail = 0;

// Pass in progress (main loop only)
static uint32_t widths[BARCODE_MAX_ELEMENTS];
static int      num_widths = 0;
static bool     in_pass = false;
static bool     last_black = false;
static uint32_t last_edge_us = 0;
static int      glitches = 0;
static bool     too_long = false;

// Runs in interrupt context: store the time and the new level only.
static void barcode_edge_isr(void)
{
    uint32_t events = gpio_get_irq_event_mask(BARCODE_PIN)
                      & (GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL);
    if (events == 0) {
        return;
    }
    gpio_acknowledge_irq(BARCODE_PIN, events);

    if (events == (GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL)) {
        return;                         // both edges at once: tiny glitch
    }

    uint32_t next = (head + 1) % QUEUE_SIZE;
    if (next == tail) {
        return;                         // queue full: drop the edge
    }
    edge_time_us[head] = time_us_32();
    edge_level[head]   = (events & GPIO_IRQ_EDGE_RISE) ? 1 : 0;
    head = next;
}

static bool level_is_black(int level)
{
    return BARCODE_BAR_IS_HIGH ? (level != 0) : (level == 0);
}

// ---------------------------------------------------------------------------
// Decoding
// ---------------------------------------------------------------------------

// Marks the 3 widest of 9 elements thick and returns the Code 39 character,
// or 0 if the pattern is not valid.
static char decode_char(const uint32_t *w, bool *thick)
{
    uint16_t pattern = 0;
    uint32_t thinnest_thick = UINT32_MAX;
    uint32_t widest_thin = 0;

    for (int i = 0; i < CHAR_ELEMENTS; i++) {
        thick[i] = false;
    }

    for (int k = 0; k < THICK_PER_CHAR; k++) {
        int best = -1;
        for (int i = 0; i < CHAR_ELEMENTS; i++) {
            if (!thick[i] && (best < 0 || w[i] > w[best])) {
                best = i;
            }
        }
        thick[best] = true;
    }

    for (int i = 0; i < CHAR_ELEMENTS; i++) {
        if (thick[i]) {
            pattern |= (uint16_t)(1u << (CHAR_ELEMENTS - 1 - i));
            if (w[i] < thinnest_thick) thinnest_thick = w[i];
        } else if (w[i] > widest_thin) {
            widest_thin = w[i];
        }
    }

    // Thick and thin must be clearly different, or the read is unreliable.
    if ((uint64_t)thinnest_thick * 10 < (uint64_t)widest_thin * MIN_RATIO_X10) {
        return 0;
    }

    for (unsigned i = 0; i < CODE39_COUNT; i++) {
        if (code39[i].pattern == pattern) {
            return code39[i].symbol;
        }
    }
    return 0;
}

// Decodes elements in the given order. Fills text and thick[].
static bool decode_symbol(const uint32_t *w, int n, char *text, bool *thick,
                          const char **error)
{
    if (n < 29 || ((n + 1) % 10) != 0) {
        *error = "wrong number of bars/gaps (Code 39 needs 29, 39, 49 ...)";
        return false;
    }

    int  chars = (n + 1) / 10;
    char decoded[BARCODE_MAX_TEXT + 2];

    for (int c = 0; c < chars; c++) {
        decoded[c] = decode_char(&w[c * 10], &thick[c * 10]);
        if (decoded[c] == 0) {
            *error = "a character did not match the Code 39 table";
            return false;
        }
        if (c < chars - 1) {
            thick[c * 10 + 9] = false;      // gap between characters
        }
    }

    if (decoded[0] != '*' || decoded[chars - 1] != '*') {
        *error = "missing * start or stop character";
        return false;
    }

    int len = chars - 2;
    memcpy(text, &decoded[1], (size_t)len);
    text[len] = '\0';
    return true;
}

static barcode_cmd_t command_for(const char *text)
{
    // First data character decides the command. Change to suit the course.
    switch (text[0]) {
        case 'L': case 'A': return BARCODE_CMD_LEFT;
        case 'R': case 'B': return BARCODE_CMD_RIGHT;
        default:            return BARCODE_CMD_NONE;
    }
}

// Rough thick/thin split, only used for display when decoding fails.
static void mark_thick_by_threshold(barcode_result_t *r)
{
    uint32_t lo = UINT32_MAX, hi = 0;
    for (int i = 0; i < r->num_elements; i++) {
        if (r->width_us[i] < lo) lo = r->width_us[i];
        if (r->width_us[i] > hi) hi = r->width_us[i];
    }
    uint32_t mid = (lo + hi) / 2;
    for (int i = 0; i < r->num_elements; i++) {
        r->is_thick[i] = r->width_us[i] > mid;
    }
}

static void finish_pass(barcode_result_t *r)
{
    memset(r, 0, sizeof(*r));
    r->num_elements = num_widths;
    r->glitches     = glitches;
    memcpy(r->width_us, widths, sizeof(uint32_t) * (size_t)num_widths);

    if (too_long) {
        r->error = "too many bars/gaps for the buffer";
    } else if (decode_symbol(widths, num_widths, r->text, r->is_thick,
                             &r->error)) {
        r->valid = true;
    } else {
        // Try backwards (car moving the other way).
        uint32_t rev[BARCODE_MAX_ELEMENTS];
        bool     rev_thick[BARCODE_MAX_ELEMENTS];
        const char *rev_error = NULL;

        for (int i = 0; i < num_widths; i++) {
            rev[i] = widths[num_widths - 1 - i];
        }
        if (decode_symbol(rev, num_widths, r->text, rev_thick, &rev_error)) {
            r->valid    = true;
            r->reversed = true;
            r->error    = NULL;
            for (int i = 0; i < num_widths; i++) {
                r->is_thick[i] = rev_thick[num_widths - 1 - i];
            }
        }
    }

    if (r->valid) {
        r->command = command_for(r->text);
    } else {
        mark_thick_by_threshold(r);
    }

    in_pass    = false;
    num_widths = 0;
}

// ---------------------------------------------------------------------------
// Public functions
// ---------------------------------------------------------------------------

void barcode_init(void)
{
    gpio_init(BARCODE_PIN);
    gpio_set_dir(BARCODE_PIN, GPIO_IN);
    gpio_disable_pulls(BARCODE_PIN);    // the MH-sensor board drives DO

    // Raw handler: does not take over the shared GPIO callback, so it can
    // live alongside the encoder / ultrasonic interrupts later.
    gpio_add_raw_irq_handler(BARCODE_PIN, barcode_edge_isr);
    gpio_set_irq_enabled(BARCODE_PIN,
                         GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);
    irq_set_enabled(IO_IRQ_BANK0, true);
}

bool barcode_poll(barcode_result_t *result)
{
    while (tail != head) {
        uint32_t t     = edge_time_us[tail];
        bool     black = level_is_black(edge_level[tail]);
        tail = (tail + 1) % QUEUE_SIZE;

        if (!in_pass) {
            if (black) {                    // first bar starts a pass
                in_pass      = true;
                num_widths   = 0;
                glitches     = 0;
                too_long     = false;
                last_black   = true;
                last_edge_us = t;
            }
            continue;
        }

        if (black == last_black) {
            continue;                       // repeated level, ignore
        }

        uint32_t dt = t - last_edge_us;

        if (dt < BARCODE_GLITCH_US) {
            // Noise: undo the previous edge and skip this one.
            glitches++;
            if (num_widths > 0) {
                num_widths--;
                last_edge_us -= widths[num_widths];
                last_black    = black;
            } else {
                in_pass = false;
            }
            continue;
        }

        if (num_widths < BARCODE_MAX_ELEMENTS) {
            widths[num_widths++] = dt;
        } else {
            too_long = true;
        }
        last_edge_us = t;
        last_black   = black;
    }

    if (in_pass && (time_us_32() - last_edge_us) > BARCODE_END_GAP_MS * 1000u) {
        finish_pass(result);
        return true;
    }
    return false;
}

const char *barcode_cmd_name(barcode_cmd_t cmd)
{
    switch (cmd) {
        case BARCODE_CMD_LEFT:  return "TURN LEFT";
        case BARCODE_CMD_RIGHT: return "TURN RIGHT";
        default:                return "no command for this code";
    }
}
