/*
 * ======================================================================================
 * CRÉALO - MONITOR BINANCE P2P DE ESCRITORIO (USDT / VES) - MODO HORIZONTAL (320x240)
 * ======================================================================================
 * Hardware: Sunton ESP32-2432S028 (CYD Dual USB)
 * Pantalla: TFT_eSPI (Driver ST7789 a 55MHz nativo)
 * Táctil: XPT2046_Touchscreen (Bus SPI independiente)
 * API: https://monitor-luz-vercel-six.vercel.app/api/p2p
 * ======================================================================================
 */

#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>

// -----------------------------------------------------------------------------
// PINES DEL TÁCTIL (XPT2046 en CYD)
// -----------------------------------------------------------------------------
#define XPT2046_IRQ  36
#define XPT2046_MOSI 32
#define XPT2046_MISO 39
#define XPT2046_CLK  25
#define XPT2046_CS   33

SPIClass touchSpi = SPIClass(VSPI);
XPT2046_Touchscreen ts(XPT2046_CS, XPT2046_IRQ);
TFT_eSPI tft = TFT_eSPI();

// -----------------------------------------------------------------------------
// PINES DE ALERTA HARDWARE (LED RGB TRASERO EN CYD)
// -----------------------------------------------------------------------------
#define RGB_LED_RED   4
#define RGB_LED_GREEN 16
#define RGB_LED_BLUE  17

// -----------------------------------------------------------------------------
// CONFIGURACIÓN DE ALERTAS TELEGRAM Y P2P (#1)
// -----------------------------------------------------------------------------
const char* TELEGRAM_ALERT_URL = "https://monitor-luz-vercel-six.vercel.app/api/p2p-alert";
const char* TELEGRAM_CHAT_ID   = "TU_CHAT_ID_TELEGRAM";

// Variables de Alerta de COMPRA
bool isAlertBuyActive              = false;
float alertBuyTargetPrice          = 0.0;
unsigned long lastAlertBuyTriggerMs = 0;
float lastTriggeredBuyPrice        = 0.0;

// Variables de Alerta de VENTA
bool isAlertSellActive             = false;
float alertSellTargetPrice         = 0.0;
unsigned long lastAlertSellTriggerMs = 0;
float lastTriggeredSellPrice       = 0.0;

// Estado del Modal y Alerta a Pantalla Completa
bool isAlertModalOpen              = false;
String alertInputPrice             = "";
bool isFullScreenAlertOpen         = false;
unsigned long fullScreenAlertStartMs = 0;
const unsigned long FULL_ALERT_DURATION_MS = 120000; // 2 minutos
bool isAlertFlashing               = false;
unsigned long alertFlashStartMs    = 0;
String alertTriggeredSide          = "";
String alertTriggeredTrader        = "";
String alertTriggeredPriceStr      = "";
String alertTriggeredTargetStr     = "";
String alertTriggeredOrdersStr     = "";
String alertTriggeredBankStr       = "";
String alertTriggeredCryptoStr     = "";

// -----------------------------------------------------------------------------
// GESTIÓN MULTI-WIFI Y MEMORIA NVS (Preferences)
// -----------------------------------------------------------------------------
Preferences prefs;
#define MAX_SAVED_NETWORKS 5

struct SavedNet {
  char ssid[33];
  char pass[65];
};

SavedNet savedNetworks[MAX_SAVED_NETWORKS];
int savedNetworksCount = 0;

// Configuración por defecto de respaldo
const char* DEFAULT_WIFI_SSID = "TU_WIFI_AQUI";
const char* DEFAULT_WIFI_PASS = "TU_PASSWORD_AQUI";
const char* API_BASE_URL      = "https://monitor-luz-vercel-six.vercel.app/api/p2p";

// Variables de estado del gestor de WiFi táctil
bool isWifiListOpen         = false;
bool isWifiKbOpen           = false;
String selectedWifiSSID     = "";
String wifiPasswordInput    = "";
bool wifiPassVisible        = false;
int wifiKbMode              = 0; // 0 = abc, 1 = ABC, 2 = 123/símbolos
int wifiListPage            = 0;
int scannedNetworksCount    = 0;

#define MAX_SCANNED 15
struct ScannedNet {
  String ssid;
  int32_t rssi;
  bool isKnown;
};
ScannedNet scannedNets[MAX_SCANNED];

// -----------------------------------------------------------------------------
// FILTROS INTERACTIVOS
// -----------------------------------------------------------------------------
// 1. Filtro de Monto
String currentAmountFilter = ""; // Vacío = "Todos los montos", o ej "3500"
bool isKeypadOpen          = false;
String keypadInput         = "";

// 2. Filtro de Bancos Oficiales de Binance en Venezuela
const char* BANK_TITLES[] = {
  // Página 1: Principales
  "Todos los Metodos",
  "Pago Movil",
  "Banesco",
  "Mercantil",
  "Banco de Venezuela",
  "Bancamiga",
  "Provincial (BBVA)",
  
  // Página 2: Estatales y Comerciales
  "B. D. Trabajadores",
  "Banco del Tesoro",
  "Banco Plaza",
  "100% Banco",
  "Banco Exterior",
  "BNC",
  "Bancaribe",

  // Página 3: Otros Bancos
  "Banplus",
  "BFC Fondo Comun",
  "Banco Activo",
  "Venezolano Credito",
  "Transf. Bancaria"
};

const char* BANK_CHIP_NAMES[] = {
  // Página 1
  "Todos",
  "P. Movil",
  "Banesco",
  "Mercantil",
  "Bco Vzla",
  "Bancamiga",
  "Provincial",

  // Página 2
  "Trabajadores",
  "Tesoro",
  "B. Plaza",
  "100% Bco",
  "Exterior",
  "BNC",
  "Bancaribe",

  // Página 3
  "Banplus",
  "BFC",
  "Activo",
  "VeneCredit",
  "Transfer"
};

const char* BANK_VALS[] = {
  // Página 1
  "ALL",
  "PagoMovil",
  "Banesco",
  "Mercantil",
  "BancoDeVenezuela",
  "Bancamiga",
  "Provincial",

  // Página 2
  "BDDT",
  "BancoDelTesoro",
  "BancoPlaza",
  "BANK",
  "BANK",
  "BNCBancoNacional",
  "Bancaribe",

  // Página 3
  "Banplus",
  "BFC",
  "BancoActivo",
  "BancoVeneCredit",
  "BANK"
};

const int TOTAL_BANKS     = 19;
int selectedBankIdx       = 1; // "Pago Movil" por defecto
bool isBankModalOpen      = false;
int bankModalPage         = 0; // 0 = Pág 1, 1 = Pág 2, 2 = Pág 3, 3 = Pág 4

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

void initColors() {
  COLOR_BG          = tft.color565(0x18, 0x1A, 0x20); // #181A20 Fondo oscuro oficial
  COLOR_HEADER_BG   = tft.color565(0x10, 0x12, 0x16); // #101216
  COLOR_CARD_BG     = tft.color565(0x20, 0x26, 0x30); // #202630 Tarjetas
  COLOR_CARD_BORDER = tft.color565(0x2E, 0x36, 0x42); // #2E3642 Borde
  COLOR_BINANCE_YEL = tft.color565(0xF0, 0xB9, 0x0B); // #F0B90B Amarillo
  COLOR_BUY_GREEN   = tft.color565(0x0E, 0xCB, 0x81); // #0ECB81 Verde
  COLOR_SELL_RED    = tft.color565(0xF6, 0x46, 0x5D); // #F6465D Rojo
  COLOR_TEXT_WHITE  = TFT_WHITE;
  COLOR_TEXT_GRAY   = tft.color565(0x84, 0x8E, 0x9C); // #848E9C Gris secundario
  COLOR_CHIP_BG     = tft.color565(0x28, 0x30, 0x3D); // #28303D Fondo de chip
  COLOR_CHIP_TXT    = tft.color565(0xEA, 0xEC, 0xF0); // #EAECF0
  COLOR_CYAN        = tft.color565(0x38, 0xBD, 0xF8); // #38BDF8 Azul Pago Móvil
}

// -----------------------------------------------------------------------------
// ESTRUCTURA DE DATOS PARA CADA ANUNCIO P2P
// -----------------------------------------------------------------------------
// ESTRUCTURA DE OFERTAS P2P (Buffers Fijos para erradicar fragmentación de RAM)
// -----------------------------------------------------------------------------
struct P2PAd {
  char name[32];
  int  orders;
  char rate[12];
  char price[14];
  char minLimit[14];
  char maxLimit[14];
  char crypto[14];
  char banks[48];
};

inline void safeStrCopy(char* dest, const char* src, size_t maxLen) {
  if (!dest || maxLen == 0) return;
  if (!src) {
    dest[0] = '\0';
    return;
  }
  strncpy(dest, src, maxLen - 1);
  dest[maxLen - 1] = '\0';
}

#define MAX_ADS 15
P2PAd adsList[MAX_ADS];
int adsCount = 0;
String currentTradeType = "BUY"; // "BUY" = Comprar, "SELL" = Vender
String lastUpdatedStr   = "--:--";

// Control de navegación por páginas de ofertas (2 tarjetas panorámicas por página)
int currentCardPage = 0;
bool isDragging = false;
int touchStartY = 0;
int lastTouchY = 0;

// Variables FreeRTOS y control de concurrencia multinúcleo
TaskHandle_t netTaskHandle = NULL;
SemaphoreHandle_t p2pMutex = NULL; // Llave de exclusión mutua (Mutex) para proteger memoria entre Core 0 y Core 1
volatile bool requestImmediateFetch = false;
volatile bool hasNewDataToDisplay   = false;
unsigned long lastTouchMs           = 0;

// Refresco periódico - Intervalos Inteligentes (Optimización de cuota Vercel 100k/mes)
unsigned long lastFetchMillis = 0;
const unsigned long FETCH_INTERVAL_AWAKE_MS = 20000; // 20 seg: Pantalla activa despierta
const unsigned long FETCH_INTERVAL_SLEEP_MS = 60000; // 60 seg: Modo salvapantallas (reposo)
const unsigned long FETCH_INTERVAL_ALERT_MS = 15000; // 15 seg: Si hay alerta activa en este mercado
bool isFetching = false;

// -----------------------------------------------------------------------------
// TASAS DE CAMBIO: BCV E INTERVENCIÓN CAMBIARIA (+0.5%)
// -----------------------------------------------------------------------------
const char* BCV_API_URL        = "https://rates.dolarvzla.com/bcv/current.json";
const char* BCV_API_FALLBACK   = "https://ve.dolarapi.com/v1/dolares/oficial";
float bcvRate                  = 0.0;
float intervencionRate         = 0.0;
unsigned long lastBcvFetchMs   = 0;
const unsigned long BCV_FETCH_INTERVAL_MS = 300000; // Consultar BCV cada 5 minutos (solo cambia 1-2 veces al día)
String bcvUpdatedAt            = "--:--";

// Declaración previa
bool fetchBcvRate();

// -----------------------------------------------------------------------------
// SALVAPANTALLAS "CYBER STARFIELD & CLOCK" (Zero-RAM, seguro y ultra liviano)
// -----------------------------------------------------------------------------
bool isScreensaverActive = false;
unsigned long lastUserInteractionMs = 0;
const unsigned long SCREENSAVER_TIMEOUT_MS = 120000; // 2 minutos de inactividad para entrar
unsigned long lastScreensaverAnimMs = 0;
const int SCREENSAVER_FPS_DELAY = 60; // ~16 fps suave y descansado para el micro

// Posición y velocidad de la cápsula flotante
float ssCardX = 25.0;
float ssCardY = 30.0;
float ssSpeedX = 0.8;
float ssSpeedY = 0.6;
const int SS_CARD_W = 240;
const int SS_CARD_H = 126;

// Estrellas de fondo (35 estrellas calculadas con memoria estática fija)
#define NUM_STARS 35
struct ScreenStar {
  int16_t x;
  int16_t y;
  uint8_t speed;
  uint16_t color;
};
ScreenStar stars[NUM_STARS];
bool starsInitialized = false;

// Declaración previa de funciones
void drawUI();
void drawWifiKeyboard();
void drawWifiListModal();
void enterScreensaver();
void exitScreensaver();
void updateScreensaver();
void initScreensaverStars();
void drawAlertModal();
void drawAlertModalDisplayBox();
void drawAlertNotificationBanner();
void drawFullScreenAlert();
void handleAlertModalTouch(int x, int y);
void checkP2PAlerts();
void sendTelegramP2PAlert(P2PAd topAd, float targetPrice);

// -----------------------------------------------------------------------------
// CALCULADORA DE ARBITRAJE INTERVENCIÓN / P2P (Wizard Dual: USDT o VES)
// -----------------------------------------------------------------------------
bool isCalcOpen            = false;
int calcMode               = 0;    // 0 = Tengo USDT (Dólares), 1 = Tengo VES (Bolívares)
int calcStep               = -1;   // -1: Selector Inicial (USDT o VES), 0: Monto Inicial, 1: Tasa Venta, 2: Tasa Interv, 3: Com. Banco, 4: Com. Pasarela, 5: Aplica P2P?, 6: Com. P2P, 7: Resumen
String calcInputStr        = "";
float calcMontoEntrada     = 0.0;  // Puede ser USDT o Bs según calcMode
float calcMontoBs          = 0.0;
float calcTasaVenta        = 0.0;
float calcTasaInterv       = 0.0;
float calcComBancoPct      = 0.0;
float calcComPasarelaPct   = 0.0;
bool  calcAplicaP2P        = false;
float calcComP2PPct        = 0.0;

// Resultados calculados
float calcUsdtInicial      = 0.0;
float calcUsdBruto         = 0.0;
float calcUsdTrasBanco     = 0.0;
float calcUsdPasarela      = 0.0;
float calcUsdFinal         = 0.0;
float calcGananciaUsd      = 0.0;
float calcGananciaBs       = 0.0;
float calcRoiPct           = 0.0;

void openCalculator();
void closeCalculator();
void drawCalculatorScreen();
void handleCalculatorTouch(int x, int y);

// -----------------------------------------------------------------------------
// GESTIÓN DE REDES EN MEMORIA NO VOLÁTIL (NVS - Preferences)
// -----------------------------------------------------------------------------
void loadSavedNetworks() {
  prefs.begin("crealo_wifi", false);
  savedNetworksCount = prefs.getInt("count", 0);
  if (savedNetworksCount <= 0) {
    savedNetworksCount = 1;
    strncpy(savedNetworks[0].ssid, DEFAULT_WIFI_SSID, 32);
    savedNetworks[0].ssid[32] = '\0';
    strncpy(savedNetworks[0].pass, DEFAULT_WIFI_PASS, 64);
    savedNetworks[0].pass[64] = '\0';
    prefs.putInt("count", 1);
    prefs.putString("ssid_0", savedNetworks[0].ssid);
    prefs.putString("pass_0", savedNetworks[0].pass);
  } else {
    if (savedNetworksCount > MAX_SAVED_NETWORKS) savedNetworksCount = MAX_SAVED_NETWORKS;
    for (int i = 0; i < savedNetworksCount; i++) {
      String s = prefs.getString(("ssid_" + String(i)).c_str(), "");
      String p = prefs.getString(("pass_" + String(i)).c_str(), "");
      strncpy(savedNetworks[i].ssid, s.c_str(), 32);
      savedNetworks[i].ssid[32] = '\0';
      strncpy(savedNetworks[i].pass, p.c_str(), 64);
      savedNetworks[i].pass[64] = '\0';
    }
  }
  prefs.end();
}

void saveKnownNetwork(const char* ssid, const char* pass) {
  if (!ssid || strlen(ssid) == 0) return;
  prefs.begin("crealo_wifi", false);

  int existingIdx = -1;
  for (int i = 0; i < savedNetworksCount; i++) {
    if (strcmp(savedNetworks[i].ssid, ssid) == 0) {
      existingIdx = i;
      break;
    }
  }

  if (existingIdx >= 0) {
    strncpy(savedNetworks[existingIdx].pass, pass, 64);
    savedNetworks[existingIdx].pass[64] = '\0';
    prefs.putString(("pass_" + String(existingIdx)).c_str(), savedNetworks[existingIdx].pass);
  } else {
    int targetIdx = savedNetworksCount;
    if (targetIdx >= MAX_SAVED_NETWORKS) {
      targetIdx = MAX_SAVED_NETWORKS - 1;
    } else {
      savedNetworksCount++;
      prefs.putInt("count", savedNetworksCount);
    }
    strncpy(savedNetworks[targetIdx].ssid, ssid, 32);
    savedNetworks[targetIdx].ssid[32] = '\0';
    strncpy(savedNetworks[targetIdx].pass, pass, 64);
    savedNetworks[targetIdx].pass[64] = '\0';
    prefs.putString(("ssid_" + String(targetIdx)).c_str(), savedNetworks[targetIdx].ssid);
    prefs.putString(("pass_" + String(targetIdx)).c_str(), savedNetworks[targetIdx].pass);
  }
  prefs.end();
}

bool autoConnectWiFi() {
  loadSavedNetworks();
  Serial.println("[WiFi] Escaneando el entorno para autoconectar...");
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  int n = WiFi.scanNetworks();
  Serial.printf("[WiFi] %d redes detectadas en el aire\n", n);

  int bestSavedIdx = -1;
  if (n > 0) {
    int bestRssi = -999;
    for (int i = 0; i < n; i++) {
      String airSSID = WiFi.SSID(i);
      for (int k = 0; k < savedNetworksCount; k++) {
        if (airSSID == savedNetworks[k].ssid) {
          int32_t rssi = WiFi.RSSI(i);
          if (rssi > bestRssi) {
            bestRssi = rssi;
            bestSavedIdx = k;
          }
        }
      }
    }
  }

  if (bestSavedIdx == -1 && savedNetworksCount > 0) {
    bestSavedIdx = 0; // Intentar red 0 por defecto
  }

  if (bestSavedIdx >= 0) {
    Serial.printf("[WiFi] Conectando a: %s\n", savedNetworks[bestSavedIdx].ssid);
    WiFi.begin(savedNetworks[bestSavedIdx].ssid, savedNetworks[bestSavedIdx].pass);
    int retries = 0;
    while (WiFi.status() != WL_CONNECTED && retries < 25) {
      delay(350);
      Serial.print(".");
      retries++;
    }
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\n[WiFi] Conectado exitosamente! IP: " + WiFi.localIP().toString());
      return true;
    }
  }
  return false;
}

void scanWifiNetworks() {
  // Mostrar pantalla de búsqueda
  tft.fillRoundRect(8, 20, 224, 284, 8, COLOR_CARD_BG);
  tft.drawRoundRect(8, 20, 224, 284, 8, COLOR_CARD_BORDER);
  tft.setTextColor(COLOR_BINANCE_YEL, COLOR_CARD_BG);
  tft.drawCentreString("REDES WIFI", 120, 32, 2);
  tft.setTextColor(COLOR_CYAN, COLOR_CARD_BG);
  tft.drawCentreString("Buscando redes...", 120, 140, 2);

  int n = WiFi.scanNetworks();
  scannedNetworksCount = 0;
  if (n > 0) {
    for (int i = 0; i < n && scannedNetworksCount < MAX_SCANNED; i++) {
      String s = WiFi.SSID(i);
      s.trim();
      if (s.length() == 0) continue;

      bool dup = false;
      for (int d = 0; d < scannedNetworksCount; d++) {
        if (scannedNets[d].ssid == s) {
          dup = true;
          break;
        }
      }
      if (dup) continue;

      scannedNets[scannedNetworksCount].ssid = s;
      scannedNets[scannedNetworksCount].rssi = WiFi.RSSI(i);

      bool known = false;
      for (int k = 0; k < savedNetworksCount; k++) {
        if (s == savedNetworks[k].ssid) {
          known = true;
          break;
        }
      }
      scannedNets[scannedNetworksCount].isKnown = known;
      scannedNetworksCount++;
    }
  }
}

void drawWifiIcon(int x, int y, bool connected, int32_t rssi) {
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
// CONSULTA A LA API DE VERCEL (Con filtros en tiempo real)
// -----------------------------------------------------------------------------
bool fetchBinanceP2P(String tradeType) {
  if (WiFi.status() != WL_CONNECTED) return false;
  isFetching = true;

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.setReuse(true); // Reutilizar conexión HTTP Keep-Alive

  String url = String(API_BASE_URL) + "?tradeType=" + tradeType;
  url += "&payType=" + String(BANK_VALS[selectedBankIdx]);
  if (currentAmountFilter.length() > 0) {
    url += "&transAmount=" + currentAmountFilter;
  }
  url += "&rows=15";
  
  if (!http.begin(client, url)) {
    isFetching = false;
    return false;
  }

  http.setTimeout(5000);
  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    
    StaticJsonDocument<256> filter;
    filter["success"] = true;
    filter["updatedAt"] = true;
    JsonObject adFilter = filter["ads"].createNestedObject();
    adFilter["name"] = true;
    adFilter["orders"] = true;
    adFilter["rate"] = true;
    adFilter["price"] = true;
    adFilter["min"] = true;
    adFilter["max"] = true;
    adFilter["crypto"] = true;
    adFilter["banks"] = true;

    DynamicJsonDocument doc(4096);
    DeserializationError error = deserializeJson(doc, payload, DeserializationOption::Filter(filter));

    if (!error && doc["success"]) {
      String newUpdatedAt = doc["updatedAt"].as<String>();
      JsonArray arr = doc["ads"].as<JsonArray>();
      
      P2PAd tempAds[MAX_ADS];
      int tempCount = 0;
      for (JsonObject item : arr) {
        if (tempCount >= MAX_ADS) break;
        safeStrCopy(tempAds[tempCount].name, item["name"] | "", sizeof(tempAds[tempCount].name));
        tempAds[tempCount].orders   = item["orders"] | 0;
        safeStrCopy(tempAds[tempCount].rate, item["rate"] | "", sizeof(tempAds[tempCount].rate));
        safeStrCopy(tempAds[tempCount].price, item["price"] | "", sizeof(tempAds[tempCount].price));
        safeStrCopy(tempAds[tempCount].minLimit, item["min"] | "", sizeof(tempAds[tempCount].minLimit));
        safeStrCopy(tempAds[tempCount].maxLimit, item["max"] | "", sizeof(tempAds[tempCount].maxLimit));
        safeStrCopy(tempAds[tempCount].crypto, item["crypto"] | "", sizeof(tempAds[tempCount].crypto));
        
        tempAds[tempCount].banks[0] = '\0';
        JsonArray banksArr = item["banks"].as<JsonArray>();
        for (JsonVariant b : banksArr) {
          const char* bName = b.as<const char*>();
          if (!bName) continue;
          if (tempAds[tempCount].banks[0] != '\0') {
            strncat(tempAds[tempCount].banks, " - ", sizeof(tempAds[tempCount].banks) - strlen(tempAds[tempCount].banks) - 1);
          }
          strncat(tempAds[tempCount].banks, bName, sizeof(tempAds[tempCount].banks) - strlen(tempAds[tempCount].banks) - 1);
        }
        tempCount++;
      }

      // Copia atómica y protegida por Mutex para evitar colisiones con Core 1
      if (p2pMutex != NULL && xSemaphoreTake(p2pMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        lastUpdatedStr = newUpdatedAt;
        adsCount = tempCount;
        for (int i = 0; i < adsCount; i++) {
          adsList[i] = tempAds[i];
        }
        int totalPages = (adsCount > 0) ? ((adsCount + 1) / 2) : 1;
        if (currentCardPage >= totalPages) currentCardPage = totalPages - 1;
        if (currentCardPage < 0) currentCardPage = 0;
        
        // Evaluar si la oferta #1 cumple con la alerta configurada
        checkP2PAlerts();

        xSemaphoreGive(p2pMutex);
      }

      http.end();
      isFetching = false;
      return true;
    }
  }

  http.end();
  isFetching = false;
  return false;
}

// -----------------------------------------------------------------------------
// SISTEMA DE ALERTAS: ENVÍO DE NOTIFICACIÓN A TELEGRAM Y VERIFICACIÓN
// -----------------------------------------------------------------------------
void sendTelegramP2PAlert(P2PAd topAd, float targetPrice) {
  if (WiFi.status() != WL_CONNECTED) return;

  WiFiClientSecure alertClient;
  alertClient.setInsecure();
  HTTPClient alertHttp;
  alertHttp.setTimeout(6000);

  if (!alertHttp.begin(alertClient, TELEGRAM_ALERT_URL)) return;

  alertHttp.addHeader("Content-Type", "application/json");

  StaticJsonDocument<512> alertDoc;
  alertDoc["chatId"] = TELEGRAM_CHAT_ID;
  alertDoc["tradeType"] = currentTradeType;
  alertDoc["targetPrice"] = String(targetPrice, 2);
  alertDoc["price"] = topAd.price;
  alertDoc["trader"] = topAd.name;
  alertDoc["orders"] = topAd.orders;
  alertDoc["rate"] = topAd.rate;
  alertDoc["bank"] = topAd.banks;
  alertDoc["crypto"] = topAd.crypto;

  String body;
  serializeJson(alertDoc, body);

  alertHttp.POST(body);
  alertHttp.end();
}

void checkP2PAlerts() {
  if (adsCount <= 0) return;

  bool isBuy = (currentTradeType == "BUY");
  bool active = isBuy ? isAlertBuyActive : isAlertSellActive;
  float target = isBuy ? alertBuyTargetPrice : alertSellTargetPrice;

  if (!active || target <= 0.0) return;

  String cleanP = adsList[0].price;
  cleanP.replace(",", "");
  cleanP.replace(" ", "");
  float p1 = cleanP.toFloat();
  if (p1 <= 0.0) return;

  // En COMPRA: alerta si el puesto #1 sube y alcanza o supera la meta (ej: meta 943.00 -> dispara en 943.00 o 943.50)
  // En VENTA: alerta si el puesto #1 baja y alcanza o cae por debajo de la meta (ej: meta 941.00 -> dispara en 941.00 o 940.30)
  bool conditionMet = false;
  if (isBuy) {
    conditionMet = (p1 >= target);
  } else {
    conditionMet = (p1 <= target);
  }

  unsigned long nowMs = millis();
  unsigned long lastTriggerMs = isBuy ? lastAlertBuyTriggerMs : lastAlertSellTriggerMs;
  float lastPrice = isBuy ? lastTriggeredBuyPrice : lastTriggeredSellPrice;

  if (conditionMet && (nowMs - lastTriggerMs >= 180000 || p1 != lastPrice)) {
    if (isBuy) {
      lastAlertBuyTriggerMs = nowMs;
      lastTriggeredBuyPrice = p1;
    } else {
      lastAlertSellTriggerMs = nowMs;
      lastTriggeredSellPrice = p1;
    }

    isAlertFlashing = true;
    alertFlashStartMs = nowMs;
    isFullScreenAlertOpen = true;
    fullScreenAlertStartMs = nowMs;
    alertTriggeredSide = isBuy ? "COMPRA" : "VENTA";
    alertTriggeredTrader = adsList[0].name;
    alertTriggeredPriceStr = adsList[0].price;
    alertTriggeredTargetStr = String(target, 2);
    alertTriggeredOrdersStr = String(adsList[0].orders) + " ord (" + adsList[0].rate + ")";
    alertTriggeredBankStr = adsList[0].banks;
    alertTriggeredCryptoStr = adsList[0].crypto;

    // Desactivar automáticamente la alerta al ser alcanzada (solo dispara 1 vez)
    if (isBuy) {
      isAlertBuyActive = false;
      prefs.begin("p2p_alerts", false);
      prefs.putBool("buy_act", false);
      prefs.end();
    } else {
      isAlertSellActive = false;
      prefs.begin("p2p_alerts", false);
      prefs.putBool("sell_act", false);
      prefs.end();
    }

    // Si la pantalla estaba en salvapantallas, despertar de inmediato
    if (isScreensaverActive) {
      exitScreensaver();
    }

    // Desplegar aviso a pantalla completa y enviar Telegram
    drawFullScreenAlert();
    sendTelegramP2PAlert(adsList[0], target);
  }
}

// -----------------------------------------------------------------------------
// CONSULTA A LA API DE BCV (Oficial) Y CÁLCULO DE INTERVENCIÓN (+0.5%)
// -----------------------------------------------------------------------------
bool fetchBcvRate() {
  if (WiFi.status() != WL_CONNECTED) return false;

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.setReuse(false);

  // 1. Intentar con fuente principal más actualizada (rates.dolarvzla.com)
  bool success = false;
  if (http.begin(client, BCV_API_URL)) {
    http.setTimeout(4000);
    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      StaticJsonDocument<384> doc;
      DeserializationError err = deserializeJson(doc, payload);
      if (!err && doc.containsKey("current") && doc["current"].containsKey("usd")) {
        float rate = doc["current"]["usd"].as<float>();
        if (rate > 0.0) {
          bcvRate = rate;
          intervencionRate = bcvRate * 1.005; // Sumar 0.5% (Tasa de Intervención)
          if (doc["current"].containsKey("date")) {
            String f = doc["current"]["date"].as<String>();
            if (f.length() >= 10) {
              bcvUpdatedAt = f.substring(5, 10); // MM-DD
            }
          }
          success = true;
        }
      }
    }
    http.end();
  }

  if (success) return true;

  // 2. Fallback secundario si la principal falla o está caída
  if (http.begin(client, BCV_API_FALLBACK)) {
    http.setTimeout(4000);
    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      StaticJsonDocument<384> doc;
      DeserializationError err = deserializeJson(doc, payload);
      if (!err && doc.containsKey("promedio")) {
        float rate = doc["promedio"].as<float>();
        if (rate > 0.0) {
          bcvRate = rate;
          intervencionRate = bcvRate * 1.005;
          if (doc.containsKey("fechaActualizacion")) {
            String f = doc["fechaActualizacion"].as<String>();
            if (f.length() >= 10) {
              bcvUpdatedAt = f.substring(5, 10);
            }
          }
          success = true;
        }
      }
    }
    http.end();
  }

  return success;
}

// -----------------------------------------------------------------------------
// TAREA MULTINÚCLEO DEDICADA A RED EN CORE 0 (FreeRTOS)
// -----------------------------------------------------------------------------
void networkTask(void *pvParameters) {
  for (;;) {
    bool shouldFetchBinance = false;
    bool shouldFetchBcv     = false;
    unsigned long now = millis();

    if (requestImmediateFetch) {
      requestImmediateFetch = false;
      shouldFetchBinance = true;
    } else {
      if (!isKeypadOpen && !isBankModalOpen && !isWifiListOpen && !isWifiKbOpen) {
        // Cálculo del intervalo adaptativo
        unsigned long currentInterval = FETCH_INTERVAL_AWAKE_MS;
        if (isScreensaverActive) {
          currentInterval = FETCH_INTERVAL_SLEEP_MS; // 60s en salvapantallas
        } else {
          bool alertActive = (currentTradeType == "BUY") ? isAlertBuyActive : isAlertSellActive;
          if (alertActive) {
            currentInterval = FETCH_INTERVAL_ALERT_MS; // 15s si hay alerta activa
          }
        }

        if (now - lastFetchMillis >= currentInterval) {
          shouldFetchBinance = true;
        }
        if (now - lastBcvFetchMs >= BCV_FETCH_INTERVAL_MS || bcvRate <= 0.0) {
          shouldFetchBcv = true;
        }
      }
    }

    if (WiFi.status() == WL_CONNECTED) {
      if (shouldFetchBcv) {
        lastBcvFetchMs = millis();
        if (fetchBcvRate()) {
          hasNewDataToDisplay = true;
        }
      }
      if (shouldFetchBinance) {
        lastFetchMillis = millis();
        fetchBinanceP2P(currentTradeType);
        hasNewDataToDisplay = true;
      }
    }

    vTaskDelay(pdMS_TO_TICKS(80)); // Descanso de 80ms para ceder tiempo a tareas del sistema
  }
}

// -----------------------------------------------------------------------------
// DIBUJO DE INTERFAZ ESTILO CREALO P2P - MODO HORIZONTAL (320x240)
// -----------------------------------------------------------------------------
String formatCompactTime(String raw) {
  raw.trim();
  raw.replace("p. m.", "PM");
  raw.replace("a. m.", "AM");
  raw.replace("p.m.", "PM");
  raw.replace("a.m.", "AM");
  raw.replace("P.M.", "PM");
  raw.replace("A.M.", "AM");
  raw.replace("  ", " ");

  // Remover segundos (:SS) para mantener el encabezado limpio y compacto
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

void drawHeader() {
  tft.fillRect(0, 0, 320, 24, COLOR_HEADER_BG);
  
  // 1. Insignia Logo CREALO (x=6..22, y=4..20)
  tft.fillRoundRect(6, 4, 16, 16, 4, COLOR_CYAN);
  tft.setTextColor(COLOR_HEADER_BG, COLOR_CYAN);
  tft.drawCentreString("C", 14, 5, 2);
  
  // Marca: CREALO (x=25, y=5)
  tft.setTextColor(COLOR_TEXT_WHITE, COLOR_HEADER_BG);
  tft.drawString("CREALO", 25, 5, 2);

  // 2. Icono WiFi interactivo (x=80..114)
  bool conn = (WiFi.status() == WL_CONNECTED);
  int32_t sig = conn ? WiFi.RSSI() : -100;
  drawWifiIcon(80, 3, conn, sig);

  // 3. Botón interactivo CALC (x=124..166, w=42, h=18)
  tft.fillRoundRect(124, 3, 42, 18, 4, tft.color565(0x28, 0x36, 0x4A));
  tft.drawRoundRect(124, 3, 42, 18, 4, COLOR_BINANCE_YEL);
  tft.setTextColor(COLOR_BINANCE_YEL, tft.color565(0x28, 0x36, 0x4A));
  tft.drawCentreString("CALC", 145, 5, 2);

  // 4. Botón interactivo ALERTA (#1) correspondiente a la pestaña activa (x=176..244, w=68, h=18)
  bool isBuy = (currentTradeType == "BUY");
  bool active = isBuy ? isAlertBuyActive : isAlertSellActive;
  float target = isBuy ? alertBuyTargetPrice : alertSellTargetPrice;

  uint16_t alertBg = active ? COLOR_BINANCE_YEL : tft.color565(0x28, 0x36, 0x4A);
  uint16_t alertBorder = active ? COLOR_BINANCE_YEL : tft.color565(0x3E, 0x4A, 0x5C);
  uint16_t alertTxt = active ? COLOR_HEADER_BG : COLOR_TEXT_GRAY;
  tft.fillRoundRect(176, 3, 68, 18, 4, alertBg);
  tft.drawRoundRect(176, 3, 68, 18, 4, alertBorder);
  tft.setTextColor(alertTxt, alertBg);
  
  String alertLbl = "ALERTA";
  if (active) {
    if (target == (int)target) {
      alertLbl = "#1:" + String((int)target);
    } else {
      alertLbl = "#1:" + String(target, 1);
    }
  }
  tft.drawCentreString(alertLbl, 210, 5, 2);

  // 5. Hora limpia a la derecha (espacio holgado y garantizado, x=314)
  tft.setTextColor(COLOR_TEXT_GRAY, COLOR_HEADER_BG);
  tft.drawRightString(formatCompactTime(lastUpdatedStr), 314, 5, 2);
}

void drawTabsAndFilters() {
  bool isBuy = (currentTradeType == "BUY");

  // Limpiar franja completa antes de dibujar
  tft.fillRect(0, 24, 320, 48, COLOR_BG);

  // 1. Pestañas Comprar / Vender (y = 25..45, h=21, ancho 150 c/u)
  if (isBuy) {
    tft.fillRoundRect(6, 25, 150, 21, 4, COLOR_BUY_GREEN);
    tft.setTextColor(TFT_WHITE, COLOR_BUY_GREEN);
  } else {
    tft.fillRoundRect(6, 25, 150, 21, 4, COLOR_CARD_BG);
    tft.drawRoundRect(6, 25, 150, 21, 4, COLOR_CARD_BORDER);
    tft.setTextColor(COLOR_TEXT_GRAY, COLOR_CARD_BG);
  }
  tft.drawCentreString("COMPRAR", 81, 28, 2);

  if (!isBuy) {
    tft.fillRoundRect(164, 25, 150, 21, 4, COLOR_SELL_RED);
    tft.setTextColor(TFT_WHITE, COLOR_SELL_RED);
  } else {
    tft.fillRoundRect(164, 25, 150, 21, 4, COLOR_CARD_BG);
    tft.drawRoundRect(164, 25, 150, 21, 4, COLOR_CARD_BORDER);
    tft.setTextColor(COLOR_TEXT_GRAY, COLOR_CARD_BG);
  }
  tft.drawCentreString("VENDER", 239, 28, 2);

  // 2. Fila de Filtros Interactivos (y = 48..68, h=21)
  // Chip 1: USDT (Fijo indicador)
  tft.fillRoundRect(6, 48, 54, 21, 4, COLOR_CHIP_BG);
  tft.drawRoundRect(6, 48, 54, 21, 4, COLOR_CARD_BORDER);
  tft.setTextColor(COLOR_BINANCE_YEL, COLOR_CHIP_BG);
  tft.drawCentreString("USDT", 33, 52, 1);

  // Chip 2: MONTO (Táctil - abre teclado numérico)
  tft.fillRoundRect(66, 48, 120, 21, 4, COLOR_CHIP_BG);
  tft.drawRoundRect(66, 48, 120, 21, 4, COLOR_CARD_BORDER);
  tft.setTextColor(COLOR_BINANCE_YEL, COLOR_CHIP_BG);
  String amtStr = currentAmountFilter.length() > 0 ? (currentAmountFilter + " Bs v") : "Monto: Todo v";
  tft.drawCentreString(amtStr, 126, 52, 1);

  // Chip 3: BANCO (Táctil - abre menú desplegable de bancos)
  tft.fillRoundRect(192, 48, 122, 21, 4, COLOR_CHIP_BG);
  tft.drawRoundRect(192, 48, 122, 21, 4, COLOR_CARD_BORDER);
  tft.setTextColor(COLOR_CYAN, COLOR_CHIP_BG);
  String payStr = String(BANK_CHIP_NAMES[selectedBankIdx]) + " v";
  tft.drawCentreString(payStr, 253, 52, 1);
}

void drawAdsList() {
  tft.fillRect(0, 71, 320, 144, COLOR_BG);

  // Copia segura local bajo el Mutex (< 2 microsegundos)
  P2PAd localAds[2];
  int localCount = 0;
  int totalAds = 0;

  if (p2pMutex != NULL && xSemaphoreTake(p2pMutex, pdMS_TO_TICKS(40)) == pdTRUE) {
    totalAds = adsCount;
    int startIdx = currentCardPage * 2;
    for (int i = 0; i < 2; i++) {
      int adIdx = startIdx + i;
      if (adIdx < adsCount) {
        localAds[i] = adsList[adIdx];
        localCount++;
      }
    }
    xSemaphoreGive(p2pMutex);
  } else {
    return; // Si el Core 0 está escribiendo, esperar al próximo frame
  }

  for (int i = 0; i < localCount; i++) {
    int cardY = 73 + (i * 70);

    // Tarjeta estilizada (312px de ancho x 66px de alto)
    tft.fillRoundRect(4, cardY, 312, 66, 6, COLOR_CARD_BG);
    tft.drawRoundRect(4, cardY, 312, 66, 6, COLOR_CARD_BORDER);

    // Fila 1: Avatar inicial
    tft.fillCircle(16, cardY + 13, 7, tft.color565(0x35, 0x40, 0x50));
    char initial = (localAds[i].name[0] != '\0') ? localAds[i].name[0] : 'U';
    tft.setTextColor(TFT_WHITE, tft.color565(0x35, 0x40, 0x50));
    tft.drawCentreString(String(initial), 16, cardY + 8, 1);

    // Fila 1: Nombre del comerciante
    tft.setTextColor(COLOR_TEXT_WHITE, COLOR_CARD_BG);
    String displayName = localAds[i].name;
    if (displayName.length() > 20) displayName = displayName.substring(0, 19) + "..";
    tft.drawString(displayName, 28, cardY + 6, 2);

    // Fila 1: Órdenes y efectividad (alineado a la derecha en 308)
    tft.setTextColor(COLOR_TEXT_GRAY, COLOR_CARD_BG);
    String stats = String(localAds[i].orders) + " ord | " + localAds[i].rate;
    tft.drawRightString(stats, 308, cardY + 7, 1);

    // Fila 2: Precio en grande (Verde o Rojo)
    bool isBuy = (currentTradeType == "BUY");
    tft.setTextColor(isBuy ? COLOR_BUY_GREEN : COLOR_SELL_RED, COLOR_CARD_BG);
    tft.drawString(String("Bs ") + localAds[i].price, 10, cardY + 24, 4);

    // Fila 2: Cápsula de Saldo Disponible en USDT (Reemplaza botón redundante)
    tft.fillRoundRect(196, cardY + 23, 114, 19, 4, tft.color565(0x16, 0x20, 0x2C));
    tft.drawRoundRect(196, cardY + 23, 114, 19, 4, tft.color565(0x28, 0x36, 0x48));
    tft.setTextColor(tft.color565(0x84, 0x92, 0xA6), tft.color565(0x16, 0x20, 0x2C));
    tft.drawString("Disp:", 202, cardY + 28, 1);
    String cryptoStr = localAds[i].crypto;
    if (cryptoStr.length() > 9) cryptoStr = cryptoStr.substring(0, 8) + "..";
    tft.setTextColor(COLOR_BINANCE_YEL, tft.color565(0x16, 0x20, 0x2C));
    tft.drawRightString(cryptoStr + " $", 304, cardY + 28, 1);

    // Fila 3: Límites
    tft.setTextColor(tft.color565(0x84, 0x8E, 0x9C), COLOR_CARD_BG);
    String lim = String("Lim: ") + localAds[i].minLimit + " - " + localAds[i].maxLimit;
    if (lim.length() > 28) lim = lim.substring(0, 27) + "..";
    tft.drawString(lim, 10, cardY + 48, 1);

    // Fila 3: Banco (Chip a la derecha alineado con la cápsula superior)
    String b = localAds[i].banks;
    if (b.length() > 18) b = b.substring(0, 17) + "..";
    tft.fillRoundRect(196, cardY + 45, 114, 16, 3, tft.color565(0x18, 0x24, 0x32));
    tft.setTextColor(COLOR_CYAN, tft.color565(0x18, 0x24, 0x32));
    tft.drawCentreString(b, 253, cardY + 47, 1);
  }

  if (totalAds == 0) {
    tft.setTextColor(COLOR_TEXT_GRAY, COLOR_BG);
    tft.drawCentreString(isFetching ? "Filtrando ofertas..." : "Sin ofertas para este filtro", 160, 130, 2);
  }
}

// -----------------------------------------------------------------------------
// VENTANA MODAL: CALCULADORA DE ARBITRAJE INTERVENCIÓN / P2P
// -----------------------------------------------------------------------------
void openCalculator() {
  isCalcOpen = true;
  calcStep = -1; // Arranca en el Selector de Modo: [USDT] o [VES]
  calcInputStr = "";
  calcMontoEntrada = 0.0;
  calcMontoBs = 0.0;
  calcTasaVenta = 0.0;
  calcTasaInterv = (intervencionRate > 0.0) ? intervencionRate : ((bcvRate > 0.0) ? bcvRate * 1.005 : 0.0);
  calcComBancoPct = 0.0;
  calcComPasarelaPct = 0.0;
  calcAplicaP2P = false;
  calcComP2PPct = 0.0;
  drawCalculatorScreen();
}

void closeCalculator() {
  isCalcOpen = false;
  drawUI();
}

void calcularResultadosArbitraje() {
  if (calcMode == 0) {
    // Modo: Arranco con USDT (ej: 500 USDT)
    calcUsdtInicial = calcMontoEntrada;
    calcMontoBs = calcUsdtInicial * calcTasaVenta; // Calcula los bolívares obtenidos
  } else {
    // Modo: Arranco con Bolívares (ej: 480,000 Bs)
    calcMontoBs = calcMontoEntrada;
    if (calcTasaVenta > 0.0) {
      calcUsdtInicial = calcMontoBs / calcTasaVenta;
    } else {
      calcUsdtInicial = 0.0;
    }
  }

  if (calcTasaInterv > 0.0) {
    calcUsdBruto = calcMontoBs / calcTasaInterv;
  } else {
    calcUsdBruto = 0.0;
  }

  // Descuento comisión banco
  calcUsdTrasBanco = calcUsdBruto * (1.0 - (calcComBancoPct / 100.0));

  // Descuento comisión pasarela (BPAY, Wally, etc.)
  calcUsdPasarela = calcUsdTrasBanco * (1.0 - (calcComPasarelaPct / 100.0));

  // Descuento comisión P2P si aplica (ej. Wally a P2P)
  if (calcAplicaP2P) {
    calcUsdFinal = calcUsdPasarela * (1.0 - (calcComP2PPct / 100.0));
  } else {
    calcUsdFinal = calcUsdPasarela;
  }

  // Ganancia neta
  calcGananciaUsd = calcUsdFinal - calcUsdtInicial;
  calcGananciaBs = calcGananciaUsd * calcTasaVenta;

  if (calcUsdtInicial > 0.0) {
    calcRoiPct = (calcGananciaUsd / calcUsdtInicial) * 100.0;
  } else {
    calcRoiPct = 0.0;
  }
}

void drawCalcKeypad(const char* promptTitle, const char* unitSuffix) {
  tft.fillRoundRect(8, 4, 304, 232, 8, COLOR_CARD_BG);
  tft.drawRoundRect(8, 4, 304, 232, 8, COLOR_CARD_BORDER);

  // Barra de título del paso
  tft.setTextColor(COLOR_BINANCE_YEL, COLOR_CARD_BG);
  tft.drawCentreString("CALCULADORA DE ARBITRAJE", 160, 8, 2);

  // Pregunta del paso
  tft.setTextColor(COLOR_CYAN, COLOR_CARD_BG);
  tft.drawCentreString(promptTitle, 160, 24, 2);

  // Cajetín de entrada de datos
  tft.fillRoundRect(24, 42, 272, 26, 4, tft.color565(0x10, 0x14, 0x1A));
  tft.drawRoundRect(24, 42, 272, 26, 4, COLOR_BINANCE_YEL);

  String disp = calcInputStr;
  if (disp.length() == 0) disp = "0";
  disp += " " + String(unitSuffix);
  tft.setTextColor(COLOR_TEXT_WHITE, tft.color565(0x10, 0x14, 0x1A));
  tft.drawCentreString(disp, 160, 46, 2);

  // Teclado con número y punto decimal
  const char* KEYS[4][3] = {
    {"1", "2", "3"},
    {"4", "5", "6"},
    {"7", "8", "9"},
    {".", "0", "OK"}
  };

  for (int r = 0; r < 4; r++) {
    for (int c = 0; c < 3; c++) {
      int bx = 28 + c * 90;
      int by = 73 + r * 33;
      
      uint16_t bColor = COLOR_CHIP_BG;
      uint16_t tColor = COLOR_TEXT_WHITE;
      if (r == 3 && c == 0) { bColor = tft.color565(0x28, 0x36, 0x48); tColor = COLOR_CYAN; } // Punto .
      if (r == 3 && c == 2) { bColor = COLOR_BUY_GREEN; tColor = TFT_WHITE; } // OK

      tft.fillRoundRect(bx, by, 84, 29, 4, bColor);
      tft.drawRoundRect(bx, by, 84, 29, 4, COLOR_CARD_BORDER);

      tft.setTextColor(tColor, bColor);
      tft.drawCentreString(KEYS[r][c], bx + 42, by + 5, 4);
    }
  }

  // Fila de botones inferiores: [BORRAR] y [CANCELAR]
  tft.fillRoundRect(28, 207, 126, 22, 4, tft.color565(0x40, 0x22, 0x26));
  tft.drawRoundRect(28, 207, 126, 22, 4, COLOR_SELL_RED);
  tft.setTextColor(COLOR_SELL_RED, tft.color565(0x40, 0x22, 0x26));
  tft.drawCentreString("Borrar <", 91, 210, 2);

  tft.fillRoundRect(166, 207, 126, 22, 4, tft.color565(0x2E, 0x36, 0x42));
  tft.drawRoundRect(166, 207, 126, 22, 4, COLOR_CARD_BORDER);
  tft.setTextColor(COLOR_TEXT_GRAY, tft.color565(0x2E, 0x36, 0x42));
  tft.drawCentreString("Salir", 229, 210, 2);
}

void drawCalculatorScreen() {
  if (calcStep == -1) {
    // PANTALLA SELECTOR INICIAL: ¿Con qué moneda vas a arrancar?
    tft.fillRoundRect(8, 4, 304, 232, 8, COLOR_CARD_BG);
    tft.drawRoundRect(8, 4, 304, 232, 8, COLOR_CARD_BORDER);

    tft.setTextColor(COLOR_BINANCE_YEL, COLOR_CARD_BG);
    tft.drawCentreString("CALCULADORA DE ARBITRAJE", 160, 14, 2);

    tft.setTextColor(COLOR_TEXT_WHITE, COLOR_CARD_BG);
    tft.drawCentreString("Con que moneda vas a arrancar?", 160, 38, 2);

    // Opción 1: Tengo Dólares / USDT (ej: $500)
    tft.fillRoundRect(22, 68, 132, 108, 8, tft.color565(0x18, 0x34, 0x28));
    tft.drawRoundRect(22, 68, 132, 108, 8, COLOR_BUY_GREEN);
    tft.setTextColor(COLOR_BUY_GREEN, tft.color565(0x18, 0x34, 0x28));
    tft.drawCentreString("[ $ ] USDT", 88, 80, 4);
    tft.setTextColor(COLOR_TEXT_WHITE, tft.color565(0x18, 0x34, 0x28));
    tft.drawCentreString("Tengo Dolares", 88, 116, 2);
    tft.setTextColor(COLOR_TEXT_GRAY, tft.color565(0x18, 0x34, 0x28));
    tft.drawCentreString("(ej: 500 USDT)", 88, 142, 1);

    // Opción 2: Tengo Bolívares / VES (ej: 480,000 Bs)
    tft.fillRoundRect(166, 68, 132, 108, 8, tft.color565(0x1A, 0x2E, 0x48));
    tft.drawRoundRect(166, 68, 132, 108, 8, COLOR_CYAN);
    tft.setTextColor(COLOR_CYAN, tft.color565(0x1A, 0x2E, 0x48));
    tft.drawCentreString("[ Bs ] VES", 232, 80, 4);
    tft.setTextColor(COLOR_TEXT_WHITE, tft.color565(0x1A, 0x2E, 0x48));
    tft.drawCentreString("Tengo Bolivares", 232, 116, 2);
    tft.setTextColor(COLOR_TEXT_GRAY, tft.color565(0x1A, 0x2E, 0x48));
    tft.drawCentreString("(ej: 480,000 Bs)", 232, 142, 1);

    // Botón Salir
    tft.fillRoundRect(70, 192, 180, 28, 4, tft.color565(0x2E, 0x36, 0x42));
    tft.drawRoundRect(70, 192, 180, 28, 4, COLOR_CARD_BORDER);
    tft.setTextColor(COLOR_TEXT_GRAY, tft.color565(0x2E, 0x36, 0x42));
    tft.drawCentreString("Cerrar / Volver", 160, 198, 2);

  } else if (calcStep == 0) {
    if (calcMode == 0) {
      drawCalcKeypad("1. Monto a vender (USDT):", "USDT");
    } else {
      drawCalcKeypad("1. Monto en Bolivares (Bs):", "Bs");
    }
  } else if (calcStep == 1) {
    if (calcInputStr.length() == 0) {
      if (p2pMutex != NULL && xSemaphoreTake(p2pMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
        if (adsCount > 0) {
          calcInputStr = adsList[0].price; // Precargar sugerencia en vivo
        }
        xSemaphoreGive(p2pMutex);
      }
    }
    drawCalcKeypad("2. Tasa venta USDT:", "Bs/USDT");
  } else if (calcStep == 2) {
    if (calcInputStr.length() == 0 && calcTasaInterv > 0.0) {
      calcInputStr = String(calcTasaInterv, 2); // Precargar Intervención calculada
    }
    drawCalcKeypad("3. Tasa Intervencion:", "Bs/$");
  } else if (calcStep == 3) {
    drawCalcKeypad("4. Comision Banco (%):", "%");
  } else if (calcStep == 4) {
    drawCalcKeypad("5. Comision Pasarela (%):", "%");
  } else if (calcStep == 5) {
    // Pregunta: ¿Aplica P2P? (Botones grandes [NO] y [SI])
    tft.fillRoundRect(8, 4, 304, 232, 8, COLOR_CARD_BG);
    tft.drawRoundRect(8, 4, 304, 232, 8, COLOR_CARD_BORDER);

    tft.setTextColor(COLOR_BINANCE_YEL, COLOR_CARD_BG);
    tft.drawCentreString("CALCULADORA DE ARBITRAJE", 160, 12, 2);

    tft.setTextColor(COLOR_TEXT_WHITE, COLOR_CARD_BG);
    tft.drawCentreString("6. Aplica venta P2P?", 160, 42, 4);

    tft.setTextColor(COLOR_TEXT_GRAY, COLOR_CARD_BG);
    tft.drawCentreString("BPAY = No  |  Wally/Zinli = Si", 160, 75, 2);

    // Botón NO (ej: BPAY entra en USD)
    tft.fillRoundRect(30, 105, 120, 60, 8, tft.color565(0x20, 0x36, 0x4E));
    tft.drawRoundRect(30, 105, 120, 60, 8, COLOR_CYAN);
    tft.setTextColor(COLOR_CYAN, tft.color565(0x20, 0x36, 0x4E));
    tft.drawCentreString("NO", 90, 118, 4);
    tft.setTextColor(COLOR_TEXT_GRAY, tft.color565(0x20, 0x36, 0x4E));
    tft.drawCentreString("(Pasan USD directos)", 90, 146, 1);

    // Botón SÍ (ej: Wally/Zinli a vender en P2P)
    tft.fillRoundRect(170, 105, 120, 60, 8, tft.color565(0x3E, 0x34, 0x14));
    tft.drawRoundRect(170, 105, 120, 60, 8, COLOR_BINANCE_YEL);
    tft.setTextColor(COLOR_BINANCE_YEL, tft.color565(0x3E, 0x34, 0x14));
    tft.drawCentreString("SI", 230, 118, 4);
    tft.setTextColor(COLOR_TEXT_GRAY, tft.color565(0x3E, 0x34, 0x14));
    tft.drawCentreString("(Vender saldo en P2P)", 230, 146, 1);

    // Botón Cancelar
    tft.fillRoundRect(70, 192, 180, 28, 4, tft.color565(0x2E, 0x36, 0x42));
    tft.drawRoundRect(70, 192, 180, 28, 4, COLOR_CARD_BORDER);
    tft.setTextColor(COLOR_TEXT_GRAY, tft.color565(0x2E, 0x36, 0x42));
    tft.drawCentreString("Cancelar y Salir", 160, 198, 2);

  } else if (calcStep == 6) {
    drawCalcKeypad("7. Comision / Descuento P2P (%):", "%");
  } else if (calcStep == 7) {
    // PANTALLA FINAL: TICKET DE RENDIMIENTO FINANCIERO
    calcularResultadosArbitraje();

    tft.fillRoundRect(8, 4, 304, 232, 8, COLOR_CARD_BG);
    tft.drawRoundRect(8, 4, 304, 232, 8, COLOR_CARD_BORDER);

    // Encabezado
    tft.fillRoundRect(14, 8, 292, 22, 4, tft.color565(0x18, 0x22, 0x30));
    tft.setTextColor(COLOR_BINANCE_YEL, tft.color565(0x18, 0x22, 0x30));
    tft.drawCentreString("RESULTADO DE LA OPERACION", 160, 11, 2);

    // Fila 1: Inversión inicial y USDT iniciales
    tft.setTextColor(COLOR_TEXT_GRAY, COLOR_CARD_BG);
    tft.drawString("Capital:", 16, 35, 2);
    tft.setTextColor(COLOR_TEXT_WHITE, COLOR_CARD_BG);
    tft.drawRightString(String(calcMontoBs, 0) + " Bs (" + String(calcUsdtInicial, 2) + " USDT)", 302, 35, 2);

    // Fila 2: Tasa Venta y Tasa Intervención
    tft.setTextColor(COLOR_TEXT_GRAY, COLOR_CARD_BG);
    tft.drawString("Tasas (Vta / Int):", 16, 55, 2);
    tft.setTextColor(COLOR_CYAN, COLOR_CARD_BG);
    tft.drawRightString(String(calcTasaVenta, 1) + " / " + String(calcTasaInterv, 2), 302, 55, 2);

    // Fila 3: Total Dólares Finales
    tft.setTextColor(COLOR_TEXT_GRAY, COLOR_CARD_BG);
    tft.drawString("Dolares Finales:", 16, 75, 2);
    tft.setTextColor(COLOR_TEXT_WHITE, COLOR_CARD_BG);
    tft.drawRightString("$ " + String(calcUsdFinal, 2) + " USD", 302, 75, 2);

    // Línea divisora
    tft.drawFastHLine(16, 96, 288, COLOR_CARD_BORDER);

    // CAJA DESTACADA DE GANANCIA NETA
    bool esGanancia = (calcGananciaUsd >= 0.0);
    uint16_t boxColor = esGanancia ? tft.color565(0x10, 0x2A, 0x1E) : tft.color565(0x35, 0x14, 0x18);
    uint16_t borderColor = esGanancia ? COLOR_BUY_GREEN : COLOR_SELL_RED;
    tft.fillRoundRect(14, 102, 292, 60, 6, boxColor);
    tft.drawRoundRect(14, 102, 292, 60, 6, borderColor);

    tft.setTextColor(COLOR_TEXT_WHITE, boxColor);
    tft.drawString("GANANCIA NETA:", 22, 108, 2);

    tft.setTextColor(borderColor, boxColor);
    String sign = esGanancia ? "+" : "";
    tft.drawRightString(sign + "$" + String(calcGananciaUsd, 2) + " USD", 298, 106, 4);

    tft.setTextColor(COLOR_BINANCE_YEL, boxColor);
    tft.drawString(sign + String(calcGananciaBs, 2) + " Bs", 22, 134, 2);

    tft.setTextColor(borderColor, boxColor);
    tft.drawRightString("ROI: " + sign + String(calcRoiPct, 2) + "%", 298, 134, 2);

    // Botones de acción inferior: [NUEVO CALCULO] y [CERRAR]
    tft.fillRoundRect(16, 172, 140, 32, 4, COLOR_BUY_GREEN);
    tft.setTextColor(TFT_WHITE, COLOR_BUY_GREEN);
    tft.drawCentreString("NUEVO CALCULO", 86, 180, 2);

    tft.fillRoundRect(164, 172, 142, 32, 4, tft.color565(0x2E, 0x36, 0x42));
    tft.drawRoundRect(164, 172, 142, 32, 4, COLOR_CARD_BORDER);
    tft.setTextColor(COLOR_TEXT_GRAY, tft.color565(0x2E, 0x36, 0x42));
    tft.drawCentreString("CERRAR", 235, 180, 2);

    tft.setTextColor(COLOR_TEXT_GRAY, COLOR_CARD_BG);
    tft.drawCentreString(calcAplicaP2P ? "(Con salida por P2P)" : "(Salida USD directos sin P2P)", 160, 214, 1);
  }
}
void drawKeypadDisplayBox() {
  tft.fillRoundRect(24, 30, 272, 28, 4, tft.color565(0x10, 0x14, 0x1A));
  tft.drawRoundRect(24, 30, 272, 28, 4, COLOR_BINANCE_YEL);
  
  if (keypadInput.length() == 0) {
    tft.setTextColor(COLOR_TEXT_GRAY, tft.color565(0x10, 0x14, 0x1A));
    tft.drawCentreString("Todos los montos (0)", 160, 36, 2);
  } else {
    tft.setTextColor(COLOR_TEXT_WHITE, tft.color565(0x10, 0x14, 0x1A));
    tft.drawCentreString(keypadInput + " Bs", 160, 34, 4);
  }
}

void drawKeypad() {
  tft.fillRoundRect(12, 6, 296, 228, 8, COLOR_CARD_BG);
  tft.drawRoundRect(12, 6, 296, 228, 8, COLOR_CARD_BORDER);

  tft.setTextColor(COLOR_BINANCE_YEL, COLOR_CARD_BG);
  tft.drawCentreString("FILTRAR POR MONTO (VES)", 160, 10, 2);

  drawKeypadDisplayBox();

  const char* KEYS[4][3] = {
    {"1", "2", "3"},
    {"4", "5", "6"},
    {"7", "8", "9"},
    {"<", "0", "OK"}
  };

  for (int r = 0; r < 4; r++) {
    for (int c = 0; c < 3; c++) {
      int bx = 28 + c * 90;
      int by = 64 + r * 35;
      
      uint16_t bColor = COLOR_CHIP_BG;
      uint16_t tColor = COLOR_TEXT_WHITE;
      if (r == 3 && c == 0) { bColor = tft.color565(0x40, 0x22, 0x26); tColor = COLOR_SELL_RED; } // Borrar
      if (r == 3 && c == 2) { bColor = COLOR_BUY_GREEN; tColor = TFT_WHITE; } // OK

      tft.fillRoundRect(bx, by, 84, 31, 5, bColor);
      tft.drawRoundRect(bx, by, 84, 31, 5, COLOR_CARD_BORDER);

      tft.setTextColor(tColor, bColor);
      tft.drawCentreString(KEYS[r][c], bx + 42, by + 5, 4);
    }
  }

  tft.fillRoundRect(28, 206, 264, 22, 4, tft.color565(0x2E, 0x36, 0x42));
  tft.setTextColor(COLOR_TEXT_GRAY, tft.color565(0x2E, 0x36, 0x42));
  tft.drawCentreString("Restablecer a Todo / Cerrar", 160, 209, 2);
}

// -----------------------------------------------------------------------------
// VENTANA MODAL 2: MENÚ DESPLEGABLE DE BANCOS CON PAGINACIÓN (5 por página en horizontal)
// -----------------------------------------------------------------------------
void drawBankModal() {
  tft.fillRoundRect(16, 6, 288, 228, 8, COLOR_CARD_BG);
  tft.drawRoundRect(16, 6, 288, 228, 8, COLOR_CARD_BORDER);

  int totalPages = (TOTAL_BANKS + 4) / 5;
  tft.setTextColor(COLOR_BINANCE_YEL, COLOR_CARD_BG);
  String title = "METODOS DE PAGO (" + String(bankModalPage + 1) + "/" + String(totalPages) + ")";
  tft.drawCentreString(title, 160, 10, 2);

  int startIdx = bankModalPage * 5;
  for (int i = 0; i < 5; i++) {
    int bankIdx = startIdx + i;
    if (bankIdx >= TOTAL_BANKS) break;

    int by = 30 + i * 33;
    bool isSelected = (bankIdx == selectedBankIdx);

    uint16_t bgCol = isSelected ? tft.color565(0x28, 0x38, 0x4C) : COLOR_CHIP_BG;
    uint16_t borderCol = isSelected ? COLOR_BINANCE_YEL : COLOR_CARD_BORDER;
    uint16_t txtCol = isSelected ? COLOR_BINANCE_YEL : COLOR_TEXT_WHITE;

    tft.fillRoundRect(24, by, 272, 29, 5, bgCol);
    tft.drawRoundRect(24, by, 272, 29, 5, borderCol);

    if (isSelected) {
      tft.setTextColor(COLOR_BINANCE_YEL, bgCol);
      tft.drawString("[*]", 32, by + 7, 2);
    }

    tft.setTextColor(txtCol, bgCol);
    tft.drawString(BANK_TITLES[bankIdx], isSelected ? 58 : 34, by + 7, 2);
  }

  // Botones inferiores: Paginación y Cerrar
  // Botón Paginación (x=24, w=130, h=26)
  tft.fillRoundRect(24, 198, 130, 26, 4, tft.color565(0x24, 0x34, 0x44));
  tft.drawRoundRect(24, 198, 130, 26, 4, COLOR_CARD_BORDER);
  tft.setTextColor(COLOR_BINANCE_YEL, tft.color565(0x24, 0x34, 0x44));
  String pageBtn = (bankModalPage < totalPages - 1) ? "Mas Bancos >" : "< Inicio";
  tft.drawCentreString(pageBtn, 89, 203, 2);

  // Botón Cerrar (x=166, w=130, h=26)
  tft.fillRoundRect(166, 198, 130, 26, 4, tft.color565(0x2E, 0x36, 0x42));
  tft.drawRoundRect(166, 198, 130, 26, 4, COLOR_CARD_BORDER);
  tft.setTextColor(COLOR_TEXT_GRAY, tft.color565(0x2E, 0x36, 0x42));
  tft.drawCentreString("Cerrar", 231, 203, 2);
}

// -----------------------------------------------------------------------------
// VENTANA MODAL: CONFIGURAR ALERTA DE PRECIO (VIGILANCIA PUESTO #1)
// -----------------------------------------------------------------------------
void drawAlertModalDisplayBox() {
  bool isBuy = (currentTradeType == "BUY");
  bool active = isBuy ? isAlertBuyActive : isAlertSellActive;
  uint16_t themeCol = isBuy ? COLOR_BUY_GREEN : COLOR_SELL_RED;

  tft.fillRoundRect(24, 38, 272, 28, 4, tft.color565(0x10, 0x14, 0x1A));
  tft.drawRoundRect(24, 38, 272, 28, 4, active ? themeCol : COLOR_BINANCE_YEL);

  if (alertInputPrice.length() == 0) {
    tft.setTextColor(COLOR_TEXT_GRAY, tft.color565(0x10, 0x14, 0x1A));
    tft.drawCentreString("Meta: 0.00 Bs", 160, 44, 2);
  } else {
    tft.setTextColor(active ? themeCol : COLOR_TEXT_WHITE, tft.color565(0x10, 0x14, 0x1A));
    tft.drawCentreString("Meta: Bs " + alertInputPrice, 160, 42, 4);
  }
}

void drawAlertModal() {
  bool isBuy = (currentTradeType == "BUY");
  bool active = isBuy ? isAlertBuyActive : isAlertSellActive;
  uint16_t themeCol = isBuy ? COLOR_BUY_GREEN : COLOR_SELL_RED;

  tft.fillRoundRect(12, 6, 296, 228, 8, COLOR_CARD_BG);
  tft.drawRoundRect(12, 6, 296, 228, 8, COLOR_CARD_BORDER);

  tft.setTextColor(themeCol, COLOR_CARD_BG);
  tft.drawCentreString(isBuy ? "ALERTA #1: COMPRAR USDT" : "ALERTA #1: VENDER USDT", 160, 10, 2);

  // Resumen de condiciones fijadas en cápsula
  String cond = String(isBuy ? "COMPRA" : "VENTA") + " | " + 
                (currentAmountFilter.length() > 0 ? (currentAmountFilter + " Bs") : String("Todo Monto")) + " | " + 
                String(BANK_CHIP_NAMES[selectedBankIdx]);
  tft.setTextColor(COLOR_CYAN, COLOR_CARD_BG);
  tft.drawCentreString(cond, 160, 26, 1);

  drawAlertModalDisplayBox();

  const char* KEYS[4][3] = {
    {"1", "2", "3"},
    {"4", "5", "6"},
    {"7", "8", "9"},
    {".", "0", "<"}
  };

  for (int r = 0; r < 4; r++) {
    for (int c = 0; c < 3; c++) {
      int bx = 28 + c * 90;
      int by = 70 + r * 30;

      uint16_t bColor = COLOR_CHIP_BG;
      uint16_t tColor = COLOR_TEXT_WHITE;
      if (r == 3 && c == 2) { bColor = tft.color565(0x40, 0x22, 0x26); tColor = COLOR_SELL_RED; } // Borrar

      tft.fillRoundRect(bx, by, 84, 26, 4, bColor);
      tft.drawRoundRect(bx, by, 84, 26, 4, COLOR_CARD_BORDER);

      tft.setTextColor(tColor, bColor);
      tft.drawCentreString(KEYS[r][c], bx + 42, by + 4, 2);
    }
  }

  // Botón ACTIVAR (x=24..154, w=130, h=28, y=194)
  tft.fillRoundRect(24, 194, 130, 28, 4, COLOR_BUY_GREEN);
  tft.setTextColor(TFT_WHITE, COLOR_BUY_GREEN);
  tft.drawCentreString(active ? "ACTUALIZAR" : "ACTIVAR", 89, 200, 2);

  // Botón DESACTIVAR / CERRAR (x=166..296, w=130, h=28, y=194)
  uint16_t offBg = active ? COLOR_SELL_RED : tft.color565(0x28, 0x36, 0x4A);
  tft.fillRoundRect(166, 194, 130, 28, 4, offBg);
  tft.setTextColor(TFT_WHITE, offBg);
  tft.drawCentreString(active ? "DESACTIVAR" : "CERRAR", 231, 200, 2);
}

void drawAlertNotificationBanner() {
  bool isBuy = (alertTriggeredSide == "COMPRA");
  uint16_t bg = isBuy ? COLOR_BUY_GREEN : COLOR_SELL_RED;
  tft.fillRoundRect(6, 25, 308, 44, 6, bg);
  tft.drawRoundRect(6, 25, 308, 44, 6, TFT_WHITE);
  tft.setTextColor(TFT_WHITE, bg);
  tft.drawCentreString("! ALERTA " + alertTriggeredSide + ": PUESTO #1 EN META !", 160, 28, 2);
  tft.setTextColor(COLOR_BINANCE_YEL, bg);
  tft.drawCentreString("Bs " + alertTriggeredPriceStr + " - Toca para silenciar", 160, 48, 1);
}

void drawFullScreenAlert() {
  bool isBuy = (alertTriggeredSide == "COMPRA");
  uint16_t themeColor = isBuy ? COLOR_BUY_GREEN : COLOR_SELL_RED;
  uint16_t boxBg = tft.color565(0x12, 0x18, 0x24);

  // 1. Fondo completo oscuro y doble borde vibrante
  tft.fillRect(0, 0, 320, 240, COLOR_BG);
  tft.drawRoundRect(4, 4, 312, 232, 8, themeColor);
  tft.drawRoundRect(5, 5, 310, 230, 7, themeColor);

  // 2. Encabezado de Alarma (y = 10..34)
  tft.fillRoundRect(10, 10, 300, 24, 6, themeColor);
  tft.setTextColor(TFT_WHITE, themeColor);
  String title = isBuy ? "! ALERTA P2P: COMPRAR USDT !" : "! ALERTA P2P: VENDER USDT !";
  tft.drawCentreString(title, 160, 14, 2);

  // 3. Bloque de Precio y Meta (y = 38..84)
  tft.fillRoundRect(10, 38, 300, 48, 6, boxBg);
  tft.drawRoundRect(10, 38, 300, 48, 6, tft.color565(0x28, 0x36, 0x4A));

  tft.setTextColor(themeColor, boxBg);
  tft.drawCentreString("Bs " + alertTriggeredPriceStr, 160, 42, 4);

  tft.setTextColor(COLOR_BINANCE_YEL, boxBg);
  tft.drawCentreString("Meta fijada: Bs " + alertTriggeredTargetStr + " (PUESTO #1)", 160, 70, 1);

  // 4. Tarjeta de Detalles del Comerciante (y = 90..182) - Formato Telegram
  tft.fillRoundRect(10, 90, 300, 92, 6, COLOR_CARD_BG);
  tft.drawRoundRect(10, 90, 300, 92, 6, COLOR_CARD_BORDER);

  // Comerciante
  tft.setTextColor(COLOR_TEXT_GRAY, COLOR_CARD_BG);
  tft.drawString("Comerciante:", 18, 97, 1);
  tft.setTextColor(COLOR_TEXT_WHITE, COLOR_CARD_BG);
  String traderDisp = alertTriggeredTrader;
  if (traderDisp.length() > 20) traderDisp = traderDisp.substring(0, 19) + "..";
  tft.drawString(traderDisp, 105, 95, 2);

  // Calificación / Órdenes
  tft.setTextColor(COLOR_TEXT_GRAY, COLOR_CARD_BG);
  tft.drawString("Efectividad:", 18, 118, 1);
  tft.setTextColor(COLOR_BUY_GREEN, COLOR_CARD_BG);
  tft.drawString(alertTriggeredOrdersStr, 105, 118, 1);

  // Banco / Método
  tft.setTextColor(COLOR_TEXT_GRAY, COLOR_CARD_BG);
  tft.drawString("Metodo Pago:", 18, 138, 1);
  tft.setTextColor(COLOR_CYAN, COLOR_CARD_BG);
  String bankDisp = alertTriggeredBankStr;
  if (bankDisp.length() > 28) bankDisp = bankDisp.substring(0, 27) + "..";
  tft.drawString(bankDisp, 105, 138, 1);

  // Cripto disponible
  tft.setTextColor(COLOR_TEXT_GRAY, COLOR_CARD_BG);
  tft.drawString("Disponible:", 18, 158, 1);
  tft.setTextColor(COLOR_BINANCE_YEL, COLOR_CARD_BG);
  tft.drawString(alertTriggeredCryptoStr + " USDT", 105, 158, 1);

  // 5. Botón Inferior para Descartar y temporizador (y = 188..234)
  tft.fillRoundRect(20, 188, 280, 26, 4, tft.color565(0x20, 0x2A, 0x38));
  tft.drawRoundRect(20, 188, 280, 26, 4, COLOR_CARD_BORDER);
  tft.setTextColor(TFT_WHITE, tft.color565(0x20, 0x2A, 0x38));
  tft.drawCentreString("TOCA LA PANTALLA PARA CERRAR", 160, 193, 2);

  tft.setTextColor(COLOR_TEXT_GRAY, COLOR_BG);
  tft.drawCentreString("Auto-cierre en 2 min | Meta cumplida y desactivada", 160, 222, 1);
}

void drawBottomBar() {
  tft.fillRect(0, 216, 320, 24, COLOR_HEADER_BG);
  tft.drawFastHLine(0, 216, 320, COLOR_CARD_BORDER);

  int totalPages = 1;
  int currentAdsCount = 0;
  if (p2pMutex != NULL && xSemaphoreTake(p2pMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
    currentAdsCount = adsCount;
    totalPages = (adsCount > 0) ? ((adsCount + 1) / 2) : 1;
    xSemaphoreGive(p2pMutex);
  }
  bool canUp = (currentCardPage > 0);
  bool canDown = (currentCardPage < totalPages - 1);

  // 1. Botón Subir (x=6..102, w=96, h=20)
  uint16_t upBg = canUp ? tft.color565(0x20, 0x32, 0x48) : tft.color565(0x16, 0x1B, 0x22);
  uint16_t upTxt = canUp ? COLOR_CYAN : tft.color565(0x40, 0x48, 0x54);
  tft.fillRoundRect(6, 218, 96, 20, 4, upBg);
  tft.drawRoundRect(6, 218, 96, 20, 4, canUp ? COLOR_CYAN : tft.color565(0x28, 0x30, 0x3A));
  // Flecha arriba ▲
  tft.fillTriangle(18, 230, 23, 223, 28, 230, upTxt);
  tft.setTextColor(upTxt, upBg);
  tft.drawString("Subir", 34, 222, 1);

  // 2. Indicador central: Alternar o mostrar BCV y página de forma compacta
  tft.fillRect(104, 217, 112, 22, COLOR_HEADER_BG);
  if (bcvRate > 0.0) {
    tft.setTextColor(COLOR_CYAN, COLOR_HEADER_BG);
    tft.drawString("BCV:" + String(bcvRate, 2), 106, 222, 1);
    tft.setTextColor(COLOR_TEXT_GRAY, COLOR_HEADER_BG);
    tft.drawString(" | P" + String(currentCardPage + 1) + "/" + String(totalPages), 172, 222, 1);
  } else {
    String posStr = "Pag " + String(currentCardPage + 1) + "/" + String(totalPages) + " (" + String(currentAdsCount) + ")";
    tft.setTextColor(COLOR_TEXT_GRAY, COLOR_HEADER_BG);
    tft.drawCentreString(posStr, 160, 222, 1);
  }

  // 3. Botón Bajar (x=218..314, w=96, h=20)
  uint16_t dnBg = canDown ? tft.color565(0x20, 0x32, 0x48) : tft.color565(0x16, 0x1B, 0x22);
  uint16_t dnTxt = canDown ? COLOR_BINANCE_YEL : tft.color565(0x40, 0x48, 0x54);
  tft.fillRoundRect(218, 218, 96, 20, 4, dnBg);
  tft.drawRoundRect(218, 218, 96, 20, 4, canDown ? COLOR_BINANCE_YEL : tft.color565(0x28, 0x30, 0x3A));
  tft.setTextColor(dnTxt, dnBg);
  tft.drawString("Bajar", 238, 222, 1);
  // Flecha abajo ▼
  tft.fillTriangle(292, 224, 297, 231, 302, 224, dnTxt);
}

// -----------------------------------------------------------------------------
// VENTANA MODAL 3: GESTOR DE REDES WIFI (Lista con escaneo y señales)
// -----------------------------------------------------------------------------
const char KB_R0_LOWER[10] = {'1','2','3','4','5','6','7','8','9','0'};
const char KB_R1_LOWER[10] = {'q','w','e','r','t','y','u','i','o','p'};
const char KB_R2_LOWER[10] = {'a','s','d','f','g','h','j','k','l','-'};
const char KB_R3_LOWER[7]  = {'z','x','c','v','b','n','m'};

const char KB_R0_UPPER[10] = {'1','2','3','4','5','6','7','8','9','0'};
const char KB_R1_UPPER[10] = {'Q','W','E','R','T','Y','U','I','O','P'};
const char KB_R2_UPPER[10] = {'A','S','D','F','G','H','J','K','L','-'};
const char KB_R3_UPPER[7]  = {'Z','X','C','V','B','N','M'};

const char KB_R0_SYM[10]   = {'!','@','#','$','%','^','&','*','(',')'};
const char KB_R1_SYM[10]   = {'+','=','{','}','[',']','<','>','/','\\'};
const char KB_R2_SYM[10]   = {';',':','\'','"','`','~','|','?','!','_'};
const char KB_R3_SYM[7]    = {'1','2','3','4','5','6','7'};

void drawWifiListModal() {
  tft.fillRoundRect(16, 6, 288, 228, 8, COLOR_CARD_BG);
  tft.drawRoundRect(16, 6, 288, 228, 8, COLOR_CARD_BORDER);

  int totalPages = (scannedNetworksCount > 0) ? ((scannedNetworksCount + 3) / 4) : 1;
  tft.setTextColor(COLOR_BINANCE_YEL, COLOR_CARD_BG);
  String title = "REDES WIFI (" + String(wifiListPage + 1) + "/" + String(totalPages) + ")";
  tft.drawCentreString(title, 160, 10, 2);

  int startIdx = wifiListPage * 4;
  if (scannedNetworksCount == 0) {
    tft.setTextColor(COLOR_TEXT_GRAY, COLOR_CARD_BG);
    tft.drawCentreString("No se encontraron redes", 160, 90, 2);
    tft.drawCentreString("Toca [Buscar] para reintentar", 160, 115, 2);
  } else {
    for (int i = 0; i < 4; i++) {
      int idx = startIdx + i;
      if (idx >= scannedNetworksCount) break;

      int by = 32 + i * 40;
      bool isCurrent = (WiFi.status() == WL_CONNECTED && WiFi.SSID() == scannedNets[idx].ssid);

      uint16_t bgCol = isCurrent ? tft.color565(0x1C, 0x32, 0x48) : COLOR_CHIP_BG;
      uint16_t borderCol = isCurrent ? COLOR_CYAN : COLOR_CARD_BORDER;

      tft.fillRoundRect(24, by, 272, 36, 5, bgCol);
      tft.drawRoundRect(24, by, 272, 36, 5, borderCol);

      String dispSSID = scannedNets[idx].ssid;
      if (dispSSID.length() > 22) dispSSID = dispSSID.substring(0, 21) + "..";

      tft.setTextColor(COLOR_TEXT_WHITE, bgCol);
      tft.drawString(dispSSID, 34, by + 5, 2);

      if (isCurrent) {
        tft.setTextColor(COLOR_BUY_GREEN, bgCol);
        tft.drawString("[CONECTADO]", 34, by + 21, 1);
      } else if (scannedNets[idx].isKnown) {
        tft.setTextColor(COLOR_CYAN, bgCol);
        tft.drawString("[GUARDADA]", 34, by + 21, 1);
      } else {
        tft.setTextColor(COLOR_TEXT_GRAY, bgCol);
        tft.drawString("Tocar para conectar", 34, by + 21, 1);
      }

      int bars = 1;
      if (scannedNets[idx].rssi >= -65) bars = 4;
      else if (scannedNets[idx].rssi >= -75) bars = 3;
      else if (scannedNets[idx].rssi >= -85) bars = 2;

      for (int b = 0; b < 4; b++) {
        int bh = 3 + b * 2;
        int bx = 264 + b * 4;
        int sby = by + 25 - bh;
        uint16_t bCol = (b < bars) ? COLOR_BUY_GREEN : tft.color565(0x40, 0x48, 0x54);
        tft.fillRect(bx, sby, 2, bh, bCol);
      }
    }
  }

  // Fila inferior de botones (y = 196..224, h=28)
  tft.fillRoundRect(24, 196, 84, 28, 4, tft.color565(0x24, 0x34, 0x44));
  tft.drawRoundRect(24, 196, 84, 28, 4, COLOR_CARD_BORDER);
  tft.setTextColor(COLOR_CYAN, tft.color565(0x24, 0x34, 0x44));
  tft.drawCentreString("Buscar", 66, 202, 2);

  tft.fillRoundRect(118, 196, 84, 28, 4, tft.color565(0x24, 0x34, 0x44));
  tft.drawRoundRect(118, 196, 84, 28, 4, COLOR_CARD_BORDER);
  tft.setTextColor(COLOR_BINANCE_YEL, tft.color565(0x24, 0x34, 0x44));
  tft.drawCentreString("Mas >", 160, 202, 2);

  tft.fillRoundRect(212, 196, 84, 28, 4, tft.color565(0x2E, 0x36, 0x42));
  tft.drawRoundRect(212, 196, 84, 28, 4, COLOR_CARD_BORDER);
  tft.setTextColor(COLOR_TEXT_GRAY, tft.color565(0x2E, 0x36, 0x42));
  tft.drawCentreString("Cerrar", 254, 202, 2);
}

void drawWifiKeyboardDisplayBox() {
  tft.fillRoundRect(16, 38, 236, 26, 4, tft.color565(0x10, 0x14, 0x1A));
  tft.drawRoundRect(16, 38, 236, 26, 4, COLOR_CYAN);

  String displayTxt = "";
  if (wifiPassVisible) {
    displayTxt = wifiPasswordInput;
  } else {
    for (int i = 0; i < wifiPasswordInput.length(); i++) {
      displayTxt += "*";
    }
  }

  if (displayTxt.length() > 22) {
    displayTxt = displayTxt.substring(displayTxt.length() - 22);
  }

  tft.setTextColor(COLOR_TEXT_WHITE, tft.color565(0x10, 0x14, 0x1A));
  if (displayTxt.length() == 0) {
    tft.setTextColor(COLOR_TEXT_GRAY, tft.color565(0x10, 0x14, 0x1A));
    tft.drawString("Ingresa clave...", 24, 43, 2);
  } else {
    tft.drawString(displayTxt + "|", 24, 43, 2);
  }

  uint16_t eyeBg = wifiPassVisible ? tft.color565(0x1C, 0x32, 0x48) : tft.color565(0x22, 0x2A, 0x36);
  tft.fillRoundRect(258, 38, 48, 26, 4, eyeBg);
  tft.drawRoundRect(258, 38, 48, 26, 4, COLOR_CARD_BORDER);
  tft.setTextColor(wifiPassVisible ? COLOR_CYAN : COLOR_TEXT_GRAY, eyeBg);
  tft.drawCentreString(wifiPassVisible ? "VER" : "OCU", 282, 43, 2);
}

void drawWifiKeyboard() {
  tft.fillRoundRect(6, 4, 308, 232, 8, COLOR_CARD_BG);
  tft.drawRoundRect(6, 4, 308, 232, 8, COLOR_CARD_BORDER);

  tft.setTextColor(COLOR_BINANCE_YEL, COLOR_CARD_BG);
  tft.drawCentreString("CONECTAR A WIFI", 160, 8, 2);

  tft.setTextColor(COLOR_TEXT_WHITE, COLOR_CARD_BG);
  String netHeader = "Red: " + selectedWifiSSID;
  if (netHeader.length() > 28) netHeader = netHeader.substring(0, 27) + "..";
  tft.drawCentreString(netHeader, 160, 23, 2);

  drawWifiKeyboardDisplayBox();

  // Fila 0 (Números/Símbolos)
  for (int c = 0; c < 10; c++) {
    int kx = 16 + c * 29;
    char ch = (wifiKbMode == 2) ? KB_R0_SYM[c] : KB_R0_LOWER[c];
    tft.fillRoundRect(kx, 68, 27, 26, 3, COLOR_CHIP_BG);
    tft.drawRoundRect(kx, 68, 27, 26, 3, COLOR_CARD_BORDER);
    tft.setTextColor(COLOR_TEXT_WHITE, COLOR_CHIP_BG);
    tft.drawCentreString(String(ch), kx + 13, 73, 2);
  }

  // Fila 1
  for (int c = 0; c < 10; c++) {
    int kx = 16 + c * 29;
    char ch = (wifiKbMode == 2) ? KB_R1_SYM[c] : ((wifiKbMode == 1) ? KB_R1_UPPER[c] : KB_R1_LOWER[c]);
    tft.fillRoundRect(kx, 97, 27, 26, 3, COLOR_CHIP_BG);
    tft.drawRoundRect(kx, 97, 27, 26, 3, COLOR_CARD_BORDER);
    tft.setTextColor(COLOR_TEXT_WHITE, COLOR_CHIP_BG);
    tft.drawCentreString(String(ch), kx + 13, 102, 2);
  }

  // Fila 2
  for (int c = 0; c < 10; c++) {
    int kx = 16 + c * 29;
    char ch = (wifiKbMode == 2) ? KB_R2_SYM[c] : ((wifiKbMode == 1) ? KB_R2_UPPER[c] : KB_R2_LOWER[c]);
    tft.fillRoundRect(kx, 126, 27, 26, 3, COLOR_CHIP_BG);
    tft.drawRoundRect(kx, 126, 27, 26, 3, COLOR_CARD_BORDER);
    tft.setTextColor(COLOR_TEXT_WHITE, COLOR_CHIP_BG);
    tft.drawCentreString(String(ch), kx + 13, 131, 2);
  }

  // Fila 3: Shift [A/a] + 7 letras + DEL
  tft.fillRoundRect(16, 155, 38, 26, 3, (wifiKbMode == 1) ? COLOR_BINANCE_YEL : tft.color565(0x28, 0x34, 0x44));
  tft.drawRoundRect(16, 155, 38, 26, 3, COLOR_CARD_BORDER);
  tft.setTextColor((wifiKbMode == 1) ? COLOR_HEADER_BG : COLOR_TEXT_WHITE, (wifiKbMode == 1) ? COLOR_BINANCE_YEL : tft.color565(0x28, 0x34, 0x44));
  tft.drawCentreString("A/a", 35, 160, 2);

  for (int c = 0; c < 7; c++) {
    int kx = 58 + c * 29;
    char ch = (wifiKbMode == 2) ? KB_R3_SYM[c] : ((wifiKbMode == 1) ? KB_R3_UPPER[c] : KB_R3_LOWER[c]);
    tft.fillRoundRect(kx, 155, 27, 26, 3, COLOR_CHIP_BG);
    tft.drawRoundRect(kx, 155, 27, 26, 3, COLOR_CARD_BORDER);
    tft.setTextColor(COLOR_TEXT_WHITE, COLOR_CHIP_BG);
    tft.drawCentreString(String(ch), kx + 13, 160, 2);
  }

  tft.fillRoundRect(265, 155, 41, 26, 3, tft.color565(0x40, 0x22, 0x26));
  tft.drawRoundRect(265, 155, 41, 26, 3, COLOR_SELL_RED);
  tft.setTextColor(COLOR_SELL_RED, tft.color565(0x40, 0x22, 0x26));
  tft.drawCentreString("DEL", 285, 160, 2);

  // Fila 4: [?123/abc] [.] [ESPACIO] [_] [@] [-]
  tft.fillRoundRect(16, 184, 48, 24, 3, tft.color565(0x28, 0x34, 0x44));
  tft.drawRoundRect(16, 184, 48, 24, 3, COLOR_CARD_BORDER);
  tft.setTextColor(COLOR_CYAN, tft.color565(0x28, 0x34, 0x44));
  tft.drawCentreString((wifiKbMode == 2) ? "abc" : "?123", 40, 188, 2);

  tft.fillRoundRect(68, 184, 30, 24, 3, COLOR_CHIP_BG);
  tft.drawRoundRect(68, 184, 30, 24, 3, COLOR_CARD_BORDER);
  tft.setTextColor(COLOR_TEXT_WHITE, COLOR_CHIP_BG);
  tft.drawCentreString(".", 83, 188, 2);

  tft.fillRoundRect(102, 184, 116, 24, 3, COLOR_CHIP_BG);
  tft.drawRoundRect(102, 184, 116, 24, 3, COLOR_CARD_BORDER);
  tft.setTextColor(COLOR_TEXT_GRAY, COLOR_CHIP_BG);
  tft.drawCentreString("ESPACIO", 160, 188, 2);

  tft.fillRoundRect(222, 184, 26, 24, 3, COLOR_CHIP_BG);
  tft.drawRoundRect(222, 184, 26, 24, 3, COLOR_CARD_BORDER);
  tft.setTextColor(COLOR_TEXT_WHITE, COLOR_CHIP_BG);
  tft.drawCentreString("_", 235, 188, 2);

  tft.fillRoundRect(252, 184, 26, 24, 3, COLOR_CHIP_BG);
  tft.drawRoundRect(252, 184, 26, 24, 3, COLOR_CARD_BORDER);
  tft.setTextColor(COLOR_TEXT_WHITE, COLOR_CHIP_BG);
  tft.drawCentreString("@", 265, 188, 2);

  tft.fillRoundRect(282, 184, 24, 24, 3, COLOR_CHIP_BG);
  tft.drawRoundRect(282, 184, 24, 24, 3, COLOR_CARD_BORDER);
  tft.setTextColor(COLOR_TEXT_WHITE, COLOR_CHIP_BG);
  tft.drawCentreString("-", 294, 188, 2);

  // Fila 5: [CONECTAR] y [CANCELAR]
  tft.fillRoundRect(16, 211, 140, 21, 4, COLOR_BUY_GREEN);
  tft.setTextColor(TFT_WHITE, COLOR_BUY_GREEN);
  tft.drawCentreString("CONECTAR", 86, 214, 2);

  tft.fillRoundRect(164, 211, 142, 21, 4, tft.color565(0x2E, 0x36, 0x42));
  tft.drawRoundRect(164, 211, 142, 21, 4, COLOR_CARD_BORDER);
  tft.setTextColor(COLOR_TEXT_GRAY, tft.color565(0x2E, 0x36, 0x42));
  tft.drawCentreString("CANCELAR", 235, 214, 2);
}

void connectToSelectedWifi() {
  tft.fillRect(16, 36, 288, 196, COLOR_CARD_BG);
  tft.setTextColor(COLOR_BINANCE_YEL, COLOR_CARD_BG);
  tft.drawCentreString("Conectando a:", 160, 60, 2);
  
  tft.setTextColor(COLOR_TEXT_WHITE, COLOR_CARD_BG);
  String netShort = selectedWifiSSID;
  if (netShort.length() > 24) netShort = netShort.substring(0, 23) + "..";
  tft.drawCentreString(netShort, 160, 86, 4);

  tft.setTextColor(COLOR_TEXT_GRAY, COLOR_CARD_BG);
  tft.drawCentreString("Por favor espere...", 160, 120, 2);

  WiFi.disconnect();
  delay(150);
  WiFi.begin(selectedWifiSSID.c_str(), wifiPasswordInput.c_str());

  unsigned long startTry = millis();
  bool success = false;
  int dotCount = 0;
  while (millis() - startTry < 12000) {
    if (WiFi.status() == WL_CONNECTED) {
      success = true;
      break;
    }
    delay(300);
    dotCount++;
    String dots = "";
    for (int d = 0; d < (dotCount % 4); d++) dots += ".";
    tft.fillRect(130, 142, 60, 20, COLOR_CARD_BG);
    tft.setTextColor(COLOR_CYAN, COLOR_CARD_BG);
    tft.drawCentreString(dots, 160, 142, 2);
  }

  if (success) {
    saveKnownNetwork(selectedWifiSSID.c_str(), wifiPasswordInput.c_str());
    tft.setTextColor(COLOR_BUY_GREEN, COLOR_CARD_BG);
    tft.drawCentreString("CONECTADO EXITOSO!", 160, 140, 2);
    tft.setTextColor(COLOR_TEXT_WHITE, COLOR_CARD_BG);
    tft.drawCentreString("IP: " + WiFi.localIP().toString(), 160, 165, 2);
    delay(1500);
    isWifiKbOpen = false;
    isWifiListOpen = false;
    currentCardPage = 0;
    requestImmediateFetch = true;
    drawUI();
  } else {
    tft.setTextColor(COLOR_SELL_RED, COLOR_CARD_BG);
    tft.drawCentreString("Error de conexion", 160, 140, 2);
    tft.setTextColor(COLOR_TEXT_GRAY, COLOR_CARD_BG);
    tft.drawCentreString("Verifica la clave", 160, 165, 2);
    delay(1800);
    drawWifiKeyboard();
  }
}

void drawUI() {
  drawHeader();
  drawTabsAndFilters();
  drawAdsList();
  drawBottomBar();
}

// -----------------------------------------------------------------------------
// IMPLEMENTACIÓN DEL SALVAPANTALLAS "CYBER STARFIELD & CLOCK"
// -----------------------------------------------------------------------------
void initScreensaverStars() {
  for (int i = 0; i < NUM_STARS; i++) {
    stars[i].x = random(0, 320);
    stars[i].y = random(0, 240);
    stars[i].speed = random(1, 4); // Distintos planos de profundidad (paralaje)
    if (stars[i].speed == 1) {
      stars[i].color = tft.color565(0x40, 0x48, 0x58); // Lejana (tenue)
    } else if (stars[i].speed == 2) {
      stars[i].color = tft.color565(0x80, 0x90, 0xA5); // Media
    } else {
      stars[i].color = tft.color565(0xE0, 0xE8, 0xF5); // Cercana y brillante
    }
  }
  starsInitialized = true;
}

void enterScreensaver() {
  isScreensaverActive = true;
  if (!starsInitialized) {
    initScreensaverStars();
  }
  tft.fillScreen(TFT_BLACK);
  ssCardX = random(15, 320 - SS_CARD_W - 15);
  ssCardY = random(15, 240 - SS_CARD_H - 15);
  ssSpeedX = (random(0, 2) == 0) ? 1.0 : -1.0;
  ssSpeedY = (random(0, 2) == 0) ? 0.7 : -0.7;
}

void exitScreensaver() {
  isScreensaverActive = false;
  lastUserInteractionMs = millis();
  requestImmediateFetch = true; // Consulta inmediata a Binance al despertar
  tft.fillScreen(COLOR_BG);
  drawUI();
}

void updateScreensaver() {
  unsigned long now = millis();
  if (now - lastScreensaverAnimMs < SCREENSAVER_FPS_DELAY) {
    return;
  }
  lastScreensaverAnimMs = now;

  // 1. Guardar coordenadas anteriores de la cápsula para borrar únicamente su estela
  int prevX = (int)ssCardX;
  int prevY = (int)ssCardY;

  // 2. Mover la cápsula flotante
  ssCardX += ssSpeedX;
  ssCardY += ssSpeedY;

  // Rebotar en bordes horizontales
  if (ssCardX <= 4) {
    ssCardX = 4;
    ssSpeedX = -ssSpeedX;
  } else if (ssCardX >= 320 - SS_CARD_W - 4) {
    ssCardX = 320 - SS_CARD_W - 4;
    ssSpeedX = -ssSpeedX;
  }

  // Rebotar en bordes verticales
  if (ssCardY <= 4) {
    ssCardY = 4;
    ssSpeedY = -ssSpeedY;
  } else if (ssCardY >= 240 - SS_CARD_H - 4) {
    ssCardY = 240 - SS_CARD_H - 4;
    ssSpeedY = -ssSpeedY;
  }

  int curX = (int)ssCardX;
  int curY = (int)ssCardY;

  // 3. Si la tarjeta se desplazó, borrar el borde que dejó atrás
  if (curX != prevX || curY != prevY) {
    // Borrado selectivo ultraliviano de bordes anteriores
    if (curX > prevX) tft.fillRect(prevX - 1, prevY - 1, (curX - prevX) + 2, SS_CARD_H + 3, TFT_BLACK);
    if (curX < prevX) tft.fillRect(curX + SS_CARD_W, prevY - 1, (prevX - curX) + 3, SS_CARD_H + 3, TFT_BLACK);
    if (curY > prevY) tft.fillRect(prevX - 1, prevY - 1, SS_CARD_W + 3, (curY - prevY) + 2, TFT_BLACK);
    if (curY < prevY) tft.fillRect(prevX - 1, curY + SS_CARD_H, SS_CARD_W + 3, (prevY - curY) + 3, TFT_BLACK);
  }

  // 4. Actualizar estrellas de fondo que no colisionen con la cápsula
  for (int i = 0; i < NUM_STARS; i++) {
    // Borrar estrella en su posición anterior si no está dentro de la cápsula
    bool inCard = (stars[i].x >= curX && stars[i].x <= curX + SS_CARD_W &&
                   stars[i].y >= curY && stars[i].y <= curY + SS_CARD_H);
    if (!inCard) {
      tft.drawPixel(stars[i].x, stars[i].y, TFT_BLACK);
    }

    // Mover estrella hacia la izquierda
    stars[i].x -= stars[i].speed;
    if (stars[i].x < 0) {
      stars[i].x = 319;
      stars[i].y = random(0, 240);
    }

    // Dibujar estrella si está fuera de la cápsula
    inCard = (stars[i].x >= curX && stars[i].x <= curX + SS_CARD_W &&
              stars[i].y >= curY && stars[i].y <= curY + SS_CARD_H);
    if (!inCard) {
      tft.drawPixel(stars[i].x, stars[i].y, stars[i].color);
    }
  }

  // 5. Dibujar la Cápsula Flotante "Cyber Binance"
  // Fondo oscuro grafito
  uint16_t ssBg = tft.color565(0x14, 0x18, 0x22);
  tft.fillRoundRect(curX, curY, SS_CARD_W, SS_CARD_H, 10, ssBg);
  tft.drawRoundRect(curX, curY, SS_CARD_W, SS_CARD_H, 10, COLOR_BINANCE_YEL);
  tft.drawRoundRect(curX + 1, curY + 1, SS_CARD_W - 2, SS_CARD_H - 2, 9, tft.color565(0x28, 0x32, 0x42));

  // Cabecera de la cápsula: Logo y Etiqueta
  tft.fillRoundRect(curX + 8, curY + 6, 14, 14, 3, COLOR_CYAN);
  tft.setTextColor(ssBg, COLOR_CYAN);
  tft.drawCentreString("C", curX + 15, curY + 7, 1);

  tft.setTextColor(COLOR_TEXT_GRAY, ssBg);
  tft.drawString("MONITOR CAMBIARIO", curX + 26, curY + 7, 1);

  // Hora actual a la derecha (Fuente 2 limpia)
  String dispTime = formatCompactTime(lastUpdatedStr);
  if (dispTime == "--:--" || dispTime.length() == 0) {
    dispTime = "12:00 PM";
  }
  tft.setTextColor(COLOR_TEXT_WHITE, ssBg);
  tft.drawRightString(dispTime, curX + SS_CARD_W - 8, curY + 7, 2);

  // Línea divisora sutil
  tft.drawFastHLine(curX + 8, curY + 23, SS_CARD_W - 16, tft.color565(0x25, 0x2E, 0x3D));

  // 1. TASA BINANCE P2P
  String binancePrice = "---.--";
  float binanceVal = 0.0;
  if (p2pMutex != NULL && xSemaphoreTake(p2pMutex, pdMS_TO_TICKS(20)) == pdTRUE) {
    if (adsCount > 0) {
      binancePrice = adsList[0].price;
      binanceVal = atof(adsList[0].price);
    }
    xSemaphoreGive(p2pMutex);
  }
  tft.setTextColor(COLOR_BINANCE_YEL, ssBg);
  tft.drawString("BINANCE P2P:", curX + 8, curY + 28, 2);
  tft.setTextColor(COLOR_BUY_GREEN, ssBg);
  tft.drawRightString(binancePrice + " Bs", curX + SS_CARD_W - 8, curY + 28, 2);

  // 2. TASA BCV (OFICIAL)
  String bcvStr = "---.--";
  if (bcvRate > 0.0) {
    bcvStr = String(bcvRate, 2);
  }
  tft.setTextColor(COLOR_CYAN, ssBg);
  tft.drawString("DOLAR BCV:", curX + 8, curY + 49, 2);
  tft.setTextColor(COLOR_TEXT_WHITE, ssBg);
  tft.drawRightString(bcvStr + " Bs", curX + SS_CARD_W - 8, curY + 49, 2);

  // 3. TASA DE INTERVENCIÓN (+0.5%)
  String intervStr = "---.--";
  if (intervencionRate > 0.0) {
    intervStr = String(intervencionRate, 2);
  }
  tft.setTextColor(tft.color565(0xFF, 0x98, 0x00), ssBg); // Naranja brillante
  tft.drawString("INTERVENCION (+0.5%):", curX + 8, curY + 70, 2);
  tft.setTextColor(COLOR_BINANCE_YEL, ssBg);
  tft.drawRightString(intervStr + " Bs", curX + SS_CARD_W - 8, curY + 70, 2);

  // Línea divisora inferior
  tft.drawFastHLine(curX + 8, curY + 91, SS_CARD_W - 16, tft.color565(0x22, 0x2B, 0x38));

  // Brecha porcentual Binance vs BCV (Spread cambiario en vivo)
  if (bcvRate > 0.0 && binanceVal > 0.0) {
    float spread = ((binanceVal - bcvRate) / bcvRate) * 100.0;
    tft.setTextColor(COLOR_TEXT_GRAY, ssBg);
    tft.drawString("Brecha P2P / BCV:", curX + 8, curY + 95, 1);
    tft.setTextColor((spread >= 0) ? COLOR_BUY_GREEN : COLOR_SELL_RED, ssBg);
    String sprStr = (spread >= 0 ? "+" : "") + String(spread, 1) + "%";
    tft.drawRightString(sprStr, curX + SS_CARD_W - 8, curY + 95, 1);
  }

  // Subtítulo sutil en la base
  tft.setTextColor(tft.color565(0x50, 0x5C, 0x6E), ssBg);
  tft.drawCentreString("Toca la pantalla para operar", curX + (SS_CARD_W / 2), curY + 112, 1);
}

// -----------------------------------------------------------------------------
// CONTROLADOR TÁCTIL DE LA CALCULADORA
// -----------------------------------------------------------------------------
void handleCalculatorTouch(int x, int y) {
  unsigned long now = millis();

  // 0. Si está en la pantalla -1 (Selector Inicial: USDT o VES)
  if (calcStep == -1) {
    // Opción 1: [ $ ] USDT (x=22..154, y=68..176)
    if (x >= 22 && x <= 154 && y >= 68 && y <= 176) {
      calcMode = 0; // Modo USDT
      calcStep = 0;
      calcInputStr = "";
      lastTouchMs = now;
      drawCalculatorScreen();
      return;
    }
    // Opción 2: [ Bs ] VES (x=166..298, y=68..176)
    if (x >= 166 && x <= 298 && y >= 68 && y <= 176) {
      calcMode = 1; // Modo Bolívares
      calcStep = 0;
      calcInputStr = "";
      lastTouchMs = now;
      drawCalculatorScreen();
      return;
    }
    // Botón Salir / Volver (x=70..250, y=192..220)
    if (x >= 70 && x <= 250 && y >= 192 && y <= 220) {
      closeCalculator();
      lastTouchMs = now;
      return;
    }
    return;
  }

  // 1. Si está en el paso 5 (Pregunta de Aplica P2P?)
  if (calcStep == 5) {
    // Botón NO (x=30..150, y=105..165)
    if (x >= 30 && x <= 150 && y >= 105 && y <= 165) {
      calcAplicaP2P = false;
      calcComP2PPct = 0.0;
      calcStep = 7; // Saltar directo a la pantalla de resultados
      lastTouchMs = now;
      drawCalculatorScreen();
      return;
    }
    // Botón SÍ (x=170..290, y=105..165)
    if (x >= 170 && x <= 290 && y >= 105 && y <= 165) {
      calcAplicaP2P = true;
      calcInputStr = "";
      calcStep = 6; // Ir a pedir comisión P2P
      lastTouchMs = now;
      drawCalculatorScreen();
      return;
    }
    // Botón Cancelar y Salir (x=70..250, y=192..220)
    if (x >= 70 && x <= 250 && y >= 192 && y <= 220) {
      closeCalculator();
      lastTouchMs = now;
      return;
    }
    return;
  }

  // 2. Si está en la pantalla 7 de Resultados Finales
  if (calcStep == 7) {
    // Botón NUEVO CÁLCULO (x=16..156, y=172..204)
    if (x >= 16 && x <= 156 && y >= 172 && y <= 204) {
      openCalculator();
      lastTouchMs = now;
      return;
    }
    // Botón CERRAR (x=164..306, y=172..204)
    if (x >= 164 && x <= 306 && y >= 172 && y <= 204) {
      closeCalculator();
      lastTouchMs = now;
      return;
    }
    return;
  }

  // 3. Pasos 0, 1, 2, 3, 4 y 6 (Teclado numérico con punto decimal)
  // Botón Borrar (x=28..154, y=207..229)
  if (x >= 28 && x <= 154 && y >= 207 && y <= 229) {
    if (calcInputStr.length() > 0) {
      calcInputStr.remove(calcInputStr.length() - 1);
      drawCalculatorScreen();
    }
    lastTouchMs = now;
    return;
  }

  // Botón Salir (x=166..292, y=207..229)
  if (x >= 166 && x <= 292 && y >= 207 && y <= 229) {
    closeCalculator();
    lastTouchMs = now;
    return;
  }

  // Rejilla de teclas del teclado numérico
  for (int r = 0; r < 4; r++) {
    for (int c = 0; c < 3; c++) {
      int bx = 28 + c * 90;
      int by = 73 + r * 33;

      if (x >= bx && x <= bx + 84 && y >= by && y <= by + 29) {
        if (r == 3 && c == 0) {
          // Punto decimal '.'
          if (calcInputStr.indexOf('.') == -1 && calcInputStr.length() < 9) {
            if (calcInputStr.length() == 0) calcInputStr = "0";
            calcInputStr += ".";
            drawCalculatorScreen();
          }
        } else if (r == 3 && c == 2) {
          // Botón OK: Procesar paso actual y avanzar
          float val = calcInputStr.toFloat();

          if (calcStep == 0) {
            calcMontoEntrada = val;
            calcInputStr = "";
            calcStep = 1; // Pasar a Tasa Venta
          } else if (calcStep == 1) {
            calcTasaVenta = val;
            calcInputStr = "";
            calcStep = 2; // Pasar a Tasa Intervención
          } else if (calcStep == 2) {
            calcTasaInterv = val;
            calcInputStr = "";
            calcStep = 3; // Pasar a Comisión Banco
          } else if (calcStep == 3) {
            calcComBancoPct = val;
            calcInputStr = "";
            calcStep = 4; // Pasar a Comisión Pasarela
          } else if (calcStep == 4) {
            calcComPasarelaPct = val;
            calcInputStr = "";
            calcStep = 5; // Pasar a pregunta ¿Aplica P2P?
          } else if (calcStep == 6) {
            calcComP2PPct = val;
            calcInputStr = "";
            calcStep = 7; // Pasar a Pantalla de Resultados
          }

          drawCalculatorScreen();
        } else {
          // Teclas numéricas 0-9
          char digit = (r == 3 && c == 1) ? '0' : ('1' + (r * 3 + c));
          if (calcInputStr.length() < 10) {
            calcInputStr += digit;
            drawCalculatorScreen();
          }
        }
        lastTouchMs = now;
        return;
      }
    }
  }
}

// -----------------------------------------------------------------------------
// CONTROLADOR TÁCTIL (XPT2046) - ULTRA FLUIDO SIN DELAYS
// -----------------------------------------------------------------------------
void handleKeypadTouch(int x, int y) {
  unsigned long now = millis();
  // Botón Restablecer a Todo / Cerrar (y = 204..234, x = 24..296)
  if (y >= 204 && y <= 234 && x >= 24 && x <= 296) {
    currentAmountFilter = "";
    isKeypadOpen = false;
    currentCardPage = 0;
    requestImmediateFetch = true;
    lastTouchMs = now;
    drawUI();
    return;
  }

  for (int r = 0; r < 4; r++) {
    for (int c = 0; c < 3; c++) {
      int bx = 28 + c * 90;
      int by = 64 + r * 35;
      
      if (x >= bx && x <= bx + 84 && y >= by && y <= by + 31) {
        if (r == 3 && c == 0) {
          if (keypadInput.length() > 0) {
            keypadInput.remove(keypadInput.length() - 1);
            drawKeypadDisplayBox();
          }
        } else if (r == 3 && c == 2) {
          currentAmountFilter = keypadInput;
          isKeypadOpen = false;
          currentCardPage = 0;
          requestImmediateFetch = true;
          drawUI();
        } else {
          if (keypadInput.length() < 7) {
            char num = (r == 3 && c == 1) ? '0' : ('1' + (r * 3 + c));
            keypadInput += num;
            drawKeypadDisplayBox();
          }
        }
        lastTouchMs = now;
        return;
      }
    }
  }
}

void handleBankModalTouch(int x, int y) {
  unsigned long now = millis();
  int totalPages = (TOTAL_BANKS + 4) / 5;
  int startIdx = bankModalPage * 5;
  
  // Comprobar filas de bancos (5 filas)
  for (int i = 0; i < 5; i++) {
    int bankIdx = startIdx + i;
    if (bankIdx >= TOTAL_BANKS) break;

    int by = 30 + i * 33;
    if (x >= 20 && x <= 298 && y >= by && y <= by + 29) {
      selectedBankIdx = bankIdx;
      isBankModalOpen = false;
      currentCardPage = 0;
      requestImmediateFetch = true;
      lastTouchMs = now;
      drawUI();
      return;
    }
  }

  // Fila inferior de botones (y = 194..228)
  if (y >= 194 && y <= 228) {
    // Botón Izquierdo: Paginación
    if (x >= 20 && x <= 156) {
      bankModalPage = (bankModalPage + 1) % totalPages;
      drawBankModal();
      lastTouchMs = now;
      return;
    }
    // Botón Derecho: Cerrar
    if (x >= 160 && x <= 298) {
      isBankModalOpen = false;
      drawUI();
      lastTouchMs = now;
      return;
    }
  }
}

void handleWifiListTouch(int x, int y) {
  unsigned long now = millis();
  int totalPages = (scannedNetworksCount > 0) ? ((scannedNetworksCount + 3) / 4) : 1;
  int startIdx = wifiListPage * 4;

  for (int i = 0; i < 4; i++) {
    int idx = startIdx + i;
    if (idx >= scannedNetworksCount) break;

    int by = 32 + i * 40;
    if (x >= 20 && x <= 298 && y >= by && y <= by + 36) {
      selectedWifiSSID = scannedNets[idx].ssid;
      wifiPasswordInput = "";
      for (int k = 0; k < savedNetworksCount; k++) {
        if (selectedWifiSSID == savedNetworks[k].ssid) {
          wifiPasswordInput = savedNetworks[k].pass;
          break;
        }
      }
      isWifiListOpen = false;
      isWifiKbOpen = true;
      wifiKbMode = 0;
      wifiPassVisible = false;
      lastTouchMs = now;
      drawWifiKeyboard();
      return;
    }
  }

  if (y >= 192 && y <= 228) {
    if (x >= 20 && x <= 110) {
      lastTouchMs = now;
      scanWifiNetworks();
      wifiListPage = 0;
      drawWifiListModal();
      return;
    }
    if (x >= 114 && x <= 204) {
      wifiListPage = (wifiListPage + 1) % totalPages;
      lastTouchMs = now;
      drawWifiListModal();
      return;
    }
    if (x >= 208 && x <= 298) {
      isWifiListOpen = false;
      lastTouchMs = now;
      drawUI();
      return;
    }
  }
}

void handleWifiKbTouch(int x, int y) {
  unsigned long now = millis();

  // 1. Botón Ojo Ver/Ocultar (x=254..308, y=36..66)
  if (y >= 36 && y <= 66 && x >= 254 && x <= 308) {
    wifiPassVisible = !wifiPassVisible;
    drawWifiKeyboardDisplayBox();
    lastTouchMs = now;
    return;
  }

  // 2. Fila 0 (y = 66..95)
  if (y >= 66 && y <= 95) {
    for (int c = 0; c < 10; c++) {
      int kx = 16 + c * 29;
      if (x >= kx && x <= kx + 27) {
        char ch = (wifiKbMode == 2) ? KB_R0_SYM[c] : KB_R0_LOWER[c];
        if (wifiPasswordInput.length() < 64) {
          wifiPasswordInput += ch;
          drawWifiKeyboardDisplayBox();
        }
        lastTouchMs = now;
        return;
      }
    }
  }

  // 3. Fila 1 (y = 96..124)
  if (y >= 96 && y <= 124) {
    for (int c = 0; c < 10; c++) {
      int kx = 16 + c * 29;
      if (x >= kx && x <= kx + 27) {
        char ch = (wifiKbMode == 2) ? KB_R1_SYM[c] : ((wifiKbMode == 1) ? KB_R1_UPPER[c] : KB_R1_LOWER[c]);
        if (wifiPasswordInput.length() < 64) {
          wifiPasswordInput += ch;
          drawWifiKeyboardDisplayBox();
        }
        lastTouchMs = now;
        return;
      }
    }
  }

  // 4. Fila 2 (y = 125..153)
  if (y >= 125 && y <= 153) {
    for (int c = 0; c < 10; c++) {
      int kx = 16 + c * 29;
      if (x >= kx && x <= kx + 27) {
        char ch = (wifiKbMode == 2) ? KB_R2_SYM[c] : ((wifiKbMode == 1) ? KB_R2_UPPER[c] : KB_R2_LOWER[c]);
        if (wifiPasswordInput.length() < 64) {
          wifiPasswordInput += ch;
          drawWifiKeyboardDisplayBox();
        }
        lastTouchMs = now;
        return;
      }
    }
  }

  // 5. Fila 3 (y = 154..182): Shift [A/a] + 7 letras + DEL
  if (y >= 154 && y <= 182) {
    if (x >= 14 && x <= 55) {
      wifiKbMode = (wifiKbMode == 1) ? 0 : 1;
      drawWifiKeyboard();
      lastTouchMs = now;
      return;
    }
    for (int c = 0; c < 7; c++) {
      int kx = 58 + c * 29;
      if (x >= kx && x <= kx + 27) {
        char ch = (wifiKbMode == 2) ? KB_R3_SYM[c] : ((wifiKbMode == 1) ? KB_R3_UPPER[c] : KB_R3_LOWER[c]);
        if (wifiPasswordInput.length() < 64) {
          wifiPasswordInput += ch;
          drawWifiKeyboardDisplayBox();
        }
        lastTouchMs = now;
        return;
      }
    }
    if (x >= 263 && x <= 308) {
      if (wifiPasswordInput.length() > 0) {
        wifiPasswordInput.remove(wifiPasswordInput.length() - 1);
        drawWifiKeyboardDisplayBox();
      }
      lastTouchMs = now;
      return;
    }
  }

  // 6. Fila 4 (y = 183..208): [?123/abc] [.] [ESPACIO] [_] [@] [-]
  if (y >= 183 && y <= 208) {
    if (x >= 14 && x <= 65) {
      wifiKbMode = (wifiKbMode == 2) ? 0 : 2;
      drawWifiKeyboard();
      lastTouchMs = now;
      return;
    }
    if (x >= 66 && x <= 99) {
      if (wifiPasswordInput.length() < 64) {
        wifiPasswordInput += '.';
        drawWifiKeyboardDisplayBox();
      }
      lastTouchMs = now;
      return;
    }
    if (x >= 100 && x <= 220) {
      if (wifiPasswordInput.length() < 64) {
        wifiPasswordInput += ' ';
        drawWifiKeyboardDisplayBox();
      }
      lastTouchMs = now;
      return;
    }
    if (x >= 221 && x <= 249) {
      if (wifiPasswordInput.length() < 64) {
        wifiPasswordInput += '_';
        drawWifiKeyboardDisplayBox();
      }
      lastTouchMs = now;
      return;
    }
    if (x >= 250 && x <= 279) {
      if (wifiPasswordInput.length() < 64) {
        wifiPasswordInput += '@';
        drawWifiKeyboardDisplayBox();
      }
      lastTouchMs = now;
      return;
    }
    if (x >= 280 && x <= 308) {
      if (wifiPasswordInput.length() < 64) {
        wifiPasswordInput += '-';
        drawWifiKeyboardDisplayBox();
      }
      lastTouchMs = now;
      return;
    }
  }

  // 7. Fila 5 (y = 209..234): [CONECTAR] y [CANCELAR]
  if (y >= 209 && y <= 234) {
    if (x >= 14 && x <= 158) {
      lastTouchMs = now;
      connectToSelectedWifi();
      return;
    }
    if (x >= 160 && x <= 308) {
      isWifiKbOpen = false;
      isWifiListOpen = true;
      lastTouchMs = now;
      drawWifiListModal();
      return;
    }
  }
}

void handleAlertModalTouch(int x, int y) {
  // Teclado numérico (r=0..3, c=0..2)
  const char* KEYS[4][3] = {
    {"1", "2", "3"},
    {"4", "5", "6"},
    {"7", "8", "9"},
    {".", "0", "<"}
  };

  for (int r = 0; r < 4; r++) {
    int by = 70 + r * 30;
    if (y >= by && y <= by + 26) {
      for (int c = 0; c < 3; c++) {
        int bx = 28 + c * 90;
        if (x >= bx && x <= bx + 84) {
          String key = KEYS[r][c];
          if (key == "<") {
            if (alertInputPrice.length() > 0) {
              alertInputPrice.remove(alertInputPrice.length() - 1);
            }
          } else if (key == ".") {
            if (alertInputPrice.indexOf('.') == -1 && alertInputPrice.length() < 9) {
              if (alertInputPrice.length() == 0) alertInputPrice = "0.";
              else alertInputPrice += ".";
            }
          } else {
            if (alertInputPrice.length() < 9) {
              alertInputPrice += key;
            }
          }
          drawAlertModalDisplayBox();
          lastTouchMs = millis();
          return;
        }
      }
    }
  }

  // Botón ACTIVAR / ACTUALIZAR (x=24..154, y=194..224)
  if (y >= 194 && y <= 224 && x >= 24 && x <= 154) {
    float val = alertInputPrice.toFloat();
    if (val > 0.0) {
      bool isBuy = (currentTradeType == "BUY");
      if (isBuy) {
        alertBuyTargetPrice = val;
        isAlertBuyActive = true;
        lastTriggeredBuyPrice = 0.0; // Reset para evaluar de inmediato
        prefs.begin("p2p_alerts", false);
        prefs.putBool("buy_act", true);
        prefs.putFloat("buy_tgt", alertBuyTargetPrice);
        prefs.end();
      } else {
        alertSellTargetPrice = val;
        isAlertSellActive = true;
        lastTriggeredSellPrice = 0.0; // Reset para evaluar de inmediato
        prefs.begin("p2p_alerts", false);
        prefs.putBool("sell_act", true);
        prefs.putFloat("sell_tgt", alertSellTargetPrice);
        prefs.end();
      }
    }
    isAlertModalOpen = false;
    drawUI();
    lastTouchMs = millis();
    return;
  }

  // Botón DESACTIVAR / CERRAR (x=166..296, y=194..224)
  if (y >= 194 && y <= 224 && x >= 166 && x <= 296) {
    bool isBuy = (currentTradeType == "BUY");
    if (isBuy) {
      if (isAlertBuyActive) {
        isAlertBuyActive = false;
        prefs.begin("p2p_alerts", false);
        prefs.putBool("buy_act", false);
        prefs.end();
      }
    } else {
      if (isAlertSellActive) {
        isAlertSellActive = false;
        prefs.begin("p2p_alerts", false);
        prefs.putBool("sell_act", false);
        prefs.end();
      }
    }
    isAlertModalOpen = false;
    drawUI();
    lastTouchMs = millis();
    return;
  }
}

void handleTouch() {
  if (!ts.touched()) {
    isDragging = false;
    return;
  }

  unsigned long now = millis();

  // Si la alerta a pantalla completa está activa, cualquier toque la cierra y silencia
  if (isFullScreenAlertOpen) {
    isFullScreenAlertOpen = false;
    isAlertFlashing = false;
    digitalWrite(RGB_LED_RED, HIGH);
    digitalWrite(RGB_LED_GREEN, HIGH);
    digitalWrite(RGB_LED_BLUE, HIGH);
    drawUI();
    lastTouchMs = now;
    return;
  }

  // Si el salvapantallas está activo, cualquier toque lo desactiva de inmediato
  if (isScreensaverActive) {
    exitScreensaver();
    if (isAlertFlashing) {
      isAlertFlashing = false;
      digitalWrite(RGB_LED_RED, HIGH);
      digitalWrite(RGB_LED_GREEN, HIGH);
      digitalWrite(RGB_LED_BLUE, HIGH);
    }
    lastTouchMs = now;
    return;
  }

  // Si hay una alerta parpadeando en pantalla, cualquier toque silencia la alarma
  if (isAlertFlashing) {
    isAlertFlashing = false;
    digitalWrite(RGB_LED_RED, HIGH);
    digitalWrite(RGB_LED_GREEN, HIGH);
    digitalWrite(RGB_LED_BLUE, HIGH);
    drawUI();
    lastTouchMs = now;
    return;
  }

  // Actualizar marca de tiempo de interacción del usuario
  lastUserInteractionMs = now;

  TS_Point p = ts.getPoint();
  
  int x = map(p.x, 250, 3750, 0, 320);
  int y = map(p.y, 250, 3750, 0, 240);
  x = constrain(x, 0, 320);
  y = constrain(y, 0, 240);

  // 0. Si el modal de alerta #1 está abierto
  if (isAlertModalOpen) {
    if (now - lastTouchMs >= 150) {
      handleAlertModalTouch(x, y);
    }
    return;
  }

  // 0. Si la Calculadora de Arbitraje está abierta
  if (isCalcOpen) {
    if (now - lastTouchMs >= 150) {
      handleCalculatorTouch(x, y);
    }
    return;
  }

  // 1. Si el modal de teclado numérico de monto está abierto
  if (isKeypadOpen) {
    if (now - lastTouchMs >= 150) {
      handleKeypadTouch(x, y);
    }
    return;
  }

  // 2. Si el modal de bancos está abierto
  if (isBankModalOpen) {
    if (now - lastTouchMs >= 150) {
      handleBankModalTouch(x, y);
    }
    return;
  }

  // 3. Si el modal de lista WiFi está abierto
  if (isWifiListOpen) {
    if (now - lastTouchMs >= 150) {
      handleWifiListTouch(x, y);
    }
    return;
  }

  // 4. Si el teclado de clave WiFi está abierto
  if (isWifiKbOpen) {
    if (now - lastTouchMs >= 150) {
      handleWifiKbTouch(x, y);
    }
    return;
  }

  // Toque en el Icono WiFi del Encabezado (y <= 24, x >= 76 && x <= 118)
  if (y <= 24 && x >= 76 && x <= 118 && (now - lastTouchMs >= 200)) {
    lastTouchMs = now;
    isWifiListOpen = true;
    wifiListPage = 0;
    scanWifiNetworks();
    drawWifiListModal();
    return;
  }

  // Toque en el Botón CALC del Encabezado (y <= 24, x >= 120 && x <= 170)
  if (y <= 24 && x >= 120 && x <= 170 && (now - lastTouchMs >= 200)) {
    lastTouchMs = now;
    openCalculator();
    return;
  }

  // Toque en el Botón ALERTA (#1) del Encabezado (y <= 24, x >= 172 && x <= 250)
  if (y <= 24 && x >= 172 && x <= 250 && (now - lastTouchMs >= 200)) {
    lastTouchMs = now;
    isAlertModalOpen = true;
    bool isBuy = (currentTradeType == "BUY");
    float currentTgt = isBuy ? alertBuyTargetPrice : alertSellTargetPrice;
    alertInputPrice = (currentTgt > 0.0) ? String(currentTgt, 2) : "";
    drawAlertModal();
    return;
  }

  // 3. Pestañas COMPRAR / VENDER (y = 24..47)
  if (y >= 24 && y <= 47 && (now - lastTouchMs >= 160)) {
    if (x < 160 && currentTradeType != "BUY") {
      currentTradeType = "BUY";
      currentCardPage = 0;
      requestImmediateFetch = true;
      lastTouchMs = now;
      drawHeader(); // Actualiza botón de alerta para reflejar la pestaña COMPRA
      drawTabsAndFilters();
      return;
    } else if (x >= 160 && currentTradeType != "SELL") {
      currentTradeType = "SELL";
      currentCardPage = 0;
      requestImmediateFetch = true;
      lastTouchMs = now;
      drawHeader(); // Actualiza botón de alerta para reflejar la pestaña VENTA
      drawTabsAndFilters();
      return;
    }
  }

  // 4. Fila de Filtros (y = 48..70)
  if (y >= 48 && y <= 70 && (now - lastTouchMs >= 180)) {
    // Chip MONTO (x = 66..186) -> Abre Teclado Numérico
    if (x >= 66 && x <= 186) {
      keypadInput = currentAmountFilter;
      isKeypadOpen = true;
      lastTouchMs = now;
      drawKeypad();
      return;
    }
    // Chip BANCO (x >= 190 && x <= 316) -> Abre Menú de Bancos
    if (x >= 190 && x <= 316) {
      isBankModalOpen = true;
      lastTouchMs = now;
      drawBankModal();
      return;
    }
  }

  // 5. Botones de Navegación Inferior (y >= 216 && y <= 240)
  if (y >= 216 && y <= 240 && (now - lastTouchMs >= 150)) {
    int totalPages = (adsCount > 0) ? ((adsCount + 1) / 2) : 1;
    // Botón Subir (x <= 110)
    if (x <= 110) {
      if (currentCardPage > 0) {
        currentCardPage--;
        lastFetchMillis = now;
        lastTouchMs = now;
        drawAdsList();
        drawBottomBar();
      }
      return;
    }
    // Botón Bajar (x >= 210)
    if (x >= 210) {
      if (currentCardPage < totalPages - 1) {
        currentCardPage++;
        lastFetchMillis = now;
        lastTouchMs = now;
        drawAdsList();
        drawBottomBar();
      }
      return;
    }
  }

  // 6. Gesto de desplazamiento rápido (Swipe vertical)
  if (y >= 71 && y < 216) {
    if (!isDragging) {
      isDragging = true;
      touchStartY = y;
      lastTouchY = y;
    } else {
      int deltaY = touchStartY - y; // Positivo = hacia arriba (bajar de página)
      int totalPages = (adsCount > 0) ? ((adsCount + 1) / 2) : 1;
      if (deltaY > 25 && currentCardPage < totalPages - 1 && (now - lastTouchMs >= 200)) {
        currentCardPage++;
        isDragging = false;
        lastFetchMillis = now;
        lastTouchMs = now;
        drawAdsList();
        drawBottomBar();
        return;
      } else if (deltaY < -25 && currentCardPage > 0 && (now - lastTouchMs >= 200)) {
        currentCardPage--;
        isDragging = false;
        lastFetchMillis = now;
        lastTouchMs = now;
        drawAdsList();
        drawBottomBar();
        return;
      }
    }
  }
}

// -----------------------------------------------------------------------------
// SETUP
// -----------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  Serial.println("\n[CREALO P2P] Monitor CYD Horizontal (320x240)...");

  // Crear Mutex de FreeRTOS para blindar memoria RAM entre Core 0 y Core 1
  p2pMutex = xSemaphoreCreateMutex();

  // Forzar backlight en pines 21 y 27
  pinMode(21, OUTPUT);
  digitalWrite(21, HIGH);
  pinMode(27, OUTPUT);
  digitalWrite(27, HIGH);

  // Configurar LED RGB posterior integrado en placa CYD (activo en nivel bajo / LOW)
  pinMode(RGB_LED_RED, OUTPUT);
  pinMode(RGB_LED_GREEN, OUTPUT);
  pinMode(RGB_LED_BLUE, OUTPUT);
  digitalWrite(RGB_LED_RED, HIGH);   // Apagado
  digitalWrite(RGB_LED_GREEN, HIGH); // Apagado
  digitalWrite(RGB_LED_BLUE, HIGH);  // Apagado

  // Cargar estado previo de Alertas P2P (Compra y Venta) desde NVS
  prefs.begin("p2p_alerts", true);
  isAlertBuyActive      = prefs.getBool("buy_act", false);
  alertBuyTargetPrice   = prefs.getFloat("buy_tgt", 0.0);
  isAlertSellActive     = prefs.getBool("sell_act", false);
  alertSellTargetPrice  = prefs.getFloat("sell_tgt", 0.0);
  prefs.end();

  // Inicializar Pantalla con TFT_eSPI en orientación Horizontal (320 x 240)
  tft.init();
  tft.setRotation(1); // Landscape (320 x 240)
  initColors();
  tft.fillScreen(COLOR_BG);

  // Inicializar Táctil con bus independiente en orientación horizontal
  touchSpi.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  ts.begin(touchSpi);
  ts.setRotation(1);

  // Bienvenida con identidad de marca CREALO p2p
  // 1. Emblema geométrico CREALO
  tft.fillRoundRect(132, 22, 56, 56, 12, tft.color565(0x1A, 0x24, 0x32));
  tft.drawRoundRect(132, 22, 56, 56, 12, COLOR_CYAN);
  tft.drawRoundRect(133, 23, 54, 54, 11, COLOR_CYAN);
  tft.setTextColor(COLOR_CYAN, tft.color565(0x1A, 0x24, 0x32));
  tft.drawCentreString("C", 160, 32, 4);

  // 2. Títulos
  tft.setTextColor(COLOR_TEXT_WHITE, COLOR_BG);
  tft.drawCentreString("CREALO", 160, 90, 4);

  tft.setTextColor(COLOR_BINANCE_YEL, COLOR_BG);
  tft.drawCentreString("P2P MONITOR", 160, 126, 2);

  tft.setTextColor(COLOR_TEXT_GRAY, COLOR_BG);
  tft.drawCentreString("Buscando redes WiFi...", 160, 160, 2);

  // Auto-conexión Inteligente Multi-Red desde NVS
  bool connected = autoConnectWiFi();

  if (connected) {
    Serial.println("\nWiFi Conectado! IP: " + WiFi.localIP().toString());
    tft.setTextColor(COLOR_BUY_GREEN, COLOR_BG);
    String connMsg = "Conectado a " + WiFi.SSID();
    if (connMsg.length() > 26) connMsg = connMsg.substring(0, 25) + "..";
    tft.drawCentreString(connMsg, 160, 192, 2);
  } else {
    Serial.println("\nSin conexion WiFi directa");
    tft.setTextColor(COLOR_SELL_RED, COLOR_BG);
    tft.drawCentreString("Sin WiFi. Toca icono para config", 160, 192, 2);
  }
  delay(1000);

  // Primera consulta con filtros (solo si hay conexión)
  if (connected) {
    fetchBcvRate();
    fetchBinanceP2P("BUY");
  }
  drawUI();

  // Inicializar temporizador de inactividad
  lastUserInteractionMs = millis();

  // Iniciar tarea de red en Core 0 (desacoplada del hilo de render y touch en Core 1)
  xTaskCreatePinnedToCore(
    networkTask,
    "p2pNetTask",
    10240,
    NULL,
    1,
    &netTaskHandle,
    0
  );
}

// -----------------------------------------------------------------------------
// LOOP PRINCIPAL (Core 1: Táctil y render ultra fluido)
// -----------------------------------------------------------------------------
void loop() {
  handleTouch();

  unsigned long now = millis();

  // ---------------------------------------------------------------------------
  // VIGILANCIA PREVENTIVA DE MEMORIA RAM (Heap Watchdog 24/7)
  // ---------------------------------------------------------------------------
  static unsigned long lastHeapWatchdogMs = 0;
  if (now - lastHeapWatchdogMs >= 30000) { // Comprobar cada 30 segundos
    lastHeapWatchdogMs = now;
    uint32_t freeH = ESP.getFreeHeap();
    uint32_t minH  = ESP.getMinFreeHeap();
    if (freeH < 32000 || minH < 24000) {
      Serial.printf("[HEAP WATCHDOG] Memoria crítica (%u bytes). Reinicio preventivo...\n", freeH);
      delay(200);
      esp_restart();
    }
  }

  // Si la alarma de precio está disparada, hacer parpadear el LED RGB onboard durante 12 segundos
  if (isAlertFlashing) {
    if (now - alertFlashStartMs >= 12000) {
      isAlertFlashing = false;
      digitalWrite(RGB_LED_RED, HIGH);
      digitalWrite(RGB_LED_GREEN, HIGH);
      digitalWrite(RGB_LED_BLUE, HIGH);
    } else {
      // Parpadeo alternado ámbar/apagado cada 250ms
      bool blink = ((now / 250) % 2) == 0;
      digitalWrite(RGB_LED_RED, blink ? LOW : HIGH);
      digitalWrite(RGB_LED_GREEN, blink ? LOW : HIGH); // Rojo + Verde = Amarillo/Ámbar
      digitalWrite(RGB_LED_BLUE, HIGH);
    }
  }

  // Auto-cierre de la Alerta a Pantalla Completa tras 2 minutos (120 seg)
  if (isFullScreenAlertOpen) {
    if (now - fullScreenAlertStartMs >= FULL_ALERT_DURATION_MS) {
      isFullScreenAlertOpen = false;
      isAlertFlashing = false;
      digitalWrite(RGB_LED_RED, HIGH);
      digitalWrite(RGB_LED_GREEN, HIGH);
      digitalWrite(RGB_LED_BLUE, HIGH);
      drawUI();
    }
  }

  // 1. Verificar si corresponde entrar en modo Salvapantallas por inactividad
  if (!isScreensaverActive) {
    // Solo si no hay ningún menú, teclado, calculadora, modal o alerta a pantalla completa abierta
    if (!isKeypadOpen && !isBankModalOpen && !isWifiListOpen && !isWifiKbOpen && !isCalcOpen && !isAlertModalOpen && !isFullScreenAlertOpen) {
      if (now - lastUserInteractionMs >= SCREENSAVER_TIMEOUT_MS) {
        enterScreensaver();
      }
    } else {
      // Si hay un teclado, modal, calculadora o alerta abierta, mantener activo el temporizador
      lastUserInteractionMs = now;
    }
  }

  // 2. Si el salvapantallas está activo, actualizar animación estelar y cápsula flotante
  if (isScreensaverActive) {
    updateScreensaver();
    if (hasNewDataToDisplay) {
      hasNewDataToDisplay = false; // Consumir bandera para que la cápsula muestre el nuevo precio
    }
    delay(10);
    return;
  }

  // 3. Si no está en salvapantallas y hay datos frescos de Binance, redibujar la UI normal
  if (hasNewDataToDisplay) {
    hasNewDataToDisplay = false;
    if (!isKeypadOpen && !isBankModalOpen && !isWifiListOpen && !isWifiKbOpen && !isCalcOpen && !isAlertModalOpen && !isFullScreenAlertOpen) {
      drawHeader();
      drawAdsList();
      drawBottomBar();
      if (isAlertFlashing) {
        drawAlertNotificationBanner();
      }
    }
  }

  delay(2);
}
