#!/usr/bin/env bash
# Akilli Sinif MQTT smoke test
# On kosul: mosquitto-clients (mosquitto_pub, mosquitto_sub) ve calisan broker.

set -euo pipefail

BROKER="${MQTT_BROKER:-localhost}"
PORT="${MQTT_PORT:-1883}"
USER="${MQTT_USER:-esp32}"
PASS="${MQTT_PASS:-akilli321}"
TOPIC="akilli-sinif/smoke-test/sensors/temperature"
PAYLOAD='{"value": 22.5, "unit": "C", "timestamp": 0}'
TIMEOUT=3

echo "[smoke] Broker: $BROKER:$PORT"

# Subscribe arka planda, ilk mesajda cik
TMPFILE=$(mktemp)
trap 'rm -f "$TMPFILE"' EXIT

mosquitto_sub -h "$BROKER" -p "$PORT" -u "$USER" -P "$PASS" \
  -t "$TOPIC" -C 1 -W "$TIMEOUT" > "$TMPFILE" &
SUB_PID=$!

sleep 0.5

mosquitto_pub -h "$BROKER" -p "$PORT" -u "$USER" -P "$PASS" \
  -t "$TOPIC" -m "$PAYLOAD"

wait "$SUB_PID" || { echo "[smoke] FAIL: subscribe timeout/error"; exit 1; }

RECEIVED=$(cat "$TMPFILE")
if [[ "$RECEIVED" != "$PAYLOAD" ]]; then
  echo "[smoke] FAIL: payload mismatch"
  echo "  expected: $PAYLOAD"
  echo "  received: $RECEIVED"
  exit 1
fi

echo "[smoke] OK: publish/subscribe round-trip basarili"
