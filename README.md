# Akıllı Sınıf Sistemi

> IoT-based smart classroom automation with ESP32, MQTT, YOLOv8, and Grafana.

[![Build & Release](https://github.com/MagiMigi/akilli-sinif/actions/workflows/build-firmware.yml/badge.svg?event=push)](https://github.com/MagiMigi/akilli-sinif/actions/workflows/build-firmware.yml)
[![Release](https://img.shields.io/github/v/release/MagiMigi/akilli-sinif?include_prereleases)](https://github.com/MagiMigi/akilli-sinif/releases/latest)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)

ESP32 tabanlı IoT sistemi. Sınıflarda sensör verilerine göre otomatik ışık/fan kontrolü, kamera ile doluluk takibi ve enerji tasarrufu sağlar. Tek firmware binary ile N adet sınıfı yönetir; güncellemeler kablosuz (OTA) yapılır.

---

## İçindekiler

- [Nasıl Çalışır?](#nasıl-çalışır)
- [Gereksinimler](#gereksinimler)
- [Kurulum](#kurulum)
- [Firmware Yükleme](#firmware-yükleme)
- [Kullanım](#kullanım)
- [Firmware Güncelleme (OTA)](#firmware-güncelleme-ota)
- [Yeni Sınıf Ekleme](#yeni-sınıf-ekleme)
- [Donanım Referansı](#donanım-referansı)
- [Proje Yapısı](#proje-yapısı)
- [Sorun Giderme](#sorun-giderme)
- [Katkıda Bulunma](#katkıda-bulunma)

---

## Nasıl Çalışır?

```
ESP32-PLC ──┐                    ┌── Node-RED ──► InfluxDB ──► Grafana
            ├── WiFi/MQTT ──► Mosquitto ──┤
ESP32-CAM ──┘                    └── YOLOv8 (kişi sayma)
```

| Bileşen | Görev |
|---------|-------|
| **ESP32-PLC** | Sensörlerden veri okur, LED/fan'ı kontrol eder, TFT ekranda durum gösterir |
| **ESP32-CAM** | Kamera görüntüsünü YOLOv8'e gönderir, kişi sayısını MQTT'ye yayınlar |
| **ESP32-SIM** | Gerçek donanım olmadan sinüs bazlı simülasyon üretir (test amaçlı) |
| **Node-RED** | Tüm cihazları orkestre eder, otomasyon kurallarını uygular, OTA yönetir |
| **Grafana** | Gerçek zamanlı dashboard — sıcaklık, nem, doluluk, enerji grafikleri |

---

## Gereksinimler

### Donanım

| Bileşen | Adet | Notlar |
|---------|------|--------|
| ESP32 DevKit V1 | 2 | PLC + simülatör |
| ESP32-CAM | 1 | Kamera modülü (opsiyonel) |
| DHT22 | 1 | Sıcaklık/nem |
| LDR | 1 | Işık seviyesi |
| MQ-135 | 1 | Hava kalitesi |
| PIR | 1 | Hareket algılama |
| Reed Switch | 1 | Pencere durumu |
| TFT Ekran (ST7735 1.44") | 1 | Durum göstergesi |
| LED Strip (12V) + DC Fan (12V) | 1+1 | Aktüatörler |
| 12V Adaptör + XL4015 | 1+1 | Güç: 12V → 5V |
| IRLZ44N MOSFET | 2 | LED + fan sürücü |

Devre şemaları: [`hardware/schematics/`](hardware/schematics/)

### Sunucu (Laptop/PC)

| Gereksinim | Minimum |
|-----------|---------|
| OS | Linux (Arch, Ubuntu, Debian) |
| RAM | 4 GB |
| Disk | 20 GB |
| Node.js | 18+ |
| Python | 3.11+ |
| Arduino IDE | 2.x (yalnızca ilk firmware yüklemesi için) |

---

## Kurulum

### 1. Repoyu klonla

```bash
git clone https://github.com/MagiMigi/akilli-sinif.git
cd akilli-sinif
```

### 2. Sunucu servislerini kur ve başlat

Detaylı kurulum (Mosquitto, InfluxDB, Node-RED, Grafana, YOLOv8): **[docs/setup-guide.md](docs/setup-guide.md)**

Tüm servisler kurulduktan sonra tek komutla başlat:

```bash
./akilli-start.sh
```

### 3. Node-RED flow'unu içe aktar

1. **http://localhost:1880** adresini aç
2. Sağ üst menü → **Import** → `server/node-red/flows-v3.json`
3. **Deploy**

### 4. OTA için GitHub repo adresini ayarla

Node-RED'de `ota-check-github` node'unu aç, URL'yi kontrol et:

```
https://api.github.com/repos/MagiMigi/akilli-sinif/releases/latest
```

---

## Firmware Yükleme

> İlk kurulumda kablo ile yapılır. Sonraki tüm güncellemeler OTA (kablosuz) ile yapılır.

1. Arduino IDE'de ilgili `.ino` dosyasını aç:

   | Cihaz | Dosya |
   |-------|-------|
   | PLC | `firmware/esp32-plc/src/main/main.ino` |
   | CAM | `firmware/esp32-cam/src/main/main.ino` |
   | SIM | `firmware/esp32-simulator/src/main/main.ino` |

2. Board: **ESP32 Dev Module** — Port: ilgili `/dev/ttyUSB*` — **Upload**

### WiFi ve Cihaz Ayarı (tek seferlik)

Upload sonrası ESP32 captive portal moduna girer:

1. WiFi listesinden ilgili ağa bağlan:
   - PLC → `Akilli-Sinif-Setup`
   - CAM → `Akilli-CAM-Setup`
   - SIM → `Akilli-SIM-Setup`
2. Tarayıcı **192.168.4.1** adresine yönlendirir
3. Formu doldur:

   | Alan | Örnek |
   |------|-------|
   | WiFi SSID | OkulWifi |
   | WiFi Şifre | ••••••• |
   | MQTT Broker IP | 192.168.1.100 |
   | Sinif ID | sinif-1 |
   | Mock Modu | false (PLC/CAM) · true (SIM) |

4. **Kaydet** → ESP32 yeniden başlar ve bağlanır

Ayarlar NVS'e kalıcı kaydedilir. Sıfırlamak için: **GPIO0 (BOOT) butonuna 5 saniye basılı tut.**

---

## Kullanım

### Servis Adresleri

| Servis | Adres | Giriş |
|--------|-------|-------|
| Grafana | http://localhost:3000 | admin / akilli123456 |
| Node-RED | http://localhost:1880 | — |
| InfluxDB | http://localhost:8086 | admin / akilli123456 |
| MQTT Broker | localhost:1883 | esp32 / akilli123 |
| YOLOv8 API | http://localhost:5000 | API key (.env) |

### Grafana Dashboard

Sınıf bazında gerçek zamanlı izleme:
- Sıcaklık, nem, ışık seviyesi, hava kalitesi
- Kişi sayısı (YOLOv8)
- LED ve fan durumu

### Node-RED ile Kontrol

http://localhost:1880 adresinde:
- Inject node'larına tıklayarak sınıflara komut gönder (ışık, fan)
- OTA güncelleme tetikle
- Cihaz durumlarını izle

---

## Firmware Güncelleme (OTA)

```bash
git add .
git commit -m "feat: açıklama"
git tag v1.2.0
git push origin main --tags
```

GitHub Actions otomatik olarak 3 firmware'i derler ve [GitHub Release](https://github.com/MagiMigi/akilli-sinif/releases) oluşturur.

**Node-RED'den cihazlara gönder:**

1. http://localhost:1880 → **OTA Yönetimi** tabı
2. "Manuel Kontrol" → son sürüm GitHub'dan çekilir
3. Hedef cihaza tıkla:
   - `sinif-1 PLC Güncelle` / `sinif-1 CAM Güncelle` — tek cihaz
   - `TÜM PLC'leri Güncelle` / `TÜM CAM'leri Güncelle` — broadcast, tüm sınıflar aynı anda
4. İlerleme ve sonuç alt panelde görünür

> OTA sırasında WiFi/MQTT ayarları korunur, portal açılmaz.

MQTT topic detayları: **[docs/mqtt-topics.md](docs/mqtt-topics.md)**

---

## Config Sıfırlama

WiFi şifresi veya MQTT broker IP'si değiştiğinde cihazı portal moduna almak için:

**Fiziksel:** ESP32 açılırken **GPIO0 (BOOT)** butonunu 5 saniye basılı tut.

**Uzaktan (Node-RED):**

1. http://localhost:1880 → **Config Sıfırlama** tabı
2. İlgili sınıfı seç (`sinif-1 Sıfırla`, `sinif-2 Sıfırla`) veya tümünü sıfırla
3. ESP32 ekranda "Uzaktan Reset!" gösterir, yeniden başlar
4. `Akilli-Sinif-Setup` WiFi ağına bağlan → `192.168.4.1` → yeni ayarları gir

> Sıfırlama sonrası broker IP boşsa cihaz doğrudan portal moduna girer, kayıtlı WiFi'ya bağlanmaya çalışmaz.

---

## Yeni Sınıf Ekleme

1. Yeni ESP32'ye `firmware-plc.bin` yaz (tek seferlik, kablo ile)
2. `Akilli-Sinif-Setup` WiFi ağına bağlan
3. Portal'da `sinif_id = sinif-3`, WiFi bilgileri ve MQTT IP gir
4. **Bitti** — Node-RED ve Grafana yeni sınıfı otomatik tanır

Sonraki güncellemeler OTA ile kablosuz yapılır.

---

## Donanım Referansı

### ESP32-PLC Pin Haritası

**Sensörler (Giriş):**

| Bileşen | Pin | Tür |
|---------|-----|-----|
| DHT22 | GPIO4 | Dijital |
| LDR | GPIO34 | Analog |
| MQ-135 | GPIO35 | Analog |
| PIR | GPIO27 | Dijital |
| Reed Switch | GPIO26 | Dijital |

**Aktüatörler (Çıkış):**

| Bileşen | Pin | Tür |
|---------|-----|-----|
| LED Strip | GPIO16 | PWM |
| DC Fan | GPIO17 | PWM |
### TFT Ekran Durum Göstergeleri

| Durum | Ekran |
|-------|-------|
| Normal | Yeşil başlık, MQTT yeşil daire |
| Sıcaklık yüksek (28–32°C) | Sarı arka plan + "!" ikonu |
| Sıcaklık kritik (>32°C) | Kırmızı yanıp sönen ekran + uyarı mesajı |
| Hava kalitesi kötü (PPM yüksek) | Sadece PPM değeri kırmızı renkte gösterilir |
| MQTT kopuk | Sağ üst kırmızı daire |

---

## Proje Yapısı

```
akilli-sinif/
├── .github/workflows/
│   └── build-firmware.yml      # CI/CD: tag → derleme → release
├── firmware/
│   ├── version.h                # Sürüm (CI tarafından güncellenir)
│   ├── esp32-plc/               # Ana kontrol ünitesi + TFT
│   ├── esp32-cam/               # Kamera + YOLOv8
│   └── esp32-simulator/         # Test simülatörü
├── server/
│   ├── mosquitto/               # MQTT broker config
│   ├── node-red/flows-v3.json   # Orkestrasyon akışları
│   ├── influxdb/                # Veritabanı config
│   ├── grafana/                 # Dashboard tanımları
│   └── ai-processing/           # YOLOv8 kişi sayma servisi
├── hardware/schematics/         # Devre şemaları
├── mobile-app/                  # React Native (geliştirmede)
├── docs/
│   ├── setup-guide.md           # Detaylı kurulum kılavuzu
│   └── mqtt-topics.md           # MQTT topic referansı
└── akilli-start.sh              # Tüm servisleri başlat
```

---

## Sorun Giderme

| Sorun | Çözüm |
|-------|-------|
| ESP32 WiFi'ya bağlanmıyor | GPIO0'a 5 sn bas → portal'dan tekrar ayarla |
| Seri port erişim hatası | `sudo usermod -a -G uucp $USER` (Arch) veya `dialout` (Ubuntu) |
| Mosquitto başlamıyor | `journalctl -u mosquitto -e` ile log kontrol et |
| Grafana'da veri yok | InfluxDB datasource token'ını kontrol et |
| OTA başarısız | Node-RED'de GitHub URL'nin doğru olduğunu kontrol et |

Detaylı sorun giderme: **[docs/setup-guide.md#sorun-giderme](docs/setup-guide.md#sorun-giderme)**

---

## Katkıda Bulunma

1. Repoyu fork'la
2. Feature branch oluştur (`git checkout -b feat/yeni-ozellik`)
3. Commit at (`git commit -m "feat: açıklama"`)
4. Push et (`git push origin feat/yeni-ozellik`)
5. Pull Request aç

---

## Lisans

[MIT](LICENSE) © 2026
