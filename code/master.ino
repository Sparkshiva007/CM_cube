#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Preferences.h>
#include <lvgl.h>
#include <LovyanGFX.hpp>
#include <LGFX_AUTODETECT.hpp>

static LGFX tft;
Preferences prefs;

/* ---------------- NETWORK ---------------- */
const char* ssid = "JioFiber-shiva360";
const char* pass = "shiva@786";
const char* broker = "broker.hivemq.com";

/* ---------------- MQTT ---------------- */
WiFiClient espClient;
PubSubClient mqtt(espClient);

/* ---------------- CM3 ENUMS ---------------- */
enum CM3Mode { MODE_SLEEP, MODE_STUDY, MODE_GAME, MODE_EMERGENCY, MODE_MANUAL };
enum NodeID { NODE_C3, NODE_C6 };

/* ---------------- GLOBAL STATE ---------------- */
CM3Mode currentMode = MODE_STUDY;
bool manualOverride = false;
unsigned long manualOverrideUntil = 0;

/* ---------------- NODE STATE ---------------- */
struct NodeState {
  float temp;
  float hum;
  int aqi;
  bool online;
  unsigned long lastSeen;
};

NodeState c3, c6;

/* ---------------- RULE ENGINE ---------------- */
struct Rule {
  bool enabled;
  float tempMin, tempMax;
  int aqiMax;
  CM3Mode targetMode;
  uint8_t priority;
};

Rule rules[4];

/* ---------------- UI ---------------- */
lv_obj_t *lblMode, *lblC3, *lblC6, *lblLog;

/* ---------------- DISPLAY ---------------- */
void disp_flush(lv_display_t *d, const lv_area_t *a, uint8_t *px) {
  tft.pushImage(a->x1, a->y1,
                a->x2 - a->x1 + 1,
                a->y2 - a->y1 + 1,
                (uint16_t*)px);
  lv_display_flush_ready(d);
}

/* ---------------- LOGGING ---------------- */
void logMsg(const char* msg) {
  Serial.println(msg);
  lv_label_set_text(lblLog, msg);
}

/* ---------------- MODE HELPERS ---------------- */
const char* modeStr(CM3Mode m) {
  switch(m) {
    case MODE_SLEEP: return "SLEEP";
    case MODE_STUDY: return "STUDY";
    case MODE_GAME: return "GAME";
    case MODE_EMERGENCY: return "EMERGENCY";
    case MODE_MANUAL: return "MANUAL";
    default: return "UNKNOWN";
  }
}

void publishMode() {
  mqtt.publish("cm3/system/mode", modeStr(currentMode), true);
  lv_label_set_text_fmt(lblMode, "MODE: %s", modeStr(currentMode));
}

/* ---------------- RULE ENGINE ---------------- */
void initRules() {
  rules[0] = {true, 0, 100, 80, MODE_STUDY, 1};
  rules[1] = {true, 30, 100, 200, MODE_GAME, 2};
  rules[2] = {true, 0, 100, 120, MODE_SLEEP, 3};
  rules[3] = {true, 0, 100, 300, MODE_EMERGENCY, 4};
}

void evaluateRules() {
  if(manualOverride && millis() < manualOverrideUntil) return;
  manualOverride = false;

  CM3Mode selected = MODE_STUDY;
  uint8_t bestPrio = 0;

  for(int i=0;i<4;i++) {
    if(!rules[i].enabled) continue;
    if(c3.temp >= rules[i].tempMin &&
       c3.temp <= rules[i].tempMax &&
       c6.aqi <= rules[i].aqiMax) {
      if(rules[i].priority > bestPrio) {
        bestPrio = rules[i].priority;
        selected = rules[i].targetMode;
      }
    }
  }

  if(selected != currentMode) {
    currentMode = selected;
    publishMode();
    logMsg("Rule triggered mode change");
  }
}

/* ---------------- MQTT CALLBACK ---------------- */
void mqttCallback(char* topic, byte* payload, unsigned int len) {
  payload[len] = '\0';
  String msg = String((char*)payload);

  if(String(topic) == "cm3/node/c3/telemetry") {
    sscanf(msg.c_str(), "T=%f;H=%f", &c3.temp, &c3.hum);
    c3.lastSeen = millis();
    c3.online = true;
    lv_label_set_text_fmt(lblC3, "C3  T:%.1f  H:%.1f", c3.temp, c3.hum);
  }

  if(String(topic) == "cm3/node/c6/telemetry") {
    c6.aqi = msg.substring(msg.indexOf('=')+1).toInt();
    c6.lastSeen = millis();
    c6.online = true;
    lv_label_set_text_fmt(lblC6, "C6  AQI:%d", c6.aqi);
  }

  if(String(topic) == "cm3/system/manual") {
    manualOverride = true;
    manualOverrideUntil = millis() + 600000;
    currentMode = MODE_MANUAL;
    publishMode();
    logMsg("Manual override enabled");
  }
}

/* ---------------- HEALTH WATCHDOG ---------------- */
void checkHealth() {
  unsigned long now = millis();
  c3.online = (now - c3.lastSeen < 7000);
  c6.online = (now - c6.lastSeen < 7000);

  if(!c6.online) {
    currentMode = MODE_EMERGENCY;
    publishMode();
    logMsg("C6 offline EMERGENCY");
  }
}

/* ---------------- UI ---------------- */
void buildUI() {
  lv_obj_t *scr = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), 0);

  lblMode = lv_label_create(scr);
  lv_obj_align(lblMode, LV_ALIGN_TOP_MID, 0, 10);

  lblC3 = lv_label_create(scr);
  lv_obj_align(lblC3, LV_ALIGN_CENTER, 0, -20);

  lblC6 = lv_label_create(scr);
  lv_obj_align(lblC6, LV_ALIGN_CENTER, 0, 10);

  lblLog = lv_label_create(scr);
  lv_obj_align(lblLog, LV_ALIGN_BOTTOM_MID, 0, -10);

  lv_screen_load(scr);
}

/* ---------------- SETUP ---------------- */
void setup() {
  Serial.begin(115200);
  initRules();

  tft.init();
  tft.setRotation(1);
  tft.setBrightness(200);
  tft.setSwapBytes(true);

  lv_init();
  lv_display_t *disp = lv_display_create(320,240);
  lv_display_set_flush_cb(disp, disp_flush);
  static uint8_t buf[320*40*2];
  lv_display_set_buffers(disp, buf, NULL, sizeof(buf), LV_DISPLAY_RENDER_MODE_PARTIAL);
  buildUI();

  WiFi.begin(ssid, pass);
  while(WiFi.status()!=WL_CONNECTED) delay(500);

  mqtt.setServer(broker,1883);
  mqtt.setCallback(mqttCallback);
  while(!mqtt.connected()) mqtt.connect("CM3_CORE");

  mqtt.subscribe("cm3/node/c3/telemetry");
  mqtt.subscribe("cm3/node/c6/telemetry");
  mqtt.subscribe("cm3/system/manual");

  publishMode();
}

/* ---------------- LOOP ---------------- */
void loop() {
  mqtt.loop();
  lv_timer_handler();

  static unsigned long lastEval=0;
  if(millis()-lastEval>3000) {
    evaluateRules();
    checkHealth();
    lastEval = millis();
  }

  delay(5);
}
