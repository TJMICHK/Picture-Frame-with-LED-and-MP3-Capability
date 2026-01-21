#include <Adafruit_NeoPixel.h>
#include <HardwareSerial.h>
#include <DFRobotDFPlayerMini.h>

#define LED_PIN     27
#define LED_COUNT   60
#define AUDIO_PIN   34     // envelope input
#define POT_PIN     35     // volume pot wiper (ADC1)

Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

HardwareSerial mp3Serial(2);
DFRobotDFPlayerMini mp3;

unsigned long lastPrint = 0;

// ---- TUNING (LED REACTIVITY) ----
float sensitivity = 0.6f;  // ↑ = more LEDs for same audio
int   minLit = 2;          // minimum LEDs lit
int   noiseMargin = 30;    // subtract baseline noise

// Auto-calibration
float floorLvl = 50;       // baseline (auto updates)
float peakLvl  = 200;      // peak (auto updates)

// ---- VOLUME POT ----
int currentVol = -1;
float potSmooth = 0;
unsigned long lastVolUpdate = 0;

// Map pot ADC (0..4095) to DFPlayer volume (0..30)
int potToVolume(int adc) {
  int v = map(adc, 0, 4095, 0, 30);
  if (v < 0) v = 0;
  if (v > 30) v = 30;
  return v;
}

void setup() {
  Serial.begin(115200);

  // LEDs
  strip.begin();
  strip.setBrightness(80);
  strip.clear();
  strip.show();

  // ADC
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);
  pinMode(POT_PIN, INPUT);

  // MP3
  mp3Serial.begin(9600, SERIAL_8N1, 16, 17); // RX=16 TX=17
  if (!mp3.begin(mp3Serial)) {
    Serial.println("MP3 module not found");
    strip.fill(strip.Color(255, 0, 0));
    strip.show();
    while (true) delay(100);
  }
  Serial.println("MP3 OK");

  // Initialize volume from pot (so it matches knob position on boot)
  int potRaw = analogRead(POT_PIN);
  potSmooth = potRaw;
  currentVol = potToVolume((int)potSmooth);
  mp3.volume(currentVol);
  Serial.print("Initial Volume: ");
  Serial.println(currentVol);

  delay(200);
  mp3.play(1);

  // Startup wipe
  for (int i = 0; i < LED_COUNT; i++) {
    strip.setPixelColor(i, strip.Color(0, 0, 40));
    strip.show();
    delay(5);
  }
  strip.clear();
  strip.show();
}

void loop() {
  // ----------------- LED REACTIVITY -----------------
  int raw = analogRead(AUDIO_PIN);

  // Update floor (baseline) slowly
  floorLvl = floorLvl * 0.995f + raw * 0.005f;

  // Compute level above floor
  float level = raw - floorLvl - noiseMargin;
  if (level < 0) level = 0;

  // Peak tracking with decay
  if (level > peakLvl) peakLvl = level;
  peakLvl *= 0.995f;
  if (peakLvl < 50) peakLvl = 50;

  // Normalize and apply sensitivity
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

  // ----------------- VOLUME POT UPDATE -----------------
  // Update volume ~5x/sec to avoid spamming the module
  if (millis() - lastVolUpdate > 50) {
    lastVolUpdate = millis();

    int potRaw = analogRead(POT_PIN);

    // Smooth pot readings to prevent jitter
    potSmooth = potRaw;

    int newVol = potToVolume((int)potSmooth);

    if (newVol != currentVol) {
      currentVol = newVol;
      mp3.volume(currentVol);
      Serial.print("Volume: ");
      Serial.println(currentVol);
    }
  }

  // Debug 5x/sec (LED + audio)
  if (millis() - lastPrint > 200) {
    lastPrint = millis();
    Serial.print("raw=");   Serial.print(raw);
    Serial.print(" floor=");Serial.print((int)floorLvl);
    Serial.print(" level=");Serial.print((int)level);
    Serial.print(" peak="); Serial.print((int)peakLvl);
    Serial.print(" lit=");  Serial.print(lit);
    Serial.print(" vol=");  Serial.println(currentVol);
  }

  delay(10);
}
