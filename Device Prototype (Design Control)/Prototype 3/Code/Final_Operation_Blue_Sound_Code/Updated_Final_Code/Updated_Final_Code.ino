#include <Adafruit_NeoPixel.h>
#include <HardwareSerial.h>
#include <DFRobotDFPlayerMini.h>

// ---------------- LEDs ----------------
#define LED_PIN   32
#define LED_COUNT 40
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

// Twinkle settings
#define MAX_TWINKLES  25
#define TWINKLE_FADE   8
#define LED_UPDATE_MS  40

struct Twinkle {
  int pixel;
  uint8_t brightness;
  uint32_t color;
  bool active;
};
Twinkle twinkles[MAX_TWINKLES];

// ---------------- Buttons (ACTIVE HIGH) ----------------
#define BTN_START 25   // D25
#define BTN_SKIP  19   // D19
#define BTN_MODE  2    // D2

// Simple debounce / edge detect
struct Button {
  uint8_t pin;
  bool stable = false;
  bool lastStable = false;
  bool lastRead = false;
  unsigned long lastChange = 0;
};

const unsigned long DEBOUNCE_MS = 30;
Button bStart{BTN_START}, bSkip{BTN_SKIP}, bMode{BTN_MODE};

bool pressed(Button &b) {
  bool raw = digitalRead(b.pin);

  if (raw != b.lastRead) {
    b.lastRead = raw;
    b.lastChange = millis();
  }

  if (millis() - b.lastChange > DEBOUNCE_MS) {
    b.stable = b.lastRead;
  }

  bool rising = (b.stable == true && b.lastStable == false);
  b.lastStable = b.stable;
  return rising;
}

// ---------------- DFPlayer ----------------
HardwareSerial mp3Serial(2);      // UART2 (default pins: RX2=16, TX2=17)
DFRobotDFPlayerMini mp3;
bool mp3Ready = false;

// Two folders on SD: /01 and /02
uint8_t currentFolder = 1;
uint16_t currentTrack = 1;
bool isPlaying = false;

// IMPORTANT: set these to match how many tracks are in each folder
const uint16_t TRACKS_IN_FOLDER_1 = 6;
const uint16_t TRACKS_IN_FOLDER_2 = 6;

uint16_t tracksInCurrentFolder() {
  return (currentFolder == 1) ? TRACKS_IN_FOLDER_1 : TRACKS_IN_FOLDER_2;
}

void playTrack(uint8_t folder, uint16_t track) {
  if (!mp3Ready) return;
  mp3.playFolder(folder, track);   // plays /0folder/0track.mp3 style
  currentFolder = folder;
  currentTrack = track;
  isPlaying = true;
}

void stopPlayback() {
  if (!mp3Ready) return;
  mp3.stop();
  isPlaying = false;
}

// ---------------- Auto-next when track finishes ----------------
void handleAutoNext() {
  if (!mp3Ready || !isPlaying) return;

  uint16_t maxTracks = tracksInCurrentFolder();

  if (currentTrack < maxTracks) {
    playTrack(currentFolder, currentTrack + 1);
    Serial.print("Auto-next: Track ");
    Serial.println(currentTrack);
  } else {
    stopPlayback();
    Serial.println("Auto-next: reached last track, stopping.");
  }
}

// ---------------- LED modes ----------------
enum LedMode { LED_TWINKLE, LED_RAINBOW };
LedMode ledMode = LED_TWINKLE;

// ---------------- Twinkle helpers ----------------
uint32_t randomTwinkleColor() {
  switch (random(4)) {
    case 0: return strip.Color(0, 0, 255);      // blue
    case 1: return strip.Color(0, 150, 255);    // baby blue
    case 2: return strip.Color(160, 0, 255);    // purple
    default:return strip.Color(255, 255, 255);  // white
  }
}

void spawnTwinkle() {
  for (int i = 0; i < MAX_TWINKLES; i++) {
    if (!twinkles[i].active) {
      twinkles[i].pixel = random(LED_COUNT);
      twinkles[i].brightness = 255;
      twinkles[i].color = randomTwinkleColor();
      twinkles[i].active = true;
      break;
    }
  }
}

void updateTwinkles() {
  strip.clear();

  for (int i = 0; i < MAX_TWINKLES; i++) {
    if (twinkles[i].active) {
      uint8_t b = twinkles[i].brightness;

      uint8_t r = (twinkles[i].color >> 16) & 0xFF;
      uint8_t g = (twinkles[i].color >> 8) & 0xFF;
      uint8_t bl = twinkles[i].color & 0xFF;

      r = (r * b) / 255;
      g = (g * b) / 255;
      bl = (bl * b) / 255;

      strip.setPixelColor(twinkles[i].pixel, r, g, bl);

      if (twinkles[i].brightness > TWINKLE_FADE) twinkles[i].brightness -= TWINKLE_FADE;
      else twinkles[i].active = false;
    }
  }

  if (random(3) == 0) spawnTwinkle();

  strip.show();
}

// ---------------- Rainbow helpers ----------------
// Wheel() maps 0-255 to a color (classic NeoPixel rainbow helper)
uint32_t Wheel(byte WheelPos) {
  WheelPos = 255 - WheelPos;
  if (WheelPos < 85) {
    return strip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  }
  if (WheelPos < 170) {
    WheelPos -= 85;
    return strip.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
  WheelPos -= 170;
  return strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
}

// Non-blocking rainbow animation
void updateRainbow() {
  static uint16_t rainbowOffset = 0; // increments over time

  for (int i = 0; i < LED_COUNT; i++) {
    // Spread colors along the strip + animate by offset
    uint8_t hue = (uint8_t)((i * 256 / LED_COUNT + rainbowOffset) & 0xFF);
    strip.setPixelColor(i, Wheel(hue));
  }
  strip.show();

  rainbowOffset++; // speed; increase by 2/3 to go faster
}

void clearTwinklesState() {
  for (int i = 0; i < MAX_TWINKLES; i++) twinkles[i].active = false;
}

// ---------------- Setup / Loop ----------------
void setup() {
  delay(1000);
  Serial.begin(115200);

  // Buttons: active HIGH, with external pulldown resistors (your wiring)
  pinMode(BTN_START, INPUT);
  pinMode(BTN_SKIP,  INPUT);
  pinMode(BTN_MODE,  INPUT);

  // LEDs
  strip.begin();
  strip.setBrightness(80);
  strip.clear();
  strip.show();

  randomSeed(analogRead(0));

  // DFPlayer init
  mp3Serial.begin(9600, SERIAL_8N1, 16, 17); // RX=16, TX=17

  Serial.println("Initializing DFPlayer...");
  for (int attempt = 1; attempt <= 15; attempt++) {
    if (mp3.begin(mp3Serial)) {
      mp3Ready = true;
      mp3.volume(15); // 0-30
      Serial.println("DFPlayer ready.");
      break;
    }
    delay(2000);
  }
  if (!mp3Ready) Serial.println("DFPlayer NOT found.");
}

void loop() {
  // --- DFPlayer events (auto-advance) ---
  if (mp3Ready && mp3.available()) {
    uint8_t type = mp3.readType();
    if (type == DFPlayerPlayFinished) {
      Serial.println("Track finished.");
      handleAutoNext();
    }
  }

  // --- Buttons ---
  if (pressed(bMode)) {
    ledMode = (ledMode == LED_TWINKLE) ? LED_RAINBOW : LED_TWINKLE;

    // Optional cleanup when switching modes
    strip.clear();
    strip.show();
    clearTwinklesState();

    Serial.print("LED Mode = ");
    Serial.println((ledMode == LED_TWINKLE) ? "TWINKLE" : "RAINBOW");
  }

  if (pressed(bStart)) {
    playTrack(currentFolder, 1);
    Serial.print("Start: Folder /0");
    Serial.print(currentFolder);
    Serial.println(" Track 1");
  }

  if (pressed(bSkip)) {
    uint16_t maxTracks = tracksInCurrentFolder();
    if (!isPlaying) {
      Serial.println("Skip pressed but nothing playing.");
    } else if (currentTrack < maxTracks) {
      playTrack(currentFolder, currentTrack + 1);
      Serial.print("Skip: now Track ");
      Serial.println(currentTrack);
    } else {
      stopPlayback();
      Serial.println("Skip on last track -> stop.");
    }
  }

  // --- LEDs (non-blocking) ---
  static unsigned long lastLed = 0;
  if (millis() - lastLed >= LED_UPDATE_MS) {
    lastLed = millis();

    if (ledMode == LED_TWINKLE) updateTwinkles();
    else updateRainbow();
  }
}
