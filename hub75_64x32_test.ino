#include <Adafruit_Protomatter.h>

#define PANEL_WIDTH  64
#define PANEL_HEIGHT 32

uint8_t rgbPins[]  = {42, 41, 40, 38, 39, 37};
uint8_t addrPins[] = {45, 36, 48, 35}; // A,B,C,D only -- 64x32 is 1/16 scan
uint8_t clockPin   = 2;
uint8_t latchPin   = 47;
uint8_t oePin      = 14;

Adafruit_Protomatter matrix(
  PANEL_WIDTH,
  4,              // bit depth per color channel
  1, rgbPins,
  sizeof(addrPins), addrPins,
  clockPin, latchPin, oePin,
  false           // double buffering off; set true if you need tear-free animation
);

void setup() {
  Serial.begin(115200);

  ProtomatterStatus status = matrix.begin();
  Serial.printf("Protomatter begin() status: %d\n", status);

  matrix.fillScreen(0);
  matrix.setTextSize(1);
  matrix.setTextColor(matrix.color565(0, 255, 0));
  matrix.setCursor(2, 12);
  matrix.print("Hello!");
  matrix.show();
}

void loop() {
  // static test pattern; add animation here
}
