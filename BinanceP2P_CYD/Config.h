#pragma once
#include <Arduino.h>

/*
 * ======================================================================================
 * CONFIG.H - CONFIGURACIÓN DE HARDWARE, PINES, CREDENCIALES Y CONSTANTES
 * ======================================================================================
 */

// -----------------------------------------------------------------------------
// PINES DEL TÁCTIL (XPT2046 en CYD)
// -----------------------------------------------------------------------------
#define XPT2046_IRQ  36
#define XPT2046_MOSI 32
#define XPT2046_MISO 39
#define XPT2046_CLK  25
#define XPT2046_CS   33

// -----------------------------------------------------------------------------
// PINES DE ALERTA HARDWARE (LED RGB TRASERO EN CYD)
// -----------------------------------------------------------------------------
#define RGB_LED_RED   4
#define RGB_LED_GREEN 16
#define RGB_LED_BLUE  17

// -----------------------------------------------------------------------------
// CONFIGURACIÓN DE ALERTAS TELEGRAM Y P2P
// -----------------------------------------------------------------------------
const char* TELEGRAM_ALERT_URL = "https://monitor-luz-vercel-six.vercel.app/api/p2p-alert";
const char* TELEGRAM_CHAT_ID   = "TU_CHAT_ID_TELEGRAM";

// -----------------------------------------------------------------------------
// GESTIÓN MULTI-WIFI Y MEMORIA NVS (Preferences)
// -----------------------------------------------------------------------------
#define MAX_SAVED_NETWORKS 5

struct SavedNet {
  char ssid[33];
  char pass[65];
};

const char* DEFAULT_WIFI_SSID = "TU_WIFI_AQUI";
const char* DEFAULT_WIFI_PASS = "TU_PASSWORD_AQUI";
const char* API_BASE_URL      = "https://monitor-luz-vercel-six.vercel.app/api/p2p";

#define MAX_SCANNED 15
struct ScannedNet {
  String ssid;
  int32_t rssi;
  bool isKnown;
};

// -----------------------------------------------------------------------------
// FILTROS INTERACTIVOS: BANCOS
// -----------------------------------------------------------------------------
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

const int TOTAL_BANKS = 19;

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

#define MAX_ADS 15

// Refresco periódico - Intervalos Inteligentes (Optimización de cuota Vercel 100k/mes)
const unsigned long FETCH_INTERVAL_AWAKE_MS = 20000; // 20 seg: Pantalla activa despierta
const unsigned long FETCH_INTERVAL_SLEEP_MS = 60000; // 60 seg: Modo salvapantallas (reposo)
const unsigned long FETCH_INTERVAL_ALERT_MS = 15000; // 15 seg: Si hay alerta activa en este mercado
const unsigned long FULL_ALERT_DURATION_MS  = 120000; // 2 minutos

// TASAS DE CAMBIO: BCV E INTERVENCIÓN CAMBIARIA (+0.5%)
const char* BCV_API_URL        = "https://rates.dolarvzla.com/bcv/current.json";
const char* BCV_API_FALLBACK   = "https://ve.dolarapi.com/v1/dolares/oficial";
const unsigned long BCV_FETCH_INTERVAL_MS = 300000; // Consultar BCV cada 5 minutos

// SALVAPANTALLAS "CYBER STARFIELD & CLOCK"
#define NUM_STARS 35
#define SCREENSAVER_TIMEOUT_MS 120000 // 2 minutos
#define SCREENSAVER_FPS_DELAY 60      // ~16 fps
#define SS_CARD_W 240
#define SS_CARD_H 126

struct ScreenStar {
  int16_t x;
  int16_t y;
  uint8_t speed;
  uint16_t color;
};

// CONTROL INTELIGENTE DE BRILLO Y BACKLIGHT (LEDC Hardware PWM)
#define BACKLIGHT_PIN_A   21
#define BACKLIGHT_PIN_B   27
#define BACKLIGHT_FREQ    5000
#define BACKLIGHT_RES     8
#define BRIGHTNESS_FULL   255
#define BRIGHTNESS_DIM    50  // ~20% brillo para descanso térmico en salvapantallas

// TECLADOS VIRTUALES
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
const char KB_R2_SYM[10]   = {';',':','\'','\"','`','~','|','?','!','_'};
const char KB_R3_SYM[7]    = {'1','2','3','4','5','6','7'};

// TECLADO NUMÉRICO TÁCTIL (Monto)
const char* KEYPAD_BTNS[12] = {
  "1", "2", "3",
  "4", "5", "6",
  "7", "8", "9",
  "C", "0", "OK"
};
