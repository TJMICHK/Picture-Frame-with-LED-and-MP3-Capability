#include <Adafruit_NeoPixel.h>
#include <HardwareSerial.h>
#include <DFRobotDFPlayerMini.h>

// ---------------- LED SETUP ----------------
#define LED_PIN     27
#define LED_COUNT   30
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

// ---------------- BUTTON SETUP ----------------
// Wiring style you’re using: 3V3 -> button -> GPIO
// So use INPUT_PULLDOWN (or external pulldown if you have it)
#define PLAY_PIN    25   // D25 / GPIO25
#define SKIP_PIN    19   // D19 / GPIO19

// ---------------- MP3 SETUP ----------------
HardwareSerial mp3Serial(2);        // UART2
DFRobotDFPlayerMini mp3;

bool mp3Ready = false;

// ---------------- TRACK STATE ----------------
const int FIRST_TRACK = 1;
const int LAST_TRACK  = 6;          // CHANGE if you have more/less
int currentTrack = FIRST_TRACK;
bool isPlaying = false;

// ---------------- ANIMATION STATE ----------------
uint16_t hueOffset = 0;
unsigned long lastAnimUpdate = 0;
const unsigned long animIntervalMs = 30;

// ---------------- DEBOUNCE (PER-BUTTON) ----------------
struct DebouncedButton {
  uint8_t pin;
  bool lastReading = false;
  bool debouncedState = false;
  unsigned long lastDebounceTime = 0;
  const unsigned long debounceMs = 35;

  void begin(uint8_t p) {
    pin = p;
    pinMode(pin, INPUT_PULLDOWN);
    lastReading = (digitalRead(pin) == HIGH);
    debouncedState = lastReading;
    lastDebounceTime = millis();
  }

  // rising-edge press event
  bool pressedEvent() {
    bool reading = (digitalRead(pin) == HIGH);

    if (reading != lastReading) {
      lastDebounceTime = millis();
      lastReading = reading;
    }

    if (millis() - lastDebounceTime > debounceMs) {
      if (reading != debouncedState) {
        debouncedState = reading;
        if (debouncedState == true) return true;
      }
    }
    return false;
  }
};

DebouncedButton playBtn;
DebouncedButton skipBtn;

// ---------------- LED ANIMATION ----------------
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

// ---------------- MP3 CONTROL ----------------
void playTrack(int t) {
  if (!mp3Ready) return;
  currentTrack = t;
  mp3.play(currentTrack);     // NOTE: DFPlayer often plays by index, not filename
  isPlaying = true;

  Serial.print("Playing track ");
  Serial.println(currentTrack);
}

void stopPlayback() {
  if (!mp3Ready) return;
  mp3.stop();
  isPlaying = false;
  currentTrack = FIRST_TRACK;
  Serial.println("Stopped. Reset to track 1.");
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

      Serial.println("Ready. PLAY starts at track 1. SKIP goes next; on last track, stops.");
      return;
    }

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

  // Buttons
  playBtn.begin(PLAY_PIN);
  skipBtn.begin(SKIP_PIN);

  // DFPlayer UART
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

  // PLAY button: start (or restart) at track 1
  if (mp3Ready && playBtn.pressedEvent()) {
    Serial.println("PLAY pressed -> starting track 1");
    playTrack(FIRST_TRACK);
  }

  // SKIP button: only works if playing
  if (mp3Ready && skipBtn.pressedEvent()) {
    Serial.println("SKIP pressed");

    if (!isPlaying) {
      Serial.println("Skip ignored (not playing).");
    } else if (currentTrack < LAST_TRACK) {
      playTrack(currentTrack + 1);
    } else {
      Serial.println("Skip on last track -> stopping.");
      stopPlayback();
    }
  }

  delay(1);
}
