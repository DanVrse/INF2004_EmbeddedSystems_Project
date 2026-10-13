/*
 * buddy3-irline-test.c
 * Bench test for Buddy 3:
 *  - IR line: prints when the left/right MH-sensor moves between line and floor
 *  - Barcode: after each pass over a barcode, prints every bar and gap
 *    (thick or thin, width in us) and the decoded text and command
 */

#include <stdio.h>
#include <stdbool.h>
#include "pico/stdlib.h"
#include "ir-line-sensor.h"
#include "barcode.h"

#define SAMPLE_MS       10
#define STATUS_MS       1000
#define SHOW_IR_STATUS  0   // 0 = hide the 1 s line status while testing barcodes
#define SHOW_WIDTHS     1   // 0 = hide the per-element width list

static const char *where(bool on_line)
{
    return on_line ? "ON LINE" : "floor  ";
}

// What the two readings mean when the sensors straddle the line.
static const char *position(bool left, bool right)
{
    if (left && right)  return "both on line (crossing / junction)";
    if (left)           return "drifted RIGHT -> steer left";
    if (right)          return "drifted LEFT  -> steer right";
    return "line between sensors (centred) or no line";
}

static void print_barcode(const barcode_result_t *r)
{
    // Pattern: B = thick bar, b = thin bar, W = thick gap, w = thin gap
    printf("\n=== Barcode pass: %d bars/gaps, %d glitch(es) removed ===\n",
           r->num_elements, r->glitches);
    printf("Pattern: ");
    for (int i = 0; i < r->num_elements; i++) {
        bool bar = (i % 2) == 0;            // passes always start on a bar
        char c = bar ? (r->is_thick[i] ? 'B' : 'b')
                     : (r->is_thick[i] ? 'W' : 'w');
        printf("%c", c);
        if ((i % 10) == 8) printf(" ");     // space after each character
    }
    printf("\n");

    if (SHOW_WIDTHS) {
        for (int i = 0; i < r->num_elements; i++) {
            bool bar = (i % 2) == 0;
            printf("  %2d %s %-5s %7lu us\n", i,
                   bar ? "BAR (black)" : "GAP (white)",
                   r->is_thick[i] ? "THICK" : "thin",
                   (unsigned long)r->width_us[i]);
        }
    }

    if (r->valid) {
        printf("DECODED: \"%s\"%s -> %s\n\n", r->text,
               r->reversed ? " (read backwards)" : "",
               barcode_cmd_name(r->command));
    } else {
        printf("NOT DECODED: %s\n\n", r->error);
    }
}

int main(void)
{
    stdio_init_all();
    sleep_ms(2000);                 // time to open the serial monitor

    ir_line_sensor_init();
    barcode_init();

    printf("\nBuddy 3 test. Line: Left=GP%d Right=GP%d | Barcode=GP%d\n",
           IR_LEFT_PIN, IR_RIGHT_PIN, BARCODE_PIN);
    printf("Move the line sensors over the line, or pass a Code 39 "
           "barcode (e.g. *L*) under the barcode sensor.\n\n");

    bool prev_left  = ir_line_is_left_on_line();
    bool prev_right = ir_line_is_right_on_line();
    uint32_t next_status = 0;
    barcode_result_t result;

    while (true)
    {
        bool left  = ir_line_is_left_on_line();
        bool right = ir_line_is_right_on_line();
        uint32_t now = to_ms_since_boot(get_absolute_time());

        if (left != prev_left)
        {
            printf("[%7lu ms] LEFT  -> %s\n", (unsigned long)now, where(left));
            prev_left = left;
        }
        if (right != prev_right)
        {
            printf("[%7lu ms] RIGHT -> %s\n", (unsigned long)now, where(right));
            prev_right = right;
        }

        if (barcode_poll(&result))
        {
            print_barcode(&result);
        }

        if (SHOW_IR_STATUS && now >= next_status)
        {
            next_status = now + STATUS_MS;
            printf("L: %s (raw %d) | R: %s (raw %d) | %s\n",
                   where(left), ir_line_left_raw(),
                   where(right), ir_line_right_raw(),
                   position(left, right));
        }

        sleep_ms(SAMPLE_MS);
    }
}