#include <Adafruit_NeoPixel.h>
#include <HardwareSerial.h>
#include <DFRobotDFPlayerMini.h>

// ---------------- LED SETUP ----------------
#define LED_PIN     32
#define LED_COUNT   40

Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

// ---------------- MP3 SETUP ----------------
HardwareSerial mp3Serial(2);        // UART2
DFRobotDFPlayerMini mp3;

bool mp3Ready = false;

// ---------------- ANIMATION STATE ----------------
uint16_t hueOffset = 0;
unsigned long lastAnimUpdate = 0;
const unsigned long animIntervalMs = 30; // smaller = faster

// Rainbow cycle animation
void runRainbowAnimation() {
  if (millis() - lastAnimUpdate < animIntervalMs) return;
  lastAnimUpdate = millis();

  for (int i = 0; i < LED_COUNT; i++) {
    // Spread hue across strip and scroll it
    uint16_t hue = hueOffset + (i * 65535UL / LED_COUNT);
    strip.setPixelColor(i, strip.gamma32(strip.ColorHSV(hue, 255, 255)));
  }

  strip.show();
  hueOffset += 256; // speed of color rotation
}

void tryInitDFPlayer() {
  Serial.println("Initializing DFPlayer...");
  delay(800);

  for (int attempt = 1; attempt <= 15; attempt++) {
    Serial.print("  Attempt ");
    Serial.println(attempt);

    if (mp3.begin(mp3Serial)) {
      mp3Ready = true;
      Serial.println("DFPlayer found!");

      mp3.volume(20);     // 0–30
      delay(100);
      mp3.play(1);        // plays 0001.mp3
      Serial.println("Playing track 1.");
      return;
    }

    // Keep rainbow running while retrying
    unsigned long t0 = millis();
    while (millis() - t0 < 300) {
      runRainbowAnimation();
      delay(1);
    }
  }

  Serial.println("DFPlayer NOT found (will keep trying in loop).");
  mp3Ready = false;
}

void setup() {
  Serial.begin(115200);
  delay(2000);

  // LEDs
  strip.begin();
  strip.setBrightness(80);
  strip.clear();
  strip.show();

  // Start UART to DFPlayer
  mp3Serial.begin(9600, SERIAL_8N1, 16, 17);

  // First init attempt
  tryInitDFPlayer();
}

void loop() {
  delay(1000);
  // Always animate LEDs
  runRainbowAnimation();

  delay(1000);
  
  // Retry DFPlayer if needed
  if (!mp3Ready) {
    static unsigned long lastTry = 0;
    if (millis() - lastTry > 1500) {
      lastTry = millis();
      tryInitDFPlayer();
    }
  }

  delay(1);
}
