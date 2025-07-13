#ifndef WATERING_ZONE_H
#define WATERING_ZONE_H

#include <Arduino.h>
#include <Preferences.h>

// Default configuration values
const int DEFAULT_WET_THRESHOLD = 80;
const int DEFAULT_DRY_THRESHOLD = 30;
const int DEFAULT_AIR_VALUE = 3700;
const int DEFAULT_DRY_VALUE = 3200;
const int DEFAULT_WATER_VALUE = 1500;

// Sensor reading constants
const int SENSOR_SAMPLES = 5;
const int SENSOR_DELAY_MS = 10;
const int MAX_PUMP_RUNTIME_SEC = 30; // 30 seconds max pump runtime
const int PUMP_COOLDOWN_SEC = 300;   // 5 minutes (300 seconds) cooldown

class WateringZone
{
public:
  // Configuration
  String name;
  int id;
  int moisturePin;
  int pumpPin;

  // Settings
  int moistureThresholdWet;
  int moistureThresholdDry;
  int airValue;
  int dryValue;
  int waterValue;
  unsigned long maxPumpRuntimeMs; // Runtime in milliseconds (for efficient timing checks)
  unsigned long pumpCooldownMs;   // Cooldown in milliseconds (for efficient timing checks)

  // Runtime state
  int soilMoistureRaw;
  int soilMoisturePercent;
  bool pumpState;
  unsigned long pumpStartTime;
  unsigned long pumpStopTime;
  bool pumpStoppedByTimeout; // Track if pump was stopped due to timeout

  // Constructor
  WateringZone(int zoneId, const String &zoneName, int sensorPin, int relayPin);

  // Methods
  void init();
  void loadSettings();
  void saveSetting(const String &key, int value);
  void updateSoilMoisture();
  bool isPumpInCooldown() const;
  unsigned long getRemainingCooldownSeconds() const;
  bool isSensorInAir() const;

private:
  static Preferences preferences; // Shared by all zones

  // Simple control methods
  void readSensor();
  void turnPumpOn();
  void turnPumpOff();
  bool isPumpTimedOut() const;
};

#endif // WATERING_ZONE_H
