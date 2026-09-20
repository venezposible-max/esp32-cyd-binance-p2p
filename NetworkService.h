#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "Config.h"
#include "GlobalState.h"

/*
 * ======================================================================================
 * NETWORKSERVICE.H - GESTIÓN DE WIFI, CLIENTE HTTP, TAREAS FREERTOS Y APIS
 * ======================================================================================
 */

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
  // Mostrar pantalla de búsqueda centrada en modo horizontal 320x240
  tft.fillRoundRect(16, 6, 288, 228, 8, COLOR_CARD_BG);
  tft.drawRoundRect(16, 6, 288, 228, 8, COLOR_CARD_BORDER);
  tft.setTextColor(COLOR_BINANCE_YEL, COLOR_CARD_BG);
  tft.drawCentreString("REDES WIFI", 160, 20, 2);
  tft.setTextColor(COLOR_CYAN, COLOR_CARD_BG);
  tft.drawCentreString("Buscando redes...", 160, 110, 2);

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
        
        xSemaphoreGive(p2pMutex);
      }

      // Evaluar si la oferta #1 cumple con la alerta configurada (fuera del Mutex para no bloquear a Core 1)
      checkP2PAlerts();

      http.end();
      isFetching = false;
      return true;
    }
  }

  http.end();
  isFetching = false;
  return false;
}


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
        // Cálculo del intervalo adaptativo inteligente
        bool alertActive = (currentTradeType == "BUY") ? isAlertBuyActive : isAlertSellActive;
        unsigned long currentInterval = FETCH_INTERVAL_AWAKE_MS; // 25s en pantalla activa
        if (alertActive) {
          currentInterval = FETCH_INTERVAL_ALERT_MS; // 20s si hay alerta armada (incluso en salvapantallas)
        } else if (isScreensaverActive) {
          currentInterval = FETCH_INTERVAL_SLEEP_MS; // 90s en reposo normal sin alertas
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

