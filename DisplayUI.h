#pragma once
#include <Arduino.h>
#include "Config.h"
#include "GlobalState.h"

/*
 * ======================================================================================
 * DISPLAYUI.H - RENDERIZADO GRÁFICO TFT, SALVAPANTALLAS, MODALES Y GESTOR TÁCTIL
 * ======================================================================================
 */

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


void drawUI() {
  drawHeader();
  drawTabsAndFilters();
  drawAdsList();
  drawBottomBar();
}


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
  setBacklightBrightness(BRIGHTNESS_DIM); // Atenuar brillo al 20% para descanso térmico
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
  setBacklightBrightness(BRIGHTNESS_FULL); // 100% brillo al despertar
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

  // Filtrar toques fantasmas y ruido analógico descartando presiones espurias
  if (p.z < 150) {
    return;
  }

  // Doble muestra rápida para suavizado y eliminación de jitter analógico
  if (ts.touched()) {
    TS_Point p2 = ts.getPoint();
    if (p2.z >= 150) {
      p.x = (p.x + p2.x) / 2;
      p.y = (p.y + p2.y) / 2;
    }
  }

  // Mapeo calibrado de alta precisión para pantalla resistiva CYD (320x240) sin zonas muertas
  int x = map(p.x, 200, 3750, 0, 319);
  int y = map(p.y, 200, 3750, 0, 239);
  x = constrain(x, 0, 319);
  y = constrain(y, 0, 239);

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

