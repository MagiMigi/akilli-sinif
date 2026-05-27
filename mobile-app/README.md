# Akıllı Sınıf — Mobil Uygulama

Android için profesyonel, modern ve sade Expo + React Native uygulaması.
ESP32 cihazlarına Mosquitto MQTT-over-WebSocket üzerinden doğrudan bağlanır.

## Özellikler

- Sınıf listesi (otomatik tespit, retained `status/connection`'dan)
- Canlı sensör kartları: sıcaklık, nem, ışık, hava kalitesi, kişi, pencere
- Çizgi grafik (son 120 örnek, RAM içi ring buffer)
- LED slider kontrolü (PWM 0-100) + soğutma/ısıtma röle toggle butonları (anlık MQTT publish)
- Uyarı gönderme (info / warning / danger) — TFT ekranda gösterilir
- OTA güncelleme — GitHub Releases listesi, tek cihaz veya broadcast
- Config sıfırlama (cihazı portal moduna alma)
- Bildirim kanalı (FCM credential sonra eklenebilir)
- Dark + teal tema, Türkçe arayüz

## Önkoşullar

- Node.js 18+
- Mosquitto WebSocket listener — `server/mosquitto/mosquitto.conf` içinde port `9001` zaten açık.
- Mosquitto'da `mobile` kullanıcısı:
  ```bash
  sudo mosquitto_passwd /etc/mosquitto/passwd mobile
  # ACL'de (/etc/mosquitto/acl) "mobile" kuralları repo'da hazır
  sudo systemctl restart mosquitto
  ```

## Geliştirme

```bash
cd mobile-app
npm install
npx expo start
```

Telefonunda **Expo Go** uygulamasını aç → terminaldeki QR'i okut.
İlk açılışta Setup ekranı gelir. Broker IP, port (9001), kullanıcı (mobile),
şifre gir → Bağlan.

## Production APK

İki seçenek:

### Yöntem A — EAS Build (cloud)

```bash
npm install -g eas-cli
eas login
eas build -p android --profile preview
```

Çıktı: APK indirme linki.

### Yöntem B — Yerel Gradle build

```bash
npx expo prebuild --platform android
cd android && ./gradlew assembleRelease
# çıktı: android/app/build/outputs/apk/release/app-release.apk
```

## Mimari

```
src/
├── App.tsx                  # Polyfill + NavigationContainer
├── theme/                   # Dark+teal palette, spacing, typography
├── lib/
│   ├── types.ts             # Sensor/Status/Command shape'leri
│   ├── topics.ts            # MQTT topic builder/parser
│   ├── parsers.ts           # JSON → strongly-typed reading
│   ├── storage.ts           # SecureStore + AsyncStorage
│   └── mqtt.ts              # mqtt.js singleton (ws/wss)
├── store/
│   ├── classroomStore.ts    # zustand: classroom map + ring buffer
│   └── wireMqtt.ts          # MQTT events → store
├── navigation/
│   └── RootNavigator.tsx    # Setup → Tabs(Dashboard, Settings)
├── screens/
│   ├── SetupScreen.tsx
│   ├── DashboardScreen.tsx
│   ├── ClassroomScreen.tsx
│   ├── ControlScreen.tsx
│   ├── AlertScreen.tsx
│   ├── OtaScreen.tsx
│   ├── ResetScreen.tsx
│   └── SettingsScreen.tsx
└── components/              # SensorCard, ClassroomCard, Slider, LineChart …
```

## Asset (logo / splash)

`app.json` şu an Expo default ikon ve splash'ı kullanır. Kendi logonu eklemek için:

1. `mobile-app/assets/` klasörü oluştur
2. İçine `icon.png` (1024x1024), `adaptive-icon.png` (1024x1024 transparan), `splash.png` koy
3. `app.json`'a path'leri ekle:
   ```json
   "icon": "./assets/icon.png",
   "splash": { "image": "./assets/splash.png", ... },
   "android": { "adaptiveIcon": { "foregroundImage": "./assets/adaptive-icon.png", ... } }
   ```

## Sorun Giderme

| Sorun | Çözüm |
|-------|-------|
| "Bağlantı kurulamadı" | Telefon ile broker aynı LAN'da mı? `mosquitto_sub -p 9001` ile WS test et. |
| Sensör verisi gelmiyor | ACL'de `mobile` kullanıcısının `read sensors/#` izni var mı? |
| Komut "MQTT bağlı değil" | Status pill yeşil mi? Setup → Ayarlar üzerinden tekrar bağlan. |
| GitHub OTA listesi boş | Repo `MagiMigi/akilli-sinif` ve internet bağlantısı kontrol et. |

## Lisans

MIT — kök repo lisansı geçerli.
