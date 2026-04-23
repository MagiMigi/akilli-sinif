/*
 * Akilli Sinif Sistemi - ESP32 PLC Firmware
 * ==========================================
 *
 * Bu kod ESP32'yi bir PLC gibi calistirir:
 * - WiFiManager ile guvenli WiFi kurulumu (AP portal, NVS'e kayit)
 * - MQTT ile haberlesme
 * - Sensor okuma (simule veya gercek)
 * - Aktuator kontrolu (LED dimming, fan)
 * - TFT Ekran (ST7735) ile bilgi gosterimi
 * - OTA (Over-the-Air) firmware guncelleme (MQTT tetikli)
 *
 * MQTT Topic Yapisi:
 * - Publish:     akilli-sinif/{sinif_id}/sensors/{sensor_type}
 * - Subscribe:   akilli-sinif/{sinif_id}/control/{actuator_type}
 * - OTA Komut:  akilli-sinif/{sinif_id}/control/ota
 * - OTA Durum:  akilli-sinif/{sinif_id}/status/ota
 * - Status:     akilli-sinif/{sinif_id}/status/connection
 *
 * ILK KURULUM:
 *   1. Firmware'i yak
 *   2. "Akilli-Sinif-Setup" WiFi agina baglan
 *   3. Acilan portala gir: WiFi, MQTT IP, Sinif ID, Mock modu ayarla
 *   4. Kaydet — ESP32 yeniden baslar ve baglanir
 *
 * CONFIG SIFIRLAMA:
 *   GPIO0 butonuna 5 saniye bas → NVS silinir → Portal tekrar acar
 *
 * Yazar: Akilli Sinif Projesi
 * Tarih: 2026
 */

#include <WiFi.h>
#include <WiFiManager.h>     // tzapu/WiFiManager — portal ile kolay kurulum
#include <Preferences.h>     // NVS (Non-Volatile Storage) — kalici hafiza
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include <TFT_eSPI.h>  // TFT kutuphanesi
#include <DHT.h>       // DHT sicaklik/nem sensoru
#include "ota_manager.h"  // OTA guncelleme modulu
#include "../../../version.h"   // Firmware versiyon

// ============================================
// YAPILANDIRMA - NVS'DEN OKUNUR (hardcoded degil!)
// WiFi + diger ayarlar ilk botta WiFiManager portali
// uzerinden ayarlanir ve ESP32'nin flash'ina kaydedilir.
// ============================================

Preferences prefs;  // NVS okuma/yazma

// Runtime'da NVS'den doludurulan degiskenler
char MQTT_BROKER[40]   = "";
char CLASSROOM_ID[20]  = "sinif-1";
char MOCK_MODE_STR[6]  = "false";  // "true" / "false"
bool MOCK_MODE         = false;

// Sabit kalan degerler (degistirmen gerekirse portal'a da eklenebilir)
const int   MQTT_PORT      = 1883;
const char* MQTT_USER      = "esp32";
const char* MQTT_PASSWORD  = "akilli123";

// MQTT Client ID: sinif ID'sinden otomatik uretilir
String mqttClientId;  // "esp32-plc-sinif-1" formatinda

// Config sifirlama butonu (GPIO0 = BOOT butonu)
const int CONFIG_RESET_PIN          = 0;
const unsigned long RESET_HOLD_MS   = 5000;  // 5 saniye bas

// ============================================
// PIN TANIMLARI
// ============================================

// Sensor Pinleri (gercek donanim icin)
const int PIN_DHT = 4;           // DHT11 sicaklik/nem sensoru
const int PIN_LDR = 34;          // Isik sensoru (ADC)
const int PIN_MQ135 = 35;        // Hava kalitesi sensoru (ADC)
const int PIN_PIR = 27;          // Hareket sensoru
const int PIN_WINDOW = 26;       // Pencere sensoru (reed switch)

// DHT Sensor Ayari
#define DHTTYPE DHT11            // DHT11 kullaniyoruz (DHT22 icin DHT22 yaz)

// Aktuator Pinleri
const int PIN_LED = 13;          // LED (PWM ile dimming)
const int PIN_FAN = 12;          // Fan kontrolu (PWM)
// NOT: Buzzer yerine gorsel uyari sistemi kullaniliyor (TFT ekran + Status LED)

// Menu Butonlari (ogrencinin sayfayi degistirmesi icin)
const int PIN_BTN_NEXT = 25;     // Sonraki sayfa (aktif LOW, dahili pullup)
const int PIN_BTN_PREV = 14;     // Onceki sayfa (aktif LOW, dahili pullup)

// TFT Ekran Pinleri (ST7735 1.44")
// NOT: TFT_eSPI kutuphanesi User_Setup.h dosyasindan pin okur
// Asagidaki pinler referans icindir:
// TFT_CS   = GPIO 15
// TFT_DC   = GPIO 33  (A0/RS)
// TFT_RST  = GPIO 32
// TFT_MOSI = GPIO 23  (SDA)
// TFT_SCLK = GPIO 18  (SCK)
// TFT_BL   = 3.3V (veya GPIO ile kontrol)

// PWM Ayarlari (ESP32 Core 3.x)
// NOT: PC817 optocoupler yavas oldugu icin dusuk frekans kullaniyoruz
const int PWM_FREQ = 500;       // 1000Hz (PC817 icin uygun)
const int PWM_RESOLUTION = 8;    // 0-255 aralik

// ============================================
// GLOBAL DEGISKENLER
// ============================================

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
TFT_eSPI tft = TFT_eSPI();  // TFT nesnesi
DHT dht(PIN_DHT, DHTTYPE);  // DHT sensoru nesnesi

// OTA Yönetici (mqttClient hazir olduktan sonra ayarlanir)
OTAManager* otaManager = nullptr;
String otaStatusTopic;  // akilli-sinif/{id}/status/ota

// Sensor Degerleri
float temperature = 0;
float humidity = 0;
int lightLevel = 0;
int airQuality = 0;
bool motionDetected = false;
bool windowOpen = false;

// Kamera verisi (MQTT'den gelecek)
int personCount = 0;
unsigned long lastPersonCountUpdate = 0;

// Aktuator Durumlari
int ledBrightness = 0;           // 0-100 arasi
int fanSpeed = 0;                // 0-100 arasi

// Uyari Sistemi (Gorsel)
enum AlertLevel { ALERT_NONE, ALERT_INFO, ALERT_WARNING, ALERT_DANGER };
AlertLevel currentAlert = ALERT_NONE;
String alertMessage = "";
unsigned long alertStartTime = 0;
unsigned long lastAlertBlink = 0;
bool alertBlinkState = false;

// Zamanlama
unsigned long lastSensorRead = 0;
unsigned long lastMqttPublish = 0;
unsigned long lastStatusPublish = 0;
unsigned long lastWifiCheck = 0;
unsigned long lastDisplayUpdate = 0;

const unsigned long SENSOR_INTERVAL = 2000;      // 2 saniye
const unsigned long PUBLISH_INTERVAL = 5000;     // 5 saniye
const unsigned long STATUS_INTERVAL = 30000;     // 30 saniye
const unsigned long WIFI_CHECK_INTERVAL = 10000; // 10 saniye
const unsigned long DISPLAY_INTERVAL = 2000;     // 2 saniye

// Baglanti Durumu
bool wifiConnected = false;
bool mqttConnected = false;
int reconnectAttempts = 0;
const int MAX_RECONNECT_ATTEMPTS = 5;
unsigned long lastMqttReconnectAttempt = 0;
const unsigned long MQTT_RECONNECT_INTERVAL = 5000;  // 5 saniye bekle yeniden denemeden once

// Ekran renkleri
#define COLOR_BG        TFT_BLACK
#define COLOR_TEXT      TFT_WHITE
#define COLOR_HEADER    TFT_CYAN
#define COLOR_VALUE     TFT_GREEN
#define COLOR_LABEL     TFT_LIGHTGREY
#define COLOR_WARNING   TFT_YELLOW
#define COLOR_DANGER    TFT_RED
#define COLOR_OK        TFT_GREEN
#define COLOR_EMPTY     TFT_RED
#define COLOR_OCCUPIED  TFT_GREEN
#define COLOR_ALERT_BG  0xFBE0    // Acik sari arka plan

// Uyari LED zamanlama (ms)
#define LED_BLINK_NORMAL   1000   // Normal: yavas yanip sonme
#define LED_BLINK_WARNING  300    // Uyari: orta hizda
#define LED_BLINK_DANGER   100    // Tehlike: hizli

// Ekran onceki degerleri (flicker onleme)
float prevTemperature = -999;
float prevHumidity = -999;
int prevAirQuality = -999;
int prevPersonCount = -999;
bool prevMqttConnected = false;
AlertLevel prevAlert = ALERT_NONE;
int prevMinute = -1;
int prevDay    = -1;

// ============================================
// MENU / SAYFA SISTEMI
// ============================================

enum Page {
  PAGE_SENSORS,    // Ana ekran: saat/tarih + kisi + sensor
  PAGE_NOW,        // Simdiki ders (ogretmen, konu, ilerleme)
  PAGE_WEEK,       // Haftalik plan (5 gun grid)
  PAGE_ANNOUNCE,   // Duyurular
  PAGE_COUNT
};

Page currentPage = PAGE_SENSORS;
uint32_t lastPageChangeMs = 0;
bool pageDirty = true;          // sayfa degisti, tam yeniden ciz
bool rotationPaused = false;    // uzun basis ile duraklatma

const uint32_t AUTO_ROTATE_MS       = 6000;   // sayfa cevrim hizi
const uint32_t BUTTON_DEBOUNCE_MS   = 50;
const uint32_t BUTTON_LONG_PRESS_MS = 3000;

struct ButtonState {
  int pin;
  bool lastRaw;        // son ham okuma (HIGH = bosta pullup)
  bool stable;         // debounce sonrasi kararli durum
  uint32_t lastChangeMs;
  uint32_t pressStartMs;
  bool pressHandled;   // uzun/kisa basisi tek sefer isle
};

ButtonState btnNext = {PIN_BTN_NEXT, HIGH, HIGH, 0, 0, true};
ButtonState btnPrev = {PIN_BTN_PREV, HIGH, HIGH, 0, 0, true};

// ============================================
// DERS PROGRAMI / DUYURU VERILERI (MQTT'den gelir)
// ============================================

struct CurrentLesson {
  String subject;
  String teacher;
  String startHHMM;
  String endHHMM;
  bool valid;
};

struct TodaySlot {
  String code;
  String teacher;
  String startHHMM;
  String endHHMM;
};

static const int MAX_TODAY_SLOTS = 10;
struct TodayPlan {
  TodaySlot slots[MAX_TODAY_SLOTS];
  int count;
  bool valid;
};

static const int MAX_DAYS       = 5;
static const int MAX_WEEK_SLOTS = 8;
struct WeekDay {
  String day;
  String codes[MAX_WEEK_SLOTS];
  int slotCount;
};
struct WeekPlan {
  WeekDay days[MAX_DAYS];
  int dayCount;
  bool valid;
};

struct Announcement {
  String title;
  String body;
  String severity;  // info / warning / danger
  bool valid;
};

CurrentLesson currentLesson = {"", "", "", "", false};
TodayPlan     todayPlan     = {};
WeekPlan      weekPlan      = {};
Announcement  announcement  = {"", "", "info", false};

// ============================================
// TOPIC YARDIMCI FONKSIYONLARI
// ============================================

// Topic olusturma fonksiyonu
String buildTopic(const char* type, const char* name) {
  // Ornek: akilli-sinif/sinif-1/sensors/temperature
  return String("akilli-sinif/") + CLASSROOM_ID + "/" + type + "/" + name;
}

// ============================================
// TFT EKRAN FONKSIYONLARI
// ============================================

void setupDisplay() {
  tft.init();
  tft.setRotation(1);  // Yatay mod (128x128 -> yatay)
  tft.fillScreen(COLOR_BG);
  
  // Baslangic ekrani
  tft.setTextColor(COLOR_HEADER, COLOR_BG);
  tft.setTextSize(1);
  tft.setCursor(10, 20);
  tft.println("AKILLI SINIF");
  tft.setCursor(10, 40);
  tft.println("SISTEMI");
  tft.setTextColor(COLOR_LABEL, COLOR_BG);
  tft.setCursor(10, 70);
  tft.println("Baslatiliyor...");
  
  Serial.println("TFT Ekran baslatildi");
}

// ============================================
// SAYFA DISPATCHER + ORTAK YARDIMCILAR
// ============================================

void drawHeader(const char* title, uint16_t bg) {
  tft.setTextSize(1);
  tft.setTextColor(COLOR_HEADER, bg);
  tft.setCursor(5, 5);
  tft.print(title);

  // Ust cizgi
  tft.drawLine(0, 18, 128, 18, COLOR_HEADER);

  // MQTT durum ikonu (sag ust)
  tft.fillCircle(120, 8, 4, mqttConnected ? COLOR_OK : COLOR_DANGER);

  // Duraklatma gostergesi
  if (rotationPaused) {
    tft.setTextColor(COLOR_WARNING, bg);
    tft.setCursor(100, 5);
    tft.print("P");
  }

  // Uyari var ise baslikta "!" goster
  if (currentAlert == ALERT_WARNING) {
    tft.setTextColor(COLOR_WARNING, bg);
    tft.setCursor(85, 5);
    tft.print("!");
  }
}

// Metni kelime sinirinda satirlara bolup yazdirir. Donus: yazilan satir sayisi.
int wrapText(const String& text, int x, int y, int cols,
             uint16_t bg, uint16_t fg, int maxLines) {
  tft.setTextSize(1);
  tft.setTextColor(fg, bg);
  const int lineH = 10;
  int cursor = 0;
  int row = 0;
  const int len = (int)text.length();
  while (cursor < len && row < maxLines) {
    int end = cursor + cols;
    if (end >= len) {
      end = len;
    } else {
      int space = text.lastIndexOf(' ', end);
      if (space > cursor) end = space;
    }
    tft.setCursor(x, y + row * lineH);
    tft.print(text.substring(cursor, end));
    cursor = end;
    if (cursor < len && text[cursor] == ' ') cursor++;
    row++;
  }
  return row;
}

// ---- Sayfa: ANA EKRAN (saat/tarih + kisi + sensor) ----
void renderSensors(bool full) {
  uint16_t bgColor = (currentAlert == ALERT_WARNING) ? COLOR_ALERT_BG : COLOR_BG;

  if (full) {
    tft.fillScreen(bgColor);
    drawHeader(CLASSROOM_ID, bgColor);

    // Saat/tarih bolumu alt ayirici
    tft.drawLine(0, 46, 128, 46, COLOR_LABEL);

    // Kisi sayisi etiketi
    tft.setTextSize(1);
    tft.setTextColor(COLOR_LABEL, bgColor);
    tft.setCursor(5, 50);
    tft.print("KISI SAYISI");

    // Sensor bolumu ayirici
    tft.drawLine(0, 91, 128, 91, COLOR_HEADER);

    // Sensor etiketleri
    tft.setTextColor(COLOR_LABEL, bgColor);
    tft.setCursor(5, 94);  tft.print("Sicaklik:");
    tft.setCursor(5, 105); tft.print("Nem:");
    tft.setCursor(5, 116); tft.print("Hava:");

    // Flicker-onleme icin sifirla
    prevTemperature   = -999;
    prevHumidity      = -999;
    prevAirQuality    = -999;
    prevPersonCount   = -999;
    prevMqttConnected = !mqttConnected;
    prevMinute        = -1;
    prevDay           = -1;
  }

  // MQTT durum ikonu
  if (mqttConnected != prevMqttConnected) {
    tft.fillCircle(120, 8, 5, bgColor);
    tft.fillCircle(120, 8, 4, mqttConnected ? COLOR_OK : COLOR_DANGER);
    prevMqttConnected = mqttConnected;
  }

  // Saat / gun / tarih (partial update)
  struct tm t;
  bool haveTime = getLocalTime(&t, 100);
  if (haveTime) {
    if (t.tm_min != prevMinute) {
      char timeStr[6];
      strftime(timeStr, sizeof(timeStr), "%H:%M", &t);
      tft.fillRect(0, 20, 84, 17, bgColor);
      tft.setTextSize(2);
      tft.setTextColor(COLOR_VALUE, bgColor);
      tft.setCursor(5, 21);
      tft.print(timeStr);

      const char* dayNames[7] = {"Paz","Pzt","Sal","Car","Per","Cum","Cmt"};
      tft.fillRect(84, 20, 44, 17, bgColor);
      tft.setTextSize(1);
      tft.setTextColor(COLOR_LABEL, bgColor);
      tft.setCursor(90, 26);
      tft.print(dayNames[t.tm_wday]);

      prevMinute = t.tm_min;
    }

    if (t.tm_wday != prevDay) {
      char dateStr[12];
      snprintf(dateStr, sizeof(dateStr), "%02d/%02d/%04d",
               t.tm_mday, t.tm_mon + 1, t.tm_year + 1900);
      tft.fillRect(0, 37, 128, 9, bgColor);
      tft.setTextSize(1);
      tft.setTextColor(COLOR_LABEL, bgColor);
      tft.setCursor(34, 38);
      tft.print(dateStr);
      prevDay = t.tm_wday;
    }
  } else if (prevMinute != -2) {
    tft.fillRect(0, 20, 128, 26, bgColor);
    tft.setTextSize(1);
    tft.setTextColor(COLOR_LABEL, bgColor);
    tft.setCursor(5, 30);
    tft.print("NTP bekleniyor...");
    prevMinute = -2;
  }

  // Kisi sayisi
  if (personCount != prevPersonCount) {
    tft.setTextSize(3);
    tft.setTextColor(personCount == 0 ? COLOR_EMPTY : COLOR_OCCUPIED, bgColor);
    tft.setCursor(46, 58);
    char countStr[4];
    snprintf(countStr, sizeof(countStr), "%2d", personCount);
    tft.print(countStr);

    tft.setTextSize(1);
    tft.setCursor(5, 83);
    if (personCount == 0) {
      tft.setTextColor(COLOR_EMPTY, bgColor);
      tft.print("SINIF BOS  ");
    } else {
      tft.setTextColor(COLOR_OCCUPIED, bgColor);
      tft.print("SINIF DOLU ");
    }
    prevPersonCount = personCount;
  }

  // Sicaklik
  if (abs(temperature - prevTemperature) > 0.05) {
    tft.setTextSize(1);
    tft.setTextColor(getTemperatureColor(temperature), bgColor);
    tft.setCursor(60, 94);
    char tempStr[10];
    snprintf(tempStr, sizeof(tempStr), "%5.1fC ", temperature);
    tft.print(tempStr);
    prevTemperature = temperature;
  }

  // Nem
  if (abs(humidity - prevHumidity) > 0.5) {
    tft.setTextSize(1);
    tft.setTextColor(COLOR_VALUE, bgColor);
    tft.setCursor(60, 105);
    char humStr[10];
    snprintf(humStr, sizeof(humStr), "%3.0f%%  ", humidity);
    tft.print(humStr);
    prevHumidity = humidity;
  }

  // Hava kalitesi
  if (airQuality != prevAirQuality) {
    tft.setTextSize(1);
    tft.setTextColor(getAirQualityColor(airQuality), bgColor);
    tft.setCursor(60, 116);
    char aqStr[12];
    snprintf(aqStr, sizeof(aqStr), "%4dppm ", airQuality);
    tft.print(aqStr);
    prevAirQuality = airQuality;
  }
}

// ---- Yardimci: HH:MM stringinden dakika hesapla ----
int hhmmToMinutes(const String& s) {
  if (s.length() < 5) return -1;
  int h = s.substring(0, 2).toInt();
  int m = s.substring(3, 5).toInt();
  return h * 60 + m;
}


// ---- Sayfa: SIMDIKI DERS ----
void renderNow(bool full) {
  if (!full) return;

  tft.fillScreen(COLOR_BG);
  drawHeader("DERS", COLOR_BG);

  if (!currentLesson.valid) {
    tft.setTextSize(1);
    tft.setTextColor(COLOR_LABEL, COLOR_BG);
    tft.setCursor(5, 50);
    tft.print("Ders bilgisi yok");
    return;
  }

  // Konu (buyuk)
  tft.setTextSize(2);
  tft.setTextColor(COLOR_HEADER, COLOR_BG);
  tft.setCursor(5, 24);
  tft.print(currentLesson.subject.substring(0, 10));

  // Ogretmen
  tft.setTextSize(1);
  tft.setTextColor(COLOR_TEXT, COLOR_BG);
  tft.setCursor(5, 48);
  tft.print(currentLesson.teacher.substring(0, 21));

  // Saat araligi
  tft.setTextColor(COLOR_LABEL, COLOR_BG);
  tft.setCursor(5, 62);
  tft.print(currentLesson.startHHMM);
  tft.print(" - ");
  tft.print(currentLesson.endHHMM);

  // Ilerleme cubugu
  struct tm t;
  float pct = 0.0f;
  if (getLocalTime(&t, 100)) {
    int startMins = hhmmToMinutes(currentLesson.startHHMM);
    int endMins   = hhmmToMinutes(currentLesson.endHHMM);
    int nowMins   = t.tm_hour * 60 + t.tm_min;
    if (startMins >= 0 && endMins > startMins) {
      pct = (float)(nowMins - startMins) / (float)(endMins - startMins);
      if (pct < 0) pct = 0;
      if (pct > 1) pct = 1;
    }
  }
  const int barX = 5, barY = 82, barW = 112, barH = 10;
  tft.drawRect(barX, barY, barW, barH, COLOR_HEADER);
  tft.fillRect(barX + 1, barY + 1, (int)((barW - 2) * pct), barH - 2, COLOR_VALUE);

  tft.setTextColor(COLOR_VALUE, COLOR_BG);
  tft.setCursor(5, 98);
  char pctStr[8];
  snprintf(pctStr, sizeof(pctStr), "%d%%", (int)(pct * 100));
  tft.print(pctStr);
}

// ---- Sayfa: HAFTALIK PLAN (5 gun grid) ----
void renderWeek(bool full) {
  if (!full) return;

  tft.fillScreen(COLOR_BG);
  drawHeader("HAFTA", COLOR_BG);

  if (!weekPlan.valid) {
    tft.setTextSize(1);
    tft.setTextColor(COLOR_LABEL, COLOR_BG);
    tft.setCursor(5, 50);
    tft.print("Plan yok");
    return;
  }

  // Bugunun indeksini bul (0=Pzt ... 4=Cum)
  int todayIdx = -1;
  struct tm t;
  if (getLocalTime(&t, 100)) {
    if (t.tm_wday >= 1 && t.tm_wday <= 5) todayIdx = t.tm_wday - 1;
  }

  const int cols = 5;
  const int colW = 25;
  const int rowH = 10;
  const int startY = 22;

  // Gun basliklari
  tft.setTextSize(1);
  for (int d = 0; d < weekPlan.dayCount && d < cols; d++) {
    int x = d * colW + 3;
    if (d == todayIdx) {
      tft.fillRect(d * colW, startY - 1, colW, rowH, COLOR_HEADER);
      tft.setTextColor(COLOR_BG, COLOR_HEADER);
    } else {
      tft.setTextColor(COLOR_LABEL, COLOR_BG);
    }
    tft.setCursor(x, startY);
    tft.print(weekPlan.days[d].day.substring(0, 3));
  }

  // Maksimum slot sayisini bul
  int maxSlots = 0;
  for (int d = 0; d < weekPlan.dayCount; d++) {
    if (weekPlan.days[d].slotCount > maxSlots) maxSlots = weekPlan.days[d].slotCount;
  }
  if (maxSlots > MAX_WEEK_SLOTS) maxSlots = MAX_WEEK_SLOTS;

  // Kod satirlari
  for (int s = 0; s < maxSlots; s++) {
    int y = startY + rowH + 2 + s * rowH;
    for (int d = 0; d < weekPlan.dayCount && d < cols; d++) {
      int x = d * colW + 3;
      tft.setCursor(x, y);
      tft.setTextColor(d == todayIdx ? COLOR_VALUE : COLOR_TEXT, COLOR_BG);
      if (s < weekPlan.days[d].slotCount) {
        tft.print(weekPlan.days[d].codes[s].substring(0, 3));
      }
    }
  }
}

// ---- Sayfa: DUYURULAR ----
void renderAnnounce(bool full) {
  if (!full) return;

  tft.fillScreen(COLOR_BG);
  drawHeader("DUYURU", COLOR_BG);

  if (!announcement.valid) {
    tft.setTextSize(1);
    tft.setTextColor(COLOR_LABEL, COLOR_BG);
    tft.setCursor(5, 50);
    tft.print("Duyuru yok");
    return;
  }

  // Baslik rengi severity'e gore
  uint16_t titleColor = COLOR_VALUE;
  if      (announcement.severity == "warning") titleColor = COLOR_WARNING;
  else if (announcement.severity == "danger")  titleColor = COLOR_DANGER;

  // Baslik (max 2 satir)
  int titleLines = wrapText(announcement.title, 5, 24, 21,
                             COLOR_BG, titleColor, 2);

  // Ayirici
  int bodyY = 24 + titleLines * 10 + 4;
  tft.drawLine(0, bodyY - 2, 128, bodyY - 2, COLOR_HEADER);

  // Govde
  wrapText(announcement.body, 5, bodyY, 21, COLOR_BG, COLOR_TEXT, 8);
}

// ---- Ana dispatcher ----
void updateDisplay() {
  // Tehlike: tum ekrani kaplar, sayfalari gecici olarak gizle
  if (currentAlert == ALERT_DANGER) {
    if (alertBlinkState) {
      tft.fillScreen(COLOR_DANGER);
      tft.setTextColor(COLOR_TEXT, COLOR_DANGER);
      tft.setTextSize(2);
      tft.setCursor(15, 30);
      tft.println("UYARI!");
      tft.setTextSize(1);
      tft.setCursor(10, 60);
      tft.println(alertMessage);
    } else {
      pageDirty = true;  // tehlike bitince sayfayi yeniden ciz
    }
    prevAlert = currentAlert;
    return;
  }

  if (currentAlert != prevAlert) {
    pageDirty = true;
    prevAlert = currentAlert;
  }

  switch (currentPage) {
    case PAGE_SENSORS:  renderSensors(pageDirty);  break;
    case PAGE_NOW:      renderNow(pageDirty);      break;
    case PAGE_WEEK:     renderWeek(pageDirty);     break;
    case PAGE_ANNOUNCE: renderAnnounce(pageDirty); break;
    default: break;
  }
  pageDirty = false;
}

// ============================================
// MENU NAVIGASYON (buton + otomatik cevrim)
// ============================================

void changePage(int delta) {
  int next = ((int)currentPage + delta + (int)PAGE_COUNT) % (int)PAGE_COUNT;
  currentPage = (Page)next;
  lastPageChangeMs = millis();
  pageDirty = true;
  updateDisplay();  // butona basildiginda anlik tepki
}

void pollButton(ButtonState& b, int direction, bool allowLongPress) {
  bool raw = digitalRead(b.pin);
  uint32_t now = millis();

  if (raw != b.lastRaw) {
    b.lastChangeMs = now;
    b.lastRaw = raw;
  }

  if ((now - b.lastChangeMs) > BUTTON_DEBOUNCE_MS && raw != b.stable) {
    b.stable = raw;
    if (b.stable == LOW) {
      // Basildi
      b.pressStartMs = now;
      b.pressHandled = false;
    } else {
      // Birakildi
      if (!b.pressHandled) {
        uint32_t held = now - b.pressStartMs;
        if (held < BUTTON_LONG_PRESS_MS) {
          changePage(direction);
        }
        b.pressHandled = true;
      }
    }
  }

  // Uzun basis (sadece NEXT butonunda aktif)
  if (allowLongPress && b.stable == LOW && !b.pressHandled) {
    if ((now - b.pressStartMs) >= BUTTON_LONG_PRESS_MS) {
      rotationPaused = !rotationPaused;
      pageDirty = true;
      updateDisplay();
      b.pressHandled = true;
    }
  }
}

void pollButtons() {
  pollButton(btnNext, +1, true);
  pollButton(btnPrev, -1, false);
}

void handleAutoRotate() {
  if (rotationPaused) return;
  if ((millis() - lastPageChangeMs) >= AUTO_ROTATE_MS) {
    changePage(+1);
  }
}

void setupButtons() {
  pinMode(PIN_BTN_NEXT, INPUT_PULLUP);
  pinMode(PIN_BTN_PREV, INPUT_PULLUP);
  btnNext.lastRaw = btnNext.stable = digitalRead(PIN_BTN_NEXT);
  btnPrev.lastRaw = btnPrev.stable = digitalRead(PIN_BTN_PREV);
  Serial.println("[Menu] Butonlar hazir: NEXT=GPIO25, PREV=GPIO14");
}

// ============================================
// NTP — Turkiye saati (UTC+3, DST yok)
// ============================================

void setupNTP() {
  configTime(0, 0, "pool.ntp.org", "time.google.com", "time.cloudflare.com");
  setenv("TZ", "TRT-3", 1);
  tzset();
  Serial.println("[NTP] Zaman senkronizasyonu baslatildi (TZ=TRT-3)");
}

uint16_t getTemperatureColor(float temp) {
  if (temp < 18) return TFT_BLUE;
  if (temp < 22) return TFT_CYAN;
  if (temp < 26) return COLOR_OK;
  if (temp < 30) return COLOR_WARNING;
  return COLOR_DANGER;
}

uint16_t getAirQualityColor(int aqi) {
  if (aqi < 100) return COLOR_OK;
  if (aqi < 200) return COLOR_WARNING;
  return COLOR_DANGER;
}

// ============================================
// GORSEL UYARI FONKSIYONLARI
// ============================================

void setAlert(AlertLevel level, String message) {
  currentAlert = level;
  alertMessage = message;
  alertStartTime = millis();
  alertBlinkState = true;
  pageDirty = true;  // Ekrani yeniden ciz
  
  Serial.print("Uyari ayarlandi - Seviye: ");
  Serial.print(level);
  Serial.print(", Mesaj: ");
  Serial.println(message);
}

void clearAlert() {
  currentAlert = ALERT_NONE;
  alertMessage = "";
  alertStartTime = 0;
  pageDirty = true;  // Ekrani yeniden ciz
}

void updateAlertLED() {
  // Uyari seviyesine gore LED yanip sonme hizi
  unsigned long blinkInterval;
  
  switch (currentAlert) {
    case ALERT_DANGER:
      blinkInterval = LED_BLINK_DANGER;
      break;
    case ALERT_WARNING:
      blinkInterval = LED_BLINK_WARNING;
      break;
    case ALERT_INFO:
    case ALERT_NONE:
    default:
      blinkInterval = LED_BLINK_NORMAL;
      break;
  }
  
  // Yanip sonme zamani geldi mi?
  if (millis() - lastAlertBlink >= blinkInterval) {
    lastAlertBlink = millis();
    alertBlinkState = !alertBlinkState;
  }
}

void checkAutoAlerts() {
  // Otomatik uyari kontrolleri
  // Hava kalitesi icin tam ekran uyari YAPILMIYOR - sadece ppm degeri renk ile gosterilir (getAirQualityColor)
  // Sicaklik cok yuksek
  if (temperature > 32 && currentAlert != ALERT_DANGER) {
    setAlert(ALERT_DANGER, "SICAKLIK COK!");
  }
  // Sicaklik yuksek
  else if (temperature > 28 && temperature <= 32 && currentAlert != ALERT_WARNING && currentAlert != ALERT_DANGER) {
    setAlert(ALERT_WARNING, "Sicaklik Yuksek");
  }
  // Normal duruma don
  else if (temperature < 27 && currentAlert != ALERT_NONE) {
    clearAlert();
  }
}

// ============================================
// CONFIG YÖNETİMİ (NVS)
// ============================================

// NVS'den kayitli config'i oku
void loadConfig() {
  prefs.begin("akilli-sinif", true);  // read-only
  String broker  = prefs.getString("mqtt_broker", "");
  String sinifId = prefs.getString("classroom_id", "sinif-1");
  String mockStr = prefs.getString("mock_mode", "false");
  prefs.end();

  broker.toCharArray(MQTT_BROKER,   sizeof(MQTT_BROKER));
  sinifId.toCharArray(CLASSROOM_ID, sizeof(CLASSROOM_ID));
  mockStr.toCharArray(MOCK_MODE_STR, sizeof(MOCK_MODE_STR));

  MOCK_MODE = (mockStr == "true");

  // MQTT Client ID: sinif ID'sinden otomatik uret
  mqttClientId = "esp32-plc-" + sinifId;

  // OTA status topic'i guncelle
  otaStatusTopic = "akilli-sinif/" + sinifId + "/status/ota";

  Serial.println("[Config] Sinif ID: " + sinifId);
  Serial.println("[Config] MQTT Broker: " + broker);
  Serial.println("[Config] Mock Modu: " + mockStr);
  Serial.println("[Config] Firmware: v" + OTAManager::getCurrentVersion());
}

// Config sifirlama butonu kontrolu
// GPIO0 (BOOT) 5 saniye basili tutulursa NVS silinir
void checkConfigReset() {
  pinMode(CONFIG_RESET_PIN, INPUT_PULLUP);
  if (digitalRead(CONFIG_RESET_PIN) == LOW) {
    unsigned long pressStart = millis();
    Serial.println("[Config] Reset butonu algilandi, 5 sn basili tut...");

    // TFT'de geri sayim goster
    tft.fillScreen(COLOR_BG);
    tft.setTextColor(COLOR_WARNING, COLOR_BG);
    tft.setTextSize(1);
    tft.setCursor(5, 20);
    tft.println("Sifirlama icin");
    tft.setCursor(5, 35);
    tft.println("5 sn bas...");

    while (digitalRead(CONFIG_RESET_PIN) == LOW) {
      unsigned long held = millis() - pressStart;
      if (held >= RESET_HOLD_MS) {
        // Config'i sil
        prefs.begin("akilli-sinif", false);
        prefs.clear();
        prefs.end();

        // WiFi kimlik bilgilerini de sil - portal acilmak zorunda kalsin
        WiFi.disconnect(true, true);

        Serial.println("[Config] NVS + WiFi silindi! WiFiManager portali acilacak...");
        tft.fillScreen(COLOR_BG);
        tft.setTextColor(COLOR_DANGER, COLOR_BG);
        tft.setCursor(5, 30);
        tft.println("Config silindi!");
        tft.setCursor(5, 50);
        tft.println("Yeniden");
        tft.setCursor(5, 65);
        tft.println("baslatiliyor...");
        delay(2000);
        ESP.restart();
      }
      // Geri sayim goster
      int remaining = (RESET_HOLD_MS - held) / 1000 + 1;
      tft.fillRect(5, 50, 60, 15, COLOR_BG);
      tft.setCursor(5, 50);
      tft.setTextColor(COLOR_DANGER, COLOR_BG);
      tft.print(remaining);
      tft.print(" sn...");
      delay(100);
    }
  }
}

// ============================================
// WIFI FONKSIYONLARI (WiFiManager)
// ============================================

void setupWiFi() {
  Serial.println("\n========================================");
  Serial.println("WiFiManager Baslatiliyor...");
  Serial.println("========================================");

  // TFT'de goster
  tft.fillScreen(COLOR_BG);
  tft.setTextColor(COLOR_HEADER, COLOR_BG);
  tft.setTextSize(1);
  tft.setCursor(5, 20);
  tft.println("WiFi Kurulum");
  tft.setTextColor(COLOR_LABEL, COLOR_BG);
  tft.setCursor(5, 38);
  tft.println("Akilli-Sinif");
  tft.setCursor(5, 52);
  tft.println("-Setup agina");
  tft.setCursor(5, 66);
  tft.println("baglan");

  WiFiManager wm;

  // Portal'da ekstra alanlar (WiFi den ayri config)
  WiFiManagerParameter p_mqttBroker("mqtt_broker", "MQTT Broker IP",
                                     MQTT_BROKER, 39);
  WiFiManagerParameter p_classroomId("classroom_id", "Sinif ID (sinif-1, sinif-2...)",
                                      CLASSROOM_ID, 19);
  WiFiManagerParameter p_mockMode("mock_mode", "Mock Modu (true/false)",
                                   MOCK_MODE_STR, 5);

  wm.addParameter(&p_mqttBroker);
  wm.addParameter(&p_classroomId);
  wm.addParameter(&p_mockMode);

  // Portal AP adi: "Akilli-Sinif-Setup"
  // Baglandiktan sonra 192.168.4.1 otomatik acar
  wm.setConfigPortalTimeout(180);  // 3 dakika icinde baglanilmazsa devam et

  // Config sifirlandiysa (broker IP bos) veya BOOT butonu basili ise portal ac
  bool forcePortal = (strlen(MQTT_BROKER) == 0) || (digitalRead(CONFIG_RESET_PIN) == LOW);

  bool connected;
  if (forcePortal) {
    Serial.println("[WiFi] Portal modu - baglanmadan once config gerekli.");
    connected = wm.startConfigPortal("Akilli-Sinif-Setup");
  } else {
    connected = wm.autoConnect("Akilli-Sinif-Setup");
  }

  if (!connected) {
    Serial.println("[WiFi] Baglanti saglanamadi, yeniden baslatiliyor...");
    tft.fillScreen(COLOR_BG);
    tft.setTextColor(COLOR_DANGER, COLOR_BG);
    tft.setCursor(5, 30);
    tft.println("WiFi HATASI!");
    tft.setCursor(5, 50);
    tft.println("Yeniden");
    tft.setCursor(5, 65);
    tft.println("baslatiliyor...");
    delay(3000);
    ESP.restart();
    return;
  }

  // WiFi baglandi — portal'dan gelen config degerlerini NVS'e kaydet
  String newBroker  = String(p_mqttBroker.getValue());
  String newSinifId = String(p_classroomId.getValue());
  String newMock    = String(p_mockMode.getValue());

  // Bos gelmisse mevcut degerleri koru
  if (newBroker.length() > 0 || newSinifId.length() > 0) {
    prefs.begin("akilli-sinif", false);
    if (newBroker.length()  > 0) prefs.putString("mqtt_broker",   newBroker);
    if (newSinifId.length() > 0) prefs.putString("classroom_id",  newSinifId);
    if (newMock.length()    > 0) prefs.putString("mock_mode",      newMock);
    prefs.end();

    // Degiskenleri guncelle
    newBroker.toCharArray(MQTT_BROKER,   sizeof(MQTT_BROKER));
    newSinifId.toCharArray(CLASSROOM_ID, sizeof(CLASSROOM_ID));
    newMock.toCharArray(MOCK_MODE_STR,   sizeof(MOCK_MODE_STR));
    MOCK_MODE = (newMock == "true");
    mqttClientId  = "esp32-plc-" + newSinifId;
    otaStatusTopic = "akilli-sinif/" + newSinifId + "/status/ota";
  }

  wifiConnected = true;
  Serial.println("[WiFi] BAGLANDI!");
  Serial.print("[WiFi] IP: ");
  Serial.println(WiFi.localIP());

  // TFT'de goster
  tft.fillScreen(COLOR_BG);
  tft.setTextColor(COLOR_OK, COLOR_BG);
  tft.setCursor(5, 20);
  tft.println("WiFi OK!");
  tft.setTextColor(COLOR_TEXT, COLOR_BG);
  tft.setCursor(5, 38);
  tft.println(WiFi.localIP().toString());
  tft.setTextColor(COLOR_LABEL, COLOR_BG);
  tft.setCursor(5, 55);
  tft.print("Sinif: ");
  tft.println(CLASSROOM_ID);
  delay(1500);
}

void checkWiFi() {
  if (WiFi.status() != WL_CONNECTED) {
    wifiConnected = false;
    Serial.println("[WiFi] Baglanti koptu! Yeniden baglaniliyor...");
    // WiFiManager mevcut kimlik bilgileri ile otomatik yeniden baglantı kurar
    WiFi.reconnect();
    delay(5000);
    if (WiFi.status() == WL_CONNECTED) {
      wifiConnected = true;
      Serial.println("[WiFi] Yeniden baglandi.");
    }
  }
}

// ============================================
// MQTT FONKSIYONLARI
// ============================================

// MQTT mesaj callback fonksiyonu
// Bu fonksiyon, subscribe olunan topic'lere mesaj geldiginde cagrilir
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  // Gelen mesaji string'e cevir
  String message = "";
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  
  Serial.println("\n>>> MQTT MESAJ ALINDI <<<");
  Serial.print("Topic: ");
  Serial.println(topic);
  Serial.print("Mesaj: ");
  Serial.println(message);

  // JSON olarak parse et (1 KB — haftalik plan icin buyuk)
  DynamicJsonDocument doc(1024);
  DeserializationError error = deserializeJson(doc, message);

  if (error) {
    Serial.print("JSON Parse Hatasi: ");
    Serial.println(error.c_str());
    return;
  }
  
  // Topic'e gore islem yap
  String topicStr = String(topic);
  
  // Kamera/Kisi Sayisi Verisi
  // Topic: akilli-sinif/sinif-1/sensors/camera
  if (topicStr.endsWith("/sensors/camera")) {
    if (doc.containsKey("person_count")) {
      personCount = doc["person_count"];
      lastPersonCountUpdate = millis();
      Serial.print("Kisi sayisi guncellendi: ");
      Serial.println(personCount);
    }
  }
  
  // LED Kontrolu
  // Topic: akilli-sinif/sinif-1/control/led
  else if (topicStr.endsWith("/control/led")) {
    if (doc.containsKey("brightness")) {
      int brightness = doc["brightness"];
      setLedBrightness(brightness);
      Serial.print("LED Parlakligi ayarlandi: ");
      Serial.print(brightness);
      Serial.println("%");
    }
    if (doc.containsKey("state")) {
      const char* state = doc["state"];
      if (strcmp(state, "on") == 0) {
        setLedBrightness(100);
        Serial.println("LED ACILDI");
      } else if (strcmp(state, "off") == 0) {
        setLedBrightness(0);
        Serial.println("LED KAPATILDI");
      }
    }
  }
  
  // Fan Kontrolu
  // Topic: akilli-sinif/sinif-1/control/fan
  else if (topicStr.endsWith("/control/fan")) {
    if (doc.containsKey("speed")) {
      int speed = doc["speed"];
      setFanSpeed(speed);
      Serial.print("Fan Hizi ayarlandi: ");
      Serial.print(speed);
      Serial.println("%");
    }
    if (doc.containsKey("state")) {
      const char* state = doc["state"];
      if (strcmp(state, "on") == 0) {
        setFanSpeed(50);
        Serial.println("Fan ACILDI");
      } else if (strcmp(state, "off") == 0) {
        setFanSpeed(0);
        Serial.println("Fan KAPATILDI");
      }
    }
  }
  
  // Gorsel Uyari Kontrolu
  // Topic: akilli-sinif/sinif-1/control/alert
  else if (topicStr.endsWith("/control/alert")) {
    if (doc.containsKey("level")) {
      const char* level = doc["level"];
      if (strcmp(level, "none") == 0) {
        clearAlert();
        Serial.println("Uyari KALDIRILDI");
      } else if (strcmp(level, "info") == 0) {
        String msg = doc.containsKey("message") ? doc["message"].as<String>() : "Bilgi";
        setAlert(ALERT_INFO, msg);
        Serial.println("INFO uyarisi ayarlandi");
      } else if (strcmp(level, "warning") == 0) {
        String msg = doc.containsKey("message") ? doc["message"].as<String>() : "Uyari!";
        setAlert(ALERT_WARNING, msg);
        Serial.println("WARNING uyarisi ayarlandi");
      } else if (strcmp(level, "danger") == 0) {
        String msg = doc.containsKey("message") ? doc["message"].as<String>() : "TEHLIKE!";
        setAlert(ALERT_DANGER, msg);
        Serial.println("DANGER uyarisi ayarlandi");
      }
    }
  }
  
  // OTA Guncelleme Komutu
  // Topic: akilli-sinif/sinif-1/control/ota  VEYA  akilli-sinif/all/control/ota
  else if (topicStr.endsWith("/control/ota")) {
    Serial.println("[OTA] Guncelleme komutu alindi!");
    if (otaManager != nullptr) {
      otaManager->handleCommand(message);
    } else {
      Serial.println("[OTA] OTA manager hazir degil!");
    }
  }

  // ======== DERS PROGRAMI + DUYURU ========
  // Simdiki ders — Topic: akilli-sinif/{id}/schedule/current
  else if (topicStr.endsWith("/schedule/current")) {
    currentLesson.subject   = String((const char*)(doc["subject"] | ""));
    currentLesson.teacher   = String((const char*)(doc["teacher"] | ""));
    currentLesson.startHHMM = String((const char*)(doc["start"]   | ""));
    currentLesson.endHHMM   = String((const char*)(doc["end"]     | ""));
    currentLesson.valid = currentLesson.subject.length() > 0;
    if (currentPage == PAGE_NOW) pageDirty = true;
    Serial.println("[Schedule] current guncellendi: " + currentLesson.subject);
  }

  // Bugunun dersleri — Topic: akilli-sinif/{id}/schedule/today
  else if (topicStr.endsWith("/schedule/today")) {
    JsonArray arr = doc.as<JsonArray>();
    todayPlan.count = 0;
    for (JsonObject slot : arr) {
      if (todayPlan.count >= MAX_TODAY_SLOTS) break;
      todayPlan.slots[todayPlan.count].code      = String((const char*)(slot["code"]    | slot["subject"] | ""));
      todayPlan.slots[todayPlan.count].teacher   = String((const char*)(slot["teacher"] | ""));
      todayPlan.slots[todayPlan.count].startHHMM = String((const char*)(slot["start"]   | ""));
      todayPlan.slots[todayPlan.count].endHHMM   = String((const char*)(slot["end"]     | ""));
      todayPlan.count++;
    }
    todayPlan.valid = todayPlan.count > 0;
    Serial.print("[Schedule] today: ");
    Serial.print(todayPlan.count);
    Serial.println(" ders");
  }

  // Haftalik plan — Topic: akilli-sinif/{id}/schedule/week
  else if (topicStr.endsWith("/schedule/week")) {
    JsonArray days = doc.as<JsonArray>();
    weekPlan.dayCount = 0;
    for (JsonObject dayObj : days) {
      if (weekPlan.dayCount >= MAX_DAYS) break;
      WeekDay& wd = weekPlan.days[weekPlan.dayCount];
      wd.day = String((const char*)(dayObj["day"] | ""));
      wd.slotCount = 0;
      for (JsonObject slot : dayObj["slots"].as<JsonArray>()) {
        if (wd.slotCount >= MAX_WEEK_SLOTS) break;
        wd.codes[wd.slotCount] = String((const char*)(slot["code"] | ""));
        wd.slotCount++;
      }
      weekPlan.dayCount++;
    }
    weekPlan.valid = weekPlan.dayCount > 0;
    if (currentPage == PAGE_WEEK) pageDirty = true;
    Serial.print("[Schedule] week: ");
    Serial.print(weekPlan.dayCount);
    Serial.println(" gun");
  }

  // Duyuru — Topic: akilli-sinif/{id}/announcements
  else if (topicStr.endsWith("/announcements")) {
    announcement.title    = String((const char*)(doc["title"]    | ""));
    announcement.body     = String((const char*)(doc["body"]     | ""));
    announcement.severity = String((const char*)(doc["severity"] | "info"));
    announcement.valid = announcement.title.length() > 0;
    if (currentPage == PAGE_ANNOUNCE) pageDirty = true;
    Serial.println("[Announce] " + announcement.title);
  }

  // Uzaktan Config Sifirlama
  // Topic: akilli-sinif/sinif-1/control/reset
  // Payload: {"action":"reset_config"}
  else if (topicStr.endsWith("/control/reset")) {
    if (doc["action"] == "reset_config") {
      Serial.println("[Reset] Uzaktan config sifirlama komutu alindi!");

      // TFT'de goster
      tft.fillScreen(COLOR_BG);
      tft.setTextColor(COLOR_DANGER, COLOR_BG);
      tft.setTextSize(1);
      tft.setCursor(5, 30);
      tft.println("Uzaktan Reset!");
      tft.setCursor(5, 50);
      tft.println("Config siliniyor...");
      delay(1000);

      // NVS ve WiFi kimlik bilgilerini sil
      prefs.begin("akilli-sinif", false);
      prefs.clear();
      prefs.end();
      WiFi.disconnect(true, true);

      Serial.println("[Reset] NVS + WiFi silindi. Yeniden baslatiliyor...");
      delay(500);
      ESP.restart();
    }
  }

  Serial.println();
}

void setupMQTT() {
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setBufferSize(1024);  // Buyuk JSON mesajlari + OTA komutlari icin arttirildi

  // OTA manager'i basklat (MQTT client hazir olduktan sonra)
  if (otaManager != nullptr) delete otaManager;
  otaManager = new OTAManager(mqttClient, otaStatusTopic);
  Serial.println("[OTA] OTA Manager hazir. Firmware: v" + OTAManager::getCurrentVersion());
}

void connectMQTT() {
  Serial.println("\n========================================");
  Serial.println("MQTT Broker'a Baglaniliyor...");
  Serial.println("========================================");
  Serial.print("Broker: ");
  Serial.print(MQTT_BROKER);
  Serial.print(":");
  Serial.println(MQTT_PORT);
  
  // Ekranda goster
  tft.fillScreen(COLOR_BG);
  tft.setTextColor(COLOR_HEADER, COLOR_BG);
  tft.setTextSize(1);
  tft.setCursor(10, 30);
  tft.println("MQTT");
  tft.setCursor(10, 45);
  tft.println("Baglaniyor...");
  
  while (!mqttClient.connected() && reconnectAttempts < MAX_RECONNECT_ATTEMPTS) {
    Serial.print("MQTT Baglanti denemesi ");
    Serial.print(reconnectAttempts + 1);
    Serial.print("/");
    Serial.print(MAX_RECONNECT_ATTEMPTS);
    Serial.println("...");
    
    // LWT (Last Will and Testament) mesaji
    // Baglanti kopunca bu mesaj otomatik yayinlanir
    String willTopic   = buildTopic("status", "connection");
    String willMessage = "{\"status\":\"offline\",\"device\":\"" + mqttClientId + "\"}";

    if (mqttClient.connect(mqttClientId.c_str(), MQTT_USER, MQTT_PASSWORD,
                           willTopic.c_str(), 1, true, willMessage.c_str())) {
      mqttConnected = true;
      reconnectAttempts = 0;
      
      Serial.println("\n----------------------------------------");
      Serial.println("MQTT BAGLANDI!");
      Serial.println("----------------------------------------");
      
      // Ekranda goster
      tft.fillScreen(COLOR_BG);
      tft.setTextColor(COLOR_OK, COLOR_BG);
      tft.setCursor(10, 30);
      tft.println("MQTT OK!");
      delay(500);

      // Ana ekrani tekrar ciz
      pageDirty = true;
      
      // Kontrol topic'lerine subscribe ol
      subscribeToControlTopics();
      
      // Online durum mesaji gonder
      publishStatus("online");
      
    } else {
      Serial.print("Baglanti BASARISIZ, hata kodu: ");
      Serial.println(mqttClient.state());
      /*
       * Hata Kodlari:
       * -4 : MQTT_CONNECTION_TIMEOUT
       * -3 : MQTT_CONNECTION_LOST
       * -2 : MQTT_CONNECT_FAILED
       * -1 : MQTT_DISCONNECTED
       *  1 : MQTT_CONNECT_BAD_PROTOCOL
       *  2 : MQTT_CONNECT_BAD_CLIENT_ID
       *  3 : MQTT_CONNECT_UNAVAILABLE
       *  4 : MQTT_CONNECT_BAD_CREDENTIALS
       *  5 : MQTT_CONNECT_UNAUTHORIZED
       */
      reconnectAttempts++;
      
      // Ekranda deneme sayisini goster
      tft.fillRect(10, 80, 100, 15, COLOR_BG);
      tft.setTextColor(COLOR_WARNING, COLOR_BG);
      tft.setCursor(10, 80);
      tft.print("Deneme: ");
      tft.print(reconnectAttempts);
      tft.print("/");
      tft.print(MAX_RECONNECT_ATTEMPTS);
      
      delay(3000);  // 3 saniye bekle
    }
  }
  
  if (!mqttClient.connected()) {
    Serial.println("\n!!! MQTT BAGLANTI HATASI !!!");
    Serial.println("5 saniye sonra tekrar denenecek...");
    
    // Ekranda hata goster
    tft.fillScreen(COLOR_BG);
    tft.setTextColor(COLOR_DANGER, COLOR_BG);
    tft.setCursor(10, 30);
    tft.println("MQTT HATA!");
    tft.setCursor(10, 50);
    tft.println("Tekrar");
    tft.setCursor(10, 65);
    tft.println("deneniyor...");
    
    // Sonraki denemede tekrar dene
    reconnectAttempts = 0;
  }
}

void subscribeToControlTopics() {
  // Kontrol topic'lerine abone ol
  String sinifBase = String("akilli-sinif/") + CLASSROOM_ID;
  String topics[] = {
    buildTopic("control", "led"),
    buildTopic("control", "fan"),
    buildTopic("control", "alert"),        // Gorsel uyari sistemi
    buildTopic("control", "all"),          // Toplu komutlar icin
    buildTopic("control", "ota"),          // OTA guncelleme komutu
    buildTopic("control", "reset"),        // Bu cihaza ozel config sifirlama
    buildTopic("sensors", "camera"),       // Kamera verisini al (kisi sayisi)
    buildTopic("schedule", "current"),     // Simdiki ders bilgisi
    buildTopic("schedule", "today"),       // Bugunun dersleri
    buildTopic("schedule", "week"),        // Haftalik plan
    sinifBase + "/announcements",          // Duyurular
    "akilli-sinif/all/control/reset",      // Tum ESP32'lere broadcast reset
    "akilli-sinif/all/control/ota"         // Tum ESP32'lere broadcast OTA
  };

  const int numTopics = sizeof(topics) / sizeof(topics[0]);
  Serial.println("\nAbone olunan topic'ler:");
  for (int i = 0; i < numTopics; i++) {
    if (mqttClient.subscribe(topics[i].c_str(), 1)) {  // QoS 1 - OTA icin onemli
      Serial.print("  [OK] ");
      Serial.println(topics[i]);
    } else {
      Serial.print("  [HATA] ");
      Serial.println(topics[i]);
    }
  }
  Serial.println();
}

// ============================================
// SENSOR FONKSIYONLARI
// ============================================

void readSensors() {
  if (MOCK_MODE) {
    // Simule sensor verileri (test icin)
    readMockSensors();
  } else {
    // Gercek sensor okuma
    readRealSensors();
  }
}

void readMockSensors() {
  // Gercekci rastgele degerler uret
  // random() fonksiyonu ile kucuk degisimler ekle
  
  // Sicaklik: 20-28 derece arasi, yavas degisim
  static float baseTemp = 24.0;
  baseTemp += (random(-10, 11) / 100.0);  // -0.1 ile +0.1 arasi
  baseTemp = constrain(baseTemp, 20.0, 28.0);
  temperature = baseTemp;
  
  // Nem: 40-60% arasi
  static float baseHumidity = 50.0;
  baseHumidity += (random(-20, 21) / 100.0);  // -0.2 ile +0.2 arasi
  baseHumidity = constrain(baseHumidity, 40.0, 60.0);
  humidity = baseHumidity;
  
  // Isik seviyesi: 0-1000 lux
  // Gun icerisinde degisim simule et
  int hour = (millis() / 60000) % 24;  // Dakikayi saat gibi kullan (hizlandirilmis)
  if (hour >= 6 && hour <= 18) {
    lightLevel = random(300, 800);  // Gunduz
  } else {
    lightLevel = random(0, 100);    // Gece
  }
  
  // Hava kalitesi: 50-300 ppm
  static int baseAirQuality = 100;
  baseAirQuality += random(-5, 6);
  baseAirQuality = constrain(baseAirQuality, 50, 300);
  airQuality = baseAirQuality;
  
  // Hareket: rastgele (her 10 okumada bir degisim)
  if (random(0, 10) == 0) {
    motionDetected = !motionDetected;
  }
  
  // Pencere durumu: genellikle kapali, nadiren degisir
  if (random(0, 50) == 0) {
    windowOpen = !windowOpen;
  }
}

void readRealSensors() {
  // ========== DHT11 - Sicaklik ve Nem ==========
  float newTemp = dht.readTemperature();
  float newHum = dht.readHumidity();
  
  // DHT okuma hatasi kontrolu
  if (!isnan(newTemp) && !isnan(newHum)) {
    temperature = newTemp;
    humidity = newHum;
  } else {
    Serial.println("DHT okuma hatasi!");
    // Onceki degerleri koru
  }
  
  // ========== LDR - Isik Seviyesi ==========
  // ESP32 ADC 12-bit (0-4095)
  // LDR + 10K direnc ile voltaj bolucu
  int rawLight = analogRead(PIN_LDR);
  // Ham degeri lux'a yaklasik cevir (0-1000 lux)
  // Dusuk deger = karanlik, yuksek deger = aydinlik
  lightLevel = map(rawLight, 0, 4095, 0, 1000);
  
  // ========== MQ-135 - Hava Kalitesi ==========
  // MQ-135 isindiktan sonra (1-2 dakika) dogru deger verir
  int rawAir = analogRead(PIN_MQ135);
  // Ham degeri ppm'e yaklasik cevir
  // Dusuk deger = temiz hava, yuksek deger = kirli hava
  airQuality = map(rawAir, 0, 4095, 0, 500);
  
  // ========== PIR - Hareket Algilama ==========
  // HIGH = hareket var, LOW = hareket yok
  motionDetected = digitalRead(PIN_PIR) == HIGH;
  
  // ========== Reed Switch - Pencere Durumu ==========
  // Pull-up aktif: LOW = kapali (magnet yakin), HIGH = acik (magnet uzak)
  windowOpen = digitalRead(PIN_WINDOW) == HIGH;
}

// ============================================
// AKTUATOR FONKSIYONLARI
// ============================================

void setupActuators() {
  // ESP32 Arduino Core 3.x yeni PWM API
  // ledcAttach(pin, freq, resolution) - kanal otomatik atanir
  ledcAttach(PIN_LED, PWM_FREQ, PWM_RESOLUTION);
  ledcAttach(PIN_FAN, PWM_FREQ, PWM_RESOLUTION);
  
  // Baslangicta tum aktuatorleri kapat
  setLedBrightness(0);
  setFanSpeed(0);
  
  Serial.println("Aktuatorler hazir.");
}

void setupSensors() {
  // DHT11 sensoru baslat
  dht.begin();
  Serial.println("DHT11 sensoru baslatildi");
  
  // PIR sensoru - dijital giris
  pinMode(PIN_PIR, INPUT);
  Serial.println("PIR sensoru baslatildi");
  
  // Reed switch - dahili pull-up ile dijital giris
  // Kapali = LOW (magnet yakin), Acik = HIGH (magnet uzak)
  pinMode(PIN_WINDOW, INPUT_PULLUP);
  Serial.println("Pencere sensoru (Reed Switch) baslatildi");
  
  // LDR ve MQ-135 analog pinler - ayar gerekmez (analogRead otomatik)
  // Ancak ADC cozunurlugunu ayarlayabiliriz
  analogReadResolution(12);  // 12-bit (0-4095)
  Serial.println("Analog sensorler (LDR, MQ-135) hazir");
  
  Serial.println("Tum sensorler baslatildi.");
}

void setLedBrightness(int percent) {
  // Yuzdeyi 0-255 araligina cevir
  percent = constrain(percent, 0, 100);
  int pwmValue = map(percent, 0, 100, 0, 255);
  ledcWrite(PIN_LED, pwmValue);  // ESP32 Core 3.x: pin numarasi kullanilir
  ledBrightness = percent;
  
  // Durum bilgisi yayinla
  publishActuatorStatus("led", ledBrightness);
}

void setFanSpeed(int percent) {
  // Yuzdeyi 0-255 araligina cevir
  percent = constrain(percent, 0, 100);
  int pwmValue = map(percent, 0, 100, 0, 255);
  ledcWrite(PIN_FAN, pwmValue);  // ESP32 Core 3.x: pin numarasi kullanilir
  fanSpeed = percent;
  
  // Durum bilgisi yayinla
  publishActuatorStatus("fan", fanSpeed);
}

// ============================================
// MQTT PUBLISH FONKSIYONLARI
// ============================================

void publishSensorData() {
  // Her sensor icin ayri topic'e yayinla
  
  // Sicaklik
  StaticJsonDocument<128> tempDoc;
  tempDoc["value"] = temperature;
  tempDoc["unit"] = "C";
  tempDoc["timestamp"] = millis();
  publishJson(buildTopic("sensors", "temperature"), tempDoc);
  
  // Nem
  StaticJsonDocument<128> humDoc;
  humDoc["value"] = humidity;
  humDoc["unit"] = "%";
  humDoc["timestamp"] = millis();
  publishJson(buildTopic("sensors", "humidity"), humDoc);
  
  // Isik
  StaticJsonDocument<128> lightDoc;
  lightDoc["value"] = lightLevel;
  lightDoc["unit"] = "lux";
  lightDoc["timestamp"] = millis();
  publishJson(buildTopic("sensors", "light"), lightDoc);
  
  // Hava Kalitesi
  StaticJsonDocument<128> airDoc;
  airDoc["value"] = airQuality;
  airDoc["unit"] = "ppm";
  airDoc["timestamp"] = millis();
  publishJson(buildTopic("sensors", "air_quality"), airDoc);
  
  // Hareket
  StaticJsonDocument<128> pirDoc;
  pirDoc["detected"] = motionDetected;
  pirDoc["timestamp"] = millis();
  publishJson(buildTopic("sensors", "pir"), pirDoc);
  
  // Pencere Durumu
  StaticJsonDocument<128> windowDoc;
  windowDoc["open"] = windowOpen;
  windowDoc["timestamp"] = millis();
  publishJson(buildTopic("sensors", "window"), windowDoc);
  
  // Serial'a ozet yazdir
  Serial.println("--- Sensor Verileri Yayinlandi ---");
  Serial.print("Sicaklik: ");
  Serial.print(temperature, 1);
  Serial.print("C | Nem: ");
  Serial.print(humidity, 1);
  Serial.print("% | Isik: ");
  Serial.print(lightLevel);
  Serial.print(" lux | Hava: ");
  Serial.print(airQuality);
  Serial.print(" ppm | Hareket: ");
  Serial.print(motionDetected ? "VAR" : "YOK");
  Serial.print(" | Pencere: ");
  Serial.print(windowOpen ? "ACIK" : "KAPALI");
  Serial.print(" | Kisi: ");
  Serial.println(personCount);
}

void publishActuatorStatus(const char* actuator, int value) {
  StaticJsonDocument<128> doc;
  doc["value"] = value;
  doc["unit"] = "%";
  doc["timestamp"] = millis();
  
  String topic = buildTopic("actuators", actuator);
  publishJson(topic, doc);
}

void publishStatus(const char* status) {
  StaticJsonDocument<256> doc;
  doc["status"] = status;
  doc["device"]           = mqttClientId;
  doc["classroom"]        = CLASSROOM_ID;
  doc["ip"]               = WiFi.localIP().toString();
  doc["rssi"]             = WiFi.RSSI();
  doc["uptime"]           = millis() / 1000;
  doc["mock_mode"]        = MOCK_MODE;
  doc["firmware_version"] = FIRMWARE_VERSION;  // OTA versiyon takibi icin

  String topic = buildTopic("status", "connection");
  publishJson(topic, doc, true);  // retained message

  Serial.print("Durum yayinlandi: ");
  Serial.println(status);
}

void publishJson(String topic, JsonDocument& doc, bool retained) {
  char buffer[512];
  serializeJson(doc, buffer);
  mqttClient.publish(topic.c_str(), buffer, retained);
}

// Overload: retained varsayilan false
void publishJson(String topic, JsonDocument& doc) {
  publishJson(topic, doc, false);
}

// ============================================
// SETUP VE LOOP
// ============================================

void setup() {
  // Seri port baslatma
  Serial.begin(115200);
  delay(500);

  // ── ADIM 0: Config reset butonunu kontrol et (TFT oncesi, erken yakalamak icin)
  // (TFT henuz hazir degil, sadece serial log)
  pinMode(CONFIG_RESET_PIN, INPUT_PULLUP);

  // ── ADIM 1: NVS'den kayitli config'i oku
  // (WiFiManager'dan once olmali, broker IP vs. için)
  // TFT baslatmadan once cagrilmasi gerciyor cunku WiFiManager TFT kullanabilir
  // Ortak prefs alanlarini tanimlayabilmek icin once cagiriyoruz:
  {
    prefs.begin("akilli-sinif", true);
    String sinifId = prefs.getString("classroom_id", "sinif-1");
    String mockStr = prefs.getString("mock_mode", "false");
    String broker  = prefs.getString("mqtt_broker", "");
    prefs.end();
    sinifId.toCharArray(CLASSROOM_ID, sizeof(CLASSROOM_ID));
    mockStr.toCharArray(MOCK_MODE_STR, sizeof(MOCK_MODE_STR));
    broker.toCharArray(MQTT_BROKER, sizeof(MQTT_BROKER));
    MOCK_MODE    = (mockStr == "true");
    mqttClientId = "esp32-plc-" + sinifId;
    otaStatusTopic = "akilli-sinif/" + sinifId + "/status/ota";
  }

  Serial.println("\n\n");
  Serial.println("╔══════════════════════════════════════╗");
  Serial.println("║     AKILLI SINIF SISTEMI              ║");
  Serial.println("║     ESP32 PLC + TFT Display          ║");
  Serial.println("╠══════════════════════════════════════╣");
  Serial.print  ("║  Sinif:    ");
  Serial.println(CLASSROOM_ID);
  Serial.print  ("║  Mod:      ");
  Serial.println(MOCK_MODE ? "SIMULASYON" : "GERCEK DONANIM");
  Serial.print  ("║  Firmware: v");
  Serial.println(FIRMWARE_VERSION);
  Serial.println("╚══════════════════════════════════════╝\n");

  // ── ADIM 2: TFT Ekrani baslat
  setupDisplay();
  delay(500);

  // ── ADIM 3: Config reset butonu kontrolu (TFT ekranla geri sayimli)
  checkConfigReset();

  // ── ADIM 4: Aktuatorler ve sensorler
  setupActuators();
  setupSensors();

  // ── ADIM 4b: Menu butonlari (NEXT/PREV — sayfa gezinme)
  setupButtons();

  // ── ADIM 5: WiFiManager ile WiFi baglan
  // (Ilk kez veya config silinmisse portal acar)
  setupWiFi();

  // ── ADIM 5b: NTP ile saat senkronizasyonu (saat sayfasi + geri sayim icin)
  setupNTP();

  // ── ADIM 6: MQTT ayarla ve baglan
  setupMQTT();
  connectMQTT();

  Serial.println("\n╔══════════════════════════════════════╗");
  Serial.println("║         SİSTEM HAZIR!              ║");
  Serial.print  ("║  Sinif: ");
  Serial.print(CLASSROOM_ID);
  Serial.println("                      ║");
  Serial.print  ("║  OTA topic: control/ota         ║");
  Serial.println("╚══════════════════════════════════════╝\n");

  // Ilk ekran guncellemesi
  updateDisplay();
  lastPageChangeMs = millis();  // Otomatik cevrim referansi
}

void loop() {
  unsigned long currentMillis = millis();

  // Menu butonlari + otomatik sayfa cevrimi (her iterasyonda, gecikmesiz)
  pollButtons();
  handleAutoRotate();

  // WiFi kontrolu
  if (currentMillis - lastWifiCheck >= WIFI_CHECK_INTERVAL) {
    lastWifiCheck = currentMillis;
    checkWiFi();
  }
  
  // MQTT baglantisini kontrol et ve gerekirse yeniden baglan
  if (!mqttClient.connected()) {
    mqttConnected = false;

    // Cok hizli deneme yapmayi onle - 5 saniye bekle
    if (currentMillis - lastMqttReconnectAttempt >= MQTT_RECONNECT_INTERVAL) {
      lastMqttReconnectAttempt = currentMillis;
      connectMQTT();
    }
  }
  mqttClient.loop();  // Gelen mesajlari isle
  
  // Sensor okuma
  if (currentMillis - lastSensorRead >= SENSOR_INTERVAL) {
    lastSensorRead = currentMillis;
    readSensors();
  }
  
  // MQTT yayinlama
  if (currentMillis - lastMqttPublish >= PUBLISH_INTERVAL) {
    lastMqttPublish = currentMillis;
    if (mqttConnected) {
      publishSensorData();
    }
  }
  
  // Periyodik durum yayinlama
  if (currentMillis - lastStatusPublish >= STATUS_INTERVAL) {
    lastStatusPublish = currentMillis;
    if (mqttConnected) {
      publishStatus("online");
    }
  }
  
  // Ekran guncellemesi
  if (currentMillis - lastDisplayUpdate >= DISPLAY_INTERVAL) {
    lastDisplayUpdate = currentMillis;
    updateDisplay();
    checkAutoAlerts();  // Otomatik uyari kontrolu
  }
  
  // Uyari LED guncellemesi (surekli calistir)
  updateAlertLED();
}
