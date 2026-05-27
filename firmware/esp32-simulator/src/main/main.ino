/*
 * Akilli Sinif Sistemi - ESP32 Simulator Firmware
 * =================================================
 *
 * Bu firmware, gerçek sensör donanımı OLMAYAN bir ESP32 üzerinde
 * çalışır ve başka bir sınıfı temsil eder (örn. sinif-2).
 *
 * Özellikler:
 * - Gerçekçi sinüs bazlı sensör simülasyonu (gün/gece döngüsü)
 * - MQTT üzerinden veri yayını (PLC firmware ile aynı topic yapısı)
 * - WiFiManager ile kimlik bilgisi yönetimi (NVS'e kaydedilir)
 * - OTA firmware güncelleme
 * - PLC automation mantığı (yazılım içi: LED %, cooling/heating durumu hesaplanır)
 *
 * ILK KURULUM:
 *   1. Firmware'i yak
 *   2. "Akilli-SIM-Setup" WiFi ağına bağlan
 *   3. Sinif ID: sinif-2 | MQTT IP: gir
 *   4. Kaydet
 *
 * AP SIFRESI:
 *   "Akilli-SIM-Setup" agi WPA2 korumali. Sifre cihaz MAC'inden turetilir
 *   → "akilli-XXXXXX". Boot anında Serial monitor'de yazilir.
 *
 * CONFIG SIFIRLAMA:
 *   GPIO0 (BOOT) butonuna 5 sn bas → NVS silinir → Portal tekrar acar
 *
 * Yazar: Akilli Sinif Projesi
 * Tarih: 2026
 */

#include <WiFi.h>
#include <WiFiManager.h>
#include <Preferences.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Update.h>
#include "../../../version.h"
#include "../../../secrets.h"   // MQTT user/sifre — repo'ya commit'lenmez

// ============================================
// YAPILANDIRMA - NVS'DEN OKUNUR
// ============================================

Preferences prefs;

char MQTT_BROKER[40]  = "";
char CLASSROOM_ID[20] = "sinif-2";

const int   MQTT_PORT     = 1883;
const char* MQTT_USER     = MQTT_USER_DEFAULT;      // secrets.h
const char* MQTT_PASSWORD = MQTT_PASSWORD_DEFAULT;  // secrets.h
String mqttClientId;

const int CONFIG_RESET_PIN        = 0;
const unsigned long RESET_HOLD_MS = 5000;

// ============================================
// ZAMANLAMA
// ============================================

const unsigned long PUBLISH_INTERVAL = 5000;
const unsigned long STATUS_INTERVAL  = 30000;
const unsigned long WIFI_CHECK_INT   = 10000;

unsigned long lastPublish    = 0;
unsigned long lastStatus     = 0;
unsigned long lastWifiCheck  = 0;
unsigned long lastMqttReconn = 0;

// ============================================
// MQTT
// ============================================

WiFiClient   wifiClient;
PubSubClient mqttClient(wifiClient);
bool mqttConnected    = false;
int  reconnectAttempts = 0;
const int MAX_RECONNECT          = 5;
const unsigned long MQTT_RECONNECT_INT = 5000;

// ============================================
// SIMULASYON VERISI
// ============================================

// simTime: dakika cinsinden "sanal saat" (1 gerçek sn = 1 sanal dk)
unsigned long simTime = 480;  // 08:00'den başla

struct SimData {
  float temperature;
  float humidity;
  int   lightLevel;
  int   airQuality;
  bool  motionDetected;
  bool  windowOpen;
  int   personCount;
  int   ledBrightness;
  bool  coolingOn;
  bool  heatingOn;
};

SimData sim;

float simSinusoidal(float minVal, float maxVal, float periodMin, float phaseMin) {
  float angle = (2.0f * PI * (simTime - phaseMin)) / periodMin;
  float ratio = (sin(angle) + 1.0f) / 2.0f;
  return minVal + ratio * (maxVal - minVal);
}

void updateSimulation() {
  static unsigned long lastSimUpdate = 0;
  if (millis() - lastSimUpdate >= 1000) {
    lastSimUpdate = millis();
    simTime++;
    if (simTime >= 1440) simTime = 0;
  }

  float hour = simTime / 60.0f;

  // Sicaklik: geceleri 18C oglen 28C
  sim.temperature = simSinusoidal(18.0f, 28.0f, 1440.0f, 360.0f);
  sim.temperature += (random(-5, 6)) / 10.0f;

  // Nem: ters korelasyon
  sim.humidity = simSinusoidal(60.0f, 40.0f, 1440.0f, 360.0f);
  sim.humidity += (random(-10, 11)) / 10.0f;
  sim.humidity = constrain(sim.humidity, 20.0f, 90.0f);

  // Isik: gunduz yuksek
  if (hour >= 7.0f && hour <= 17.5f) {
    sim.lightLevel = (int)simSinusoidal(100.0f, 900.0f, 1440.0f, 420.0f);
    sim.lightLevel += random(-30, 31);
  } else {
    sim.lightLevel = random(0, 20);
  }
  sim.lightLevel = constrain(sim.lightLevel, 0, 1000);

  // Hava kalitesi: ders saatlerinde artar
  bool classPeriod = (hour >= 8.0f && hour <= 12.0f) ||
                     (hour >= 13.0f && hour <= 17.0f);
  sim.airQuality = classPeriod ? 80 + random(0, 100) : 50 + random(0, 30);

  // Hareket + Kisi sayisi
  if (classPeriod) {
    sim.motionDetected = true;
    sim.personCount    = 20 + random(-5, 11);
  } else {
    sim.motionDetected = false;
    sim.personCount    = 0;
  }
  sim.personCount = constrain(sim.personCount, 0, 30);

  // Pencere: nadir toggle
  if (random(0, 500) == 0) sim.windowOpen = !sim.windowOpen;

  // Automation: LED
  if (sim.motionDetected && sim.lightLevel < 200)      sim.ledBrightness = 80;
  else if (sim.motionDetected && sim.lightLevel < 400) sim.ledBrightness = 40;
  else                                                  sim.ledBrightness = 0;

  // Automation: Cooling histeresis (PLC ile aynı: 26 ON / 24 OFF)
  if (sim.temperature > 26.0f)      sim.coolingOn = true;
  else if (sim.temperature < 24.0f) sim.coolingOn = false;

  // Automation: Heating histeresis (PLC ile aynı: 20 ON / 22 OFF)
  if (sim.temperature < 20.0f)      sim.heatingOn = true;
  else if (sim.temperature > 22.0f) sim.heatingOn = false;
}

// ============================================
// YARDIMCI FONKSIYONLAR
// ============================================

String buildTopic(const char* type, const char* name) {
  return String("akilli-sinif/") + CLASSROOM_ID + "/" + type + "/" + name;
}

void publishJson(String topic, JsonDocument& doc, bool retained = false) {
  char buf[256];
  serializeJson(doc, buf);
  mqttClient.publish(topic.c_str(), buf, retained);
}

void publishSensorData() {
  StaticJsonDocument<128> d;

  d["value"] = sim.temperature; d["unit"] = "C"; d["sim"] = true;
  publishJson(buildTopic("sensors", "temperature"), d);

  d.clear();
  d["value"] = sim.humidity; d["unit"] = "%"; d["sim"] = true;
  publishJson(buildTopic("sensors", "humidity"), d);

  d.clear();
  d["value"] = sim.lightLevel; d["unit"] = "lux"; d["sim"] = true;
  publishJson(buildTopic("sensors", "light"), d);

  d.clear();
  d["value"] = sim.airQuality; d["unit"] = "ppm"; d["sim"] = true;
  publishJson(buildTopic("sensors", "air_quality"), d);

  d.clear();
  d["detected"] = sim.motionDetected; d["sim"] = true;
  publishJson(buildTopic("sensors", "pir"), d);

  d.clear();
  d["open"] = sim.windowOpen; d["sim"] = true;
  publishJson(buildTopic("sensors", "window"), d);

  d.clear();
  d["person_count"] = sim.personCount; d["sim"] = true;
  publishJson(buildTopic("sensors", "camera"), d);

  d.clear();
  d["value"] = sim.ledBrightness; d["unit"] = "%"; d["sim"] = true;
  publishJson(buildTopic("actuators", "led"), d);

  d.clear();
  d["state"] = sim.coolingOn ? "on" : "off"; d["sim"] = true;
  publishJson(buildTopic("actuators", "cooling"), d);

  d.clear();
  d["state"] = sim.heatingOn ? "on" : "off"; d["sim"] = true;
  publishJson(buildTopic("actuators", "heating"), d);

  Serial.printf("[SIM] %02d:%02d | %.1fC | %.0f%% | %dlux | %dppm | %dkisi\n",
                (int)(simTime / 60), (int)(simTime % 60),
                sim.temperature, sim.humidity,
                sim.lightLevel, sim.airQuality, sim.personCount);
}

void publishStatus(const char* status) {
  StaticJsonDocument<256> doc;
  doc["status"]           = status;
  doc["device"]           = mqttClientId;
  doc["classroom"]        = CLASSROOM_ID;
  doc["ip"]               = WiFi.localIP().toString();
  doc["rssi"]             = WiFi.RSSI();
  doc["uptime"]           = millis() / 1000;
  doc["mock_mode"]        = true;
  doc["firmware_version"] = FIRMWARE_VERSION;
  doc["sim_hour"]         = simTime / 60;

  String topic = buildTopic("status", "connection");
  publishJson(topic, doc, true);
}

// ============================================
// MQTT
// ============================================

// ── OTA: HTTPS + GitHub root CA pin'li + URL allowlist + MD5 zorunlu ──
// Pattern PLC/CAM ile ozdes; SIM kendi binary'sini cektigi icin
// (firmware-sim-vX.Y.Z.bin) bagimsiz update yapar.
static const char* GITHUB_ROOT_CA_SIM = R"EOF(
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

static String fetchOtaMd5SidecarSim(const String& binUrl) {
  WiFiClientSecure mclient;
  mclient.setCACert(GITHUB_ROOT_CA_SIM);
  mclient.setTimeout(15);
  HTTPClient mhttp;
  mhttp.begin(mclient, binUrl + ".md5");
  mhttp.setTimeout(15000);
  mhttp.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  int code = mhttp.GET();
  if (code != HTTP_CODE_OK) { mhttp.end(); return ""; }
  String body = mhttp.getString();
  mhttp.end();
  body.trim();
  if (body.length() < 32) return "";
  String md5 = body.substring(0, 32);
  md5.toLowerCase();
  return md5;
}

void performOTA(const String& url, const String& version, const String& expectedMd5 = "") {
  Serial.println("[OTA] Guncelleme basliyor: " + version);
  Serial.println("[OTA] URL: " + url);

  const char* ALLOWED_PREFIX = "https://github.com/MagiMigi/akilli-sinif/releases/";
  if (!url.startsWith(ALLOWED_PREFIX)) {
    Serial.println("[OTA] URL allowlist disinda — reddedildi.");
    return;
  }

  String md5 = expectedMd5;
  if (md5.length() == 0) {
    md5 = fetchOtaMd5SidecarSim(url);
    if (md5.length() != 32) {
      Serial.println("[OTA] MD5 yok/gecersiz — reddedildi.");
      return;
    }
  }

  WiFiClientSecure client;
  client.setCACert(GITHUB_ROOT_CA_SIM);
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
    Serial.println("[OTA] Yetersiz alan!");
    http.end();
    return;
  }
  if (!Update.setMD5(md5.c_str())) {
    Serial.println("[OTA] MD5 set hatasi.");
    Update.abort();
    http.end();
    return;
  }
  Update.writeStream(*http.getStreamPtr());
  http.end();

  if (Update.end() && Update.isFinished()) {
    Serial.println("[OTA] BASARILI — restart...");
    delay(500);
    ESP.restart();
  } else {
    Serial.println("[OTA] Hata: " + String(Update.errorString()));
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String topicStr = String(topic);
  String msg = "";
  for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];
  Serial.println("[SIM] MQTT: " + topicStr + " -> " + msg);

  StaticJsonDocument<512> doc;
  if (deserializeJson(doc, msg) != DeserializationError::Ok) return;

  if (topicStr.endsWith("/control/ota")) {
    const char* action  = doc["action"]  | "";
    const char* version = doc["version"] | "";
    const char* url     = doc["url"]     | "";
    const char* md5     = doc["md5"]     | "";
    if (strcmp(action, "update") == 0 && strlen(url) > 0) {
      performOTA(String(url), String(version), String(md5));
    }
    return;
  }

  if (topicStr.endsWith("/control/reset")) {
    if (strcmp(doc["action"] | "", "reset_config") == 0) {
      Serial.println("[SIM] Reset komutu — NVS siliniyor.");
      prefs.begin("akilli-sinif", false);
      prefs.clear();
      prefs.end();
      delay(500);
      ESP.restart();
    }
  }
}

void setupMQTT() {
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setBufferSize(512);
}

void connectMQTT() {
  if (reconnectAttempts >= MAX_RECONNECT) {
    // Cooldown: backoff'a girdik, bir sonraki tetik PUBLISH_INTERVAL sonrasi.
    static unsigned long lastWarn = 0;
    if (millis() - lastWarn > 30000) {
      lastWarn = millis();
      Serial.printf("[MQTT] %d basarisiz deneme — backoff. WiFi yeniden baglaninca sifirlanir.\n", reconnectAttempts);
    }
    return;
  }

  Serial.print("[MQTT] Baglaniyor...");
  String willTopic = buildTopic("status", "connection");
  String willMsg   = "{\"status\":\"offline\",\"device\":\"" + mqttClientId + "\"}";

  if (mqttClient.connect(mqttClientId.c_str(), MQTT_USER, MQTT_PASSWORD,
                         willTopic.c_str(), 1, true, willMsg.c_str())) {
    mqttConnected     = true;
    reconnectAttempts = 0;
    Serial.println(" BAGLANDI!");
    // Single-cihaz + broadcast OTA / reset
    mqttClient.subscribe(buildTopic("control", "ota").c_str(),   1);
    mqttClient.subscribe(buildTopic("control", "reset").c_str(), 1);
    mqttClient.subscribe("akilli-sinif/all/control/ota",   1);
    mqttClient.subscribe("akilli-sinif/all/control/reset", 1);
    publishStatus("online");
  } else {
    Serial.printf(" HATA: %d\n", mqttClient.state());
    reconnectAttempts++;
  }
}

// ============================================
// CONFIG
// ============================================

void loadConfig() {
  prefs.begin("akilli-sinif", true);
  String broker  = prefs.getString("mqtt_broker",  "");
  String sinifId = prefs.getString("classroom_id", "sinif-2");
  prefs.end();

  broker.toCharArray(MQTT_BROKER,   sizeof(MQTT_BROKER));
  sinifId.toCharArray(CLASSROOM_ID, sizeof(CLASSROOM_ID));
  mqttClientId = "esp32-sim-" + sinifId;

  Serial.println("[Config] Sinif: " + sinifId + " | MQTT: " + broker);
}

// ============================================
// WIFI (WiFiManager)
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

// GPIO0 (BOOT) 5 sn basili tutulursa NVS silinir → portal yeniden acar
void checkConfigReset() {
  if (digitalRead(CONFIG_RESET_PIN) == LOW) {
    unsigned long pressStart = millis();
    Serial.println("[Config] Reset butonu algilandi, 5 sn basili tut...");
    while (digitalRead(CONFIG_RESET_PIN) == LOW) {
      if (millis() - pressStart >= RESET_HOLD_MS) {
        prefs.begin("akilli-sinif", false);
        prefs.clear();
        prefs.end();
        WiFi.disconnect(true, true);
        Serial.println("[Config] NVS + WiFi silindi! Yeniden baslatiliyor...");
        delay(1000);
        ESP.restart();
      }
      delay(100);
    }
  }
}

// ============================================
// RUNTIME PORTAL TETIKLEYICI (loop() icinden)
// ============================================
// Cihaz acikken BOOT (GPIO0) 5 sn basili tutulursa:
//   - Serial'a geri sayim 5..1 yazar (SIM'de TFT yok)
//   - NVS'e "force_portal" bayragi yazar
//   - Cihaz yeniden baslar, setupWiFi() WiFi portal acar
// Bayrak yaklasimi: classroom_id ve mqtt_broker silinmez, portal
// mevcut degerleri gosterir — sadece degistirmek istedigini gunceller.
unsigned long bootBtnPressStartMs   = 0;
bool          bootCountdownActive   = false;
int           lastBootCountdownShown = -1;

void checkRuntimePortalRequest() {
  bool pressed = (digitalRead(CONFIG_RESET_PIN) == LOW);

  if (!pressed) {
    if (bootCountdownActive) {
      Serial.println("[Portal] BOOT birakildi — iptal.");
      bootCountdownActive = false;
    }
    bootBtnPressStartMs    = 0;
    lastBootCountdownShown = -1;
    return;
  }

  unsigned long nowMs = millis();
  if (bootBtnPressStartMs == 0) {
    bootBtnPressStartMs = nowMs;
    return;
  }

  unsigned long held = nowMs - bootBtnPressStartMs;
  if (held < 400) return;  // 400 ms'den kisa basislari yoksay

  if (!bootCountdownActive) {
    bootCountdownActive    = true;
    lastBootCountdownShown = -1;
    Serial.println("[Portal] BOOT basili — WiFi portal icin 5 sn tut...");
  }

  int remaining = (int)((RESET_HOLD_MS - held) / 1000) + 1;
  if (remaining < 1) remaining = 1;
  if (remaining > 5) remaining = 5;

  if (remaining != lastBootCountdownShown) {
    Serial.printf("[Portal] %d...\n", remaining);
    lastBootCountdownShown = remaining;
  }

  if (held >= RESET_HOLD_MS) {
    Serial.println("[Portal] 5 sn doldu — bayrak yazilip yeniden baslatiliyor...");
    prefs.begin("akilli-sinif", false);
    prefs.putBool("force_portal", true);
    prefs.end();
    delay(500);
    ESP.restart();
  }
}

void setupWiFi() {
  WiFiManager wm;

  // ── GUVENLIK: WPA2 AP sifresi (MAC turevli)
  String apPass = makeApPassword();

  Serial.println("[WiFi] ===== AP BILGILERI =====");
  Serial.println("[WiFi] SSID: Akilli-SIM-Setup");
  Serial.println("[WiFi] WPA2 Sifre: " + apPass);
  Serial.println("[WiFi] ========================");

  WiFiManagerParameter p_broker("mqtt_broker",  "MQTT Broker IP",     MQTT_BROKER,  39);
  WiFiManagerParameter p_sinif ("classroom_id", "Sinif ID (sinif-2)", CLASSROOM_ID, 19);

  wm.addParameter(&p_broker);
  wm.addParameter(&p_sinif);
  wm.setConfigPortalTimeout(180);

  // NVS'teki force_portal bayragini oku (runtime BOOT 5sn ile set edilir)
  prefs.begin("akilli-sinif", false);
  bool forcePortalFlag = prefs.getBool("force_portal", false);
  if (forcePortalFlag) prefs.remove("force_portal");  // one-shot
  prefs.end();

  // Portal acma sartlari: bayrak, broker bos, acilista BOOT basili
  bool forcePortal = forcePortalFlag
                     || (strlen(MQTT_BROKER) == 0)
                     || (digitalRead(CONFIG_RESET_PIN) == LOW);

  bool connected;
  if (forcePortal) {
    Serial.println("[WiFi] Portal modu zorlandi.");
    connected = wm.startConfigPortal("Akilli-SIM-Setup", apPass.c_str());
  } else {
    connected = wm.autoConnect("Akilli-SIM-Setup", apPass.c_str());
  }

  if (!connected) {
    Serial.println("[WiFi] Baglanti basarisiz! Yeniden baslatiliyor...");
    delay(3000);
    ESP.restart();
    return;
  }

  String newBroker = String(p_broker.getValue());
  String newSinif  = String(p_sinif.getValue());

  if (newBroker.length() > 0 || newSinif.length() > 0) {
    prefs.begin("akilli-sinif", false);
    if (newBroker.length() > 0) prefs.putString("mqtt_broker",  newBroker);
    if (newSinif.length()  > 0) prefs.putString("classroom_id", newSinif);
    prefs.end();

    newBroker.toCharArray(MQTT_BROKER,   sizeof(MQTT_BROKER));
    newSinif.toCharArray( CLASSROOM_ID,  sizeof(CLASSROOM_ID));
    mqttClientId = "esp32-sim-" + newSinif;
  }

  Serial.println("[WiFi] BAGLANDI! IP: " + WiFi.localIP().toString());
}

// ============================================
// SETUP & LOOP
// ============================================

void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(CONFIG_RESET_PIN, INPUT_PULLUP);
  delay(50);
  checkConfigReset();
  loadConfig();

  Serial.println("\n\n");
  Serial.println("╔══════════════════════════════════════╗");
  Serial.println("║   AKILLI SINIF - SIMULATOR           ║");
  Serial.println("╠══════════════════════════════════════╣");
  Serial.print  ("║  Sinif:    "); Serial.println(CLASSROOM_ID);
  Serial.print  ("║  Firmware: v"); Serial.println(FIRMWARE_VERSION);
  Serial.println("╚══════════════════════════════════════╝\n");

  setupWiFi();
  setupMQTT();
  connectMQTT();

  Serial.println("[SIM] Simulator hazir!");
}

void loop() {
  unsigned long now = millis();

  // BOOT (GPIO0) 5 sn basili -> Serial countdown + WiFi portal acmak icin restart
  checkRuntimePortalRequest();

  if (now - lastWifiCheck >= WIFI_CHECK_INT) {
    lastWifiCheck = now;
    static wl_status_t prevWifi = WL_DISCONNECTED;
    wl_status_t curWifi = WiFi.status();
    if (curWifi != WL_CONNECTED) {
      WiFi.reconnect();
    } else if (prevWifi != WL_CONNECTED) {
      // WiFi yeni geri geldi — MQTT backoff'unu sifirla.
      reconnectAttempts = 0;
    }
    prevWifi = curWifi;
  }

  if (!mqttClient.connected()) {
    mqttConnected = false;
    if (now - lastMqttReconn >= MQTT_RECONNECT_INT) {
      lastMqttReconn = now;
      connectMQTT();
    }
  }
  mqttClient.loop();

  updateSimulation();

  if (now - lastPublish >= PUBLISH_INTERVAL) {
    lastPublish = now;
    if (mqttConnected) publishSensorData();
  }

  if (now - lastStatus >= STATUS_INTERVAL) {
    lastStatus = now;
    if (mqttConnected) publishStatus("online");
  }

  delay(100);
}
