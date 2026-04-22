// ==================== Smart WiFi Config ====================
extern int selectedClientIndex;

void startConfigMode() {
  Serial.println("\nEntering WiFi config mode...");
  Serial.println("Please connect to WiFi: " + String(wifimanager_ssid));
  Serial.println("Password: " + String(wifimanager_password));
  Serial.println("Then visit: http://192.168.4.1");
  Serial.println("You can continue using AT commands during config");
  Serial.println("Type AT+EXITCONFIG to exit config mode");

  wm.resetSettings();
  wm.setAPStaticIPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
  wm.setConfigPortalTimeout(300);

  wm.setConfigPortalBlocking(false);
  wm.startConfigPortal(wifimanager_ssid, wifimanager_password);

  inConfigMode = true;
  configModeStartTime = millis();

  Serial.println("OK Config mode started (non-blocking)");
}

void handleConfigMode() {
  if (!inConfigMode) return;

  wm.process();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nOK WiFi config successful!");
    Serial.println("  SSID: " + WiFi.SSID());
    Serial.println("  IP: " + WiFi.localIP().toString());
    inConfigMode = false;
    wm.stopConfigPortal();

    Serial.println("System will restart...");
    delay(1000);
    ESP.restart();
  }

  if (millis() - configModeStartTime > configModeTimeout) {
    Serial.println("\nX Config timeout, restarting...");
    inConfigMode = false;
    wm.stopConfigPortal();
    ESP.restart();
  }
}

void exitConfigMode() {
  if (inConfigMode) {
    Serial.println("Exiting config mode...");
    inConfigMode = false;
    wm.stopConfigPortal();
    Serial.println("OK Exited config mode");
  } else {
    Serial.println("Not in config mode");
  }
}

void configModeCallback(WiFiManager *myWiFiManager) {
  Serial.println("Entered config mode");
  setLED(CRGB(255, 255, 0));
}

// ==================== Client Mode ====================
void initClientMode() {
  if (debugMode) {
    Serial.println("Initializing client mode...");
  }

  pinMode(CONFIG_MODE_PIN, INPUT_PULLUP);
  if (digitalRead(CONFIG_MODE_PIN) == LOW) {
    if (debugMode) {
      Serial.println("Config mode triggered, entering WiFi config...");
    }
    startConfigMode();
    return;
  }

  WiFi.begin(client_wifi_ssid, client_wifi_password);

  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
    yield();
    if (debugMode) {
      Serial.print(".");
    }

    if (millis() - startTime > 15000) {
      if (debugMode) {
        Serial.println("\nWiFi connection timeout, starting config mode...");
      }
      startConfigMode();
      return;
    }
  }

  wifiConnected = true;
  if (debugMode) {
    Serial.println("\nWiFi connected");
    Serial.println("  Client IP: " + WiFi.localIP().toString());
  }

  connectToServer();
}

void connectToServer() {
  if (debugMode) {
    Serial.print("Connecting to server (" + String(server_ip) + ":" + String(server_port) + ")...");
  }

  if (tcpClient.connect(server_ip, server_port)) {
    tcpConnected = true;
    if (debugMode) {
      Serial.println(" Success");
    }

    String clientInfo = "CLIENT_ID:" + client_id + "|TIMESTAMP:" + String(millis());
    tcpClient.println(clientInfo);

    if (debugMode) {
      Serial.println("  Client ID: " + client_id);
    }
  } else {
    tcpConnected = false;
    if (debugMode) {
      Serial.println(" Failed");
    }
  }
}

void runClientMode() {
  static unsigned long lastConnectAttempt = 0;
  static unsigned long wifiConnectStart = 0;
  static bool wifiConnecting = false;
  const unsigned long reconnectionInterval = 5000;
  const unsigned long wifiConnectTimeout = 15000;
  static String tcpLineBuffer = "";

  wl_status_t wifiStatus = WiFi.status();

  if (!wifiConnected || wifiStatus != WL_CONNECTED) {
    if (!wifiConnecting && (wifiStatus == WL_IDLE_STATUS || wifiStatus == WL_DISCONNECTED || wifiStatus == WL_NO_SSID_AVAIL || wifiStatus == WL_CONNECT_FAILED)) {
      wifiConnecting = true;
      wifiConnectStart = millis();
      WiFi.begin(client_wifi_ssid, client_wifi_password);
    }

    if (wifiStatus == WL_CONNECTED) {
      wifiConnected = true;
      wifiConnecting = false;
      connectToServer();
    } else if (millis() - wifiConnectStart > wifiConnectTimeout) {
      wifiConnecting = false;
    }
  }

  if (!tcpConnected && wifiConnected) {
    if (millis() - lastConnectAttempt > reconnectionInterval) {
      connectToServer();
      lastConnectAttempt = millis();
    }
  }

  if (tcpConnected && tcpClient.available()) {
    size_t available = tcpClient.available();
    size_t toRead = min(available, (size_t)256);
    uint8_t buf[256];
    size_t readBytes = tcpClient.read(buf, toRead);

    // 使用DMA发送
    uart_write_bytes(UART_NUM_2, (const char *)buf, readBytes);
    
    // 记录接收的日志
    if (logToSD && sdCardReady) {
      for (size_t j = 0; j < readBytes; j++) {
        char c = buf[j];
        if (c == '\n') {
          if (tcpLineBuffer.length() > 1) {
            enqueueSDLog(tcpLineBuffer, client_id, false);
          }
          tcpLineBuffer = "";
        } else if (c != '\r') {
          tcpLineBuffer += c;
        }
      }
    }
  }

  if (tcpConnected && !tcpClient.connected()) {
    if (debugMode) {
      Serial.println("TCP disconnected");
    }
    tcpClient.stop();
    tcpConnected = false;
  }
}

// ==================== Server Mode ====================
void initServerMode() {
  if (debugMode) {
    Serial.println("Initializing server mode...");
  }

  delay(200);

  WiFi.mode(WIFI_AP);

  esp_wifi_set_max_tx_power(40);

  IPAddress localIP(192, 168, 1, 1);
  IPAddress gateway(192, 168, 1, 1);
  IPAddress subnet(255, 255, 255, 0);
  WiFi.softAPConfig(localIP, gateway, subnet);

  if (WiFi.softAP(ap_ssid, ap_password)) {
    wifiConnected = true;
    if (debugMode) {
      Serial.println("WiFi AP started");
      Serial.println("  AP SSID: " + String(ap_ssid));
      Serial.println("  AP IP: " + WiFi.softAPIP().toString());
    }
    if (logToSD && sdCardReady) {
      saveServerSystemLog("WiFi AP started - SSID: " + String(ap_ssid) + ", IP: " + WiFi.softAPIP().toString());
    }
  } else {
    wifiConnected = false;
    if (debugMode) {
      Serial.println("WiFi AP failed to start");
    }
    if (logToSD && sdCardReady) {
      saveServerSystemLog("WiFi AP failed to start");
    }
  }

  tcpServer.begin();
  if (debugMode) {
    Serial.println("TCP server started");
    Serial.println("  Listen port: " + String(server_listen_port));
  }
  if (logToSD && sdCardReady) {
    saveServerSystemLog("TCP server started on port " + String(server_listen_port));
  }

  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (serverClients[i]) {
      serverClients[i].stop();
    }
    clientSerialData[i] = "";
    clientLineBuffer[i] = "";
  }
}

void runServerMode() {
  WiFiClient newClient = tcpServer.available();
  if (newClient) {
    IPAddress newIP = newClient.remoteIP();
    int replaceSlot = -1;
    int freeSlot = -1;

    for (int i = 0; i < MAX_CLIENTS; i++) {
      if (serverClients[i] && serverClients[i].connected()) {
        if (serverClients[i].remoteIP() == newIP) {
          replaceSlot = i;
          break;
        }
      } else if (freeSlot < 0) {
        freeSlot = i;
      }
    }

    if (replaceSlot >= 0) {
      serverClients[replaceSlot].stop();
      serverClients[replaceSlot] = newClient;
      clientSerialData[replaceSlot] = "// Reconnected\n";
      serverClients[replaceSlot].println("Welcome back to ESP32 UART Server");
      if (logToSD && sdCardReady) {
        saveServerSystemLog("Client reconnected: " + newIP.toString());
      }
    } else if (freeSlot >= 0) {
      serverClients[freeSlot] = newClient;
      clientSerialData[freeSlot] = "// Serial data log\n";
      serverClients[freeSlot].println("Welcome to ESP32 UART Server");
      if (logToSD && sdCardReady) {
        saveServerSystemLog("New client connected: " + newIP.toString());
      }
    } else {
      newClient.println("Server client limit reached");
      newClient.stop();
      if (logToSD && sdCardReady) {
        saveServerSystemLog("Client connection rejected: server limit reached - " + newIP.toString());
      }
    }
  }

  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (serverClients[i] && serverClients[i].connected()) {
      size_t available = serverClients[i].available();
      if (available > 0) {
        size_t toRead = min(available, (size_t)256);
        uint8_t buf[256];
        size_t readBytes = serverClients[i].read(buf, toRead);

        // 写入网页串口显示缓冲区（全局）
        noInterrupts();
        appendToSerialBuffer((char*)buf, readBytes);
        interrupts();
        
        // 写入客户端专属缓冲区（用于客户端详情页）
        for (size_t j = 0; j < readBytes; j++) {
          if (buf[j] != '\r') {
            clientSerialData[i] += (char)buf[j];
          }
        }
        
        // 只有选中客户端时才透传到UART2和调试串口
        if (selectedClientIndex >= 0 && selectedClientIndex == i) {
          Serial.write((char*)buf, readBytes);
          uart_write_bytes(UART_NUM_2, (const char *)buf, readBytes);
        }

        // 记录日志到SD卡
        for (size_t j = 0; j < readBytes; j++) {
          char c = buf[j];
          if (c == '\n') {
            clientLineBuffer[i] += '\n';
            if (clientLineBuffer[i].length() > 1) {
              if (logToSD && sdCardReady) {
                String clientId = serverClients[i].remoteIP().toString();
                enqueueSDLog(clientLineBuffer[i], clientId, true);
              }
            }
            clientLineBuffer[i] = "";
          } else if (c != '\r') {
            clientLineBuffer[i] += c;
          }
        }

        if (clientSerialData[i].length() > 2000) {
          clientSerialData[i] = clientSerialData[i].substring(clientSerialData[i].length() - 1500);
        }
        if (clientLineBuffer[i].length() > 500) {
          clientLineBuffer[i] = "";
        }
      }
    } else if (serverClients[i]) {
      IPAddress clientIP = serverClients[i].remoteIP();
      serverClients[i].stop();
      clientLineBuffer[i] = "";
      if (logToSD && sdCardReady) {
        saveServerSystemLog("Client disconnected: " + clientIP.toString());
      }
    }
  }

  yield();
}

String parseClientId(String data) {
  int idStart = data.indexOf("CLIENT_ID:");
  if (idStart >= 0) {
    int idEnd = data.indexOf("|", idStart);
    if (idEnd >= 0) {
      return data.substring(idStart + 10, idEnd);
    }
  }
  return "UNKNOWN";
}

int getConnectedClientCount() {
  int count = 0;
  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (serverClients[i] && serverClients[i].connected()) {
      count++;
    }
  }
  return count;
}