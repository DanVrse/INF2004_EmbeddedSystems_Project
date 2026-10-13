/*
 * barcode.h
 * Barcode reader on MH-sensor #3 beside the track (Buddy 3).
 * Reads Code 39 barcodes: thick/thin black bars and thick/thin white gaps,
 * framed by '*' start and stop characters (e.g. "*L*").
 */

#ifndef BARCODE_H
#define BARCODE_H

#include <stdbool.h>
#include <stdint.h>

// Pin from the Design Review pin table (Section 2).
#define BARCODE_PIN            7      // MH-sensor #3 DO (Grove 7)

// 1 = DO reads HIGH over a black bar (same as the line sensors).
#define BARCODE_BAR_IS_HIGH    1

#define BARCODE_GLITCH_US      2000   // pulses shorter than this are noise
#define BARCODE_END_GAP_MS     400    // no edge for this long = pass finished
#define BARCODE_MAX_ELEMENTS   129    // up to 11 characters incl. * and *
#define BARCODE_MAX_TEXT       12

typedef enum
{
    BARCODE_CMD_NONE = 0,
    BARCODE_CMD_LEFT,
    BARCODE_CMD_RIGHT
} barcode_cmd_t;

// Everything measured and decoded in one pass over a barcode.
typedef struct
{
    int           num_elements;                      // bars + gaps
    uint32_t      width_us[BARCODE_MAX_ELEMENTS];    // in the order scanned
    bool          is_thick[BARCODE_MAX_ELEMENTS];    // thick or thin
    bool          valid;                             // decoded OK
    bool          reversed;                          // scanned backwards
    char          text[BARCODE_MAX_TEXT + 1];        // data between the *s
    barcode_cmd_t command;
    int           glitches;                          // pulses < 2 ms removed
    const char *  error;                             // why it failed
} barcode_result_t;

void        barcode_init(void);

// Call often from the main loop. Returns true when a pass has finished
// and fills *result.
bool        barcode_poll(barcode_result_t *result);

const char *barcode_cmd_name(barcode_cmd_t cmd);

#endif // BARCODE_H
