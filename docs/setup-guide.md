# Kurulum Kilavuzu

Bu kilavuz, Akilli Sinif Sistemi sunucusunu sifirdan kurmayı anlatir.

## Icindekiler

- [Sistem Gereksinimleri](#sistem-gereksinimleri)
- [1. Mosquitto MQTT Broker](#1-mosquitto-mqtt-broker)
- [2. InfluxDB](#2-influxdb)
- [3. Node-RED](#3-node-red)
- [4. Grafana](#4-grafana)
- [5. YOLOv8 AI Kisi Sayma](#5-yolov8-ai-kisi-sayma)
- [6. ESP32 Gelistirme Ortami](#6-esp32-gelistirme-ortami)
- [7. Kurulumu Dogrula](#7-kurulumu-dogrula)
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
| Arduino IDE | 2.x (firmware yukleme icin) |

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

### Konfigurasyon

Repo icindeki config dosyasini kopyala:

```bash
sudo cp server/mosquitto/mosquitto.conf /etc/mosquitto/mosquitto.conf
```

`mosquitto.conf` icerigi:

| Ayar | Deger | Aciklama |
|------|-------|----------|
| `listener` | 1883 | Standart MQTT portu |
| `allow_anonymous` | false | Kimlik dogrulama zorunlu |
| `password_file` | /etc/mosquitto/passwd | Sifre dosyasi |
| `persistence` | true | Mesajlar diskte saklanir |
| `persistence_location` | /var/lib/mosquitto/ | Saklama dizini |
| `message_size_limit` | 1048576 | Maks mesaj boyutu (1 MB) |
| `autosave_interval` | 1800 | Otomatik kayit araligi (30 dk) |

### MQTT Kullanicilarini Olustur

Sistemde iki MQTT kullanicisi vardir:

| Kullanici | Sifre | Kullanan |
|-----------|-------|----------|
| `esp32` | `akilli123` | ESP32 cihazlari (PLC, CAM, Simulator) |
| `nodered` | `nodered123` | Node-RED ve YOLO sunucusu |

```bash
# esp32 kullanicisi
sudo mosquitto_passwd -c /etc/mosquitto/passwd esp32
# Sifre: akilli123

# nodered kullanicisi
sudo mosquitto_passwd /etc/mosquitto/passwd nodered
# Sifre: nodered123
```

### Log Dizini ve Servis

```bash
sudo mkdir -p /var/log/mosquitto
sudo chown mosquitto:mosquitto /var/log/mosquitto
sudo systemctl enable --now mosquitto
```

### Test

```bash
# Terminal 1 — dinle
mosquitto_sub -h localhost -t "test" -u esp32 -P akilli123

# Terminal 2 — gonder
mosquitto_pub -h localhost -t "test" -m "Merhaba" -u esp32 -P akilli123
```

Terminal 1'de "Merhaba" gormalisin.

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

[Resmi kurulum kilavuzu](https://docs.influxdata.com/influxdb/v2/install/)
</details>

### Servisi Baslat

```bash
sudo systemctl enable --now influxdb
```

### Ilk Kurulum (Web Arayuzu)

1. http://localhost:8086 adresine git
2. Asagidaki bilgileri gir:

   | Alan | Deger |
   |------|-------|
   | Username | admin |
   | Password | akilli123456 |
   | Organization | AkilliSinif |
   | Bucket | sinif_data |

3. Olusan API token'i not al — Grafana ve Node-RED'de kullanacaksin

### Retention Policy (opsiyonel)

```bash
influx bucket update --id <BUCKET_ID> --retention 4320h   # 180 gun
```

---

## 3. Node-RED

### Kurulum

```bash
npm install -g node-red
```

### Eklentileri Kur

```bash
cd ~/.node-red
npm install node-red-contrib-influxdb node-red-dashboard node-red-contrib-image-output
```

### Systemd Servisi

Repo icinde hazir servis dosyasi var:

```bash
sudo cp server/node-red/node-red.service /etc/systemd/system/
```

Servis dosyasi icerigi (`server/node-red/node-red.service`):

```ini
[Unit]
Description=Node-RED
After=network.target

[Service]
Type=simple
User=magimigi
WorkingDirectory=/home/magimigi
ExecStart=/home/magimigi/.local/share/mise/installs/node/25.1.0/bin/node-red
Restart=on-failure
RestartSec=10

[Install]
WantedBy=multi-user.target
```

> **Not:** `ExecStart` satirindaki node-red yolunu kendi sistemine gore guncelle. `which node-red` komutuyla bulabilirsin.

```bash
sudo systemctl daemon-reload
sudo systemctl enable --now node-red
```

### Flow'u Iceri Aktar

1. http://localhost:1880 adresine git
2. Sag ust menu > **Import** > `server/node-red/flows-v3.json` dosyasini sec
3. **Deploy**

### MQTT Broker Yapilandirmasi

1. Herhangi bir MQTT node'una cift tikla (mor renkli)
2. **Server** yanindaki kalem ikonuna tikla
3. **Security** sekmesi:
   - Username: `nodered`
   - Password: `nodered123`
4. **Update** > **Done** > **Deploy**

### InfluxDB Yapilandirmasi

1. Herhangi bir InfluxDB out node'una cift tikla
2. **Server** yanindaki kalem ikonuna tikla
3. Ayarlar:
   - Version: `2.0`
   - URL: `http://localhost:8086`
   - Token: *(InfluxDB'den aldigin API token)*
4. **Update** > **Done** > **Deploy**

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

### Servisi Baslat

```bash
sudo systemctl enable --now grafana
```

### Ilk Kurulum

1. http://localhost:3000 > giris: `admin` / `admin`
2. Yeni sifre: `akilli123456`

### InfluxDB Datasource Ekle

1. **Configuration > Data Sources > Add data source > InfluxDB**
2. Ayarlar:

   | Alan | Deger |
   |------|-------|
   | Query Language | Flux |
   | URL | http://localhost:8086 |
   | Organization | AkilliSinif |
   | Token | *(InfluxDB'den aldigin token)* |
   | Default Bucket | sinif_data |

3. **Save & Test** — "Data source is working" mesaji gelmeli

### Dashboard Import

1. Sol menu > **Dashboards** > **Import**
2. **Upload JSON file** > `server/grafana/dashboards/akilli-sinif-v2.json`
3. **Import**

---

## 5. YOLOv8 AI Kisi Sayma

### Python Ortamini Kur

```bash
cd server/ai-processing
python -m venv venv
source venv/bin/activate
pip install -r requirements.txt
```

`requirements.txt` icindeki ana bagimliliklar:

| Paket | Versiyon | Amac |
|-------|----------|------|
| Flask | 3.1.3 | REST API sunucusu |
| ultralytics | 8.4.23 | YOLOv8 modeli |
| paho-mqtt | 2.1.0 | MQTT baglantisi |
| opencv-python | 4.13.0.92 | Goruntu isleme |
| torch | 2.10.0+cpu | PyTorch (CPU) |
| python-dotenv | >=1.0.0 | .env dosyasi okuma |

### .env Dosyasini Olustur

```bash
cp .env.example .env
```

`.env` dosyasi icerigi:

```env
# MQTT Ayarlari
MQTT_BROKER=localhost
MQTT_PORT=1883
MQTT_USER=nodered
MQTT_PASSWORD=nodered123

# YOLO Sunucu Auth (API anahtari)
API_KEY=ultra-mega-secret-key
```

| Degisken | Varsayilan | Aciklama |
|----------|-----------|----------|
| `MQTT_BROKER` | localhost | MQTT broker adresi |
| `MQTT_PORT` | 1883 | MQTT portu |
| `MQTT_USER` | nodered | MQTT kullanici adi |
| `MQTT_PASSWORD` | (bos) | MQTT sifresi |
| `API_KEY` | (bos) | API auth anahtari. Bos birakılırsa auth devre disi kalir (gelistirme modu) |

### YOLO Sunucusu Detaylari

| Ozellik | Deger |
|---------|-------|
| Port | 5000 |
| Model | yolov8n.pt (Nano — en hizli) |
| Confidence threshold | 0.45 |
| Tespit sinifi | person (COCO class 0) |
| Foto kayit | `captured_images/` dizinine |
| Maks foto | 200 adet, 7 gunden eski olanlar silinir |

### API Endpoint'leri

| Endpoint | Method | Auth | Aciklama |
|----------|--------|------|----------|
| `GET /` | GET | Yok | Sunucu durumu |
| `POST /analyze` | POST | X-API-Key | ESP32-CAM'den JPEG al, analiz et |
| `GET /test` | GET | X-API-Key | Webcam'den test foto cek |
| `GET /count/<sinif_id>` | GET | Yok | Son kisi sayisini dondur |

`/analyze` endpoint'i:
- Header: `Content-Type: image/jpeg`, `X-Classroom-ID: sinif-1`, `X-API-Key: <anahtar>`
- Body: Ham JPEG verisi
- Yanit: `{"success": true, "person_count": 5, "detections": [...], "classroom_id": "sinif-1"}`
- Sonuc otomatik olarak MQTT topic'ine yayinlanir: `akilli-sinif/<sinif_id>/sensors/camera`

### Test

```bash
source venv/bin/activate
python -c "from ultralytics import YOLO; print('YOLOv8 OK')"
```

### Servisi Baslat

```bash
cd server/ai-processing
source venv/bin/activate
python yolo_server.py
```

YOLO modeli kurulu degilse mock mod otomatik aktif olur (rastgele kisi sayisi uretir).

---

## 6. ESP32 Gelistirme Ortami

### Arduino IDE

<details>
<summary>Arch Linux</summary>

```bash
yay -S arduino-ide-bin
```
</details>

<details>
<summary>Diger dagitimlar</summary>

[Arduino IDE 2.x](https://www.arduino.cc/en/software) indirip kur.
</details>

### ESP32 Board Destegi

1. **File > Preferences**
2. Additional Board Manager URLs:
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
3. **Tools > Board > Boards Manager** > `esp32` ara > **Install**

### Gerekli Kutuphaneler

Library Manager'dan kur (**Tools > Manage Libraries**):

| Kutuphane | Kullanan Firmware |
|-----------|-------------------|
| PubSubClient | PLC, CAM, Simulator |
| ArduinoJson | PLC, CAM, Simulator |
| WiFiManager (tzapu) | PLC, CAM, Simulator |
| TFT_eSPI | PLC |
| DHT sensor library | PLC |

### TFT_eSPI Pin Yapilandirmasi

PLC firmware'i ST7735 1.44" TFT ekran kullaniyor. `TFT_eSPI` kutuphanesi pinleri `User_Setup.h` dosyasindan okur.

Arduino kutuphaneler dizininde `TFT_eSPI/User_Setup.h` dosyasini ac ve su pinleri ayarla:

| TFT Pini | ESP32 GPIO |
|----------|------------|
| TFT_CS | 15 |
| TFT_DC (A0/RS) | 33 |
| TFT_RST | 32 |
| TFT_MOSI (SDA) | 23 |
| TFT_SCLK (SCK) | 18 |
| TFT_BL | 3.3V (veya GPIO ile kontrol) |

### Seri Port Izni

```bash
# Arch
sudo usermod -a -G uucp $USER

# Ubuntu / Debian
sudo usermod -a -G dialout $USER
```

> Gruba eklendikten sonra **logout/login** yap.

---

## 7. Kurulumu Dogrula

### Servislerin Durumu

```bash
sudo systemctl status mosquitto influxdb node-red grafana
```

### MQTT Uctan Uca Test

```bash
# Terminal 1 — tum mesajlari dinle
mosquitto_sub -h localhost -t "akilli-sinif/#" -u esp32 -P akilli123 -v

# Terminal 2 — test verisi gonder
mosquitto_pub -h localhost \
  -t "akilli-sinif/sinif-1/sensors/temperature" \
  -m '{"value": 23.5, "unit": "C"}' \
  -u esp32 -P akilli123
```

Terminal 1'de mesaji goruyorsan MQTT calisiyor.

### InfluxDB Kontrolu

1. http://localhost:8086 > **Data Explorer**
2. Bucket: `sinif_data` > filtrele
3. Node-RED uzerinden gelen veriler burada gorunmeli

### Grafana Kontrolu

1. http://localhost:3000
2. Dashboard'a git — verilerin grafikte gorunmesi gerekir

### Tum Servisleri Tek Komutla Baslat

```bash
./akilli-start.sh
```

Bu script sirasiyla kontrol eder ve baslatir:
1. Mosquitto
2. InfluxDB
3. Grafana
4. Node-RED
5. YOLO Sunucusu

---

## Sorun Giderme

### Mosquitto Baslamiyor

```bash
# Detayli log
journalctl -u mosquitto -e

# Manuel calistirma (debug)
mosquitto -c /etc/mosquitto/mosquitto.conf -v
```

### ESP32 Seri Port Erisim Hatasi

```bash
# Gecici cozum
sudo chmod 666 /dev/ttyUSB0

# Kalici cozum
sudo usermod -a -G uucp $USER   # Arch
sudo usermod -a -G dialout $USER # Ubuntu
# Logout/login yap
```

### MQTT Baglanti Hatasi

```bash
# Mosquitto calisiyor mu?
systemctl status mosquitto

# Manuel test
mosquitto_pub -h localhost -u esp32 -P akilli123 -t "test" -m "hello"
```

### InfluxDB'de Veri Yok

1. MQTT mesajlarinin geldigini dogrula (`mosquitto_sub` ile)
2. Node-RED'de InfluxDB node'unun bagli oldugundan emin ol (yesil nokta = bagli)
3. InfluxDB token'inin Node-RED'de dogru oldugunu kontrol et
4. Bucket adinin `sinif_data` oldugunu dogrula

### Grafana'da Veri Gorunmuyor

1. **Configuration > Data Sources > InfluxDB > Save & Test** — "Data source is working" gelmeli
2. Token veya bucket adi yanlissa duzelt
3. Zaman araligini kontrol et (sag ustte "Last 5 minutes" dene)

### Node-RED Eklentileri Gorunmuyor

```bash
cd ~/.node-red
npm install <paket-adi>
sudo systemctl restart node-red
```

### YOLO Sunucusu Calismiyor

```bash
cd server/ai-processing
source venv/bin/activate
python yolo_server.py
```

Model dosyasi (`yolov8n.pt`) yoksa otomatik indirilir. Internet baglantisi gerekir.

### ESP32 WiFi'ya Baglanmiyor

- 2.4 GHz WiFi kullan (5 GHz desteklenmez)
- GPIO0 (BOOT) butonuna 5 sn bas > portal acar > WiFi bilgilerini tekrar gir
- Modem/router'i yeniden baslat

### OTA Guncelleme Basarisiz

- Node-RED'de GitHub release URL'sinin dogru oldugunu kontrol et
- ESP32'nin internet erisimi oldugunu dogrula
- `akilli-sinif/<sinif_id>/status/ota` topic'ini dinleyerek hata mesajini gor

---

## Baglanti Ozeti

| Servis | Adres | Kullanici | Sifre |
|--------|-------|-----------|-------|
| Node-RED | http://localhost:1880 | — | — |
| Grafana | http://localhost:3000 | admin | akilli123456 |
| InfluxDB | http://localhost:8086 | admin | akilli123456 |
| MQTT (ESP32) | localhost:1883 | esp32 | akilli123 |
| MQTT (Node-RED) | localhost:1883 | nodered | nodered123 |
| YOLO API | http://localhost:5000 | — | X-API-Key header |
