int ledPin = 13;  // use singular name for clarity
char data;

void setup() {
  pinMode(ledPin, OUTPUT);
  Serial.begin(9600);
}

void loop() {
  if (Serial.available()) {
    data = Serial.read();
    
    if (data == '1') {
      // Blink the LED 3 times
      for (int i = 0; i < 3; i++) {
        digitalWrite(ledPin, HIGH);
        delay(500);
        digitalWrite(ledPin, LOW);
        delay(500);
      }
    }
    
    if (data == '0') {
      digitalWrite(ledPin, LOW);   // Turn LED off
    }
  }
}
