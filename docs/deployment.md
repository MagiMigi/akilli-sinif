# Deployment

Bu doküman, akilli-sinif sistemini bir sunucu makinesine kurmak için pratik bir kontrol listesidir. Geliştirme makinesindeki tek-script `akilli-start.sh` yerine, sürdürülebilir bir kurulum için systemd unit'leri ve provisioning kullanmak önerilir.

## On Kosullar

- Arch Linux / Ubuntu / Raspberry Pi OS
- Python 3.11+
- Node.js 18+ (Node-RED için)
- Sudo erişimi (servisleri kayıt etmek için)

## Servisler

| Servis | Port | Kurulum |
|---|---|---|
| Mosquitto | 1883 (MQTT), 9001 (WS) | `pacman -S mosquitto` veya `apt install mosquitto` |
| InfluxDB v2 | 8086 | resmi paket veya docker |
| Node-RED | 1880 | `npm install -g --unsafe-perm node-red` |
| Grafana | 3000 | resmi paket |
| YOLO Flask | 5000 | `server/ai-processing/` icinde venv |

## Mosquitto

```bash
sudo cp server/mosquitto/mosquitto.conf /etc/mosquitto/mosquitto.conf
sudo cp server/mosquitto/acl /etc/mosquitto/acl

# Parola dosyasi olustur (sadece ilk kurulumda)
sudo mosquitto_passwd -c /etc/mosquitto/passwd esp32
sudo mosquitto_passwd     /etc/mosquitto/passwd nodered
sudo mosquitto_passwd     /etc/mosquitto/passwd mobile

sudo systemctl enable --now mosquitto
```

TLS opsiyonel — `server/mosquitto/gen-certs.sh` self-signed cert üretir, `mosquitto.conf` içindeki TLS bloğunu açın. ESP32 firmware ve mobil istemci tarafında da CA güveni gerekir.

## InfluxDB

İlk açılışta `http://localhost:8086` üzerinden org `AkilliSinif`, bucket `sinif_data` oluştur. Token üret. Token'ı:

```bash
# server/influxdb/credentials.env (gitignored)
INFLUXDB_URL=http://localhost:8086
INFLUXDB_ORG=AkilliSinif
INFLUXDB_BUCKET=sinif_data
INFLUXDB_TOKEN=<token>
```

## Node-RED

```bash
mkdir -p ~/.node-red
cp server/node-red/flows-v3.json ~/.node-red/flows.json
cp server/node-red/credentials.env ~/.node-red/  # if needed

# Gerekli node paketleri
cd ~/.node-red
npm install node-red-contrib-influxdb node-red-dashboard

sudo systemctl enable --now nodered  # paket sundu ise
```

## YOLO Server

```bash
cd server/ai-processing
python -m venv venv
source venv/bin/activate
pip install -r requirements.txt
cp .env.example .env  # API_KEY, MQTT_BROKER, MQTT_USER, MQTT_PASS doldur
```

systemd unit (`/etc/systemd/system/yolo-server.service`):

```ini
[Unit]
Description=Akilli Sinif YOLO Server
After=network.target mosquitto.service

[Service]
Type=simple
User=akilli
WorkingDirectory=/home/akilli/akilli-sinif/server/ai-processing
EnvironmentFile=/home/akilli/akilli-sinif/server/ai-processing/.env
ExecStart=/home/akilli/akilli-sinif/server/ai-processing/venv/bin/python yolo_server.py
Restart=on-failure

[Install]
WantedBy=multi-user.target
```

```bash
sudo systemctl daemon-reload
sudo systemctl enable --now yolo-server
```

## Grafana

```bash
sudo systemctl enable --now grafana
```

`http://localhost:3000` (admin/admin) → Data Sources → InfluxDB, datasource UID'sini `ffgfokah0gqv4c` yapın (provisioning örneği için `server/grafana/dashboards/README.md`). Dashboards → Import: `akilli-sinif-v2.json`.

## Saglik Kontrol

`akilli-start.sh` ile manuel olarak:

```bash
./akilli-start.sh
```

her servisin TCP portunu bekler; cikti `✓` (hazir) veya `!` (zaman asimi) gosterir.

## Firewall

LAN-only kurulum oneren default:
- 1883 (MQTT) — sadece LAN
- 9001 (MQTT WS) — sadece LAN
- 3000 (Grafana) — yetkili kullanicilar
- 5000 (YOLO) — sadece ESP32-CAM ve LAN
- 8086 (Influx) — sadece Node-RED + Grafana

İnternete açılacaksa hepsinin önüne reverse proxy + TLS + auth koy.
