#include "wifi_manager.h"

#include <ESPmDNS.h>
#include <SD_MMC.h>
#include <WebServer.h>
#include <WiFi.h>

#include "config.h"
#include "storage.h"

static WebServer s_server(80);
static std::function<String()> s_statusJson;
static std::function<bool()> s_onStart;
static std::function<bool()> s_onStop;
static std::function<void()> s_onReboot;
static bool s_mdnsUp = false;
static bool s_begun = false;

void wifiConnect(uint32_t timeoutMs) {
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setHostname(HOSTNAME);
  Serial.printf("[wifi] connecting to %s", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < timeoutMs) {
    delay(250);
    Serial.print('.');
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("[wifi] connected, ip=%s rssi=%d\n",
                  WiFi.localIP().toString().c_str(), WiFi.RSSI());
  } else {
    Serial.println("[wifi] not connected yet, will keep retrying");
  }
  WiFi.setAutoReconnect(true);
}

bool wifiReady() {
  return WiFi.status() == WL_CONNECTED;
}

static void handleStatus() {
  s_server.send(200, "application/json", s_statusJson ? s_statusJson() : "{}");
}

static void handleStart() {
  bool ok = s_onStart ? s_onStart() : false;
  s_server.send(ok ? 200 : 409, "application/json", ok ? "{\"ok\":true}" : "{\"ok\":false,\"error\":\"cannot start now\"}");
}

static void handleStop() {
  bool ok = s_onStop ? s_onStop() : false;
  s_server.send(ok ? 200 : 409, "application/json", ok ? "{\"ok\":true}" : "{\"ok\":false,\"error\":\"not recording\"}");
}

static void handleReboot() {
  s_server.send(200, "application/json", "{\"ok\":true}");
  delay(250);
  if (s_onReboot) s_onReboot();
}

static void handleFiles() {
  std::vector<String> sessions;
  storageListSessions(sessions);
  String out = "[";
  bool first = true;
  for (size_t i = 0; i < sessions.size(); i++) {
    if (!first) out += ',';
    first = false;
    String dir = sessions[i];
    uint32_t frames = storageCountFrames(dir);
    out += "{\"name\":\"" + dir + "\",\"frames\":" + String(frames) +
           ",\"complete\":" + (SD_MMC.exists(String(SD_MOUNTPOINT) + PRINTS_DIR + "/" + dir + "/session.json") ? "true" : "false") + "}";
  }
  out += "]";
  s_server.send(200, "application/json", out);
}

static void handleDownload() {
  String session = s_server.arg("session");
  String file = s_server.arg("file");
  if (!storageValidSessionName(session) || !storageValidFileName(file)) {
    s_server.send(400, "application/json", "{\"error\":\"bad path\"}");
    return;
  }
  String path = String(SD_MOUNTPOINT) + PRINTS_DIR + "/" + session + "/" + file;
  if (!SD_MMC.exists(path)) {
    s_server.send(404, "application/json", "{\"error\":\"not found\"}");
    return;
  }
  File f = SD_MMC.open(path, FILE_READ);
  if (!f) {
    s_server.send(500, "application/json", "{\"error\":\"open failed\"}");
    return;
  }
  const char* mime = file.endsWith(".json") ? "application/json" : "image/jpeg";
  s_server.streamFile(f, mime);
  f.close();
}

static const char INDEX_HTML[] = R"rawliteral(<!doctype html>
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>PrintCam</title>
<style>
body{font-family:system-ui,sans-serif;background:#111;color:#eee;margin:0 auto;max-width:32rem;padding:1.2rem}
h1{font-size:1.4rem;margin:.2rem 0 1rem}
#s{font-size:1.05rem;line-height:1.7;background:#1b1b1b;border-radius:8px;padding:.8rem 1rem;white-space:pre-wrap}
button{margin:1rem .5rem 0 0;padding:.6rem 1.4rem;font-size:1rem;border:0;border-radius:6px;cursor:pointer}
#go{background:#2e7d32;color:#fff}#stop{background:#b71c1c;color:#fff}
a{color:#8ab4f8}
</style>
<h1>&#127909; PrintCam</h1>
<div id="s">loading...</div>
<button id="go" onclick="p('start')">Start</button>
<button id="stop" onclick="p('stop')">Stop</button>
<script>
async function r(){try{
const j=await(await fetch('/status')).json();
const st={idle:'IDLE - watching for motion',recording:'RECORDING',cooldown:'COOLDOWN',error:'ERROR',boot:'BOOTING'}[j.state]||j.state;
let t='state      : '+st+'\n';
t+='session    : '+(j.session||'-')+'\n';
t+='frames     : '+j.frames+'\n';
t+='free space : '+j.free_mb+' MB / '+j.total_mb+' MB\n';
t+='capture    : every '+j.interval_s+'s ('+j.capture+')\n';
t+='time       : '+j.time+'\n';
t+='wifi       : '+j.ip+' ('+j.rssi+' dBm)\n';
t+='uptime     : '+j.uptime_s+'s\n';
if(j.error)t+='error      : '+j.error+'\n';
document.getElementById('s').textContent=t;
}catch(e){document.getElementById('s').textContent='status unreachable: '+e}}
async function p(a){await fetch('/'+a,{method:'POST'});r()}
r();setInterval(r,2000);
</script>
)rawliteral";

static void handleIndex() {
  s_server.send(200, "text/html", INDEX_HTML);
}

void webBegin(std::function<String()> getStatusJson,
              std::function<bool()> onStart,
              std::function<bool()> onStop,
              std::function<void()> onReboot) {
  s_statusJson = getStatusJson;
  s_onStart = onStart;
  s_onStop = onStop;
  s_onReboot = onReboot;

  s_server.on("/", HTTP_GET, handleIndex);
  s_server.on("/status", HTTP_GET, handleStatus);
  s_server.on("/start", HTTP_POST, handleStart);
  s_server.on("/stop", HTTP_POST, handleStop);
  s_server.on("/reboot", HTTP_POST, handleReboot);
  s_server.on("/files", HTTP_GET, handleFiles);
  s_server.on("/download", HTTP_GET, handleDownload);
  s_server.onNotFound([]() {
    s_server.send(404, "application/json", "{\"error\":\"not found\"}");
  });
  s_server.begin();
  s_begun = true;
  Serial.println("[web] server started on port 80");
}

void webLoop() {
  if (!s_begun) return;

  if (WiFi.status() == WL_CONNECTED) {
    if (!s_mdnsUp) {
      if (MDNS.begin(HOSTNAME)) {
        MDNS.addService("http", "tcp", 80);
        s_mdnsUp = true;
        Serial.printf("[web] mDNS: http://%s.local\n", HOSTNAME);
      }
    }
    s_server.handleClient();
  } else if (s_mdnsUp) {
    MDNS.end();
    s_mdnsUp = false;
  }
}
