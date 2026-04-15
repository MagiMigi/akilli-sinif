/*
 * Akilli Sinif Sistemi - Firmware Versiyon Tanımları
 * ====================================================
 *
 * Bu dosya GitHub Actions tarafından otomatik güncellenir.
 * Elle düzenleme yapma — git tag push'layınca CI otomatik halleder.
 *
 * Versiyon örnekleri:
 *   v1.0.0  → Major.Minor.Patch (SemVer)
 */

#ifndef VERSION_H
#define VERSION_H

#define FIRMWARE_VERSION_MAJOR 1
#define FIRMWARE_VERSION_MINOR 0
#define FIRMWARE_VERSION_PATCH 0

// Tam versiyon string'i — MQTT status mesajlarında kullanılır
#define FIRMWARE_VERSION "1.0.0"

// GitHub Actions tarafından doldurulur (release tag'ı)
// Manuel build'de __DATE__ ve __TIME__ kullanılır
#define BUILD_DATE __DATE__
#define BUILD_TIME __TIME__

#endif // VERSION_H
