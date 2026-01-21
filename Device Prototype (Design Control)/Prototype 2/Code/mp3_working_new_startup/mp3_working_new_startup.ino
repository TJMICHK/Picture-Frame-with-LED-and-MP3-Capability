#include <HardwareSerial.h>
#include <DFRobotDFPlayerMini.h>

HardwareSerial mp3Serial(2);        // UART2
DFRobotDFPlayerMini mp3;

bool mp3Ready = false;

void tryInitDFPlayer() {
  Serial.println("Initializing DFPlayer...");

  // Give the module time to power up / read SD card
  delay(800);

  // Try multiple times instead of locking up forever
  for (int attempt = 1; attempt <= 15; attempt++) {
    Serial.print("  Attempt ");
    Serial.print(attempt);
    Serial.println("...");

    if (mp3.begin(mp3Serial)) {
      mp3Ready = true;
      Serial.println("DFPlayer found!");

      mp3.volume(20);     // 0–30
      delay(100);         // small settle delay
      mp3.play(1);        // plays 0001.mp3
      Serial.println("Playing track 1.");
      return;
    }

    delay(300);
  }

  Serial.println("DFPlayer NOT found (will keep trying in loop).");
  mp3Ready = false;
}

void setup() {
  Serial.begin(115200);
  delay(2000); // let Serial come up

  // Start UART to DFPlayer
  mp3Serial.begin(9600, SERIAL_8N1, 16, 17);

  // First init attempt
  tryInitDFPlayer();
}

void loop() {
  // If it wasn't ready at boot, keep trying periodically
  if (!mp3Ready) {
    tryInitDFPlayer();
    delay(1500);
  }

  // Nothing else to do
  delay(50);
}
