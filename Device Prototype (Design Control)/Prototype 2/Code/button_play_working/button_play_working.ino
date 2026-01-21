#include <Adafruit_NeoPixel.h>
#include <HardwareSerial.h>
#include <DFRobotDFPlayerMini.h>

// ---------------- LED SETUP ----------------
#define LED_PIN     32
#define LED_COUNT   40

Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

// ---------------- BUTTON SETUP ----------------
#define BUTTON_PIN  25   // D25 / GPIO25

// ---------------- MP3 SETUP ----------------
HardwareSerial mp3Serial(2);        // UART2
DFRobotDFPlayerMini mp3;

bool mp3Ready = false;
bool musicStarted = false;          // track has been started at least once

// ---------------- ANIMATION STATE ----------------
uint16_t hueOffset = 0;
unsigned long lastAnimUpdate = 0;
const unsigned long animIntervalMs = 30; // smaller = faster

// ---------------- BUTTON DEBOUNCE STATE ----------------
bool lastButtonReading = false;
bool debouncedButtonState = false;
unsigned long lastDebounceTime = 0;
const unsigned long debounceMs = 35;

// Rainbow cycle animation
void runRainbowAnimation() {
  if (millis() - lastAnimUpdate < animIntervalMs) return;
  lastAnimUpdate = millis();

  for (int i = 0; i < LED_COUNT; i++) {
    uint16_t hue = hueOffset + (i * 65535UL / LED_COUNT);
    strip.setPixelColor(i, strip.gamma32(strip.ColorHSV(hue, 255, 255)));
  }

  strip.show();
  hueOffset += 256;
}

// Returns true once per press (on rising edge)
bool buttonPressedEvent() {
  bool reading = (digitalRead(BUTTON_PIN) == HIGH);

  if (reading != lastButtonReading) {
    lastDebounceTime = millis();
    lastButtonReading = reading;
  }

  if (millis() - lastDebounceTime > debounceMs) {
    // stable reading -> update debounced state
    if (reading != debouncedButtonState) {
      debouncedButtonState = reading;

      // Rising edge = button press
      if (debouncedButtonState == true) return true;
    }
  }

  return false;
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

      // NOTE: We no longer auto-play here.
      Serial.println("Ready. Waiting for button press to start track 1.");
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

  // Button: since your wiring is 3V3 -> button -> GPIO25,
  // use internal pulldown so it reads LOW when not pressed.
  pinMode(BUTTON_PIN, INPUT_PULLDOWN);

  // Start UART to DFPlayer
  mp3Serial.begin(9600, SERIAL_8N1, 16, 17);

  // Init DFPlayer (but don't start music)
  tryInitDFPlayer();
}

void loop() {
  runRainbowAnimation();

  // Retry DFPlayer if needed
  if (!mp3Ready) {
    static unsigned long lastTry = 0;
    if (millis() - lastTry > 1500) {
      lastTry = millis();
      tryInitDFPlayer();
    }
  }

  // Start music on button press
  if (mp3Ready && buttonPressedEvent()) {
    Serial.println("Button pressed -> starting track 1");
    mp3.play(1);              // plays 0001.mp3
    musicStarted = true;
  }

  delay(1);
}
