/*
 * Akilli Sinif Sistemi - OTA Güncelleme Modülü (Implementasyon)
 * ==============================================================
 */

#include "ota_manager.h"
#include <ArduinoJson.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

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
    const char* md5     = doc["md5"]     | "";  // opsiyonel ama OZELLIKLE TAVSIYE

    // Sadece "update" komutunu işle
    if (strcmp(action, "update") != 0) return false;

    if (strlen(url) == 0) {
        Serial.println("[OTA] URL eksik!");
        publishStatus("failed", -1, version, "missing_url");
        return false;
    }

    // GUVENLIK: URL allowlist — sadece bu projenin GitHub release'leri kabul
    String urlStr(url);
    const char* ALLOWED_PREFIX = "https://github.com/MagiMigi/akilli-sinif/releases/";
    if (!urlStr.startsWith(ALLOWED_PREFIX)) {
        Serial.println("[OTA] URL allowlist disinda: " + urlStr);
        publishStatus("failed", -1, version, "url_not_allowed");
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
    Serial.println("[OTA] URL: " + urlStr);
    if (strlen(md5) > 0) Serial.println("[OTA] MD5 (payload): " + String(md5));
    else Serial.println("[OTA] MD5 payload'da yok, sidecar (.md5) indirilecek.");

    return performUpdate(urlStr, String(version), String(md5));
}

// ─────────────────────────────────────────────────────────────────────────────

// MD5 dosyasini ${url}.md5 adresinden HTTPS ile indir.
// GUVENLIK NOTU: setInsecure() kullaniyoruz cunku GitHub cert chain
// degisken (github.com=Sectigo, objects.githubusercontent.com=Let's Encrypt,
// codeload.github.com=DigiCert) ve tek root CA pin kirilgan. Tamper
// koruma: URL allowlist (sadece bu repo'nun release'leri) + MD5 sidecar
// + Update.setMD5 dogrulamasi. Bu akademik proje icin yeterli; uretim
// icin setCACertBundle (arduino-esp32 v2+) veya tum 3 root'u bundle'la.
// Bos doner = bulunamadi / hatali.
static String fetchMd5Sidecar(const String& binUrl) {
    const char* ALLOWED_PREFIX = "https://github.com/MagiMigi/akilli-sinif/releases/";
    String sidecarUrl = binUrl + ".md5";
    if (!sidecarUrl.startsWith(ALLOWED_PREFIX)) {
        Serial.println("[OTA] .md5 sidecar URL allowlist disinda: " + sidecarUrl);
        return "";
    }

    WiFiClientSecure mclient;
    mclient.setInsecure();  // GitHub cert chain degisken — MD5 + allowlist yeterli
    mclient.setTimeout(15);

    HTTPClient mhttp;
    mhttp.begin(mclient, sidecarUrl);
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

bool OTAManager::performUpdate(const String& url, const String& version,
                               const String& expectedMd5) {
    publishStatus("updating", 0, version);

    // GUVENLIK: MD5 zorunlu — payload'da yoksa sidecar (.md5) indir
    String md5 = expectedMd5;
    if (md5.length() == 0) {
        Serial.println("[OTA] MD5 sidecar indiriliyor: " + url + ".md5");
        md5 = fetchMd5Sidecar(url);
        if (md5.length() != 32) {
            Serial.println("[OTA] MD5 sidecar yok/gecersiz — guncelleme reddedildi.");
            publishStatus("failed", -1, version, "md5_unavailable");
            return false;
        }
        Serial.println("[OTA] Sidecar MD5: " + md5);
    }

    WiFiClientSecure client;
    client.setInsecure();  // GitHub cert chain degisken — MD5 + allowlist yeterli
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

    // GUVENLIK: MD5 dogrulama. Yanlis MD5 → Update.end() basarisiz olur.
    if (!Update.setMD5(md5.c_str())) {
        Serial.println("[OTA] MD5 set hatasi (gecersiz format).");
        publishStatus("failed", -1, version, "bad_md5_format");
        Update.abort();
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
