# Changelog

Tüm önemli değişiklikler bu dosyada belgelenir.
Format: [Keep a Changelog](https://keepachangelog.com/tr/1.1.0/), [Semantic Versioning](https://semver.org/).

> **Not:** Her firmware (PLC, CAM, SIM) bağımsız versiyon takip eder
> (`firmware/<device>/VERSION`). Aşağıdaki başlıklar projenin bütünündeki
> önemli değişiklikleri özetler; tek firmware'a özgü küçük yamalar git
> log'unda görülebilir.

## [Unreleased]

(degisiklik yok)

## [2.1.6] - 2026-06-03

### Fixed

- **PLC akim olcumu kalibrasyonu (PLC v1.3.6)** — `rawToAmps` placeholder
  piecewise-linear sabitleri (`CAL_*`, sahada olculmemis) ~1:1 voltaj→akim
  orani veriyordu (1.8V girisinde ~2A okuyordu). Donanim tasarimina gore
  (shunt + LM358, `1.5A → 3.0V`) oran-tabanli tek lineer fit ile degistirildi:
  `A = pinVolt × AMPS_PER_VOLT` (`AMPS_PER_VOLT = 1.5/3.0 = 0.5 A/V`).
  Kalibrasyon noktasi (`CAL_AMPS`/`CAL_VOLTS`) koda acikca yazildi. Artik
  3.0V → 1.5A, 1.8V → 0.9A, 0V → 0A.

## [2.1.0] - 2026-05-18

### Added

- **PLC ladder kodu — cooling/heating role kontrolu (PLC v1.3.0)** —
  `esp32-relay-test` sketch'inde dogrulanmis BC337 + JRC-19F role surucu
  topolojisi PLC ana sketch'ine entegre edildi. GPIO 21 cooling
  (12V DC fan), GPIO 22 heating (22Ω 5W direnc). DHT11 sicakligi ile
  histeresis: cooling >26°C ON / <24°C OFF, heating <20°C ON / >22°C OFF.
  Reed switch (GPIO 26) pencere kilidi → ikisi de zorla KAPAT. MQTT
  manuel komut sonrasi 5 dk otomasyon override. Yeni topic'ler:
  `control/cooling`, `control/heating`, `control/mode` ({"auto":bool}).
- **Mobil push uyarıları aktif** — sıcaklık ≥ 35°C, hava kalitesi ≥ 500 ppm,
  OTA başarısız / başarılı (updating sonrası), cihaz online → offline,
  broker error/offline. Kenar tetiklemeli, `(classroomId, event)` başına
  60s debounce. `mobile-app/src/lib/notifications.ts` — Zustand
  `subscribe()` ile store dışında, side-effect-safe.
- **ESP32-CAM ve ESP32-SIM tam OTA pipeline'ina dahil** — ikisi de artık
  `control/ota` ve `control/reset` (single + broadcast) topic'lerini
  dinler. CAM mevcut `performOTA` fonksiyonu MQTT dispatcher'a bağlandı;
  SIM kendi binary'si (`firmware-sim-vX.Y.Z.bin`) için TLS+CA-pin+MD5
  korumalı OTA implementasyonu eklendi.

### Security

- **YOLO sunucusu sertleştirildi:** `X-Classroom-ID` header'i regex whitelist
  (`^[A-Za-z0-9_-]{1,32}$`) ile dogrulanir → path traversal kapali. API key
  `hmac.compare_digest` ile karsilastiriliyor (timing attack korumasi).
  Flask `MAX_CONTENT_LENGTH = 8 MB` ve PIL `MAX_IMAGE_PIXELS = 150 MP` ile
  decompression-bomb / memory DoS engeli. `/`, `/count/<id>` artik auth ister;
  `/status` endpoint'i auth'lu detay icin eklendi.
- **Mosquitto TLS opt-in dokümante edildi** — `server/mosquitto/gen-certs.sh`
  self-signed CA + server cert üretir (SAN = LAN IP + localhost + hostname).
  `mosquitto.conf` yorumu uncomment recipe'sini gösterir; `1883` listener'ı
  üstüne LAN-only güvenlik uyarısı eklendi.
- **YOLO Flask opsiyonel TLS** — `YOLO_TLS_CERT` + `YOLO_TLS_KEY` env
  değişkenleri set ise HTTPS, aksi halde plain HTTP (geriye dönük uyumlu).
  Yeni dependency yok; Flask native `ssl_context`.

### Fixed

- **Mobil OTA simülatör hedefi:** UI `simulator` değerini `firmware-simulator`
  olarak arıyor, CI artifact'i `firmware-sim` olarak yayinliyordu — `sim`
  hedefli release asset'i hiç bulunamıyordu. UI hedefi `sim`'e hizalandi.
- README pin haritasi DHT22 yazıyordu, firmware DHT11 kullanıyor — düzeltildi.

### Notes

- **ESP32-CAM HTTPS POST v2 planlı** — NVS'den `yolo_ca_pem`, WiFiManager
  portalına alanı, `https://` prefix detection. Heap budget (TLS handshake
  ~16-30 KB ek + frame buffer) ölçülecek. Şu an plain HTTP'de; bu yüzden
  CAM ↔ YOLO trafiği LAN sınırına hapsedilmelidir.

## [2.0.0] - 2026-04-25

Güvenlik odaklı büyük versiyon. Public repo varsayımıyla tüm sırlar ayrıştırıldı.

### Security

- **WPA2 captive portal** — `Akilli-{PLC,CAM,SIM}-Setup` AP'leri WPA2 korumali.
  Şifre cihazın MAC son 3 byte'ından türetilir (`akilli-XXXXXX`). PLC'de TFT'de,
  CAM/SIM'de Serial monitor'de gösterilir.
- **HTTP Basic Auth** — CAM `/` ve `/reset-config` endpoint'leri AP şifresiyle
  korumali (kullanici `admin`).
- **OTA TLS sertleştirme** — `WiFiClientSecure.setInsecure()` kaldirildi,
  `GITHUB_ROOT_CA` (ISRG Root X1) pin'lendi. URL allowlist:
  `https://github.com/MagiMigi/akilli-sinif/releases/` dışı reddedilir.
- **MD5 zorunlu** — OTA payload'da MD5 yoksa `<url>.md5` sidecar otomatik
  indirilir; yine yoksa update reddedilir. Kötü niyetli/bozuk binary akmaz.
- **Mosquitto ACL** — `esp32` cihazları sadece sensor/status yazar, control
  okur (komut yazamaz). `mobile` user control yazar, sensor/status okur.
- **Sırlar repo'dan ayrıldı** — MQTT user/password `firmware/secrets.h`
  (gitignored). `secrets.h.example` template repo'da. CI build'de
  `MQTT_PASSWORD` GitHub Secret'tan `secrets.h` dinamik üretilir.
- **YOLO API_KEY zorunlu** — boş ya da <16 karakter ise startup'ta
  `RuntimeError`. (Önceki "boş = dev mode" davranışı kaldırıldı.)

### Added

- **5 sn BOOT-hold reset** — SIM'e de eklendi (PLC/CAM zaten vardı).
- **Otomatik portal** — WiFi ~50 sn bağlanamazsa portal açar (5 dk),
  mevcut config korunur.

### Removed

- `flows.json` (v1) ve `flows-v2.json` silindi — `flows-v3.json` kapsayıcı.
- **PLC PWM/MOSFET fan kaldırıldı (GPIO 16)** — relay tabanlı cooling/heating
  ile değiştirildi. Eski `control/fan` topic'i ve `setFanSpeed()` API'si
  kaldırıldı; yeni topic'ler `control/cooling` + `control/heating`.

## [1.2.0] - 2026-04 (PLC firmware ekran iyileştirmeleri)

### Added

- **TFT 5 sayfa menü** — sensors, clock, now (ders), week (haftalık plan),
  announcements. BOOT/PREV/NEXT butonlarıyla gezinme; uzun basışta rotasyon
  duraklatma.
- **Ders programi MQTT entegrasyonu** — `schedule/current`, `schedule/today`,
  `schedule/week` topic'leri; Node-RED'den dolduruluyor.
- **Header'da saat/tarih** — tüm sayfalarda görünür.

### Fixed

- v1.2.3 — Sensör değerleri x=60'da hizalı (leading-space padding kaldırıldı).
- v1.2.2 — Saat/tarih header'da kalıcı.
- v1.2.1 — Ana ekrana saat/tarih geri eklendi.

## [1.1.0] - 2026-04-14

### Added

- **YOLO API Key kimlik doğrulama** — `/analyze` ve `/test` endpoint'leri
  `X-API-Key` header'ı gerektiriyor. Key `.env`'den okunuyor.
  *(2.0.0'da sertleştirildi: zorunlu, boş bırakılamaz.)*
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

- **ESP32-PLC** — DHT11, LDR, MQ-135, PIR, Reed Switch sensörleri; LED Strip ve DC Fan PWM kontrolü; TFT ekran (ST7735) durum göstergesi.
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

[Unreleased]: https://github.com/MagiMigi/akilli-sinif/compare/v2.0.0...HEAD
[2.0.0]: https://github.com/MagiMigi/akilli-sinif/compare/v1.2.0...v2.0.0
[1.2.0]: https://github.com/MagiMigi/akilli-sinif/compare/v1.1.0...v1.2.0
[1.1.0]: https://github.com/MagiMigi/akilli-sinif/compare/v1.0.0...v1.1.0
[1.0.0]: https://github.com/MagiMigi/akilli-sinif/releases/tag/v1.0.0
