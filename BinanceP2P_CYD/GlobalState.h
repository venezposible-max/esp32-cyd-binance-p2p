#pragma once
#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <WiFi.h>
#include <Preferences.h>
#include "Config.h"

/*
 * ======================================================================================
 * GLOBALSTATE.H - OBJETOS DE HARDWARE, ESTADO COMPARTIDO Y HELPERS
 * ======================================================================================
 */

// -----------------------------------------------------------------------------
// OBJETOS DE HARDWARE
// -----------------------------------------------------------------------------
SPIClass touchSpi = SPIClass(VSPI);
XPT2046_Touchscreen ts(XPT2046_CS, XPT2046_IRQ);
TFT_eSPI tft = TFT_eSPI();
Preferences prefs;

// -----------------------------------------------------------------------------
// PALETA OFICIAL BINANCE (Calculada con color565 de TFT_eSPI)
// -----------------------------------------------------------------------------
uint16_t COLOR_BG;
uint16_t COLOR_HEADER_BG;
uint16_t COLOR_CARD_BG;
uint16_t COLOR_CARD_BORDER;
uint16_t COLOR_BINANCE_YEL;
uint16_t COLOR_BUY_GREEN;
uint16_t COLOR_SELL_RED;
uint16_t COLOR_TEXT_WHITE;
uint16_t COLOR_TEXT_GRAY;
uint16_t COLOR_CHIP_BG;
uint16_t COLOR_CHIP_TXT;
uint16_t COLOR_CYAN;

inline void initColors() {
  COLOR_BG          = tft.color565(0x18, 0x1A, 0x20); // #181A20 Fondo oscuro oficial
  COLOR_HEADER_BG   = tft.color565(0x10, 0x12, 0x16); // #101216
  COLOR_CARD_BG     = tft.color565(0x20, 0x26, 0x30); // #202630 Tarjetas
  COLOR_CARD_BORDER = tft.color565(0x2E, 0x36, 0x42); // #2E3642 Borde
  COLOR_BINANCE_YEL = tft.color565(0xF0, 0xB9, 0x0B); // #F0B90B Amarillo
  COLOR_BUY_GREEN   = tft.color565(0x0E, 0xCB, 0x81); // #0ECB81 Verde
  COLOR_SELL_RED    = tft.color565(0xF6, 0x46, 0x5D); // #F6465D Rojo
  COLOR_TEXT_WHITE  = tft.color565(0xFA, 0xFA, 0xFA); // #FAFAFA Blanco
  COLOR_TEXT_GRAY   = tft.color565(0x84, 0x8E, 0x9C); // #848E9C Gris
  COLOR_CHIP_BG     = tft.color565(0x2B, 0x31, 0x3A); // #2B313A
  COLOR_CHIP_TXT    = tft.color565(0xEA, 0xEC, 0xF0); // #EAECF0
  COLOR_CYAN        = tft.color565(0x38, 0xBD, 0xF8); // #38BDF8 Azul Pago Móvil
}

inline void safeStrCopy(char* dest, const char* src, size_t maxLen) {
  if (!dest || maxLen == 0) return;
  if (!src) {
    dest[0] = '\0';
    return;
  }
  strncpy(dest, src, maxLen - 1);
  dest[maxLen - 1] = '\0';
}

inline void setBacklightBrightness(uint8_t brightness) {
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
  ledcWrite(BACKLIGHT_PIN_A, brightness);
  ledcWrite(BACKLIGHT_PIN_B, brightness);
#else
  ledcWrite(0, brightness);
  ledcWrite(1, brightness);
#endif
}

inline void initBacklight() {
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
  ledcAttach(BACKLIGHT_PIN_A, BACKLIGHT_FREQ, BACKLIGHT_RES);
  ledcAttach(BACKLIGHT_PIN_B, BACKLIGHT_FREQ, BACKLIGHT_RES);
#else
  ledcSetup(0, BACKLIGHT_FREQ, BACKLIGHT_RES);
  ledcAttachPin(BACKLIGHT_PIN_A, 0);
  ledcSetup(1, BACKLIGHT_FREQ, BACKLIGHT_RES);
  ledcAttachPin(BACKLIGHT_PIN_B, 1);
#endif
  setBacklightBrightness(BRIGHTNESS_FULL);
}

inline String formatCompactTime(String raw) {
  raw.trim();
  raw.replace("p. m.", "PM");
  raw.replace("a. m.", "AM");
  raw.replace("p.m.", "PM");
  raw.replace("a.m.", "AM");
  raw.replace("P.M.", "PM");
  raw.replace("A.M.", "AM");
  raw.replace("  ", " ");

  int firstColon = raw.indexOf(':');
  if (firstColon != -1) {
    int secondColon = raw.indexOf(':', firstColon + 1);
    if (secondColon != -1) {
      String timePart = raw.substring(0, secondColon);
      String rest = "";
      if (secondColon + 3 < raw.length()) {
        rest = raw.substring(secondColon + 3);
        rest.trim();
      }
      raw = (rest.length() > 0) ? (timePart + " " + rest) : timePart;
    }
  }
  return raw;
}

inline void drawWifiIcon(int x, int y, bool connected, int32_t rssi) {
  uint16_t boxBg = connected ? tft.color565(0x18, 0x28, 0x38) : tft.color565(0x32, 0x18, 0x18);
  uint16_t border = connected ? COLOR_CYAN : COLOR_SELL_RED;
  tft.fillRoundRect(x, y, 34, 18, 4, boxBg);
  tft.drawRoundRect(x, y, 34, 18, 4, border);

  if (!connected) {
    tft.setTextColor(COLOR_SELL_RED, boxBg);
    tft.drawString("!WF", x + 6, y + 5, 1);
  } else {
    int bars = 1;
    if (rssi >= -65) bars = 4;
    else if (rssi >= -75) bars = 3;
    else if (rssi >= -85) bars = 2;

    for (int b = 0; b < 4; b++) {
      int bh = 3 + b * 2;
      int bx = x + 4 + b * 4;
      int by = y + 14 - bh;
      uint16_t bCol = (b < bars) ? COLOR_BUY_GREEN : tft.color565(0x35, 0x45, 0x55);
      tft.fillRect(bx, by, 2, bh, bCol);
    }
    tft.setTextColor(COLOR_CYAN, boxBg);
    tft.drawString("WF", x + 19, y + 5, 1);
  }
}

// -----------------------------------------------------------------------------
// VARIABLES DE ESTADO GLOBALES
// -----------------------------------------------------------------------------
SavedNet savedNetworks[MAX_SAVED_NETWORKS];
int savedNetworksCount = 0;
bool isWifiListOpen         = false;
bool isWifiKbOpen           = false;
String selectedWifiSSID     = "";
String wifiPasswordInput    = "";
bool wifiPassVisible        = false;
int wifiKbMode              = 0; // 0 = abc, 1 = ABC, 2 = 123/símbolos
int wifiListPage            = 0;
int scannedNetworksCount    = 0;
ScannedNet scannedNets[MAX_SCANNED];

String currentAmountFilter = "";
bool isKeypadOpen          = false;
String keypadInput         = "";
int selectedBankIdx       = 1; // "Pago Movil" por defecto
bool isBankModalOpen      = false;
int bankModalPage         = 0;

P2PAd adsList[MAX_ADS];
int adsCount = 0;
String currentTradeType = "BUY";
String lastUpdatedStr   = "--:--";
int currentCardPage = 0;
bool isDragging = false;
int touchStartY = 0;
int lastTouchY = 0;

TaskHandle_t netTaskHandle = NULL;
SemaphoreHandle_t p2pMutex = NULL;
volatile bool requestImmediateFetch = false;
volatile bool hasNewDataToDisplay   = false;
unsigned long lastTouchMs           = 0;
unsigned long lastFetchMillis       = 0;
bool isFetching                     = false;

float bcvRate                  = 0.0;
float intervencionRate         = 0.0;
unsigned long lastBcvFetchMs   = 0;
String bcvUpdatedAt            = "--:--";

bool isAlertBuyActive              = false;
float alertBuyTargetPrice          = 0.0;
unsigned long lastAlertBuyTriggerMs = 0;
float lastTriggeredBuyPrice        = 0.0;

bool isAlertSellActive             = false;
float alertSellTargetPrice         = 0.0;
unsigned long lastAlertSellTriggerMs = 0;
float lastTriggeredSellPrice       = 0.0;

bool isAlertModalOpen              = false;
String alertInputPrice             = "";
bool isFullScreenAlertOpen         = false;
unsigned long fullScreenAlertStartMs = 0;
bool isAlertFlashing               = false;
unsigned long alertFlashStartMs    = 0;
String alertTriggeredSide          = "";
String alertTriggeredTrader        = "";
String alertTriggeredPriceStr      = "";
String alertTriggeredTargetStr     = "";
String alertTriggeredOrdersStr     = "";
String alertTriggeredBankStr       = "";
String alertTriggeredCryptoStr     = "";
bool telegramAlertDelivered        = false;

bool isScreensaverActive = false;
unsigned long lastUserInteractionMs = 0;
unsigned long lastScreensaverAnimMs = 0;
float ssCardX = 25.0;
float ssCardY = 30.0;
float ssSpeedX = 0.8;
float ssSpeedY = 0.6;
ScreenStar stars[NUM_STARS];
bool starsInitialized = false;

bool isCalcOpen            = false;
int calcMode               = 0;
int calcStep               = -1;
String calcInputStr        = "";
float calcMontoEntrada     = 0.0;
float calcMontoBs          = 0.0;
float calcTasaVenta        = 0.0;
float calcTasaInterv       = 0.0;
float calcComBancoPct      = 0.0;
float calcComPasarelaPct   = 0.0;
bool  calcAplicaP2P        = false;
float calcComP2PPct        = 0.0;

float calcUsdtInicial      = 0.0;
float calcUsdBruto         = 0.0;
float calcUsdTrasBanco     = 0.0;
float calcUsdPasarela      = 0.0;
float calcUsdFinal         = 0.0;
float calcGananciaUsd      = 0.0;
float calcGananciaBs       = 0.0;
float calcRoiPct           = 0.0;

int lastRawX = -1;
int lastRawY = -1;
bool hasLastTouch = false;

// -----------------------------------------------------------------------------
// DECLARACIÓN PREVIA DE FUNCIONES (Forward Declarations)
// -----------------------------------------------------------------------------
void drawUI();
void drawHeader();
void drawTabsAndFilters();
void drawAdsList();
void drawBottomBar();
void drawKeypadDisplayBox();
void drawKeypad();
void drawBankModal();
void drawAlertModal();
void drawAlertModalDisplayBox();
void drawAlertNotificationBanner();
void drawFullScreenAlert();
void drawWifiListModal();
void drawWifiKeyboardDisplayBox();
void drawWifiKeyboard();
void enterScreensaver();
void exitScreensaver();
void updateScreensaver();
void initScreensaverStars();
void openCalculator();
void closeCalculator();
void calcularResultadosArbitraje();
void drawCalcKeypad(const char* promptTitle, const char* unitSuffix);
void drawCalculatorScreen();
void handleCalculatorTouch(int x, int y);
void handleKeypadTouch(int x, int y);
void handleBankModalTouch(int x, int y);
void handleWifiListTouch(int x, int y);
void handleWifiKbTouch(int x, int y);
void handleAlertModalTouch(int x, int y);
void handleTouch();
void loadSavedNetworks();
void saveKnownNetwork(const char* ssid, const char* pass);
bool autoConnectWiFi();
void scanWifiNetworks();
void connectToSelectedWifi();
bool fetchBinanceP2P(String tradeType);
bool sendTelegramP2PAlert(P2PAd topAd, float targetPrice);
void checkP2PAlerts();
bool fetchBcvRate();
void networkTask(void *pvParameters);
