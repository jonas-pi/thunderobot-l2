#include "status_screen.h"

#include <Arduino.h>
#include <SPI.h>

#include "glyphs.h"

namespace {

constexpr int kWidth = 170;
constexpr int kHeight = 320;
constexpr int kColOffset = 35;

constexpr int kPinSck = 5;
constexpr int kPinMosi = 4;
constexpr int kPinDc = 6;
constexpr int kPinCs = 7;
constexpr int kPinRst = 14;
constexpr int kPinBl = 15;

constexpr uint16_t kBg = 0x1082;
constexpr uint16_t kCard = 0x18C3;
constexpr uint16_t kWhite = 0xFFFF;
constexpr uint16_t kGreen = 0x07E0;
constexpr uint16_t kRed = 0xF800;

bool shown = false;
bool shownBle = false;
bool shownZigbee = false;

void writeCmd(uint8_t command) {
  digitalWrite(kPinDc, LOW);
  digitalWrite(kPinCs, LOW);
  SPI.transfer(command);
  digitalWrite(kPinCs, HIGH);
}

void writeData(const uint8_t* bytes, size_t length) {
  digitalWrite(kPinDc, HIGH);
  digitalWrite(kPinCs, LOW);
  SPI.writeBytes(bytes, length);
  digitalWrite(kPinCs, HIGH);
}

void writeDataByte(uint8_t value) {
  writeData(&value, 1);
}

void setWindow(int x, int y, int w, int h) {
  int x0 = x + kColOffset;
  int x1 = x0 + w - 1;
  int y1 = y + h - 1;
  uint8_t column[] = {uint8_t(x0 >> 8), uint8_t(x0), uint8_t(x1 >> 8), uint8_t(x1)};
  uint8_t row[] = {uint8_t(y >> 8), uint8_t(y), uint8_t(y1 >> 8), uint8_t(y1)};
  writeCmd(0x2A);
  writeData(column, sizeof(column));
  writeCmd(0x2B);
  writeData(row, sizeof(row));
  writeCmd(0x2C);
}

void fillRect(int x, int y, int w, int h, uint16_t color) {
  if (w <= 0 || h <= 0) {
    return;
  }
  setWindow(x, y, w, h);
  uint8_t line[kWidth * 2];
  uint8_t hi = color >> 8;
  uint8_t lo = color & 0xFF;
  for (int i = 0; i < w * 2; i += 2) {
    line[i] = hi;
    line[i + 1] = lo;
  }
  digitalWrite(kPinDc, HIGH);
  digitalWrite(kPinCs, LOW);
  for (int row = 0; row < h; row++) {
    SPI.writeBytes(line, w * 2);
  }
  digitalWrite(kPinCs, HIGH);
}

void blitMono(int x, int y, int w, int h, const uint8_t* bits, int scale, uint16_t fg, uint16_t bg) {
  int dw = w * scale;
  uint8_t row[kWidth * 2];
  int bytesPerRow = (w + 7) / 8;
  for (int sy = 0; sy < h * scale; sy++) {
    const uint8_t* src = bits + (sy / scale) * bytesPerRow;
    for (int sx = 0; sx < dw; sx++) {
      int gx = sx / scale;
      bool on = src[gx >> 3] & (0x80 >> (gx & 7));
      uint16_t color = on ? fg : bg;
      row[sx * 2] = color >> 8;
      row[sx * 2 + 1] = color & 0xFF;
    }
    setWindow(x, y + sy, dw, 1);
    digitalWrite(kPinDc, HIGH);
    digitalWrite(kPinCs, LOW);
    SPI.writeBytes(row, dw * 2);
    digitalWrite(kPinCs, HIGH);
  }
}

void drawGlyphs(int x, int y, const uint8_t* const* glyphs, int count, uint16_t fg, uint16_t bg) {
  for (int i = 0; i < count; i++) {
    blitMono(x + i * 32, y, 16, 16, glyphs[i], 2, fg, bg);
  }
}

void paint() {
  fillRect(0, 0, kWidth, kHeight, kBg);

  fillRect(8, 16, kWidth - 16, 120, kCard);
  const uint8_t* bleName[] = {kGlyph_lan, kGlyph_ya};
  drawGlyphs(20, 32, bleName, 2, kWhite, kCard);
  const uint8_t* bleState[] = {shownBle ? kGlyph_yi : kGlyph_wei, kGlyph_lian, kGlyph_jie};
  drawGlyphs(20, 80, bleState, 3, shownBle ? kGreen : kRed, kCard);

  fillRect(8, 160, kWidth - 16, 136, kCard);
  blitMono(20, 176, 56, 16, kGlyph_zigbee, 2, kWhite, kCard);
  const uint8_t* zbState[] = {shownZigbee ? kGlyph_yi : kGlyph_wei, kGlyph_lian, kGlyph_jie};
  drawGlyphs(20, 228, zbState, 3, shownZigbee ? kGreen : kRed, kCard);
}

}  // namespace

void screenBegin() {
  pinMode(kPinDc, OUTPUT);
  pinMode(kPinCs, OUTPUT);
  pinMode(kPinRst, OUTPUT);
  pinMode(kPinBl, OUTPUT);
  digitalWrite(kPinCs, HIGH);
  digitalWrite(kPinBl, HIGH);
  digitalWrite(kPinRst, HIGH);
  delay(20);
  digitalWrite(kPinRst, LOW);
  delay(20);
  digitalWrite(kPinRst, HIGH);
  delay(120);

  SPI.begin(kPinSck, -1, kPinMosi, -1);
  SPI.beginTransaction(SPISettings(40000000, MSBFIRST, SPI_MODE0));

  writeCmd(0x01);
  delay(150);
  writeCmd(0x11);
  delay(120);
  writeCmd(0x3A);
  writeDataByte(0x55);
  writeCmd(0x36);
  writeDataByte(0x00);
  writeCmd(0x21);
  writeCmd(0x13);
  writeCmd(0x29);
  delay(20);

  shown = false;
  shownBle = false;
  shownZigbee = false;
  paint();
  digitalWrite(kPinBl, LOW);
}

void screenShow(bool bleUp, bool zigbeeUp) {
  if (shown && bleUp == shownBle && zigbeeUp == shownZigbee) {
    return;
  }
  shownBle = bleUp;
  shownZigbee = zigbeeUp;
  shown = true;
  paint();
}
