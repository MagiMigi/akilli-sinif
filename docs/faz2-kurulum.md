# FAZ 2 Kurulum Kılavuzu

Bu kılavuz ESP32, Node-RED ve Grafana'nın yapılandırılmasını adım adım açıklar.

## 1. Node-RED Flow Import

### Adım 1: Node-RED'i aç
```
http://localhost:1880
```

### Adım 2: Flow'u Import Et
1. Sağ üst köşedeki **hamburger menüsüne** (☰) tıkla
2. **Import** seçeneğini tıkla
3. **Clipboard** sekmesinde, aşağıdaki dosyanın içeriğini yapıştır:
   ```
   /home/magimigi/school/akilli-sinif/server/node-red/flows.json
   ```
4. **Import** butonuna tıkla

### Adım 3: MQTT Broker Yapılandırması
1. Herhangi bir **MQTT node'una** çift tıkla (mor renkli)
2. **Server** yanındaki kalem ikonuna tıkla
3. **Security** sekmesine git
4. Bilgileri kontrol et:
   - Username: `nodered`
   - Password: `nodered123`
5. **Update** → **Done** → **Deploy**

### Adım 4: InfluxDB Yapılandırması (ÖNEMLİ!)
1. Herhangi bir **InfluxDB out node'una** çift tıkla (mavi-yeşil renkli)
2. **Server** yanındaki kalem ikonuna tıkla
3. Ayarları kontrol et:
   - Version: `2.0`
   - URL: `http://localhost:8086`
   - Token: (aşağıdaki token'ı yapıştır)
   ```
   DiQv3VZMADYLOcS40oIY4_dKmejt5j9ACG_cydVwdQu3linCPtErXm2Q1nEWgudC_ED3isqiiIrmypkze0TGtw==
   ```
4. **Update** → **Done**
5. Sağ üstteki **Deploy** butonuna tıkla

### Adım 5: Bağlantıyı Test Et
1. MQTT node'larının altında **connected** yazmalı
2. Terminalde test mesajı gönder:
   ```bash
   mosquitto_pub -h localhost -u esp32 -P akilli123 \
     -t "akilli-sinif/sinif-1/sensors/temperature" \
     -m '{"value": 25.5, "unit": "C", "timestamp": 1234567890}'
   ```
3. Node-RED debug panelinde mesajı görmelisin

---

## 2. Grafana Dashboard Import

### Adım 1: Grafana'yı aç
```
http://localhost:3000
```
- Kullanıcı: `admin`
- Şifre: `akilli123456`

### Adım 2: Dashboard'u Import Et
1. Sol menüden **Dashboards** → **Import**
2. **Upload JSON file** tıkla
3. Bu dosyayı seç:
   ```
   /home/magimigi/school/akilli-sinif/server/grafana/dashboards/akilli-sinif.json
   ```
4. **Import** tıkla

### Adım 3: InfluxDB Datasource Kontrol
Eğer datasource hatası alırsan:
1. Sol menü → **Connections** → **Data sources**
2. **InfluxDB** tıkla (yoksa **Add data source** → **InfluxDB**)
3. Ayarlar:
   - Query Language: `Flux`
   - URL: `http://localhost:8086`
   - Organization: `AkilliSinif`
   - Token: `DiQv3VZMADYLOcS40oIY4_dKmejt5j9ACG_cydVwdQu3linCPtErXm2Q1nEWgudC_ED3isqiiIrmypkze0TGtw==`
   - Default Bucket: `sinif_data`
4. **Save & test**

---

## 3. ESP32 Programlama

### Adım 1: Arduino IDE'yi aç
```bash
cd ~/Downloads
./arduino-ide_2.3.4_Linux_64bit.AppImage
```

### Adım 2: Kodu aç
```
/home/magimigi/school/akilli-sinif/firmware/esp32-plc/src/main.ino
```

### Adım 3: WiFi Bilgilerini Kontrol Et (satır 29-33)
```cpp
const char* WIFI_SSID = "SUPERONLINE_D065";      // ✓ Senin WiFi'n
const char* WIFI_PASSWORD = "YuJhNd68cH";        // ✓ Senin şifren
const char* MQTT_BROKER = "192.168.1.8";         // ✓ Bilgisayarının IP'si
```

### Adım 4: Board Ayarları
- Board: **ESP32 Dev Module**
- Port: `/dev/ttyUSB0` (veya ESP32'nin bağlı olduğu port)

### Adım 5: Yükle
1. **Upload** butonuna tıkla (→ ok ikonu)
2. Yükleme başlamadan önce ESP32'deki **BOOT** butonuna basılı tut
3. "Connecting..." mesajını görünce bırak

### Adım 6: Serial Monitor'u aç
- Baud rate: `115200`
- Mesajları izle:
  ```
  ╔══════════════════════════════════════╗
  ║     AKILLI SINIF SISTEMI v1.0        ║
  ║          ESP32 PLC Firmware          ║
  ╠══════════════════════════════════════╣
  ║  Sinif: sinif-1                      ║
  ║  Mod: SIMULASYON                     ║
  ╚══════════════════════════════════════╝
  
  WiFi BAGLANDI!
  IP Adresi: 192.168.1.xxx
  
  MQTT BAGLANDI!
  ```

---

## 4. Test Komutları

### MQTT Mesajlarını İzle (Terminalde)
```bash
# Tüm sensör verilerini izle
mosquitto_sub -h localhost -u nodered -P nodered123 -t "akilli-sinif/#" -v
```

### LED Kontrolü (Node-RED veya Terminal)
```bash
# LED'i %50 yap
mosquitto_pub -h localhost -u nodered -P nodered123 \
  -t "akilli-sinif/sinif-1/control/led" \
  -m '{"brightness": 50}'

# LED'i kapat
mosquitto_pub -h localhost -u nodered -P nodered123 \
  -t "akilli-sinif/sinif-1/control/led" \
  -m '{"state": "off"}'
```

### Fan Kontrolü
```bash
# Fan'ı %75 hızda çalıştır
mosquitto_pub -h localhost -u nodered -P nodered123 \
  -t "akilli-sinif/sinif-1/control/fan" \
  -m '{"speed": 75}'
```

---

## 5. Sorun Giderme

### ESP32 WiFi'ye bağlanamıyor
- SSID ve şifre doğru mu kontrol et
- 2.4GHz WiFi kullan (5GHz çalışmaz)
- Modem/router'ı yeniden başlat

### MQTT bağlantı hatası
```bash
# Mosquitto çalışıyor mu?
systemctl status mosquitto

# Manuel test
mosquitto_pub -h localhost -u esp32 -P akilli123 -t "test" -m "hello"
```

### InfluxDB'ye veri yazılmıyor
1. Node-RED'de InfluxDB token'ı doğru mu?
2. InfluxDB çalışıyor mu?
   ```bash
   systemctl status influxdb
   ```
3. Bucket adı doğru mu? (`sinif_data`)

### Grafana'da veri görünmüyor
1. Time range'i kontrol et (Last 5 minutes veya Last 1 hour)
2. Datasource bağlantısını test et
3. Flux query'lerinde bucket ve classroom adları doğru mu?

---

## Bağlantı Özeti

| Servis | Adres | Kullanıcı | Şifre |
|--------|-------|-----------|-------|
| Node-RED | http://localhost:1880 | - | - |
| Grafana | http://localhost:3000 | admin | akilli123456 |
| InfluxDB | http://localhost:8086 | admin | akilli123456 |
| MQTT (ESP32) | localhost:1883 | esp32 | akilli123 |
| MQTT (Node-RED) | localhost:1883 | nodered | nodered123 |

---

## Sonraki Adımlar

FAZ 2 tamamlandıktan sonra:
- **FAZ 3**: ESP32-CAM entegrasyonu ve YOLO ile kişi sayma
- **FAZ 4**: TFT Display (ST7735) ile bilgilendirme paneli
- **FAZ 5**: Gerçek sensör entegrasyonu (DHT22, LDR, MQ135, PIR, Reed Switch)
