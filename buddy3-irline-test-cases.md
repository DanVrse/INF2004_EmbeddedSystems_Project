# Buddy 3 – Bench Test Cases: IR Line Following and Barcode Decoding

Test build: `buddy3-irline-test.uf2` (`buddy3-irline-test.c`, `src/ir-line-sensor.c`, `src/barcode.c`).
All observations are read from the USB serial monitor.

## Setup

| Item | Detail |
|---|---|
| Line sensors | Left = GP12 (servo port 1, S pin), Right = GP13 (servo port 2, S pin), DO only, VCC from 3.3 V (not the servo port's + pin) |
| Barcode sensor | GP27 (Grove 6), AO only, VCC from 3.3 V, Grove 6's GP26 wire unconnected |
| Flags in `buddy3-irline-test.c` | IR tests: `SHOW_IR_STATUS 1`. Barcode tests: `SHOW_BC_LEVELS 1`, `SHOW_WIDTHS 1` |
| Track | Black tape narrower than the gap between the left and right line sensors |
| Barcodes | Code 39 `*L*`, `*R*`, `*X*`, `*LEFT*`, matte laser print, thin bars ≥ 3 mm (see BC-S2) |
| Tools | Ruler, card spacers for sensor height |
| Power-up | Keep the barcode sensor over white floor (it learns the floor level at start-up) |

## 1. IR Line Following Tests (GP12 left, GP13 right)

| ID | Test | Steps | Expected observation |
|---|---|---|---|
| IR-01 | Polarity | Hold each sensor over white, then over black | White: `floor (raw 0)`. Black: `ON LINE (raw 1)`. Module LED switches. If reversed, set `IR_LINE_IS_HIGH 0` |
| IR-02 | Height range | Repeat IR-01 at 3, 5, 8, 10, 15 mm | Reliable switching inside a height range; flicker or no switching outside it |
| IR-03 | Centred | Place the tape between the two sensors | `L: floor \| R: floor`, `line between sensors (centred)`, steer 0 |
| IR-04 | Drift right | Slide the car right until the tape is under the left sensor | `[LEFT] -> ON LINE`, then `drifted RIGHT -> steer left (steer 50)` |
| IR-05 | Drift left | Slide the car left until the tape is under the right sensor | `[RIGHT] -> ON LINE`, then `drifted LEFT -> steer right (steer -50)` |
| IR-06 | Crossing | Do IR-04, then lay a strip across under both sensors | `crossing / junction -> hold`, steer stays at the last value (50) |
| IR-07 | Line ends | With the car centred, remove the tape | Still `centred`: shows the straddle layout cannot tell "centred" from "line ended" (FR-NAV-03 limitation) |
| IR-08 | Car lifted | Lift the car off the surface | Both sensors read `ON LINE` (no reflection) and it looks like a crossing |
| IR-09 | Lighting | Room lights on/off, phone torch nearby | Readings do not change (NFR-PRF-03); count any changes |
| IR-10 | Edge chatter | Move each sensor slowly across the tape edge 10 times | Ideally one message per crossing; extra ON/floor toggles are chatter |

## 2. Barcode Sensor Characterisation (do these first)

| ID | Test | Steps | Expected observation |
|---|---|---|---|
| BC-S1 | Height vs contrast | Fix the sensor at 2, 4, 6, 8, 10, 13 mm; read `floor` over white and `now` over a black card | Contrast rises, peaks, then falls. Use the peak height |
| BC-S2 | Smallest readable bar | Slide stripes 1–5 mm wide (equal white gaps) under the sensor at the chosen height | Wide stripes reach the full black level; narrow ones only rise partway. Minimum thin bar = narrowest stripe reaching ~70% of full contrast |

### BC-S1 record

| Height (mm) | Floor AO | Black AO | Contrast (counts) | Contrast (V) = counts × 3.3 / 4095 |
|---|---|---|---|---|
| 2 | | | | |
| 4 | | | | |
| 6 | | | | |
| 8 | | | | |
| 10 | | | | |
| 13 | | | | |

Contrast below ~500 counts is marginal; the code ignores anything below 250 counts (`BARCODE_MIN_CONTRAST`).

## 3. Barcode Decoding Tests (GP27 AO)

Use a paper-pull jig (sensor fixed at the BC-S1 height, barcode pulled underneath along a ruler) or push the car along a ruler with the sensor mounted.

| ID | Test | Steps | Expected observation |
|---|---|---|---|
| BC-01 | Levels and polarity | Sensor over white, then a black card under it | `Barcode AO: now ≈ floor`; over black, `now` rises well above `floor` and `bar` follows. If `now` drops, set `BARCODE_BAR_IS_HIGH 0` |
| BC-02 | Height | Repeat BC-01 at different heights | The `bar − floor` gap changes; confirms the BC-S1 choice |
| BC-03 | Forward read | Pass `*L*` at a steady speed | `29 bars/gaps`, `Pattern:` line, `DECODED: "L" -> TURN LEFT` |
| BC-04 | Backwards read | Pass `*R*` the opposite way | `DECODED: "R" (read backwards) -> TURN RIGHT` |
| BC-05 | Speed | Pass `*L*` slow, medium, fast | Medium decodes. Too slow (any gap > 400 ms) splits into failed passes. Too fast (thin bar < 2 ms) shows glitches removed and a wrong bar/gap count |
| BC-06 | Unknown code | Pass `*X*` | `DECODED: "X" -> no command for this code` (production treats this as an error) |
| BC-07 | Partial pass | Stop halfway through `*L*` | `NOT DECODED: wrong number of bars/gaps` |
| BC-08 | Not a barcode | Pass one black stripe or the guide line | `NOT DECODED` (production ignores it silently) |
| BC-09 | Idle | Leave over white floor for 5 minutes | No passes printed; any pass is a false trigger |
| BC-10 | Repeatability | 20 passes each of `*L*` and `*R*` | ≥ 95% decoded correctly (NFR-PRF-04); note the reason for each failure |
| BC-11 | Longer code | Pass `*LEFT*` | `39 bars/gaps`, `DECODED: "LEFT"` |

## 4. Measurements to Record and Analyse

### IR line sensors

| Measurement | How | What it tells you |
|---|---|---|
| Working height range (mm) | IR-02 | Mounting height and potentiometer setting to use |
| Switch-over offset (mm) | Ruler: distance past the tape edge where each sensor switches | Uneven left/right offsets make the car pull to one side |
| Chatter count per crossing | IR-10 | Evidence for the 2 ms glitch filter in the production build |
| Time between sensors (ms) | `[xxxxx ms]` timestamps while sweeping across the line | How fast the line passes between sensors |

### Barcode sensor

| Measurement | How | What it tells you |
|---|---|---|
| Contrast (counts / V) | `bar − floor` (BC-S1, BC-01) | Signal strength; aim for ≥ 500 counts |
| Noise | Spread of `now` over plain floor | How close noise is to the trigger threshold |
| Thick/thin ratio | Mean thick width ÷ mean thin width per pass | Code 39 is normally 2–3; the decoder needs ≥ 1.5 |
| Speed (mm/s) | Printed thin bar width (mm) ÷ thin element time (µs) × 10⁶ | Fastest reliable speed; aim for thin bar ≥ 5 ms |
| Sampling rate | Printed widths should be near multiples of 500 µs | Confirms the 2 kHz ADC sampling (pin table: ≥ 1 kHz) |
| Success rate (%) | BC-10 | NFR-PRF-04 (≥ 95%) |
| Glitches removed | Per-pass header | Should be 0; higher means a noisy signal or the sensor is too high |

Note: the test build decodes only after 400 ms with no edges, so it does not show real decode latency. Measure NFR-RTM-05 (decode within 500 ± 100 ms) on the production build.

## 5. Results Log

| Test ID | Date | Conditions (height, speed, lighting) | Observed output | Measured values | Pass/Fail |
|---|---|---|---|---|---|
| | | | | | |
