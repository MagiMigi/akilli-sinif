# Firmware Referansi

Bu dokuman uc ESP32 firmware'inin mimarisini, yapilandirmasini, WiFiManager portal kullanimi ve OTA guncelleme sistemini detayli aciklar.

Kaynak dosyalar:
- `firmware/esp32-plc/src/main/main.ino` — Ana kontrol unitesi
- `firmware/esp32-plc/src/main/ota_manager.h` / `ota_manager.cpp` — OTA modulu
- `firmware/esp32-cam/src/main/main.ino` — Kamera modulu
- `firmware/esp32-simulator/src/main/main.ino` — Test simulatoru
- `firmware/version.h` — Ortak versiyon tanimlari

---

## Firmware Turleri

| Firmware | Dosya | Board | Amac |
|----------|-------|-------|------|
| ESP32-PLC | `firmware/esp32-plc/src/main/main.ino` | ESP32 Dev Module | Sensor okuma, aktuator kontrolu, TFT ekran, OTA |
| ESP32-CAM | `firmware/esp32-cam/src/main/main.ino` | ESP32 Wrover (min_spiffs) | Kamera, YOLO'ya foto gonderme, OTA |
| ESP32-SIM | `firmware/esp32-simulator/src/main/main.ino` | ESP32 Dev Module | Sinus bazli simulasyon, donanımsız test |

---

## Versiyon Yonetimi

`firmware/version.h` dosyasi:

```c
#define FIRMWARE_VERSION_MAJOR 1
#define FIRMWARE_VERSION_MINOR 0
#define FIRMWARE_VERSION_PATCH 0
#define FIRMWARE_VERSION "1.0.0"
```

- Bu dosya **elle duzenlenmez**
- GitHub Actions, `v*.*.*` tag'ine gore otomatik gunceller
- MQTT status mesajlarinda `firmware_version` alani olarak gonderilir
- OTA guncelleme karsilastirmasi bu degeri kullanir

---

## WiFiManager Portal Sistemi

Uc firmware de WiFiManager kutuphanesini kullanir. Ilk acilista veya config sifirlandiktan sonra portal acar.

### Portal AP Adlari

| Firmware | AP Adi | Varsayilan IP |
|----------|--------|---------------|
| PLC | Akilli-Sinif-Setup | 192.168.4.1 |
| CAM | Akilli-CAM-Setup | 192.168.4.1 |
| Simulator | Akilli-SIM-Setup | 192.168.4.1 |

### Portal Akisi

1. ESP32 acilir
2. NVS'den config okunur
3. Config bossa (broker IP yok) VEYA GPIO0 basili > **portal acar**
4. Config varsa > WiFi'ya baglanmayi dener > basarisizsa portal acar
5. Kullanici telefon/bilgisayardan AP'ye baglanir
6. 192.168.4.1 otomatik acar (captive portal)
7. WiFi + ek alanlar doldurulur
8. **Kaydet** > NVS'e yazilir > ESP32 yeniden baslar

### Portal Timeout

| Firmware | Timeout | Portal Acilmama Durumu |
|----------|---------|------------------------|
| PLC | 180 sn (3 dk) | ESP32 yeniden baslar |
| CAM | 180 sn (3 dk) | ESP32 yeniden baslar |
| Simulator | 180 sn (3 dk) | ESP32 yeniden baslar |

### Portal Ek Alanlari

**PLC:**

| Alan | NVS Key | Varsayilan |
|------|---------|-----------|
| MQTT Broker IP | `mqtt_broker` | (bos) |
| Sinif ID | `classroom_id` | sinif-1 |
| Mock Modu (true/false) | `mock_mode` | false |

**CAM:**

| Alan | NVS Key | Varsayilan |
|------|---------|-----------|
| AI Server URL | `server_url` | (bos) |
| Sinif ID | `classroom_id` | sinif-1 |
| API Anahtari | `api_key` | (bos) |
| MQTT Broker IP | `mqtt_broker` | 192.168.1.100 |

**Simulator:**

| Alan | NVS Key | Varsayilan |
|------|---------|-----------|
| MQTT Broker IP | `mqtt_broker` | (bos) |
| Sinif ID | `classroom_id` | sinif-2 |

---

## Config Sifirlama

Uc yontem vardir:

### 1. Fiziksel: GPIO0 Butonu (Tum firmware'lar)

1. ESP32 acikken GPIO0 (BOOT) butonuna bas
2. 5 saniye basili tut (PLC'de TFT'de geri sayim gorulur)
3. NVS silinir + WiFi kimlik bilgileri silinir
4. ESP32 yeniden baslar > portal acar

### 2. MQTT Komutu (PLC)

```bash
mosquitto_pub -h localhost -u nodered -P "$NODERED_PASS" \
  -t "akilli-sinif/sinif-1/control/reset" \
  -m '{"action":"reset_config"}'
```

- PLC'de TFT ekranda "Uzaktan Reset!" gosterilir
- NVS + WiFi silinir
- ESP32 yeniden baslar > portal acar

### 3. HTTP Endpoint (Sadece CAM)

```
http://<ESP32-CAM-IP>/reset-config
```

- Tarayicidan erisilir
- NVS silinir
- ESP32 yeniden baslar > portal acar
- IP adresini ogrenmek icin: `akilli-sinif/<sinif_id>/status/ip` MQTT topic'ini dinle

---

## ESP32-PLC Firmware Detaylari

### Sensor Okuma

Iki mod vardir: **gercek donanim** ve **mock (simulasyon)**.

**Gercek donanim modu** (`MOCK_MODE = false`):

| Sensor | Okuma Yontemi | Donusum |
|--------|---------------|---------|
| DHT11 | `dht.readTemperature()` / `dht.readHumidity()` | Direkt C / % |
| LDR | `analogRead(34)` | ADC 0-4095 > map 0-1000 lux |
| MQ-135 | `analogRead(35)` | ADC 0-4095 > map 0-500 ppm |
| PIR | `digitalRead(27)` | HIGH = hareket var |
| Reed | `digitalRead(26)` | HIGH = pencere acik (pull-up) |

**Mock modu** (`MOCK_MODE = true`):

- Sicaklik: 20-28C arasi, yavas degisim (+-0.1 adim)
- Nem: 40-60% arasi
- Isik: Gun/gece dongusune gore 0-1000
- Hava: 50-300 ppm arasi
- Hareket: Rastgele (her 10 okumada bir degisim)
- Pencere: Nadiren degisir (her 50 okumada bir)

### Aktuator Kontrolu

| Aktuator | GPIO | PWM Freq | Resolution | Aralik |
|----------|------|----------|------------|--------|
| LED | 13 | 500 Hz | 8-bit | 0-100% > map 0-255 |
| Fan | 12 | 500 Hz | 8-bit | 0-100% > map 0-255 |

ESP32 Arduino Core 3.x API kullanilir:
- `ledcAttach(pin, freq, resolution)` — setup'ta
- `ledcWrite(pin, value)` — kontrol sirasinda

### Otomatik Uyari Sistemi

`checkAutoAlerts()` fonksiyonu her ekran guncellemesinde calisir:

| Kosul | Uyari Seviyesi | Ekran |
|-------|----------------|-------|
| Sicaklik > 32C | DANGER | Kirmizi yanip sonen |
| Sicaklik 28-32C | WARNING | Sari arka plan |
| Sicaklik < 27C | NONE | Normal |

> **Not:** Hava kalitesi icin tam ekran uyari yapilmaz — sadece ppm degeri renk ile gosterilir.

### Zamanlama Araliklari

| Islem | Aralik | Aciklama |
|-------|--------|----------|
| Sensor okuma | 2 sn | `SENSOR_INTERVAL` |
| MQTT yayinlama | 5 sn | `PUBLISH_INTERVAL` |
| Durum yayinlama | 30 sn | `STATUS_INTERVAL` |
| WiFi kontrolu | 10 sn | `WIFI_CHECK_INTERVAL` |
| Ekran guncelleme | 2 sn | `DISPLAY_INTERVAL` |

### MQTT Yeniden Baglanti

- Maks deneme: 5
- Denemeler arasi bekleme: 5 sn
- 5 basarisiz denemeden sonra sayac sifirlanir ve tekrar denir
- `loop()` icinde cok hizli yeniden deneme `MQTT_RECONNECT_INTERVAL` ile onlenir

---

## ESP32-CAM Firmware Detaylari

### Foto Cekim ve Gonderim Dongusu

1. `esp_camera_fb_get()` ile JPEG frame al
2. HTTP POST ile YOLO sunucusuna gonder (`SERVER_URL`)
3. Header'a `X-Classroom-ID` ve `X-API-Key` ekle
4. Sunucu yaniti: `{"person_count": N}`
5. Kisi varsa cekim araligi 10 sn, yoksa 60 sn
6. Frame buffer'i serbest birak

### Hata Yonetimi

- Ardisik hata sayaci: `consecutiveFailures`
- 5 ardisik hatada ESP32-CAM yeniden baslar
- Basarili gonderimde sayac sifirlanir

### WiFi Kopunca Otomatik Portal

- WiFi kopunca `wifiFailCount` artar
- 10 basarisiz deneme (~50 sn) sonra portal acar
- Portal 5 dakika acik kalir
- Mevcut ayarlar (API key, server URL) korunur, sadece WiFi guncellenir
- Portal kapandiktan sonra MQTT tekrar baglantı kurar

### MQTT Canli Config Guncelleme

`akilli-sinif/{sinif_id}/control/config` topic'ini dinler.

Desteklenen alanlar:
- `api_key` — YOLO sunucu auth anahtari
- `server_url` — YOLO sunucu adresi
- `mqtt_broker` — MQTT broker IP (restart gerekebilir)

Degisiklikler aninda NVS'e kaydedilir, restart gerekmez.

---

## ESP32-Simulator Firmware Detaylari

### Sinus Bazli Simulasyon

Simulator "sanal saat" kavrami kullanir: 1 gercek saniye = 1 sanal dakika.
Baslangic: 08:00 (simTime = 480).

| Sensor | Min | Max | Periyot | Aciklama |
|--------|-----|-----|---------|----------|
| Sicaklik | 18C | 28C | 24 saat | Gece dusuk, ogle yuksek |
| Nem | 40% | 60% | 24 saat | Sicaklikla ters korelasyon |
| Isik | 0 | 900 lux | Gunduz/gece | 07:00-17:30 arasi aydinlik |
| Hava | 50-80 ppm | 80-180 ppm | — | Ders saatlerinde (08-12, 13-17) artar |
| Hareket | — | — | — | Ders saatlerinde true |
| Kisi | 0 | 25-30 | — | Ders saatlerinde 20+-5 |
| Pencere | — | — | — | Rastgele, nadir degisir |

### Yazilim Ici Otomasyon

Simulator kendi LED ve fan degerlerini hesaplar:

**LED:**
- Hareket var + isik < 200 lux > %80
- Hareket var + isik < 400 lux > %40
- Diger > %0

**Fan:**
- Sicaklik > 26C veya hava > 200 ppm > %70
- Sicaklik > 24C > %30
- Diger > %0

---

## OTA (Over-the-Air) Guncelleme

### Genel Akis

```
git tag v1.2.0
git push origin main --tags
        |
        v
GitHub Actions (build-firmware.yml)
        |
        v
3 firmware binary derlenir (PLC, CAM, SIM)
        |
        v
GitHub Release olusur (firmware-plc-v1.2.0.bin, ...)
        |
        v
Node-RED "Manuel Kontrol" ile son surumu cekilir
        |
        v
MQTT komutu gonderilir: akilli-sinif/{id}/control/ota
        |
        v
ESP32 binary'yi HTTPS ile indirir ve flash'a yazar
        |
        v
ESP32 yeniden baslar (yeni firmware)
```

### GitHub Actions CI/CD

Dosya: `.github/workflows/build-firmware.yml`

Tetikleme: `v*.*.*` formatinda tag push

Derlenen firmware'lar:

| Firmware | Artifact | Board FQBN |
|----------|----------|------------|
| PLC | firmware-plc-v*.*.*.bin | esp32:esp32:esp32 |
| CAM | firmware-cam-v*.*.*.bin | esp32:esp32:esp32wrover:PartitionScheme=min_spiffs |
| SIM | firmware-sim-v*.*.*.bin | esp32:esp32:esp32 |

CI sirasinda `version.h` tag degerine gore guncellenir.

### PLC OTA Modulu

`ota_manager.h` / `ota_manager.cpp` dosyalari.

Ozellikler:
- HTTPS ile GitHub'dan binary indirir (ISRG Root X1 CA sertifikasi gomulu)
- Redirect'leri takip eder (`HTTPC_STRICT_FOLLOW_REDIRECTS`)
- Indirme ilerlemesini %10 aralikla MQTT'ye bildirir
- Zaten ayni versiyondaysa guncelleme yapmaz
- 60 sn indirme timeout, 30 sn baglanti timeout

### CAM OTA

CAM firmware'i daha basit bir HTTP OTA mekanizmasi kullanir:
- `HTTPClient` + `Update` API
- HTTPS yerine HTTP (yerel ag icin yeterli)
- `min_spiffs` partition tablosu kullanilmali (PSRAM ve flash kisitli)

### OTA MQTT Mesaj Formati

**Komut (Node-RED > ESP32):**
```json
{
  "action": "update",
  "version": "v1.2.0",
  "url": "https://github.com/MagiMigi/akilli-sinif/releases/download/v1.2.0/firmware-plc-v1.2.0.bin"
}
```

**Durum (ESP32 > Node-RED):**
```json
{"status": "updating", "progress": 50, "current_version": "1.0.0", "target_version": "v1.2.0"}
{"status": "success", "progress": 100, "current_version": "1.0.0", "target_version": "v1.2.0"}
{"status": "failed", "current_version": "1.0.0", "target_version": "v1.2.0", "error": "http_error_404"}
{"status": "up_to_date", "current_version": "1.0.0", "target_version": "1.0.0"}
```

### OTA Sirasinda

- WiFi ve MQTT ayarlari korunur (NVS silinmez)
- Portal acilmaz
- Guncelleme basarili olursa ESP32 otomatik yeniden baslar
- Basarisiz olursa mevcut firmware ile calismaya devam eder

---

## Ilk Firmware Yukleme (Kablo ile)

> Sadece ilk kurulumda gerekir. Sonraki tum guncellemeler OTA ile yapilir.

1. Arduino IDE'de ilgili `.ino` dosyasini ac:

   | Cihaz | Dosya |
   |-------|-------|
   | PLC | `firmware/esp32-plc/src/main/main.ino` |
   | CAM | `firmware/esp32-cam/src/main/main.ino` |
   | SIM | `firmware/esp32-simulator/src/main/main.ino` |

2. Board ayari:
   - PLC / SIM: **ESP32 Dev Module**
   - CAM: **AI Thinker ESP32-CAM** (veya ESP32 Wrover Module, Partition: min_spiffs)

3. Port: `/dev/ttyUSB0` (veya ESP32'nin bagli oldugu port)

4. **Upload** > ESP32'deki BOOT butonuna basili tut > "Connecting..." gorunce birak

5. Upload tamamlandiktan sonra Serial Monitor ac (115200 baud):
   ```
   ╔══════════════════════════════════════╗
   ║     AKILLI SINIF SISTEMI              ║
   ║     ESP32 PLC + TFT Display          ║
   ╠══════════════════════════════════════╣
   ║  Sinif:    sinif-1                    ║
   ║  Mod:      SIMULASYON                 ║
   ║  Firmware: v1.0.0                     ║
   ╚══════════════════════════════════════╝
   ```

6. Portal'a baglan ve ayarlari gir (bkz. WiFiManager Portal Sistemi)
