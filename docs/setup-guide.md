# Kurulum Kılavuzu

Bu kılavuz, Akıllı Sınıf Sistemi sunucusunu sıfırdan kurmayı anlatır.

## İçindekiler

- [Sistem Gereksinimleri](#sistem-gereksinimleri)
- [1. Mosquitto MQTT Broker](#1-mosquitto-mqtt-broker)
- [2. InfluxDB](#2-influxdb)
- [3. Node-RED](#3-node-red)
- [4. Grafana](#4-grafana)
- [5. YOLOv8 (AI Kişi Sayma)](#5-yolov8-ai-kişi-sayma)
- [6. ESP32 Geliştirme Ortamı](#6-esp32-geliştirme-ortamı)
- [7. Kurulumu Doğrula](#7-kurulumu-doğrula)
- [Sorun Giderme](#sorun-giderme)

---

## Sistem Gereksinimleri

| Gereksinim | Minimum |
|-----------|---------|
| OS | Linux (Arch, Ubuntu 22.04+, Debian 12+) |
| RAM | 4 GB |
| Disk | 20 GB |
| Node.js | 18+ |
| Python | 3.11+ |
| Arduino IDE | 2.x (firmware yükleme için) |

---

## 1. Mosquitto MQTT Broker

<details>
<summary>Arch Linux</summary>

```bash
sudo pacman -S mosquitto
```
</details>

<details>
<summary>Ubuntu / Debian</summary>

```bash
sudo apt install mosquitto mosquitto-clients
```
</details>

**Konfigürasyon:**

```bash
sudo cp server/mosquitto/mosquitto.conf /etc/mosquitto/mosquitto.conf
```

**MQTT kullanıcıları oluştur:**

```bash
# esp32 kullanıcısı (cihazlar için)
sudo mosquitto_passwd -c /etc/mosquitto/passwd esp32
# Şifre gir: akilli123

# nodered kullanıcısı (Node-RED için)
sudo mosquitto_passwd /etc/mosquitto/passwd nodered
# Şifre gir: nodered123
```

**Log dizini ve servis:**

```bash
sudo mkdir -p /var/log/mosquitto
sudo chown mosquitto:mosquitto /var/log/mosquitto
sudo systemctl enable --now mosquitto
```

**Test:**

```bash
# Terminal 1
mosquitto_sub -h localhost -t "test" -u esp32 -P akilli123

# Terminal 2
mosquitto_pub -h localhost -t "test" -m "Merhaba" -u esp32 -P akilli123
```

Terminal 1'de "Merhaba" görmelisin.

---

## 2. InfluxDB

<details>
<summary>Arch Linux</summary>

```bash
sudo pacman -S influxdb influx-cli
```
</details>

<details>
<summary>Ubuntu / Debian</summary>

InfluxDB 2.x için [resmi kurulum kılavuzunu](https://docs.influxdata.com/influxdb/v2/install/) takip et.
</details>

**Servisi başlat:**

```bash
sudo systemctl enable --now influxdb
```

**Web arayüzünden ilk kurulum:**

1. http://localhost:8086 adresine git
2. Aşağıdaki bilgileri gir:

   | Alan | Değer |
   |------|-------|
   | Username | admin |
   | Password | akilli123456 |
   | Organization | AkilliSinif |
   | Bucket | sinif_data |

3. Oluşturulan API token'ı not al — Grafana ve Node-RED'de kullanacaksın

**Retention policy (opsiyonel):**

```bash
influx bucket update --id <BUCKET_ID> --retention 4320h   # 180 gün
```

---

## 3. Node-RED

**Kurulum:**

```bash
npm install -g node-red
```

**Eklentileri kur:**

```bash
cd ~/.node-red
npm install node-red-contrib-influxdb node-red-dashboard node-red-contrib-image-output
```

**Systemd servisi oluştur:**

```bash
sudo cp server/node-red/node-red.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now node-red
```

**Flow'u içe aktar:**

1. http://localhost:1880 adresine git
2. Sağ üst menü → **Import** → `server/node-red/flows-v3.json`
3. **Deploy**

---

## 4. Grafana

<details>
<summary>Arch Linux</summary>

```bash
sudo pacman -S grafana
```
</details>

<details>
<summary>Ubuntu / Debian</summary>

```bash
sudo apt install grafana
```
</details>

**Servisi başlat:**

```bash
sudo systemctl enable --now grafana
```

**İlk kurulum:**

1. http://localhost:3000 → giriş: `admin` / `admin`
2. Yeni şifre olarak `akilli123456` gir

**InfluxDB datasource ekle:**

1. **Configuration → Data Sources → Add data source → InfluxDB**
2. Ayarlar:

   | Alan | Değer |
   |------|-------|
   | Query Language | Flux |
   | URL | http://localhost:8086 |
   | Organization | AkilliSinif |
   | Token | *(InfluxDB'den aldığın token)* |
   | Default Bucket | sinif_data |

3. **Save & Test**

---

## 5. YOLOv8 (AI Kişi Sayma)

```bash
cd server/ai-processing

python -m venv venv
source venv/bin/activate
pip install -r requirements.txt
```

**`.env` dosyasını oluştur:**

```bash
cp .env.example .env
# .env dosyasını düzenle: API_KEY, MQTT kullanıcı/şifre ayarla
```

**Test:**

```bash
python -c "from ultralytics import YOLO; print('YOLOv8 OK')"
```

**Servisi başlat:**

```bash
python yolo_server.py
# http://localhost:5000 adresinde çalışır
```

---

## 6. ESP32 Geliştirme Ortamı

### Arduino IDE

<details>
<summary>Arch Linux</summary>

```bash
yay -S arduino-ide-bin
```
</details>

<details>
<summary>Diğer dağıtımlar</summary>

[Arduino IDE 2.x](https://www.arduino.cc/en/software) indirip kur.
</details>

### ESP32 Board Desteği

1. **File → Preferences**
2. Additional Board Manager URLs alanına ekle:
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
3. **Tools → Board → Boards Manager** → `esp32` ara → **Install**

### Gerekli Kütüphaneler

**Library Manager'dan kur:**
- PubSubClient
- ArduinoJson
- WiFiManager
- TFT_eSPI (PLC için)
- DHT sensor library (PLC için)

### Seri Port İzni

```bash
# Arch
sudo usermod -a -G uucp $USER

# Ubuntu / Debian
sudo usermod -a -G dialout $USER
```

> Gruba eklendikten sonra **logout/login** yapman gerekir.

---

## 7. Kurulumu Doğrula

### Servislerin durumunu kontrol et

```bash
sudo systemctl status mosquitto influxdb node-red grafana
```

### MQTT üzerinden uçtan uca test

```bash
# Terminal 1 — tüm sensör verilerini dinle
mosquitto_sub -h localhost -t "akilli-sinif/#" -u esp32 -P akilli123 -v

# Terminal 2 — test verisi gönder
mosquitto_pub -h localhost \
  -t "akilli-sinif/sinif-1/sensor/temperature" \
  -m '{"value": 23.5, "unit": "C"}' \
  -u esp32 -P akilli123
```

Terminal 1'de mesajı görüyorsan, InfluxDB'de de yazılıp yazılmadığını kontrol et:
1. http://localhost:8086 → **Data Explorer**
2. Bucket: `sinif_data` → filtrele

### Grafana'da veriyi gör

1. http://localhost:3000
2. İlgili dashboard'a git — test verisinin grafikte göründüğünü doğrula

---

## Sorun Giderme

### Mosquitto başlamıyor

```bash
# Detaylı log
journalctl -u mosquitto -e

# Manuel çalıştırma (debug)
mosquitto -c /etc/mosquitto/mosquitto.conf -v
```

### ESP32 seri port erişim hatası

```bash
# Geçici çözüm
sudo chmod 666 /dev/ttyUSB0

# Kalıcı çözüm
sudo usermod -a -G uucp $USER   # Arch
sudo usermod -a -G dialout $USER # Ubuntu
# Logout/login yap
```

### Node-RED eklentileri görünmüyor

```bash
cd ~/.node-red
npm install <paket-adı>
sudo systemctl restart node-red
```

### InfluxDB'de veri yok

1. MQTT mesajlarının geldiğini doğrula (`mosquitto_sub` ile)
2. Node-RED'de InfluxDB node'unun bağlı olduğunu kontrol et (yeşil nokta = bağlı)
3. InfluxDB token'ının Node-RED'de doğru girildiğini kontrol et

### Grafana'da veri görünmüyor

1. **Configuration → Data Sources → InfluxDB → Save & Test** — "Data source is working" mesajı gelmeli
2. Token veya bucket adı yanlışsa düzelt
3. Zaman aralığını kontrol et (sağ üstte "Last 1 hour" yerine "Last 5 minutes" dene)
