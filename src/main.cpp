// Combine the best of both examples
#include <Arduino.h>
#include <DNSServer.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

DNSServer dnsServer;
AsyncWebServer server(80);

// Pin definitions
const int SOIL_MOISTURE_PIN = A0; // Connect soil moisture sensor to GPIO 34 (ADC pin)
const int PUMP_PIN = 8;           // MOSFET gate pin for pump control

// Values
int moistureThresholdWet = 80; // Lower threshold - soil is wet enough below this value
int moistureThresholdDry = 30; // Upper threshold - soil is too dry above this value, needs watering
int soilMoistureRaw = 0;       // Raw ADC value from sensor
int soilMoisturePercent = 0;   // Moisture in percentage
bool pumpState = false;        // Current pump state

// Calibration values for soil moisture sensor
const int AIR_VALUE = 3700;   // Value when sensor is in air (dry)
const int DRY_VALUE = 3200;   // Value when soil is dry
const int WATER_VALUE = 1500; // Value when sensor is in water (wet)

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html><head>
  <title>Watering System</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: Arial; margin: 20px; }
    .container { max-width: 400px; margin: 0 auto; }
    input[type=range] { width: 100%; }
    input[type=submit] { background-color: #4CAF50; color: white; border: none; padding: 10px; }
    .sensor { background-color: #f0f0f0; padding: 10px; margin: 10px 0; }
    .pump-on { background-color: #4CAF50; color: white; }
    .pump-off { background-color: #f44336; color: white; }
  </style>
  <script>
    function refreshSensorData() {
      fetch('/sensor-data')
        .then(response => response.json())
        .then(data => {
          document.getElementById('moisture-percent').innerHTML = data.percent + '%';
          document.getElementById('moisture-raw').innerHTML = data.raw;
          document.getElementById('pump-status').innerHTML = data.pumpState ? 'ON' : 'OFF';
          document.getElementById('pump-status').className = data.pumpState ? 'pump-on' : 'pump-off';
        });
    }
    setInterval(refreshSensorData, 5000);
  </script>
</head><body>
  <div class="container">
    <h2>Watering System</h2>
    
    <div class="sensor">
      <h3>Soil Moisture: <span id="moisture-percent">%MOISTURE_PERCENT%</span></h3>
      <p>Raw: <span id="moisture-raw">%MOISTURE_RAW%</span></p>
      <p>Wet: %WET_THRESHOLD% | Dry: %DRY_THRESHOLD%</p>
      <p>Pump: <span id="pump-status" class="%PUMP_CLASS%">%PUMP_STATUS%</span></p>
    </div>
    
    <form action="/get">
      <p>
        <label>Wet Threshold: %WET_THRESHOLD%%</label><br>
        <input type="range" name="wetThreshold" min="0" max="100" value="%WET_THRESHOLD%">
      </p>
      
      <p>
        <label>Dry Threshold: %DRY_THRESHOLD%%</label><br>
        <input type="range" name="dryThreshold" min="0" max="100" value="%DRY_THRESHOLD%">
      </p>
      
      <input type="submit" value="Update">
    </form>
  </div>
</body></html>)rawliteral";

// Function to control pump
void controlPump(bool state) {
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
    if (pumpState) {
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
  if (soilMoisturePercent <= moistureThresholdDry && !pumpState) {
    // Soil is too dry (low percentage), turn pump ON
    controlPump(true);
  } else if (soilMoisturePercent >= moistureThresholdWet && pumpState) {
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

  // Initialize pins
  pinMode(SOIL_MOISTURE_PIN, INPUT);
  pinMode(PUMP_PIN, OUTPUT);
  digitalWrite(PUMP_PIN, LOW); // Start with pump OFF
  analogReadResolution(12); // 12-bit resolution for ESP32

  // Configure access point
  WiFi.mode(WIFI_AP);
  WiFi.softAP("ESP32-Portal"); // Change to your preferred AP name
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
      moistureThresholdWet = request->getParam("wetThreshold")->value().toInt();
      // Ensure value is within range
      moistureThresholdWet = constrain(moistureThresholdWet, 0, 100);
      Serial.print("Wet threshold set to: ");
      Serial.println(moistureThresholdWet);
    }
    
    if (request->hasParam("dryThreshold")) {
      moistureThresholdDry = request->getParam("dryThreshold")->value().toInt();
      // Ensure value is within range
      moistureThresholdDry = constrain(moistureThresholdDry, 0, 100);
      Serial.print("Dry threshold set to: ");
      Serial.println(moistureThresholdDry);
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
  if (millis() - lastCheck > 1000) {
    updateSoilMoisture();
    lastCheck = millis();
  }
}