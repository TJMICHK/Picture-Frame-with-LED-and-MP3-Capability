#include <Adafruit_NeoPixel.h>
#include <HardwareSerial.h>
#include <DFRobotDFPlayerMini.h>

// ---------------- LED SETUP ----------------
#define LED_PIN     32
#define LED_COUNT   40
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

// ---------------- BUTTONS ----------------
// Your wiring: 3V3 -> button -> GPIO, and 10k pulldown to GND
// => active HIGH
#define PLAY_PIN    25   // D25
#define SKIP_PIN    19   // D19
#define MODE_PIN     2   // D2 / GPIO2  (active HIGH w/ external 10k pulldown)

// ---------------- DFPLAYER ----------------
HardwareSerial mp3Serial(2);
DFRobotDFPlayerMini mp3;
bool mp3Ready = false;

// ---------------- MODES ----------------
enum Mode { MODE_MUSIC, MODE_ENCOURAGEMENT };
Mode currentMode = MODE_MUSIC;

// SD folders: /01 and /02
const uint8_t MUSIC_FOLDER   = 1;  // /01
const uint8_t ENCOUR_FOLDER  = 2;  // /02

// Track counts
const uint8_t MUSIC_FIRST = 1;
const uint8_t MUSIC_LAST  = 6;     // /01/001.mp3 ... /01/006.mp3

const uint8_t ENCOUR_FIRST = 1;
const uint8_t ENCOUR_LAST  = 2;    // /02/001.mp3 ... /02/002.mp3

uint8_t currentTrack = 1;
bool isPlaying = false;

// ---------------- ANIMATION STATE ----------------
uint16_t hueOffset = 0;
unsigned long lastAnimUpdate = 0;
const unsigned long animIntervalMs = 30;

// ---------------- ACTIVE-HIGH BUTTON DEBOUNCE ----------------
struct DebouncedButtonHigh {
  uint8_t pin;
  bool lastReading = false;
  bool debouncedState = false;
  unsigned long lastDebounceTime = 0;
  const unsigned long debounceMs = 35;

  void begin(uint8_t p) {
    pin = p;
    pinMode(pin, INPUT);  // external pulldown used
    lastReading = (digitalRead(pin) == HIGH);
    debouncedState = lastReading;
    lastDebounceTime = millis();
  }

  bool pressedEvent() { // rising edge
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

DebouncedButtonHigh playBtn;
DebouncedButtonHigh skipBtn;
DebouncedButtonHigh modeBtn;

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

// ----- Mode helpers -----
uint8_t folderForMode() {
  return (currentMode == MODE_MUSIC) ? MUSIC_FOLDER : ENCOUR_FOLDER;
}
uint8_t firstTrackForMode() {
  return (currentMode == MODE_MUSIC) ? MUSIC_FIRST : ENCOUR_FIRST;
}
uint8_t lastTrackForMode() {
  return (currentMode == MODE_MUSIC) ? MUSIC_LAST : ENCOUR_LAST;
}
const char* modeName() {
  return (currentMode == MODE_MUSIC) ? "MUSIC" : "ENCOURAGEMENT";
}

// ----- Playback helpers -----
// IMPORTANT: playFolder expects 3-digit file names: 001.mp3, 002.mp3, ...
void playCurrent() {
  if (!mp3Ready) return;

  uint8_t folder = folderForMode();
  mp3.playFolder(folder, currentTrack);
  isPlaying = true;

  Serial.print("Mode=");
  Serial.print(modeName());
  Serial.print("  Playing /");
  if (folder < 10) Serial.print("0");
  Serial.print(folder);
  Serial.print("/");

  if (currentTrack < 10) Serial.print("00");
  else if (currentTrack < 100) Serial.print("0");
  Serial.print(currentTrack);
  Serial.println(".mp3");
}

void stopAll() {
  if (!mp3Ready) return;
  mp3.stop();
  isPlaying = false;
  currentTrack = firstTrackForMode();
  Serial.println("Stopped.");
}

void startFromBeginning() {
  currentTrack = firstTrackForMode();
  playCurrent();
}

void skipTrack() {
  if (!isPlaying) {
    Serial.println("Skip ignored (not playing).");
    return;
  }

  uint8_t last = lastTrackForMode();
  if (currentTrack < last) {
    currentTrack++;
    playCurrent();
  } else {
    Serial.println("End of this mode -> stopping.");
    stopAll();
  }
}

void toggleMode() {
  stopAll();
  currentMode = (currentMode == MODE_MUSIC) ? MODE_ENCOURAGEMENT : MODE_MUSIC;
  currentTrack = firstTrackForMode();

  Serial.print("MODE changed -> ");
  Serial.println(modeName());
}

void tryInitDFPlayer() {
  Serial.println("Initializing DFPlayer...");
  delay(800);

  for (int attempt = 1; attempt <= 15; attempt++) {
    Serial.print("Attempt ");
    Serial.println(attempt);

    if (mp3.begin(mp3Serial)) {
      mp3Ready = true;
      mp3.volume(20);
      delay(150);
      Serial.println("DFPlayer ready.");
      Serial.println("Folders: /01 (music), /02 (encouragement)");
      Serial.println("Files must be: 001.mp3, 002.mp3, ... inside each folder");
      return;
    }
    delay(300);
  }

  mp3Ready = false;
  Serial.println("DFPlayer NOT found.");
}

void setup() {
  Serial.begin(115200);
  delay(1200);

  strip.begin();
  strip.setBrightness(80);
  strip.clear();
  strip.show();

  playBtn.begin(PLAY_PIN);
  skipBtn.begin(SKIP_PIN);
  modeBtn.begin(MODE_PIN);

  mp3Serial.begin(9600, SERIAL_8N1, 16, 17);

  currentMode = MODE_MUSIC;
  currentTrack = MUSIC_FIRST;

  tryInitDFPlayer();
}

void loop() {
  runRainbowAnimation();

  if (!mp3Ready) return;

  // ---------------- NEW: auto-advance when a track finishes ----------------
  if (mp3.available()) {
    uint8_t type = mp3.readType();
    int value = mp3.read();

    // Most important event: track finished
    if (type == DFPlayerPlayFinished) {
      Serial.print("Track finished (value=");
      Serial.print(value);
      Serial.println(") -> auto-skip");
      skipTrack();
    }
  }
  // ------------------------------------------------------------------------

  // MODE toggle
  if (modeBtn.pressedEvent()) {
    Serial.println("MODE pressed");
    toggleMode();
  }

  // PLAY starts from beginning of current mode
  if (playBtn.pressedEvent()) {
    Serial.println("PLAY pressed");
    startFromBeginning();
  }

  // SKIP within current mode
  if (skipBtn.pressedEvent()) {
    Serial.println("SKIP pressed");
    skipTrack();
  }

  delay(1);
}
