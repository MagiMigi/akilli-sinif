/*
 * Akilli Sinif - ESP32 Relay-Test Firmware
 * =========================================
 * Ana akilli-sinif PLC projesinden AYRI minimal modul.
 * 2 GPIO output ile 2 roleyi (cooling + heating) ON/OFF kontrol eder.
 *
 * Donanim hatti (her role icin):
 *   GPIO 21/22 -> 1k -> BC337 base (NPN low-side switch)
 *   BC337 emitter -> GND, collector -> 5V role bobini
 *   1N4007 flyback diyot bobine ters paralel
 *   Role NO/COM kontagi 12V hatti siviçler:
 *     Role 1 (cooling): 12V + DC fan + LED
 *     Role 2 (heating): 12V + 22ohm direnç + LED
 *
 * MQTT Topic Yapisi:
 *   Publish:    akilli-sinif/relay-test/{device_id}/status/{connection|cooling|heating|ota}
 *   Subscribe:  akilli-sinif/relay-test/{device_id}/control/{cooling|heating|ota|reset}
 *               akilli-sinif/relay-test/all/control/{cooling|heating|ota|reset}
 *
 * Ilk Kurulum:
 *   1. Flash et
 *   2. "Relay-Test-Setup" WiFi agina bagla (WPA2 sifre: "relay-XXXXXX" -> MAC son 3 byte)
 *   3. 192.168.4.1 portal: WiFi + MQTT Broker IP + Device ID gir
 *
 * Config Sifirlama:
 *   GPIO0 (BOOT) 5 sn basili -> NVS silinir, portal acilir
 *   VEYA MQTT: control/reset {"action":"reset_config"}
 */

#include <WiFi.h>
#include <WiFiManager.h>
#include <Preferences.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "ota_manager.h"
#include "../../../version.h"
#include "../../../secrets.h"

// ============================================
// PIN TANIMLARI
// ============================================
const int PIN_COOLING = 21;   // Role 1 - cooling (DC fan + LED)
const int PIN_HEATING = 22;   // Role 2 - heating (22ohm + LED)

// BC337 NPN low-side switch: GPIO HIGH -> transistor saturates -> role enerjili
#define RELAY_ACTIVE_LEVEL HIGH

const int CONFIG_RESET_PIN          = 0;     // GPIO0 = BOOT butonu
const unsigned long RESET_HOLD_MS   = 5000;  // 5 sn

// ============================================
// CONFIG (NVS'den dolar)
// ============================================
Preferences prefs;

char MQTT_BROKER[40] = "";
char DEVICE_ID[24]   = "relay-test-1";

const int   MQTT_PORT      = 1883;
const char* MQTT_USER      = MQTT_USER_DEFAULT;
const char* MQTT_PASSWORD  = MQTT_PASSWORD_DEFAULT;

String mqttClientId;     // "relay-test-1" (DEVICE_ID ile ayni)
String otaStatusTopic;   // "akilli-sinif/relay-test/{id}/status/ota"

// ============================================
// GLOBALS
// ============================================
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
OTAManager* otaManager = nullptr;

bool wifiConnected = false;
bool mqttConnected = false;
int  reconnectAttempts = 0;
const int MAX_RECONNECT_ATTEMPTS = 5;
unsigned long lastMqttReconnectAttempt = 0;
const unsigned long MQTT_RECONNECT_INTERVAL = 5000;

unsigned long lastWifiCheck = 0;
unsigned long lastStatusPublish = 0;
const unsigned long WIFI_CHECK_INTERVAL = 10000;
const unsigned long STATUS_INTERVAL     = 30000;

// Role durumlari (status echo'da kullanilir)
bool coolingOn = false;
bool heatingOn = false;

// ============================================
// TOPIC HELPER
// ============================================
String buildTopic(const char* type, const char* name) {
  // Ornek: akilli-sinif/relay-test/relay-test-1/control/cooling
  return String("akilli-sinif/relay-test/") + DEVICE_ID + "/" + type + "/" + name;
}

// ============================================
// CONFIG: NVS LOAD/SAVE
// ============================================
void loadConfig() {
  prefs.begin("relay-test", true);
  String broker = prefs.getString("mqtt_broker", "");
  String devId  = prefs.getString("device_id",   "relay-test-1");
  prefs.end();

  broker.toCharArray(MQTT_BROKER, sizeof(MQTT_BROKER));
  devId.toCharArray(DEVICE_ID,    sizeof(DEVICE_ID));

  mqttClientId   = String(DEVICE_ID);
  otaStatusTopic = String("akilli-sinif/relay-test/") + DEVICE_ID + "/status/ota";

  Serial.println("[Config] Device ID: " + devId);
  Serial.println("[Config] MQTT Broker: " + broker);
  Serial.println("[Config] Firmware: v" + OTAManager::getCurrentVersion());
}

// GPIO0 5 sn basili -> NVS sil, restart (non-blocking, loop'tan cagrilir)
void checkConfigReset() {
  static unsigned long pressStart = 0;
  static bool pressed = false;

  if (digitalRead(CONFIG_RESET_PIN) == LOW) {
    if (!pressed) {
      pressed = true;
      pressStart = millis();
      Serial.println("[Config] Reset butonu algilandi, 5 sn basili tut...");
    } else if (millis() - pressStart >= RESET_HOLD_MS) {
      prefs.begin("relay-test", false);
      prefs.clear();
      prefs.end();
      WiFi.disconnect(true, true);
      Serial.println("[Config] NVS + WiFi silindi! Portal acilacak...");
      delay(500);
      ESP.restart();
    }
  } else {
    if (pressed) Serial.println("[Config] Buton birakildi, iptal.");
    pressed = false;
  }
}

// ============================================
// WIFI / WIFIMANAGER
// ============================================
String makeApPassword() {
  uint8_t mac[6];
  WiFi.macAddress(mac);
  char buf[16];
  snprintf(buf, sizeof(buf), "relay-%02X%02X%02X", mac[3], mac[4], mac[5]);
  return String(buf);
}

void setupWiFi() {
  Serial.println("\n[WiFi] WiFiManager basliyor...");
  WiFiManager wm;

  String apPass = makeApPassword();
  Serial.println("[WiFi] AP SSID: Relay-Test-Setup");
  Serial.println("[WiFi] AP WPA2 Sifre: " + apPass);

  WiFiManagerParameter p_mqttBroker("mqtt_broker", "MQTT Broker IP",
                                     MQTT_BROKER, 39);
  WiFiManagerParameter p_deviceId  ("device_id",   "Device ID (relay-test-1, vs.)",
                                     DEVICE_ID,  23);

  wm.addParameter(&p_mqttBroker);
  wm.addParameter(&p_deviceId);
  wm.setConfigPortalTimeout(180);

  bool forcePortal = (strlen(MQTT_BROKER) == 0) || (digitalRead(CONFIG_RESET_PIN) == LOW);
  bool connected;
  if (forcePortal) {
    Serial.println("[WiFi] Portal modu - config gerekli.");
    connected = wm.startConfigPortal("Relay-Test-Setup", apPass.c_str());
  } else {
    connected = wm.autoConnect("Relay-Test-Setup", apPass.c_str());
  }

  if (!connected) {
    Serial.println("[WiFi] Baglanti yok, restart...");
    delay(3000);
    ESP.restart();
    return;
  }

  // Portal'dan gelen yeni degerleri NVS'e yaz
  String newBroker = String(p_mqttBroker.getValue());
  String newDevId  = String(p_deviceId.getValue());

  if (newBroker.length() > 0 || newDevId.length() > 0) {
    prefs.begin("relay-test", false);
    if (newBroker.length() > 0) prefs.putString("mqtt_broker", newBroker);
    if (newDevId.length()  > 0) prefs.putString("device_id",   newDevId);
    prefs.end();

    newBroker.toCharArray(MQTT_BROKER, sizeof(MQTT_BROKER));
    newDevId.toCharArray(DEVICE_ID,    sizeof(DEVICE_ID));
    mqttClientId   = String(DEVICE_ID);
    otaStatusTopic = String("akilli-sinif/relay-test/") + DEVICE_ID + "/status/ota";
  }

  wifiConnected = true;
  Serial.print("[WiFi] BAGLANDI. IP: ");
  Serial.println(WiFi.localIP());
}

void checkWiFi() {
  if (WiFi.status() != WL_CONNECTED) {
    wifiConnected = false;
    Serial.println("[WiFi] Baglanti koptu, reconnect...");
    WiFi.reconnect();
    delay(3000);
    if (WiFi.status() == WL_CONNECTED) {
      wifiConnected = true;
      Serial.println("[WiFi] Yeniden baglandi.");
    }
  }
}

// ============================================
// ROLE KONTROL + STATUS ECHO
// ============================================
void publishRelayStatus(const char* name, bool on) {
  StaticJsonDocument<96> doc;
  doc["state"] = on ? "on" : "off";
  doc["ts"]    = millis();
  char buf[96];
  serializeJson(doc, buf);
  String topic = buildTopic("status", name);
  mqttClient.publish(topic.c_str(), buf, true);  // retained
}

void setRelay(int pin, bool on, const char* name, bool* stateVar) {
  digitalWrite(pin, on ? RELAY_ACTIVE_LEVEL : !RELAY_ACTIVE_LEVEL);
  *stateVar = on;
  Serial.print("[Role] ");
  Serial.print(name);
  Serial.println(on ? " ON" : " OFF");
  if (mqttClient.connected()) publishRelayStatus(name, on);
}

// {"state":"on"} / {"state":"off"} / {"state":true} / {"state":false}
bool parseOnOff(JsonDocument& doc, bool& out) {
  if (doc.containsKey("state")) {
    if (doc["state"].is<const char*>()) {
      const char* s = doc["state"];
      if (strcmp(s, "on") == 0)  { out = true;  return true; }
      if (strcmp(s, "off") == 0) { out = false; return true; }
    }
    if (doc["state"].is<bool>()) {
      out = doc["state"].as<bool>();
      return true;
    }
  }
  return false;
}

// ============================================
// MQTT
// ============================================
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message;
  message.reserve(length);
  for (unsigned int i = 0; i < length; i++) message += (char)payload[i];

  Serial.println("\n>>> MQTT: " + String(topic));
  Serial.println("    Payload: " + message);

  StaticJsonDocument<512> doc;
  DeserializationError err = deserializeJson(doc, message);
  if (err) {
    Serial.println("[MQTT] JSON parse hatasi: " + String(err.c_str()));
    return;
  }

  String t = String(topic);

  if (t.endsWith("/control/cooling")) {
    bool on;
    if (parseOnOff(doc, on)) setRelay(PIN_COOLING, on, "cooling", &coolingOn);
  }
  else if (t.endsWith("/control/heating")) {
    bool on;
    if (parseOnOff(doc, on)) setRelay(PIN_HEATING, on, "heating", &heatingOn);
  }
  else if (t.endsWith("/control/ota")) {
    Serial.println("[OTA] Komut alindi.");
    if (otaManager != nullptr) otaManager->handleCommand(message);
  }
  else if (t.endsWith("/control/reset")) {
    if (doc["action"] == "reset_config") {
      Serial.println("[Reset] Uzaktan config reset!");
      prefs.begin("relay-test", false);
      prefs.clear();
      prefs.end();
      WiFi.disconnect(true, true);
      delay(500);
      ESP.restart();
    }
  }
}

void publishConnectionStatus(const char* status) {
  StaticJsonDocument<256> doc;
  doc["status"]           = status;
  doc["device"]           = mqttClientId;
  doc["ip"]               = WiFi.localIP().toString();
  doc["rssi"]             = WiFi.RSSI();
  doc["uptime"]           = millis() / 1000;
  doc["firmware_version"] = FIRMWARE_VERSION;

  char buf[256];
  serializeJson(doc, buf);
  String topic = buildTopic("status", "connection");
  mqttClient.publish(topic.c_str(), buf, true);  // retained
}

void subscribeToControlTopics() {
  String topics[] = {
    buildTopic("control", "cooling"),
    buildTopic("control", "heating"),
    buildTopic("control", "ota"),
    buildTopic("control", "reset"),
    "akilli-sinif/relay-test/all/control/cooling",
    "akilli-sinif/relay-test/all/control/heating",
    "akilli-sinif/relay-test/all/control/ota",
    "akilli-sinif/relay-test/all/control/reset"
  };
  Serial.println("\n[MQTT] Subscribe edilen topic'ler:");
  const int n = sizeof(topics) / sizeof(topics[0]);
  for (int i = 0; i < n; i++) {
    if (mqttClient.subscribe(topics[i].c_str(), 1)) {  // QoS 1 (OTA icin)
      Serial.println("  [OK] " + topics[i]);
    } else {
      Serial.println("  [HATA] " + topics[i]);
    }
  }
}

void setupMQTT() {
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setBufferSize(1024);  // OTA komutlari icin

  if (otaManager != nullptr) delete otaManager;
  otaManager = new OTAManager(mqttClient, otaStatusTopic);
  Serial.println("[OTA] Manager hazir. Firmware v" + OTAManager::getCurrentVersion());
}

void connectMQTT() {
  Serial.println("\n[MQTT] Broker: " + String(MQTT_BROKER) + ":" + String(MQTT_PORT));

  while (!mqttClient.connected() && reconnectAttempts < MAX_RECONNECT_ATTEMPTS) {
    Serial.printf("[MQTT] Deneme %d/%d...\n", reconnectAttempts + 1, MAX_RECONNECT_ATTEMPTS);

    String willTopic   = buildTopic("status", "connection");
    String willMessage = String("{\"status\":\"offline\",\"device\":\"") + mqttClientId + "\"}";

    if (mqttClient.connect(mqttClientId.c_str(), MQTT_USER, MQTT_PASSWORD,
                           willTopic.c_str(), 1, true, willMessage.c_str())) {
      mqttConnected = true;
      reconnectAttempts = 0;
      Serial.println("[MQTT] BAGLANDI!");
      subscribeToControlTopics();
      publishConnectionStatus("online");
      // Mevcut role durumlarini retained olarak yeniden yayinla (dashboard senkronu icin)
      publishRelayStatus("cooling", coolingOn);
      publishRelayStatus("heating", heatingOn);
    } else {
      Serial.print("[MQTT] BASARISIZ, kod: ");
      Serial.println(mqttClient.state());
      reconnectAttempts++;
      delay(3000);
    }
  }

  if (!mqttClient.connected()) {
    Serial.println("[MQTT] Tekrar denenecek (5 sn).");
    reconnectAttempts = 0;
  }
}

// ============================================
// SETUP / LOOP
// ============================================
void setup() {
  Serial.begin(115200);
  delay(500);

  // Reset butonu (TFT yok, direkt kontrol)
  pinMode(CONFIG_RESET_PIN, INPUT_PULLUP);

  // Roleler GUVENLI baslangic: KAPALI
  pinMode(PIN_COOLING, OUTPUT);
  pinMode(PIN_HEATING, OUTPUT);
  digitalWrite(PIN_COOLING, !RELAY_ACTIVE_LEVEL);
  digitalWrite(PIN_HEATING, !RELAY_ACTIVE_LEVEL);

  Serial.println("\n========================================");
  Serial.println("  AKILLI SINIF - Relay Test Modulu");
  Serial.print  ("  Firmware: v");
  Serial.println(FIRMWARE_VERSION);
  Serial.println("========================================");

  loadConfig();
  setupWiFi();
  setupMQTT();
  connectMQTT();

  Serial.println("[System] HAZIR.");
}

void loop() {
  unsigned long now = millis();

  checkConfigReset();

  if (now - lastWifiCheck >= WIFI_CHECK_INTERVAL) {
    lastWifiCheck = now;
    checkWiFi();
  }

  if (!mqttClient.connected()) {
    mqttConnected = false;
    if (now - lastMqttReconnectAttempt >= MQTT_RECONNECT_INTERVAL) {
      lastMqttReconnectAttempt = now;
      connectMQTT();
    }
  }
  mqttClient.loop();

  if (now - lastStatusPublish >= STATUS_INTERVAL) {
    lastStatusPublish = now;
    if (mqttConnected) publishConnectionStatus("online");
  }
}
