# MQTT Topic Yapısı - Akıllı Sınıf Sistemi

## Genel Yapı

```
akilli-sinif/{sinif_id}/{kategori}/{alt_konu}
```

## Topic Listesi

### 1. Sensör Verileri (ESP32 → Sunucu)

| Topic | Açıklama | Payload Örneği |
|-------|----------|----------------|
| `akilli-sinif/sinif-1/sensor/temperature` | Sıcaklık | `{"value": 23.5, "unit": "C", "ts": 1710801234}` |
| `akilli-sinif/sinif-1/sensor/humidity` | Nem | `{"value": 65, "unit": "%", "ts": 1710801234}` |
| `akilli-sinif/sinif-1/sensor/light` | Işık seviyesi (LDR) | `{"value": 450, "unit": "lux", "ts": 1710801234}` |
| `akilli-sinif/sinif-1/sensor/air-quality` | Hava kalitesi (MQ-135) | `{"value": 120, "unit": "ppm", "ts": 1710801234}` |
| `akilli-sinif/sinif-1/sensor/pir` | Hareket algılama | `{"value": 1, "ts": 1710801234}` |
| `akilli-sinif/sinif-1/sensor/window` | Pencere durumu (Reed) | `{"value": 0, "ts": 1710801234}` |

**Notlar:**
- `ts`: Unix timestamp (saniye)
- `pir`: 0 = hareket yok, 1 = hareket var
- `window`: 0 = kapalı, 1 = açık (klima kontrolü için önemli)

### 2. Kontrol Komutları (Sunucu → ESP32)

| Topic | Açıklama | Payload Örneği |
|-------|----------|----------------|
| `akilli-sinif/sinif-1/control/led/set` | LED parlaklık ayarı | `{"brightness": 80, "source": "auto"}` |
| `akilli-sinif/sinif-1/control/fan/set` | Fan hız ayarı | `{"speed": 60, "source": "manual"}` |
| `akilli-sinif/sinif-1/control/ac/set` | Klima kontrolü | `{"action": "on", "temp": 24, "mode": "cool"}` |

**source değerleri:**
- `auto`: Otomatik sistem tarafından
- `manual`: Kullanıcı tarafından (web/mobil)
- `schedule`: Zamanlayıcı tarafından

### 3. Kontrol Durumu (ESP32 → Sunucu)

| Topic | Açıklama | Payload Örneği |
|-------|----------|----------------|
| `akilli-sinif/sinif-1/control/led/status` | LED mevcut durumu | `{"brightness": 80, "on": true}` |
| `akilli-sinif/sinif-1/control/fan/status` | Fan mevcut durumu | `{"speed": 60, "on": true}` |
| `akilli-sinif/sinif-1/control/ac/status` | Klima mevcut durumu | `{"on": true, "temp": 24, "mode": "cool"}` |

### 4. Kamera (ESP32-CAM ↔ Sunucu)

| Topic | Açıklama | Payload Örneği |
|-------|----------|----------------|
| `akilli-sinif/sinif-1/camera/status` | Kamera durumu | `{"status": "ready", "ts": 1710801234}` |
| `akilli-sinif/sinif-1/camera/trigger` | Fotoğraf çek komutu | `{"action": "capture"}` |
| `akilli-sinif/sinif-1/camera/detection` | AI tespit sonuçları | `{"people": 15, "occupancy": 75, "trash": false, "ts": 1710801234}` |

**Not:** Fotoğraflar HTTP POST ile gönderilir, MQTT ile sadece metadata.

### 5. Ekran Güncellemeleri (Sunucu → ESP32)

| Topic | Açıklama | Payload Örneği |
|-------|----------|----------------|
| `akilli-sinif/sinif-1/display/update` | Ekran verisi | Aşağıda detaylı |

```json
{
  "time": "12:45",
  "date": "19 Mar 2026",
  "temperature": 23.5,
  "humidity": 65,
  "air_quality": "iyi",
  "current_lesson": {
    "name": "Matematik",
    "teacher": "Prof. Ahmet Yılmaz",
    "end_time": "13:30"
  },
  "next_lesson": {
    "name": "Fizik",
    "teacher": "Doç. Ayşe Kaya",
    "start_time": "13:45"
  },
  "occupancy": 25
}
```

### 6. Sistem Durumu

| Topic | Açıklama | Payload Örneği |
|-------|----------|----------------|
| `akilli-sinif/sinif-1/status/online` | Cihaz online durumu | `{"esp32_plc": true, "esp32_cam": true, "ts": 1710801234}` |
| `akilli-sinif/sinif-1/status/error` | Hata bildirimi | `{"code": 101, "message": "WiFi bağlantı hatası", "ts": 1710801234}` |
| `akilli-sinif/sinif-1/status/heartbeat` | Kalp atışı | `{"uptime": 3600, "free_heap": 45000, "ts": 1710801234}` |

### 7. LWT (Last Will and Testament)

ESP32 bağlantısı koptuğunda otomatik gönderilir:

| Topic | Payload |
|-------|---------|
| `akilli-sinif/sinif-1/status/lwt` | `{"online": false, "ts": 1710801234}` |

## QoS Seviyeleri

| Kategori | QoS | Açıklama |
|----------|-----|----------|
| Sensör verileri | 0 | At-most-once, kayıp olabilir |
| Kontrol komutları | 1 | At-least-once, önemli |
| Sistem durumu | 1 | At-least-once |
| LWT | 1 | At-least-once |

## Retain Flag

| Kategori | Retain | Açıklama |
|----------|--------|----------|
| Sensör verileri | false | Anlık veri |
| Kontrol status | true | Son durumu sakla |
| Display update | true | Son ekran verisini sakla |
| Online status | true | Son durumu sakla |

## Sınıf ID'leri

| ID | Açıklama |
|----|----------|
| `sinif-1` | Gerçek donanımlı test sınıfı |
| `sinif-2` | Simülasyon sınıfı (sahte veri) |

## Örnek Subscribe Patterns

```bash
# Tüm sensör verilerini dinle
mosquitto_sub -t "akilli-sinif/+/sensor/#" -u esp32 -P akilli123

# Belirli sınıfın tüm verilerini dinle
mosquitto_sub -t "akilli-sinif/sinif-1/#" -u esp32 -P akilli123

# Sadece sıcaklık verilerini dinle
mosquitto_sub -t "akilli-sinif/+/sensor/temperature" -u esp32 -P akilli123

# Tüm kontrol komutlarını dinle
mosquitto_sub -t "akilli-sinif/+/control/+/set" -u esp32 -P akilli123
```
