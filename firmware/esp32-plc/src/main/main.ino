/*
 * Akilli Sinif Sistemi - ESP32 PLC Firmware
 * ==========================================
 *
 * Bu kod ESP32'yi bir PLC gibi calistirir:
 * - WiFiManager ile guvenli WiFi kurulumu (AP portal, NVS'e kayit)
 * - MQTT ile haberlesme
 * - Sensor okuma (simule veya gercek)
 * - Aktuator kontrolu (LED dimming, cooling/heating roleleri)
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
 *   3. Acilan portala gir: WiFi, MQTT IP, Sinif ID ayarla
 *   4. Kaydet — ESP32 yeniden baslar ve baglanir
 *
 * CONFIG SIFIRLAMA:
 *   GPIO0 butonuna 5 saniye bas → NVS silinir → Portal tekrar acar
 *
 * AP SIFRESI:
 *   AP "Akilli-Sinif-Setup" agi WPA2 korumali. Sifre cihaz MAC'inden
 *   turetilir → "akilli-XXXXXX" (XXXXXX = MAC'in son 3 byte hex).
 *   TFT ekranda portal modunda gorulur, cihaz etiketinde de yazili.
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
#include <stdarg.h>    // logRemote(...) varargs
#include "ota_manager.h"  // OTA guncelleme modulu
#include "../../../version.h"   // Firmware versiyon
#include "../../../secrets.h"   // MQTT user/sifre — repo'ya commit'lenmez

// Forward decl: Arduino IDE auto-prototype'i pollButton(ButtonState&)
// dosyanin tepesine koyar. Struct asagida tanimli oldugu icin tipi
// burada bildirmek gerek.
struct ButtonState;

// Forward decl: varargs (...) icin auto-prototype calismaz, manuel lazim
void logRemote(const char* fmt, ...);

// ============================================
// YAPILANDIRMA - NVS'DEN OKUNUR (hardcoded degil!)
// WiFi + diger ayarlar ilk botta WiFiManager portali
// uzerinden ayarlanir ve ESP32'nin flash'ina kaydedilir.
// ============================================

Preferences prefs;  // NVS okuma/yazma

// Runtime'da NVS'den doludurulan degiskenler
char MQTT_BROKER[40]   = "";
char CLASSROOM_ID[20]  = "sinif-1";

// Sabit kalan degerler (degistirmen gerekirse portal'a da eklenebilir)
const int   MQTT_PORT      = 1883;
const char* MQTT_USER      = MQTT_USER_DEFAULT;      // secrets.h
const char* MQTT_PASSWORD  = MQTT_PASSWORD_DEFAULT;  // secrets.h

// MQTT Client ID: sinif ID'sinden otomatik uretilir
String mqttClientId;  // "esp32-plc-sinif-1" formatinda

// Config sifirlama butonu (GPIO0 = BOOT butonu)
const int CONFIG_RESET_PIN          = 0;
const unsigned long RESET_HOLD_MS   = 5000;  // 5 saniye bas

// ============================================
// PIN TANIMLARI
// ============================================

// Sensor Pinleri (gercek donanim icin)
// NOT: ESP32-CAM (AI-Thinker) kamera pinleri 5/18/19/21'i kullanir.
// Bu PLC kartinda 21 cooling role'sine ayrilmistir — fiziksel cakisma yok,
// iki ayri board. Pinleri PLC <-> CAM arasinda tasirken cakisma kontrol et.
const int PIN_DHT = 4;           // DHT11 sicaklik/nem sensoru
const int PIN_LDR = 34;          // Isik sensoru (ADC, input-only)
const int PIN_MQ135 = 35;        // Hava kalitesi sensoru (ADC, input-only)
const int PIN_CURRENT = 36;      // LM358 opamp cikisi (ADC1_CH0, VP, input-only)
const int PIN_PIR = 27;          // Hareket sensoru
const int PIN_WINDOW = 26;       // Pencere sensoru (reed switch)

// DHT Sensor Ayari
#define DHTTYPE DHT11            // DHT11 kullaniyoruz (DHT22 icin DHT22 yaz)

// Aktuator Pinleri
const int PIN_LED = 13;          // LED (PWM ile dimming)
// GPIO 16: eski PWM/MOSFET fan icin kullaniliyordu, artik bos (ileride ayrilabilir)
const int PIN_COOLING = 21;      // Role 1 - cooling fan (BC337 + JRC-19F + 12V DC fan)
const int PIN_HEATING = 22;      // Role 2 - heater (BC337 + JRC-19F + 22ohm 5W direnc)
#define RELAY_ACTIVE_LEVEL HIGH  // BC337 NPN low-side: GPIO HIGH -> role enerjili
// NOT: Buzzer yerine gorsel uyari sistemi kullaniliyor (TFT ekran + Status LED)

// Menu Butonlari (ogrencinin sayfayi degistirmesi icin)
const int PIN_BTN_NEXT = 25;     // Sonraki sayfa (aktif LOW, dahili pullup)
const int PIN_BTN_PREV = 14;     // Onceki sayfa (aktif LOW, dahili pullup)

// Fiziksel duvar anahtarlari (push button, aktif LOW, INPUT_PULLUP)
// Online (MQTT) ve fiziksel buton ayni state'i degistirir (toggle mantigi).
const int PIN_BTN_LED     = 5;   // LED on/off toggle
const int PIN_BTN_COOLING = 16;  // Cooling on/off toggle (eski PWM fan pini)
const int PIN_BTN_HEATING = 19;  // Heating on/off toggle

// TFT Ekran Pinleri (ST7735 1.44")
// NOT: TFT_eSPI kutuphanesi User_Setup.h dosyasindan pin okur
// Asagidaki pinler referans icindir:
// TFT_CS   = GPIO 17
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
int rawCurrent = 0;
float currentAmps = 0.0f;
float powerWatts = 0.0f;

// 12V besleme gerilimi (sabit kabul)
const float SUPPLY_VOLTAGE = 12.0f;

// Akim olcumu: shunt + LM358 (gain ~20). Donanim tasarimi: 1.5A -> 3.0V ADC.
// Akim, pin voltaji ile DOGRU ORANTILI -> A = V * AMPS_PER_VOLT.
// Kalibrasyon noktasi: 1.5 A @ 3.0 V  ->  AMPS_PER_VOLT = 1.5 / 3.0 = 0.5 A/V
//   ornek: 3.0V -> 1.5A,  1.8V -> 0.9A,  0V -> 0A
// ADC: 11db atenuasyon, 12-bit -> tam olcek ~3.3V = 4095 raw.
// Yeniden kalibrasyon: ampermetre ile gercek akimi olcup karsi pin voltajini
// not al, CAL_AMPS / CAL_VOLTS noktasini guncelle.
const float ADC_VREF      = 3.3f;                    // V, ADC_11db tam olcek
const int   ADC_MAX       = 4095;                    // 12-bit
const float CAL_AMPS      = 1.5f;                    // kalibrasyon akimi (A)
const float CAL_VOLTS     = 3.0f;                    // o akimdaki pin voltaji (V)
const float AMPS_PER_VOLT = CAL_AMPS / CAL_VOLTS;    // = 0.5 A/V

// Kamera verisi (MQTT'den gelecek)
int personCount = 0;
unsigned long lastPersonCountUpdate = 0;

// Aktuator Durumlari
int ledBrightness = 0;           // 0-100 arasi
int lastLedBrightness = 100;     // LED kapanmadan onceki parlaklik (toggle icin yedek)
bool coolingOn = false;          // Cooling role durumu
bool heatingOn = false;          // Heating role durumu

// Fiziksel duvar anahtari debounce state'i (push button, aktif LOW)
struct ToggleBtn {
  int pin;
  bool stable;          // son kararli okuma
  bool lastReading;     // son anlik okuma
  unsigned long lastChangeMs;
};
ToggleBtn btnLedTog     = {PIN_BTN_LED,     HIGH, HIGH, 0};
ToggleBtn btnCoolingTog = {PIN_BTN_COOLING, HIGH, HIGH, 0};
ToggleBtn btnHeatingTog = {PIN_BTN_HEATING, HIGH, HIGH, 0};
const unsigned long TOGGLE_DEBOUNCE_MS = 25;

// Ladder otomasyon kontrolu
bool autoMode = true;            // true: sicaklik esikleri ile otomatik; false: sadece MQTT
unsigned long manualOverrideUntil = 0;  // millis() bu degerin altindayken otomatik baski
const unsigned long MANUAL_OVERRIDE_MS = 5UL * 60UL * 1000UL;  // 5 dk

// Sicaklik esikleri (config.json automation.thermal_control ile ayni)
const float TEMP_COOL_ON  = 26.0;
const float TEMP_COOL_OFF = 24.0;
const float TEMP_HEAT_ON  = 20.0;
const float TEMP_HEAT_OFF = 22.0;

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

// Olcum / debug log degiskenleri (3.1 Sistem Performansi olcumleri icin)
unsigned long g_wifiDownAtMs = 0;   // WiFi kopma anindaki millis() (reconnect olcumu)
unsigned long g_mqttDownAtMs = 0;   // MQTT kopma anindaki millis()

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
int alertIconShown = 0;     // sag ust uyari ikonu ekranda gorunur mu (0/1) — blink takibi
int prevMinute = -1;
int prevDay    = -1;

// ============================================
// MENU / SAYFA SISTEMI
// ============================================

enum Page {
  PAGE_HOME = 0,   // Ana ekran (saat + ders + sensor seridi + status)
  PAGE_SENSORS,    // Detay sensor: kisi sayisi + buyuk sensor degerleri
  PAGE_NOW,        // Simdiki ders (ogretmen, konu, ilerleme)
  PAGE_WEEK,       // Haftalik plan (5 gun grid)
  PAGE_ANNOUNCE,   // Duyurular
  PAGE_COUNT
};

Page currentPage = PAGE_HOME;
uint32_t lastPageChangeMs = 0;
bool pageDirty = true;          // sayfa degisti, tam yeniden ciz
bool rotationPaused = false;    // ikisi-buton basisi ile duraklatma

// Sayfa basina otomatik gecis suresi (HOME daha uzun durur)
const uint32_t PAGE_DURATION_MS[PAGE_COUNT] = {
  15000, // HOME — ana ekran, en uzun
   6000, // SENSORS
   6000, // NOW
   6000, // WEEK
   6000  // ANNOUNCE
};

// Buton zamanlama sabitleri (profesyonel debounce + double + hold)
const uint32_t BUTTON_DEBOUNCE_MS    = 25;
const uint32_t BUTTON_DOUBLE_GAP_MS  = 400;   // iki kisa basis arasi maksimum
const uint32_t BUTTON_HOLD_START_MS  = 400;   // bu sure sonra auto-repeat baslar
const uint32_t BUTTON_HOLD_INTERVAL  = 200;   // auto-repeat aralik
const uint32_t BUTTON_BOTH_PAUSE_MS  = 1000;  // ikisi-bas: pause toggle suresi

// Buton event queue (loop bloklanmaz, asenkron isleme)
enum BtnEvent {
  BTN_NONE = 0,
  BTN_NEXT_SHORT,
  BTN_PREV_SHORT,
  BTN_NEXT_DOUBLE,
  BTN_PREV_DOUBLE,
  BTN_NEXT_REPEAT,
  BTN_PREV_REPEAT
};

static const uint8_t BTN_QUEUE_SIZE = 8;
BtnEvent btnQueue[BTN_QUEUE_SIZE];
uint8_t btnQueueHead = 0;
uint8_t btnQueueTail = 0;

bool btnQueuePush(BtnEvent e) {
  uint8_t next = (btnQueueHead + 1) % BTN_QUEUE_SIZE;
  if (next == btnQueueTail) return false; // dolu, dusur
  btnQueue[btnQueueHead] = e;
  btnQueueHead = next;
  return true;
}

BtnEvent btnQueuePop() {
  if (btnQueueHead == btnQueueTail) return BTN_NONE;
  BtnEvent e = btnQueue[btnQueueTail];
  btnQueueTail = (btnQueueTail + 1) % BTN_QUEUE_SIZE;
  return e;
}

struct ButtonState {
  int pin;
  bool stable;             // HIGH=bosta, LOW=basili (debounce sonrasi)
  bool lastRaw;            // son ham okuma
  uint32_t lastChangeMs;   // raw degisim zamani
  uint32_t pressStartMs;   // basili kalma baslangici
  uint32_t lastRepeatMs;   // son auto-repeat fire zamani
  uint32_t pendingSingleMs;// > 0 ise single basis double-window'da bekliyor
  bool repeatActive;       // hold-repeat baslatti / double parcasi (release'de short emitleme)
  BtnEvent shortEvent;
  BtnEvent doubleEvent;
  BtnEvent repeatEvent;
};

ButtonState btnNext = {
  PIN_BTN_NEXT, HIGH, HIGH, 0, 0, 0, 0, false,
  BTN_NEXT_SHORT, BTN_NEXT_DOUBLE, BTN_NEXT_REPEAT
};
ButtonState btnPrev = {
  PIN_BTN_PREV, HIGH, HIGH, 0, 0, 0, 0, false,
  BTN_PREV_SHORT, BTN_PREV_DOUBLE, BTN_PREV_REPEAT
};

// Ikisi-buton (NEXT+PREV) basili kalinca pause toggle
uint32_t bothHeldStartMs = 0;
bool bothPauseFired = false;
bool bothCurrentlyPressed = false;

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
  // Satir 1: sinif adi + durum ikonlari
  tft.setTextSize(1);
  tft.setTextColor(COLOR_HEADER, bg);
  tft.setCursor(5, 1);
  tft.print(title);

  // MQTT durum ikonu (sag ust, daha yukarida)
  tft.fillCircle(122, 4, 3, mqttConnected ? COLOR_OK : COLOR_DANGER);

  // Duraklatma gostergesi
  if (rotationPaused) {
    tft.setTextColor(COLOR_WARNING, bg);
    tft.setCursor(100, 1);
    tft.print("P");
  }

  // Uyari ikonu (sag ust kose, yanip sonen "!") updateAlertIcon() tarafindan
  // cizilir. Header fillScreen ile temizlendigi icin ikonu "gizli" isaretle;
  // updateAlertIcon bir sonraki adimda gerekirse yeniden cizer.
  alertIconShown = 0;

  // Satir 2: saat / gun / tarih (updateHeaderTime gercek degerleri basar)
  prevMinute = -1;
  prevDay    = -1;

  // Ust cizgi
  tft.drawLine(0, 20, 128, 20, COLOR_HEADER);
}

// Header saat/tarih alanini partial-update: her tick'te cagrilir.
void updateHeaderTime() {
  uint16_t bgColor = COLOR_BG;  // uyari ekran arka plan rengini degistirmez

  struct tm t;
  bool haveTime = getLocalTime(&t, 100);

  if (!haveTime) {
    if (prevMinute != -2) {
      tft.fillRect(0, 10, 128, 9, bgColor);
      tft.setTextSize(1);
      tft.setTextColor(COLOR_LABEL, bgColor);
      tft.setCursor(5, 11);
      tft.print("NTP bekleniyor");
      prevMinute = -2;
    }
    return;
  }

  if (t.tm_min != prevMinute || t.tm_wday != prevDay) {
    tft.fillRect(0, 10, 128, 9, bgColor);
    tft.setTextSize(1);

    char timeStr[6];
    strftime(timeStr, sizeof(timeStr), "%H:%M", &t);
    tft.setTextColor(COLOR_VALUE, bgColor);
    tft.setCursor(5, 11);
    tft.print(timeStr);

    const char* dayNames[7] = {"Paz","Pzt","Sal","Car","Per","Cum","Cmt"};
    tft.setTextColor(COLOR_LABEL, bgColor);
    tft.setCursor(40, 11);
    tft.print(dayNames[t.tm_wday]);

    char dateStr[12];
    snprintf(dateStr, sizeof(dateStr), "%02d/%02d/%04d",
             t.tm_mday, t.tm_mon + 1, t.tm_year + 1900);
    tft.setCursor(62, 11);
    tft.print(dateStr);

    prevMinute = t.tm_min;
    prevDay    = t.tm_wday;
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

// ---- Pagination Dots: alt bantta sayfa gostergeleri ----
// Her sayfanin alt 8px'inde, ortada noktalar + uclarda < > oklari.
// Aktif sayfa: dolu daire (header rengi); pasif: kucuk gri nokta.
// Pause modunda aktif dot rengi sariya doner (header'daki "P" ile cift gosterge).
void drawPaginationDots(uint8_t current, uint8_t total, bool paused) {
  const int barY        = 120;
  const int barH        = 8;
  const int dotY        = 124;
  const int leftArrowX  = 3;
  const int rightArrowX = 119;
  const int dotsCenterX = 64;
  const int dotSpacing  = 16;

  uint16_t bgColor = COLOR_BG;  // uyari ekran arka plan rengini degistirmez

  // Bar arka planini temizle
  tft.fillRect(0, barY, 128, barH, bgColor);

  // Sol/sag oklar (buton yonu ipucu)
  tft.setTextSize(1);
  tft.setTextColor(COLOR_LABEL, bgColor);
  tft.setCursor(leftArrowX, barY);
  tft.print('<');
  tft.setCursor(rightArrowX, barY);
  tft.print('>');

  // Noktalari ortala
  if (total == 0) return;
  int totalSpan = ((int)total - 1) * dotSpacing;
  int startX = dotsCenterX - totalSpan / 2;
  for (uint8_t i = 0; i < total; i++) {
    int x = startX + (int)i * dotSpacing;
    if (i == current) {
      uint16_t color = paused ? COLOR_WARNING : COLOR_HEADER;
      tft.fillCircle(x, dotY, 2, color);
    } else {
      tft.fillCircle(x, dotY, 1, COLOR_LABEL);
    }
  }
}

// ---- Sayfa: HOME (ana ekran) ----
// Saat+tarih header (ortak drawHeader) + buyuk ders blogu + sensor seridi + status dot
void renderHome(bool full) {
  uint16_t bgColor = COLOR_BG;  // uyari ekran arka plan rengini degistirmez

  if (full) {
    tft.fillScreen(bgColor);
    drawHeader(CLASSROOM_ID, bgColor);
  }

  // --- Buyuk blok: o anki ders (y=24..76) ---
  // y=26: konu (size 2, 16px)
  // y=44: saat araligi (size 1, 8px)
  // y=54: "X dk kaldi" (size 1, 8px)
  // y=64: progress bar (8px) — derslerin gorsel ilerlemesi
  tft.fillRect(0, 24, 128, 52, bgColor);
  if (currentLesson.valid) {
    // Konu (buyuk)
    tft.setTextSize(2);
    tft.setTextColor(COLOR_HEADER, bgColor);
    tft.setCursor(5, 26);
    tft.print(currentLesson.subject.substring(0, 10));

    // Saat araligi
    tft.setTextSize(1);
    tft.setTextColor(COLOR_LABEL, bgColor);
    tft.setCursor(5, 44);
    tft.print(currentLesson.startHHMM);
    tft.print(" - ");
    tft.print(currentLesson.endHHMM);

    // Kalan dakika + progress bar
    struct tm t;
    float pct = -1.0f;
    if (getLocalTime(&t, 100)) {
      int startMins = hhmmToMinutes(currentLesson.startHHMM);
      int endMins   = hhmmToMinutes(currentLesson.endHHMM);
      int nowMins   = t.tm_hour * 60 + t.tm_min;
      if (startMins >= 0 && endMins > startMins && nowMins >= startMins && nowMins <= endMins) {
        int remaining = endMins - nowMins;
        pct = (float)(nowMins - startMins) / (float)(endMins - startMins);
        if (pct < 0) pct = 0;
        if (pct > 1) pct = 1;
        tft.setTextColor(COLOR_VALUE, bgColor);
        tft.setCursor(5, 54);
        tft.print(remaining);
        tft.print(" dk kaldi");
      } else if (nowMins < startMins) {
        tft.setTextColor(COLOR_LABEL, bgColor);
        tft.setCursor(5, 54);
        tft.print("Teneffus");
      }
    }

    // Progress bar (her zaman ciz, ders aktif degilse bos goster)
    const int barX = 5, barY = 64, barW = 118, barH = 8;
    tft.drawRect(barX, barY, barW, barH, COLOR_HEADER);
    if (pct >= 0.0f) {
      int fillW = (int)((barW - 2) * pct);
      if (fillW > 0) {
        tft.fillRect(barX + 1, barY + 1, fillW, barH - 2, COLOR_VALUE);
      }
    }
  } else {
    tft.setTextSize(2);
    tft.setTextColor(COLOR_LABEL, bgColor);
    tft.setCursor(5, 40);
    tft.print("Ders yok");
  }

  // --- Ayirici ---
  tft.drawLine(0, 78, 128, 78, COLOR_HEADER);

  // --- Sensor seridi (y=82..104) — ogrenci icin anlamli ozet ---
  // Pencere/PIR/lux kaldirildi: ogrenciye teknik gurultu, K (kisi) zaten doluluk anlam tasiyor.
  tft.fillRect(0, 82, 128, 22, bgColor);
  tft.setTextSize(1);
  char buf[16];

  // Ust satir: Sicaklik / Nem
  tft.setTextColor(getTemperatureColor(temperature), bgColor);
  tft.setCursor(2, 84);
  snprintf(buf, sizeof(buf), "Sic:%.0fC", temperature);
  tft.print(buf);

  tft.setTextColor(COLOR_VALUE, bgColor);
  tft.setCursor(70, 84);
  snprintf(buf, sizeof(buf), "Nem:%.0f%%", humidity);
  tft.print(buf);

  // Alt satir: Hava kalitesi (ppm) / Kisi sayisi
  tft.setTextColor(getAirQualityColor(airQuality), bgColor);
  tft.setCursor(2, 96);
  int aqShown = airQuality > 999 ? 999 : airQuality;
  snprintf(buf, sizeof(buf), "Hava:%dppm", aqShown);
  tft.print(buf);

  tft.setTextColor(personCount == 0 ? COLOR_EMPTY : COLOR_OCCUPIED, bgColor);
  tft.setCursor(80, 96);
  snprintf(buf, sizeof(buf), "Kisi:%d", personCount);
  tft.print(buf);

  // y=108-118 arasi bos (eski Durum bandi). Ihtiyac olursa sayfa duzeni genisletilebilir.
}

// ---- Sayfa: ANA EKRAN (saat/tarih + kisi + sensor) ----
void renderSensors(bool full) {
  uint16_t bgColor = COLOR_BG;  // uyari ekran arka plan rengini degistirmez

  if (full) {
    tft.fillScreen(bgColor);
    drawHeader(CLASSROOM_ID, bgColor);

    // Kisi sayisi etiketi
    tft.setTextSize(1);
    tft.setTextColor(COLOR_LABEL, bgColor);
    tft.setCursor(5, 26);
    tft.print("KISI SAYISI");

    // Sensor bolumu ayirici
    tft.drawLine(0, 80, 128, 80, COLOR_HEADER);

    // Sensor etiketleri
    tft.setTextColor(COLOR_LABEL, bgColor);
    tft.setCursor(5, 84);  tft.print("Sicaklik:");
    tft.setCursor(5, 98);  tft.print("Nem:");
    tft.setCursor(5, 112); tft.print("Hava:");

    // Flicker-onleme icin sifirla
    prevTemperature   = -999;
    prevHumidity      = -999;
    prevAirQuality    = -999;
    prevPersonCount   = -999;
    prevMqttConnected = !mqttConnected;
  }

  // MQTT durum ikonu
  if (mqttConnected != prevMqttConnected) {
    tft.fillCircle(122, 4, 4, bgColor);
    tft.fillCircle(122, 4, 3, mqttConnected ? COLOR_OK : COLOR_DANGER);
    prevMqttConnected = mqttConnected;
  }

  // Kisi sayisi
  if (personCount != prevPersonCount) {
    tft.setTextSize(4);
    tft.setTextColor(personCount == 0 ? COLOR_EMPTY : COLOR_OCCUPIED, bgColor);
    tft.setCursor(40, 36);
    char countStr[4];
    snprintf(countStr, sizeof(countStr), "%2d", personCount);
    tft.print(countStr);

    tft.setTextSize(1);
    tft.setCursor(5, 70);
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
    tft.fillRect(60, 84, 68, 8, bgColor);
    tft.setTextSize(1);
    tft.setTextColor(getTemperatureColor(temperature), bgColor);
    tft.setCursor(60, 84);
    char tempStr[10];
    snprintf(tempStr, sizeof(tempStr), "%.1fC", temperature);
    tft.print(tempStr);
    prevTemperature = temperature;
  }

  // Nem
  if (abs(humidity - prevHumidity) > 0.5) {
    tft.fillRect(60, 98, 68, 8, bgColor);
    tft.setTextSize(1);
    tft.setTextColor(COLOR_VALUE, bgColor);
    tft.setCursor(60, 98);
    char humStr[10];
    snprintf(humStr, sizeof(humStr), "%.0f%%", humidity);
    tft.print(humStr);
    prevHumidity = humidity;
  }

  // Hava kalitesi
  if (airQuality != prevAirQuality) {
    tft.fillRect(60, 112, 68, 8, bgColor);
    tft.setTextSize(1);
    tft.setTextColor(getAirQualityColor(airQuality), bgColor);
    tft.setCursor(60, 112);
    char aqStr[12];
    snprintf(aqStr, sizeof(aqStr), "%dppm", airQuality);
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
  const int startY = 24;

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
  // Uyari (warning/danger) ekran rengini/sayfasini DEGISTIRMEZ; sadece sag
  // ust kosede renkli "!" ikonu gosterilir (bkz. drawHeader). Alert seviyesi
  // degisince sayfayi tam yeniden ciz ki kose ikonu guncellensin.
  if (currentAlert != prevAlert) {
    pageDirty = true;
    prevAlert = currentAlert;
  }

  switch (currentPage) {
    case PAGE_HOME:     renderHome(pageDirty);     break;
    case PAGE_SENSORS:  renderSensors(pageDirty);  break;
    case PAGE_NOW:      renderNow(pageDirty);      break;
    case PAGE_WEEK:     renderWeek(pageDirty);     break;
    case PAGE_ANNOUNCE: renderAnnounce(pageDirty); break;
    default: break;
  }

  // Sayfa altinda navigasyon noktalari (her sayfada)
  drawPaginationDots((uint8_t)currentPage, (uint8_t)PAGE_COUNT, rotationPaused);

  // Header saat/tarih - her tick'te tum sayfalarda guncel
  updateHeaderTime();

  // Tam yeniden cizimden sonra uyari ikonunu hemen geri getir (blink loop'ta surer)
  updateAlertIcon();

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

// Tek buton durum makinesi: debounce -> short / double / hold-repeat eventleri uretir.
// Pause toggle bu fonksiyonda DEGIL — checkBothButtonsPause()'da yapilir.
void pollButton(ButtonState& b) {
  bool raw = digitalRead(b.pin);
  uint32_t now = millis();

  if (raw != b.lastRaw) {
    b.lastChangeMs = now;
    b.lastRaw = raw;
  }

  // Debounce sonrasi stable durumu guncelle
  if ((now - b.lastChangeMs) > BUTTON_DEBOUNCE_MS && raw != b.stable) {
    b.stable = raw;
    if (b.stable == LOW) {
      // PRESS: yeni basis baslangici
      b.pressStartMs = now;
      b.repeatActive = false;
      b.lastRepeatMs = 0;
      // Onceki single basis double-window icindeyse → DOUBLE event
      if (b.pendingSingleMs != 0 &&
          (now - b.pendingSingleMs) <= BUTTON_DOUBLE_GAP_MS) {
        btnQueuePush(b.doubleEvent);
        b.pendingSingleMs = 0;
        b.repeatActive = true;  // bu basis double-parcasi, release SHORT emitleme
      } else {
        b.pendingSingleMs = 0;
      }
    } else {
      // RELEASE
      if (!b.repeatActive) {
        // Single basis bekle (ardindan double gelirse iptal)
        b.pendingSingleMs = now;
      }
      b.repeatActive = false;
    }
  }

  // Hold ile auto-repeat (ikisi-buton modunda devre disi).
  // lastRepeatMs == 0 sentinel: repeatActive=true ama henuz hold-fire olmamis demek.
  // Boylece double-press veya bothPause sonrasi parmak basili kalsa bile REPEAT firlatmaz.
  if (b.stable == LOW && !bothCurrentlyPressed) {
    if (!b.repeatActive) {
      if ((now - b.pressStartMs) >= BUTTON_HOLD_START_MS) {
        btnQueuePush(b.repeatEvent);
        b.repeatActive = true;
        b.lastRepeatMs = now;
      }
    } else if (b.lastRepeatMs != 0) {
      if ((now - b.lastRepeatMs) >= BUTTON_HOLD_INTERVAL) {
        btnQueuePush(b.repeatEvent);
        b.lastRepeatMs = now;
      }
    }
    // repeatActive=true && lastRepeatMs==0 → suppressed (double/bothPause)
  }

  // Pending single zamanasimi → SHORT emit (double gelmedi)
  if (b.pendingSingleMs != 0 &&
      (now - b.pendingSingleMs) > BUTTON_DOUBLE_GAP_MS) {
    btnQueuePush(b.shortEvent);
    b.pendingSingleMs = 0;
  }
}

// Ikisi-buton (NEXT+PREV) basili kalinca pause toggle.
// Single-button eventlerini bastirmak icin bothCurrentlyPressed flag set edilir.
void checkBothButtonsPause() {
  uint32_t now = millis();
  bool both = (btnNext.stable == LOW) && (btnPrev.stable == LOW);

  if (both) {
    if (bothHeldStartMs == 0) bothHeldStartMs = now;
    if (!bothPauseFired && (now - bothHeldStartMs) >= BUTTON_BOTH_PAUSE_MS) {
      rotationPaused = !rotationPaused;
      pageDirty = true;
      bothPauseFired = true;
      // Tek-buton bekleyen eventleri iptal et
      btnNext.pendingSingleMs = 0;
      btnPrev.pendingSingleMs = 0;
      btnNext.repeatActive = true;
      btnPrev.repeatActive = true;
      Serial.print("[Menu] Pause: ");
      Serial.println(rotationPaused ? "AKTIF" : "PASIF");
    }
  } else {
    bothHeldStartMs = 0;
    bothPauseFired = false;
  }
}

void pollButtons() {
  // bothCurrentlyPressed bayragi pollButton icindeki hold-repeat'i bastirir
  bothCurrentlyPressed = (btnNext.stable == LOW) && (btnPrev.stable == LOW);
  pollButton(btnNext);
  pollButton(btnPrev);
  checkBothButtonsPause();
}

void handleButtonEvent(BtnEvent ev) {
  switch (ev) {
    case BTN_NEXT_SHORT:
    case BTN_NEXT_REPEAT:
      changePage(+1);
      break;
    case BTN_PREV_SHORT:
    case BTN_PREV_REPEAT:
      changePage(-1);
      break;
    case BTN_NEXT_DOUBLE:
    case BTN_PREV_DOUBLE:
      // Cift bas: ana ekrana (HOME) hizli donus
      currentPage = PAGE_HOME;
      lastPageChangeMs = millis();
      pageDirty = true;
      updateDisplay();
      break;
    default:
      break;
  }
}

void processButtonEvents() {
  while (true) {
    BtnEvent e = btnQueuePop();
    if (e == BTN_NONE) break;
    handleButtonEvent(e);
  }
}

void handleAutoRotate() {
  if (rotationPaused) return;
  uint32_t dur = PAGE_DURATION_MS[(int)currentPage];
  if ((millis() - lastPageChangeMs) >= dur) {
    changePage(+1);
  }
}

void setupButtons() {
  pinMode(PIN_BTN_NEXT, INPUT_PULLUP);
  pinMode(PIN_BTN_PREV, INPUT_PULLUP);
  btnNext.lastRaw = btnNext.stable = digitalRead(PIN_BTN_NEXT);
  btnPrev.lastRaw = btnPrev.stable = digitalRead(PIN_BTN_PREV);
  btnQueueHead = btnQueueTail = 0;
  Serial.println("[Menu] Butonlar hazir: NEXT=GPIO25, PREV=GPIO14");
  Serial.println("[Menu] Kisa=ge, Cift=HOME, Basili tut=hizli ge, Ikisi-bas=pause");

  // Fiziksel duvar anahtarlari (LED/Cooling/Heating toggle)
  pinMode(PIN_BTN_LED,     INPUT_PULLUP);
  pinMode(PIN_BTN_COOLING, INPUT_PULLUP);
  pinMode(PIN_BTN_HEATING, INPUT_PULLUP);
  btnLedTog.lastReading     = btnLedTog.stable     = digitalRead(PIN_BTN_LED);
  btnCoolingTog.lastReading = btnCoolingTog.stable = digitalRead(PIN_BTN_COOLING);
  btnHeatingTog.lastReading = btnHeatingTog.stable = digitalRead(PIN_BTN_HEATING);
  Serial.println("[Switch] Duvar anahtarlari: LED=GPIO5 COOLING=GPIO16 HEATING=GPIO19");
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

// Sag ust kosedeki yanip sonen uyari ikonu ("!"). Loop'ta sik cagrilir
// (updateAlertLED ile ayni cadansta), boylece ikon blink eder.
// alertBlinkState updateAlertLED() tarafindan toggle edilir.
// Warning = turuncu, Danger = kirmizi. Ekranin geri kalanina dokunmaz.
void updateAlertIcon() {
  bool active = (currentAlert == ALERT_WARNING || currentAlert == ALERT_DANGER);
  int desired = (active && alertBlinkState) ? 1 : 0;
  if (desired == alertIconShown) return;   // degisim yok -> bos yere cizme

  alertIconShown = desired;
  if (desired == 1) {
    tft.setTextSize(1);
    tft.setTextColor((currentAlert == ALERT_DANGER) ? COLOR_DANGER : TFT_ORANGE, COLOR_BG);
    tft.setCursor(112, 1);
    tft.print("!");
  } else {
    tft.fillRect(112, 1, 8, 8, COLOR_BG);  // ikonu sil
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
  prefs.end();

  broker.toCharArray(MQTT_BROKER,   sizeof(MQTT_BROKER));
  sinifId.toCharArray(CLASSROOM_ID, sizeof(CLASSROOM_ID));

  // MQTT Client ID: sinif ID'sinden otomatik uret
  mqttClientId = "esp32-plc-" + sinifId;

  // OTA status topic'i guncelle
  otaStatusTopic = "akilli-sinif/" + sinifId + "/status/ota";

  Serial.println("[Config] Sinif ID: " + sinifId);
  Serial.println("[Config] MQTT Broker: " + broker);
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
// RUNTIME PORTAL TETIKLEYICI (loop() icinden)
// ============================================
// Cihaz acikken BOOT (GPIO0) 5 sn basili tutulursa:
//   - TFT'de buyuk geri sayim 5..1 gosterir
//   - NVS'e "force_portal" bayragi yazar
//   - Cihaz yeniden baslar, setupWiFi() WiFi portal acar
// Bayrak yaklasiminin avantaji: sinif_id / mqtt_broker
// silinmez — portal mevcut degerleri gosterir, kullanici sadece
// degistirmek istedigini gunceller.
unsigned long bootBtnPressStartMs   = 0;
bool          bootCountdownActive   = false;
int           lastBootCountdownShown = -1;

void checkRuntimePortalRequest() {
  bool pressed = (digitalRead(CONFIG_RESET_PIN) == LOW);

  // DEBUG: her press/release gecisinde Serial'a log
  static bool wasPressed = false;
  if (pressed != wasPressed) {
    Serial.printf("[Portal] BOOT pini %s (millis=%lu)\n",
                  pressed ? "BASILI (LOW)" : "BIRAKILDI (HIGH)", millis());
    wasPressed = pressed;
  }

  if (!pressed) {
    if (bootCountdownActive) {
      pageDirty = true;  // iptal -> normal sayfa yeniden cizilsin
      bootCountdownActive = false;
    }
    bootBtnPressStartMs    = 0;
    lastBootCountdownShown = -1;
    return;
  }

  unsigned long now = millis();
  if (bootBtnPressStartMs == 0) {
    bootBtnPressStartMs = now;
    Serial.println("[Portal] press timer baslatildi");
    return;
  }

  unsigned long held = now - bootBtnPressStartMs;
  if (held < 400) return;  // 400 ms'den kisa basislari yoksay (yanlislikla)

  if (!bootCountdownActive) {
    bootCountdownActive    = true;
    lastBootCountdownShown = -1;
    tft.fillScreen(COLOR_BG);
    tft.setTextSize(1);
    tft.setTextColor(COLOR_WARNING, COLOR_BG);
    tft.setCursor(5, 6);
    tft.println("WEB PORTAL");
    tft.setCursor(5, 18);
    tft.println("aciliyor...");
    tft.setTextColor(COLOR_LABEL, COLOR_BG);
    tft.setCursor(5, 110);
    tft.println("BOOT'u birak");
    tft.setCursor(5, 120);
    tft.println("=> iptal");
  }

  int remaining = (int)((RESET_HOLD_MS - held) / 1000) + 1;
  if (remaining < 1) remaining = 1;
  if (remaining > 5) remaining = 5;

  if (remaining != lastBootCountdownShown) {
    tft.fillRect(0, 40, 128, 60, COLOR_BG);
    tft.setTextSize(6);
    tft.setTextColor(COLOR_DANGER, COLOR_BG);
    tft.setCursor(46, 48);
    tft.print(remaining);
    lastBootCountdownShown = remaining;
  }

  if (held >= RESET_HOLD_MS) {
    // Bayragi yaz + cihazi yeniden baslat
    prefs.begin("akilli-sinif", false);
    prefs.putBool("force_portal", true);
    prefs.end();

    tft.fillScreen(COLOR_BG);
    tft.setTextSize(1);
    tft.setTextColor(COLOR_OK, COLOR_BG);
    tft.setCursor(5, 25);
    tft.println("WiFi PORTAL");
    tft.setCursor(5, 40);
    tft.println("aciliyor...");
    tft.setTextColor(COLOR_LABEL, COLOR_BG);
    tft.setCursor(5, 70);
    tft.println("Yeniden");
    tft.setCursor(5, 82);
    tft.println("basliyor...");

    Serial.println("[Portal] Runtime BOOT 5sn -> portal modunda yeniden baslatiliyor");
    delay(1500);
    ESP.restart();
  }
}

// ============================================
// WIFI FONKSIYONLARI (WiFiManager)
// ============================================

// MAC'in son 3 byte'indan benzersiz AP sifresi uretir.
// Ornek: MAC = AA:BB:CC:11:22:33  →  "akilli-112233"
// Public repo guvenli: kod sir icermez, sifre cihaz donanimina bagli.
String makeApPassword() {
  uint8_t mac[6];
  WiFi.macAddress(mac);
  char buf[16];
  snprintf(buf, sizeof(buf), "akilli-%02X%02X%02X", mac[3], mac[4], mac[5]);
  return String(buf);
}

void setupWiFi() {
  Serial.println("\n========================================");
  Serial.println("WiFiManager Baslatiliyor...");
  Serial.println("========================================");

  WiFiManager wm;

  // ── GUVENLIK: WPA2 AP sifresi (MAC turevli) ile portal korumali
  String apPass = makeApPassword();
  Serial.println("[WiFi] AP SSID: Akilli-Sinif-Setup");
  Serial.println("[WiFi] AP/Portal WPA2 Sifre: " + apPass);

  // TFT'de SSID + sifreyi goster (kurulumcu okuyabilsin)
  tft.fillScreen(COLOR_BG);
  tft.setTextColor(COLOR_HEADER, COLOR_BG);
  tft.setTextSize(1);
  tft.setCursor(5, 10);  tft.println("WiFi Kurulum");
  tft.setTextColor(COLOR_LABEL, COLOR_BG);
  tft.setCursor(5, 30);  tft.println("AP: Akilli-Sinif");
  tft.setCursor(5, 42);  tft.println("    -Setup");
  tft.setCursor(5, 60);  tft.println("WPA2 Sifre:");
  tft.setTextColor(COLOR_WARNING, COLOR_BG);
  tft.setCursor(5, 75);  tft.println(apPass);
  tft.setTextColor(COLOR_LABEL, COLOR_BG);
  tft.setCursor(5, 95);  tft.println("Bagland.sonra");
  tft.setCursor(5, 107); tft.println("portal acilir");

  // Portal'da ekstra alanlar (WiFi den ayri config)
  WiFiManagerParameter p_mqttBroker("mqtt_broker", "MQTT Broker IP",
                                     MQTT_BROKER, 39);
  WiFiManagerParameter p_classroomId("classroom_id", "Sinif ID (sinif-1, sinif-2...)",
                                      CLASSROOM_ID, 19);

  wm.addParameter(&p_mqttBroker);
  wm.addParameter(&p_classroomId);

  // Portal AP adi: "Akilli-Sinif-Setup"
  // Baglandiktan sonra 192.168.4.1 otomatik acar
  wm.setConfigPortalTimeout(180);  // 3 dakika icinde baglanilmazsa devam et

  // ESP32 cold-boot'ta ILK WiFi baglanti denemesi sik sik gecici patlar
  // (RF/scan henuz hazir degil). Retry vermezsek autoConnect direkt portal'a
  // dusuyordu -> kullanici elle 2. reset atinca baglaniyordu. Asagisi onu
  // otomatiklestirir: her deneme 15s, 3 kez dene; cogu zaman 2. denemede baglanir.
  wm.setConnectTimeout(15);   // her baglanti denemesi icin 15s bekle
  wm.setConnectRetries(3);    // portal'a dusmeden once 3 kez dene

  // NVS'teki force_portal bayragini oku (runtime BOOT 5sn ile set edilir)
  prefs.begin("akilli-sinif", false);
  bool forcePortalFlag = prefs.getBool("force_portal", false);
  if (forcePortalFlag) prefs.remove("force_portal");  // one-shot
  prefs.end();

  // Portal acma sartlari:
  //  - NVS bayragi (runtime BOOT 5sn)
  //  - Config sifirlandi / hic kayit yok (broker IP bos)
  //  - Acilista BOOT basili
  bool forcePortal = forcePortalFlag
                     || (strlen(MQTT_BROKER) == 0)
                     || (digitalRead(CONFIG_RESET_PIN) == LOW);

  bool connected;
  if (forcePortal) {
    Serial.println("[WiFi] Portal modu - baglanmadan once config gerekli.");
    connected = wm.startConfigPortal("Akilli-Sinif-Setup", apPass.c_str());
  } else {
    connected = wm.autoConnect("Akilli-Sinif-Setup", apPass.c_str());
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

  // Bos gelmisse mevcut degerleri koru
  if (newBroker.length() > 0 || newSinifId.length() > 0) {
    prefs.begin("akilli-sinif", false);
    if (newBroker.length()  > 0) prefs.putString("mqtt_broker",   newBroker);
    if (newSinifId.length() > 0) prefs.putString("classroom_id",  newSinifId);
    prefs.end();

    // Degiskenleri guncelle
    newBroker.toCharArray(MQTT_BROKER,   sizeof(MQTT_BROKER));
    newSinifId.toCharArray(CLASSROOM_ID, sizeof(CLASSROOM_ID));
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

// Non-blocking: WiFi.reconnect() asenkron, durum sonraki WIFI_CHECK_INTERVAL'da okunur.
// Eskiden delay(5000) ile loop'u bloke ediyordu.
void checkWiFi() {
  wl_status_t st = WiFi.status();
  if (st != WL_CONNECTED) {
    if (wifiConnected) {
      Serial.println("[WiFi] Baglanti koptu! reconnect tetiklendi.");
      g_wifiDownAtMs = millis();  // 3.1 olcum: kopma anini kaydet
    }
    wifiConnected = false;
    WiFi.reconnect();
  } else if (!wifiConnected) {
    wifiConnected = true;
    Serial.println("[WiFi] Yeniden baglandi.");
    // logRemote burada calismaz: mqttConnected hala false.
    // Reconnect olcumu MQTT geri donunce connectMQTT() icinde yayinlanir.
  }
}

// ============================================
// MQTT FONKSIYONLARI
// ============================================

// MQTT mesaj callback fonksiyonu
// Bu fonksiyon, subscribe olunan topic'lere mesaj geldiginde cagrilir
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  // 3.1 olcum: MQTT komut alis timestamp'i (actuator cevap gecikmesi icin baz)
  unsigned long _cb_t0 = millis();

  // Gelen mesaji string'e cevir
  String message = "";
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  // ÖNEMLI: topic'i HEMEN kopyala. PubSubClient TEK bir dahili buffer kullanir;
  // 'topic' ve 'payload' o buffer'i isaret eder. Callback icinden publish edince
  // (asagidaki logRemote ve branch'lerdeki status yayinlari) ayni buffer'a yazilir
  // ve 'topic' isaretcisi BOZULUR. Once String'e kopyalamazsak topicStr cop olur,
  // hicbir /control/... branch'i eslesmez -> tum komutlar sessizce yutulur.
  // (v1.3.3'te logRemote eklenince giren regresyon.)
  String topicStr = String(topic);

  Serial.println("\n>>> MQTT MESAJ ALINDI <<<");
  Serial.print("Topic: ");
  Serial.println(topicStr);
  Serial.print("Mesaj: ");
  Serial.println(message);

  // Uzaktan log (Wi-Fi uzeri Serial yerine) — artik guvenli kopya kullaniyoruz
  logRemote("CMD rx topic=%s len=%u", topicStr.c_str(), length);

  // JSON olarak parse et (1 KB — haftalik plan icin buyuk)
  DynamicJsonDocument doc(1024);
  DeserializationError error = deserializeJson(doc, message);

  if (error) {
    Serial.print("JSON Parse Hatasi: ");
    Serial.println(error.c_str());
    return;
  }

  // Topic'e gore islem yap (topicStr yukarida, publish'ten ONCE kopyalandi)

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
      if (brightness > 0) lastLedBrightness = brightness;  // fiziksel toggle icin yedek
      setLedBrightness(brightness);
      Serial.print("LED Parlakligi ayarlandi: ");
      Serial.print(brightness);
      Serial.println("%");
    }
    if (doc.containsKey("state")) {
      const char* state = doc["state"];
      if (strcmp(state, "on") == 0) {
        setLedBrightness(lastLedBrightness > 0 ? lastLedBrightness : 100);
        Serial.println("LED ACILDI");
      } else if (strcmp(state, "off") == 0) {
        if (ledBrightness > 0) lastLedBrightness = ledBrightness;  // kapanmadan once yedekle
        setLedBrightness(0);
        Serial.println("LED KAPATILDI");
      }
    }
    logRemote("LAT cmd->led dt=%lu ms", millis() - _cb_t0);
  }

  // Cooling Role Kontrolu (12V DC fan, BC337 + JRC-19F)
  // Topic: akilli-sinif/sinif-1/control/cooling   payload: {"state":"on"|"off"}
  else if (topicStr.endsWith("/control/cooling")) {
    bool on;
    if (parseOnOff(doc, on)) {
      manualOverrideUntil = millis() + MANUAL_OVERRIDE_MS;
      setCooling(on);
      logRemote("LAT cmd->cooling state=%s dt=%lu ms", on?"on":"off", millis() - _cb_t0);
    }
  }

  // Heating Role Kontrolu (22ohm 5W direnc, BC337 + JRC-19F)
  // Topic: akilli-sinif/sinif-1/control/heating   payload: {"state":"on"|"off"}
  else if (topicStr.endsWith("/control/heating")) {
    bool on;
    if (parseOnOff(doc, on)) {
      manualOverrideUntil = millis() + MANUAL_OVERRIDE_MS;
      setHeating(on);
      logRemote("LAT cmd->heating state=%s dt=%lu ms", on?"on":"off", millis() - _cb_t0);
    }
  }

  // Otomasyon mod degistirme (auto ladder vs sadece MQTT)
  // Topic: akilli-sinif/sinif-1/control/mode   payload: {"auto":true|false}
  else if (topicStr.endsWith("/control/mode")) {
    if (doc.containsKey("auto")) {
      autoMode = doc["auto"].as<bool>();
      Serial.print("[Mode] autoMode=");
      Serial.println(autoMode ? "true" : "false");
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
  mqttClient.setSocketTimeout(2);  // Default 15 sn -> 2 sn: broker erisilemezse loop bloke olmasin

  // OTA manager'i basklat (MQTT client hazir olduktan sonra)
  if (otaManager != nullptr) delete otaManager;
  otaManager = new OTAManager(mqttClient, otaStatusTopic);
  Serial.println("[OTA] OTA Manager hazir. Firmware: v" + OTAManager::getCurrentVersion());
}

// Non-blocking: tek deneme yapar, loop() MQTT_RECONNECT_INTERVAL (5sn) ile tekrar cagirir.
// Eskiden while+delay(3000) ile 15 sn'ye kadar loop'u bloke ediyordu —
// bu sirada BOOT/menu butonlari okunamiyordu.
void connectMQTT() {
  Serial.print("[MQTT] Baglanti denemesi (broker=");
  Serial.print(MQTT_BROKER);
  Serial.print("): ");

  String willTopic   = buildTopic("status", "connection");
  String willMessage = "{\"status\":\"offline\",\"device\":\"" + mqttClientId + "\"}";

  if (mqttClient.connect(mqttClientId.c_str(), MQTT_USER, MQTT_PASSWORD,
                         willTopic.c_str(), 1, true, willMessage.c_str())) {
    mqttConnected = true;
    reconnectAttempts = 0;
    Serial.println("BAGLANDI!");

    pageDirty = true;
    subscribeToControlTopics();
    publishStatus("online");

    // 3.1 olcum: WiFi kopma → MQTT geri donme toplam suresi (varsa)
    if (g_wifiDownAtMs != 0) {
      logRemote("WIFI reconnect downtime=%lu ms", millis() - g_wifiDownAtMs);
      g_wifiDownAtMs = 0;
    }
    logRemote("MQTT connected attempts=%d", reconnectAttempts);
  } else {
    reconnectAttempts++;
    Serial.printf("BASARISIZ (hata=%d, deneme=%d)\n",
                  mqttClient.state(), reconnectAttempts);
    // Hata kodlari: -4 TIMEOUT, -3 LOST, -2 FAILED, -1 DISCONNECTED
    //                1 BAD_PROTOCOL, 2 BAD_CLIENT, 3 UNAVAILABLE, 4 BAD_CREDS, 5 UNAUTHORIZED
  }
}

void subscribeToControlTopics() {
  // Kontrol topic'lerine abone ol
  String sinifBase = String("akilli-sinif/") + CLASSROOM_ID;
  String topics[] = {
    buildTopic("control", "led"),
    buildTopic("control", "cooling"),      // Cooling role (12V fan)
    buildTopic("control", "heating"),      // Heating role (22ohm direnc)
    buildTopic("control", "mode"),         // Auto/manual otomasyon mod
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

// LM358 cikisindaki ham ADC degerini akima (A) cevirir.
// pin voltaji = raw * Vref/4095, akim = voltaj * AMPS_PER_VOLT (1.5A @ 3V orani).
float rawToAmps(int raw) {
  float pinVolts = raw * (ADC_VREF / (float)ADC_MAX);
  float a = pinVolts * AMPS_PER_VOLT;
  if (a < 0.0f) a = 0.0f;
  return a;
}

void readSensors() {
  // Mock modu kaldirildi — PLC her zaman gercek sensorleri okur.
  // Simulasyon gerekiyorsa ayri esp32-simulator firmware'i kullanilir.
  readRealSensors();
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
  // MQ-135 isindiktan sonra (1-2 dakika) dogru deger verir.
  // NOT: MQ-135 cikisi gaz konsantrasyonuyla LOGARITMIK iliskidedir.
  // Asagidaki lineer map sadece TREND amaclidir, mutlak ppm degeri verir.
  // Bitirme tezinde "kalibre edilmemis, gosterge amacli" olarak belirt.
  int rawAir = analogRead(PIN_MQ135);
  airQuality = map(rawAir, 0, 4095, 0, 500);

  // ========== Shunt + LM358 - Akim ve Guc ==========
  // 0.1 ohm shunt (low-side), LM358 evirici amp (gain ~20)
  // 1.5A -> ~3.0V ADC. Piecewise-linear kalibrasyon ile A'ya cevriliyor.
  // Dusuk sinyalde ADC gurultusu yuksek -> 16 ornek ortalama.
  uint32_t adcSum = 0;
  for (int i = 0; i < 16; i++) {
    adcSum += analogRead(PIN_CURRENT);
  }
  rawCurrent = (int)(adcSum / 16);
  currentAmps = rawToAmps(rawCurrent);
  powerWatts = SUPPLY_VOLTAGE * currentAmps;

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
  // LED: PWM dimming (ESP32 Core 3.x: ledcAttach kanal otomatik atar)
  ledcAttach(PIN_LED, PWM_FREQ, PWM_RESOLUTION);
  setLedBrightness(0);

  // Cooling/Heating roleleri: digital ON/OFF, GUVENLI baslangic KAPALI
  pinMode(PIN_COOLING, OUTPUT);
  pinMode(PIN_HEATING, OUTPUT);
  digitalWrite(PIN_COOLING, !RELAY_ACTIVE_LEVEL);
  digitalWrite(PIN_HEATING, !RELAY_ACTIVE_LEVEL);

  Serial.println("Aktuatorler hazir (LED PWM, cooling/heating role).");
}

void setupSensors() {
  // DHT11 sensoru baslat
  dht.begin();
  Serial.println("DHT11 sensoru baslatildi");
  
  // PIR sensoru - dijital giris
  pinMode(PIN_PIR, INPUT_PULLDOWN);  // takili degilken LOW -> floating yanlis hareketi onlenir
  Serial.println("PIR sensoru baslatildi");
  
  // Reed switch - dahili pull-up ile dijital giris
  // Kapali = LOW (magnet yakin), Acik = HIGH (magnet uzak)
  pinMode(PIN_WINDOW, INPUT_PULLUP);
  Serial.println("Pencere sensoru (Reed Switch) baslatildi");
  
  // LDR ve MQ-135 analog pinler - ayar gerekmez (analogRead otomatik)
  // Ancak ADC cozunurlugunu ayarlayabiliriz
  analogReadResolution(12);  // 12-bit (0-4095)
  // Akim sensoru (LM358 cikisi) icin 0-3.3V tam aralik. Core 3.x'te default
  // attenuation bazi surumlerde degisik geliyor, explicit set garanti.
  analogSetPinAttenuation(PIN_CURRENT, ADC_11db);
  analogSetPinAttenuation(PIN_LDR, ADC_11db);
  analogSetPinAttenuation(PIN_MQ135, ADC_11db);
  Serial.println("Analog sensorler (LDR, MQ-135, Current) hazir");

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

// Role durumu yayinla (cooling/heating ortak format: {state:"on"|"off"})
// retained=true: broker son durumu tutar, dashboard ve ESP32 reboot sonrasi senkron.
void publishRelayStatus(const char* actuator, bool on) {
  StaticJsonDocument<96> doc;
  doc["state"] = on ? "on" : "off";
  doc["timestamp"] = millis();
  String topic = buildTopic("status", actuator);
  publishJson(topic, doc, true);
}

// Cooling role ON/OFF (BC337 + JRC-19F coil + 12V fan)
void setCooling(bool on) {
  digitalWrite(PIN_COOLING, on ? RELAY_ACTIVE_LEVEL : !RELAY_ACTIVE_LEVEL);
  coolingOn = on;
  Serial.print("[Role] cooling ");
  Serial.println(on ? "ON" : "OFF");
  publishRelayStatus("cooling", on);
}

// Heating role ON/OFF (BC337 + JRC-19F coil + 22ohm 5W direnc)
void setHeating(bool on) {
  digitalWrite(PIN_HEATING, on ? RELAY_ACTIVE_LEVEL : !RELAY_ACTIVE_LEVEL);
  heatingOn = on;
  Serial.print("[Role] heating ");
  Serial.println(on ? "ON" : "OFF");
  publishRelayStatus("heating", on);
}

// ============================================
// FIZIKSEL DUVAR ANAHTARLARI (TOGGLE)
// ============================================

void toggleLed() {
  if (ledBrightness > 0) {
    lastLedBrightness = ledBrightness;  // mevcut parlakligi yedekle
    setLedBrightness(0);
    Serial.println("[Switch] LED -> OFF");
  } else {
    setLedBrightness(lastLedBrightness > 0 ? lastLedBrightness : 100);
    Serial.print("[Switch] LED -> ON @");
    Serial.print(ledBrightness);
    Serial.println("%");
  }
}

void toggleCooling() {
  manualOverrideUntil = millis() + MANUAL_OVERRIDE_MS;  // ladder otomasyonu ezmesin
  setCooling(!coolingOn);
  Serial.print("[Switch] COOLING -> ");
  Serial.println(coolingOn ? "ON" : "OFF");
}

void toggleHeating() {
  manualOverrideUntil = millis() + MANUAL_OVERRIDE_MS;
  setHeating(!heatingOn);
  Serial.print("[Switch] HEATING -> ");
  Serial.println(heatingOn ? "ON" : "OFF");
}

// Tek butonu polling + debounce ile oku. HIGH -> LOW gecisinde true doner (basma ani).
bool pollToggleEdge(ToggleBtn& b) {
  bool raw = digitalRead(b.pin);
  unsigned long now = millis();
  if (raw != b.lastReading) {
    b.lastReading = raw;
    b.lastChangeMs = now;
    return false;
  }
  if ((now - b.lastChangeMs) < TOGGLE_DEBOUNCE_MS) return false;
  if (raw == b.stable) return false;
  bool edge = (b.stable == HIGH && raw == LOW);  // basma kenari
  b.stable = raw;
  return edge;
}

void updateToggleButtons() {
  if (pollToggleEdge(btnLedTog))     toggleLed();
  if (pollToggleEdge(btnCoolingTog)) toggleCooling();
  if (pollToggleEdge(btnHeatingTog)) toggleHeating();
}

// Ladder otomasyon: sicaklik esikleri + pencere kilidi + manuel override
// loop()'ta her sensor okumasindan sonra cagrilir.
void runLadder() {
  if (!autoMode) return;
  if (millis() < manualOverrideUntil) return;  // manuel komut sonrasi 5 dk bypass

  // Reed switch: HIGH = pencere acik (mıknatis uzak), LOW = kapali
  bool windowOpen = (digitalRead(PIN_WINDOW) == HIGH);

  // Pencere acikken ikisini de zorla KAPAT (enerji kacagi koruma)
  if (windowOpen) {
    if (coolingOn) setCooling(false);
    if (heatingOn) setHeating(false);
    return;
  }

  // Cooling: histeresis (>26 -> ON, <24 -> OFF). Heating ile karsilikli dislama.
  if (!coolingOn && temperature > TEMP_COOL_ON && !heatingOn) {
    setCooling(true);
  } else if (coolingOn && temperature < TEMP_COOL_OFF) {
    setCooling(false);
  }

  // Heating: histeresis (<20 -> ON, >22 -> OFF). Cooling ile karsilikli dislama.
  if (!heatingOn && temperature < TEMP_HEAT_ON && !coolingOn) {
    setHeating(true);
  } else if (heatingOn && temperature > TEMP_HEAT_OFF) {
    setHeating(false);
  }
}

// MQTT control payload'undan on/off ayikla. {"state":"on"/"off"} veya {"state":true/false}
bool parseOnOff(JsonDocument& doc, bool& out) {
  if (!doc.containsKey("state")) return false;
  if (doc["state"].is<const char*>()) {
    const char* s = doc["state"];
    if (strcmp(s, "on")  == 0) { out = true;  return true; }
    if (strcmp(s, "off") == 0) { out = false; return true; }
    return false;
  }
  if (doc["state"].is<bool>()) {
    out = doc["state"].as<bool>();
    return true;
  }
  return false;
}

// ============================================
// MQTT PUBLISH FONKSIYONLARI
// ============================================

void publishSensorData() {
  // 3.1 olcum: sensor okuma + MQTT yayin toplam gecikmesi
  unsigned long _pub_t0 = millis();

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

  // Akim (mA)
  StaticJsonDocument<128> currDoc;
  currDoc["value"] = currentAmps * 1000.0f;
  currDoc["raw"] = rawCurrent;
  currDoc["unit"] = "mA";
  currDoc["timestamp"] = millis();
  publishJson(buildTopic("sensors", "current"), currDoc);

  // Guc (W)
  StaticJsonDocument<128> pwrDoc;
  pwrDoc["value"] = powerWatts;
  pwrDoc["unit"] = "W";
  pwrDoc["timestamp"] = millis();
  publishJson(buildTopic("sensors", "power"), pwrDoc);

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
  Serial.print(personCount);
  Serial.print(" | Akim: ");
  Serial.print(currentAmps * 1000.0f, 0);
  Serial.print(" mA (raw=");
  Serial.print(rawCurrent);
  Serial.print(", ~");
  Serial.print(rawCurrent * (ADC_VREF / (float)ADC_MAX), 3);
  Serial.print("V) | Guc: ");
  Serial.print(powerWatts, 2);
  Serial.println(" W");

  // 3.1 olcum: tum sensor JSON publish'lerinin toplam suresi
  logRemote("PUB sensors dt=%lu ms", millis() - _pub_t0);
}

void publishActuatorStatus(const char* actuator, int value) {
  StaticJsonDocument<128> doc;
  doc["brightness"] = value;
  doc["unit"] = "%";
  doc["timestamp"] = millis();

  String topic = buildTopic("status", actuator);
  publishJson(topic, doc, true);  // retained: dashboard ve reboot sonrasi son durum senkron
}

void publishStatus(const char* status) {
  StaticJsonDocument<256> doc;
  doc["status"] = status;
  doc["device"]           = mqttClientId;
  doc["classroom"]        = CLASSROOM_ID;
  doc["ip"]               = WiFi.localIP().toString();
  doc["rssi"]             = WiFi.RSSI();
  doc["uptime"]           = millis() / 1000;
  doc["mock_mode"]        = false;  // PLC her zaman gercek donanim (Node-RED uyumu icin alan korunuyor)
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
// UZAKTAN DEBUG LOG (Wi-Fi uzeri Serial Monitor yerine)
// ============================================
// Topic: akilli-sinif/<sinif_id>/debug/log
// Dinle: mosquitto_sub -h <broker> -t 'akilli-sinif/+/debug/log' -v
// Tez 3.1 olcumlerinin gercek donanim kanitlari icin kullanilir.
void logRemote(const char* fmt, ...) {
  if (!mqttConnected) return;  // baglanti yokken yutar (UART log devam eder)
  char msg[200];
  va_list args;
  va_start(args, fmt);
  vsnprintf(msg, sizeof(msg), fmt, args);
  va_end(args);

  StaticJsonDocument<256> doc;
  doc["t_ms"]   = millis();
  doc["dev"]    = mqttClientId;
  doc["msg"]    = msg;
  publishJson(buildTopic("debug", "log"), doc);
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
    String broker  = prefs.getString("mqtt_broker", "");
    prefs.end();
    sinifId.toCharArray(CLASSROOM_ID, sizeof(CLASSROOM_ID));
    broker.toCharArray(MQTT_BROKER, sizeof(MQTT_BROKER));
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
  Serial.println("GERCEK DONANIM");
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

  // BOOT (GPIO0) 5 sn basili -> TFT countdown + WiFi portal acmak icin restart
  checkRuntimePortalRequest();

  // Menu butonlari + otomatik sayfa cevrimi (her iterasyonda, gecikmesiz)
  pollButtons();
  processButtonEvents();
  handleAutoRotate();

  // Fiziksel duvar anahtarlari (LED/Cooling/Heating toggle)
  updateToggleButtons();

  // WiFi kontrolu
  if (currentMillis - lastWifiCheck >= WIFI_CHECK_INTERVAL) {
    lastWifiCheck = currentMillis;
    checkWiFi();
  }
  
  // MQTT baglantisini kontrol et ve gerekirse yeniden baglan
  if (!mqttClient.connected()) {
    mqttConnected = false;

    // WiFi yoksa MQTT denemeye gerek yok (bos yere socket timeout bekletmesin)
    if (WiFi.status() == WL_CONNECTED &&
        currentMillis - lastMqttReconnectAttempt >= MQTT_RECONNECT_INTERVAL) {
      lastMqttReconnectAttempt = currentMillis;
      connectMQTT();
    }
  }
  mqttClient.loop();  // Gelen mesajlari isle
  
  // Sensor okuma + ladder otomasyon (sicaklik+pencere -> cooling/heating)
  if (currentMillis - lastSensorRead >= SENSOR_INTERVAL) {
    lastSensorRead = currentMillis;
    readSensors();
    runLadder();
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
  
  // Ekran guncellemesi (BOOT countdown ekranini bozmamak icin gate'li)
  if (currentMillis - lastDisplayUpdate >= DISPLAY_INTERVAL) {
    lastDisplayUpdate = currentMillis;
    if (!bootCountdownActive) updateDisplay();
    checkAutoAlerts();  // Otomatik uyari kontrolu
  }
  
  // Uyari LED guncellemesi (surekli calistir)
  updateAlertLED();

  // Sag ust uyari ikonunu yanip sondur (LED ile ayni cadans)
  updateAlertIcon();
}
