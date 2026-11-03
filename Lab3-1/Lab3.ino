#define LED_PIN 17
#define BUTTON_SW1 16
#define BUTTON_SW2 14


void setup() {
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON_SW1, INPUT_PULLUP);
  Serial.begin(115200);
}

void loop() {
  if (digitalRead(BUTTON_SW1) == LOW) {
    digitalWrite(LED_PIN, LOW);
    Serial.print("ติด");
  } else {
    digitalWrite(LED_PIN, HIGH);
    Serial.println("ดับ");
  }
}
