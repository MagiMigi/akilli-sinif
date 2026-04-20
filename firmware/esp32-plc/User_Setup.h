//                            USER DEFINED SETTINGS
//   Akilli Sinif Sistemi - ESP32 + ST7735 1.44" TFT Ekran Yapilandirmasi
//
//   Bu dosya ESP32 DevKit V1 ve 1.44" ST7735 TFT ekran icin ozellestirilmistir.
//   Pin baglantiları:
//     TFT_CS   = GPIO 15
//     TFT_DC   = GPIO 33  (A0/RS)
//     TFT_RST  = GPIO 32
//     TFT_MOSI = GPIO 23  (SDA)
//     TFT_SCLK = GPIO 18  (SCK)
//     TFT_BL   = 3.3V (veya GPIO ile kontrol)
//
//   CI build'inde bu dosya GitHub Actions tarafindan TFT_eSPI kutuphanesinin
//   icine kopyalanir (bkz. .github/workflows/build-firmware.yml). Local build
//   icin Arduino/libraries/TFT_eSPI/User_Setup.h ile senkron tut.

#define USER_SETUP_INFO "Akilli_Sinif_ESP32_ST7735"

// ##################################################################################
//
// Section 1. Call up the right driver file and any options for it
//
// ##################################################################################

// ST7735 driver - 1.44" 128x128 ekran icin
#define ST7735_DRIVER

// Ekran boyutu (portrait modu icin)
#define TFT_WIDTH  128
#define TFT_HEIGHT 128

// ST7735 ekran tipi - 128x128 ekranlar icin
// Ekranda kayma veya karincalanma varsa farkli secenekleri deneyin
// Sadece BIR TANE aktif olmali!

// #define ST7735_GREENTAB128    // 128 x 128 ekran (yesil tab) - offset sorunu olabilir
// #define ST7735_BLACKTAB       // Siyah tab - 1.44" ekranlar icin
// #define ST7735_REDTAB         // Kirmizi tab
// #define ST7735_GREENTAB       // Yesil tab (160x128)
// #define ST7735_GREENTAB2      // Yesil tab varyant 2
#define ST7735_GREENTAB3         // Yesil tab varyant 3 - 1.44" icin dene

// 1.44" 128x128 ekranlar icin manuel offset ayari
#define TFT_COLSTART 0
#define TFT_ROWSTART 0

// ##################################################################################
//
// Section 2. Define the pins that are used to interface with the display here
//
// ##################################################################################

#define TFT_MISO  -1
#define TFT_MOSI  23
#define TFT_SCLK  18
#define TFT_CS    15
#define TFT_DC    33
#define TFT_RST   32

// ##################################################################################
//
// Section 3. Define the fonts that are to be used here
//
// ##################################################################################

#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
#define LOAD_GFXFF

#define SMOOTH_FONT

// ##################################################################################
//
// Section 4. Other options
//
// ##################################################################################

#define SPI_FREQUENCY  20000000
#define SPI_READ_FREQUENCY  10000000
