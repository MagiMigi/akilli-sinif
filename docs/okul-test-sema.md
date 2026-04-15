# Okul Testi - Breadboard Devre Semasi

Bu dokuman 12V DC kaynagi ile test edilecek bilesenlerin baglanti semasini icerir.

## Test Edilecek Bilesenler

| Bilesen | Besleme | Test Durumu |
|---------|---------|-------------|
| PIR (HC-SR501) | 5V | Bekliyor |
| MQ-135 | 5V | Bekliyor |
| DC Fan + MOSFET | 12V | Bekliyor |
| Serit LED + MOSFET | 12V | Bekliyor |
| Buck Converter (12V->5V) | 12V giris | Bekliyor |

## Guc Dagilimi

```
12V DC Kaynagi
      |
      +-------------------> Serit LED (MOSFET ile)
      |
      +-------------------> DC Fan (MOSFET ile)
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

## MOSFET Surucu Devresi (PC817 + IRLZ44N)

Her kanal icin (LED ve Fan):

```
                                         +12V
                                           |
              +----------------------------+
              |                            |
            10K (pull-up)                LOAD
              |                       (Fan veya LED)
              |                            |
         PC817 Collector -------- IRLZ44N Gate
              |                            |
         PC817 Emitter --------- IRLZ44N Source --- GND
                                           |
                                    10K (pull-down)
                                           |
                                          GND

ESP32 Tarafi:
GPIO --- 1K --- PC817 Anot
                   |
               PC817 Katot --- GND (ESP32)
```

### LED Kanali (GPIO16)
- ESP32 GPIO16 -> 1K direnc -> PC817 Anot
- PC817 Katot -> ESP32 GND
- PC817 Collector -> 10K -> +12V
- PC817 Collector -> IRLZ44N Gate
- PC817 Emitter -> GND (12V tarafi)
- IRLZ44N Gate -> 10K -> GND (pull-down)
- IRLZ44N Drain -> Serit LED (-) ucu
- IRLZ44N Source -> GND
- Serit LED (+) -> +12V

### Fan Kanali (GPIO17)
- ESP32 GPIO17 -> 1K direnc -> PC817 Anot
- PC817 Katot -> ESP32 GND
- PC817 Collector -> 10K -> +12V
- PC817 Collector -> IRLZ44N Gate
- PC817 Emitter -> GND (12V tarafi)
- IRLZ44N Gate -> 10K -> GND (pull-down)
- IRLZ44N Drain -> DC Fan (-) ucu
- IRLZ44N Source -> GND
- DC Fan (+) -> +12V
- **1N4007 flyback diyot**: Katot -> +12V, Anot -> IRLZ44N Drain (Fan'a paralel)

## Sensor Baglantilari

### PIR (HC-SR501)
```
PIR VCC  ----> Buck Converter 5V Cikisi
PIR GND  ----> Ortak GND
PIR OUT  ----> ESP32 GPIO27
```

### MQ-135
```
MQ-135 VCC  ----> Buck Converter 5V Cikisi
MQ-135 GND  ----> Ortak GND
MQ-135 AOUT ----> ESP32 GPIO35
```
**NOT**: MQ-135 isinmasi icin 1-2 dakika bekle, sonra degerler stabilize olur.

## Pin Ozeti Tablosu

| Bilesen | ESP32 Pin | Baglanti |
|---------|-----------|----------|
| DHT11 DATA | GPIO4 | +10K pull-up (3.3V'a) |
| LDR | GPIO34 | Voltaj bolucu (pull-down) |
| MQ-135 AOUT | GPIO35 | Analog giris |
| PIR OUT | GPIO27 | Dijital giris |
| Reed Switch | GPIO26 | Pull-up (dahili) |
| LED MOSFET | GPIO16 | PC817 ile izole |
| Fan MOSFET | GPIO17 | PC817 ile izole |
| TFT CS | GPIO15 | SPI |
| TFT DC | GPIO33 | SPI |
| TFT RST | GPIO32 | SPI |
| TFT MOSI | GPIO23 | SPI |
| TFT SCK | GPIO18 | SPI |

## Malzeme Listesi (Okulda Gerekli)

| Malzeme | Adet | Kullanim |
|---------|------|----------|
| IRLZ44N MOSFET | 2 | LED ve Fan kontrolu |
| PC817 Optocoupler | 2 | Izolasyon |
| 1K direnc | 2 | PC817 LED akimi |
| 10K direnc | 4 | 2x pull-up, 2x pull-down |
| 1N4007 diyot | 1 | Fan flyback korumasi |
| Buck Converter (5A) | 1 | 12V -> 5V |
| Breadboard | 1 | Baglanti |
| Jumper kablolar | ~20 | Baglanti |

## Test Proseduru

### 1. Buck Converter Testi
1. 12V DC kaynagini buck converter girisine bagla
2. Multimetre ile cikisi olc -> 5.0V olmali
3. ESP32'yi baglamadan once voltaji dogrula!

### 2. PIR Testi
1. PIR'i 5V'a bagla
2. Serial Monitor'da "Hareket: VAR/YOK" kontrol et
3. Onunde hareket et -> "Hareket: VAR" olmali

### 3. MQ-135 Testi
1. MQ-135'i 5V'a bagla
2. 1-2 dakika isinmasini bekle
3. Serial Monitor'da hava kalitesi degeri -> 50-150 ppm arasi normal
4. Ustle (veya cozmeli kalemle) -> deger artmali

### 4. MOSFET LED Testi
1. MOSFET devresini kur (PC817 + IRLZ44N + direncler)
2. Serit LED'i bagla
3. MQTT'den komut gonder:
   ```bash
   mosquitto_pub -h localhost -u esp32 -P akilli123 \
     -t "akilli-sinif/sinif-1/control/led" \
     -m '{"brightness": 50}'
   ```
4. LED %50 parlamali

### 5. MOSFET Fan Testi
1. Fan MOSFET devresini kur (flyback diyot unutma!)
2. DC Fan'i bagla
3. MQTT'den komut gonder:
   ```bash
   mosquitto_pub -h localhost -u esp32 -P akilli123 \
     -t "akilli-sinif/sinif-1/control/fan" \
     -m '{"speed": 50}'
   ```
4. Fan %50 hizda donmeli

## Guvenlik Notlari

1. **Flyback diyot**: Fan icin MUTLAKA kullan, yoksa MOSFET yanabilir
2. **Ortak GND**: Tum GND'leri birlestir
3. **Buck converter**: ESP32 baglamadan once voltaji kontrol et
4. **MQ-135**: Isinma suresi bekle (yanlis okumalar olabilir)
5. **Kisa devre**: Baglantilari kontrol et, 12V ESP32'ye giderse yakar!

## Sorun Giderme

| Sorun | Olasi Neden | Cozum |
|-------|-------------|-------|
| PIR surekli HIGH | Hassasiyet yuksek | PIR uzerindeki pot'u ayarla |
| MQ-135 0 ppm | Isinmamis veya baglanti | 2 dk bekle, kabloları kontrol et |
| MOSFET calismiyor | Pull-up/down eksik | 10K direnc ekle |
| Fan donmuyor | Flyback diyot ters | Diyot yonunu kontrol et |
| LED yanmiyor | 12V yok veya MOSFET | Voltaj ve gate sinyalini olc |
