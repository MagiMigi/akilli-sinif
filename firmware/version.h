/*
 * Akilli Sinif Sistemi - Firmware Versiyon Tanımları
 * ====================================================
 *
 * Bu dosya CI (GitHub Actions) tarafından build sırasında overwrite edilir.
 * Gerçek versiyon kaynağı: firmware/<device>/VERSION dosyaları.
 *
 * Lokal build'lerde aşağıdaki "dev" stub kullanılır — binary'de
 * FIRMWARE_VERSION = "dev" olarak görünür, bu da lokal/CI ayrımını net gösterir.
 */

#ifndef VERSION_H
#define VERSION_H

#define FIRMWARE_VERSION "dev"

#define BUILD_DATE __DATE__
#define BUILD_TIME __TIME__

#endif // VERSION_H
