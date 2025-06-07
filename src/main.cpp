// Combine the best of both examples
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

// WiFi configuration
const char *WIFI_SSID = "ESP32-Portal2";
const char *WIFI_PASSWORD = "123456789"; // Empty for open network, or set a password

// Pin definitions
const int SOIL_MOISTURE_PIN = A0; // Connect soil moisture sensor to GPIO 34 (ADC pin)
const int PUMP_PIN = 5;           // MOSFET gate pin for pump control

// Values
int moistureThresholdWet = 80; // Lower threshold - soil is wet enough below this value
int moistureThresholdDry = 30; // Upper threshold - soil is too dry above this value, needs watering
int soilMoistureRaw = 0;       // Raw ADC value from sensor
int soilMoisturePercent = 0;   // Moisture in percentage
bool pumpState = false;        // Current pump state

// Calibration values for soil moisture sensor
int AIR_VALUE = 3700;   // Value when sensor is in air (dry)
int DRY_VALUE = 3200;   // Value when soil is dry
int WATER_VALUE = 1500; // Value when sensor is in water (wet)

// Function to save settings to non-volatile storage
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

// Function to save individual setting to reduce flash wear
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

// Function to load settings from non-volatile storage
void loadSettings()
{
  preferences.begin("watering", true);                           // true = read-only
  moistureThresholdWet = preferences.getInt("wetThreshold", 80); // Default: 80
  moistureThresholdDry = preferences.getInt("dryThreshold", 30); // Default: 30
  AIR_VALUE = preferences.getInt("airValue", 3700);              // Default: 3700
  DRY_VALUE = preferences.getInt("dryValue", 3200);              // Default: 3200
  WATER_VALUE = preferences.getInt("waterValue", 1500);          // Default: 1500
  preferences.end();

  Serial.println("Settings loaded from flash memory:");
  Serial.printf("  Wet Threshold: %d%%\n", moistureThresholdWet);
  Serial.printf("  Dry Threshold: %d%%\n", moistureThresholdDry);
  Serial.printf("  Air Value: %d\n", AIR_VALUE);
  Serial.printf("  Dry Value: %d\n", DRY_VALUE);
  Serial.printf("  Water Value: %d\n", WATER_VALUE);
}

// Function to control pump
void controlPump(bool state)
{
  pumpState = state;
  digitalWrite(PUMP_PIN, state ? HIGH : LOW);
  Serial.print("Pump turned ");
  Serial.println(state ? "ON" : "OFF");
}

// Function to read soil moisture and calculate percentage
void updateSoilMoisture()
{
  // Read the sensor value (average of multiple readings for stability)
  int sum = 0;
  for (int i = 0; i < 5; i++)
  {
    sum += analogRead(SOIL_MOISTURE_PIN);
    delay(10);
  }
  soilMoistureRaw = sum / 5;

  // Check if sensor is in air (not in soil)
  if (soilMoistureRaw > AIR_VALUE)
  {
    soilMoisturePercent = 0; // If reading is higher than AIR_VALUE, sensor is not in soil
    Serial.println("Sensor not in soil - pump disabled for safety");

    // Turn off pump immediately for safety when sensor is not reading properly
    if (pumpState)
    {
      controlPump(false);
    }
    return; // Exit function early - no pump control when sensor is invalid
  }
  else
  {
    // Convert to percentage (0% = dry soil, 100% = wet)
    soilMoisturePercent = map(soilMoistureRaw, DRY_VALUE, WATER_VALUE, 0, 100);

    // Constrain to 0-100% range
    soilMoisturePercent = constrain(soilMoisturePercent, 0, 100);
  }

  // Automatic pump control logic - only when sensor is reading properly
  if (soilMoisturePercent <= moistureThresholdDry && !pumpState)
  {
    // Soil is too dry (low percentage), turn pump ON
    controlPump(true);
  }
  else if (soilMoisturePercent >= moistureThresholdWet && pumpState)
  {
    // Soil is wet enough (high percentage), turn pump OFF
    controlPump(false);
  }

  Serial.print("Soil Moisture Raw: ");
  Serial.print(soilMoistureRaw);
  Serial.print(", Percentage: ");
  Serial.print(soilMoisturePercent);
  Serial.print("%, Pump: ");
  Serial.println(pumpState ? "ON" : "OFF");
}

class CaptiveRequestHandler : public AsyncWebHandler
{
public:
  CaptiveRequestHandler() {}
  virtual ~CaptiveRequestHandler() {}

  bool canHandle(AsyncWebServerRequest *request)
  {
    return true;
  }
  void handleRequest(AsyncWebServerRequest *request)
  {
    updateSoilMoisture(); // Update moisture reading

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
    request->send(response);
    Serial.println("Captive portal request handled");
  }
};

void setup()
{
  Serial.begin(115200);
  delay(2000); // Allow time for serial monitor to connect
  Serial.println("Setting up captive portal...");

  // Load saved settings from flash memory
  loadSettings();

  // Initialize pins
  pinMode(SOIL_MOISTURE_PIN, INPUT);
  pinMode(PUMP_PIN, OUTPUT);
  digitalWrite(PUMP_PIN, LOW); // Start with pump OFF
  analogReadResolution(12);    // 12-bit resolution for ESP32
  // Configure access point
  WiFi.mode(WIFI_AP);
  WiFi.softAP(WIFI_SSID, WIFI_PASSWORD); // Use variables for AP name and password
  Serial.print("AP IP address: ");
  Serial.println(WiFi.softAPIP());

  // Set up the web server
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    updateSoilMoisture(); // Update moisture reading
    
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

  // Add endpoint for AJAX sensor data updates
  server.on("/sensor-data", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    updateSoilMoisture(); // Update moisture reading
    
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
    
    // Handle advanced calibration parameters
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
    
    // Redirect back to the main page to show updated values
    request->redirect("/"); });

  // Default handler for captive portal
  server.addHandler(new CaptiveRequestHandler()).setFilter(ON_AP_FILTER);

  // Start DNS server - crucial for captive portal detection
  dnsServer.start(53, "*", WiFi.softAPIP());

  // Start server
  server.begin();
  Serial.println("HTTP server started");
}

void loop()
{
  dnsServer.processNextRequest();

  // Check soil moisture and control pump every 10 seconds
  static unsigned long lastCheck = 0;
  if (millis() - lastCheck > 1000)
  {
    updateSoilMoisture();
    lastCheck = millis();
  }
}