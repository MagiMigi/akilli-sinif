# Grafana Dashboards

## Dosyalar

- `akilli-sinif.json` — v1, ilk surum dashboard
- `akilli-sinif-v2.json` — guncel, soguma/isitma role gostergeleri eklendi

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
