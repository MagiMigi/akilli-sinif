/*
 * Akıllı Sınıf - ESP32 Blink Test
 * Bu kod ESP32'nin düzgün çalıştığını test eder
 * Built-in LED yanıp sönecek
 */

// ESP32 DevKit'te built-in LED genellikle GPIO 2'de
#define LED_PIN 2

void setup() {
  // Seri port başlat (debug için)
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("=================================");
  Serial.println("Akıllı Sınıf - ESP32 Blink Test");
  Serial.println("=================================");
  
  // LED pinini çıkış olarak ayarla
  pinMode(LED_PIN, OUTPUT);
  
  Serial.println("LED pin ayarlandı: GPIO 2");
  Serial.println("Test başlıyor...");
}

void loop() {
  // LED'i yak
  digitalWrite(LED_PIN, HIGH);
  Serial.println("LED: AÇIK");
  delay(1000);
  
  // LED'i söndür
  digitalWrite(LED_PIN, LOW);
  Serial.println("LED: KAPALI");
  delay(1000);
}
