# ESP32 Relay-Test Mini Project

> **DEPRECATED — esp32-plc v1.3.0'a entegre edildi.**
> Bu sketch'te doğrulanan donanım topolojisi (BC337 NPN low-side + JRC-19F 5V coil + 1N4007 flyback) ve sürücü kodu artık ana PLC sketch'inde yaşıyor. Cooling GPIO 21, heating GPIO 22, MQTT topic'leri `akilli-sinif/{classroom_id}/control/{cooling,heating,mode}`. Ayrıca DHT11 histeresis ladder + reed switch pencere kilidi + 5 dk manuel override otomasyonu eklendi (config.json `automation.thermal_control`). Bu klasör sadece referans/donanım test sketch'i olarak kalır.

Ana akilli-sinif PLC projesinden **ayrı**, sadece **2 röle ON/OFF** kontrolü yapan minimal modül. Aynı MQTT broker, aynı OTA altyapısı, aynı `secrets.h` ve `version.h` dosyaları kullanılır.

## Amaç

3.3V GPIO ile 12V hattını **röle üzerinden** kontrol etmeyi pratik etmek (sadece MOSFET ile kalmayıp NPN+röle yolunu deneyimlemek). İleride ana projenin MOSFET'li DC fan kontrolünün yerini alabilir.

## Pin Atamaları

| GPIO | Görev | Bağlı Devre |
|------|-------|-------------|
| **21** | Cooling röle (BC337 base, 1kΩ üzerinden) | Röle 1 → 12V hattı: DC fan + LED |
| **22** | Heating röle (BC337 base, 1kΩ üzerinden) | Röle 2 → 12V hattı: 22Ω direnç + LED |

Ana projedeki PLC firmware GPIO 21/22'yi kullanmıyor; ileride entegrasyon için çakışma yok.

## Donanım Şeması (her röle için aynı)

```
ESP32 GPIO 21/22 ──── 1kΩ ──── BC337 base
                                  │
                                  ├── collector ─── 5V relay coil ─── +5V
                                  │                    │
                                  │                    └── 1N4007 (flyback, ters paralel) ─── +5V
                                  │
                                  └── emitter ─── GND  (ESP32 + 5V + 12V GND ortak!)

Relay NO/COM kontak'ı 12V hattını siviçler:

   +12V ── [Relay COM] ── [Relay NO] ── (yük) ── GND

Röle 1 yükü:  DC fan (paralel) + LED + ~1kΩ seri direnç (LED için)
Röle 2 yükü:  22Ω güç direnci (paralel) + LED + ~1kΩ seri direnç (LED için)
```

**ÖNEMLİ uyarılar:**
- **Flyback diyotsuz çalıştırma:** Bobin enerjisi kapanırken back-EMF transistörü öldürür. 1N4007 her rölenin bobinine ters paralel **zorunlu**.
- **LED ayrı seri direnç:** 22Ω direnç + LED **paralel** ise LED'in kendi seri direnci (~1kΩ) olmalı. Aksi halde LED 12V'a doğrudan bağlanır ve ölür.
- **Ortak GND:** ESP32, 5V kaynak, 12V kaynak GND'leri **ortak** olmalı. Aksi halde transistör sürmez.
- BC337 max IC = 800 mA, 5V röle bobini ~70 mA → bol margin var.

## İlk Kurulum

1. `secrets.h` dosyasını `firmware/secrets.h.example`'dan kopyala (eğer yoksa) ve MQTT user/sifre gir. Bu dosya tüm cihazlarla paylaşılır.
2. Arduino IDE veya `arduino-cli` ile `src/main/main.ino`'yu derle ve flash et.
   ```bash
   arduino-cli compile --fqbn esp32:esp32:esp32 src/main
   arduino-cli upload --fqbn esp32:esp32:esp32 -p /dev/ttyUSB0 src/main
   ```
3. İlk boot'ta `Relay-Test-Setup` WiFi AP'sine bağlan (WPA2 şifre `relay-XXXXXX`, MAC son 3 byte hex).
4. Tarayıcıda 192.168.4.1 → portal aç:
   - WiFi SSID/Pass
   - **MQTT Broker IP**
   - **Device ID** (örn `relay-test-1`)
5. Kaydet → ESP32 yeniden başlar ve broker'a bağlanır.

## Config Sıfırlama

GPIO 0 (BOOT butonu) **5 saniye** basılı tut → NVS silinir → portal yine açılır.
Veya MQTT üzerinden:
```
mosquitto_pub -t "akilli-sinif/relay-test/relay-test-1/control/reset" -m '{"action":"reset_config"}'
```

## MQTT Topic'leri

Tüm topic'ler `akilli-sinif/relay-test/<device_id>/...` ya da broadcast için `akilli-sinif/relay-test/all/...`.

### Subscribe (komut alır)

| Topic | Payload | Etkisi |
|-------|---------|--------|
| `.../control/cooling` | `{"state":"on"}` veya `{"state":"off"}` | Röle 1 (cooling) ON/OFF |
| `.../control/heating` | `{"state":"on"}` veya `{"state":"off"}` | Röle 2 (heating) ON/OFF |
| `.../control/ota` | `{"action":"update","version":"v1.0.1","url":"https://github.com/MagiMigi/akilli-sinif/releases/.../firmware-relay-test.bin","md5":"..."}` | OTA güncelleme tetik |
| `.../control/reset` | `{"action":"reset_config"}` | NVS sil + portal modu |

Broadcast: `akilli-sinif/relay-test/all/control/{cooling\|heating\|ota\|reset}` (tüm relay-test cihazları dinler).

### Publish (durum yayınlar)

| Topic | Payload | Retained |
|-------|---------|----------|
| `.../status/connection` | `{"status":"online\|offline","device":"relay-test-1","ip":"...","rssi":-60,"uptime":123,"firmware_version":"v1.0.0"}` | ✓ |
| `.../status/cooling` | `{"state":"on\|off","ts":12345}` | ✓ |
| `.../status/heating` | `{"state":"on\|off","ts":12345}` | ✓ |
| `.../status/ota` | OTAManager üretir | ✗ |

LWT: `status/connection` → `{"status":"offline","device":"..."}` retained, broker bağlantı koptuğunda otomatik yayınlar.

## Manuel Test

```bash
# Cooling açma
mosquitto_pub -h <broker> -u mobile -P <pw> \
  -t "akilli-sinif/relay-test/relay-test-1/control/cooling" -m '{"state":"on"}'

# Heating kapama
mosquitto_pub -h <broker> -u mobile -P <pw> \
  -t "akilli-sinif/relay-test/relay-test-1/control/heating" -m '{"state":"off"}'

# Tüm durumları izleme
mosquitto_sub -h <broker> -u nodered -P <pw> -t "akilli-sinif/relay-test/#" -v
```

Node-RED'de "🔌 Relay Test" tab'inden de buton'larla test edilebilir.

## OTA Güncelleme

1. Yeni VERSION yaz (örn `v1.0.1`).
2. CI build → GitHub release'e `firmware-relay-test.bin` + `firmware-relay-test.bin.md5` ekle.
3. Node-RED "🔌 Relay Test" tab'inden OTA tetikle veya:
   ```bash
   mosquitto_pub -t "akilli-sinif/relay-test/relay-test-1/control/ota" -m '{
     "action":"update",
     "version":"v1.0.1",
     "url":"https://github.com/MagiMigi/akilli-sinif/releases/download/relay-test-v1.0.1/firmware-relay-test.bin"
   }'
   ```
4. ESP32 indirir, MD5 doğrular, uygular ve restart eder.

## PWM?

**Yok.** Mekanik röle = sadece ON/OFF. Hızlı switching kontak aşındırır. PWM için MOSFET veya SSR gerekir; bu modül "röle deneyimi" için kuruldu.
