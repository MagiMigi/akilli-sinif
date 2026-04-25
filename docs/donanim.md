# Donanim Referansi

Bu dokuman Akilli Sinif Sistemi'nin donanim baglantilari, pin haritalari, devre semalari ve test prosedurlerini icerir.

Kaynak: `firmware/esp32-plc/src/main/main.ino`, `firmware/esp32-cam/src/main/main.ino`.

---

## Genel Mimari

```
12V DC Kaynagi
      |
      +-------------------> Serit LED (MOSFET ile, GPIO13)
      |
      +-------------------> DC Fan (MOSFET ile, GPIO12)
      |
      +---> Buck Converter (12V -> 5V, 5A)
                  |
                  +-------> ESP32-PLC (VIN)
                  |
                  +-------> ESP32-CAM (5V)
                  |
                  +-------> PIR Sensor (VCC)
                  |
                  +-------> MQ-135 (VCC)

ESP32 3.3V Pini (dahili regulator)
      |
      +-------> TFT Display (VCC)
      |
      +-------> DHT11 (VCC)
      |
      +-------> LDR Devresi (3.3V)

ONEMLI: Tum GND'ler birbirine bagli olmali!
```

---

## Malzeme Listesi

### ESP32 Kartlari

| Bilesen | Adet | Kullanim |
|---------|------|----------|
| ESP32 DevKit V1 | 2 | PLC + Simulator |
| ESP32-CAM (AI-Thinker) | 1 | Kamera modulu (opsiyonel) |

### Sensorler

| Bilesen | Adet | Besleme | Aciklama |
|---------|------|---------|----------|
| DHT11 | 1 | 3.3V | Sicaklik ve nem |
| LDR | 1 | 3.3V | Isik seviyesi (voltaj bolucu ile) |
| MQ-135 | 1 | 5V | Hava kalitesi (1-2 dk isinma suresi) |
| PIR (HC-SR501) | 1 | 5V | Hareket algilama |
| Reed Switch | 1 | Dahili pull-up | Pencere durumu |

### Aktuatorler

| Bilesen | Adet | Besleme | Aciklama |
|---------|------|---------|----------|
| LED Strip | 1 | 12V | MOSFET ile PWM kontrolu |
| DC Fan | 1 | 12V | MOSFET ile PWM kontrolu |

### Ekran

| Bilesen | Adet | Besleme | Aciklama |
|---------|------|---------|----------|
| TFT Ekran (ST7735 1.44") | 1 | 3.3V | SPI baglanti, durum gostergesi |

### Guc

| Bilesen | Adet | Aciklama |
|---------|------|----------|
| 12V DC Adaptor | 1 | Ana guc kaynagi |
| XL4015 Buck Converter | 1 | 12V > 5V donusturucu (5A) |

### Devre Elemanlari

| Bilesen | Adet | Aciklama |
|---------|------|----------|
| IRLZ44N MOSFET | 2 | LED ve Fan kontrolu |
| PC817 Optocoupler | 2 | ESP32-MOSFET izolasyonu |
| 1K direnc | 2 | PC817 LED akimi sinirlamasi |
| 10K direnc | 4+1 | 2x pull-up, 2x pull-down, 1x DHT11 pull-up |
| 1N4007 diyot | 1 | Fan flyback korumasi |
| Breadboard | 1 | Prototip baglanti |
| Jumper kablolar | ~20 | Baglanti |

---

## ESP32-PLC Pin Haritasi

### Sensor Pinleri (Giris)

| Bilesen | ESP32 GPIO | Tur | Notlar |
|---------|------------|-----|--------|
| DHT11 DATA | 4 | Dijital | +10K pull-up direnc (3.3V'a) |
| LDR | 34 | Analog (ADC) | Voltaj bolucu ile (10K pull-down) |
| MQ-135 AOUT | 35 | Analog (ADC) | 1-2 dk isinma suresi |
| PIR OUT | 27 | Dijital | HIGH = hareket var |
| Reed Switch | 26 | Dijital | INPUT_PULLUP, LOW = kapali |

### Aktuator Pinleri (Cikis)

| Bilesen | ESP32 GPIO | Tur | Notlar |
|---------|------------|-----|--------|
| LED Strip | 13 | PWM | PC817 optocoupler ile izole |
| DC Fan | 12 | PWM | PC817 optocoupler ile izole |

### TFT Ekran Pinleri (SPI)

| TFT Pini | ESP32 GPIO | Aciklama |
|----------|------------|----------|
| CS | 15 | Chip Select |
| DC (A0/RS) | 33 | Data/Command |
| RST | 32 | Reset |
| MOSI (SDA) | 23 | SPI Data |
| SCLK (SCK) | 18 | SPI Clock |
| BL | 3.3V | Backlight (sabit acik) |

### Ozel Pinler

| Pin | Fonksiyon |
|-----|-----------|
| GPIO0 (BOOT) | Config sifirlama butonu (5 sn basili tut) |

### PWM Ayarlari

Kodda tanimli degerler (`main.ino`):

| Ayar | Deger | Aciklama |
|------|-------|----------|
| PWM_FREQ | 500 Hz | PC817 optocoupler icin uygun dusuk frekans |
| PWM_RESOLUTION | 8-bit | 0-255 aralik |

> **Not:** PC817 optocoupler yavas calistigi icin standart 5 kHz yerine 500 Hz PWM kullaniliyor.

---

## ESP32-CAM Pin Haritasi

AI-Thinker ESP32-CAM modulu kullanilir. Kamera pinleri sabit:

| Pin | GPIO | Aciklama |
|-----|------|----------|
| PWDN | 32 | Power down |
| RESET | -1 | Kullanilmiyor |
| XCLK | 0 | Clock |
| SIOD | 26 | I2C Data |
| SIOC | 27 | I2C Clock |
| Y2-Y9 | 5,18,19,21,36,39,34,35 | Data pinleri |
| VSYNC | 25 | |
| HREF | 23 | |
| PCLK | 22 | |
| Flash LED | 4 | Dahili flash (kod icinde devre disi) |

### Kamera Ayarlari

| Ayar | PSRAM Var | PSRAM Yok |
|------|-----------|-----------|
| Cozunurluk | VGA (640x480) | QVGA (320x240) |
| JPEG Kalite | 12 | 15 |
| Frame Buffer | 2 | 1 |

### Cekim Araliklari

| Durum | Aralik | Aciklama |
|-------|--------|----------|
| Kisi var | 10 saniye | Daha sik cekim |
| Kisi yok | 60 saniye | Enerji tasarrufu |

### Web Sunucusu

ESP32-CAM dahili web sunucusu barindirir (port 80):

| Endpoint | Aciklama |
|----------|----------|
| `GET /` | Durum sayfasi (sinif ID, firmware, IP) |
| `GET /reset-config` | NVS'i siler, cihaz yeniden baslar, portal acar |

---

## MOSFET Surucu Devresi

Her kanal icin (LED ve Fan) ayni devre kullanilir:

### Devre Semasi

```
ESP32 GPIO ---> 1K direnc ---> PC817 Anot
                                  |
                              PC817 Katot ---> ESP32 GND

PC817 Collector ---> 10K pull-up ---> +12V
PC817 Collector ---> IRLZ44N Gate
PC817 Emitter ---> GND (12V tarafi)

IRLZ44N Gate ---> 10K pull-down ---> GND
IRLZ44N Drain ---> LOAD (-) ucu (LED veya Fan)
IRLZ44N Source ---> GND
LOAD (+) ---> +12V
```

### LED Kanali (GPIO13)

1. ESP32 GPIO13 > 1K direnc > PC817 Anot
2. PC817 Katot > ESP32 GND
3. PC817 Collector > 10K > +12V
4. PC817 Collector > IRLZ44N Gate
5. PC817 Emitter > GND (12V tarafi)
6. IRLZ44N Gate > 10K > GND (pull-down)
7. IRLZ44N Drain > Serit LED (-) ucu
8. IRLZ44N Source > GND
9. Serit LED (+) > +12V

### Fan Kanali (GPIO12)

1. ESP32 GPIO12 > 1K direnc > PC817 Anot
2. PC817 Katot > ESP32 GND
3. PC817 Collector > 10K > +12V
4. PC817 Collector > IRLZ44N Gate
5. PC817 Emitter > GND (12V tarafi)
6. IRLZ44N Gate > 10K > GND (pull-down)
7. IRLZ44N Drain > DC Fan (-) ucu
8. IRLZ44N Source > GND
9. DC Fan (+) > +12V
10. **1N4007 flyback diyot**: Katot > +12V, Anot > IRLZ44N Drain (Fan'a paralel)

> **ONEMLI:** Fan icin flyback diyot MUTLAKA kullanilmali. Motor durundugunda olusan ters EMF, diyot olmadan MOSFET'i yakabilir.

---

## Sensor Baglanti Detaylari

### DHT11 (GPIO4)

```
DHT11 VCC  ---> ESP32 3.3V
DHT11 GND  ---> GND
DHT11 DATA ---> ESP32 GPIO4
                    |
                   10K direnc ---> 3.3V (pull-up)
```

### LDR (GPIO34)

Voltaj bolucu devresi:

```
3.3V ---> LDR ---> GPIO34 ---> 10K direnc ---> GND
```

- Karanlik: LDR direnci yuksek > GPIO34 voltaji dusuk > dusuk ADC degeri
- Aydinlik: LDR direnci dusuk > GPIO34 voltaji yuksek > yuksek ADC degeri
- ADC 12-bit: 0-4095 > kod icinde 0-1000 lux'a map edilir

### MQ-135 (GPIO35)

```
MQ-135 VCC  ---> Buck Converter 5V
MQ-135 GND  ---> GND
MQ-135 AOUT ---> ESP32 GPIO35
```

- 1-2 dakika isinma suresi gerektirir
- ADC 12-bit: 0-4095 > kod icinde 0-500 ppm'e map edilir
- Dusuk deger = temiz hava, yuksek deger = kirli hava

### PIR HC-SR501 (GPIO27)

```
PIR VCC ---> Buck Converter 5V
PIR GND ---> GND
PIR OUT ---> ESP32 GPIO27
```

- HIGH = hareket algilandi
- Uzerindeki potansiyometre ile hassasiyet ve gecikme ayarlanabilir

### Reed Switch (GPIO26)

```
Reed Switch bir ucu ---> ESP32 GPIO26
Reed Switch diger ucu ---> GND
```

- `INPUT_PULLUP` kullanilir (dahili pull-up direnc)
- LOW = kapali (magnet yakin, pencere kapali)
- HIGH = acik (magnet uzak, pencere acik)

---

## TFT Ekran Durum Gostergeleri

PLC firmware'inin TFT ekraninda gosterilen bilgiler:

### Normal Ekran Duzenlemesi

```
+------------------------+
| SINIF-1          [*]   |  <- Baslik + MQTT durumu (yesil/kirmizi daire)
|========================|
| KISI SAYISI            |
|        15              |  <- Buyuk font, yesil (dolu) veya kirmizi (bos)
| SINIF DOLU             |
|========================|
| Sicaklik:  23.5C       |  <- Renk: mavi(<18) cyan(<22) yesil(<26) sari(<30) kirmizi(>30)
| Nem:       65%         |
| Hava:      120ppm      |  <- Renk: yesil(<100) sari(<200) kirmizi(>200)
+------------------------+
```

### Uyari Durumlari

| Durum | TFT Davranisi |
|-------|---------------|
| Normal | Siyah arka plan, yesil baslik |
| Sicaklik 28-32C | Sari arka plan + baslikta "!" ikonu |
| Sicaklik >32C | Kirmizi yanip sonen ekran + "UYARI!" + "SICAKLIK COK!" mesaji |
| Hava kalitesi kotu | Sadece ppm degeri kirmizi renkte (tam ekran uyari YOK) |
| MQTT bagli | Sag ust yesil daire |
| MQTT kopuk | Sag ust kirmizi daire |
| Kisi yok | Sayi kirmizi, "SINIF BOS" |
| Kisi var | Sayi yesil, "SINIF DOLU" |

> **Not:** Sicaklik 27C altina dustugunde uyari otomatik kalkar.

---

## Test Proseduru

### 1. Buck Converter Testi

1. 12V DC kaynagini buck converter girisine bagla
2. Multimetre ile cikisi olc > 5.0V olmali
3. **ESP32 baglamadan once voltaji dogrula!** 12V ESP32'ye giderse yakar.

### 2. DHT11 Testi

1. DHT11'i 3.3V'a bagla (pull-up direnc ile)
2. Serial Monitor'da sicaklik/nem degerlerini kontrol et
3. `DHT okuma hatasi!` mesaji geliyorsa kablolari kontrol et

### 3. PIR Testi

1. PIR'i 5V'a bagla (buck converter)
2. Serial Monitor'da hareket durumunu izle
3. Onunde hareket et > `Hareket: VAR` olmali
4. Surekli HIGH kaliyorsa PIR uzerindeki hassasiyet pot'unu ayarla

### 4. MQ-135 Testi

1. MQ-135'i 5V'a bagla
2. **1-2 dakika isinmasini bekle** (yoksa yanlis okuma olur)
3. Normal hava: 50-150 ppm
4. Ufleme veya cozucu > deger artmali

### 5. LDR Testi

1. Voltaj bolucu devresini kur
2. Serial Monitor'da lux degerini izle
3. Eli ile kapat > deger dusmeli
4. Isik tut > deger artmali

### 6. MOSFET + LED Testi

1. MOSFET devresini kur (PC817 + IRLZ44N + direncler)
2. Serit LED'i bagla
3. MQTT'den komut gonder:
   ```bash
   mosquitto_pub -h localhost -u esp32 -P "$MQTT_PASS" \
     -t "akilli-sinif/sinif-1/control/led" \
     -m '{"brightness": 50}'
   ```
4. LED %50 parlamali

### 7. MOSFET + Fan Testi

1. Fan MOSFET devresini kur (**flyback diyot unutma!**)
2. DC Fan'i bagla
3. MQTT'den komut gonder:
   ```bash
   mosquitto_pub -h localhost -u esp32 -P "$MQTT_PASS" \
     -t "akilli-sinif/sinif-1/control/fan" \
     -m '{"speed": 50}'
   ```
4. Fan %50 hizda donmeli

### 8. TFT Ekran Testi

ESP32'yi baslat, ekranda sirasiyla:
1. "AKILLI SINIF SISTEMI / Baslatiliyor..."
2. "WiFi Kurulum / Akilli-Sinif-Setup agina baglan" (veya WiFi OK!)
3. "MQTT Baglaniyor..."
4. Ana ekran (kisi sayisi, sicaklik, nem, hava kalitesi)

---

## Guvenlik Notlari

1. **Flyback diyot**: Fan icin MUTLAKA kullan
2. **Ortak GND**: Tum GND'leri birlestir (ESP32, 12V, sensorler)
3. **Buck converter**: ESP32 baglamadan once voltaji kontrol et (5V)
4. **MQ-135**: Isinma suresi bekle (ilk 1-2 dakika yanlis deger verir)
5. **Kisa devre**: 12V hatti ESP32'nin 3.3V/5V girisleriyle temas etmemeli
6. **PC817 izolasyonu**: ESP32'nin GND'si ile 12V hattinin GND'si optocoupler sayesinde elektriksel olarak ayridir. Ancak pratikte ortak GND kullaniliyor (basitlik icin)
