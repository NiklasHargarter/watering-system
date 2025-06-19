// Automated watering system with web interface
#include <Arduino.h>
#include <DNSServer.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Preferences.h>
#include "html_content.h"

DNSServer dnsServer;
AsyncWebServer server(80);
Preferences preferences;

const char *WIFI_SSID = "ESP32-Portal2";
const char *WIFI_PASSWORD = "123456789";

const int SOIL_MOISTURE_PIN = 4;
const int PUMP_PIN = 5;

// Default configuration values
const int DEFAULT_WET_THRESHOLD = 80;
const int DEFAULT_DRY_THRESHOLD = 30;
const int DEFAULT_AIR_VALUE = 3700;
const int DEFAULT_DRY_VALUE = 3200;
const int DEFAULT_WATER_VALUE = 1500;

// Current settings (initialized by loadSettings())
int moistureThresholdWet;
int moistureThresholdDry;

// Runtime sensor readings (updated by updateSoilMoisture())
int soilMoistureRaw;
int soilMoisturePercent;

// Pump state (initialized to OFF to match hardware setup)
bool pumpState = false;

// Sensor calibration values (initialized by loadSettings())
int AIR_VALUE;
int DRY_VALUE;
int WATER_VALUE;

void saveSettings()
{
  preferences.begin("watering", false);
  preferences.putInt("wetThreshold", moistureThresholdWet);
  preferences.putInt("dryThreshold", moistureThresholdDry);
  preferences.putInt("airValue", AIR_VALUE);
  preferences.putInt("dryValue", DRY_VALUE);
  preferences.putInt("waterValue", WATER_VALUE);
  preferences.end();
  Serial.println("Settings saved to flash memory");
}

void saveSetting(const char *key, int value)
{
  preferences.begin("watering", false);
  int currentValue = preferences.getInt(key, -1);
  if (currentValue != value)
  {
    preferences.putInt(key, value);
    Serial.printf("Setting %s updated: %d -> %d\n", key, currentValue, value);
  }
  else
  {
    Serial.printf("Setting %s unchanged: %d\n", key, value);
  }
  preferences.end();
}

void loadSettings()
{
  preferences.begin("watering", true);
  moistureThresholdWet = preferences.getInt("wetThreshold", DEFAULT_WET_THRESHOLD);
  moistureThresholdDry = preferences.getInt("dryThreshold", DEFAULT_DRY_THRESHOLD);
  AIR_VALUE = preferences.getInt("airValue", DEFAULT_AIR_VALUE);
  DRY_VALUE = preferences.getInt("dryValue", DEFAULT_DRY_VALUE);
  WATER_VALUE = preferences.getInt("waterValue", DEFAULT_WATER_VALUE);
  preferences.end();

  Serial.println("Settings loaded from flash memory:");
  Serial.printf("  Wet Threshold: %d%%\n", moistureThresholdWet);
  Serial.printf("  Dry Threshold: %d%%\n", moistureThresholdDry);
  Serial.printf("  Air Value: %d\n", AIR_VALUE);
  Serial.printf("  Dry Value: %d\n", DRY_VALUE);
  Serial.printf("  Water Value: %d\n", WATER_VALUE);
}

void controlPump(bool state)
{
  pumpState = state;
  digitalWrite(PUMP_PIN, state ? HIGH : LOW);
  Serial.print("Pump turned ");
  Serial.println(state ? "ON" : "OFF");
}

void updateSoilMoisture()
{
  // Average multiple readings for stability
  int sum = 0;
  for (int i = 0; i < 5; i++)
  {
    sum += analogRead(SOIL_MOISTURE_PIN);
    delay(10);
  }
  soilMoistureRaw = sum / 5;

  // Safety check: sensor not in soil
  if (soilMoistureRaw > AIR_VALUE)
  {
    soilMoisturePercent = 0;
    Serial.println("Sensor not in soil - pump disabled for safety");

    if (pumpState)
    {
      controlPump(false);
    }
    return;
  }
  else
  {
    soilMoisturePercent = map(soilMoistureRaw, DRY_VALUE, WATER_VALUE, 0, 100);
    soilMoisturePercent = constrain(soilMoisturePercent, 0, 100);
  }

  // Pump control logic with hysteresis
  if (soilMoisturePercent <= moistureThresholdDry && !pumpState)
  {
    controlPump(true);
  }
  else if (soilMoisturePercent >= moistureThresholdWet && pumpState)
  {
    controlPump(false);
  }

  Serial.print("Soil Moisture Raw: ");
  Serial.print(soilMoistureRaw);
  Serial.print(", Percentage: ");
  Serial.print(soilMoisturePercent);
  Serial.print("%, Pump: ");
  Serial.println(pumpState ? "ON" : "OFF");
}

void setup()
{
  Serial.begin(115200);
  delay(2000);
  Serial.println("Setting up simplified web portal...");

  loadSettings();

  pinMode(SOIL_MOISTURE_PIN, INPUT);
  pinMode(PUMP_PIN, OUTPUT);
  digitalWrite(PUMP_PIN, LOW);
  analogReadResolution(12);

  WiFi.mode(WIFI_AP);
  WiFi.softAP(WIFI_SSID, WIFI_PASSWORD);

  IPAddress apIP(192, 168, 4, 1);
  IPAddress netMsk(255, 255, 255, 0);
  WiFi.softAPConfig(apIP, apIP, netMsk);

  Serial.print("AP IP address: ");
  Serial.println(WiFi.softAPIP());
  Serial.print("AP MAC address: ");
  Serial.println(WiFi.softAPmacAddress());
  delay(500);
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    updateSoilMoisture();
    
    String html = String(index_html);
    html.replace("%WET_THRESHOLD%", String(moistureThresholdWet));
    html.replace("%DRY_THRESHOLD%", String(moistureThresholdDry));
    html.replace("%MOISTURE_RAW%", String(soilMoistureRaw));
    html.replace("%MOISTURE_PERCENT%", String(soilMoisturePercent));
    html.replace("%PUMP_STATUS%", pumpState ? "ON" : "OFF");
    html.replace("%PUMP_CLASS%", pumpState ? "pump-on" : "pump-off");
    html.replace("%AIR_VALUE%", String(AIR_VALUE));
    html.replace("%DRY_VALUE%", String(DRY_VALUE));
    html.replace("%WATER_VALUE%", String(WATER_VALUE));
    
    AsyncWebServerResponse *response = request->beginResponse(200, "text/html", html);
    response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    request->send(response); });

  server.on("/sensor-data", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    updateSoilMoisture();
    
    String json = "{\"raw\":" + String(soilMoistureRaw) + 
                  ",\"percent\":" + String(soilMoisturePercent) + 
                  ",\"inAir\":" + (soilMoistureRaw > AIR_VALUE ? "true" : "false") +
                  ",\"pumpState\":" + (pumpState ? "true" : "false") + "}";
                  
    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", json);
    response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    request->send(response); });
  server.on("/get", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    if (request->hasParam("wetThreshold")) {
      int newValue = request->getParam("wetThreshold")->value().toInt();
      newValue = constrain(newValue, 0, 100);
      moistureThresholdWet = newValue;
      saveSetting("wetThreshold", moistureThresholdWet);
    }
    
    if (request->hasParam("dryThreshold")) {
      int newValue = request->getParam("dryThreshold")->value().toInt();
      newValue = constrain(newValue, 0, 100);
      moistureThresholdDry = newValue;
      saveSetting("dryThreshold", moistureThresholdDry);
    }
    
    if (request->hasParam("airValue")) {
      int newValue = request->getParam("airValue")->value().toInt();
      newValue = constrain(newValue, 0, 4095);
      AIR_VALUE = newValue;
      saveSetting("airValue", AIR_VALUE);
    }
    
    if (request->hasParam("dryValue")) {
      int newValue = request->getParam("dryValue")->value().toInt();
      newValue = constrain(newValue, 0, 4095);
      DRY_VALUE = newValue;
      saveSetting("dryValue", DRY_VALUE);
    }
    
    if (request->hasParam("waterValue")) {
      int newValue = request->getParam("waterValue")->value().toInt();
      newValue = constrain(newValue, 0, 4095);
      WATER_VALUE = newValue;
      saveSetting("waterValue", WATER_VALUE);
    }
    
    request->redirect("/"); });
  server.onNotFound([](AsyncWebServerRequest *request)
                    {
    updateSoilMoisture();
    
    String html = String(index_html);
    html.replace("%WET_THRESHOLD%", String(moistureThresholdWet));
    html.replace("%DRY_THRESHOLD%", String(moistureThresholdDry));
    html.replace("%MOISTURE_RAW%", String(soilMoistureRaw));
    html.replace("%MOISTURE_PERCENT%", String(soilMoisturePercent));
    html.replace("%PUMP_STATUS%", pumpState ? "ON" : "OFF");
    html.replace("%PUMP_CLASS%", pumpState ? "pump-on" : "pump-off");
    html.replace("%AIR_VALUE%", String(AIR_VALUE));
    html.replace("%DRY_VALUE%", String(DRY_VALUE));
    html.replace("%WATER_VALUE%", String(WATER_VALUE));
    
    request->send(200, "text/html", html); });

  if (dnsServer.start(53, "*", WiFi.softAPIP()))
  {
    Serial.println("DNS server started successfully");
  }
  else
  {
    Serial.println("Failed to start DNS server");
  }

  server.begin();
  Serial.println("HTTP server started");
  Serial.println("Web portal ready!");
  Serial.println("Connect to WiFi: " + String(WIFI_SSID));
  Serial.println("Then browse to: http://192.168.4.1");
}

void loop()
{
  dnsServer.processNextRequest();

  static unsigned long lastCheck = 0;
  if (millis() - lastCheck > 10000)
  {
    updateSoilMoisture();
    lastCheck = millis();
  }
}