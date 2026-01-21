#include <Adafruit_NeoPixel.h>

#define LED_PIN   32
#define LED_COUNT 40

Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

// ---------- Twinkle settings ----------
#define MAX_TWINKLES 6        // how many LEDs can twinkle at once
#define TWINKLE_FADE 15       // fade speed (lower = faster)

// ---------- Twinkle state ----------
struct Twinkle {
  int pixel;
  uint8_t brightness;
  uint32_t color;
  bool active;
};

Twinkle twinkles[MAX_TWINKLES];

// ---------- Utility ----------
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

      if (twinkles[i].brightness > TWINKLE_FADE)
        twinkles[i].brightness -= TWINKLE_FADE;
      else
        twinkles[i].active = false;
    }
  }

  // Random chance to spawn a new twinkle
  if (random(10) == 0) {
    spawnTwinkle();
  }

  strip.show();
}

void setup() {
  strip.begin();
  strip.setBrightness(80);
  strip.clear();
  strip.show();

  randomSeed(analogRead(0));   // add randomness
}

void loop() {
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate > 40) {   // update rate
    lastUpdate = millis();
    updateTwinkles();
  }
}
