// ==================== Battery Monitor ====================

// 电池采样校准参数
// NOTE: 这里默认按 2:1 分压 + ESP32 ADC 3.3V 基准换算。
// 如果实测串口电压明显偏高/偏低，请优先根据你的分压电阻和实测值微调 BATTERY_DIVIDER_RATIO。
#define BATTERY_ADC_REFERENCE_VOLTAGE  3.3f
#define BATTERY_DIVIDER_RATIO           2.0f

// 过滤ANSI转义序列
String filterAnsiEscape(String input) {
  if (input.length() == 0) return input;
  
  String output = "";
  int len = input.length();
  
  for (int i = 0; i < len; i++) {
    if (input[i] == 0x1B) {
      int j = i + 1;
      if (j < len && input[j] == '[') {
        j++;
        while (j < len) {
          char c = input[j];
          if ((c >= '0' && c <= '9') || c == ';' || c == '?' || c == '>' || c == '<' || c == '=') {
            j++;
          } else {
            break;
          }
        }
        if (j < len) {
          char c = input[j];
          if (c >= 0x40 && c <= 0x7E) {
            j++;
          }
        }
        i = j - 1;
        continue;
      }
    }
    output += input[i];
  }
  return output;
}

float getBatteryPercentage(float voltage) {
  // 以锂电池 3.7V 标准进行展示：
  // 4.20V 视为 100%，3.70V 视为约 50%，3.00V 视为 0%
  const float batteryFullVoltage = 4.20f;
  const float batteryNominalVoltage = 3.70f;
  const float batteryEmptyVoltage = 3.00f;

  float percentage = 0.0f;
  if (voltage >= batteryNominalVoltage) {
    percentage = 50.0f + (voltage - batteryNominalVoltage) * 50.0f / (batteryFullVoltage - batteryNominalVoltage);
  } else {
    percentage = (voltage - batteryEmptyVoltage) * 50.0f / (batteryNominalVoltage - batteryEmptyVoltage);
  }

  if (percentage > 100.0f) percentage = 100.0f;
  if (percentage < 0.0f) percentage = 0.0f;
  return percentage;
}

void checkBattery() {
  int adcValue = analogRead(BATTERY_ADC_PIN);
  float adcVoltage = (adcValue / 4095.0f) * BATTERY_ADC_REFERENCE_VOLTAGE;
  batteryVoltage = adcVoltage * BATTERY_DIVIDER_RATIO;
  float batteryPercent = getBatteryPercentage(batteryVoltage);

  if (debugMode) {
    Serial.printf("Battery ADC=%d, ADC Voltage=%.3fV, Battery=%.3fV, Percent=%.0f%%\n",
                  adcValue, adcVoltage, batteryVoltage, batteryPercent);
  }

  if (batteryVoltage < BATTERY_LOW_VOLTAGE && batteryVoltage > 2.0) {
    if (!lowBattery) {
      lowBattery = true;
      if (debugMode) {
        Serial.println("! Low battery warning! Voltage: " + String(batteryVoltage, 3) + "V, " + String((int)batteryPercent) + "%");
      }
      // 非阻塞多次闪烁提示低电量
      requestMultiFlash(CRGB(255, 0, 0), 5, 200);
    }
  } else {
    lowBattery = false;
  }
}

// ==================== SD Card Status Check ====================
void checkSDCardStatus() {
  static bool lastSDCardReady = sdCardReady;
  static unsigned long lastCheck = 0;
  
  if (millis() - lastCheck < 5000) return;
  lastCheck = millis();

  if (sdCardReady) {
    if (!SD.exists("/")) {
      sdCardReady = false;
      sdCardError = true;
      if (lastSDCardReady && debugMode) {
        Serial.println("! SD card removed!");
        Serial.println("! Logging paused");
      }
    }
  } else {
    initSDCard();
    if (sdCardReady && !lastSDCardReady) {
      if (debugMode) {
        Serial.println("! SD card reconnected!");
        Serial.println("! Logging resumed");
      }
    }
  }

  lastSDCardReady = sdCardReady;
}

// ==================== SD Card Management ====================
void initSDCard() {
  digitalWrite(SD_POWER_PIN, HIGH);
  delay(100);

  SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS_PIN);

  if (SD.begin(SD_CS_PIN)) {
    sdCardReady = true;
    sdCardError = false;
    if (currentMode == MODE_SERVER && debugMode) {
      Serial.println("? SD card initialized");
      Serial.println("  Capacity: " + String(SD.cardSize() / (1024 * 1024)) + " MB");
      Serial.println("  SPI pins: SCK=" + String(SD_SCK) + " MISO=" + String(SD_MISO) + " MOSI=" + String(SD_MOSI) + " CS=" + String(SD_CS_PIN));
    }

    if (currentMode == MODE_CLIENT) {
      createDirectory("/client_local");
      createDirectory("/client_u1");    // UART1 日志目录
    } else {
      createDirectory("/server");
      createDirectory("/server/system");
      createDirectory("/server/uart1"); // UART1 日志目录
    }
  } else {
    sdCardReady = false;
    sdCardError = true;
    if (currentMode == MODE_SERVER && debugMode) {
      Serial.println("? SD card init failed - Storage disabled");
      Serial.println("  Check SD card pins: SCK=" + String(SD_SCK) + " MISO=" + String(SD_MISO) + " MOSI=" + String(SD_MOSI) + " CS=" + String(SD_CS_PIN));
    }
  }
}

void powerOffSDCard() {
  if (sdCardReady) {
    SD.end();
    digitalWrite(SD_POWER_PIN, LOW);
    sdCardReady = false;
    if (debugMode) {
      Serial.println("SD card powered off");
    }
  }
}

void powerOnSDCard() {
  if (!sdCardReady) {
    digitalWrite(SD_POWER_PIN, HIGH);
    delay(100);
    initSDCard();
  }
}

void createDirectory(String path) {
  if (sdCardReady && !SD.exists(path)) {
    SD.mkdir(path);
    if (debugMode) {
      Serial.println("  Created directory: " + path);
    }
  }
}

String sanitizeFilename(String input) {
  String result = "";
  for (unsigned int i = 0; i < input.length(); i++) {
    char c = input.charAt(i);
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') || c == '_' || c == '-') {
      result += c;
    } else if (c == ' ') {
      result += '_';
    }
  }
  if (result.length() == 0) {
    result = "UNKNOWN";
  }
  return result;
}

void saveDataToSD(String data, String clientId, bool isServer) {
  if (!sdCardReady) {
    return;
  }

  char path[128];
  char safeClientId[32];
  char clientDir[64];

  clientId.toCharArray(safeClientId, sizeof(safeClientId));
  sanitizeFilenameInPlace(safeClientId);

  if (isServer) {
    snprintf(clientDir, sizeof(clientDir), "/server/%s", safeClientId);
    snprintf(path, sizeof(path), "%s/%s_%s.txt",
             clientDir, logFileName.c_str(), getDateString().c_str());
    createDirectory("/server");
    createDirectory(clientDir);
  } else {
    snprintf(clientDir, sizeof(clientDir), "/client_local");
    snprintf(path, sizeof(path), "%s/%s_%s.txt",
             clientDir, logFileName.c_str(), getDateString().c_str());
    createDirectory("/client_local");
  }

  File file = SD.open(path, FILE_APPEND);
  if (file) {
    file.println(data);
    logFileSize = file.size();
    logCount++;
    file.close();
  } else {
    sdCardError = true;
    sdCardReady = false;
  }
}

void saveServerSystemLog(String data) {
  if (!sdCardReady) {
    return;
  }

  createDirectory("/server");
  createDirectory("/server/system");

  String fileName = "system_" + getDateString() + ".txt";
  char path[64];
  snprintf(path, sizeof(path), "/server/system/%s", fileName.c_str());

  File file = SD.open(path, FILE_APPEND);
  if (file) {
    file.println(data);
    file.close();
  }
}

void sanitizeFilenameInPlace(char* str) {
  int j = 0;
  for (int i = 0; str[i]; i++) {
    char c = str[i];
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') || c == '_' || c == '-') {
      str[j++] = c;
    } else if (c == ' ') {
      str[j++] = '_';
    }
  }
  str[j] = '\0';
  if (j == 0) {
    strcpy(str, "UNKNOWN");
  }
}

// ==================== SD异步写入队列 ====================
#define SD_WRITE_QUEUE_SIZE 256
#define SD_WRITE_BATCH_SIZE 32
#define SD_WRITE_INTERVAL_MS 1000  // 每1秒批量写入一次

typedef struct {
  String data;
  String clientId;
  bool isServer;
  uint8_t uartChannel;  // 1=UART1, 2=UART2(默认)
} SDLogEntry;

SDLogEntry sdWriteQueue[SD_WRITE_QUEUE_SIZE];
int sdQueueHead = 0;
int sdQueueTail = 0;
int sdQueueCount = 0;
unsigned long lastSDWriteTime = 0;
portMUX_TYPE sdQueueMux = portMUX_INITIALIZER_UNLOCKED;

String getLogTimestamp() {
  // 如果有RTC时间可以使用，应该先检查RTC
  // 这里使用系统运行时间作为时间戳
  unsigned long uptime = millis();
  unsigned long seconds = uptime / 1000;
  unsigned long hours = seconds / 3600;
  unsigned long minutes = (seconds % 3600) / 60;
  unsigned long secs = seconds % 60;
  unsigned long ms = uptime % 1000;
  
  char buf[32];
  snprintf(buf, sizeof(buf), "[%02lu:%02lu:%02lu.%03lu]", hours, minutes, secs, ms);
  return String(buf);
}

bool enqueueSDLog(String data, String clientId, bool isServer, uint8_t uartChannel) {
  if (!sdCardReady || data.length() == 0) return false;

  // 过滤ANSI转义序列并添加时间戳（在临界区之外完成，减少锁占用时间）
  String filteredData = filterAnsiEscape(data);
  if (logWithTimestamp) {
    String timestamp = getLogTimestamp();
    filteredData = timestamp + " " + filteredData;
  }

  portENTER_CRITICAL(&sdQueueMux);
  if (sdQueueCount >= SD_WRITE_QUEUE_SIZE) {
    portEXIT_CRITICAL(&sdQueueMux);
    return false;
  }

  sdWriteQueue[sdQueueHead].data = filteredData;
  sdWriteQueue[sdQueueHead].clientId = clientId;
  sdWriteQueue[sdQueueHead].isServer = isServer;
  sdWriteQueue[sdQueueHead].uartChannel = uartChannel;
  sdQueueHead = (sdQueueHead + 1) % SD_WRITE_QUEUE_SIZE;
  sdQueueCount++;
  portEXIT_CRITICAL(&sdQueueMux);
  return true;
}

void processSDWriteQueue() {
  if (!sdCardReady) return;

  unsigned long now = millis();
  portENTER_CRITICAL(&sdQueueMux);
  if (sdQueueCount == 0) {
    portEXIT_CRITICAL(&sdQueueMux);
    return;
  }
  if (now - lastSDWriteTime < SD_WRITE_INTERVAL_MS && sdQueueCount < SD_WRITE_BATCH_SIZE) {
    portEXIT_CRITICAL(&sdQueueMux);
    return;
  }

  lastSDWriteTime = now;

  int batchSize = min(sdQueueCount, SD_WRITE_BATCH_SIZE);

  // 为减少临界区内的工作量，先把要写入的条目复制到本地数组，然后释放锁进行实际写入
  SDLogEntry batch[SD_WRITE_BATCH_SIZE];
  for (int i = 0; i < batchSize; i++) {
    batch[i] = sdWriteQueue[sdQueueTail];
    sdQueueTail = (sdQueueTail + 1) % SD_WRITE_QUEUE_SIZE;
    sdQueueCount--;
  }
  portEXIT_CRITICAL(&sdQueueMux);

  for (int i = 0; i < batchSize; i++) {
    SDLogEntry &entry = batch[i];

    char path[128];
    char safeClientId[32];
    char clientDir[64];

    entry.clientId.toCharArray(safeClientId, sizeof(safeClientId));
    sanitizeFilenameInPlace(safeClientId);

    if (entry.uartChannel == 1) {
      // UART1 存入独立目录，与 UART2 完全隔离
      if (entry.isServer) {
        snprintf(clientDir, sizeof(clientDir), "/server/uart1");
        createDirectory("/server/uart1");
      } else {
        snprintf(clientDir, sizeof(clientDir), "/client_u1");
        createDirectory("/client_u1");
      }
      snprintf(path, sizeof(path), "%s/%s_%s.txt",
               clientDir, logFileName.c_str(), getDateString().c_str());
    } else {
      if (entry.isServer) {
        snprintf(clientDir, sizeof(clientDir), "/server/%s", safeClientId);
        snprintf(path, sizeof(path), "%s/%s_%s.txt",
                 clientDir, logFileName.c_str(), getDateString().c_str());
      } else {
        snprintf(clientDir, sizeof(clientDir), "/client_local");
        snprintf(path, sizeof(path), "%s/%s_%s.txt",
                 clientDir, logFileName.c_str(), getDateString().c_str());
      }
    }

    File file = SD.open(path, FILE_APPEND);
    if (file) {
      file.println(entry.data);
      file.close();
    }
  }
}
