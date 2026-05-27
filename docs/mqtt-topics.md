# MQTT Topic Yapisi

Bu dokuman, Akilli Sinif Sistemi'ndeki tum MQTT topic'lerini, payload formatlarini ve QoS ayarlarini tanimlar.

Icerik koddan turetilmistir. Kaynak dosyalar: `firmware/esp32-plc/src/main/main.ino`, `firmware/esp32-cam/src/main/main.ino`, `firmware/esp32-simulator/src/main/main.ino`, `server/ai-processing/yolo_server.py`.

## Genel Yapi

```
akilli-sinif/{sinif_id}/{kategori}/{alt_konu}
```

| Segment | Aciklama | Ornekler |
|---------|----------|----------|
| `sinif_id` | Cihaz portal'dan alir, NVS'e kaydeder | `sinif-1`, `sinif-2` |
| `kategori` | Mesaj turu | `sensors`, `control`, `actuators`, `status` |
| `alt_konu` | Alt konu | `temperature`, `led`, `ota`, `connection` |

**Ozel broadcast adresi:** `akilli-sinif/all/control/...` — tum cihazlara ayni anda komut gonderir.

---

## 1. Sensor Verileri (ESP32 > Sunucu)

PLC ve Simulator firmware'i tarafindan yayinlanir. Kategori: `sensors`.

| Topic | Aciklama | Payload |
|-------|----------|---------|
| `akilli-sinif/{id}/sensors/temperature` | Sicaklik | `{"value": 23.5, "unit": "C", "timestamp": 123456}` |
| `akilli-sinif/{id}/sensors/humidity` | Nem | `{"value": 65, "unit": "%", "timestamp": 123456}` |
| `akilli-sinif/{id}/sensors/light` | Isik seviyesi (LDR) | `{"value": 450, "unit": "lux", "timestamp": 123456}` |
| `akilli-sinif/{id}/sensors/air_quality` | Hava kalitesi (MQ-135) | `{"value": 120, "unit": "ppm", "timestamp": 123456}` |
| `akilli-sinif/{id}/sensors/pir` | Hareket algilama (PIR) | `{"detected": true, "timestamp": 123456}` |
| `akilli-sinif/{id}/sensors/window` | Pencere durumu (Reed Switch) | `{"open": false, "timestamp": 123456}` |
| `akilli-sinif/{id}/sensors/camera` | Kisi sayisi (YOLO sonucu) | `{"person_count": 15, "timestamp": 123456}` |

**Notlar:**
- `timestamp`: `millis()` degeri (ESP32 uptime, ms cinsinden). YOLO sunucusu ise Unix timestamp (ms) kullanir.
- PIR: `detected: true` = hareket var
- Window: `open: true` = pencere acik (klima kontrolu icin onemli)
- Camera topic'ine iki kaynak yazar: YOLO sunucusu (HTTP sonrasi MQTT publish) ve Simulator (dogrudan)

### Simulator Ek Alani

Simulator firmware'i tum sensor mesajlarina `"sim": true` alani ekler. Bu sayede gercek ve simule veri ayirt edilebilir.

---

## 2. Kontrol Komutlari (Sunucu > ESP32)

Node-RED veya mobil uygulama tarafindan gonderilir. PLC firmware'i bu topic'lere subscribe olur.

| Topic | Aciklama | Payload |
|-------|----------|---------|
| `akilli-sinif/{id}/control/led` | LED parlaklik ayari | `{"brightness": 80}` veya `{"state": "on"}` / `{"state": "off"}` |
| `akilli-sinif/{id}/control/cooling` | Sogutma rolesi (12V DC fan) | `{"state": "on"}` / `{"state": "off"}` |
| `akilli-sinif/{id}/control/heating` | Isitma rolesi (22ohm 5W direnc) | `{"state": "on"}` / `{"state": "off"}` |
| `akilli-sinif/{id}/control/mode` | Otomatik / manuel mod | `{"auto": true}` / `{"auto": false}` |
| `akilli-sinif/{id}/control/alert` | Gorsel uyari sistemi | Asagida detayli |
| `akilli-sinif/{id}/control/ota` | OTA firmware guncelleme | Asagida detayli |
| `akilli-sinif/{id}/control/reset` | Config sifirlama | `{"action": "reset_config"}` |
| `akilli-sinif/{id}/control/config` | Canli config guncelleme (CAM) | Asagida detayli |

### Broadcast Komutlari

| Topic | Aciklama |
|-------|----------|
| `akilli-sinif/all/control/ota` | Tum cihazlara OTA komutu |
| `akilli-sinif/all/control/reset` | Tum cihazlara config sifirlama |

### LED Kontrolu

```json
// Parlaklik ayarla (0-100)
{"brightness": 80}

// Ac / kapat
{"state": "on"}   // %100 yakar
{"state": "off"}  // kapatir
```

- `state: "on"` gonderildiginde LED %100 yanar
- `state: "off"` gonderildiginde LED kapanir
- `brightness` alaniyla 0-100 arasi hassas ayar yapilabilir

### Cooling / Heating Kontrolu

PLC v1.3.0+ icinde fan PWM yerine **iki ayri role** ile kontrol edilir:

```json
// Sogutma rolesi (12V DC fan)
{"state": "on"}   // role kapanir, fan calisir
{"state": "off"}  // role acilir, fan durur

// Isitma rolesi (22ohm 5W direnc)
{"state": "on"}   // role kapanir, direnc isinir
{"state": "off"}  // role acilir, direnc soguma
```

**Otomatik / manuel mod:**

```json
{"auto": true}   // PLC kendi histeresisi ile karar verir
{"auto": false}  // Sadece dis komutla acilir/kapanir
```

PLC firmware'inin yerlesik histeresis esikleri:

| Aktuator | ON esigi | OFF esigi |
|----------|----------|-----------|
| Cooling  | T > 26°C | T < 24°C |
| Heating  | T < 20°C | T > 22°C |

`mode.auto = false` iken bu mantik atlanir; sadece `control/cooling` ve `control/heating` komutlari uygulanir.

### Gorsel Uyari (Alert)

```json
// Uyari seviyesi ayarla
{"level": "none"}                            // Uyariyi kaldir
{"level": "info", "message": "Bilgi"}       // Bilgi
{"level": "warning", "message": "Dikkat!"}  // Uyari — sari arka plan
{"level": "danger", "message": "TEHLIKE!"}  // Tehlike — kirmizi yanip sonen ekran
```

Uyari seviyeleri ve TFT ekran davranisi:

| Seviye | Ekran | LED Blink |
|--------|-------|-----------|
| `none` | Normal gorunum | — |
| `info` | Normal gorunum | 1000 ms |
| `warning` | Sari arka plan, baslikta "!" | 300 ms |
| `danger` | Kirmizi yanip sonen tam ekran + "UYARI!" yazisi | 100 ms |

### OTA Guncelleme Komutu

```json
{
  "action": "update",
  "version": "v1.2.0",
  "url": "https://github.com/MagiMigi/akilli-sinif/releases/download/v1.2.0/firmware-plc-v1.2.0.bin"
}
```

- `action`: Sadece `"update"` desteklenir
- `version`: Hedef firmware versiyonu
- `url`: Binary indirme URL'si (HTTPS, GitHub Releases)
- Zaten ayni versiyondaysa guncelleme yapilmaz, `"up_to_date"` durum mesaji doner

### Config Sifirlama

```json
{"action": "reset_config"}
```

NVS (Non-Volatile Storage) ve WiFi kimlik bilgilerini siler. ESP32 yeniden baslar ve WiFiManager portal'i acar.

### Canli Config Guncelleme (Sadece ESP32-CAM)

Topic: `akilli-sinif/{id}/control/config`

```json
// API anahtarini guncelle
{"api_key": "yeni-anahtar"}

// Sunucu URL'sini guncelle
{"server_url": "http://192.168.1.50:5000/analyze"}

// MQTT broker'i guncelle (yeniden baslama gerekebilir)
{"mqtt_broker": "192.168.1.100"}

// Birden fazla alan ayni anda
{"api_key": "yeni-key", "server_url": "http://yeni-ip:5000/analyze"}
```

Degisiklikler aninda NVS'e kaydedilir, restart gerekmez (mqtt_broker haric).

---

## 3. Aktuator Durumlari (ESP32 > Sunucu)

PLC firmware'i aktuator degistiginde bu topic'lere yayinlar. Kategori: `actuators`.
**Hepsi `retain=true` yayinlanir** — broker son durumu tutar, dashboard ve ESP32 reboot
sonrasi senkron kalir. Hem online MQTT komutu (`control/*`) hem de fiziksel duvar anahtari
(GPIO 5/16/19 push button toggle) ayni state'i degistirir ve bu topic'leri tetikler.

| Topic | Aciklama | Payload | Retained |
|-------|----------|---------|----------|
| `akilli-sinif/{id}/actuators/led` | LED mevcut durumu | `{"value": 80, "unit": "%", "timestamp": 123456}` | Evet |
| `akilli-sinif/{id}/actuators/cooling` | Sogutma rolesi durumu | `{"state": "on"}` veya `{"state": "off"}` | Evet |
| `akilli-sinif/{id}/actuators/heating` | Isitma rolesi durumu | `{"state": "on"}` veya `{"state": "off"}` | Evet |

---

## 4. Sistem Durumu

### Baglanti Durumu

| Topic | Aciklama | Retained |
|-------|----------|----------|
| `akilli-sinif/{id}/status/connection` | Cihaz online/offline durumu | Evet |

**Online payload:**
```json
{
  "status": "online",
  "device": "esp32-plc-sinif-1",
  "classroom": "sinif-1",
  "ip": "192.168.1.42",
  "rssi": -45,
  "uptime": 3600,
  "mock_mode": false,
  "firmware_version": "1.0.0"
}
```

Simulator ek alan: `"sim_hour": 14` (sanal saat).

**Offline payload (LWT — otomatik):**
```json
{
  "status": "offline",
  "device": "esp32-plc-sinif-1"
}
```

LWT (Last Will and Testament): ESP32 baglantisi kopunca broker bu mesaji otomatik yayinlar.

### OTA Durum Bildirimi

Topic: `akilli-sinif/{id}/status/ota`

```json
// Guncelleme basliyor
{"status": "updating", "progress": 0, "current_version": "1.0.0", "target_version": "v1.2.0"}

// Ilerleme (%10 aralikla)
{"status": "updating", "progress": 50, "current_version": "1.0.0", "target_version": "v1.2.0"}

// Basarili
{"status": "success", "progress": 100, "current_version": "1.0.0", "target_version": "v1.2.0"}

// Zaten guncel
{"status": "up_to_date", "current_version": "1.0.0", "target_version": "1.0.0"}

// Hata
{"status": "failed", "current_version": "1.0.0", "target_version": "v1.2.0", "error": "http_error_404"}
```

### IP Adresi (Sadece ESP32-CAM)

Topic: `akilli-sinif/{id}/status/ip`

```
192.168.1.42
```

Her boot'ta `retain=true` ile yayinlanir. Node-RED bu sayede CAM'in IP'sini her zaman bilir.

### Config Onay (Sadece ESP32-CAM)

Topic: `akilli-sinif/{id}/status/config`

```json
{"config_updated": true}
```

MQTT uzerinden config guncellendikten sonra yayinlanir.

---

## 5. PLC Subscribe Listesi

PLC firmware'inin `subscribeToControlTopics()` fonksiyonu su topic'lere abone olur (QoS 1):

```
akilli-sinif/{id}/control/led
akilli-sinif/{id}/control/cooling
akilli-sinif/{id}/control/heating
akilli-sinif/{id}/control/mode
akilli-sinif/{id}/control/alert
akilli-sinif/{id}/control/ota
akilli-sinif/{id}/control/reset
akilli-sinif/{id}/sensors/camera        # Kisi sayisini almak icin
akilli-sinif/all/control/reset          # Broadcast reset
akilli-sinif/all/control/ota            # Broadcast OTA
```

---

## 6. CAM Subscribe Listesi

CAM firmware'inin subscribe listesi:

```
akilli-sinif/{id}/control/config        # Canli config guncelleme
```

---

## 7. QoS Seviyeleri

| Kategori | QoS | Aciklama |
|----------|-----|----------|
| Sensor verileri | 0 | At-most-once, kayip olabilir |
| Kontrol komutlari | 1 | At-least-once, onemli |
| OTA komutlari | 1 | At-least-once, kritik |
| Sistem durumu | 1 | At-least-once |
| LWT | 1 | At-least-once |

---

## 8. Retain Flag

| Kategori | Retain |
|----------|--------|
| Sensor verileri | Hayir |
| Aktuator durumlari | Hayir |
| Baglanti durumu (status/connection) | Evet |
| IP adresi (status/ip) | Evet |
| LWT | Evet |

---

## 9. Sinif ID'leri

Portal'dan ayarlanir, NVS'e kaydedilir. Varsayilan degerler:

| Firmware | Varsayilan ID | Aciklama |
|----------|---------------|----------|
| PLC | sinif-1 | Gercek donanim test sinifi |
| CAM | sinif-1 | PLC ile ayni sinif |
| Simulator | sinif-2 | Simulasyon sinifi |

---

## 10. Ornek Komutlar

```bash
# Tum sensor verilerini dinle
mosquitto_sub -t "akilli-sinif/+/sensors/#" -u esp32 -P "$MQTT_PASS" -v

# Belirli sinifin tum verilerini dinle
mosquitto_sub -t "akilli-sinif/sinif-1/#" -u esp32 -P "$MQTT_PASS" -v

# Sadece sicaklik verilerini dinle
mosquitto_sub -t "akilli-sinif/+/sensors/temperature" -u esp32 -P "$MQTT_PASS" -v

# LED'i %50 yap
mosquitto_pub -h localhost -u nodered -P "$NODERED_PASS" \
  -t "akilli-sinif/sinif-1/control/led" \
  -m '{"brightness": 50}'

# Sogutmayi ac
mosquitto_pub -h localhost -u nodered -P "$NODERED_PASS" \
  -t "akilli-sinif/sinif-1/control/cooling" \
  -m '{"state": "on"}'

# Isitmayi kapat
mosquitto_pub -h localhost -u nodered -P "$NODERED_PASS" \
  -t "akilli-sinif/sinif-1/control/heating" \
  -m '{"state": "off"}'

# Manuel moda gec (otomatik histeresis devre disi)
mosquitto_pub -h localhost -u nodered -P "$NODERED_PASS" \
  -t "akilli-sinif/sinif-1/control/mode" \
  -m '{"auto": false}'

# Tehlike uyarisi gonder
mosquitto_pub -h localhost -u nodered -P "$NODERED_PASS" \
  -t "akilli-sinif/sinif-1/control/alert" \
  -m '{"level": "danger", "message": "YANGIN!"}'

# OTA guncelleme tetikle (tek cihaz)
mosquitto_pub -h localhost -u nodered -P "$NODERED_PASS" \
  -t "akilli-sinif/sinif-1/control/ota" \
  -m '{"action":"update","version":"v1.2.0","url":"https://github.com/MagiMigi/akilli-sinif/releases/download/v1.2.0/firmware-plc-v1.2.0.bin"}'

# Tum cihazlara config sifirlama
mosquitto_pub -h localhost -u nodered -P "$NODERED_PASS" \
  -t "akilli-sinif/all/control/reset" \
  -m '{"action":"reset_config"}'

# OTA durumunu izle
mosquitto_sub -t "akilli-sinif/+/status/ota" -u esp32 -P "$MQTT_PASS" -v
```
