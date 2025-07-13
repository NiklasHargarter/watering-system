#include "WateringZone.h"

// Define the static member
Preferences WateringZone::preferences;

// Constructor implementation
WateringZone::WateringZone(int zoneId, const String &zoneName, int sensorPin, int relayPin)
    : id(zoneId), name(zoneName), moisturePin(sensorPin), pumpPin(relayPin)
{
  // Runtime state only
  soilMoistureRaw = 0;
  soilMoisturePercent = 0;
  pumpState = false;
  pumpStartTime = 0;
  pumpStopTime = 0;
  pumpStoppedByTimeout = false;
}

void WateringZone::init()
{
  // Validate pin numbers
  if (moisturePin < 0 || pumpPin < 0)
  {
    Serial.printf("Zone %d: Invalid pin configuration - Sensor: GPIO%d, Pump: GPIO%d\n",
                  id, moisturePin, pumpPin);
    return;
  }

  // Load settings from NVS with defaults
  loadSettings();

  // Initialize hardware
  pinMode(moisturePin, INPUT);
  pinMode(pumpPin, OUTPUT);
  digitalWrite(pumpPin, LOW);
  Serial.printf("Zone %d (%s) initialized - Sensor: GPIO%d, Pump: GPIO%d\n",
                id, name.c_str(), moisturePin, pumpPin);
}

void WateringZone::loadSettings()
{
  String prefix = "zone" + String(id) + "_";
  preferences.begin("watering", true);

  // Load all settings with defaults
  moistureThresholdWet = preferences.getInt((prefix + "wet").c_str(), DEFAULT_WET_THRESHOLD);
  moistureThresholdDry = preferences.getInt((prefix + "dry").c_str(), DEFAULT_DRY_THRESHOLD);
  airValue = preferences.getInt((prefix + "air").c_str(), DEFAULT_AIR_VALUE);
  dryValue = preferences.getInt((prefix + "dryVal").c_str(), DEFAULT_DRY_VALUE);
  waterValue = preferences.getInt((prefix + "water").c_str(), DEFAULT_WATER_VALUE);

  // Load timing values in seconds and convert to milliseconds
  int runtimeSec = preferences.getInt((prefix + "maxRun").c_str(), MAX_PUMP_RUNTIME_SEC);
  int cooldownSec = preferences.getInt((prefix + "cooldown").c_str(), PUMP_COOLDOWN_SEC);
  maxPumpRuntimeMs = runtimeSec * 1000UL;
  pumpCooldownMs = cooldownSec * 1000UL;

  preferences.end();

  // Simple validation - fix invalid threshold settings
  if (moistureThresholdWet <= moistureThresholdDry)
  {
    moistureThresholdWet = DEFAULT_WET_THRESHOLD;
    moistureThresholdDry = DEFAULT_DRY_THRESHOLD;
    Serial.printf("Zone %d: Fixed invalid thresholds\n", id);
  }

  Serial.printf("Zone %d settings loaded: Wet=%d%%, Dry=%d%%, Runtime=%dms, Cooldown=%dms\n",
                id, moistureThresholdWet, moistureThresholdDry, (int)maxPumpRuntimeMs, (int)pumpCooldownMs);
}

void WateringZone::saveSetting(const String &key, int value)
{
  String fullKey = "zone" + String(id) + "_" + key;
  preferences.begin("watering", false);
  int currentValue = preferences.getInt(fullKey.c_str(), -1);
  if (currentValue != value)
  {
    preferences.putInt(fullKey.c_str(), value);
    Serial.printf("Zone %d: %s updated: %d -> %d\n", id, key.c_str(), currentValue, value);
  }
  preferences.end();
}

void WateringZone::updateSoilMoisture()
{
  // Read sensor
  readSensor();

  // Safety check: Don't run pump if sensor is reading air (not in soil)
  if (soilMoistureRaw >= airValue)
  {
    if (pumpState)
    {
      turnPumpOff();
      pumpStoppedByTimeout = false; // Reset timeout flag (safety stop)
      Serial.printf("Zone %d: Pump stopped - sensor in air (raw: %d >= air: %d)\n",
                    id, soilMoistureRaw, airValue);
    }
    return; // Skip pump control logic
  }

  // Simple state machine for pump control
  if (pumpState)
  {
    // Pump is ON - check if we should turn it OFF
    if (soilMoisturePercent >= moistureThresholdWet)
    {
      // Reached wet threshold - normal completion
      turnPumpOff();
      pumpStoppedByTimeout = false; // Reset timeout flag (successful completion)
    }
    else if (isPumpTimedOut())
    {
      // Timed out before reaching wet threshold
      turnPumpOff();
      pumpStoppedByTimeout = true; // Set timeout flag
    }
  }
  else
  {
    // Pump is OFF - check if we should turn it ON
    bool shouldStart = false;

    if (pumpStoppedByTimeout)
    {
      // Continue watering if still below wet threshold after timeout
      shouldStart = (soilMoisturePercent < moistureThresholdWet) && !isPumpInCooldown();
    }
    else
    {
      // Normal start condition: reached dry threshold
      shouldStart = (soilMoisturePercent <= moistureThresholdDry) && !isPumpInCooldown();
    }

    if (shouldStart)
    {
      turnPumpOn();
    }
  }
}

void WateringZone::readSensor()
{
  // Average multiple readings
  int sum = 0;
  for (int i = 0; i < SENSOR_SAMPLES; i++)
  {
    sum += analogRead(moisturePin);
    delay(SENSOR_DELAY_MS);
  }
  soilMoistureRaw = sum / SENSOR_SAMPLES;

  // Convert to percentage
  soilMoisturePercent = map(soilMoistureRaw, dryValue, waterValue, 0, 100);
  soilMoisturePercent = constrain(soilMoisturePercent, 0, 100);
}

void WateringZone::turnPumpOn()
{
  pumpState = true;
  pumpStartTime = millis();
  digitalWrite(pumpPin, HIGH);
  Serial.printf("Zone %d pump ON - moisture: %d%%\n", id, soilMoisturePercent);
}

void WateringZone::turnPumpOff()
{
  pumpState = false;
  pumpStopTime = millis();
  pumpStartTime = 0;
  digitalWrite(pumpPin, LOW);
  Serial.printf("Zone %d pump OFF - moisture: %d%%\n", id, soilMoisturePercent);
}

bool WateringZone::isPumpTimedOut() const
{
  return (millis() - pumpStartTime) > maxPumpRuntimeMs;
}

bool WateringZone::isPumpInCooldown() const
{
  if (pumpStopTime == 0)
  {
    return false; // Never ran
  }
  return (millis() - pumpStopTime) < pumpCooldownMs;
}

unsigned long WateringZone::getRemainingCooldownSeconds() const
{
  if (!isPumpInCooldown())
  {
    return 0;
  }
  unsigned long remainingMs = pumpCooldownMs - (millis() - pumpStopTime);
  return remainingMs / 1000;
}

bool WateringZone::isSensorInAir() const
{
  return soilMoistureRaw >= airValue;
}
