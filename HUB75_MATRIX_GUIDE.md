# Understanding HUB75 RGB LED Matrices (Adafruit MatrixPortal S3 + 64x32 panel)

This guide explains how a HUB75 RGB matrix actually works under the hood, and walks through the test sketch (`hub75_64x32_test.ino`) line by line so you can see exactly how each physical pin on the connector maps to a line in the code.

---

## 1. What's actually inside the panel

A 64x32 HUB75 panel is not one big grid of individually addressable pixels like a computer monitor. It's built from LED driver chips (shift registers) that only let you light up **two rows at a time**, and you cycle through all the rows fast enough that your eyes see a full, steady image (persistence of vision, same trick as old CRT TVs and movie film).

Key facts:

- The panel has 32 rows, but the electronics split it into a **top half** (rows 0-15) and a **bottom half** (rows 16-31), driven simultaneously.
- For any given row pair, you shift in color data for all 64 columns, one pixel at a time, then "latch" it so it displays, then move to the next row pair.
- This happens so fast (thousands of times per second) that the whole panel looks continuously lit.

This is why the connector has two sets of RGB pins (`R1 G1 B1` for the top half, `R2 G2 B2` for the bottom half) instead of just one.

---

## 2. What each HUB75 pin actually does

The HUB75 connector has 16 pins. Here's what each group does:

| Pins | Name | Purpose |
|---|---|---|
| `R1 G1 B1` | Top RGB data | Color bits for the current pixel in the **top half** of the panel |
| `R2 G2 B2` | Bottom RGB data | Color bits for the current pixel in the **bottom half** of the panel |
| `A B C D (E)` | Row address | A binary number that selects **which row pair** is currently active (like choosing a row out of 16, or 32 if `E` is used) |
| `CLK` | Clock | Pulses once per pixel, telling the shift registers "the RGB pins are ready, shift this pixel in" |
| `LAT` | Latch | Pulses once per full row, telling the panel "you've received a whole row, now display it" |
| `OE` | Output Enable | Turns the LEDs on/off; also used to control brightness (more on that below) |

### Why R1/G1/B1 is just 1 bit each, not 8 bits each

At any single instant, each color channel pin (R1, G1, B1...) can only be HIGH or LOW; a raw HUB75 panel has no built-in concept of "brightness levels" per pixel. So how do you get millions of colors out of a 1-bit-per-color signal?

**Binary Code Modulation (BCM):** the driving library redraws the *entire panel* multiple times per refresh, each time toggling that same 1-bit R/G/B signal, but holding it on for different durations (weighted like binary: 1x, 2x, 4x, 8x...). Averaged over time, your eye perceives this as a real brightness gradient. This is what the "bit depth" setting in the code controls: higher bit depth = smoother color gradients, but slower refresh (more redraws per frame).

### The address pins (A, B, C, D, E) and "scan rate"

The address pins tell the panel which pair of rows is currently being written. The number of address pins you need depends on the panel's scan rate:

- **1/16 scan** (most 64x32 panels): 16 row-pairs → needs 4 address pins (`A B C D`, since 2^4 = 16). Total rows = 16 pairs x 2 = 32.
- **1/32 scan** (most 64x64 panels): 32 row-pairs → needs 5 address pins (`A B C D E`, since 2^5 = 32). Total rows = 32 pairs x 2 = 64.

This is why our 64x32 panel only uses `A B C D` and leaves `E` disconnected in the code.

---

## 3. How ~13 pins are enough to control all 2048 pixels

A 64x32 panel has 2048 pixels. The connector has 6 RGB pins, 4 address pins, and 3 control pins (CLK, LAT, OE), about 13 signals total. That's obviously not "one wire per pixel." It works because of two tricks combined: **multiplexing** (address pins) and **serialization** (clock pin), each dividing the problem down by a different axis.

### Trick 1: multiplexing cuts 32 rows down to 16 row-pairs

The 4 address pins (`A B C D`) form a 4-bit binary number from 0 to 15. Each value selects one of the 16 row-pairs (row `n` on top, row `n+16` on bottom). At any instant, only that one row-pair is "listening"; every other row-pair ignores whatever is on the RGB pins.

So instead of needing 32 separate row-select wires (one per row), 4 wires are enough, because a binary number can represent 16 distinct choices. This is the same idea as a decoder in digital electronics: `2^4 = 16` row-pairs from just 4 wires.

### Trick 2: shifting handles all 64 columns with only 6 RGB wires

Within a single selected row-pair, you still need to set the color of 64 different columns individually, and you only have 6 color wires (3 for the top row of the pair, 3 for the bottom row). This is solved with a shift register, the same idea as those old Christmas light controllers or a bucket brigade:

1. Put the color for column 0 on `R1 G1 B1 R2 G2 B2`.
2. Pulse `CLK`. This "pushes" that value into the first slot of an internal 64-slot shift register inside the panel, and shoves everything already in the register one slot down.
3. Put the color for column 1 on the same 6 pins, pulse `CLK` again. Column 0's data shifts one more slot down, column 1 now sits in the first slot.
4. Repeat this 64 times, once per column.

After 64 clock pulses, all 64 columns' worth of color data are sitting in the shift register, each in the correct position, even though you only ever used 6 wires. Pulsing `LAT` then copies that entire 64-column row-pair from the shift register into the LEDs all at once, so it lights up.

### Putting the two tricks together

- 4 address wires -> select which of 16 row-pairs is currently "open for writing" (multiplexing).
- 6 RGB wires + 64 clock pulses -> load an entire row-pair's worth of color data serially, one column at a time (shifting).
- Repeat for all 16 row-pair addresses, over and over, faster than the eye can see (Section 4 below).

Total pixels actually touched per full sweep: 16 row-pairs x 64 columns x 2 rows-per-pair = 2048, exactly matching the panel. The number of physical wires stays tiny because address pins trade wire count for time (only one row-pair active at a time) and the clock/shift approach trades wire count for a sequence of pulses (one column at a time), instead of every pixel needing its own dedicated wire.

## 4. The refresh loop, in plain English

Every frame, the driving library (Adafruit_Protomatter on this board) does roughly this, over and over, very fast:

1. Set the row address (`A B C D`) to select row pair 0.
2. For each of the 64 columns: put that pixel's color on `R1 G1 B1 R2 G2 B2`, pulse `CLK` to shift it in.
3. Pulse `LAT` to latch the whole row into the display.
4. Briefly toggle `OE` to control how long this row stays lit (brightness).
5. Move to row pair 1, repeat... up to row pair 15.
6. Repeat all of the above multiple times per frame at different `OE` durations, for BCM color depth.

Your Arduino code never does any of this directly. The Protomatter library handles the entire shifting/latching/timing dance in the background; your job is just to tell it your pixel data (`drawPixel`, `print`, etc.) and call `.show()`.

---

## 5. Mapping the physical pins to the code

This is the part that trips people up: the HUB75 connector pins don't have fixed GPIO numbers, they depend on how the board designer wired them internally. On the MatrixPortal S3, Adafruit chose specific ESP32-S3 GPIOs for each HUB75 signal, and the sketch just has to tell the library which GPIO corresponds to which signal.

Here's the sketch's pin section, matched to the table from Section 2:

```cpp
uint8_t rgbPins[]  = {42, 41, 40, 38, 39, 37};
//                     R1  G1  B1  R2  G2  B2

uint8_t addrPins[] = {45, 36, 48, 35};
//                     A   B   C   D

uint8_t clockPin   = 2;   // CLK
uint8_t latchPin   = 47;  // LAT
uint8_t oePin      = 14;  // OE
```

The **order inside `rgbPins[]` matters**. It's always `R1, G1, B1, R2, G2, B2` in that exact sequence, because that's the order the library expects and it must match the physical wiring on the board. If you ever swap two entries by mistake, colors will look wrong (e.g. red shows as blue) instead of the image failing to appear at all.

Same idea for `addrPins[]`: the order must be `A, B, C, D, (E)`, since together they form a binary number selecting the row pair.

### The constructor: turning those pins into a working display

```cpp
Adafruit_Protomatter matrix(
  PANEL_WIDTH,           // 64 -- pixels per row
  4,                      // bit depth per color channel (BCM steps, see Section 2)
  1, rgbPins,             // 1 = one RGB pin set (no chained panels), then the array itself
  sizeof(addrPins), addrPins,  // how many address pins, then the array itself
  clockPin, latchPin, oePin,
  false                   // double buffering off
);
```

Notice `PANEL_HEIGHT` (32) never appears explicitly. The library derives it from `sizeof(addrPins)`: 4 address pins → 2^4 x 2 = 32 rows. This is exactly the scan-rate math from Section 2. If you ever plug in a 64x64 panel, you'd add the `E` pin back into `addrPins` (making it 5 pins), and the library would automatically compute a height of 64, no need to change `PANEL_HEIGHT` logic by hand.

### `bit depth` (the `4` in the constructor)

This is the BCM color depth mentioned in Section 2. Valid range is roughly 1-6 on ESP32-S3:
- Lower (1-2): faster refresh, blocky/banded colors, less CPU/RAM.
- Higher (5-6): smoother gradients, slightly more visible flicker or lower max frame rate, more RAM for the frame buffer.
- `4` is a reasonable default to start testing with.

---

## 6. Step by step: running the test

1. **Install the library.** Arduino IDE → Library Manager → search "Adafruit Protomatter" → Install.
2. **Install the ESP32-S3 board package** if you haven't (Boards Manager → esp32 by Espressif Systems), then select the correct MatrixPortal S3 / Feather ESP32-S3 board entry under Tools → Board.
3. **Wire the panel.** Plug the HUB75 ribbon cable from the panel into the connector on the back of the MatrixPortal S3. There's only one way it fits correctly (keyed connector).
4. **Power the panel properly.** Connect a 5V supply capable of at least 2-4A to the board's screw terminals. Don't rely on USB power alone for the matrix; USB from your computer can't supply enough current at full brightness and you'll get dim/glitchy output or a brownout reset.
5. **Upload `hub75_64x32_test.ino`.**
6. **Open the Serial Monitor** at 115200 baud. After `matrix.begin()` runs, you should see a status code print out. `0` means success (`PROTOMATTER_OK`). Any other number means a pin or configuration mismatch, double check the arrays in Section 4 against your board's actual pinout.
7. **Look at the panel.** You should see the text "Hello!" in green in the top-left area. If you see:
   - **Nothing at all:** check power to the panel and that `matrix.begin()` returned `0`.
   - **Scrambled / interlaced image, or image split into odd stripes:** the panel's scan rate doesn't match your `addrPins` count (e.g. you have a 1/8 scan panel instead of 1/16). Check your panel's datasheet.
   - **Wrong colors (e.g. red where green should be):** the order inside `rgbPins[]` doesn't match the physical wiring, double-check the R1/G1/B1/R2/G2/B2 order.
   - **Dim or flickery image:** power supply issue, use a proper 5V supply on the screw terminals, not USB.

---

## 7. Quick mental model to keep

Think of the matrix as a **printer for one line of pixels at a time**: the address pins choose which row-pair "sheet of paper" is currently in the printer, the RGB pins are the "ink" being fed in column by column, the clock pin is the "print head" moving one step per pixel, the latch pin is "commit this line to the page," and OE is the light switch that also happens to control brightness through how briefly you flick it. The Protomatter library just does this printing job over and over, faster than your eyes can notice, for every row and every brightness level, so you (the Arduino sketch) only ever have to think in terms of pixels and colors, not electrical signals.
