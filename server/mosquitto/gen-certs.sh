#!/usr/bin/env bash
# =============================================================================
# Akilli Sinif — Mosquitto self-signed cert üreteci
# =============================================================================
# LAN dışı (internet'e açık) deploy için TLS şart. Bu script yerel bir
# CA + server cert üretir ve `./certs/` altına yazar.
#
#   bash gen-certs.sh [HOST]
#
# HOST set değilse `192.168.1.10` kullanılır. SAN listesinde host + localhost +
# `hostname` çıktısı bulunur — istemciler bu adreslerden TLS doğrular.
#
# Kullanım sonrası:
#   sudo cp certs/{ca.crt,server.crt,server.key} /etc/mosquitto/certs/
#   sudo chown mosquitto:mosquitto /etc/mosquitto/certs/*
#   sudo chmod 640 /etc/mosquitto/certs/server.key
#   # mosquitto.conf'taki TLS bloğunu uncomment et, sonra:
#   sudo systemctl restart mosquitto
#
# Cert 1 yıl geçerli; süresi dolarsa scripti tekrar çalıştır + restart.
# =============================================================================
set -euo pipefail

HOST="${1:-192.168.1.10}"
DAYS=365
OUT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/certs"

mkdir -p "$OUT_DIR"
cd "$OUT_DIR"

echo "[+] CA üretiliyor..."
openssl genrsa -out ca.key 2048 2>/dev/null
openssl req -x509 -new -nodes -key ca.key -sha256 -days "$DAYS" \
  -subj "/CN=Akilli-Sinif-CA" -out ca.crt

echo "[+] Server cert (CN=$HOST) üretiliyor..."
openssl genrsa -out server.key 2048 2>/dev/null

# HOST IPv4 mi yoksa hostname mi? IPv4 ise SAN'da IP, degilse DNS olarak ekle.
if [[ "$HOST" =~ ^[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
  HOST_SAN_LINE="IP.1  = $HOST"
else
  HOST_SAN_LINE="DNS.3 = $HOST"
fi

cat > server.cnf <<EOF
[req]
distinguished_name = req_distinguished_name
req_extensions     = v3_req
prompt             = no

[req_distinguished_name]
CN = $HOST

[v3_req]
subjectAltName = @alt_names

[alt_names]
DNS.1 = $(hostname)
DNS.2 = localhost
$HOST_SAN_LINE
IP.2  = 127.0.0.1
EOF

openssl req -new -key server.key -out server.csr -config server.cnf
openssl x509 -req -in server.csr -CA ca.crt -CAkey ca.key -CAcreateserial \
  -out server.crt -days "$DAYS" -sha256 -extensions v3_req -extfile server.cnf

chmod 600 server.key ca.key

echo
echo "[+] Tamamlandı: $OUT_DIR"
ls -lh "$OUT_DIR"
echo
echo "Doğrulama:"
openssl verify -CAfile ca.crt server.crt
