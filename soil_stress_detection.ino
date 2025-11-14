#include <WiFi.h>
#include <WebServer.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>
#include <DHT.h>
#include <ArduinoOTA.h>
#include <HTTPClient.h>  // Add this for ThingSpeak

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// === Sensor Pins ===
#define DHTPIN 4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

#define SOIL_PIN 34
#define RELAY_PIN 23
#define BUZZER_PIN 2

// === Wi-Fi Credentials ===
const char* ssid = "prasanna";
const char* password = "40294029";

// === ThingSpeak Configuration ===
const char* thingSpeakApiKey = "ZLURD776FLC7SB4G"; 
const char* thingSpeakURL = "http://api.thingspeak.com/update";
unsigned long thingSpeakChannel = 2743336; 

// ThingSpeak field definitions
const int FIELD_TEMP = 1;
const int FIELD_HUMIDITY = 2;
const int FIELD_SOIL = 3;
const int FIELD_STRESS = 4;
const int FIELD_PUMP = 5;
const int FIELD_STRESS_LEVEL = 6;

// === ThingSpeak Timing ===
unsigned long lastThingSpeakUpdate = 0;
const unsigned long THINGSPEAK_INTERVAL = 30000; // Update every 30 seconds
bool thingSpeakEnabled = true;

// === Soil Stress Parameters ===
const int SOIL_CRITICAL_STRESS = 15;
const int SOIL_HIGH_STRESS = 25;
const int SOIL_MODERATE_STRESS = 35;
const int SOIL_LOW_STRESS = 45;
const int SOIL_NO_STRESS = 60;
const int SOIL_WATERLOGGED = 80;

// Temperature stress thresholds
const float TEMP_HEAT_STRESS = 35.0;
const float TEMP_COLD_STRESS = 10.0;

// Humidity stress thresholds
const float HUMIDITY_HIGH_STRESS = 85.0;
const float HUMIDITY_LOW_STRESS = 30.0;

// === Control Thresholds ===
const int SOIL_ON = 30;
const int SOIL_OFF = 45;
bool relayState = false;
bool manualControl = false;

// === Stress Monitoring Variables ===
struct StressLevel {
  String level;
  String description;
  String color;
  int severity;
  String icon;
};

StressLevel currentStress;
unsigned long stressStartTime = 0;
bool stressAlertActive = false;
String stressHistory[5];
int stressHistoryIndex = 0;

// === Data Logging ===
const int MAX_DATA_POINTS = 50;
struct SensorData {
  unsigned long timestamp;
  float temperature;
  float humidity;
  int soilMoisture;
  int stressLevel;
  bool pumpState;
  String operationMode;
};

SensorData dataLog[MAX_DATA_POINTS];
int dataIndex = 0;
unsigned long lastUpdate = 0;

// === Non-Blocking Timers ===
unsigned long lastSensorRead = 0;
unsigned long lastOLEDUpdate = 0;
unsigned long lastBuzzerAlert = 0;
const unsigned long SENSOR_INTERVAL = 5000;
const unsigned long OLED_INTERVAL = 2000;
const unsigned long BUZZER_INTERVAL = 30000;

WebServer server(80);

// ---------- Initialize Data Arrays ----------
void initializeHistory() {
  for(int i = 0; i < MAX_DATA_POINTS; i++) {
    dataLog[i] = {0, 0.0, 0.0, 0, 0, false, ""};
  }
  for(int i = 0; i < 5; i++) {
    stressHistory[i] = "";
  }
}

// ---------- ThingSpeak Cloud Integration ----------
void updateThingSpeak(float temp, float hum, int soil, int stressSeverity, bool pumpState, String stressLevel) {
  if (!thingSpeakEnabled || strlen(thingSpeakApiKey) == 0) {
    return;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    
    // Construct the URL with all data fields
    String url = String(thingSpeakURL);
    url += "?api_key=" + String(thingSpeakApiKey);
    url += "&field" + String(FIELD_TEMP) + "=" + String(temp, 1);
    url += "&field" + String(FIELD_HUMIDITY) + "=" + String(hum, 1);
    url += "&field" + String(FIELD_SOIL) + "=" + String(soil);
    url += "&field" + String(FIELD_STRESS) + "=" + String(stressSeverity);
    url += "&field" + String(FIELD_PUMP) + "=" + String(pumpState ? 1 : 0);
    url += "&field" + String(FIELD_STRESS_LEVEL) + "=" + String(stressLevel);
    
    Serial.println("☁️ Sending data to ThingSpeak...");
    Serial.println("URL: " + url.substring(0, 100) + "..."); // Print partial URL for debugging
    
    http.begin(url);
    int httpResponseCode = http.GET();
    
    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.println("✅ ThingSpeak Update - Code: " + String(httpResponseCode) + ", Response: " + response);
    } else {
      Serial.println("❌ ThingSpeak Error: " + String(httpResponseCode));
    }
    
    http.end();
  } else {
    Serial.println("❌ WiFi disconnected - Cannot update ThingSpeak");
  }
}

// ---------- Soil Stress Analysis Algorithm ----------
StressLevel analyzeSoilStress(float temp, float hum, int soilMoisture) {
  StressLevel stress;
  int stressScore = 0;
  String factors = "";

  // Soil Moisture Stress Analysis
  if (soilMoisture <= SOIL_CRITICAL_STRESS) {
    stressScore += 8;
    factors += "Extreme Drought, ";
    stress.icon = "🔥";
  } else if (soilMoisture <= SOIL_HIGH_STRESS) {
    stressScore += 6;
    factors += "Severe Drought, ";
    stress.icon = "⚠️";
  } else if (soilMoisture <= SOIL_MODERATE_STRESS) {
    stressScore += 4;
    factors += "Moderate Drought, ";
    stress.icon = "📋";
  } else if (soilMoisture >= SOIL_WATERLOGGED) {
    stressScore += 7;
    factors += "Waterlogged, ";
    stress.icon = "💦";
  } else if (soilMoisture >= SOIL_NO_STRESS) {
    stressScore += 0;
    factors += "Optimal Moisture, ";
    stress.icon = "✅";
  } else {
    stressScore += 1;
    factors += "Adequate Moisture, ";
    stress.icon = "🌱";
  }

  // Temperature Stress Analysis
  if (temp >= TEMP_HEAT_STRESS) {
    stressScore += 6;
    factors += "Heat Stress, ";
  } else if (temp <= TEMP_COLD_STRESS) {
    stressScore += 5;
    factors += "Cold Stress, ";
  }

  // Humidity Stress Analysis
  if (hum >= HUMIDITY_HIGH_STRESS) {
    stressScore += 4;
    factors += "High Humidity, ";
  } else if (hum <= HUMIDITY_LOW_STRESS) {
    stressScore += 3;
    factors += "Low Humidity, ";
  }

  // Remove trailing comma
  if (factors.length() > 0) {
    factors = factors.substring(0, factors.length() - 2);
  }

  // Determine overall stress level
  if (stressScore >= 12) {
    stress.level = "CRITICAL";
    stress.description = "Multiple stress factors detected";
    stress.color = "#e74c3c";
    stress.severity = 10;
  } else if (stressScore >= 8) {
    stress.level = "HIGH";
    stress.description = "Significant plant stress";
    stress.color = "#e67e22";
    stress.severity = 7;
  } else if (stressScore >= 5) {
    stress.level = "MODERATE";
    stress.description = "Moderate stress conditions";
    stress.color = "#f39c12";
    stress.severity = 5;
  } else if (stressScore >= 2) {
    stress.level = "LOW";
    stress.description = "Mild stress observed";
    stress.color = "#f1c40f";
    stress.severity = 3;
  } else {
    stress.level = "OPTIMAL";
    stress.description = "Ideal growing conditions";
    stress.color = "#2ecc71";
    stress.severity = 0;
    stress.icon = "🌿";
  }

  return stress;
}

// ---------- Plant Health Recommendation Engine ----------
String getPlantRecommendations(StressLevel stress, float temp, float hum, int soil) {
  String recommendations = "";
  
  if (stress.level == "CRITICAL") {
    recommendations += "🚨 IMMEDIATE ACTION REQUIRED: ";
    if (soil <= SOIL_CRITICAL_STRESS) {
      recommendations += "Emergency watering needed! ";
    }
    if (temp >= TEMP_HEAT_STRESS) {
      recommendations += "Provide shade or cooling! ";
    }
    if (soil >= SOIL_WATERLOGGED) {
      recommendations += "Improve drainage immediately! ";
    }
  } else if (stress.level == "HIGH") {
    recommendations += "⚠️ URGENT: ";
    if (soil <= SOIL_HIGH_STRESS) {
      recommendations += "Water plants immediately. ";
    }
    if (temp >= TEMP_HEAT_STRESS) {
      recommendations += "Consider shading. ";
    }
  } else if (stress.level == "MODERATE") {
    recommendations += "📋 Monitor: ";
    if (soil <= SOIL_MODERATE_STRESS) {
      recommendations += "Schedule watering soon. ";
    }
    if (temp >= 30.0) {
      recommendations += "Watch for heat stress. ";
    }
  } else if (stress.level == "LOW") {
    recommendations += "✅ Normal: ";
    recommendations += "Maintain current care routine. ";
  } else {
    recommendations += "🌱 Optimal: ";
    recommendations += "Ideal growing conditions maintained. ";
  }

  // Specific recommendations
  if (soil <= SOIL_MODERATE_STRESS) {
    recommendations += "💧 Irrigation recommended. ";
  }
  if (temp >= 32.0) {
    recommendations += "🌡️ High temperature detected. ";
  }
  if (hum >= 80.0) {
    recommendations += "💦 High humidity - watch for fungi. ";
  }

  return recommendations;
}

// ---------- Stress Alert System ----------
void checkStressAlerts(StressLevel stress) {
  if (stress.level == "CRITICAL" || stress.level == "HIGH") {
    if (!stressAlertActive) {
      stressStartTime = millis();
      stressAlertActive = true;
      Serial.println("🚨 STRESS ALERT: " + stress.level + " - " + stress.description);
      
      String timestamp = String(millis() / 1000) + "s";
      stressHistory[stressHistoryIndex] = timestamp + ": " + stress.level + " - " + stress.description;
      stressHistoryIndex = (stressHistoryIndex + 1) % 5;
      
      if (stress.level == "CRITICAL") {
        triggerBuzzer(1000);
      }
    }
  } else {
    if (stressAlertActive) {
      unsigned long stressDuration = (millis() - stressStartTime) / 1000;
      Serial.println("✅ Stress resolved. Duration: " + String(stressDuration) + " seconds");
      stressAlertActive = false;
    }
  }
}

// ---------- Buzzer Alert System ----------
void triggerBuzzer(int duration) {
  if (millis() - lastBuzzerAlert > BUZZER_INTERVAL) {
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, HIGH);
    delay(duration);
    digitalWrite(BUZZER_PIN, LOW);
    lastBuzzerAlert = millis();
  }
}

// ---------- Data Helper Functions ----------
String getTemperatureData() {
  String data = "";
  for(int i = 0; i < dataIndex; i++) {
    if(i > 0) data += ",";
    data += String(dataLog[i].temperature, 1);
  }
  return data;
}

String getHumidityData() {
  String data = "";
  for(int i = 0; i < dataIndex; i++) {
    if(i > 0) data += ",";
    data += String(dataLog[i].humidity, 1);
  }
  return data;
}

String getSoilData() {
  String data = "";
  for(int i = 0; i < dataIndex; i++) {
    if(i > 0) data += ",";
    data += String(dataLog[i].soilMoisture);
  }
  return data;
}

String getStressData() {
  String data = "";
  for(int i = 0; i < dataIndex; i++) {
    if(i > 0) data += ",";
    data += String(dataLog[i].stressLevel);
  }
  return data;
}

// ---------- Generate CSV Data (FIXED) ----------
String generateCSV() {
  String csv = "Timestamp (ms),Temperature (C),Humidity (%),Soil Moisture (%),Stress Level (0-10),Pump State,Operation Mode\n";
  
  for(int i = 0; i < dataIndex; i++) {
    csv += String(dataLog[i].timestamp) + ",";
    csv += String(dataLog[i].temperature, 1) + ",";
    csv += String(dataLog[i].humidity, 1) + ",";
    csv += String(dataLog[i].soilMoisture) + ",";
    csv += String(dataLog[i].stressLevel) + ",";
    
    // Fixed string concatenation
    if (dataLog[i].pumpState) {
      csv += "ON";
    } else {
      csv += "OFF";
    }
    csv += ",";
    
    csv += dataLog[i].operationMode + "\n";
  }
  
  Serial.println("📊 CSV generated with " + String(dataIndex) + " records");
  return csv;
}

// ---------- Generate Chart Data for JavaScript ----------
String generateChartData() {
  String data = "{";
  data += "\"temperature\":[" + getTemperatureData() + "],";
  data += "\"humidity\":[" + getHumidityData() + "],";
  data += "\"soil\":[" + getSoilData() + "],";
  data += "\"stress\":[" + getStressData() + "],";
  data += "\"labels\":[";
  for(int i = 0; i < dataIndex; i++) {
    if(i > 0) data += ",";
    data += "\"" + String(i+1) + "\"";
  }
  data += "],";
  data += "\"count\":" + String(dataIndex);
  data += "}";
  return data;
}

// ---------- CSV Download Handler ----------
void handleCSVDownload() {
  String csvData = generateCSV();
  
  // Set headers for file download
  server.sendHeader("Content-Type", "text/csv");
  server.sendHeader("Content-Disposition", "attachment; filename=plant_stress_data.csv");
  server.sendHeader("Connection", "close");
  
  server.send(200, "text/csv", csvData);
  Serial.println("📥 CSV data downloaded");
}

// ---------- Data Export Handler (JSON) ----------
void handleDataExport() {
  String jsonData = "{\n";
  jsonData += "  \"metadata\": {\n";
  jsonData += "    \"total_records\": " + String(dataIndex) + ",\n";
  jsonData += "    \"export_timestamp\": " + String(millis()) + ",\n";
  jsonData += "    \"device_id\": \"ESP32_Plant_Monitor\"\n";
  jsonData += "  },\n";
  jsonData += "  \"sensor_data\": [\n";
  
  for(int i = 0; i < dataIndex; i++) {
    jsonData += "    {\n";
    jsonData += "      \"timestamp\": " + String(dataLog[i].timestamp) + ",\n";
    jsonData += "      \"temperature\": " + String(dataLog[i].temperature, 1) + ",\n";
    jsonData += "      \"humidity\": " + String(dataLog[i].humidity, 1) + ",\n";
    jsonData += "      \"soil_moisture\": " + String(dataLog[i].soilMoisture) + ",\n";
    jsonData += "      \"stress_level\": " + String(dataLog[i].stressLevel) + ",\n";
    jsonData += "      \"pump_state\": \"";
    if (dataLog[i].pumpState) {
      jsonData += "ON";
    } else {
      jsonData += "OFF";
    }
    jsonData += "\",\n";
    jsonData += "      \"operation_mode\": \"" + dataLog[i].operationMode + "\"\n";
    jsonData += "    }";
    if (i < dataIndex - 1) jsonData += ",";
    jsonData += "\n";
  }
  
  jsonData += "  ]\n";
  jsonData += "}";
  
  server.sendHeader("Content-Type", "application/json");
  server.sendHeader("Content-Disposition", "attachment; filename=plant_data_export.json");
  server.send(200, "application/json", jsonData);
  Serial.println("📥 JSON data exported");
}

// ---------- Clear Data Handler ----------
void handleClearData() {
  dataIndex = 0;
  initializeHistory();
  server.send(200, "text/plain", "Data cleared successfully");
  Serial.println("🗑️ All data cleared");
}

// ---------- Control Endpoint ----------
void handleControl() {
  String command = server.arg("command");
  String response = "";
  
  if (command == "on") {
    relayState = true;
    digitalWrite(RELAY_PIN, LOW);
    manualControl = true;
    response = "Pump turned ON manually";
    Serial.println("🎛️ MANUAL CONTROL: Pump ON");
  } 
  else if (command == "off") {
    relayState = false;
    digitalWrite(RELAY_PIN, HIGH);
    manualControl = true;
    response = "Pump turned OFF manually";
    Serial.println("🎛️ MANUAL CONTROL: Pump OFF");
  }
  else if (command == "auto") {
    manualControl = false;
    response = "Switched to AUTO mode";
    Serial.println("🤖 MODE: Auto control activated");
  }
  else if (command == "manual") {
    manualControl = true;
    response = "Switched to MANUAL mode";
    Serial.println("👤 MODE: Manual control activated");
  }
  else if (command == "cloud_on") {
    thingSpeakEnabled = true;
    response = "ThingSpeak cloud enabled";
    Serial.println("☁️ ThingSpeak enabled");
  }
  else if (command == "cloud_off") {
    thingSpeakEnabled = false;
    response = "ThingSpeak cloud disabled";
    Serial.println("❌ ThingSpeak disabled");
  }
  else {
    response = "Unknown command";
  }
  
  server.send(200, "text/plain", response);
}

// ---------- Professional OLED Display ----------
void displayOLED(float temp, float hum, int soilPercent, bool relay) {
  display.clearDisplay();
  
  display.fillRect(0, 0, 128, 16, SSD1306_WHITE);
  display.setTextColor(SSD1306_BLACK);
  display.setCursor(2, 4);
  display.setTextSize(1);
  display.print("STRESS:");
  display.print(currentStress.level.substring(0, 4));
  
  // Add cloud status
  display.setCursor(100, 4);
  display.print(thingSpeakEnabled ? "☁" : "❌");
  
  display.drawFastHLine(0, 17, 128, SSD1306_WHITE);
  display.setTextColor(SSD1306_WHITE);
  
  display.setCursor(2, 20);
  display.print("T:");
  display.print(temp, 1);
  display.print("C");
  
  display.setCursor(68, 20);
  display.print("H:");
  display.print(hum, 1);
  display.print("%");
  
  display.setCursor(2, 32);
  display.print("S:");
  display.print(soilPercent);
  display.print("%");
  
  display.drawRect(2, 42, 124, 8, SSD1306_WHITE);
  int stressWidth = map(currentStress.severity, 0, 10, 0, 122);
  display.fillRect(3, 43, stressWidth, 6, SSD1306_WHITE);
  
  display.setCursor(2, 52);
  if(relay) {
    display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
    display.print(" WATERING ");
    display.setTextColor(SSD1306_WHITE);
  } else {
    display.print(" MONITORING");
  }
  
  display.setCursor(70, 52);
  display.print("S:");
  display.print(currentStress.severity);
  display.print("/10");
  
  display.display();
}

// ---------- Data Logging with Stress ----------
void updateHistory(float temp, float hum, int soil) {
  dataLog[dataIndex].timestamp = millis();
  dataLog[dataIndex].temperature = temp;
  dataLog[dataIndex].humidity = hum;
  dataLog[dataIndex].soilMoisture = soil;
  dataLog[dataIndex].stressLevel = currentStress.severity;
  dataLog[dataIndex].pumpState = relayState;
  dataLog[dataIndex].operationMode = manualControl ? "MANUAL" : "AUTO";
  
  dataIndex = (dataIndex + 1) % MAX_DATA_POINTS;
}

// ---------- Soil Stress Detection Dashboard ----------
void handleRoot() {
  float temp = dht.readTemperature();
  float hum  = dht.readHumidity();
  int soilRaw = analogRead(SOIL_PIN);
  int soilPercent = map(soilRaw, 4095, 0, 0, 100);
  soilPercent = constrain(soilPercent, 0, 100);

  currentStress = analyzeSoilStress(temp, hum, soilPercent);
  String recommendations = getPlantRecommendations(currentStress, temp, hum, soilPercent);
  checkStressAlerts(currentStress);

  String statusColor = relayState ? "#e74c3c" : "#2ecc71";
  String statusText = relayState ? "🔄 PUMPING ACTIVE" : "✅ SYSTEM IDLE";
  String manualStatus = manualControl ? "MANUAL" : "AUTO";
  String manualColor = manualControl ? "#f39c12" : "#3498db";

  String html = "<!DOCTYPE html><html lang='en'><head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<meta http-equiv='refresh' content='10'>";
  html += "<title>Smart Plant Stress Detection | Data Export</title>";
  html += "<script src='https://cdn.jsdelivr.net/npm/chart.js'></script>";
  
  html += "<style>";
  html += "* { margin: 0; padding: 0; box-sizing: border-box; }";
  html += "body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; ";
  html += "background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); ";
  html += "min-height: 100vh; padding: 20px; color: #333; }";
  html += ".container { max-width: 1200px; margin: 0 auto; }";
  html += ".header { text-align: center; margin-bottom: 30px; color: white; }";
  html += ".header h1 { font-size: 2.2rem; margin-bottom: 10px; text-shadow: 2px 2px 4px rgba(0,0,0,0.3); }";
  html += ".dashboard { display: grid; grid-template-columns: repeat(auto-fit, minmax(280px, 1fr)); ";
  html += "gap: 15px; margin-bottom: 20px; }";
  html += ".card { background: rgba(255, 255, 255, 0.95); border-radius: 12px; ";
  html += "padding: 20px; box-shadow: 0 8px 25px rgba(0,0,0,0.15); ";
  html += "backdrop-filter: blur(10px); border: 1px solid rgba(255,255,255,0.2); ";
  html += "transition: transform 0.3s ease; }";
  html += ".card:hover { transform: translateY(-3px); }";
  html += ".card h3 { color: #2c3e50; margin-bottom: 12px; font-size: 1.1rem; ";
  html += "border-bottom: 2px solid #3498db; padding-bottom: 6px; }";
  html += ".value { font-size: 1.8rem; font-weight: bold; margin: 8px 0; }";
  html += ".status { display: inline-block; padding: 6px 12px; border-radius: 15px; ";
  html += "color: white; font-weight: bold; margin-top: 8px; font-size: 0.85rem; }";
  html += ".stress-indicator { width: 100%; height: 20px; background: #ecf0f1; ";
  html += "border-radius: 10px; margin: 10px 0; overflow: hidden; }";
  html += ".stress-level { height: 100%; border-radius: 10px; transition: width 0.5s ease; }";
  html += ".alert-critical { animation: pulse 2s infinite; border: 2px solid #e74c3c; }";
  html += "@keyframes pulse { 0% { opacity: 1; } 50% { opacity: 0.7; } 100% { opacity: 1; } }";
  html += ".control-panel { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; margin: 15px 0; }";
  html += ".control-btn { padding: 10px 15px; border: none; border-radius: 8px; ";
  html += "color: white; font-size: 0.9rem; font-weight: bold; cursor: pointer; ";
  html += "transition: all 0.3s ease; }";
  html += ".control-btn:hover { transform: scale(1.05); opacity: 0.9; }";
  html += ".btn-on { background: linear-gradient(135deg, #e74c3c, #c0392b); }";
  html += ".btn-off { background: linear-gradient(135deg, #2ecc71, #27ae60); }";
  html += ".btn-auto { background: linear-gradient(135deg, #3498db, #2980b9); }";
  html += ".btn-manual { background: linear-gradient(135deg, #f39c12, #e67e22); }";
  html += ".btn-download { background: linear-gradient(135deg, #9b59b6, #8e44ad); }";
  html += ".btn-clear { background: linear-gradient(135deg, #95a5a6, #7f8c8d); }";
  html += ".chart-container { background: white; border-radius: 8px; padding: 12px; margin: 8px 0; height: 250px; }";
  html += ".sensor-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 12px; }";
  html += ".sensor-item { text-align: center; padding: 12px; background: rgba(52, 152, 219, 0.1); ";
  html += "border-radius: 8px; }";
  html += ".icon { font-size: 1.8rem; margin-bottom: 8px; }";
  html += ".footer { text-align: center; color: white; margin-top: 20px; ";
  html += "opacity: 0.8; font-size: 0.8rem; }";
  html += ".progress-bar { background: #ecf0f1; border-radius: 8px; height: 16px; ";
  html += "margin: 12px 0; overflow: hidden; }";
  html += ".progress-fill { height: 100%; background: linear-gradient(90deg, #e74c3c, #f39c12, #2ecc71); ";
  html += "border-radius: 8px; transition: width 0.5s ease; }";
  html += ".tab-container { margin: 15px 0; }";
  html += ".tabs { display: flex; background: rgba(255,255,255,0.1); border-radius: 8px; padding: 4px; }";
  html += ".tab { flex: 1; padding: 8px; text-align: center; color: white; cursor: pointer; ";
  html += "border-radius: 6px; transition: all 0.3s ease; font-size: 0.9rem; }";
  html += ".tab.active { background: white; color: #333; font-weight: bold; }";
  html += ".tab-content { display: none; }";
  html += ".tab-content.active { display: block; }";
  html += ".data-point { font-size: 0.8rem; color: #7f8c8d; margin-top: 5px; }";
  html += ".stress-history { max-height: 120px; overflow-y: auto; font-size: 0.8rem; }";
  html += ".data-actions { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; margin: 15px 0; }";
  html += "</style></head><body>";
  
  html += "<div class='container'>";
  html += "<div class='header'>";
  html += "<h1>🌱 Smart Plant Stress Detection</h1>";
  html += "<p>Advanced Monitoring with Cloud & Data Export</p>";
  html += "</div>";

  // Tab Navigation
  html += "<div class='tab-container'>";
  html += "<div class='tabs'>";
  html += "<div class='tab active' onclick='switchTab(\"dashboard\")'>📊 Stress Dashboard</div>";
  html += "<div class='tab' onclick='switchTab(\"analytics\")'>📈 Analytics</div>";
  html += "<div class='tab' onclick='switchTab(\"data\")'>💾 Data Export</div>";
  html += "<div class='tab' onclick='switchTab(\"control\")'>🎛️ Control</div>";
  html += "</div>";
  html += "</div>";

  // Dashboard Tab
  html += "<div id='dashboard-tab' class='tab-content active'>";
  html += "<div class='dashboard'>";
  
  // Stress Level Card
  html += "<div class='card" + String(currentStress.level == "CRITICAL" ? " alert-critical" : "") + "'>";
  html += "<h3>🧬 Plant Stress Level</h3>";
  html += "<div class='icon'>" + currentStress.icon + "</div>";
  html += "<div class='value' style='color:" + currentStress.color + ";'>" + currentStress.level + " STRESS</div>";
  html += "<div class='stress-indicator'>";
  html += "<div class='stress-level' style='width:" + String(currentStress.severity * 10) + "%; background:" + currentStress.color + ";'></div>";
  html += "</div>";
  html += "<div><strong>Severity:</strong> " + String(currentStress.severity) + "/10</div>";
  html += "<div style='font-size: 0.9rem; margin-top: 8px;'>" + currentStress.description + "</div>";
  html += "</div>";
  
  // Environmental Data Card
  html += "<div class='card'>";
  html += "<h3>📊 Environmental Data</h3>";
  html += "<div class='sensor-grid'>";
  html += "<div class='sensor-item'>";
  html += "<div class='icon'>🌡️</div>";
  html += "<div class='value'>" + String(temp, 1) + "°C</div>";
  html += "<div style='font-size: 0.8rem;'>" + String(temp >= TEMP_HEAT_STRESS ? "🔥 Heat Stress" : (temp <= TEMP_COLD_STRESS ? "❄️ Cold Stress" : "✅ Normal")) + "</div>";
  html += "</div>";
  html += "<div class='sensor-item'>";
  html += "<div class='icon'>💧</div>";
  html += "<div class='value'>" + String(hum, 1) + "%</div>";
  html += "<div style='font-size: 0.8rem;'>" + String(hum >= HUMIDITY_HIGH_STRESS ? "💦 High Humidity" : (hum <= HUMIDITY_LOW_STRESS ? "🏜️ Low Humidity" : "✅ Normal")) + "</div>";
  html += "</div>";
  html += "</div>";
  html += "</div>";
  
  // Soil Analysis Card
  html += "<div class='card'>";
  html += "<h3>🪴 Soil Analysis</h3>";
  html += "<div class='value'>" + String(soilPercent) + "%</div>";
  html += "<div class='progress-bar'>";
  html += "<div class='progress-fill' style='width:" + String(soilPercent) + "%;'></div>";
  html += "</div>";
  html += "<div style='font-size: 0.8rem; color: #7f8c8d; margin-top: 5px;'>";
  if (soilPercent <= SOIL_CRITICAL_STRESS) {
    html += "🚨 CRITICAL DROUGHT STRESS";
  } else if (soilPercent <= SOIL_HIGH_STRESS) {
    html += "⚠️ HIGH DROUGHT STRESS";
  } else if (soilPercent <= SOIL_MODERATE_STRESS) {
    html += "📋 MODERATE DROUGHT STRESS";
  } else if (soilPercent >= SOIL_WATERLOGGED) {
    html += "💦 WATERLOGGING STRESS";
  } else if (soilPercent >= SOIL_NO_STRESS) {
    html += "✅ OPTIMAL MOISTURE";
  } else {
    html += "🌱 ADEQUATE MOISTURE";
  }
  html += "</div>";
  html += "</div>";
  
  // ThingSpeak Cloud Card
  html += "<div class='card'>";
  html += "<h3>☁️ Cloud Sync</h3>";
  html += "<div style='text-align: center;'>";
  html += "<div class='icon'>" + String(thingSpeakEnabled ? "☁️" : "❌") + "</div>";
  html += "<div class='value'>" + String(thingSpeakEnabled ? "ACTIVE" : "INACTIVE") + "</div>";
  html += "<div style='font-size: 0.8rem; margin: 8px 0;'>ThingSpeak IoT</div>";
  html += "<div class='control-panel'>";
  if (thingSpeakEnabled) {
    html += "<button class='control-btn btn-off' onclick=\"sendCommand('cloud_off')\">DISABLE CLOUD</button>";
  } else {
    html += "<button class='control-btn btn-on' onclick=\"sendCommand('cloud_on')\">ENABLE CLOUD</button>";
  }
  html += "</div>";
  html += "</div>";
  html += "</div>";
  
  // Recommendations Card
  html += "<div class='card'>";
  html += "<h3>💡 Stress Recommendations</h3>";
  html += "<div style='font-size: 0.9rem; line-height: 1.4;'>" + recommendations + "</div>";
  html += "<div class='control-panel' style='margin-top: 15px;'>";
  html += "<button class='control-btn btn-on' onclick=\"sendCommand('on')\" style='grid-column: 1 / -1;'>💧 EMERGENCY WATERING</button>";
  html += "</div>";
  html += "</div>";
  
  // System Status Card
  html += "<div class='card'>";
  html += "<h3>⚙️ System Status</h3>";
  html += "<div class='status' style='background:" + statusColor + ";'>PUMP: " + String(relayState ? "ACTIVE" : "STANDBY") + "</div>";
  html += "<div class='status' style='background:" + manualColor + "; margin-top:5px;'>MODE: " + manualStatus + "</div>";
  html += "<div style='margin-top: 10px; font-size: 0.8rem;'>";
  html += "<div>WiFi: " + String(WiFi.RSSI()) + " dBm</div>";
  html += "<div>Uptime: " + String(millis() / 1000) + "s</div>";
  html += "<div>Data Points: " + String(dataIndex) + "/" + String(MAX_DATA_POINTS) + "</div>";
  html += "<div>Cloud: " + String(thingSpeakEnabled ? "Connected" : "Disabled") + "</div>";
  html += "</div>";
  html += "</div>";
  
  // Stress History Card
  html += "<div class='card'>";
  html += "<h3>📈 Stress History</h3>";
  html += "<div class='stress-history'>";
  for(int i = 0; i < 5; i++) {
    if(stressHistory[i] != "") {
      html += "<div style='padding: 2px 0; border-bottom: 1px solid #eee;'>" + stressHistory[i] + "</div>";
    }
  }
  if(stressHistory[0] == "") {
    html += "<div style='color: #95a5a6;'>No stress events recorded</div>";
  }
  html += "</div>";
  html += "</div>";
  
  html += "</div>";
  html += "</div>";

  // Analytics Tab
  html += "<div id='analytics-tab' class='tab-content'>";
  html += "<div class='dashboard'>";
  html += "<div class='card' style='grid-column: 1 / -1;'>";
  html += "<h3>📈 Stress & Environmental Trends</h3>";
  html += "<div class='chart-container'>";
  html += "<canvas id='analyticsChart'></canvas>";
  html += "</div>";
  html += "</div>";
  
  html += "<div class='card'>";
  html += "<h3>📊 Stress Statistics</h3>";
  html += "<div style='margin: 12px 0;'>";
  html += "<div><strong>Current Stress:</strong> " + String(currentStress.severity) + "/10</div>";
  html += "<div><strong>Current Temp:</strong> " + String(temp, 1) + "°C</div>";
  html += "<div><strong>Current Humidity:</strong> " + String(hum, 1) + "%</div>";
  html += "<div><strong>Current Soil:</strong> " + String(soilPercent) + "%</div>";
  html += "<div><strong>Data Points:</strong> " + String(dataIndex) + "/" + String(MAX_DATA_POINTS) + "</div>";
  html += "<div><strong>Cloud Status:</strong> " + String(thingSpeakEnabled ? "Active" : "Inactive") + "</div>";
  html += "</div>";
  html += "</div>";
  
  html += "<div class='card'>";
  html += "<h3>🔔 Stress Alerts</h3>";
  html += "<div style='margin: 12px 0;'>";
  if (currentStress.level == "CRITICAL") {
    html += "<div style='color: #e74c3c; font-weight: bold; font-size: 0.9rem;'>🚨 CRITICAL STRESS DETECTED</div>";
  }
  if (currentStress.level == "HIGH") {
    html += "<div style='color: #e67e22; font-weight: bold; font-size: 0.9rem;'>⚠️ HIGH STRESS DETECTED</div>";
  }
  if (soilPercent < SOIL_ON) {
    html += "<div style='color: #e74c3c; font-weight: bold; font-size: 0.9rem;'>💧 Soil moisture critical</div>";
  }
  if (temp > 35) {
    html += "<div style='color: #e74c3c; font-weight: bold; font-size: 0.9rem;'>🌡️ High temperature stress</div>";
  }
  if (hum > 80) {
    html += "<div style='color: #e74c3c; font-weight: bold; font-size: 0.9rem;'>💦 High humidity stress</div>";
  }
  html += "</div>";
  html += "</div>";
  html += "</div>";
  html += "</div>";

  // Data Export Tab
  html += "<div id='data-tab' class='tab-content'>";
  html += "<div class='dashboard'>";
  
  html += "<div class='card' style='grid-column: 1 / -1;'>";
  html += "<h3>💾 Data Export Center</h3>";
  html += "<div style='margin: 15px 0;'>";
  html += "<div><strong>Total Records:</strong> " + String(dataIndex) + " data points</div>";
  html += "<div><strong>Memory Usage:</strong> " + String(dataIndex * sizeof(SensorData)) + " bytes</div>";
  html += "<div><strong>Data Collection:</strong> " + String(millis() / 1000) + " seconds</div>";
  html += "<div><strong>Cloud Status:</strong> " + String(thingSpeakEnabled ? "Active (ThingSpeak)" : "Inactive") + "</div>";
  html += "</div>";
  html += "<div class='data-actions'>";
  html += "<a href='/download-csv' class='control-btn btn-download' style='text-decoration: none; text-align: center;'>📥 Download CSV</a>";
  html += "<a href='/export-json' class='control-btn btn-download' style='text-decoration: none; text-align: center;'>📊 Export JSON</a>";
  html += "<button class='control-btn btn-clear' onclick=\"clearData()\">🗑️ Clear Data</button>";
  html += "<button class='control-btn btn-download' onclick=\"location.reload()\">🔄 Refresh</button>";
  html += "</div>";
  html += "</div>";
  
  html += "<div class='card'>";
  html += "<h3>📋 Data Preview</h3>";
  html += "<div style='max-height: 200px; overflow-y: auto; font-size: 0.8rem;'>";
  if (dataIndex > 0) {
    html += "<table style='width: 100%; border-collapse: collapse;'>";
    html += "<thead><tr style='background: #f8f9fa;'>";
    html += "<th style='padding: 4px; border: 1px solid #ddd;'>Time</th>";
    html += "<th style='padding: 4px; border: 1px solid #ddd;'>Temp</th>";
    html += "<th style='padding: 4px; border: 1px solid #ddd;'>Hum</th>";
    html += "<th style='padding: 4px; border: 1px solid #ddd;'>Soil</th>";
    html += "<th style='padding: 4px; border: 1px solid #ddd;'>Stress</th>";
    html += "<th style='padding: 4px; border: 1px solid #ddd;'>Pump</th>";
  html += "</tr></thead><tbody>";
    
    // Show last 5 records
    int startIndex = dataIndex - 5;
    if (startIndex < 0) startIndex = 0;
    
    for(int i = startIndex; i < dataIndex; i++) {
      html += "<tr>";
      html += "<td style='padding: 4px; border: 1px solid #ddd;'>" + String(dataLog[i].timestamp / 1000) + "s</td>";
      html += "<td style='padding: 4px; border: 1px solid #ddd;'>" + String(dataLog[i].temperature, 1) + "°C</td>";
      html += "<td style='padding: 4px; border: 1px solid #ddd;'>" + String(dataLog[i].humidity, 1) + "%</td>";
      html += "<td style='padding: 4px; border: 1px solid #ddd;'>" + String(dataLog[i].soilMoisture) + "%</td>";
      html += "<td style='padding: 4px; border: 1px solid #ddd;'>" + String(dataLog[i].stressLevel) + "/10</td>";
      if (dataLog[i].pumpState) {
        html += "<td style='padding: 4px; border: 1px solid #ddd;'>ON</td>";
      } else {
        html += "<td style='padding: 4px; border: 1px solid #ddd;'>OFF</td>";
      }
      html += "</tr>";
    }
    html += "</tbody></table>";
  } else {
    html += "<div style='color: #95a5a6; text-align: center;'>No data collected yet</div>";
  }
  html += "</div>";
  html += "</div>";
  
  html += "<div class='card'>";
  html += "<h3>📈 Data Statistics</h3>";
  html += "<div style='font-size: 0.9rem;'>";
  if (dataIndex > 0) {
    float avgTemp = 0, avgHum = 0, avgSoil = 0, avgStress = 0;
    for(int i = 0; i < dataIndex; i++) {
      avgTemp += dataLog[i].temperature;
      avgHum += dataLog[i].humidity;
      avgSoil += dataLog[i].soilMoisture;
      avgStress += dataLog[i].stressLevel;
    }
    avgTemp /= dataIndex; avgHum /= dataIndex; avgSoil /= dataIndex; avgStress /= dataIndex;
    
    html += "<div><strong>Avg Temperature:</strong> " + String(avgTemp, 1) + "°C</div>";
    html += "<div><strong>Avg Humidity:</strong> " + String(avgHum, 1) + "%</div>";
    html += "<div><strong>Avg Soil Moisture:</strong> " + String(avgSoil, 0) + "%</div>";
    html += "<div><strong>Avg Stress Level:</strong> " + String(avgStress, 1) + "/10</div>";
    html += "<div><strong>Data Collection:</strong> " + String(dataIndex * 30) + " seconds</div>";
  } else {
    html += "<div style='color: #95a5a6;'>Collecting data...</div>";
  }
  html += "</div>";
  html += "</div>";
  
  html += "</div>";
  html += "</div>";

  // Control Tab
  html += "<div id='control-tab' class='tab-content'>";
  html += "<div class='dashboard'>";
  html += "<div class='card'>";
  html += "<h3>🎛️ Pump Control</h3>";
  html += "<div class='control-panel'>";
  html += "<button class='control-btn btn-on' onclick=\"sendCommand('on')\">💧 START PUMP</button>";
  html += "<button class='control-btn btn-off' onclick=\"sendCommand('off')\">🛑 STOP PUMP</button>";
  html += "</div>";
  html += "</div>";
  
  html += "<div class='card'>";
  html += "<h3>⚙️ Operation Mode</h3>";
  html += "<div class='control-panel'>";
  html += "<button class='control-btn btn-auto' onclick=\"sendCommand('auto')\">🤖 AUTO MODE</button>";
  html += "<button class='control-btn btn-manual' onclick=\"sendCommand('manual')\">👤 MANUAL MODE</button>";
  html += "</div>";
  html += "</div>";
  
  html += "<div class='card'>";
  html += "<h3>☁️ Cloud Control</h3>";
  html += "<div class='control-panel'>";
  if (thingSpeakEnabled) {
    html += "<button class='control-btn btn-off' onclick=\"sendCommand('cloud_off')\" style='grid-column: 1 / -1;'>❌ DISABLE CLOUD</button>";
  } else {
    html += "<button class='control-btn btn-on' onclick=\"sendCommand('cloud_on')\" style='grid-column: 1 / -1;'>☁️ ENABLE CLOUD</button>";
  }
  html += "</div>";
  html += "</div>";
  
  html += "<div class='card' style='grid-column: 1 / -1;'>";
  html += "<h3>📋 System Log</h3>";
  html += "<div style='background: #f8f9fa; padding: 12px; border-radius: 8px; height: 120px; overflow-y: auto; font-size: 0.85rem;'>";
  html += "<div>🟢 System initialized</div>";
  html += "<div>📡 Connected to WiFi: " + String(WiFi.RSSI()) + " dBm</div>";
  html += "<div>🌡️ Sensors activated</div>";
  html += "<div>💾 Data export ready</div>";
  html += "<div>☁️ ThingSpeak: " + String(thingSpeakEnabled ? "Connected" : "Disabled") + "</div>";
  html += "<div>📊 Records: " + String(dataIndex) + " points</div>";
  html += "<div>🧬 Current stress: " + currentStress.level + "</div>";
  html += "</div>";
  html += "</div>";
  html += "</div>";
  html += "</div>";

  html += "<div class='footer'>";
  html += "<p>Smart Plant Stress Detection v6.0 | Cloud IoT | NANOCHIP MINIPROJECT | ECE E SECTION</p>";
  html += "</div>";
  
  html += "</div>";

  // JavaScript
  html += "<script>";
  html += "const chartData = " + generateChartData() + ";";
  
  html += "function switchTab(tabName) {";
  html += "document.querySelectorAll('.tab').forEach(tab => tab.classList.remove('active'));";
  html += "document.querySelectorAll('.tab-content').forEach(content => content.classList.remove('active'));";
  html += "event.target.classList.add('active');";
  html += "document.getElementById(tabName + '-tab').classList.add('active');";
  html += "if(tabName === 'analytics') { initializeChart(); }";
  html += "}";
  
  html += "function sendCommand(cmd) {";
  html += "fetch('/control?command=' + cmd)";
  html += ".then(response => response.text())";
  html += ".then(data => { console.log('Command:', cmd, 'Response:', data); setTimeout(() => location.reload(), 500); })";
  html += ".catch(error => console.error('Error:', error));";
  html += "}";
  
  html += "function clearData() {";
  html += "if(confirm('Are you sure you want to clear all data?')) {";
  html += "fetch('/clear-data')";
  html += ".then(() => location.reload())";
  html += ".catch(error => console.error('Error:', error));";
  html += "}";
  html += "}";
  
  html += "function initializeChart() {";
  html += "if(chartData.count === 0) return;";
  html += "const ctx = document.getElementById('analyticsChart').getContext('2d');";
  html += "new Chart(ctx, {";
  html += "type: 'line', data: { labels: chartData.labels, datasets: [";
  html += "{ label: 'Temperature', data: chartData.temperature, borderColor: '#e74c3c', tension: 0.4 },";
  html += "{ label: 'Humidity', data: chartData.humidity, borderColor: '#3498db', tension: 0.4 },";
  html += "{ label: 'Soil', data: chartData.soil, borderColor: '#2ecc71', tension: 0.4 },";
  html += "{ label: 'Stress', data: chartData.stress, borderColor: '#9b59b6', tension: 0.4 }";
  html += "]}, options: { responsive: true, maintainAspectRatio: false }";
  html += "});";
  html += "}";
  
  html += "if(document.getElementById('analytics-tab').classList.contains('active')) {";
  html += "setTimeout(initializeChart, 100);";
  html += "}";
  html += "</script>";
  
  html += "</body></html>";

  server.send(200, "text/html", html);
}

// ---------- Setup ----------
void setup() {
  Serial.begin(115200);
  dht.begin();
  initializeHistory();

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH);
  pinMode(BUZZER_PIN, OUTPUT);

  Wire.begin(21, 22);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("❌ OLED initialization failed!");
  } else {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println(" PLANT STRESS");
    display.println("  DETECTION v6.0");
    display.println("----------------");
    display.println("Cloud IoT Ready");
    display.println("ThingSpeak:ON");
    display.display();
    delay(2000);
  }

  // WiFi Connection
  Serial.printf("🔗 Connecting to %s", ssid);
  WiFi.begin(ssid, password);
  
  int dots = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    dots = (dots + 1) % 4;
  }
  
  Serial.println("\n✅ WiFi Connected!");
  Serial.print("📡 IP Address: ");
  Serial.println(WiFi.localIP());

  // Setup OTA
  ArduinoOTA.setHostname("plant-stress-detector");
  ArduinoOTA.begin();

  // Server Routes
  server.on("/", handleRoot);
  server.on("/control", handleControl);
  server.on("/download-csv", handleCSVDownload);
  server.on("/export-json", handleDataExport);
  server.on("/clear-data", handleClearData);

  // Add ThingSpeak control routes
  server.on("/thingspeak-enable", HTTP_GET, []() {
    thingSpeakEnabled = true;
    server.send(200, "text/plain", "ThingSpeak enabled");
    Serial.println("✅ ThingSpeak enabled");
  });

  server.on("/thingspeak-disable", HTTP_GET, []() {
    thingSpeakEnabled = false;
    server.send(200, "text/plain", "ThingSpeak disabled");
    Serial.println("❌ ThingSpeak disabled");
  });

  server.on("/thingspeak-status", HTTP_GET, []() {
    String status = thingSpeakEnabled ? "enabled" : "disabled";
    server.send(200, "application/json", "{\"status\":\"" + status + "\"}");
  });
  
  server.begin();
  Serial.println("🌐 HTTP server started with cloud features");
  Serial.println("☁️ ThingSpeak Cloud Integration Ready");
  Serial.println("📊 Available endpoints:");
  Serial.println("   /download-csv - Download CSV data");
  Serial.println("   /export-json  - Export JSON data");
  Serial.println("   /clear-data   - Clear all data");
  Serial.println("   /thingspeak-enable - Enable cloud sync");
  Serial.println("   /thingspeak-disable - Disable cloud sync");
  
  if (strlen(thingSpeakApiKey) == 0 || thingSpeakChannel == 1234567) {
    Serial.println("⚠️ WARNING: ThingSpeak API Key or Channel ID not configured!");
    thingSpeakEnabled = false;
  }
}

// ---------- Main Loop ----------
void loop() {
  server.handleClient();
  ArduinoOTA.handle();

  unsigned long currentMillis = millis();

  // Non-blocking sensor reads
  if (currentMillis - lastSensorRead >= SENSOR_INTERVAL) {
    lastSensorRead = currentMillis;
    
    float temp = dht.readTemperature();
    float hum = dht.readHumidity();
    int soilRaw = analogRead(SOIL_PIN);
    int soilPercent = map(soilRaw, 4095, 0, 0, 100);
    soilPercent = constrain(soilPercent, 0, 100);

    currentStress = analyzeSoilStress(temp, hum, soilPercent);
    checkStressAlerts(currentStress);

    // ThingSpeak Cloud Update
    if (currentMillis - lastThingSpeakUpdate >= THINGSPEAK_INTERVAL) {
      updateThingSpeak(temp, hum, soilPercent, currentStress.severity, relayState, currentStress.level);
      lastThingSpeakUpdate = currentMillis;
    }

    if (currentMillis - lastUpdate > 30000) {
      updateHistory(temp, hum, soilPercent);
      lastUpdate = currentMillis;
      Serial.println("📊 Data logged - Records: " + String(dataIndex));
    }

    if (!manualControl) {
      if (!relayState && soilPercent <= SOIL_ON) {
        relayState = true;
        digitalWrite(RELAY_PIN, LOW);
        Serial.println("💧 AUTO-WATERING: Soil moisture low");
      } else if (relayState && soilPercent >= SOIL_OFF) {
        relayState = false;
        digitalWrite(RELAY_PIN, HIGH);
        Serial.println("✅ AUTO-STOP: Conditions optimal");
      }
    }
  }

  // Non-blocking OLED updates
  if (currentMillis - lastOLEDUpdate >= OLED_INTERVAL) {
    lastOLEDUpdate = currentMillis;
    float temp = dht.readTemperature();
    float hum = dht.readHumidity();
    int soilRaw = analogRead(SOIL_PIN);
    int soilPercent = map(soilRaw, 4095, 0, 0, 100);
    displayOLED(temp, hum, soilPercent, relayState);
  }
}