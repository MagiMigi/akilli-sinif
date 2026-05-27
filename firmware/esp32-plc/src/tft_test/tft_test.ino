/*
 * TFT TEST SKETCH — sadece ekran tanisi icin
 *
 * Bagli baska bir sey OLMASIN. Sadece ESP32 + TFT + USB.
 *
 * Pinler User_Setup.h'tan okunur:
 *   CS=17 DC=33 RST=32 MOSI=23 SCLK=18
 *   VCC=3V3, GND=GND, LED/BL=3V3
 *
 * Bekleneni: Renkler degisir, "TFT OK" yazisi cikar.
 * Hepsi beyaz kalirsa: modul olu veya kablo swap.
 */

#include <SPI.h>
#include <TFT_eSPI.h>

TFT_eSPI tft = TFT_eSPI();

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== TFT TEST BASLIYOR ===");

  Serial.println("[1] tft.init()");
  tft.init();

  Serial.println("[2] setRotation(1)");
  tft.setRotation(1);

  Serial.println("[3] Kirmizi");
  tft.fillScreen(TFT_RED);
  delay(1500);

  Serial.println("[4] Yesil");
  tft.fillScreen(TFT_GREEN);
  delay(1500);

  Serial.println("[5] Mavi");
  tft.fillScreen(TFT_BLUE);
  delay(1500);

  Serial.println("[6] Siyah + yazi");
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(10, 30);
  tft.println("TFT OK");
  tft.setTextSize(1);
  tft.setCursor(10, 60);
  tft.println("128x128 ST7735");

  Serial.println("=== TEST BITTI ===");
}

void loop() {
  // Her 3 saniyede ekranin tepesinde sayac
  static uint32_t lastMs = 0;
  static int counter = 0;
  if (millis() - lastMs > 1000) {
    lastMs = millis();
    counter++;
    tft.fillRect(10, 90, 100, 20, TFT_BLACK);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.setTextSize(1);
    tft.setCursor(10, 95);
    tft.print("Sayac: ");
    tft.print(counter);
    Serial.print("Sayac: ");
    Serial.println(counter);
  }
}
