#include <Arduino.h>
#include <ESPmDNS.h>
#include <IRremoteESP8266.h>
#include <WebServer.h>
#include <WiFi.h>
#include <ir_Fujitsu.h>

// Wi-Fi and device settings live in config.h (git-ignored).
// Copy config.example.h to config.h and fill it in.
#if __has_include("config.h")
#include "config.h"
#endif

#if defined(WIFI_STA_SSID) && defined(WIFI_STA_PASSWORD)
#define HAS_WIFI_SECRETS 1
#else
#define HAS_WIFI_SECRETS 0
#endif

// Fallback access point, used when there is no config.h or the Wi-Fi join
// fails. Set your own AP_PASSWORD (8+ characters) in config.h.
#ifndef AP_SSID
#define AP_SSID "ESP32-AC-Remote"
#endif
#ifndef AP_PASSWORD
#define AP_PASSWORD "change-me-please"
#endif
#ifndef DEVICE_HOSTNAME
#define DEVICE_HOSTNAME "esp32-ac"
#endif

const uint16_t kIrLedPin = 18;
const uint32_t kBaudRate = 115200;
const uint32_t kMinSendIntervalMs = 3000;
const uint32_t kWifiConnectTimeoutMs = 15000;

const char *kApSsid = AP_SSID;
const char *kApPassword = AP_PASSWORD;
const char *kMdnsName = DEVICE_HOSTNAME;

WebServer server(80);
IRFujitsuAC fujitsuAc(kIrLedPin, ARDB1);

bool desiredPower = false;
uint8_t desiredMode = kFujitsuAcModeAuto;
uint8_t desiredFan = kFujitsuAcFanAuto;
uint8_t desiredSwing = kFujitsuAcSwingOff;
float desiredTempC = 24.0;
uint32_t lastSendMs = 0;
String lastCommand = "none";

String modeName(const uint8_t mode) {
  switch (mode) {
    case kFujitsuAcModeAuto:
      return "auto";
    case kFujitsuAcModeCool:
      return "cool";
    case kFujitsuAcModeHeat:
      return "heat";
    case kFujitsuAcModeDry:
      return "dry";
    case kFujitsuAcModeFan:
      return "fan";
    default:
      return "unknown";
  }
}

String fanName(const uint8_t fan) {
  switch (fan) {
    case kFujitsuAcFanAuto:
      return "auto";
    case kFujitsuAcFanHigh:
      return "high";
    case kFujitsuAcFanMed:
      return "medium";
    case kFujitsuAcFanLow:
      return "low";
    case kFujitsuAcFanQuiet:
      return "quiet";
    default:
      return "unknown";
  }
}

bool parseMode(const String &value, uint8_t *mode) {
  if (value == "auto") {
    *mode = kFujitsuAcModeAuto;
  } else if (value == "cool") {
    *mode = kFujitsuAcModeCool;
  } else if (value == "heat") {
    *mode = kFujitsuAcModeHeat;
  } else if (value == "dry") {
    *mode = kFujitsuAcModeDry;
  } else if (value == "fan") {
    *mode = kFujitsuAcModeFan;
  } else {
    return false;
  }
  return true;
}

bool parseFan(const String &value, uint8_t *fan) {
  if (value == "auto") {
    *fan = kFujitsuAcFanAuto;
  } else if (value == "high") {
    *fan = kFujitsuAcFanHigh;
  } else if (value == "medium" || value == "med") {
    *fan = kFujitsuAcFanMed;
  } else if (value == "low") {
    *fan = kFujitsuAcFanLow;
  } else if (value == "quiet") {
    *fan = kFujitsuAcFanQuiet;
  } else {
    return false;
  }
  return true;
}

String jsonStatus() {
  String body = "{";
  body += "\"power\":";
  body += desiredPower ? "true" : "false";
  body += ",\"mode\":\"" + modeName(desiredMode) + "\"";
  body += ",\"temp_c\":";
  body += String(desiredTempC, 1);
  body += ",\"fan\":\"" + fanName(desiredFan) + "\"";
  body += ",\"last_command\":\"" + lastCommand + "\"";
  body += ",\"ip\":\"" + WiFi.localIP().toString() + "\"";
  body += ",\"ap_ip\":\"" + WiFi.softAPIP().toString() + "\"";
  body += "}";
  return body;
}

void applyDesiredState() {
  fujitsuAc.setModel(ARDB1);
  fujitsuAc.setMode(desiredMode);
  fujitsuAc.setFanSpeed(desiredFan);
  fujitsuAc.setTemp(desiredTempC, true);
  fujitsuAc.setSwing(desiredSwing);
  if (desiredPower) {
    fujitsuAc.on();
  } else {
    fujitsuAc.off();
  }
}

bool rateLimitAllowsSend() {
  if (lastSendMs == 0) return true;
  return millis() - lastSendMs >= kMinSendIntervalMs;
}

void sendState(const String &commandName) {
  if (!rateLimitAllowsSend()) {
    server.send(429, "application/json",
                "{\"error\":\"rate_limited\",\"retry_ms\":" +
                    String(kMinSendIntervalMs - (millis() - lastSendMs)) +
                    "}");
    return;
  }

  applyDesiredState();
  fujitsuAc.send();
  lastSendMs = millis();
  lastCommand = commandName;

  Serial.print("Sent command: ");
  Serial.println(commandName);
  Serial.print("State: ");
  Serial.println(fujitsuAc.toString());

  server.send(200, "application/json", jsonStatus());
}

bool applyQueryArgs() {
  if (server.hasArg("temp")) {
    const float temp = server.arg("temp").toFloat();
    if (temp < 16.0 || temp > 30.0) {
      server.send(400, "application/json",
                  "{\"error\":\"temp must be between 16 and 30 C\"}");
      return false;
    }
    desiredTempC = temp;
  }

  if (server.hasArg("mode")) {
    uint8_t parsedMode = desiredMode;
    String value = server.arg("mode");
    value.toLowerCase();
    if (!parseMode(value, &parsedMode)) {
      server.send(400, "application/json",
                  "{\"error\":\"mode must be auto,cool,heat,dry,fan\"}");
      return false;
    }
    desiredMode = parsedMode;
  }

  if (server.hasArg("fan")) {
    uint8_t parsedFan = desiredFan;
    String value = server.arg("fan");
    value.toLowerCase();
    if (!parseFan(value, &parsedFan)) {
      server.send(400, "application/json",
                  "{\"error\":\"fan must be auto,high,medium,low,quiet\"}");
      return false;
    }
    desiredFan = parsedFan;
  }

  return true;
}

// Phone page with buttons (the plain endpoints below still work).
const char kRootPage[] PROGMEM = R"HTML(<!doctype html>
<html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>AC Remote</title>
<style>
:root{--bg:#f2f5f8;--panel:#fff;--ink:#16202b;--muted:#5d6b7a;--cool:#2a7fd4;--off:#4a5562}
*{box-sizing:border-box}body{margin:0;padding:20px 16px;background:var(--bg);color:var(--ink);
font:16px/1.4 system-ui,-apple-system,Segoe UI,Roboto,sans-serif;max-width:460px;margin-inline:auto}
h1{font-size:22px;margin:0 0 16px}.panel{background:var(--panel);border-radius:14px;padding:18px;margin-bottom:14px}
.temp{display:flex;align-items:center;justify-content:space-between;gap:12px}
.big{font-size:52px;font-weight:700;font-variant-numeric:tabular-nums}
button{border:0;border-radius:12px;font-size:18px;font-weight:600;padding:16px;background:#e3e8ee;color:var(--ink)}
button:active{transform:scale(.97)}.sq{width:64px;height:64px;font-size:28px}
.row{display:flex;gap:10px}.row button{flex:1}#on{background:var(--cool);color:#fff}#off{background:var(--off);color:#fff}
#msg{color:var(--muted);font-size:14px;min-height:1.4em}
</style></head><body>
<h1>AC Remote</h1>
<div class="panel"><div class="temp"><button class="sq" id="dn">&minus;</button>
<div><span class="big" id="t">24</span> &deg;C</div><button class="sq" id="up">+</button></div></div>
<div class="row"><button id="on">Cool ON</button><button id="off">OFF</button></div>
<p id="msg"></p>
<script>
const $=id=>document.getElementById(id);let t=24;
function draw(){$('t').textContent=t;}
async function go(p){$('msg').textContent='sending...';try{const r=await fetch(p,{cache:'no-store'});const s=await r.json();
if(s.error){$('msg').textContent=s.error==='rate_limited'?'too fast: wait a second and tap again':'error: '+s.error;return;}
$('msg').textContent='AC '+(s.power?'ON, cool '+s.temp_c+' °C':'OFF')+' (sent)';if(s.temp_c)t=Math.round(s.temp_c);draw();}
catch(e){$('msg').textContent='no connection';}}
$('dn').onclick=()=>{t=Math.max(18,t-1);draw();};$('up').onclick=()=>{t=Math.min(30,t+1);draw();};
$('on').onclick=()=>go('/cool?temp='+t);$('off').onclick=()=>go('/off');
fetch('/status',{cache:'no-store'}).then(r=>r.json()).then(s=>{if(s.temp_c)t=Math.round(s.temp_c);draw();
$('msg').textContent='AC '+(s.power?'ON':'OFF');}).catch(()=>{});
</script></body></html>)HTML";

void handleRoot() {
  server.send_P(200, "text/html; charset=utf-8", kRootPage);
}

void handleOn() {
  if (!applyQueryArgs()) return;
  desiredPower = true;
  sendState("on");
}

void handleOff() {
  desiredPower = false;
  sendState("off");
}

void handleCool() {
  if (!applyQueryArgs()) return;
  desiredPower = true;
  desiredMode = kFujitsuAcModeCool;
  sendState("cool");
}

void handleSet() {
  if (!applyQueryArgs()) return;

  if (server.hasArg("power")) {
    String value = server.arg("power");
    value.toLowerCase();
    if (value == "on" || value == "1" || value == "true") {
      desiredPower = true;
    } else if (value == "off" || value == "0" || value == "false") {
      desiredPower = false;
    } else {
      server.send(400, "application/json",
                  "{\"error\":\"power must be on or off\"}");
      return;
    }
  }

  sendState("set");
}

void handleStatus() {
  server.send(200, "application/json", jsonStatus());
}

void handleNotFound() {
  server.send(404, "application/json", "{\"error\":\"not_found\"}");
}

bool connectStationWifi() {
#if HAS_WIFI_SECRETS
  WiFi.mode(WIFI_STA);
#if defined(WIFI_STATIC_IP) && defined(WIFI_GATEWAY) && defined(WIFI_SUBNET)
  IPAddress staticIp(WIFI_STATIC_IP);
  IPAddress gateway(WIFI_GATEWAY);
  IPAddress subnet(WIFI_SUBNET);
#if defined(WIFI_DNS)
  IPAddress dns(WIFI_DNS);
#else
  IPAddress dns(WIFI_GATEWAY);
#endif
  if (!WiFi.config(staticIp, gateway, subnet, dns)) {
    Serial.println("Static IP configuration failed; continuing with DHCP.");
  } else {
    Serial.print("Requested static IP: ");
    Serial.println(staticIp);
  }
#endif
  WiFi.begin(WIFI_STA_SSID, WIFI_STA_PASSWORD);

  Serial.print("Connecting to WiFi SSID: ");
  Serial.println(WIFI_STA_SSID);

  const uint32_t startMs = millis();
  while (WiFi.status() != WL_CONNECTED &&
         millis() - startMs < kWifiConnectTimeoutMs) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("WiFi connected. IP: ");
    Serial.println(WiFi.localIP());
    return true;
  }

  Serial.println("WiFi station connection failed; starting access point.");
#endif
  return false;
}

void startAccessPoint() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(kApSsid, kApPassword);
  Serial.print("Access point SSID: ");
  Serial.println(kApSsid);
  Serial.print("Access point password: ");
  Serial.println(kApPassword);
  Serial.print("Access point IP: ");
  Serial.println(WiFi.softAPIP());
}

void setupServer() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/on", HTTP_GET, handleOn);
  server.on("/off", HTTP_GET, handleOff);
  server.on("/cool", HTTP_GET, handleCool);
  server.on("/set", HTTP_GET, handleSet);
  server.on("/status", HTTP_GET, handleStatus);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("HTTP server started.");
}

void setupMdns() {
  if (MDNS.begin(kMdnsName)) {
    MDNS.addService("http", "tcp", 80);
    Serial.print("mDNS name: http://");
    Serial.print(kMdnsName);
    Serial.println(".local/");
  } else {
    Serial.println("mDNS start failed.");
  }
}

void setup() {
  Serial.begin(kBaudRate);
  delay(500);

  fujitsuAc.begin();
  applyDesiredState();

  Serial.println();
  Serial.println("FujitsuWifiRemote ready.");

  if (!connectStationWifi()) {
    startAccessPoint();
  }
  setupMdns();
  setupServer();
}

void loop() {
  server.handleClient();
}
