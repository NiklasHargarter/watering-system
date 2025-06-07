#ifndef HTML_CONTENT_H
#define HTML_CONTENT_H
#include <Arduino.h>
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
    .pump-off { background-color: #f44336; color: white; }    .slider-container { margin: 10px 0; }
    .slider-label { font-weight: bold; }
    .advanced-config { 
      background-color: #fff3cd; 
      border: 1px solid #ffeaa7; 
      padding: 15px; 
      margin: 10px 0; 
      border-radius: 5px;
      display: none;
    }
    .config-toggle { 
      background-color: #6c757d; 
      color: white; 
      border: none; 
      padding: 8px 16px; 
      margin: 10px 0;
      border-radius: 3px;
      cursor: pointer;
    }
    .config-toggle:hover { background-color: #5a6268; }
    .config-input { 
      width: 100%; 
      padding: 5px; 
      margin: 5px 0; 
      border: 1px solid #ddd; 
      border-radius: 3px;
    }
    .warning { color: #856404; font-weight: bold; }
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
      function updateSliderValue(sliderId, valueId) {
      const slider = document.getElementById(sliderId);
      const valueDisplay = document.getElementById(valueId);
      valueDisplay.innerHTML = slider.value;
    }
    
    function toggleAdvancedConfig() {
      const configDiv = document.getElementById('advanced-config');
      const toggleBtn = document.getElementById('config-toggle-btn');
      if (configDiv.style.display === 'none' || configDiv.style.display === '') {
        configDiv.style.display = 'block';
        toggleBtn.innerHTML = 'Hide Advanced Config';
      } else {
        configDiv.style.display = 'none';
        toggleBtn.innerHTML = 'Show Advanced Config';
      }
    }
    
    setInterval(refreshSensorData, 1000);
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
      <div class="slider-container">
        <label class="slider-label">Wet Threshold: <span id="wet-value">%WET_THRESHOLD%</span>%</label><br>
        <input type="range" id="wet-slider" name="wetThreshold" min="0" max="100" value="%WET_THRESHOLD%" 
               oninput="updateSliderValue('wet-slider', 'wet-value')">
      </div>
      
      <div class="slider-container">
        <label class="slider-label">Dry Threshold: <span id="dry-value">%DRY_THRESHOLD%</span>%</label><br>
        <input type="range" id="dry-slider" name="dryThreshold" min="0" max="100" value="%DRY_THRESHOLD%" 
               oninput="updateSliderValue('dry-slider', 'dry-value')">
      </div>      
      <input type="submit" value="Update">
    </form>
    
    <button id="config-toggle-btn" class="config-toggle" onclick="toggleAdvancedConfig()">Show Advanced Config</button>
    
    <div id="advanced-config" class="advanced-config">
      <h3>Advanced Sensor Calibration</h3>
      
      <form action="/get">
        <label for="air-value">Air Value (sensor in air):</label>
        <input type="number" id="air-value" name="airValue" value="%AIR_VALUE%" class="config-input" min="0" max="4095">
        
        <label for="dry-value">Dry Soil Value:</label>
        <input type="number" id="dry-value" name="dryValue" value="%DRY_VALUE%" class="config-input" min="0" max="4095">
        
        <label for="water-value">Water Value (sensor in water):</label>
        <input type="number" id="water-value" name="waterValue" value="%WATER_VALUE%" class="config-input" min="0" max="4095">
        
        <p style="font-size: 12px; color: #666;">
          Current values: Air=%AIR_VALUE%, Dry=%DRY_VALUE%, Water=%WATER_VALUE%<br>
          Raw sensor reading: %MOISTURE_RAW%
        </p>
        
        <input type="submit" value="Update Calibration" style="background-color: #dc3545;">
      </form>
    </div>
  </div>
</body></html>)rawliteral";

#endif