# YOLO HTTP API

ESP32-CAM tarafindan kullanilan HTTP arayuzunun referansi. Implementasyon: `server/ai-processing/yolo_server.py`.

## Base URL

```
http://<server-ip>:5000
```

`YOLO_TLS_CERT` + `YOLO_TLS_KEY` env degiskenleri set ise HTTPS, aksi halde duz HTTP.

## HTTPS (TLS) kurulumu

Duz HTTP'de API key + tum goruntuler agda acik gider. Paylasimli WiFi veya
internete acik deploy'da TLS sart.

1. **Cert uret** (sunucu IP'siyle):
   ```
   bash server/mosquitto/gen-certs.sh 192.168.1.10
   ```
   → `server/mosquitto/certs/{ca.crt, server.crt, server.key}`
2. **Server:** `server/ai-processing/.env` icindeki `YOLO_TLS_CERT` /
   `YOLO_TLS_KEY` satirlarini uncomment + path'leri duzelt, sonra restart.
   Log `https://0.0.0.0:5000` yazmali. Test: `curl -k https://<IP>:5000/`
3. **ESP32-CAM:** WiFiManager portal / MQTT config'te `server_url`'i
   `https://<IP>:5000/analyze` yap.
4. **CA pinning (onerilen):** `firmware/secrets.h` icindeki `YOLO_CA_CERT`'e
   `ca.crt` iceriligini yapistir + reflash. Bos birakirsan firmware
   `setInsecure()` kullanir (sifreler ama MITM korumasiz).

> **Cert omru:** self-signed 1 yil (`gen-certs.sh` DAYS=365). Suresi dolunca
> hem server cert'i hem firmware'deki CA yenilenir → **cihazlar reflash gerekir.**

## Kimlik Dogrulama

API key ile. Iki yontemden biri:

- Header: `X-API-Key: <key>`
- Query: `?api_key=<key>` (dev/test icin)

Key sunucu tarafinda `API_KEY` env'sinden okunur. Minimum 16 karakter. Karsilastirma `hmac.compare_digest` ile timing-attack guvenli.

## Endpoint'ler

### `GET /`

Servis bilgisi (anonim ya da auth'lu — implementasyona gore degisir). Health-check icin kullanilir.

### `GET /status`

Sunucu durum ozet'i: model yuklu mu, MQTT bagli mi, uptime, son N analiz.

### `POST /analyze`  (auth gerekli)

ESP32-CAM JPEG yukler, person count + bounding box doner.

**Headers:**
- `Content-Type: image/jpeg`
- `X-API-Key: <key>`
- `X-Classroom-ID: <id>`  (whitelist: `^[A-Za-z0-9_-]{1,32}$`, default `sinif-1`)

**Body:** JPEG byte stream. Maksimum 8 MB (Flask `MAX_CONTENT_LENGTH`).

**Yanit (200):**
```json
{
  "success": true,
  "person_count": 12,
  "detections": [
    {"bbox": [x, y, w, h], "confidence": 0.87, "class": "person"}
  ],
  "classroom_id": "sinif-1",
  "timestamp": 1716000000000
}
```

**Yanit (400):** bos body veya gecersiz content-type.
**Yanit (401/403):** API key eksik veya yanlis.
**Yanit (500):** islem hatasi (model yok, decode hatasi).

**Yan etki:** Sunucu, ayni sonucu MQTT `akilli-sinif/{classroom_id}/sensors/camera` topic'ine `{"person_count": N, "timestamp": ts}` olarak publish eder.

### `GET /test`  (auth gerekli)

Sunucu lokal webcam'den (cv2.VideoCapture(0)) bir frame alip analyze prosedurunu calistirir. Sadece geliscime/test icin.

### `GET /count/<classroom_id>`

Belirli sinifin son okunan person_count degeri (cache).

## Hatalar

Tum hata yanitlari JSON:
```json
{"success": false, "error": "aciklama"}
```

## Rate Limiting

Su an yok. Yuksek frekansli istemciler icin Flask-Limiter eklenebilir (ileride).
