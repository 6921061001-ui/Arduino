int counter = 0;

void setup() {
    Serial.begin(115200);
}

void loop() {
    counter = counter + 1;

    Serial.print("Counter = ");
    Serial.println(counter);

    delay(1000);
}
