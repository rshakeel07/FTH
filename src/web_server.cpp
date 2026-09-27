#include <Arduino.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <WiFi.h>
#include "heart_rate.h"
#include "imu.h"
#include "web_server.h"

namespace {
WebServer server(80);
WebSocketsServer socketServer(81);
bool serverStarted = false;

const char DASHBOARD_PAGE[] PROGMEM = R"HTML(
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>FTH Telemetry</title>
  <style>
    :root {
      color-scheme: light;
      --paper: #f3f6f4;
      --surface: #ffffff;
      --ink: #182522;
      --muted: #65736f;
      --line: #d8e1dd;
      --green: #087f67;
      --green-soft: #e0f3ed;
      --orange: #d8663a;
      --blue: #3478a7;
      --red: #bb4050;
      --grid: #e8eeeb;
    }
    * { box-sizing: border-box; }
    body {
      margin: 0;
      background: var(--paper);
      color: var(--ink);
      font: 14px/1.45 "Segoe UI", Tahoma, sans-serif;
    }
    header {
      min-height: 70px;
      padding: 14px max(20px, calc((100vw - 1240px) / 2));
      display: flex;
      justify-content: space-between;
      align-items: center;
      gap: 20px;
      background: var(--ink);
      color: #f7fbf9;
      border-bottom: 3px solid var(--green);
    }
    .brand { display: flex; align-items: baseline; gap: 14px; }
    .brand strong { font-size: 20px; font-weight: 650; }
    .brand span { color: #a9beb6; font-size: 12px; }
    .connection { display: flex; align-items: center; gap: 8px; color: #d0ddd8; }
    .dot { width: 9px; height: 9px; border-radius: 50%; background: var(--orange); }
    .dot.online { background: #50d4a4; }
    main { max-width: 1240px; margin: 0 auto; padding: 24px 20px 40px; }
    .toolbar {
      display: flex;
      justify-content: space-between;
      align-items: end;
      gap: 16px;
      margin-bottom: 18px;
    }
    h1 { margin: 0; font-size: 22px; font-weight: 650; }
    .subhead { color: var(--muted); margin-top: 3px; }
    label { display: block; color: var(--muted); font-size: 12px; margin-bottom: 4px; }
    select {
      min-width: 130px;
      padding: 8px 30px 8px 10px;
      border: 1px solid var(--line);
      border-radius: 5px;
      background: var(--surface);
      color: var(--ink);
      font: inherit;
    }
    .metrics {
      display: grid;
      grid-template-columns: repeat(3, minmax(0, 1fr));
      border: 1px solid var(--line);
      background: var(--surface);
      margin-bottom: 18px;
    }
    .metric { padding: 15px 18px; min-width: 0; }
    .metric + .metric { border-left: 1px solid var(--line); }
    .metric-label { color: var(--muted); font-size: 12px; text-transform: uppercase; }
    .metric-value {
      margin-top: 4px;
      font: 600 25px/1.2 Consolas, "Courier New", monospace;
      font-variant-numeric: tabular-nums;
      overflow-wrap: anywhere;
    }
    .metric-value small { font: 13px "Segoe UI", Tahoma, sans-serif; color: var(--muted); }
    .metric-note { color: var(--muted); font-size: 12px; margin-top: 3px; }
    .charts { display: grid; grid-template-columns: 1fr 1fr; gap: 18px; }
    .panel { min-width: 0; background: var(--surface); border: 1px solid var(--line); border-radius: 5px; }
    .panel-head {
      min-height: 56px;
      padding: 12px 15px;
      border-bottom: 1px solid var(--line);
      display: flex;
      justify-content: space-between;
      align-items: center;
      gap: 10px;
    }
    .panel-head h2 { font-size: 14px; margin: 0; font-weight: 650; }
    .panel-head span { color: var(--muted); font-size: 12px; }
    .canvas-wrap { padding: 12px 10px 8px; }
    canvas { display: block; width: 100%; height: 240px; }
    .wide { grid-column: 1 / -1; }
    .digital canvas { height: 250px; }
    .legend { display: flex; flex-wrap: wrap; gap: 14px; padding: 0 16px 13px; color: var(--muted); font-size: 12px; }
    .legend-item { display: inline-flex; align-items: center; gap: 6px; }
    .swatch { width: 9px; height: 9px; border-radius: 2px; background: currentColor; }
    .events { max-height: 250px; overflow: auto; }
    .event-row {
      display: grid;
      grid-template-columns: 105px minmax(0, 1fr);
      gap: 10px;
      padding: 9px 15px;
      border-bottom: 1px solid var(--grid);
      font-size: 13px;
    }
    .event-row:last-child { border-bottom: 0; }
    .event-time { color: var(--muted); font-family: Consolas, "Courier New", monospace; }
    .empty { padding: 18px 15px; color: var(--muted); }
    footer { color: var(--muted); padding: 14px 2px 0; font-size: 12px; }
    @media (max-width: 760px) {
      header { padding: 13px 16px; }
      .brand { display: block; }
      .brand span { display: block; }
      main { padding: 18px 12px 28px; }
      .charts { grid-template-columns: 1fr; gap: 12px; }
      .wide { grid-column: auto; }
      .metrics { grid-template-columns: 1fr; }
      .metric + .metric { border-left: 0; border-top: 1px solid var(--line); }
      .toolbar { align-items: start; }
      canvas { height: 210px; }
    }
    @media (max-width: 420px) {
      .connection { font-size: 12px; }
      .toolbar { flex-direction: column; }
      .event-row { grid-template-columns: 88px minmax(0, 1fr); }
    }
  </style>
</head>
<body>
  <header>
    <div class="brand"><strong>FTH / TELEMETRY</strong><span>ESP32 live monitor</span></div>
    <div class="connection"><span id="linkDot" class="dot"></span><span id="linkText">Connecting</span></div>
  </header>
  <main>
    <section class="toolbar">
      <div><h1>Live telemetry</h1><div class="subhead">Sensor and alert stream from this session</div></div>
      <div><label for="windowSelect">Graph window</label><select id="windowSelect"><option value="15000">15 seconds</option><option value="30000" selected>30 seconds</option><option value="60000">60 seconds</option></select></div>
    </section>
    <section class="metrics" aria-label="Current readings">
      <div class="metric"><div class="metric-label">Time since boot</div><div id="uptimeValue" class="metric-value">--</div><div class="metric-note">milliseconds</div></div>
      <div class="metric"><div class="metric-label">Heart rate</div><div id="bpmValue" class="metric-value">-- <small>BPM</small></div><div id="bpmNote" class="metric-note">Waiting for a valid reading</div></div>
      <div class="metric"><div class="metric-label">IMU jerk</div><div id="imuValue" class="metric-value">--</div><div class="metric-note">Acceleration-change alert</div></div>
    </section>
    <section class="charts">
      <article class="panel">
        <div class="panel-head"><h2>Heart rate</h2><span>beats per minute</span></div>
        <div class="canvas-wrap"><canvas id="bpmChart" aria-label="Live heart rate graph"></canvas></div>
      </article>
      <article class="panel">
        <div class="panel-head"><h2>IMU jerk</h2><span>HIGH / LOW state</span></div>
        <div class="canvas-wrap"><canvas id="imuChart" aria-label="Live IMU jerk graph"></canvas></div>
      </article>
      <article class="panel wide digital">
        <div class="panel-head"><h2>Buzzer outputs</h2><span>HIGH means the ESP32 output is active</span></div>
        <div class="canvas-wrap"><canvas id="buzzerChart" aria-label="Live left and right buzzer outputs graph"></canvas></div>
        <div class="legend"><span class="legend-item" style="color:#d8663a"><i class="swatch"></i>Left buzzer</span><span class="legend-item" style="color:#3478a7"><i class="swatch"></i>Right buzzer</span></div>
      </article>
      <article class="panel wide">
        <div class="panel-head"><h2>Alert events</h2><span>Transitions timestamped in milliseconds since boot</span></div>
        <div id="events" class="events"><div class="empty">No alert transitions received yet.</div></div>
      </article>
    </section>
    <footer>Graphs begin when this page connects. BPM is an experimental sensor estimate, not a medical reading.</footer>
  </main>
  <script>
    const history = [];
    const events = [];
    const maxEvents = 80;
    let lastPacket = null;
    let lastRenderAt = 0;
    const colors = { bpm: "#087f67", imu: "#087f67", left: "#d8663a", right: "#3478a7" };
    const windowSelect = document.getElementById("windowSelect");

    function setConnection(online, text) {
      document.getElementById("linkDot").classList.toggle("online", online);
      document.getElementById("linkText").textContent = text;
    }

    function addEvent(time, label, active) {
      events.unshift({ time, label: label + (active ? " ON" : " OFF"), active });
      if (events.length > maxEvents) events.length = maxEvents;
      renderEvents();
    }

    function renderEvents() {
      const container = document.getElementById("events");
      if (!events.length) return;
      container.replaceChildren(...events.map((event) => {
        const row = document.createElement("div");
        row.className = "event-row";
        const stamp = document.createElement("span");
        stamp.className = "event-time";
        stamp.textContent = event.time + " ms";
        const description = document.createElement("span");
        description.textContent = event.label;
        description.style.color = event.active ? "#182522" : "#65736f";
        row.append(stamp, description);
        return row;
      }));
    }

    function receive(packet) {
      const sample = {
        t: Number(packet.uptime_ms),
        bpm: packet.heart_rate_bpm == null ? null : Number(packet.heart_rate_bpm),
        imu: Boolean(packet.imu_jerking),
        left: Boolean(packet.buzzer_left),
        right: Boolean(packet.buzzer_right)
      };
      if (!Number.isFinite(sample.t)) return;

      if (lastPacket) {
        if (sample.imu !== lastPacket.imu) addEvent(sample.t, "IMU jerk", sample.imu);
        if (sample.left !== lastPacket.left) addEvent(sample.t, "Left buzzer", sample.left);
        if (sample.right !== lastPacket.right) addEvent(sample.t, "Right buzzer", sample.right);
      }
      lastPacket = sample;
      history.push(sample);

      const uptime = document.getElementById("uptimeValue");
      uptime.textContent = sample.t.toLocaleString("en-US");
      const bpmValue = document.getElementById("bpmValue");
      const bpmNote = document.getElementById("bpmNote");
      if (sample.bpm == null || sample.bpm <= 0) {
        bpmValue.innerHTML = "-- <small>BPM</small>";
        bpmNote.textContent = "Place sensor steadily against forehead";
      } else {
        bpmValue.innerHTML = `${sample.bpm} <small>BPM</small>`;
        bpmNote.textContent = "Live rolling average";
      }
      document.getElementById("imuValue").textContent = sample.imu ? "HIGH" : "LOW";

      const cutoff = sample.t - Number(windowSelect.value);
      while (history.length && history[0].t < cutoff) history.shift();

      const now = performance.now();
      if (now - lastRenderAt > 70) {
        lastRenderAt = now;
        requestAnimationFrame(renderCharts);
      }
    }

    function setupCanvas(canvas) {
      const ratio = window.devicePixelRatio || 1;
      const width = canvas.clientWidth;
      const height = canvas.clientHeight;
      if (canvas.width !== Math.round(width * ratio) || canvas.height !== Math.round(height * ratio)) {
        canvas.width = Math.round(width * ratio);
        canvas.height = Math.round(height * ratio);
      }
      const ctx = canvas.getContext("2d");
      ctx.setTransform(ratio, 0, 0, ratio, 0, 0);
      ctx.clearRect(0, 0, width, height);
      return { ctx, width, height };
    }

    function chartFrame(ctx, width, height, minY, maxY, formatY, laneLabels) {
      const left = 58;
      const right = width - 12;
      const top = 12;
      const bottom = height - 30;
      ctx.font = "11px Consolas, monospace";
      ctx.textBaseline = "middle";
      ctx.strokeStyle = "#e8eeeb";
      ctx.fillStyle = "#65736f";
      ctx.lineWidth = 1;

      if (laneLabels) {
        const laneHeight = (bottom - top) / laneLabels.length;
        laneLabels.forEach((lane, index) => {
          const laneTop = top + index * laneHeight;
          const highY = laneTop + laneHeight * 0.28;
          const lowY = laneTop + laneHeight * 0.76;
          ctx.fillText(lane.label, 2, (highY + lowY) / 2);
          ctx.fillText("HIGH", left - 48, highY);
          ctx.fillText("LOW", left - 42, lowY);
          [highY, lowY].forEach((y) => {
            ctx.beginPath(); ctx.moveTo(left, y); ctx.lineTo(right, y); ctx.stroke();
          });
        });
      } else {
        for (let i = 0; i <= 4; i++) {
          const y = top + (bottom - top) * i / 4;
          const value = maxY - (maxY - minY) * i / 4;
          ctx.beginPath(); ctx.moveTo(left, y); ctx.lineTo(right, y); ctx.stroke();
          ctx.fillText(formatY(value), 2, y);
        }
      }

      const latest = history.length ? history[history.length - 1].t : 0;
      const windowMs = Number(windowSelect.value);
      for (let i = 0; i <= 4; i++) {
        const x = left + (right - left) * i / 4;
        const time = Math.max(0, latest - windowMs + windowMs * i / 4);
        ctx.beginPath(); ctx.moveTo(x, top); ctx.lineTo(x, bottom); ctx.stroke();
        ctx.textAlign = i === 0 ? "left" : i === 4 ? "right" : "center";
        ctx.textBaseline = "alphabetic";
        ctx.fillText(`${Math.round(time)} ms`, x, height - 7);
        ctx.textBaseline = "middle";
      }
      ctx.textAlign = "left";
      return { left, right, top, bottom, minY, maxY, latest, windowMs };
    }

    function drawBpm() {
      const canvas = document.getElementById("bpmChart");
      const { ctx, width, height } = setupCanvas(canvas);
      const values = history.map((sample) => sample.bpm).filter((value) => value != null && value > 0);
      let min = 40;
      let max = 180;
      if (values.length) {
        min = Math.max(30, Math.floor(Math.min(...values) / 10) * 10 - 10);
        max = Math.min(220, Math.ceil(Math.max(...values) / 10) * 10 + 10);
        if (max - min < 40) { min = Math.max(30, min - 20); max = Math.min(220, max + 20); }
      }
      const frame = chartFrame(ctx, width, height, min, max, (value) => `${Math.round(value)}`, null);
      const points = history.filter((sample) => sample.bpm != null && sample.bpm > 0);
      if (!points.length) return;
      ctx.strokeStyle = colors.bpm;
      ctx.lineWidth = 2;
      ctx.beginPath();
      points.forEach((sample, index) => {
        const x = frame.left + (frame.right - frame.left) * (sample.t - (frame.latest - frame.windowMs)) / frame.windowMs;
        const y = frame.bottom - (frame.bottom - frame.top) * (sample.bpm - min) / (max - min);
        if (index === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y);
      });
      ctx.stroke();
    }

    function drawStateGraph(canvasId, lanes) {
      const canvas = document.getElementById(canvasId);
      const { ctx, width, height } = setupCanvas(canvas);
      const frame = chartFrame(ctx, width, height, 0, 1, (value) => `${value}`, lanes);
      if (history.length < 2) return;

      lanes.forEach((lane, laneIndex) => {
        const laneHeight = (frame.bottom - frame.top) / lanes.length;
        const highY = frame.top + laneIndex * laneHeight + laneHeight * 0.28;
        const lowY = frame.top + laneIndex * laneHeight + laneHeight * 0.76;
        const yFor = (sample) => sample[lane.key] ? highY : lowY;
        ctx.strokeStyle = lane.color;
        ctx.lineWidth = 2;
        ctx.beginPath();
        const first = history[0];
        const firstX = frame.left + (frame.right - frame.left) * (first.t - (frame.latest - frame.windowMs)) / frame.windowMs;
        ctx.moveTo(firstX, yFor(first));
        for (let i = 1; i < history.length; i++) {
          const sample = history[i];
          const x = frame.left + (frame.right - frame.left) * (sample.t - (frame.latest - frame.windowMs)) / frame.windowMs;
          ctx.lineTo(x, yFor(history[i - 1]));
          ctx.lineTo(x, yFor(sample));
        }
        ctx.lineTo(frame.right, yFor(history[history.length - 1]));
        ctx.stroke();
      });
    }

    function renderCharts() {
      drawBpm();
      drawStateGraph("imuChart", [{ key: "imu", label: "IMU", color: colors.imu }]);
      drawStateGraph("buzzerChart", [
        { key: "left", label: "LEFT", color: colors.left },
        { key: "right", label: "RIGHT", color: colors.right }
      ]);
    }

    function connect() {
      setConnection(false, "Connecting");
      const socket = new WebSocket(`ws://${location.hostname}:81/`);
      socket.onopen = () => setConnection(true, "Live · 10 Hz");
      socket.onmessage = (message) => {
        try { receive(JSON.parse(message.data)); } catch (_) { /* Ignore malformed packets. */ }
      };
      socket.onclose = () => { setConnection(false, "Reconnecting"); setTimeout(connect, 1000); };
      socket.onerror = () => socket.close();
    }

    windowSelect.addEventListener("change", () => {
      if (lastPacket) {
        const cutoff = lastPacket.t - Number(windowSelect.value);
        while (history.length && history[0].t < cutoff) history.shift();
      }
      renderCharts();
    });
    window.addEventListener("resize", renderCharts);
    connect();
  </script>
</body>
</html>
)HTML";

void handleRoot() {
  server.sendHeader("Cache-Control", "no-store");
  server.send_P(200, "text/html", DASHBOARD_PAGE);
}

void handleStatus() {
  String response;
  response.reserve(128);
  response += F("{\"uptime_ms\":");
  response += String(millis());
  response += F(",\"heart_rate_bpm\":");

  int bpm = heartRateBpm();
  if (bpm > 0) {
    response += String(bpm);
  } else {
    response += F("null");
  }

  response += F(",\"imu_jerking\":");
  response += imuIsJerking() ? F("true") : F("false");
  response += F(",\"connected_clients\":");
  response += String(WiFi.softAPgetStationNum());
  response += '}';

  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", response);
}

void handleNotFound() {
  server.send(404, "text/plain", "Not found. Try /api/status");
}
}

bool startWebServer() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/status", HTTP_GET, handleStatus);
  server.onNotFound(handleNotFound);
  server.begin();
  socketServer.begin();
  serverStarted = true;
  return serverStarted;
}

void serviceWebServer() {
  if (serverStarted) {
    server.handleClient();
    socketServer.loop();
  }
}

void publishTelemetry(
    unsigned long uptimeMs,
    int bpm,
    bool imuJerking,
    bool leftBuzzerOn,
    bool rightBuzzerOn
) {
  if (!serverStarted || socketServer.connectedClients() == 0) {
    return;
  }

  static unsigned long lastPublishedAt = 0;
  if (uptimeMs - lastPublishedAt < 100) {
    return;
  }
  lastPublishedAt = uptimeMs;

  String response;
  response.reserve(128);
  response += F("{\"uptime_ms\":");
  response += String(uptimeMs);
  response += F(",\"heart_rate_bpm\":");
  response += bpm > 0 ? String(bpm) : String("null");
  response += F(",\"imu_jerking\":");
  response += imuJerking ? F("true") : F("false");
  response += F(",\"buzzer_left\":");
  response += leftBuzzerOn ? F("true") : F("false");
  response += F(",\"buzzer_right\":");
  response += rightBuzzerOn ? F("true") : F("false");
  response += '}';

  socketServer.broadcastTXT(response);
}