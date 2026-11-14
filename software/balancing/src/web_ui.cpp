#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include "config.h"
#include "shared_state.h"
#include "web_ui.h"

static WebServer server(80);

// Web UI HTML
static const char* HTML = R"rawhtml(
<!DOCTYPE html><html><head>
<meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>BalanceCar</title>
<style>
*{box-sizing:border-box}
body{font:15px sans-serif;max-width:480px;margin:0 auto;padding:10px;background:#f0f0f0}
h2{text-align:center;margin:10px 0;color:#222}
.card{background:#fff;border-radius:8px;padding:12px 15px;margin:8px 0;box-shadow:0 1px 4px rgba(0,0,0,.15)}
.tel{font-family:monospace;font-size:14px;line-height:2.2}
.row{display:flex;align-items:center;margin:10px 0;gap:6px}
.row label{width:68px;font-weight:bold;color:#444;font-size:14px}
.row input[type=range]{flex:1;height:30px;cursor:pointer}
.num{width:58px;font-family:monospace;font-size:14px;text-align:right;color:#0066cc}
.btn{display:block;width:100%;padding:14px;background:#0066cc;color:#fff;border:none;border-radius:6px;font-size:16px;cursor:pointer;margin-top:10px}
.btn:disabled{background:#aaa;cursor:default}
.ok{text-align:center;font-size:12px;color:#090;margin-top:5px;min-height:16px}
</style></head><body>
<h2>Balance Car</h2>
<div class="card">
  <div class="tel" id="tel">接続中...</div>
</div>
<div class="card">
  <div class="row"><label>Kp</label><input type="range" id="Kp" min="0" max="2" step="0.01" oninput="upd('Kp',3)"><span class="num" id="nKp">-</span></div>
  <div class="row"><label>Ki</label><input type="range" id="Ki" min="0" max="0.5" step="0.001" oninput="upd('Ki',4)"><span class="num" id="nKi">-</span></div>
  <div class="row"><label>Kd</label><input type="range" id="Kd" min="0" max="0.5" step="0.001" oninput="upd('Kd',4)"><span class="num" id="nKd">-</span></div>
  <div class="row"><label>Kv</label><input type="range" id="Kv" min="0" max="0.5" step="0.005" oninput="upd('Kv',3)"><span class="num" id="nKv">-</span></div>
  <div class="row"><label>Setpoint</label><input type="range" id="sp" min="-15" max="15" step="0.1" oninput="upd('sp',2)"><span class="num" id="nsp">-</span></div>
  <button class="btn" id="btn" onclick="apply()">適用</button>
  <div class="ok" id="ok"></div>
</div>
<script>
const dec={Kp:3,Ki:4,Kd:4,Kv:4,sp:2};
function upd(k,d){document.getElementById('n'+k).textContent=parseFloat(document.getElementById(k).value).toFixed(d)}
function apply(){
  var b=document.getElementById('btn'); b.disabled=true;
  var q=['Kp','Ki','Kd','Kv','sp'].map(k=>k+'='+document.getElementById(k).value).join('&');
  fetch('/set?'+q)
    .then(()=>{document.getElementById('ok').textContent='✓ 適用 '+new Date().toLocaleTimeString()})
    .catch(()=>{document.getElementById('ok').textContent='❌ 送信エラー'})
    .finally(()=>{b.disabled=false});
}
function poll(){
  fetch('/status').then(r=>r.json()).then(d=>{
    document.getElementById('tel').innerHTML=
      'pitch: <b>'+d.pitch.toFixed(2)+'</b> deg&nbsp;&nbsp;'+'rate: '+d.rate.toFixed(1)+' °/s<br>'+
      'u: <b>'+d.u.toFixed(3)+'</b> V&nbsp;&nbsp;&nbsp;&nbsp;'+'vel: '+d.vel.toFixed(1)+' rad/s<br>'+
      'sp: '+d.sp.toFixed(2)+' deg&nbsp;&nbsp;&nbsp;&nbsp;'+'foc: '+d.foc_hz.toFixed(0)+' Hz';
    ['Kp','Ki','Kd','Kv','sp'].forEach(k=>{
      if(document.activeElement!==document.getElementById(k)){
        document.getElementById(k).value=d[k];
        document.getElementById('n'+k).textContent=parseFloat(d[k]).toFixed(dec[k]);
      }
    });
  }).catch(()=>{});
}
setInterval(poll,500); poll();
</script></body></html>
)rawhtml";

// HTTP ハンドラ
static void handleRoot() {
  server.send(200, "text/html", HTML);
}

static void handleStatus() {
  char buf[256];
  snprintf(buf, sizeof(buf),
    "{\"pitch\":%.2f,\"rate\":%.1f,\"u\":%.3f,\"vel\":%.1f,"
    "\"Kp\":%.4f,\"Ki\":%.4f,\"Kd\":%.4f,\"Kv\":%.4f,\"sp\":%.2f,\"foc_hz\":%.0f}",
    telem_pitch, telem_rate, telem_u, telem_vel,
    Kp, Ki, Kd, Kv, setpoint_deg, telem_foc_hz);
  server.send(200, "application/json", buf);
}

static void handleSet() {
  if (server.hasArg("Kp")) Kp           = server.arg("Kp").toFloat();
  if (server.hasArg("Ki")) Ki           = server.arg("Ki").toFloat();
  if (server.hasArg("Kd")) Kd           = server.arg("Kd").toFloat();
  if (server.hasArg("Kv")) Kv           = server.arg("Kv").toFloat();
  if (server.hasArg("sp")) setpoint_deg = server.arg("sp").toFloat();
  Serial.printf("[Web] Kp=%.3f Ki=%.4f Kd=%.4f Kv=%.4f sp=%.2f\n",
                Kp, Ki, Kd, Kv, setpoint_deg);
  server.send(200, "text/plain", "OK");
}

// FreeRTOS タスク（Core 0）
// HTTP 処理による制御周期への影響を抑えるため、Core 0 で実行。
static void wifiTask(void* param) {
  for (;;) {
    server.handleClient();
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

// 公開 API
void webUiBegin() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(WIFI_SSID, WIFI_PASS, 6);  // AP チャンネル 6
  delay(200);
  Serial.printf("[WiFi] SSID: %s  IP: %s\n", WIFI_SSID, WiFi.softAPIP().toString().c_str());

  server.on("/",       handleRoot);
  server.on("/status", handleStatus);
  server.on("/set",    handleSet);
  server.begin();
  Serial.println("[WiFi] Web server ready  ->  http://192.168.4.1");

  xTaskCreatePinnedToCore(wifiTask, "WiFiTask", 4096, NULL, 1, NULL, 0);
}
