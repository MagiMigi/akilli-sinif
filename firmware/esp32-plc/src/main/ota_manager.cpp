/*
 * Akilli Sinif Sistemi - OTA Güncelleme Modülü (Implementasyon)
 * ==============================================================
 */

#include "ota_manager.h"
#include <ArduinoJson.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

// GitHub'ın root CA sertifikası (ISRG Root X1)
// openssl s_client -connect github.com:443 -showcerts komutuyla alınabilir
// Bu sertifika ~2035 yılına kadar geçerli
static const char* GITHUB_ROOT_CA = R"EOF(
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

// ─────────────────────────────────────────────────────────────────────────────

OTAManager::OTAManager(PubSubClient& client, const String& otaStatusTopic)
    : _mqtt(client), _statusTopic(otaStatusTopic) {}

// ─────────────────────────────────────────────────────────────────────────────

bool OTAManager::handleCommand(const String& payload) {
    StaticJsonDocument<512> doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (err) {
        Serial.println("[OTA] JSON parse hatasi: " + String(err.c_str()));
        return false;
    }

    const char* action  = doc["action"]  | "";
    const char* version = doc["version"] | "";
    const char* url     = doc["url"]     | "";

    // Sadece "update" komutunu işle
    if (strcmp(action, "update") != 0) return false;

    if (strlen(url) == 0) {
        Serial.println("[OTA] URL eksik!");
        publishStatus("failed", -1, version, "missing_url");
        return false;
    }

    // Zaten aynı versiyonda mı?
    if (String(version) == FIRMWARE_VERSION) {
        Serial.println("[OTA] Zaten en guncel versiyon: " + String(FIRMWARE_VERSION));
        publishStatus("up_to_date", -1, version);
        return false;
    }

    Serial.println("[OTA] Guncelleme basliyor...");
    Serial.println("[OTA] Hedef versiyon: " + String(version));
    Serial.println("[OTA] URL: " + String(url));

    return performUpdate(String(url), String(version));
}

// ─────────────────────────────────────────────────────────────────────────────

bool OTAManager::performUpdate(const String& url, const String& version) {
    publishStatus("updating", 0, version);

    WiFiClientSecure client;
    client.setInsecure();  // GitHub CDN sertifika zinciri değişkenliği nedeniyle doğrulama atlanıyor
    client.setTimeout(30);  // 30 saniye bağlantı timeout

    HTTPClient http;
    http.begin(client, url);
    http.setTimeout(60000);        // 60 sn indirme timeout
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS); // GitHub redirect'leri takip et

    Serial.println("[OTA] HTTP GET baslatiliyor...");
    int httpCode = http.GET();

    if (httpCode != HTTP_CODE_OK) {
        String errMsg = "http_error_" + String(httpCode);
        Serial.println("[OTA] HTTP hatasi: " + errMsg);
        publishStatus("failed", -1, version, errMsg);
        http.end();
        return false;
    }

    int contentLength = http.getSize();
    if (contentLength <= 0) {
        Serial.println("[OTA] Gecersiz content-length");
        publishStatus("failed", -1, version, "invalid_size");
        http.end();
        return false;
    }

    Serial.printf("[OTA] Dosya boyutu: %d bytes\n", contentLength);

    if (!Update.begin(contentLength)) {
        String errMsg = "not_enough_space";
        Serial.println("[OTA] Yetersiz alan! Hata: " + String(Update.errorString()));
        publishStatus("failed", -1, version, errMsg);
        http.end();
        return false;
    }

    // İndirme ve yazma — ilerlemeyi MQTT'ye bildir
    WiFiClient* stream = http.getStreamPtr();
    size_t written = 0;
    uint8_t buf[1024];
    int lastProgressReport = -1;

    while (http.connected() && written < (size_t)contentLength) {
        size_t available = stream->available();
        if (available) {
            size_t toRead = min(available, sizeof(buf));
            size_t r = stream->readBytes(buf, toRead);
            Update.write(buf, r);
            written += r;

            int progress = (written * 100) / contentLength;
            // Her %10'da bir bildir (MQTT flood önleme)
            if (progress / 10 != lastProgressReport / 10) {
                lastProgressReport = progress;
                publishStatus("updating", progress, version);
                Serial.printf("[OTA] İlerleme: %d%%\n", progress);
                _mqtt.loop();  // MQTT bağlantısını canlı tut
            }
        } else {
            delay(1);
        }
    }

    http.end();

    if (Update.end()) {
        if (Update.isFinished()) {
            Serial.println("[OTA] Guncelleme BASARILI! Yeniden baslatiliyor...");
            publishStatus("success", 100, version);
            delay(1000);  // MQTT mesajının gitmesi için bekle
            ESP.restart();
            return true;  // Buraya ulaşılamaz
        } else {
            Serial.println("[OTA] Guncelleme tamamlanamadi!");
            publishStatus("failed", -1, version, "incomplete");
            return false;
        }
    } else {
        String errMsg = String(Update.errorString());
        Serial.println("[OTA] Guncelleme hatasi: " + errMsg);
        publishStatus("failed", -1, version, errMsg);
        return false;
    }
}

// ─────────────────────────────────────────────────────────────────────────────

void OTAManager::publishStatus(const String& status, int progress,
                               const String& version, const String& error) {
    StaticJsonDocument<256> doc;
    doc["status"]           = status;
    doc["current_version"]  = FIRMWARE_VERSION;

    if (progress >= 0)       doc["progress"] = progress;
    if (version.length() > 0) doc["target_version"] = version;
    if (error.length() > 0)  doc["error"] = error;

    char buf[256];
    serializeJson(doc, buf);
    _mqtt.publish(_statusTopic.c_str(), buf, false);
}
