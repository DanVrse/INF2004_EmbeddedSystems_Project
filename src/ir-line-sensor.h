/*
 * ir-line-sensor.h
 * IR line following with two MH-sensors, digital outputs (DO) only (Buddy 3).
 *
 * Layout (Design Review Section 2): the two line sensors STRADDLE the line.
 *   - LEFT IR line sensor  (left MH-sensor,   GP12) on the LEFT  side of the line
 *   - RIGHT IR line sensor (centre MH-sensor, GP13) on the RIGHT side of the line
 *   - The third (right-most) MH-sensor is the barcode sensor (see barcode.h)
 * Power the sensors from 3.3 V, NOT from the servo port's + pin
 * (that pin carries battery voltage and would damage GP12/GP13).
 */

#ifndef IR_LINE_SENSOR_H
#define IR_LINE_SENSOR_H

#include <stdbool.h>

// Pins from the Design Review pin table (Section 2).
#define IR_LEFT_PIN      12   // LEFT_IR_LINE:  MH-sensor #1 DO (servo port 1, S pin)
#define IR_RIGHT_PIN     13   // RIGHT_IR_LINE: MH-sensor #2 DO (servo port 2, S pin)

// 1 = DO reads HIGH over the black line (usual for MH-sensors).
// Set to 0 if the serial output shows the opposite.
#define IR_LINE_IS_HIGH  1

// Where the car is relative to the line, from the two readings (L | R).
typedef enum
{
    IR_POS_CENTERED = 0,     // 0 | 0  line between the sensors (or no line)
    IR_POS_DRIFT_RIGHT,      // 1 | 0  line under the left sensor
    IR_POS_DRIFT_LEFT,       // 0 | 1  line under the right sensor
    IR_POS_CROSSING          // 1 | 1  both on black: crossing / junction
} ir_line_position_t;

void ir_line_sensor_init(void);

bool ir_line_is_left_on_line(void);
bool ir_line_is_right_on_line(void);
int  ir_line_left_raw(void);      // raw DO level 0/1, for checking polarity
int  ir_line_right_raw(void);

ir_line_position_t ir_line_get_position(void);
const char        *ir_line_position_name(ir_line_position_t pos);

// Suggested steering for the motor task, -100..+100 (positive = turn LEFT).
// At a crossing it keeps the last steering value.
int  ir_line_steering(ir_line_position_t pos);

#endif // IR_LINE_SENSOR_H
