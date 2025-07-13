#ifndef HTML_CONTENT_H
#define HTML_CONTENT_H
#include <Arduino.h>

// Simple mobile overview page
const char overview_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html><head>
  <title>Watering System</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { margin: 10px; font-size: 18px; }
    div { border: 1px solid #ccc; padding: 10px; margin: 5px 0; }
    .on { background: #90EE90; }
    .off { background: #FFB6C1; }
    .cooldown { background: #FFA500; }
    .error { background: #FF6B6B; color: white; font-weight: bold; }
    a { display: block; text-decoration: none; background: #87CEEB; padding: 8px; margin: 5px 0; text-align: center; }
  </style>
  <script>
    function refreshPage() {
      location.reload();
    }
    setInterval(refreshPage, 15000); // Auto-refresh every 15 seconds
  </script>
</head><body>
  <h2>Watering System</h2>
  <a href="/">Refresh</a>
  %ZONE_LIST%
</body></html>)rawliteral";

// Simple mobile zone config page
const char zone_config_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html><head>
  <title>%ZONE_NAME%</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { margin: 10px; font-size: 18px; }
    div { border: 1px solid #ccc; padding: 10px; margin: 5px 0; }
    .on { background: #90EE90; }
    .off { background: #FFB6C1; }
    .cooldown { background: #FFA500; }
    .error { background: #FF6B6B; color: white; font-weight: bold; }
    input { width: 100%; padding: 8px; margin: 5px 0; font-size: 16px; }
    input[type=submit], a { display: block; text-decoration: none; background: #87CEEB; padding: 8px; text-align: center; border: none; }
    .section { margin: 20px 0; }
    small { color: #666; font-size: 14px; display: block; margin: 5px 0; }
  </style>
  <script>
    function refreshPage() {
      location.reload();
    }
    setInterval(refreshPage, 10000); // Auto-refresh every 10 seconds
  </script>
</head><body>
  <a href="/">Back to Overview</a>
  
  <h3>%ZONE_NAME%</h3>
  
  <div>
    <p>Moisture: %MOISTURE_PERCENT%%</p>
    <p>Raw Reading: %MOISTURE_RAW%</p>
    <p class="%PUMP_CLASS%">Pump: %PUMP_STATUS%</p>
    %SENSOR_STATUS%
    %COOLDOWN_INFO%
  </div>
  
  <form action="/zone/%ZONE_ID%/config" method="GET">
    <div class="section">
      <h4>Moisture Thresholds</h4>
      <p>Wet Threshold (pump turns OFF): %WET_THRESHOLD%%</p>
      <input type="number" name="wetThreshold" value="%WET_THRESHOLD%" min="0" max="100" required>
      
      <p>Dry Threshold (pump turns ON): %DRY_THRESHOLD%%</p>
      <input type="number" name="dryThreshold" value="%DRY_THRESHOLD%" min="0" max="100" required>
      <small>Note: Wet threshold must be higher than dry threshold for proper hysteresis</small>
    </div>
    
    <div class="section">
      <h4>Sensor Calibration</h4>
      <small>Calibrate by placing sensor in different conditions and noting the raw values</small>
      
      <p>Air Value (sensor in air): %AIR_VALUE%</p>
      <input type="number" name="airValue" value="%AIR_VALUE%" min="0" max="4095" required>
      
      <p>Dry Soil Value: %DRY_VALUE%</p>
      <input type="number" name="dryValue" value="%DRY_VALUE%" min="0" max="4095" required>
      
      <p>Wet Soil Value: %WATER_VALUE%</p>
      <input type="number" name="waterValue" value="%WATER_VALUE%" min="0" max="4095" required>
    </div>
    
    <div class="section">
      <h4>Timing Settings</h4>
      <small>Configure pump operation timing</small>
      
      <p>Max Pump Runtime (seconds): %MAX_RUNTIME_SEC%</p>
      <input type="number" name="maxRuntime" value="%MAX_RUNTIME_SEC%" min="5" max="300" required>
      
      <p>Cooldown Period (seconds): %COOLDOWN_SEC%</p>
      <input type="number" name="cooldown" value="%COOLDOWN_SEC%" min="1" max="3600" required>
    </div>
    
    <input type="submit" value="Save All Settings">
  </form>
</body></html>)rawliteral";

#endif