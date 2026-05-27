# Hardware Schematics

Bu klasor donanim semalarinin git'te tutuldugu yerdir.

## Mevcut Durum

KiCad (`.kicad_sch`, `.kicad_pcb`, `.kicad_pro`) dosyalari **henuz uretilmedi**. Tum donanim tasarimi metin/ASCII referansi olarak [`../../docs/donanim.md`](../../docs/donanim.md) icinde tutuluyor:

- Pin atamalari (GPIO tablosu)
- Akim olcumu (0.1ohm shunt + LM358 inverting amplifier, G=-20x)
- Role suruculeri (BC337 + JRC-19F 5V + 1N4007 flyback)
- LED strip suruculeri (PC817 + IRLZ44N MOSFET, indikator LED paraleli)
- 12V hat korumasi (1.5A sigorta + 15V TVS)
- Sensor baglantilari (DHT11, MQ-135, reed switch, PIR, LDR)

## Menu Butonlari

Fiziksel menu butonlarinin wiring'i ayri belgelenmistir: [`menu-buttons.md`](menu-buttons.md).

## TODO

- ESP32-PLC ana kart KiCad semati ve PCB
- ESP32-CAM modul wiring KiCad semati
- Power-supply seksiyonu (USB-C 5V girisi, 12V hat ayri)

Bitirme jurisine sunum oncesi en azindan **ESP32-PLC ana kart semasinin** KiCad'de cizilip BOM ile birlikte commit'lenmesi onerilir.
