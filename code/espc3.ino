#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>

/* ---------------- NETWORK ---------------- */
const char* ssid = "*****";
const char* password = "*****";
const char* broker = "broker.hivemq.com";

/* ---------------- MQTT ---------------- */
WiFiClient espClient;
PubSubClient mqtt(espClient);

/* ---------------- NODE CONFIG ---------------- */
#define NODE_ID "C3"
#define TELEMETRY_TOPIC "cm3/node/c3/telemetry"
#define STATUS_TOPIC    "cm3/node/c3/status"
#define MODE_TOPIC      "cm3/system/mode"
#define CMD_TOPIC       "cm3/device/c3/cmd"

/* ---------------- ENUMS ---------------- */
enum Mode { MODE_SLEEP, MODE_STUDY, MODE_GAME, MODE_EMERGENCY };
enum NodeState { NODE_INIT, NODE_ACTIVE, NODE_FAILSAFE };

/* ---------------- GLOBAL STATE ---------------- */
Mode currentMode = MODE_STUDY;
NodeState nodeState = NODE_INIT;
unsigned long lastCoreSeen = 0;

/* ---------------- SENSOR STATE ---------------- */
float temperature = 0;
float humidity = 0;

/* ---------------- DEVICE STATE ---------------- */
bool relayLight = false;
bool relayFan = false;

/* ---------------- HELPERS ---------------- */
const char* modeToStr(Mode m) {
  switch(m) {
    case MODE_SLEEP: return "SLEEP";
    case MODE_STUDY: return "STUDY";
    case MODE_GAME: return "GAME";
    case MODE_EMERGENCY: return "EMERGENCY";
    default: return "UNKNOWN";
  }
}

Mode strToMode(const String& s) {
  if(s=="SLEEP") return MODE_SLEEP;
  if(s=="STUDY") return MODE_STUDY;
  if(s=="GAME") return MODE_GAME;
  return MODE_EMERGENCY;
}

/* ---------------- SENSOR SIMULATION ---------------- */
void readSensors() {
  temperature = 22.0 + random(0, 60) / 10.0;
  humidity = 45.0 + random(0, 40) / 10.0;
}

/* ---------------- DEVICE LOGIC ---------------- */
void applyModeLogic() {
  switch(currentMode) {
    case MODE_SLEEP:
      relayLight = false;
      relayFan = false;
      break;
    case MODE_STUDY:
      relayLight = true;
      relayFan = true;
      break;
    case MODE_GAME:
      relayLight = true;
      relayFan = true;
      break;
    case MODE_EMERGENCY:
      relayLight = true;
      relayFan = false;
      break;
  }
}

/* ---------------- MQTT CALLBACK ---------------- */
void mqttCallback(char* topic, byte* payload, unsigned int len) {
  payload[len] = '\0';
  String msg = String((char*)payload);

  if(String(topic) == MODE_TOPIC) {
    currentMode = strToMode(msg);
    lastCoreSeen = millis();
    nodeState = NODE_ACTIVE;
    applyModeLogic();
  }

  if(String(topic) == CMD_TOPIC) {
    if(msg == "LIGHT_ON") relayLight = true;
    if(msg == "LIGHT_OFF") relayLight = false;
    if(msg == "FAN_ON") relayFan = true;
    if(msg == "FAN_OFF") relayFan = false;
  }
}

/* ---------------- TELEMETRY ---------------- */
void publishTelemetry() {
  char buf[64];
  snprintf(buf, sizeof(buf),
    "T=%.1f;H=%.1f;L=%d;F=%d;M=%s",
    temperature, humidity,
    relayLight, relayFan,
    modeToStr(currentMode)
  );
  mqtt.publish(TELEMETRY_TOPIC, buf);
}

/* ---------------- STATUS ---------------- */
void publishStatus() {
  mqtt.publish(STATUS_TOPIC, "ONLINE");
}

/* ---------------- FAILSAFE ---------------- */
void checkFailsafe() {
  if(millis() - lastCoreSeen > 10000) {
    nodeState = NODE_FAILSAFE;
    currentMode = MODE_EMERGENCY;
    applyModeLogic();
  }
}

/* ---------------- SETUP ---------------- */
void setup() {
  Serial.begin(115200);

  WiFi.begin(ssid, password);
  while(WiFi.status() != WL_CONNECTED) delay(500);

  mqtt.setServer(broker, 1883);
  mqtt.setCallback(mqttCallback);
  while(!mqtt.connected()) mqtt.connect("CM3_C3");

  mqtt.subscribe(MODE_TOPIC);
  mqtt.subscribe(CMD_TOPIC);

  lastCoreSeen = millis();
  nodeState = NODE_ACTIVE;
}

/* ---------------- LOOP ---------------- */
void loop() {
  mqtt.loop();

  static unsigned long lastSense = 0;
  static unsigned long lastPub = 0;

  if(millis() - lastSense > 2000) {
    readSensors();
    lastSense = millis();
  }

  if(millis() - lastPub > 3000) {
    publishTelemetry();
    publishStatus();
    lastPub = millis();
  }

  checkFailsafe();
  delay(10);
}
