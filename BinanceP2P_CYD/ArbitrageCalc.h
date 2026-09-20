#pragma once
#include <Arduino.h>
#include "Config.h"
#include "GlobalState.h"

/*
 * ======================================================================================
 * ARBITRAGECALC.H - CALCULADORA INTERACTIVA DE ARBITRAJE INTERVENCIÓN / P2P
 * ======================================================================================
 */

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

