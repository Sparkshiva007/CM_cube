#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>

/* ---------------- NETWORK ---------------- */
const char* ssid = "*****";
const char* password = "******";
const char* broker = "broker.hivemq.com";

/* ---------------- MQTT ---------------- */
WiFiClient espClient;
PubSubClient mqtt(espClient);

/* ---------------- NODE CONFIG ---------------- */
#define NODE_ID "C6"
#define TELEMETRY_TOPIC "cm3/node/c6/telemetry"
#define STATUS_TOPIC    "cm3/node/c6/status"
#define MODE_TOPIC      "cm3/system/mode"
#define EMERGENCY_TOPIC "cm3/system/emergency"

/* ---------------- ENUMS ---------------- */
enum SafetyState { SAFE, WARNING, DANGER };
enum Mode { MODE_SLEEP, MODE_STUDY, MODE_GAME, MODE_EMERGENCY };

/* ---------------- GLOBAL STATE ---------------- */
SafetyState safetyState = SAFE;
Mode currentMode = MODE_STUDY;
unsigned long lastCoreSeen = 0;

/* ---------------- SENSOR STATE ---------------- */
int aqi = 0;
bool gasDetected = false;

/* ---------------- HELPERS ---------------- */
void readAirSensors() {
  aqi = 40 + random(0, 200);
  gasDetected = (random(0, 100) > 85);
}

/* ---------------- SAFETY LOGIC ---------------- */
void evaluateSafety() {
  if(gasDetected || aqi > 150) safetyState = DANGER;
  else if(aqi > 100) safetyState = WARNING;
  else safetyState = SAFE;
}

/* ---------------- MQTT CALLBACK ---------------- */
void mqttCallback(char* topic, byte* payload, unsigned int len) {
  payload[len] = '\0';
  String msg = String((char*)payload);

  if(String(topic) == MODE_TOPIC) {
    currentMode = (msg == "EMERGENCY") ? MODE_EMERGENCY : MODE_STUDY;
    lastCoreSeen = millis();
  }
}

/* ---------------- TELEMETRY ---------------- */
void publishTelemetry() {
  char buf[64];
  snprintf(buf, sizeof(buf),
    "AQI=%d;GAS=%d;STATE=%d",
    aqi, gasDetected, safetyState
  );
  mqtt.publish(TELEMETRY_TOPIC, buf);
}

/* ---------------- EMERGENCY ---------------- */
void publishEmergency() {
  if(safetyState == DANGER) {
    mqtt.publish(EMERGENCY_TOPIC, "DANGER");
  }
}

/* ---------------- FAILSAFE ---------------- */
void checkFailsafe() {
  if(millis() - lastCoreSeen > 10000) {
    mqtt.publish(EMERGENCY_TOPIC, "CORE_LOST");
  }
}

/* ---------------- SETUP ---------------- */
void setup() {
  Serial.begin(115200);

  WiFi.begin(ssid, password);
  while(WiFi.status() != WL_CONNECTED) delay(500);

  mqtt.setServer(broker, 1883);
  mqtt.setCallback(mqttCallback);
  while(!mqtt.connected()) mqtt.connect("CM3_C6");

  mqtt.subscribe(MODE_TOPIC);
  lastCoreSeen = millis();
}

/* ---------------- LOOP ---------------- */
void loop() {
  mqtt.loop();

  static unsigned long lastSense = 0;
  static unsigned long lastPub = 0;

  if(millis() - lastSense > 2500) {
    readAirSensors();
    evaluateSafety();
    lastSense = millis();
  }

  if(millis() - lastPub > 3000) {
    publishTelemetry();
    publishEmergency();
    lastPub = millis();
  }

  checkFailsafe();
  delay(10);
}
