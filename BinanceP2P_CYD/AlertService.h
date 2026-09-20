#pragma once
#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "Config.h"
#include "GlobalState.h"

/*
 * ======================================================================================
 * ALERTSERVICE.H - MOTOR DE ALERTAS DUALES P2P, MODALES, NOTIFICACIONES Y TELEGRAM
 * ======================================================================================
 */

bool sendTelegramP2PAlert(P2PAd topAd, float targetPrice) {
  if (WiFi.status() != WL_CONNECTED) return false;

  WiFiClientSecure alertClient;
  alertClient.setInsecure();
  HTTPClient alertHttp;
  alertHttp.setTimeout(5000);

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

  bool delivered = false;

  // Hasta 3 intentos automáticos si hay microcortes de WiFi o latencia
  for (int attempt = 1; attempt <= 3; attempt++) {
    if (WiFi.status() != WL_CONNECTED) {
      vTaskDelay(pdMS_TO_TICKS(800));
      continue;
    }

    if (alertHttp.begin(alertClient, TELEGRAM_ALERT_URL)) {
      alertHttp.addHeader("Content-Type", "application/json");
      int httpCode = alertHttp.POST(body);
      if (httpCode == HTTP_CODE_OK || httpCode == 201) {
        delivered = true;
        alertHttp.end();
        break; // Confirmación exitosa de entrega
      }
      alertHttp.end();
    }

    if (attempt < 3) {
      vTaskDelay(pdMS_TO_TICKS(1000));
    }
  }

  return delivered;
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

    // Señalizar a Core 1 para que realice la transición gráfica de forma segura en el bus SPI
    pendingAlertTransition = true;

    // 1. Enviar notificación a Telegram con reintentos automáticos
    bool telegramSuccess = sendTelegramP2PAlert(adsList[0], target);
    telegramAlertDelivered = telegramSuccess;

    // 2. SOLO si Telegram confirmó la recepción exitosa (HTTP 200), desactivar la meta en NVS
    if (telegramSuccess) {
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
      Serial.println("[ALERTA P2P] Notificacion entregada en Telegram. Meta cumplida y desactivada.");
    } else {
      Serial.println("[ALERTA P2P] Fallo de red con Telegram tras 3 intentos. Meta permanece ARMADA.");
    }

    // Señalizar a Core 1 para actualizar pie de pantalla con el estado de entrega
    pendingAlertFooterUpdate = true;
  }
}


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

  tft.fillRect(10, 218, 300, 16, COLOR_BG);
  if (telegramAlertDelivered) {
    tft.setTextColor(COLOR_BUY_GREEN, COLOR_BG);
    tft.drawCentreString("Telegram enviado OK | Meta cumplida y desactivada", 160, 222, 1);
  } else {
    tft.setTextColor(COLOR_BINANCE_YEL, COLOR_BG);
    tft.drawCentreString("Sin conexion Telegram | Meta sigue armada", 160, 222, 1);
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

