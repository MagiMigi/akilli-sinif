# Grafana Dashboards

## Dosyalar

- `akilli-sinif.json` — v1, ilk surum dashboard
- `akilli-sinif-v2.json` — guncel, soguma/isitma role gostergeleri eklendi

## Coklu Sinif (classroom degiskeni)

Her iki dashboard da ust kisimda **`Sınıf`** dropdown'u (Grafana template degiskeni `classroom`) gosterir. Dropdown, InfluxDB'deki `classroom` tag degerlerini (`sinif-1`, `sinif-2`, ...) otomatik listeler ve tum paneller secili sinifa gore filtrelenir.

- Degisken sorgusu: `schema.tagValues(bucket: "sinif_data", tag: "classroom")`
- Tum panel sorgulari `|> filter(fn: (r) => r.classroom == "${classroom}")` satirini icerir.
- Yeni bir sinif eklendiginde (yeni `sinif_id` ile veri akinca) dropdown'da kendiliginden gorunur, panel eklemeye gerek yok.

> **Not:** `device_status` paneli (ESP32 Durumu / WiFi / Uptime) icin `classroom`'un InfluxDB'de **tag** olmasi gerekir. Bunu saglayan Node-RED `Status Format` fonksiyonu `flows-v3.json` icinde guncellendi — eski kayitlarda `classroom` field oldugundan filtrelenmez, flow'u yeniden deploy ettikten sonraki veriler dogru calisir.

## Onemli: Datasource UID

Iki dashboard da InfluxDB datasource'u `ffgfokah0gqv4c` UID'siyle aramaktadir. Grafana'ya **import** sirasinda:

1. Datasource'u once kurun: `Configuration > Data sources > Add > InfluxDB`
2. Dashboard import dialog'unda UID alani goruntulenecek. Sisteminizdeki gercek InfluxDB datasource UID'sini secin. Aksi halde paneller "datasource not found" verir.
3. Eger varsayilan datasource UID'niz farkliysa, JSON icindeki `"uid": "ffgfokah0gqv4c"` referanslarini toplu degistirin.

## Provisioning (otomatik)

Grafana provisioning ile `datasources.yml`'de InfluxDB'ye sabit bir UID atayip dashboard JSON ile birebir eslestirmek tercih edilir. Ornek:

```yaml
# /etc/grafana/provisioning/datasources/influxdb.yml
apiVersion: 1
datasources:
  - name: InfluxDB-AkilliSinif
    type: influxdb
    uid: ffgfokah0gqv4c      # dashboard JSON ile ayni
    access: proxy
    url: http://localhost:8086
    jsonData:
      version: Flux
      organization: AkilliSinif
      defaultBucket: sinif_data
    secureJsonData:
      token: <INFLUX_TOKEN>
```

Bu sayede `akilli-start.sh` ile sistem yeniden ayaga kalktiginda dashboard'lar otomatik calisir.
