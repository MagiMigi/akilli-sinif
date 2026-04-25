/*
 * Akilli Sinif Sistemi - OTA Güncelleme Modülü
 * ==============================================
 *
 * MQTT komutu ile tetiklenen HTTP OTA güncellemesi.
 * GitHub Releases'ten .bin dosyasını indirir ve uygular.
 *
 * Kullanım:
 *   MQTT topic: akilli-sinif/{sinif_id}/control/ota
 *   MQTT payload: {"action":"update","version":"v1.2.0","url":"https://github.com/.../firmware-plc.bin"}
 *
 * Durum bildirimleri:
 *   MQTT topic: akilli-sinif/{sinif_id}/status/ota
 *   Payload: {"status":"updating","progress":45} / {"status":"success"} / {"status":"failed"}
 */

#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <Update.h>
#include <PubSubClient.h>
#include "../../../version.h"

class OTAManager {
public:
    // otaStatusTopic: MQTT'ye ilerleme bildirmek için
    // client: mevcut MQTT client referansı
    OTAManager(PubSubClient& client, const String& otaStatusTopic);

    // MQTT'den gelen OTA komutunu işle
    // payload örneği: {"action":"update","version":"v1.2.0","url":"https://..."}
    // Dönüş: true = güncelleme başlatıldı
    bool handleCommand(const String& payload);

    // Mevcut firmware versiyonu (MQTT status mesajlarında kullanılır)
    static String getCurrentVersion() { return String(FIRMWARE_VERSION); }

private:
    PubSubClient& _mqtt;
    String _statusTopic;

    // HTTPS ile .bin indir ve OTA uygula. expectedMd5 bossa MD5 dogrulama
    // atlanir (eski Node-RED flow'lariyla uyumluluk icin); doluysa zorunlu.
    bool performUpdate(const String& url, const String& version,
                       const String& expectedMd5 = "");

    // MQTT'ye OTA durum mesajı gönder
    void publishStatus(const String& status, int progress = -1,
                       const String& version = "", const String& error = "");
};

#endif // OTA_MANAGER_H
