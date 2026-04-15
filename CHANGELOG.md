# Changelog

Tüm önemli değişiklikler bu dosyada belgelenir.
Format: [Keep a Changelog](https://keepachangelog.com/tr/1.1.0/), [Semantic Versioning](https://semver.org/).

## [1.1.0] - 2026-04-14

### Added

- **YOLO API Key kimlik doğrulama** — `/analyze` ve `/test` endpoint'leri `X-API-Key` header'ı gerektiriyor. Key `.env`'den okunuyor; boş bırakılırsa auth devre dışı kalır (geliştirme modu).
- **ESP32-CAM portal'a API key alanı** — ilk kurulumda girilir, NVS'e kaydedilir, her fotoğraf gönderiminde header olarak eklenir.
- **MQTT üzerinden canlı config güncelleme** — `akilli-sinif/{sinif-id}/control/config` topic'ine JSON göndererek `api_key`, `server_url`, `mqtt_broker` değiştirilebilir. Restart gerektirmez.
- **IP adresi MQTT yayını** — her boot'ta `akilli-sinif/{sinif-id}/status/ip` topic'ine `retain=true` ile yayınlanır.
- **HTTP config sıfırlama** — `http://<cihaz-IP>/reset-config` ile NVS silinir, portal açılır. Kapalı kutularda GPIO0'a erişim gerekmez.
- **WiFi kopunca otomatik portal** — ~50 sn bağlanamazsa portal açılır; mevcut ayarlar (API key, server URL) korunur, sadece WiFi bilgisi güncellenir. Portal 5 dk açık kalır.
- **MQTT broker alanı portal'a eklendi** — ilk kurulumda girilir, NVS'e kaydedilir.
- **Fotoğraf rotasyonu** — `captured_images/` klasöründe maks. 200 fotoğraf, 7 günden eski olanlar otomatik siliniyor.

### Security

- MQTT kullanıcı adı/şifresi ve YOLO API key `.env` dosyasına taşındı — artık kaynak kodda değil.
- `python-dotenv` bağımlılığı `requirements.txt`'e eklendi.

## [1.0.0] - 2026-04-14

### Added

- **ESP32-PLC** — DHT22, LDR, MQ-135, PIR, Reed Switch sensörleri; LED Strip ve DC Fan PWM kontrolü; TFT ekran (ST7735) durum göstergesi.
- **ESP32-CAM** — periyodik JPEG çekim (kişi varsa 10 sn, yoksa 60 sn); HTTP POST ile YOLOv8 sunucusuna gönderim; PSRAM varsa VGA, yoksa QVGA.
- **ESP32-Simulator** — sinüs bazlı sahte sensör verisi üretimi; donanımsız test.
- **Mosquitto MQTT Broker** — kullanıcı tabanlı kimlik doğrulama; topic yapısı: `akilli-sinif/{sinif-id}/{kategori}/{alt_konu}`.
- **Node-RED orkestrasyon** — sensör verilerini InfluxDB'ye yazma, otomasyon kuralları (ışık, fan, klima), OTA yönetim arayüzü.
- **InfluxDB** — zaman serisi veritabanı, 180 gün saklama, bucket: `sinif_data`.
- **YOLOv8 kişi sayma** — YOLOv8n (nano) modeli, Flask REST API (port 5000), YOLO yoksa mock mod.
- **Grafana dashboard** — gerçek zamanlı sensör verisi görselleştirme.
- **WiFiManager captive portal** — her ESP32 için tek seferlik WiFi/MQTT/sınıf-ID yapılandırması, NVS'e kalıcı kayıt.
- **GPIO0 ile NVS sıfırlama** — 5 sn basılı tutarak config silme.
- **OTA firmware güncelleme** — `v*.*.*` tag push → GitHub Actions derler → Release oluşturur → Node-RED'den MQTT ile tetiklenir.
- **Çok sınıf desteği** — tek firmware, N sınıf; her ESP32 portal'dan kendi kimliğini alır.

[1.1.0]: https://github.com/MagiMigi/akilli-sinif/compare/v1.0.0...v1.1.0
[1.0.0]: https://github.com/MagiMigi/akilli-sinif/releases/tag/v1.0.0
