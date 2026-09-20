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

#include "Config.h"
#include "GlobalState.h"
#include "NetworkService.h"
#include "AlertService.h"
#include "ArbitrageCalc.h"
#include "DisplayUI.h"

// -----------------------------------------------------------------------------
// SETUP Y LOOP PRINCIPALES
// -----------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  Serial.println("\n[CREALO P2P] Monitor CYD Horizontal (320x240)...");

  // Crear Mutex de FreeRTOS para blindar memoria RAM entre Core 0 y Core 1
  p2pMutex = xSemaphoreCreateMutex();

  // Control Inteligente de Retroiluminación (PWM Hardware)
  initBacklight();

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

