#include <Adafruit_NeoPixel.h>
#include <HardwareSerial.h>
#include <DFRobotDFPlayerMini.h>

#define LED_PIN     27
#define LED_COUNT   60
#define AUDIO_PIN   34

Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

HardwareSerial mp3Serial(2);
DFRobotDFPlayerMini mp3;

unsigned long lastPrint = 0;

// ---- TUNING ----
float sensitivity = 0.6f;     // ↑ = more LEDs for same audio (try 1.5 to 4.0)
int   minLit = 2;             // set to 1 or 2 if you want "always a little on"
int   noiseMargin = 30;       // subtract a little baseline noise

// Auto-calibration
float floorLvl = 50;          // baseline (auto updates)
float peakLvl  = 200;         // peak (auto updates)

void setup() {
  Serial.begin(115200);

  strip.begin();
  strip.setBrightness(80);
  strip.clear();
  strip.show();

  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  mp3Serial.begin(9600, SERIAL_8N1, 16, 17); // RX=16 TX=17
  if (!mp3.begin(mp3Serial)) {
    Serial.println("MP3 module not found");
    strip.fill(strip.Color(255, 0, 0));
    strip.show();
    while (true) delay(100);
  }
  Serial.println("MP3 OK");

  mp3.volume(25);
  delay(200);
  mp3.play(1);

  // startup wipe
  for (int i = 0; i < LED_COUNT; i++) {
    strip.setPixelColor(i, strip.Color(0, 0, 40));
    strip.show();
    delay(5);
  }
  strip.clear();
  strip.show();
}

void loop() {
  int raw = analogRead(AUDIO_PIN);

  // --- Update floor (baseline) slowly ---
  // Floor should follow quiet levels but not chase beats.
  floorLvl = floorLvl * 0.995f + raw * 0.005f;

  // --- Compute level above floor ---
  float level = raw - floorLvl - noiseMargin;
  if (level < 0) level = 0;

  // --- Update peak (max) with decay ---
  if (level > peakLvl) peakLvl = level;
  peakLvl *= 0.995f;               // decay
  if (peakLvl < 50) peakLvl = 50;  // keep sane

  // --- Normalize 0..1 and apply sensitivity ---
  float norm = (peakLvl > 1) ? (level / peakLvl) : 0;
  norm *= sensitivity;
  if (norm > 1) norm = 1;

  int lit = (int)(norm * LED_COUNT + 0.5f);
  if (lit < minLit) lit = minLit;
  if (lit > LED_COUNT) lit = LED_COUNT;

  // Draw bar
  strip.clear();
  for (int i = 0; i < lit; i++) {
    strip.setPixelColor(i, strip.Color(0, 255, 0));
  }
  strip.show();

  // Debug 5x/sec
  if (millis() - lastPrint > 200) {
    lastPrint = millis();
    Serial.print("raw="); Serial.print(raw);
    Serial.print(" floor="); Serial.print((int)floorLvl);
    Serial.print(" level="); Serial.print((int)level);
    Serial.print(" peak="); Serial.print((int)peakLvl);
    Serial.print(" lit="); Serial.println(lit);
  }

  delay(10);
}
