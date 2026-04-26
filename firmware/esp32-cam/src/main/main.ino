/*
 * Akilli Sinif Sistemi - ESP32-CAM Firmware
 * ==========================================
 *
 * Bu kod ESP32-CAM'i kullanarak:
 * - Periyodik olarak foto ceker
 * - HTTP POST ile Python sunucusuna gonderir
 * - Sunucu YOLOv8 ile kisi sayar ve MQTT'ye yayinlar
 * - WiFiManager ile guvenli WiFi kurulumu (AP portal)
 * - OTA (Over-the-Air) firmware guncelleme destegi
 * - Boot'ta IP adresini MQTT'ye yayinlar
 * - HTTP /reset-config endpoint'i ile kutudan cikmadan config sifirlama
 * - MQTT config guncelleme: api_key, server_url aninda degistirilebilir
 *   Topic: akilli-sinif/{sinif-id}/control/config
 *   Payload: {"api_key":"yeni-key"} veya {"server_url":"http://IP:5000/analyze"}
 * - WiFi degisince otomatik portal acar (~50 sn sonra)
 *
 * ILK KURULUM:
 *   1. Firmware'i yak
 *   2. "Akilli-CAM-Setup" WiFi agina baglan
 *   3. Acilan portala gir: WiFi + Sunucu IP + Sinif ID ayarla
 *   4. Kaydet — ESP32-CAM yeniden baslar
 *
 * CONFIG SIFIRLAMA:
 *   GPIO0 butonuna 5 sn bas → NVS silinir → Portal tekrar acar
 *   VEYA tarayıcıdan: http://<ESP32-IP>/reset-config (HTTP Basic Auth ister)
 *   (IP adresini MQTT topic'inden öğren:
 *    akilli-sinif/<sinif-id>/status/ip)
 *
 * AP SIFRESI:
 *   "Akilli-CAM-Setup" agi WPA2 korumali. Sifre cihaz MAC'inden turetilir
 *   → "akilli-XXXXXX". Boot anında Serial monitor'de yazilir.
 *   /reset-config endpoint ve portal HTTP Basic Auth ister:
 *   kullanici "admin", sifre yine ayni.
 *
 * Donanim: AI-Thinker ESP32-CAM
 *
 * Yazar: Akilli Sinif Projesi
 * Tarih: 2026
 */

#include "esp_camera.h"
#include <WiFi.h>
#include <WiFiManager.h>     // tzapu/WiFiManager
#include <Preferences.h>     // NVS
#include <HTTPClient.h>
#include <WiFiClientSecure.h> // OTA HTTPS icin
#include <WebServer.h>       // /reset-config endpoint icin
#include <PubSubClient.h>    // MQTT - IP yayinlamak icin
#include <ArduinoJson.h>
#include <Update.h>          // OTA icin
#include "../../../version.h"   // Firmware versiyon
#include "../../../secrets.h"   // MQTT user/sifre — repo'ya commit'lenmez

// ============================================
// YAPILANDIRMA - NVS'DEN OKUNUR
// ============================================

Preferences prefs;

char SERVER_URL[80]    = "";
char CLASSROOM_ID[20]  = "sinif-1";
char API_KEY[64]       = "";

// MQTT ayarları (IP yayınlamak için — config.json ile aynı değerler)
char MQTT_BROKER[40] = "192.168.1.100";  // Portal'dan ayarlanır
char MQTT_USER[20]   = MQTT_USER_DEFAULT;       // secrets.h
char MQTT_PASS[20]   = MQTT_PASSWORD_DEFAULT;   // secrets.h

// Config sifirlama
const int CONFIG_RESET_PIN        = 0;  // GPIO0 = BOOT butonu
const unsigned long RESET_HOLD_MS = 5000;

// WebServer (reset-config endpoint)
WebServer webServer(80);

// MQTT client
WiFiClient wifiClientMQTT;
PubSubClient mqttClient(wifiClientMQTT);

// ============================================
// KAMERA AYARLARI
// ============================================

const int CAPTURE_INTERVAL_ACTIVE = 10000;   // Hareket varken 10 saniye
const int CAPTURE_INTERVAL_IDLE   = 60000;   // Hareket yokken 60 saniye
int captureInterval = CAPTURE_INTERVAL_ACTIVE;

// ============================================
// AI-THINKER ESP32-CAM PIN TANIMLARI
// ============================================

#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// Flash LED
#define FLASH_GPIO_NUM     4

// ============================================
// GLOBAL DEGISKENLER
// ============================================

unsigned long lastCapture = 0;
bool cameraReady = false;
int consecutiveFailures = 0;
const int MAX_FAILURES = 5;

// WiFi yeniden bağlanma sayacı (otomatik portal için)
int wifiFailCount = 0;
const int WIFI_FAIL_PORTAL_THRESHOLD = 10;  // 10 x 5sn = ~50sn sonra portal acar

// AP adı (setupWiFi ve reconnectWiFi aynı ismi kullanır)
String camApName = "Akilli-CAM-Setup";

// ============================================
// KAMERA FONKSIYONLARI
// ============================================

bool initCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.grab_mode = CAMERA_GRAB_LATEST;
  
  // PSRAM varsa yuksek cozunurluk kullan
  if (psramFound()) {
    config.frame_size = FRAMESIZE_VGA;      // 640x480
    config.jpeg_quality = 12;                // 0-63, dusuk = kaliteli
    config.fb_count = 2;
    Serial.println("PSRAM bulundu, VGA cozunurluk kullaniliyor");
  } else {
    config.frame_size = FRAMESIZE_QVGA;     // 320x240
    config.jpeg_quality = 15;
    config.fb_count = 1;
    Serial.println("PSRAM yok, QVGA cozunurluk kullaniliyor");
  }
  
  // Kamerayi baslat
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Kamera baslatma HATASI: 0x%x\n", err);
    return false;
  }
  
  // Kamera ayarlarini optimize et
  sensor_t * s = esp_camera_sensor_get();
  if (s != NULL) {
    s->set_brightness(s, 0);     // -2 to 2
    s->set_contrast(s, 0);       // -2 to 2
    s->set_saturation(s, 0);     // -2 to 2
    s->set_special_effect(s, 0); // 0 = No Effect
    s->set_whitebal(s, 1);       // 0 = disable, 1 = enable
    s->set_awb_gain(s, 1);       // 0 = disable, 1 = enable
    s->set_wb_mode(s, 0);        // 0 to 4
    s->set_exposure_ctrl(s, 1);  // 0 = disable, 1 = enable
    s->set_aec2(s, 0);           // 0 = disable, 1 = enable
    s->set_gain_ctrl(s, 1);      // 0 = disable, 1 = enable
    s->set_agc_gain(s, 0);       // 0 to 30
    s->set_gainceiling(s, (gainceiling_t)0);  // 0 to 6
    s->set_bpc(s, 0);            // 0 = disable, 1 = enable
    s->set_wpc(s, 1);            // 0 = disable, 1 = enable
    s->set_raw_gma(s, 1);        // 0 = disable, 1 = enable
    s->set_lenc(s, 1);           // 0 = disable, 1 = enable
    s->set_hmirror(s, 0);        // 0 = disable, 1 = enable
    s->set_vflip(s, 0);          // 0 = disable, 1 = enable
    s->set_dcw(s, 1);            // 0 = disable, 1 = enable
  }
  
  Serial.println("Kamera basarıyla baslatildi");
  return true;
}

// ============================================
// CONFIG YÖNETİMİ (NVS)
// ============================================

void loadConfig() {
  prefs.begin("akilli-cam", true);
  String serverUrl   = prefs.getString("server_url",   "");
  String sinifId     = prefs.getString("classroom_id", "sinif-1");
  String apiKey      = prefs.getString("api_key",      "");
  String mqttBroker  = prefs.getString("mqtt_broker",  "");
  String mqttUser    = prefs.getString("mqtt_user",    MQTT_USER_DEFAULT);
  String mqttPass    = prefs.getString("mqtt_pass",    MQTT_PASSWORD_DEFAULT);
  prefs.end();

  serverUrl.toCharArray(SERVER_URL,    sizeof(SERVER_URL));
  sinifId.toCharArray(CLASSROOM_ID,    sizeof(CLASSROOM_ID));
  apiKey.toCharArray(API_KEY,          sizeof(API_KEY));
  if (mqttBroker.length() > 0)
    mqttBroker.toCharArray(MQTT_BROKER, sizeof(MQTT_BROKER));
  mqttUser.toCharArray(MQTT_USER,      sizeof(MQTT_USER));
  mqttPass.toCharArray(MQTT_PASS,      sizeof(MQTT_PASS));

  Serial.println("[Config] Sinif ID: "   + sinifId);
  Serial.println("[Config] Server URL: " + serverUrl);
  Serial.println("[Config] MQTT Broker: "+ mqttBroker);
  Serial.println("[Config] API Key: "    + (apiKey.length() > 0 ? String("***") : String("(bos)")));
  Serial.println("[Config] Firmware: v"  FIRMWARE_VERSION);
}

void checkConfigReset() {
  if (digitalRead(CONFIG_RESET_PIN) == LOW) {
    unsigned long pressStart = millis();
    Serial.println("[Config] Reset butonu algilandi...");
    while (digitalRead(CONFIG_RESET_PIN) == LOW) {
      if (millis() - pressStart >= RESET_HOLD_MS) {
        prefs.begin("akilli-cam", false);
        prefs.clear();
        prefs.end();
        Serial.println("[Config] NVS silindi! Yeniden baslatiliyor...");
        delay(1000);
        ESP.restart();
      }
      delay(100);
    }
  }
}

// ============================================
// OTA GÜNCELLEME (HTTPS + CA pin + URL allowlist + MD5)
// ============================================

// GitHub Root CA (ISRG Root X1) — ota_manager.cpp ile ayni sertifika
static const char* GITHUB_ROOT_CA_CAM = R"EOF(
-----BEGIN CERTIFICATE-----
MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw
TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh
cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4
WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu
ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY
MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJHP0FDfzm54rVygc
h77ct984kIxuPOZXoHj3dcKi/vVqbvYATyjb3miGbESTtrFj/RQSa78f0uoxmyF+
0TM8ukj13Xnfs7j/EvEhmkvBioZxaUpmZmyPfjxwv60pIgbz5MDmgK7iS4+3mX6U
A5/TR5d8mUgjU+g4rk8Kb4Mu0UlXjIB0ttov0DiNewNwIRt18jA8+o+u3dpjq+sW
T8KOEUt+zwvo/7V3LvSye0rgTBIlDHCNAymg4VMk7BPZ7hm/ELNKjD+Jo2FR3qyH
B5T0Y3HsLuJvW5iB4YlcNHlsdu87kGJ55tukmi8mxdAQ4Q7e2RCOFvu396j3x+UC
B5iPNgiV5+I3lg02dZ77DnKxHZu8A/lJBdiB3QW0KtZB6awBdpUKD9jf1b0SHzUv
KBds0pjBqAlkd25HN7rOrFleaJ1/ctaJxQZBKT5ZPt0m9STJEadao0xAH0ahmbWn
OlFuhjuefXKnEgV4We0+UXgVCwOPjdAvBbI+e0ocS3MFEvzG6uBQE3xDk3SzynTn
jh8BCNAw1FtxNrQHusEwMFxIt4I7mKZ9YIqioymCzLq9gwQbooMDQaHWBfEbwrbw
qHyGO0aoSCqI3Haadr8faqU9GY/rOPNk3sgrDQoo//fb4hVC1CLQJ13hef4Y53CI
rU7m2Ys6xt0nUW7/vGT1M0NPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNV
HRMBAf8EBTADAQH/MB0GA1UdDgQWBBR5tFnme7bl5AFzgAiIyBpY9umbbjANBgkq
hkiG9w0BAQsFAAOCAgEAVR9YqbyyqFDQDLHYGmkgJykIrGF1XIpu+ILlaS/V9lZL
ubhzEFnTIZd+50xx+7LSYK05qAvqFyFWhfFQDlnrzuBZ6brJFe+GnY+EgPbk6ZGQ
3BebYhtF8GaV0nxvwuo77x/Py9auJ/GpsMiu/X1+mvoiBOv/2X/qkSsisRcOj/KK
NFtY2PwByVS5uCbMiogziUwthDyC3+6WVwW6LLv3xLfHTjuCvjHIInNzktHCgKQ5
ORAzI4JMPJ+GslWYHb4phowim57iaztXOoJwTdwJx4nLCgdNbOhdjsnvzqvHu7Ur
TkXWStAmzOVyyghqpZXjFaH3pO3JLF+l+/+sKAIuvtd7u+Nxe5AW0wdeRlN8NwF
XeEPKB8QMoGMnp6WKkAXSmzPwOHhYBMmWcXIjkNs7h47I0RbBn+hwL/b6eJmNBj
jSalqOHFBn6RPZNY60KNRpXYJEGy4OHrNXOPlVIRRRl9T6VmIBAlMgLjzNjgMUCd
qmg1HcNBGWlNFmBJ0RM+sTgD4DBpMq6DUxT5K7k+X75wTCYGVNKrF/Zzwbe/MUq
H/FMveVIHJsIWoU3I3MNiOaZmfxhbpnxZb3jgfZEVHhWWFCfEbDMHB+TelKIvSdM
JuCpjCNv3LrlnHh6FGzRMzAizNOBOuarLkb8x/RVn16sN5U1+kVjGqYBDlJ6kQ==
-----END CERTIFICATE-----
)EOF";

// MD5 sidecar (.md5) dosyasini HTTPS+CA pin'li indir. Boş döner = bulunamadı.
static String fetchOtaMd5Sidecar(const String& binUrl) {
  WiFiClientSecure mclient;
  mclient.setCACert(GITHUB_ROOT_CA_CAM);
  mclient.setTimeout(15);

  HTTPClient mhttp;
  mhttp.begin(mclient, binUrl + ".md5");
  mhttp.setTimeout(15000);
  mhttp.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

  int code = mhttp.GET();
  if (code != HTTP_CODE_OK) {
    Serial.printf("[OTA] .md5 sidecar bulunamadi (HTTP %d)\n", code);
    mhttp.end();
    return "";
  }
  String body = mhttp.getString();
  mhttp.end();
  body.trim();
  if (body.length() < 32) return "";
  String md5 = body.substring(0, 32);
  md5.toLowerCase();
  return md5;
}

// CAM OTA: HTTPS + CA pin'li + URL allowlist + MD5 (zorunlu, sidecar fallback)
void performOTA(const String& url, const String& version, const String& expectedMd5 = "") {
  Serial.println("[OTA] Guncelleme basliyor: " + version);
  Serial.println("[OTA] URL: " + url);

  // GUVENLIK: URL allowlist
  const char* ALLOWED_PREFIX = "https://github.com/MagiMigi/akilli-sinif/releases/";
  if (!url.startsWith(ALLOWED_PREFIX)) {
    Serial.println("[OTA] URL allowlist disinda — reddedildi.");
    return;
  }

  // GUVENLIK: MD5 zorunlu — payload'da yoksa sidecar (.md5) indir
  String md5 = expectedMd5;
  if (md5.length() == 0) {
    Serial.println("[OTA] MD5 sidecar indiriliyor: " + url + ".md5");
    md5 = fetchOtaMd5Sidecar(url);
    if (md5.length() != 32) {
      Serial.println("[OTA] MD5 yok/gecersiz — guncelleme reddedildi.");
      return;
    }
    Serial.println("[OTA] Sidecar MD5: " + md5);
  }

  WiFiClientSecure client;
  client.setCACert(GITHUB_ROOT_CA_CAM);
  client.setTimeout(30);

  HTTPClient http;
  http.begin(client, url);
  http.setTimeout(60000);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    Serial.printf("[OTA] HTTPS hata: %d\n", code);
    http.end();
    return;
  }

  int size = http.getSize();
  if (!Update.begin(size)) {
    Serial.println("[OTA] Yetersiz alan! min_spiffs partition gerekli.");
    http.end();
    return;
  }

  // GUVENLIK: MD5 dogrulama (artik her zaman var)
  if (!Update.setMD5(md5.c_str())) {
    Serial.println("[OTA] MD5 set hatasi (gecersiz format).");
    Update.abort();
    http.end();
    return;
  }

  WiFiClient* stream = http.getStreamPtr();
  size_t written = Update.writeStream(*stream);
  http.end();

  if (Update.end() && Update.isFinished()) {
    Serial.println("[OTA] Guncelleme BASARILI! Yeniden baslatiliyor...");
    delay(500);
    ESP.restart();
  } else {
    Serial.println("[OTA] Hata: " + String(Update.errorString()));
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
  Serial.println("WiFiManager Baslatiliyor (CAM)...");
  Serial.println("========================================");

  WiFiManager wm;

  // ── GUVENLIK: WPA2 AP sifresi (MAC turevli) + portal HTTP Basic Auth
  String apPass = makeApPassword();
  wm.setHttpUser("admin");
  wm.setHttpPassword(apPass.c_str());

  Serial.println("[WiFi] ===== AP BILGILERI =====");
  Serial.println("[WiFi] SSID: " + camApName);
  Serial.println("[WiFi] WPA2 Sifre: " + apPass);
  Serial.println("[WiFi] Web kullanici: admin");
  Serial.println("[WiFi] Web sifre: " + apPass);
  Serial.println("[WiFi] ========================");

  // Portal'da ekstra alanlar
  char serverDefault[80];
  snprintf(serverDefault, sizeof(serverDefault), "%s", SERVER_URL);

  WiFiManagerParameter p_serverUrl("server_url", "AI Server URL (http://IP:5000/analyze)",
                                    serverDefault, 79);
  WiFiManagerParameter p_classroomId("classroom_id", "Sinif ID (sinif-1, sinif-2...)",
                                      CLASSROOM_ID, 19);
  WiFiManagerParameter p_apiKey("api_key", "API Anahtari (YOLO sunucu sifresi)",
                                  API_KEY, 63);
  WiFiManagerParameter p_mqttBroker("mqtt_broker", "MQTT Broker IP (sunucu bilgisayarin IP)",
                                     MQTT_BROKER, 39);

  wm.addParameter(&p_serverUrl);
  wm.addParameter(&p_classroomId);
  wm.addParameter(&p_apiKey);
  wm.addParameter(&p_mqttBroker);

  wm.setConfigPortalTimeout(180);

  bool connected = wm.autoConnect(camApName.c_str(), apPass.c_str());

  if (!connected) {
    Serial.println("[WiFi] Baglanti saglanamadi, yeniden baslatiliyor...");
    delay(3000);
    ESP.restart();
    return;
  }

  // Config'i kaydet
  String newUrl        = String(p_serverUrl.getValue());
  String newSinifId    = String(p_classroomId.getValue());
  String newApiKey     = String(p_apiKey.getValue());
  String newMqttBroker = String(p_mqttBroker.getValue());

  if (newUrl.length() > 0 || newSinifId.length() > 0 ||
      newApiKey.length() > 0 || newMqttBroker.length() > 0) {
    prefs.begin("akilli-cam", false);
    if (newUrl.length()        > 0) prefs.putString("server_url",   newUrl);
    if (newSinifId.length()    > 0) prefs.putString("classroom_id", newSinifId);
    if (newApiKey.length()     > 0) prefs.putString("api_key",      newApiKey);
    if (newMqttBroker.length() > 0) prefs.putString("mqtt_broker",  newMqttBroker);
    prefs.end();

    newUrl.toCharArray(SERVER_URL,       sizeof(SERVER_URL));
    newSinifId.toCharArray(CLASSROOM_ID, sizeof(CLASSROOM_ID));
    newApiKey.toCharArray(API_KEY,       sizeof(API_KEY));
    newMqttBroker.toCharArray(MQTT_BROKER, sizeof(MQTT_BROKER));
  }

  Serial.println("[WiFi] BAGLANDI! IP: " + WiFi.localIP().toString());
}

// ============================================
// HTTP GONDERME FONKSIYONU
// ============================================

bool sendImageToServer(camera_fb_t *fb) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi baglantisi yok!");
    return false;
  }
  
  HTTPClient http;
  http.begin(SERVER_URL);
  http.addHeader("Content-Type", "image/jpeg");
  http.addHeader("X-Classroom-ID", CLASSROOM_ID);
  if (strlen(API_KEY) > 0) {
    http.addHeader("X-API-Key", API_KEY);
  }
  http.setTimeout(30000);  // 30 saniye timeout
  
  Serial.printf("Foto gonderiliyor... Boyut: %d bytes\n", fb->len);
  
  int httpCode = http.POST(fb->buf, fb->len);
  
  if (httpCode > 0) {
    if (httpCode == HTTP_CODE_OK) {
      String response = http.getString();
      Serial.print("Sunucu yaniti: ");
      Serial.println(response);
      
      // JSON parse et
      StaticJsonDocument<256> doc;
      DeserializationError error = deserializeJson(doc, response);
      
      if (!error) {
        int personCount = doc["person_count"] | 0;
        Serial.printf("Tespit edilen kisi sayisi: %d\n", personCount);
        
        // Kisi varsa daha sik foto cek
        if (personCount > 0) {
          captureInterval = CAPTURE_INTERVAL_ACTIVE;
        } else {
          captureInterval = CAPTURE_INTERVAL_IDLE;
        }
      }
      
      consecutiveFailures = 0;
      http.end();
      return true;
    } else {
      Serial.printf("HTTP Hata kodu: %d\n", httpCode);
    }
  } else {
    Serial.printf("HTTP Baglanti hatasi: %s\n", http.errorToString(httpCode).c_str());
  }
  
  consecutiveFailures++;
  http.end();
  return false;
}

// ============================================
// MQTT - CONFIG CALLBACK
// ============================================

// Node-RED'den gelen config güncellemelerini işle
// Topic: akilli-sinif/{sinif-id}/control/config
// Payload örneği: {"api_key":"yeni-key"} veya {"server_url":"http://IP:5000/analyze"}
void onMqttMessage(char* topic, byte* payload, unsigned int length) {
  String msg;
  for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];
  Serial.println("[MQTT] Config mesaji alindi: " + msg);

  StaticJsonDocument<256> doc;
  if (deserializeJson(doc, msg) != DeserializationError::Ok) {
    Serial.println("[MQTT] JSON parse hatasi, mesaj yok sayildi.");
    return;
  }

  prefs.begin("akilli-cam", false);
  bool changed = false;

  if (doc.containsKey("api_key")) {
    String val = doc["api_key"].as<String>();
    if (val.length() >= sizeof(API_KEY)) {
      Serial.println("[MQTT] api_key cok uzun — reddedildi.");
    } else {
      val.toCharArray(API_KEY, sizeof(API_KEY));
      prefs.putString("api_key", val);
      Serial.println("[MQTT] api_key guncellendi.");
      changed = true;
    }
  }
  if (doc.containsKey("server_url")) {
    String val = doc["server_url"].as<String>();
    if (val.length() >= sizeof(SERVER_URL)) {
      Serial.println("[MQTT] server_url cok uzun — reddedildi.");
    } else {
      val.toCharArray(SERVER_URL, sizeof(SERVER_URL));
      prefs.putString("server_url", val);
      Serial.println("[MQTT] server_url guncellendi: " + val);
      changed = true;
    }
  }
  if (doc.containsKey("mqtt_broker")) {
    String val = doc["mqtt_broker"].as<String>();
    if (val.length() >= sizeof(MQTT_BROKER)) {
      Serial.println("[MQTT] mqtt_broker cok uzun — reddedildi.");
    } else {
      val.toCharArray(MQTT_BROKER, sizeof(MQTT_BROKER));
      prefs.putString("mqtt_broker", val);
      Serial.println("[MQTT] mqtt_broker guncellendi — yeniden baslat gerekebilir.");
      changed = true;
    }
  }

  prefs.end();

  if (changed) {
    // Onay mesajı yayınla
    String ackTopic = String("akilli-sinif/") + CLASSROOM_ID + "/status/config";
    mqttClient.publish(ackTopic.c_str(), "{\"config_updated\":true}", false);
  }
}

// ============================================
// MQTT - BAĞLAN + SUBSCRIBE + IP YAYINLA
// ============================================

void setupMQTT() {
  if (strlen(MQTT_BROKER) == 0) {
    Serial.println("[MQTT] Broker adresi bos, MQTT atlanıyor.");
    return;
  }

  mqttClient.setServer(MQTT_BROKER, 1883);
  mqttClient.setCallback(onMqttMessage);

  String clientId = "cam-" + String(CLASSROOM_ID);
  if (mqttClient.connect(clientId.c_str(), MQTT_USER, MQTT_PASS)) {
    // Config güncellemelerini dinle
    String subTopic = String("akilli-sinif/") + CLASSROOM_ID + "/control/config";
    mqttClient.subscribe(subTopic.c_str());
    Serial.println("[MQTT] Subscribe: " + subTopic);

    // IP adresini yayınla (retain=true → Node-RED her zaman görür)
    String ipTopic = String("akilli-sinif/") + CLASSROOM_ID + "/status/ip";
    mqttClient.publish(ipTopic.c_str(), WiFi.localIP().toString().c_str(), true);
    Serial.println("[MQTT] IP yayinlandi: " + WiFi.localIP().toString());
  } else {
    Serial.println("[MQTT] Baglanti kurulamadi, sonra tekrar denenir.");
  }
}

// ============================================
// WEB SERVER - /reset-config ENDPOINT
// ============================================

void setupWebServer() {
  // Tarayıcıdan http://<IP>/reset-config ile config sıfırla (HTTP Basic Auth)
  webServer.on("/reset-config", HTTP_GET, []() {
    String pass = makeApPassword();
    if (!webServer.authenticate("admin", pass.c_str())) {
      return webServer.requestAuthentication();
    }
    webServer.send(200, "text/plain",
      "Config siliniyor, cihaz yeniden baslatiliyor...\n"
      "Lutfen 'Akilli-CAM-Setup' WiFi agina baglanin.");
    delay(500);
    prefs.begin("akilli-cam", false);
    prefs.clear();
    prefs.end();
    Serial.println("[WebServer] /reset-config (auth OK), NVS silindi.");
    delay(500);
    ESP.restart();
  });

  // Durum sayfası (HTTP Basic Auth — version-leak korumasi)
  webServer.on("/", HTTP_GET, []() {
    String pass = makeApPassword();
    if (!webServer.authenticate("admin", pass.c_str())) {
      return webServer.requestAuthentication();
    }
    String html = "<h3>Akilli Sinif - ESP32-CAM</h3>";
    html += "<p>Sinif: " + String(CLASSROOM_ID) + "</p>";
    html += "<p>Firmware: v" FIRMWARE_VERSION "</p>";
    html += "<p>IP: " + WiFi.localIP().toString() + "</p>";
    html += "<p><a href='/reset-config'>Config Sifirla</a></p>";
    webServer.send(200, "text/html", html);
  });

  webServer.begin();
  Serial.println("[WebServer] Baslatildi: http://" + WiFi.localIP().toString());
  Serial.println("[WebServer] Config sifirla: http://" + WiFi.localIP().toString() + "/reset-config");
}

// ============================================
// FOTO CEKME VE GONDERME
// ============================================

void captureAndSend() {
  Serial.println("\n--- Foto Cekiliyor ---");
  
  // Flash LED'i ac (opsiyonel, karanlik ortam icin)
  // digitalWrite(FLASH_GPIO_NUM, HIGH);
  // delay(100);
  
  camera_fb_t *fb = esp_camera_fb_get();
  
  // digitalWrite(FLASH_GPIO_NUM, LOW);
  
  if (!fb) {
    Serial.println("Foto cekme HATASI!");
    consecutiveFailures++;
    return;
  }
  
  Serial.printf("Foto boyutu: %d bytes, Cozunurluk: %dx%d\n", 
                fb->len, fb->width, fb->height);
  
  // Sunucuya gonder
  bool success = sendImageToServer(fb);
  
  // Frame buffer'i serbest birak
  esp_camera_fb_return(fb);
  
  // Cok fazla hata varsa yeniden baslat
  if (consecutiveFailures >= MAX_FAILURES) {
    Serial.println("Cok fazla hata! ESP32-CAM yeniden baslatiliyor...");
    delay(1000);
    ESP.restart();
  }
}

// ============================================
// SETUP VE LOOP
// ============================================

void setup() {
  Serial.begin(115200);
  delay(500);

  // Config reset butonu - erken kontrol
  pinMode(CONFIG_RESET_PIN, INPUT_PULLUP);

  // NVS'den config oku
  loadConfig();

  Serial.println("\n\n");
  Serial.println("╔══════════════════════════════════════╗");
  Serial.println("║   AKILLI SINIF - ESP32-CAM            ║");
  Serial.println("║   Kisi Sayma Modulu                   ║");
  Serial.println("╠══════════════════════════════════════╣");
  Serial.print  ("║  Sinif:    "); Serial.println(CLASSROOM_ID);
  Serial.print  ("║  Firmware: v"); Serial.println(FIRMWARE_VERSION);
  Serial.println("╚══════════════════════════════════════╝\n");

  // Flash LED pini
  pinMode(FLASH_GPIO_NUM, OUTPUT);
  digitalWrite(FLASH_GPIO_NUM, LOW);

  // Config reset kontrolu
  checkConfigReset();

  // Kamerayi baslat
  cameraReady = initCamera();
  if (!cameraReady) {
    Serial.println("Kamera baslatılamadi! Yeniden baslatiliyor...");
    delay(3000);
    ESP.restart();
  }

  // WiFiManager ile baglan
  setupWiFi();

  // Web sunucusunu baslat (reset-config endpoint)
  setupWebServer();

  // MQTT bağlan, config topic'ini dinle, IP yayınla
  setupMQTT();

  Serial.println("\n╔══════════════════════════════════════╗");
  Serial.println("║     KAMERA HAZIR - Foto basliyor    ║");
  Serial.println("╚══════════════════════════════════════╝\n");

  // Ilk fotoyu hemen cek
  captureAndSend();
}

void loop() {
  // WiFi kontrolu
  if (WiFi.status() != WL_CONNECTED) {
    wifiFailCount++;
    Serial.printf("[WiFi] Koptu, deneme %d/%d\n", wifiFailCount, WIFI_FAIL_PORTAL_THRESHOLD);

    if (wifiFailCount >= WIFI_FAIL_PORTAL_THRESHOLD) {
      // Çok fazla başarısız deneme → portal aç, kullanıcı yeni WiFi girebilir
      Serial.println("[WiFi] Otomatik portal aciliyor (5 dakika)...");
      wifiFailCount = 0;

      WiFiManager wm;
      wm.setConfigPortalTimeout(300);  // 5 dakika bekle, sonra yeniden dene

      // ── GUVENLIK: WPA2 AP sifresi + portal Basic Auth (boot ile ayni)
      String apPass = makeApPassword();
      wm.setHttpUser("admin");
      wm.setHttpPassword(apPass.c_str());
      Serial.println("[WiFi] AP yeniden aciliyor — sifre: " + apPass);

      // Mevcut değerleri ön doldur
      WiFiManagerParameter p_serverUrl("server_url", "AI Server URL", SERVER_URL, 79);
      WiFiManagerParameter p_classroomId("classroom_id", "Sinif ID", CLASSROOM_ID, 19);
      WiFiManagerParameter p_apiKey("api_key", "API Anahtari", API_KEY, 63);
      WiFiManagerParameter p_mqttBroker("mqtt_broker", "MQTT Broker IP", MQTT_BROKER, 39);
      wm.addParameter(&p_serverUrl);
      wm.addParameter(&p_classroomId);
      wm.addParameter(&p_apiKey);
      wm.addParameter(&p_mqttBroker);

      wm.startConfigPortal(camApName.c_str(), apPass.c_str());

      // Portal kapandı — yeni değerleri kaydet
      String newUrl        = String(p_serverUrl.getValue());
      String newSinifId    = String(p_classroomId.getValue());
      String newApiKey     = String(p_apiKey.getValue());
      String newMqttBroker = String(p_mqttBroker.getValue());

      prefs.begin("akilli-cam", false);
      if (newUrl.length()        > 0) { prefs.putString("server_url",   newUrl);        newUrl.toCharArray(SERVER_URL, sizeof(SERVER_URL)); }
      if (newSinifId.length()    > 0) { prefs.putString("classroom_id", newSinifId);    newSinifId.toCharArray(CLASSROOM_ID, sizeof(CLASSROOM_ID)); }
      if (newApiKey.length()     > 0) { prefs.putString("api_key",      newApiKey);     newApiKey.toCharArray(API_KEY, sizeof(API_KEY)); }
      if (newMqttBroker.length() > 0) { prefs.putString("mqtt_broker",  newMqttBroker); newMqttBroker.toCharArray(MQTT_BROKER, sizeof(MQTT_BROKER)); }
      prefs.end();

      if (WiFi.status() == WL_CONNECTED) {
        Serial.println("[WiFi] Portal sonrasi baglandi: " + WiFi.localIP().toString());
        setupMQTT();  // yeni IP'yi yayınla, tekrar subscribe ol
      }
    } else {
      WiFi.reconnect();
      delay(5000);
    }
    return;  // Bu döngüde fotoğraf çekme, önce WiFi düzelsin
  }

  // WiFi bağlıysa sayacı sıfırla
  wifiFailCount = 0;

  // MQTT mesajlarını işle (config güncellemeleri vb.)
  if (!mqttClient.connected() && strlen(MQTT_BROKER) > 0) {
    setupMQTT();  // bağlantı kopmuşsa yeniden bağlan
  }
  mqttClient.loop();

  // Web sunucu isteklerini isle (/reset-config vb.)
  webServer.handleClient();

  // Periyodik foto cekme
  unsigned long currentMillis = millis();
  if (currentMillis - lastCapture >= captureInterval) {
    lastCapture = currentMillis;
    captureAndSend();
  }

  delay(100);
}
