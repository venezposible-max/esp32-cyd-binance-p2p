# 🚀 ESP32 CYD - Monitor Binance P2P (USDT/VES) & Tasa BCV + Calculadora de Arbitraje

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Hardware](https://img.shields.io/badge/Hardware-ESP32--2432S028%20(CYD)-blue.svg)](#-hardware-compatible)
[![Platform](https://img.shields.io/badge/Platform-Arduino%20IDE-green.svg)](#-instalación-y-puesta-en-marcha)

Monitor financiero de escritorio en tiempo real para la pantalla **ESP32 Cheap Yellow Display (CYD)**. Visualiza el libro de órdenes P2P de Binance (USDT/VES), la tasa oficial del Banco Central de Venezuela (BCV), la tasa de intervención bancaria (+0.5%), e integra una calculadora táctil completa para arbitraje financiero.

---

## ✨ Características Principales

- **📊 Monitor Binance P2P en Vivo:**
  - Consulta automática cada 10 segundos de las mejores ofertas de compra y venta.
  - Filtro interactivo por banco (Pago Móvil, Banesco, Mercantil, BBVA Provincial, Bancamiga, BNC, BDV, etc.).
  - Filtro táctil por monto en bolívares.
  - Muestra el saldo real disponible en USDT de cada comerciante (`Disp: XXXX $`).
  - Navegación táctil por páginas de ofertas con botones de subida/bajada.

- **🏛️ Tasas Oficiales de Venezuela (BCV e Intervención):**
  - Tasa oficial del BCV obtenida en tiempo real (con sistema de alta disponibilidad y fallback automático).
  - Cálculo dinámico de la **Tasa de Intervención** (BCV + 0.5% bancario).
  - Cálculo del diferencial o spread (%) entre el mercado P2P y la tasa oficial.

- **🧮 Calculadora de Arbitraje Financiero (Modo Dual):**
  - **Modo USDT:** Ingresa la cantidad de dólares a vender (ej. 500 USDT) y calcula el equivalente en Bs.
  - **Modo VES:** Ingresa directamente los bolívares disponibles (ej. 480.000 Bs).
  - Precarga automática de la tasa de venta P2P y la tasa de intervención.
  - Teclado numérico táctil para ingresar comisiones bancarias y de pasarela (BPAY, Wally, Zinli, etc.).
  - Salida directa en USD o venta secundaria P2P con descuento.
  - **Ticket Contable Final:** Muestra ganancia neta en USD, ganancia en Bs y porcentaje de rendimiento (ROI %).

- **🌌 Salvapantallas "Cyber Starfield & Clock":**
  - Activación automática tras 2 minutos de inactividad.
  - Fondo animado de 35 estrellas con efecto parallax.
  - Reloj flotante con tasas en vivo y despertar instantáneo al tocar la pantalla.

- **⚡ Arquitectura Multinúcleo FreeRTOS:**
  - El **Core 0** maneja las peticiones HTTP y WiFi en segundo plano.
  - El **Core 1** maneja el renderizado gráfico de alta velocidad a 55MHz nativos y la digitalización táctil sin congelamientos.

---

## 🛠️ Hardware Compatible

- **Placa:** Sunton ESP32-2432S028 (Conocida popularmente como **CYD - Cheap Yellow Display**, versión Dual USB / MicroUSB o Tipo C).
- **Pantalla:** TFT LCD de 2.8 pulgadas (320x240) con controlador ST7789 / ILI9341.
- **Panel Táctil:** Resistivo XPT2046 integrado por bus SPI.

---

## 📦 Librerías Necesarias (Arduino IDE)

Instala las siguientes librerías desde el **Gestor de Librerías** del Arduino IDE:

1. **`TFT_eSPI`** de Bodmer (v2.5.0 o superior).
2. **`XPT2046_Touchscreen`** de Paul Stoffregen.
3. **`ArduinoJson`** de Benoît Blanchon (versión 6.x recomendada).

---

## ⚙️ Configuración de `User_Setup.h` (TFT_eSPI)

En la carpeta de tu librería `Arduino/libraries/TFT_eSPI/User_Setup.h`, asegúrate de definir los siguientes pines para la pantalla CYD:

```c
#define ST7789_DRIVER
#define TFT_WIDTH  240
#define TFT_HEIGHT 320

#define TFT_MISO 12
#define TFT_MOSI 13
#define TFT_SCLK 14
#define TFT_CS   15
#define TFT_DC    2
#define TFT_RST  -1
#define TFT_BL   21
#define TFT_BACKLIGHT_ON HIGH

#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8

#define SPI_FREQUENCY  55000000
#define SPI_READ_FREQUENCY 20000000
#define SPI_TOUCH_FREQUENCY  2500000
```

---

## 🚀 Instalación y Puesta en Marcha

1. Clona o descarga este repositorio:
   ```bash
   git clone https://github.com/venezposible-max/esp32-cyd-binance-p2p.git
   ```
2. Abre la carpeta `BinanceP2P_CYD` y el archivo `BinanceP2P_CYD.ino` en el Arduino IDE.
3. Al inicio del archivo, edita tus credenciales de red WiFi:
   ```cpp
   const char* DEFAULT_WIFI_SSID = "TU_RED_WIFI";
   const char* DEFAULT_WIFI_PASS = "TU_PASSWORD_WIFI";
   ```
4. Conecta tu placa ESP32 CYD vía USB.
5. Selecciona en el menú de Arduino:
   - **Placa:** `ESP32 Dev Module`
   - **Flash Frequency:** `80MHz`
   - **Partition Scheme:** `Huge APP (3MB No OTA/1MB SPIFFS)`
   - **Upload Speed:** `921600`
6. Haz clic en **Subir**. ¡Listo!

---

## 📄 Licencia

Este proyecto está bajo la Licencia [MIT](LICENSE). Eres libre de usarlo, modificarlo y compartirlo.
