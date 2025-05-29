// Combine the best of both examples
#include <Arduino.h>
#include <DNSServer.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

DNSServer dnsServer;
AsyncWebServer server(80);

// Pin definition
const int SOIL_MOISTURE_PIN = A0; // Connect soil moisture sensor to GPIO 34 (ADC pin)

// Values
int value1 = 50;             // Default value
int value2 = 75;             // Default value
int soilMoistureRaw = 0;     // Raw ADC value from sensor
int soilMoisturePercent = 0; // Moisture in percentage

// Calibration values for soil moisture sensor
const int AIR_VALUE = 3700;   // Value when sensor is in air (dry)
const int DRY_VALUE = 3200;   // Value when soil is dry
const int WATER_VALUE = 1500; // Value when sensor is in water (wet)

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html><head>
  <title>Welcome</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: Arial; text-align: center; margin: 0px auto; padding: 20px; }
    .container { width: 90%; max-width: 400px; margin: 0 auto; }
    input, select { width: 100%; padding: 12px; margin: 8px 0; box-sizing: border-box; }
    input[type=submit] { background-color: #4CAF50; color: white; border: none; border-radius: 4px; cursor: pointer; }
    .slider-container { display: flex; flex-direction: column; align-items: center; margin: 10px 0; }
    .slider-value { margin-top: 5px; font-weight: bold; }
    .sensor-container { background-color: #f5f5f5; border-radius: 10px; padding: 15px; margin: 20px 0; }
    .sensor-value { font-size: 24px; font-weight: bold; margin: 10px 0; }
    .sensor-raw { font-size: 14px; color: #666; }
    .moisture-indicator { width: 100%; height: 20px; background: linear-gradient(to right, #ff4c4c, #ffad4c, #4caf50); border-radius: 10px; position: relative; margin-top: 10px; }
    .moisture-level { height: 100%; width: 10px; background-color: #000; position: absolute; transform: translateX(-50%); }
    .refresh-btn { background-color: #2196F3; color: white; border: none; border-radius: 4px; padding: 10px 15px; cursor: pointer; margin-top: 10px; }
  </style>  <script>
    function refreshSensorData() {
      fetch('/sensor-data')
        .then(response => response.json())
        .then(data => {
          document.getElementById('moisture-percent').innerHTML = data.percent + '%';
          document.getElementById('moisture-raw').innerHTML = '(Raw: ' + data.raw + ')';
          document.getElementById('moisture-level').style.left = data.percent + '%';
          
          // Show error message when sensor is in air
          const errorMessage = document.getElementById('error-message');
          if (data.inAir) {
            errorMessage.style.display = 'block';
          } else {
            errorMessage.style.display = 'none';
          }
        });
    }
    
    // Refresh sensor data every 5 seconds
    setInterval(refreshSensorData, 5000);
  </script>
  </head><body>
  <div class="container">
    <h2>Control Panel</h2>
    
    <div class="sensor-container">
      <h3>Soil Moisture</h3>
      <div class="sensor-value">
        <span id="moisture-percent">%MOISTURE_PERCENT%</span>
        <span class="sensor-raw" id="moisture-raw">(Raw: %MOISTURE_RAW%)</span>
      </div>      <div class="moisture-indicator">
        <div class="moisture-level" id="moisture-level" style="left: %MOISTURE_PERCENT%%;"></div>
      </div>
      <div id="error-message" style="color: red; font-weight: bold; margin-top: 10px; display: none;">
        Warning: Sensor appears to be in air or not properly connected!
      </div>
      <button class="refresh-btn" onclick="refreshSensorData()">Refresh</button>
    </div>
    
    <form action="/get">
      <div class="slider-container">
        <label for="value1">Value 1 (Current: %VALUE1%)</label>
        <input type="range" name="value1" min="0" max="100" value="%VALUE1%" oninput="this.nextElementSibling.value = this.value">
        <output class="slider-value">%VALUE1%</output>
      </div>
      
      <div class="slider-container">
        <label for="value2">Value 2 (Current: %VALUE2%)</label>
        <input type="range" name="value2" min="0" max="100" value="%VALUE2%" oninput="this.nextElementSibling.value = this.value">
        <output class="slider-value">%VALUE2%</output>
      </div>
      
      <br>
      <input type="submit" value="Submit">
    </form>
  </div>
</body></html>)rawliteral";

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
    soilMoisturePercent = 0; // If reading is higher than AIR_VALUE, soil is completely dry or sensor not in soil
    Serial.println("Sensor might not be in soil (reading higher than AIR_VALUE)");
  }
  else
  {
    // Convert to percentage (0% = dry soil, 100% = wet)
    soilMoisturePercent = map(soilMoistureRaw, DRY_VALUE, WATER_VALUE, 0, 100);

    // Constrain to 0-100% range
    soilMoisturePercent = constrain(soilMoisturePercent, 0, 100);
  }

  Serial.print("Soil Moisture Raw: ");
  Serial.print(soilMoistureRaw);
  Serial.print(", Percentage: ");
  Serial.print(soilMoisturePercent);
  Serial.println("%");
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
    html.replace("%VALUE1%", String(value1));
    html.replace("%VALUE2%", String(value2));
    html.replace("%MOISTURE_RAW%", String(soilMoistureRaw));
    html.replace("%MOISTURE_PERCENT%", String(soilMoisturePercent));

    AsyncWebServerResponse *response = request->beginResponse(200, "text/html", html);
    response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    request->send(response);
    Serial.println("Captive portal request handled");
  }
};

void setup()
{
  Serial.begin(115200);
  Serial.println("Setting up captive portal...");

  // Initialize ADC for soil moisture sensor
  pinMode(SOIL_MOISTURE_PIN, INPUT);
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
    html.replace("%VALUE1%", String(value1));
    html.replace("%VALUE2%", String(value2));
    html.replace("%MOISTURE_RAW%", String(soilMoistureRaw));
    html.replace("%MOISTURE_PERCENT%", String(soilMoisturePercent));
    
    AsyncWebServerResponse *response = request->beginResponse(200, "text/html", html);
    response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    request->send(response); });
  // Add endpoint for AJAX sensor data updates
  server.on("/sensor-data", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    updateSoilMoisture(); // Update moisture reading
    
    String json = "{\"raw\":" + String(soilMoistureRaw) + 
                  ",\"percent\":" + String(soilMoisturePercent) + 
                  ",\"inAir\":" + (soilMoistureRaw > AIR_VALUE ? "true" : "false") + "}";
                  
    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", json);
    response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    request->send(response); });

  server.on("/get", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    if (request->hasParam("value1")) {
      value1 = request->getParam("value1")->value().toInt();
      // Ensure value is within range
      value1 = constrain(value1, 0, 100);
      Serial.print("Value 1 set to: ");
      Serial.println(value1);
    }
    
    if (request->hasParam("value2")) {
      value2 = request->getParam("value2")->value().toInt();
      // Ensure value is within range
      value2 = constrain(value2, 0, 100);
      Serial.print("Value 2 set to: ");
      Serial.println(value2);
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
}