// Automated watering system with web interface - Multi-zone support
#include <Arduino.h>
#include <DNSServer.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <vector>
#include "html_content.h"
#include "WateringZone.h"

DNSServer dnsServer;
AsyncWebServer server(80);

const char *WIFI_SSID = "ESP32-Portal2";
const char *WIFI_PASSWORD = "123456789";

// Zone definitions - easily configurable
std::vector<WateringZone> zones;

void initializeZones()
{
  // Add your zones here - easily configurable
  zones.push_back(WateringZone(1, "Garden Bed 1", 1, 5));
  zones.push_back(WateringZone(2, "Garden Bed 2", 0, 7)); // Example second zone

  // Initialize hardware and load settings for each zone
  for (auto &zone : zones)
  {
    zone.init(); // This now handles both hardware setup and settings loading
  }
}

void setup()
{
  Serial.begin(115200);
  delay(2000);
  Serial.println("Setting up multi-zone watering system...");

  // Initialize zones
  initializeZones();

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

  // Main overview page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    // Update all zones
    for (auto& zone : zones) {
      zone.updateSoilMoisture();
    }
    
    String html = String(overview_html);
    String zoneList = "";
      for (auto& zone : zones) {
      zoneList += "<div>";
      zoneList += "<h4>" + zone.name + "</h4>";
      zoneList += "<p>Moisture: " + String(zone.soilMoisturePercent) + "%</p>";
      
      // Sensor status
      if (zone.isSensorInAir()) {
        zoneList += "<p class='error'>WARNING: SENSOR IN AIR</p>";
      }
      
      // Pump status with cooldown info
      String pumpClass = "off";
      String pumpStatus = "OFF";
      if (zone.pumpState) {
        pumpClass = "on";
        pumpStatus = "ON";
      } else if (zone.isPumpInCooldown()) {
        pumpClass = "cooldown";
        pumpStatus = "COOLDOWN (" + String(zone.getRemainingCooldownSeconds()) + "s)";
      }
      
      zoneList += "<p class='" + pumpClass + "'>Pump: " + pumpStatus + "</p>";
      zoneList += "<a href='/zone/" + String(zone.id) + "'>Configure</a>";
      zoneList += "</div>";
    }
    
    html.replace("%ZONE_LIST%", zoneList);
    request->send(200, "text/html", html); });

  // Individual zone configuration pages
  server.on("^/zone/([0-9]+)$", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    String zoneIdStr = request->pathArg(0);
    int zoneId = zoneIdStr.toInt();
    
    WateringZone* zone = nullptr;
    for (auto& z : zones) {
      if (z.id == zoneId) {
        zone = &z;
        break;
      }
    }
    
    if (!zone) {
      request->send(404, "text/plain", "Zone not found");
      return;
    }
    
    zone->updateSoilMoisture();
    
    String html = String(zone_config_html);
    html.replace("%ZONE_ID%", String(zone->id));
    html.replace("%ZONE_NAME%", zone->name);
    html.replace("%WET_THRESHOLD%", String(zone->moistureThresholdWet));
    html.replace("%DRY_THRESHOLD%", String(zone->moistureThresholdDry));
    html.replace("%MOISTURE_RAW%", String(zone->soilMoistureRaw));
    html.replace("%MOISTURE_PERCENT%", String(zone->soilMoisturePercent));
    html.replace("%PUMP_STATUS%", zone->pumpState ? "ON" : "OFF");
    html.replace("%PUMP_CLASS%", zone->pumpState ? "on" : "off");
    html.replace("%AIR_VALUE%", String(zone->airValue));
    html.replace("%DRY_VALUE%", String(zone->dryValue));
    html.replace("%WATER_VALUE%", String(zone->waterValue));
    html.replace("%MAX_RUNTIME_SEC%", String(zone->maxPumpRuntimeMs / 1000));
    html.replace("%COOLDOWN_SEC%", String(zone->pumpCooldownMs / 1000));
    
    // Add sensor status information
    String sensorStatus = "";
    if (zone->isSensorInAir()) {
      sensorStatus = "<p class='error'>WARNING: SENSOR IN AIR - Check sensor placement!</p>";
    }
    html.replace("%SENSOR_STATUS%", sensorStatus);
    
    // Add cooldown information
    String cooldownInfo = "";
    if (zone->isPumpInCooldown()) {
      cooldownInfo = "<p class='cooldown'>Cooldown: " + String(zone->getRemainingCooldownSeconds()) + " seconds</p>";
    }
    html.replace("%COOLDOWN_INFO%", cooldownInfo);
    
    request->send(200, "text/html", html); });
  // Configuration updates for zones
  server.on("^/zone/([0-9]+)/config$", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    String zoneIdStr = request->pathArg(0);
    int zoneId = zoneIdStr.toInt();
    
    WateringZone* zone = nullptr;
    for (auto& z : zones) {
      if (z.id == zoneId) {
        zone = &z;
        break;
      }
    }
    
    if (!zone) {
      request->send(404, "text/plain", "Zone not found");
      return;
    }
    
    bool settingsChanged = false;
    
    if (request->hasParam("wetThreshold")) {
      int newValue = request->getParam("wetThreshold")->value().toInt();
      newValue = constrain(newValue, 0, 100);
      if (zone->moistureThresholdWet != newValue) {
        zone->moistureThresholdWet = newValue;
        zone->saveSetting("wet", newValue);
        settingsChanged = true;
      }
    }
    
    if (request->hasParam("dryThreshold")) {
      int newValue = request->getParam("dryThreshold")->value().toInt();
      newValue = constrain(newValue, 0, 100);
      if (zone->moistureThresholdDry != newValue) {
        zone->moistureThresholdDry = newValue;
        zone->saveSetting("dry", newValue);
        settingsChanged = true;
      }
    }
    
    // Validate threshold relationship
    if (zone->moistureThresholdWet <= zone->moistureThresholdDry) {
      Serial.printf("Zone %d: Invalid thresholds (wet=%d, dry=%d), fixing...\n", 
                    zoneId, zone->moistureThresholdWet, zone->moistureThresholdDry);
      zone->moistureThresholdWet = zone->moistureThresholdDry + 10;
      zone->saveSetting("wet", zone->moistureThresholdWet);
      settingsChanged = true;
    }
    
    if (request->hasParam("airValue")) {
      int newValue = request->getParam("airValue")->value().toInt();
      newValue = constrain(newValue, 0, 4095);
      if (zone->airValue != newValue) {
        zone->airValue = newValue;
        zone->saveSetting("air", newValue);
        settingsChanged = true;
      }
    }
    
    if (request->hasParam("dryValue")) {
      int newValue = request->getParam("dryValue")->value().toInt();
      newValue = constrain(newValue, 0, 4095);
      if (zone->dryValue != newValue) {
        zone->dryValue = newValue;
        zone->saveSetting("dryVal", newValue);
        settingsChanged = true;
      }
    }
    
    if (request->hasParam("waterValue")) {
      int newValue = request->getParam("waterValue")->value().toInt();
      newValue = constrain(newValue, 0, 4095);
      if (zone->waterValue != newValue) {
        zone->waterValue = newValue;
        zone->saveSetting("water", newValue);
        settingsChanged = true;
      }
    }
    
    if (request->hasParam("maxRuntime")) {
      int newValue = request->getParam("maxRuntime")->value().toInt();
      newValue = constrain(newValue, 1, 300); // 1 to 300 seconds
      unsigned long newValueMs = newValue * 1000UL;
      if (zone->maxPumpRuntimeMs != newValueMs) {
        zone->maxPumpRuntimeMs = newValueMs;
        zone->saveSetting("maxRun", newValue);  // Shorter key name
        settingsChanged = true;
      }
    }
    
    if (request->hasParam("cooldown")) {
      int newValue = request->getParam("cooldown")->value().toInt();
      newValue = constrain(newValue, 1, 3600); // 1 to 3600 seconds (1 hour max)
      unsigned long newValueMs = newValue * 1000UL;
      if (zone->pumpCooldownMs != newValueMs) {
        zone->pumpCooldownMs = newValueMs;
        zone->saveSetting("cooldown", newValue);  // This key is already short enough
        settingsChanged = true;
      }
    }
    
    if (settingsChanged) {
      Serial.printf("Zone %d settings updated via web interface\n", zoneId);
    }
    
    request->redirect("/zone/" + String(zoneId)); });

  // Fallback handler
  server.onNotFound([](AsyncWebServerRequest *request)
                    { request->redirect("/"); });

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
  Serial.println("Multi-zone watering system ready!");
  Serial.println("Connect to WiFi: " + String(WIFI_SSID));
  Serial.println("Then browse to: http://192.168.4.1");
}

void loop()
{
  dnsServer.processNextRequest();

  static unsigned long lastCheck = 0;
  if (millis() - lastCheck > 10000)
  {
    // Update all zones
    for (auto &zone : zones)
    {
      zone.updateSoilMoisture();
    }
    lastCheck = millis();
  }
}