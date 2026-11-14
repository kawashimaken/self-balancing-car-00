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
.apply-btn{display:block;width:100%;padding:14px;background:#0066cc;color:#fff;border:none;border-radius:6px;font-size:16px;cursor:pointer;margin-top:10px}
.apply-btn:disabled{background:#aaa;cursor:default}
.ok{text-align:center;font-size:12px;color:#090;margin-top:5px;min-height:16px}
.tabs{display:flex;gap:4px;margin:8px 0}
.tab{flex:1;padding:10px;background:#ddd;border:none;border-radius:6px;font-size:15px;font-weight:bold;cursor:pointer;color:#555}
.tab.active{background:#0066cc;color:#fff}
.dpad{display:grid;grid-template-columns:repeat(3,84px);gap:10px;justify-content:center;margin:16px auto}
.dp{width:84px;height:74px;font-size:32px;background:#e0e0e0;border:2px solid #bbb;border-radius:8px;cursor:pointer;-webkit-user-select:none;user-select:none;touch-action:none;display:flex;align-items:center;justify-content:center}
.dp:active,.dp.on{background:#0066cc;color:#fff;border-color:#004eaa}
.dp-stop{font-size:24px;background:#e8e8e8}
.gap{width:84px;height:74px}
.cmd-info{text-align:center;margin:10px 0;font-family:monospace;font-size:13px;color:#555}
.hint{text-align:center;font-size:11px;color:#999;margin-top:4px}
</style></head><body>
<h2>Balance Car</h2>
<div class="card"><div class="tel" id="tel">接続中...</div></div>
<div class="tabs">
  <button class="tab active" id="t0" onclick="showTab(0)">パラメータ</button>
  <button class="tab" id="t1" onclick="showTab(1)">走行制御</button>
</div>
<div class="card" id="p0">
  <div class="row"><label>Kp</label><input type="range" id="Kp" min="0" max="2" step="0.01" oninput="upd('Kp',3)"><span class="num" id="nKp">-</span></div>
  <div class="row"><label>Ki</label><input type="range" id="Ki" min="0" max="0.5" step="0.001" oninput="upd('Ki',4)"><span class="num" id="nKi">-</span></div>
  <div class="row"><label>Kd</label><input type="range" id="Kd" min="0" max="0.5" step="0.001" oninput="upd('Kd',4)"><span class="num" id="nKd">-</span></div>
  <div class="row"><label>Kv</label><input type="range" id="Kv" min="0" max="2" step="0.01" oninput="upd('Kv',2)"><span class="num" id="nKv">-</span></div>
  <div class="row"><label>Kvi</label><input type="range" id="Kvi" min="0" max="5" step="0.05" oninput="upd('Kvi',2)"><span class="num" id="nKvi">-</span></div>
  <div class="row"><label>Vmax</label><input type="range" id="Vmax" min="2" max="12" step="0.5" oninput="upd('Vmax',1)"><span class="num" id="nVmax">-</span></div>
  <div class="row"><label>Setpoint</label><input type="range" id="sp" min="-15" max="15" step="0.1" oninput="upd('sp',2)"><span class="num" id="nsp">-</span></div>
  <button class="apply-btn" id="aBtn" onclick="applyP()">適用</button>
  <div class="ok" id="ok"></div>
</div>
<div class="card" id="p1" style="display:none">
  <div class="dpad">
    <div class="gap"></div>
    <div class="dp" id="bf" data-d="1" data-t="0">&#8593;</div>
    <div class="gap"></div>
    <div class="dp" id="bl" data-d="0" data-t="-1">&#8592;</div>
    <div class="dp dp-stop" id="bs">&#9632;</div>
    <div class="dp" id="br" data-d="0" data-t="1">&#8594;</div>
    <div class="gap"></div>
    <div class="dp" id="bb" data-d="-1" data-t="0">&#8595;</div>
    <div class="gap"></div>
  </div>
  <div class="cmd-info">drive: <span id="cd">0.00</span> &nbsp;&nbsp; turn: <span id="ct">0.00</span></div>
  <div class="hint">ボタンを押し続けると走行。離すと自動停止。</div>
</div>
<script>
function showTab(n){
  [0,1].forEach(function(i){
    document.getElementById('p'+i).style.display=i===n?'':'none';
    document.getElementById('t'+i).classList.toggle('active',i===n);
  });
  if(n!==1)stopCmd();
}
var dec={Kp:3,Ki:4,Kd:4,Kv:2,Kvi:2,Vmax:1,sp:2};
function upd(k,d){document.getElementById('n'+k).textContent=parseFloat(document.getElementById(k).value).toFixed(d)}
function applyP(){
  var b=document.getElementById('aBtn');b.disabled=true;
  var q=['Kp','Ki','Kd','Kv','Kvi','Vmax','sp'].map(function(k){return k+'='+document.getElementById(k).value}).join('&');
  fetch('/set?'+q)
    .then(function(){document.getElementById('ok').textContent='✓ 適用 '+new Date().toLocaleTimeString()})
    .catch(function(){document.getElementById('ok').textContent='❌ 送信エラー'})
    .finally(function(){b.disabled=false});
}
function poll(){
  fetch('/status').then(function(r){return r.json()}).then(function(d){
    document.getElementById('tel').innerHTML=
      'pitch: <b>'+d.pitch.toFixed(2)+'</b> deg&nbsp;&nbsp;rate: '+d.rate.toFixed(1)+' &#176;/s<br>'+
      'u: <b>'+d.u.toFixed(3)+'</b> V&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;vel: '+d.vel.toFixed(1)+' rad/s<br>'+
      'sp: '+d.sp.toFixed(2)+' deg&nbsp;&nbsp;pcmd: '+d.pcmd.toFixed(2)+' deg&nbsp;&nbsp;foc: '+d.foc_hz.toFixed(0)+' Hz';
    ['Kp','Ki','Kd','Kv','Kvi','Vmax','sp'].forEach(function(k){
      if(document.activeElement!==document.getElementById(k)){
        document.getElementById(k).value=d[k];
        document.getElementById('n'+k).textContent=parseFloat(d[k]).toFixed(dec[k]);
      }
    });
  }).catch(function(){});
}
setInterval(poll,500);poll();

var rpt=null;
function sendCmd(d,t){
  fetch('/cmd?drive='+d+'&turn='+t)
    .then(function(r){return r.json()})
    .then(function(s){
      document.getElementById('cd').textContent=s.drive.toFixed(2);
      document.getElementById('ct').textContent=s.turn.toFixed(2);
    }).catch(function(){});
}
function stopCmd(){clearInterval(rpt);rpt=null;sendCmd(0,0);}

document.querySelectorAll('.dp[data-d]').forEach(function(el){
  var d=parseFloat(el.dataset.d),t=parseFloat(el.dataset.t);
  function go(e){e.preventDefault();el.classList.add('on');sendCmd(d,t);clearInterval(rpt);rpt=setInterval(function(){sendCmd(d,t)},150);}
  function stop(e){e.preventDefault();el.classList.remove('on');clearInterval(rpt);rpt=null;sendCmd(0,0);}
  el.addEventListener('mousedown',go);
  el.addEventListener('touchstart',go,{passive:false});
  el.addEventListener('mouseup',stop);
  el.addEventListener('touchend',stop);
  el.addEventListener('mouseleave',stop);
  el.addEventListener('touchcancel',stop);
});
document.getElementById('bs').addEventListener('click',function(){sendCmd(0,0);});
</script></body></html>
)rawhtml";

// HTTP ハンドラ
static void handleRoot() {
  server.send(200, "text/html", HTML);
}

static void handleStatus() {
  char buf[320];
  snprintf(buf, sizeof(buf),
    "{\"pitch\":%.2f,\"rate\":%.1f,\"u\":%.3f,\"vel\":%.1f,\"pcmd\":%.2f,"
    "\"Kp\":%.4f,\"Ki\":%.4f,\"Kd\":%.4f,\"Kv\":%.4f,\"Kvi\":%.4f,\"Vmax\":%.1f,"
    "\"sp\":%.2f,\"foc_hz\":%.0f}",
    telem_pitch, telem_rate, telem_u, telem_vel, telem_pitch_cmd,
    Kp, Ki, Kd, Kv, Kvi, DriveVmax, setpoint_deg, telem_foc_hz);
  server.send(200, "application/json", buf);
}

static void handleSet() {
  if (server.hasArg("Kp")) Kp           = server.arg("Kp").toFloat();
  if (server.hasArg("Ki")) Ki           = server.arg("Ki").toFloat();
  if (server.hasArg("Kd")) Kd           = server.arg("Kd").toFloat();
  if (server.hasArg("Kv"))  Kv           = server.arg("Kv").toFloat();
  if (server.hasArg("Kvi"))  Kvi          = server.arg("Kvi").toFloat();
  if (server.hasArg("Vmax")) DriveVmax    = constrain(server.arg("Vmax").toFloat(), 0.0f, 20.0f);
  if (server.hasArg("sp"))   setpoint_deg = server.arg("sp").toFloat();
  Serial.printf("[Web] Kp=%.3f Ki=%.4f Kd=%.4f Kv=%.4f Kvi=%.4f Vmax=%.1f sp=%.2f\n",
                Kp, Ki, Kd, Kv, Kvi, DriveVmax, setpoint_deg);
  server.send(200, "text/plain", "OK");
}

// /cmd?drive=<-1..1>&turn=<-1..1>
// Web UI からの走行指令を受信。
static void handleCmd() {
  if (server.hasArg("drive")) cmd_drive = constrain(server.arg("drive").toFloat(), -1.0f, 1.0f);
  if (server.hasArg("turn"))  cmd_turn  = constrain(server.arg("turn").toFloat(),  -1.0f, 1.0f);
  last_cmd_ms = millis();
  char buf[64];
  snprintf(buf, sizeof(buf), "{\"drive\":%.2f,\"turn\":%.2f}", cmd_drive, cmd_turn);
  server.send(200, "application/json", buf);
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
void webUiHandle() {
  // Web UI は Core 0 のタスクで処理。
}

void webUiBegin() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(WIFI_SSID, WIFI_PASS, 6);  // AP チャンネル 6
  delay(200);
  Serial.printf("[WiFi] SSID: %s  IP: %s\n", WIFI_SSID, WiFi.softAPIP().toString().c_str());

  server.on("/",       handleRoot);
  server.on("/status", handleStatus);
  server.on("/set",    handleSet);
  server.on("/cmd",    handleCmd);
  server.begin();
  Serial.println("[WiFi] Web server ready  ->  http://192.168.4.1");

  xTaskCreatePinnedToCore(wifiTask, "WiFiTask", 4096, NULL, 1, NULL, 0);
}
