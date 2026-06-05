# Donanim Referansi

Bu dokuman Akilli Sinif Sistemi'nin donanim baglantilari, pin haritalari, devre semalari ve test prosedurlerini icerir.

Kaynak: `firmware/esp32-plc/src/main/main.ino`, `firmware/esp32-cam/src/main/main.ino`.

---

## Genel Mimari

```
12V DC Adaptor
      |
      v
[Cam sigorta 1.5A] --+-- [TVS 15V] -- 12V GND   (transient koruma)
                     |
                     v
              +------+-------------------+
              |                          |
              v                          v
      Cooling fan (12V)           Heater (22ohm 5W)
      (role kontak NO)            (role kontak NO)
              |                          |
              v                          v
              +------- shunt 0.1ohm -----+
                                          |
                              [Sistem GND <-> 12V GND tek kopru: shunt]
                                          |
              +---------------------------+
              |
              v
       [Buck XL4015 12V->5V]
              |
              +-- ESP32 PLC (VIN)
              +-- ESP32-CAM (5V)
              +-- PIR HC-SR501 (VCC)
              +-- MQ-135 (5V: A1/A2/H1)
              +-- MCP6002 op-amp (V+ supply)
              +-- Cooling rolesi bobini
              +-- Heating rolesi bobini

ESP32 3.3V (dahili regulator)
      +-- DHT11 (3-pin modul)
      +-- TFT ST7735 (VCC + BL)
      +-- Op-amp + giris referansi (sistem GND'ye)

Strip LED -> 12V (MOSFET ile low-side anahtarlama)
Indikator LED -> GPIO 13 -> 220-330ohm -> LED -> GND
```

> **Onemli:** 12V GND ile sistem GND arasindaki TEK elektriksel bag shunt direnci (0.1ohm) uzerindendir. Shunt'i kopruleme/atlama yapma — akim olcumu bozulur ve yuksek akimda shunt yanar.

---

## Malzeme Listesi

### Aktif Bilesenler

| Bilesen | Adet | Kullanim |
|---------|------|----------|
| ESP32 DevKit V1 | 1 | PLC ana kart |
| ESP32-CAM (AI-Thinker) | 1 | YOLO icin kamera modulu |
| BC337 NPN transistor | 2 | Cooling + heating role surucusu |
| IRLZ44N MOSFET | 1 | Strip LED surucu (logic-level) |
| PC817 optocoupler | 1 | Strip LED icin ESP32-12V izolasyonu |
| MCP6002 op-amp | 1 | Akim sensoru amplifikatoru (inverting) |
| JRC-19F 5V role | 2 | Cooling + heating |

### Diyotlar

| Bilesen | Adet | Yer |
|---------|------|-----|
| 1N4007 | 4 | 2x role flyback (bobine paralel), 2x base koruma (GPIO ile BC337 base arasinda) |
| Zener 3.3V | 1 | Op-amp cikisinda GPIO 36 ADC clamp |
| TVS 15V | 1 | 12V hat transient koruma |

### Sensorler

| Bilesen | Adet | Besleme | Aciklama |
|---------|------|---------|----------|
| DHT11 (3-pin Arduino-uyumlu modul) | 1 | 3.3V | Sicaklik ve nem (board uzerinde dahili pull-up) |
| LDR | 1 | 3.3V | Isik seviyesi (10k pull-down voltaj bolucu) |
| MQ-135 | 1 | 5V | Hava kalitesi (1-2 dk isinma; 10k+15k voltaj bolucu) |
| PIR (HC-SR501) | 1 | 5V | Hareket algilama |
| Reed Switch | 1 | Dahili pull-up | Pencere durumu (GPIO ile GND arasi) |

### Aktuatorler

| Bilesen | Adet | Besleme | Aciklama |
|---------|------|---------|----------|
| Strip LED | 1 | 12V | MOSFET ile PWM kontrolu |
| Indikator LED (5mm) | 1 | 3.3V | GPIO 13 PWM gostergesi (PLC hissi) |
| 12V DC fan | 1 | 12V | Cooling, role kontagindan |
| 22ohm 5W guc direnci | 1 | 12V | Heating elemani, role kontagindan |

### Ekran ve Kontrol

| Bilesen | Adet | Besleme | Aciklama |
|---------|------|---------|----------|
| TFT ekran (ST7735 1.44") | 1 | 3.3V | SPI baglanti, 128x128 |
| Tactile button 6x6mm | 2 | Dahili pull-up | NEXT (GPIO 25) |

### Direncler

| Deger | Adet | Yer |
|-------|------|-----|
| 0.1ohm 2W (sint) | 1 | Akim shunt (12V low-side) |
| 22ohm 5W | 1 | Heating elemani |
| 100ohm 1/4W | 1 | MOSFET gate seri direnci |
| 220-330ohm 1/4W | 1 | Indikator LED akim sinirlama |
| 470ohm 1/4W | 1 | PC817 LED akim sinirlama |
| 1k 1/4W | 4 | Op-amp giris (Rin) + op-amp cikis seri + 2x BC337 base |
| 10k 1/4W | 3 | LDR pull-down + MQ-135 voltaj bolucu ust + MOSFET gate pull-down |
| 15k 1/4W | 1 | MQ-135 voltaj bolucu alt |
| 20k 1/4W | 1 | MCP6002 feedback (Rf) |

### Guc

| Bilesen | Adet | Aciklama |
|---------|------|----------|
| 12V DC adaptor (>=2A) | 1 | Ana guc kaynagi |
| Cam sigorta 1.5A + tutucu | 1 | 12V girisinde |
| XL4015 buck converter | 1 | 12V -> 5V (5A) |

---

## ESP32-PLC Pin Haritasi

### Sensor Pinleri (Giris)

| Bilesen | ESP32 GPIO | Tur | Notlar |
|---------|------------|-----|--------|
| DHT11 DATA | 4 | Dijital | 3-pin modul, harici direnc gerekmez |
| LDR | 34 | Analog (ADC1) | 10k pull-down ile voltaj bolucu |
| MQ-135 (B1=B2) | 35 | Analog (ADC1) | 10k+15k voltaj bolucu, max ~3V |
| MCP6002 OUT | 36 (VP) | Analog (ADC1_CH0) | 1k seri + 3.3V zener clamp |
| PIR OUT | 27 | Dijital | HIGH = hareket var |
| Reed Switch | 26 | Dijital | INPUT_PULLUP, GND'ye anahtarlanir, HIGH = pencere acik |

### Buton Pinleri

| Bilesen | ESP32 GPIO | Tur | Notlar |
|---------|------------|-----|--------|
| Buton NEXT | 25 | INPUT_PULLUP | Aktif LOW, GND'ye |
| Anahtar LED (duvar) | 5 | INPUT_PULLUP | Push button, aktif LOW, GND'ye. Toggle: bas -> LED on/off |
| Anahtar COOLING (duvar) | 16 | INPUT_PULLUP | Push button, aktif LOW, GND'ye. Toggle: bas -> cooling on/off |
| Anahtar HEATING (duvar) | 19 | INPUT_PULLUP | Push button, aktif LOW, GND'ye. Toggle: bas -> heating on/off |
| BOOT (config reset) | 0 | INPUT_PULLUP | 5sn basili tut -> NVS sil |

Duvar anahtarlari yazilim debounce 25 ms ile polling tabanlidir. Online (MQTT) ve fiziksel
buton ayni state'i degistirir; durum `actuators/*` topic'lerine **retained** yayinlanir
(broker son durumu tutar, dashboard ve ESP32 reboot sonrasi senkron kalir).

### Aktuator Pinleri (Cikis)

| Bilesen | ESP32 GPIO | Tur | Notlar |
|---------|------------|-----|--------|
| Indikator LED + Strip LED | 13 | PWM (LEDC, 500Hz, 8-bit) | GPIO 13 ikiye ayrilir; bkz. devre semasi 4.8 |
| Cooling rolesi | 21 | Dijital | 1N4007 (GPIO base koruma) + 1k + BC337 + JRC-19F + 1N4007 flyback |
| Heating rolesi | 22 | Dijital | Ayni topoloji, kontak cikisinda 22ohm 5W |

### TFT Ekran Pinleri (SPI)

| TFT Pini | ESP32 GPIO | Aciklama |
|----------|------------|----------|
| CS | 17 | Chip Select |
| DC (A0/RS) | 33 | Data/Command |
| RST | 32 | Reset |
| MOSI (SDA) | 23 | SPI Data |
| SCLK (SCK) | 18 | SPI Clock |
| BL | 3.3V | Backlight (sabit acik) |
| MISO | -1 | Kullanilmiyor |

### PWM Ayarlari

Kodda tanimli degerler (`main.ino`):

| Ayar | Deger | Aciklama |
|------|-------|----------|
| PWM_FREQ | 500 Hz | PC817 optocoupler icin uygun dusuk frekans |
| PWM_RESOLUTION | 8-bit | 0-255 aralik |

> **Not:** PC817 optocoupler yavas oldugu icin standart 5 kHz yerine 500 Hz PWM kullaniliyor.

---

## ESP32-CAM Pin Haritasi

AI-Thinker ESP32-CAM modulu kullanilir. Kamera pinleri sabit:

| Pin | GPIO | Aciklama |
|-----|------|----------|
| PWDN | 32 | Power down |
| RESET | -1 | Kullanilmiyor |
| XCLK | 0 | Clock |
| SIOD | 26 | I2C Data (SCCB SDA) |
| SIOC | 27 | I2C Clock (SCCB SCL) |
| Y2-Y9 | 5,18,19,21,36,39,34,35 | Veri pinleri (D0-D7) |
| VSYNC | 25 | |
| HREF | 23 | |
| PCLK | 22 | |
| Flash LED | 4 | Dahili flash (kod icinde devre disi) |

### Programlama

ESP32-CAM dogrudan USB portu yok, harici USB-TTL (CH340 vb.) gerekir:

```
USB-TTL TX  ---> ESP32-CAM U0R
USB-TTL RX  ---> ESP32-CAM U0T
USB-TTL 5V  ---> ESP32-CAM 5V
USB-TTL GND ---> ESP32-CAM GND
ESP32-CAM IO0 ---> GND  (sadece flash modunda)
```
Yukleme bittikten sonra IO0-GND koprusunu cikar ve resetle.

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

## Devre Semalari

### 4.1 DHT11 (3-pin modul, GPIO 4)

```
3.3V    ---> DHT11 VCC
GND     ---> DHT11 GND
GPIO 4  ---> DHT11 DATA
```

3-pin Arduino-uyumlu DHT11 modul kartinda dahili 10k pull-up vardir. Harici direnc gerekmez.

### 4.2 LDR (GPIO 34)

```
3.3V ---> LDR ---+--- GPIO 34
                 |
                 +--- 10k --- GND
```

- Karanlik: LDR direnci yuksek -> GPIO 34 voltaji dusuk -> dusuk ADC
- Aydinlik: LDR direnci dusuk -> GPIO 34 voltaji yuksek -> yuksek ADC
- ADC 12-bit: 0-4095 -> kod icinde 0-1000 lux'a map edilir

### 4.3 MQ-135 (GPIO 35) — ozel kablolama

Standart modulun yerine cipi dogrudan baglayan ozel devre:

```
5V (buck) ---> A1, A2, H1
GND       ---> H2
B1 = B2   ---> 10k ---+--- GPIO 35
                      |
                      +--- 15k --- GND
```

- 10k+15k voltaj bolucu hem yuk direnci (RL) hem ADC scaler gorevi gorur
- Cikis orani: 15/(10+15) = 0.6 -> max ~3V (ESP32 ADC guvenli)
- 1-2 dakika isinma suresi gerektirir
- Dusuk ADC = temiz hava, yuksek ADC = kirli hava

### 4.4 Akim sensoru — Low-side shunt + MCP6002 inverting (GPIO 36)

12V hatti girisi:

```
12V (+) ---> [Cam sigorta 1.5A] ---+--- [TVS 15V katot bu noktada,
                                   |     anot 12V GND'de]
                                   |
                                   +--- 12V (+) yuklere (rolelere, MOSFET'e, buck'a)
```

12V donus hatti (akim olcum yolu):

```
12V (-) GND ---+--- 0.1ohm shunt ---+--- Sistem GND (ESP32, sensorler, vs.)
               |                     |
               +--- 1k --- MCP6002 V-  |
                          (eviren)   |
                                     |
                          20k feedback (V- <-> Vout)
                          V+ = sistem GND
                          VCC = +5V
                          VEE = sistem GND

MCP6002 OUT ---> 1k ---+--- GPIO 36 (ADC1_CH0, VP)
                     |
                     +--- Zener 3.3V (katot bu noktada, anot GND)
```

**Hesaplamalar:**
- Kazanc: G = -Rf/Rin = -20k/1k = -20x
- Max olcum: 1.5A * 0.1ohm * 20 = 3.0V
- Zener clamp: ESP32 ADC'yi >3.3V'tan korur

**Calisma prensibi:** Sistem akimi (yukler) sistem GND'den shunt uzerinden 12V GND'ye doner. Shunt uzerinde I*R kadar dusus olusur; 12V GND tarafi sistem GND'ye gore negatife kayar. Eviren amplifikator bu negatif gerilimi pozitif olarak ESP32 ADC'ye verir.

**Yazilim kalibrasyonu:** MCP6002 rail-to-rail CMOS op-amp oldugu icin cikis tek beslemede 0V'a yakin inebilir (MCP6002'in aksine, ~0.7V alt rail siniri yok) -> dusuk akimda olcum daha dogrusal ve sifir akimda cikis sifira yakin. Yine de shunt toleransi, op-amp offset'i ve ADC dogrusalsizligi nedeniyle mutlak dogruluk icin `firmware/esp32-plc/src/main/main.ino` icindeki oran-tabanli lineer kalibrasyon ile bilinen yuk noktalarindan (12V fan, 22ohm heater) hesaplanir.

### 4.5 PIR HC-SR501 (GPIO 27)

```
5V (buck) ---> PIR VCC
GND       ---> PIR GND
GPIO 27   ---> PIR OUT
```

- HIGH = hareket algilandi
- Modul uzerindeki iki potansiyometre ile hassasiyet ve gecikme ayarlanabilir
- Harici devre elemani gerekmez

### 4.6 Reed Switch (GPIO 26)

```
GND ---> reed switch ---> GPIO 26
```

- Firmware `pinMode(26, INPUT_PULLUP)` kullanir; harici direnc YOK
- Miknatis yakin (kontak kapali): GPIO 26 = LOW -> "pencere kapali"
- Miknatis uzak (kontak acik): GPIO 26 = HIGH -> "pencere acik"

### 4.7 Menu Butonlari (GPIO 25, 14)

```
GND ---> tactile button NEXT ---> GPIO 25
GND ---> tactile button PREV ---> GPIO 14
```

- Her iki buton INPUT_PULLUP, harici direnc gerekmez
- Aktif LOW: bosta HIGH, basinca LOW
- Detay: `hardware/schematics/menu-buttons.md`

### 4.8 LED PWM (GPIO 13) — iki paralel yol

GPIO 13 ayni anda iki yere ayrilir: kucuk gosterge LED (PLC hissi) ve guc MOSFET'i ile strip LED.

**Yol A (gosterge LED):**

```
GPIO 13 ---> 220-330ohm ---> LED anot
                             LED katot ---> GND
```

PWM degeri direkt LED'in parlakligini kontrol eder; izleme amaciyla.

**Yol B (strip LED, 12V):**

```
GPIO 13 ---> 470ohm ---> PC817 pin 1 (anot)
                         PC817 pin 2 (katot) ---> GND

PC817 pin 4 (kolektor) ---> 12V
PC817 pin 3 (emiter)   ---> 100ohm ---+--- 10k --- GND  (gate pull-down)
                                       |
                                       +--- IRLZ44N gate

IRLZ44N source ---> GND
IRLZ44N drain  ---> strip LED (-)
strip LED (+)  ---> 12V
```

**Notlar:**
- PC817 emitter-follower yapisi: GPIO HIGH -> opto LED yanar -> emiterde ~12V -> MOSFET gate yuksek -> strip LED ON (non-inverting)
- IRLZ44N max VGS = ±20V; 12V gate guvenli
- 10k pull-down: PC817 OFF iken gate'i GND'ye ceker (MOSFET kapali kalir)
- 100ohm: gate kapasitansi sarji icin akim sinirlamasi (anahtarlama yumusatma)

### 4.9 Cooling Rolesi (GPIO 21)

```
GPIO 21 ---> 1N4007 (anot=GPIO, katot=arkasi) ---> 1k ---> BC337 base
                                                            BC337 emiter ---> GND
                                                            BC337 kolektor ---+
                                                                              |
                                                                       JRC-19F bobin (+)
                                                                              |
1N4007 flyback: anot=BC337 kolektor, katot=+5V                                |
                                                                       JRC-19F bobin (-) ---> GND
                                                                       (5V besleme bobinin + ucundan)

JRC-19F COM ---> 12V (+)
JRC-19F NO  ---> DC fan (+)
DC fan (-)  ---> 12V GND
```

**Diyotlarin islevi:**
- **GPIO base diyodu (1N4007):** BC337 C-B short olursa 5V'un GPIO'ya geri akmasini engeller. Vf~0.65V dusuk akimda; base akimi: (3.3 - 0.65 - 0.7)/1k = 1.95 mA -> hFE=100 ile 195 mA surme kapasitesi -> JRC-19F 30 mA bobini icin bol marj.
- **Flyback diyodu (1N4007):** Role bobini kapatilirken olusan ters EMF'i kisa devre eder, BC337'yi ve devreyi korur.

### 4.10 Heating Rolesi (GPIO 22)

Cooling ile birebir ayni topoloji:

```
GPIO 22 ---> 1N4007 ---> 1k ---> BC337 base
[same as 4.9]

JRC-19F COM ---> 12V (+)
JRC-19F NO  ---> 22ohm 5W ---> 12V GND
```

22ohm 5W direnc isitma elemanidir. 12V/22ohm = 0.55A, P = 12 * 0.55 = 6.6W (5W direncin sinirinda; gerekirse 10W versiyon).

### 4.11 12V Guc Korumasi

```
12V adaptor (+) ---> [Cam sigorta 1.5A] ---+--- 12V hat (yuklere)
                                            |
                                            +--- TVS 15V ---> 12V GND
```

- **Cam sigorta:** Asiri akim korumasi (kisa devre, motor takiklasmasi)
- **TVS 15V:** Gecici asiri gerilimi (motor anahtarlama spike, ESD, indukleme) absorbe eder. Cift yonlu TVS polarite serbest, tek yonlu TVS'de katot 12V (+) tarafinda olmali.

### 4.12 Buck Converter (12V -> 5V)

```
12V (+)   ---> XL4015 VIN
12V GND   ---> XL4015 GNDIN

XL4015 VOUT   ---> 5V hat (ESP32 VIN, sensorler, op-amp, role bobinleri)
XL4015 GNDOUT ---> Sistem GND (ESP32 GND, sensor GND'leri)
```

> **Onemli:** XL4015'in giris ve cikis GND'leri kart icinde birbirine bagli, ortak. 12V GND ile sistem GND arasindaki tek elektriksel kopru shunt direnci uzerindendir. Bu sayede shunt akimi olcebilir.

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
2. Multimetre ile cikisi olc -> 5.0V olmali
3. **ESP32 baglamadan once voltaji dogrula!** 12V ESP32'ye giderse yakar.

### 2. DHT11 Testi

1. DHT11 modulu 3.3V'a bagla (3 kablo: VCC, GND, DATA)
2. Serial Monitor'da sicaklik/nem degerlerini kontrol et
3. `DHT okuma hatasi!` mesaji geliyorsa kablolari kontrol et

### 3. PIR Testi

1. PIR'i 5V'a bagla (buck converter)
2. Serial Monitor'da hareket durumunu izle
3. Onunde hareket et -> `Hareket: VAR` olmali
4. Surekli HIGH kaliyorsa PIR uzerindeki hassasiyet pot'unu ayarla

### 4. MQ-135 Testi

1. MQ-135 cipi A1/A2/H1 = 5V, H2 = GND, B1=B2 -> 10k -> GPIO35 -> 15k -> GND
2. **1-2 dakika isinmasini bekle**
3. Normal hava: 50-150 ppm
4. Ufleme veya cozucu -> deger artmali

### 5. LDR Testi

1. Voltaj bolucu devresini kur (3.3V -> LDR -> GPIO34 -> 10k -> GND)
2. Serial Monitor'da lux degerini izle
3. Eli ile kapat -> deger dusmeli
4. Isik tut -> deger artmali

### 6. Reed Switch Testi

1. Reed switch'in iki ucu: biri GPIO 26, digeri GND
2. Miknatis yakin: pencere kapali (GPIO LOW)
3. Miknatis uzak: pencere acik (GPIO HIGH)
4. Multimetre ile reed kontagini dogrudan kontrol etmek mumkun

### 7. Cooling Rolesi Testi

1. Cooling devresini kur (1N4007 + 1k + BC337 + JRC-19F + 1N4007 flyback + DC fan)
2. MQTT'den manuel komut gonder:
   ```bash
   mosquitto_pub -h localhost -u esp32 -P "$MQTT_PASS" \
     -t "akilli-sinif/sinif-1/control/cooling" \
     -m '{"state": "on"}'
   ```
3. Role klick sesi gelmeli, fan donmeli
4. Multimetre ile JRC-19F kontaginda 12V olcmeli (acikken)

### 8. Heating Rolesi Testi

1. Heating devresini kur (cooling ile ayni topoloji + 22ohm 5W)
2. MQTT komutu gonder (`/control/heating`)
3. Direnc isinmali (5W gucte parmakla dokunma — yanar)
4. Akim sensoru ~0.55A okumali (testte de dogrulama)

### 9. MOSFET + Strip LED Testi

1. PC817 + IRLZ44N + strip LED devresini kur
2. Indikator LED de baglanmis olmali (GPIO 13'ten paralel)
3. MQTT komutu gonder:
   ```bash
   mosquitto_pub -h localhost -u esp32 -P "$MQTT_PASS" \
     -t "akilli-sinif/sinif-1/control/led" \
     -m '{"brightness": 50}'
   ```
4. Hem indikator LED hem strip LED %50 parlamali

### 10. Akim Sensoru Kalibrasyonu

1. MCP6002 + shunt + zener clamp devresini kur
2. Bilinen yukleri tek tek bagla:
   - Sadece ESP32 + sensorler: ~0.15A baseline
   - + Cooling fan: +0.30A (~0.45A toplam)
   - + Heating direnci: +0.55A (~1.00A toplam)
   - + Strip LED tam parlaklik: +0.40A (~1.40A toplam, sigortaya yakin!)
3. Multimetre ile gercek akimi olc (DC moda al, 12V hat dahil)
4. ADC degerlerini Serial Monitor'dan kaydet
5. Piecewise-linear kalibrasyon tablosunu firmware'e gomle

### 11. TVS / Sigorta Testi

1. **TVS:** Normal calisma sirasinda TVS sicak olmamali. Surekli iletimdeyse yanlis TVS degeri (15V'tan dusuk) — TVS'yi 18V veya 20V ile degistir.
2. **Sigorta:** Test icin 1.5A ustu yuk olustur (kisa sureli iki guc yuku ayni anda) — sigorta atmali. Atmiyorsa sigorta degeri yanlis (yuksek deger).

### 12. TFT Ekran Testi

ESP32'yi baslat, ekranda sirasiyla:
1. "AKILLI SINIF SISTEMI / Baslatiliyor..."
2. "WiFi Kurulum / Akilli-Sinif-Setup agina baglan" (veya WiFi OK!)
3. "MQTT Baglaniyor..."
4. Ana ekran (kisi sayisi, sicaklik, nem, hava kalitesi)

---

## Guvenlik Notlari

1. **Flyback diyot:** Her role icin MUTLAKA flyback 1N4007 kullan. Yoksa BC337 yanar.
2. **Base koruma diyodu:** GPIO ile BC337 base arasinda 1N4007 (anot GPIO, katot 1k tarafinda). Transistor C-B short arizasinda 5V'un ESP32'ye gelmesini engeller.
3. **Ortak GND:** Sistem GND tum sensor/ESP32/buck VOUT GND'lerini birlestirir. 12V GND ile sistem GND arasindaki TEK kopru shunt direnci.
4. **Shunt'i atlama:** Akim sensorunu test ederken shunt'i kopruleme — devre yanar, akim olcumu kaybolur.
5. **Buck converter:** ESP32 baglamadan once voltaji multimetre ile kontrol et (5V).
6. **MQ-135:** Isinma suresi bekle (ilk 1-2 dakika yanlis deger verir).
7. **Kisa devre:** 12V hatti ESP32'nin 3.3V/5V girisleriyle temas etmemeli.
8. **Strip LED 12V hatti:** MOSFET drain'e veya LED kablolarina dokunmadan once 12V besleyiyi kapat.
9. **Cam sigorta:** 1.5A degerini asma; toplam yuk 1.5A'ya yaklasiyorsa sigorta degerini ve TVS degerini gozden gecir.
10. **TVS polaritesi:** Tek yonlu TVS'de katot 12V (+) tarafinda. Cift yonlu kullaniyorsan polarite serbest.
11. **PC817 izolasyonu:** Teorik olarak ESP32 GND ile 12V GND ayrilabilir. Pratikte ortak GND kullaniliyor (akim sensoru icin gerekli).
12. **22ohm heater direnci:** 5W modelin sinirinda calisir (6.6W). Kisa surelerde sorun olmaz; surekli isitma icin 10W direnc tercih edilebilir.
