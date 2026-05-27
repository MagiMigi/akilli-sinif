# Tests — Akilli Sinif

Entegrasyon ve duman (smoke) testleri. Hedef: MQTT topic kontratlarinin, YOLO HTTP API'sinin ve Node-RED akislarinin temel uctan-uca davranisini sahada hizli dogrulamak.

## Hangi test ne yapar

| Dosya | Kapsam | On kosul |
|---|---|---|
| `smoke.sh` | MQTT broker calisiyor mu, sensor topic'ine publish/subscribe edebiliyor muyuz | `mosquitto-clients` paketi |
| `test_mqtt_topics.py` | `docs/mqtt-topics.md`'deki topic semasiyla bir uctan publish, oteki uctan subscribe doruluyor | Python 3, `paho-mqtt` |
| `test_yolo_api.py` | YOLO Flask API kontrati: `/status` 200 doner, `/analyze` API key + content-type kontrolu yapar | Python 3, `requests`, YOLO server `localhost:5000`'de calisiyor |

## Kurulum

```bash
sudo pacman -S mosquitto             # mosquitto-clients icin (Arch)
python -m venv .venv && source .venv/bin/activate
pip install -r requirements.txt
```

## Calistir

Tum servisler ayakta (Mosquitto + YOLO server, ornegin `akilli-start.sh` ile basladi) iken:

```bash
bash smoke.sh                        # 3 saniyede MQTT akis dogrulamasi
pytest -v                            # tum python testleri
```

Sadece bir test grubu:

```bash
pytest -v test_mqtt_topics.py
pytest -v test_yolo_api.py -k status # /status testini izole calistir
```

## CI'a baglama (sonra)

`pytest` cikti kodu 0/1. GitHub Actions'ta `.github/workflows/test.yml` icine eklenebilir; ama hedef cihaz/broker gerektigi icin self-hosted runner ister.

## Bilinen sinirlar

- Donanim testi yok: ESP32 sensorlerinin gercek okumasi ve role anahtarlama el ile dogrulanir (`docs/donanim.md` test prosedurleri).
- E2E (mobil dahil) testi yok: mobil uygulama icin manuel test plan README'de.
- YOLO model dogrulugu testi yok; sadece API kontrati testi var.
