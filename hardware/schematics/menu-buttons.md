# Menu Butonlari — Kablo Baglantilari

TFT menu sayfalari arasinda gezinmek icin iki tactile push-button eklenir.
Hicbir harici direnc gerekmez (ESP32 dahili pullup kullanir).

## Pin Haritasi

| Buton  | ESP32 Pin | Islev                                     |
|--------|-----------|-------------------------------------------|
| NEXT   | GPIO 25   | Sonraki sayfa (kisa basis)                |
| NEXT   | GPIO 25   | Uzun basis (>=3 sn) → otomatik cevrimi duraklat / devam ettir |
| PREV   | GPIO 14   | Onceki sayfa (kisa basis)                 |

GPIO 25 ve GPIO 14 strapping pini degildir; baska bir cevre birim kullanmaz
(TFT SPI: 15/18/23/32/33, DHT: 4, LDR: 34, MQ-135: 35, PIR: 27, Reed: 26,
LED: 13, FAN: 12, BOOT reset: 0).

## Kablolama

Her iki buton da ESP32'ye ayni sekilde baglanir:

```
  ESP32 GPIO 25 ────┐
                    ├──[ NEXT BTN ]── GND
  (dahili pullup)   

  ESP32 GPIO 14 ────┐
                    ├──[ PREV BTN ]── GND
  (dahili pullup)   
```

- Butonun bir bacagi ESP32 pinine, diger bacagi GND'ye bagli.
- Firmware `pinMode(..., INPUT_PULLUP)` kullanir → bosta HIGH, basinca LOW.
- Onerilen buton: 6x6 mm tactile (breadboard dostu), ornek Omron B3F serisi.

## Yazilim Davranisi

- Debounce: 50 ms
- Uzun basis esigi: 3000 ms (sadece NEXT butonunda aktif)
- Otomatik sayfa cevrimi: 6 saniye (config.json → `display.auto_rotate_ms`)
- Buton basinca cevrim sayaci sifirlanir; uzun basista duraklatilir
- BOOT butonu (GPIO 0) sadece config sifirlama icin kullanilir — kisa basis tepki vermez
