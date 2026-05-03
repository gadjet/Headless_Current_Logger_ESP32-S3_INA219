#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <Adafruit_INA219.h>
#include <time.h>

/* ==========================================
 * 1. CONFIGURATION & PINS (Xiao ESP32 S3)
 * ========================================== */
#define ENABLE_VOLTAGE false  // Set to true to log and display Bus Voltage
#define DEBUG_SERIAL true     // Set to true to enable Serial debugging

// Manually define XIAO pins if not defined by the board package
#ifndef D3
  #define D3 4
  #define D4 5
  #define D5 6
  #define D8 7
  #define D9 8
  #define D10 9
#endif

// I2C Pins for INA219
#define I2C_SDA D4
#define I2C_SCL D5

// SPI Pins for SD Card
#define SD_MOSI D10
#define SD_MISO D9
#define SD_SCK  D8
#define SD_CS   D3

const char* ssid = "SolarLogger";
const char* password = NULL; // Open network

/* --- 2. OBJECTS & GLOBALS --- */
Adafruit_INA219 ina219;
WebServer server(80);

// State & Config
uint32_t log_interval_ms = 10000;
bool isLogging = false;
bool sdAvailable = false;
bool ina219Available = false;
bool timeIsSet = false;

// Time Tracking
time_t base_epoch = 0;
uint32_t base_millis = 0;
int tz_offset_sec = 0;

// Logging State
uint32_t recordsInFile = 0;
String currentLogFileName = "";
const uint32_t MAX_RECORDS = 5000;

/* --- 3. WEB INTERFACE (HTML/JS) --- */
const char INDEX_HTML[] PROGMEM = R"=====(
<!DOCTYPE html>
<html>
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>Solar Logger</title>
    <style>
        :root {
            --bg: #0f172a;
            --card-bg: #1e293b;
            --text: #f8fafc;
            --text-dim: #94a3b8;
            --green: #22c55e;
            --red: #ef4444;
            --blue: #3b82f6;
            --border: #334155;
        }
        body { font-family: 'Segoe UI', system-ui, -apple-system, sans-serif; background: var(--bg); color: var(--text); margin: 0; padding: 20px; display: flex; flex-direction: column; align-items: center; }
        .card { background: var(--card-bg); padding: 24px; border-radius: 16px; width: 100%; max-width: 400px; box-shadow: 0 10px 25px -5px rgba(0,0,0,0.3); border: 1px solid var(--border); }
        h2 { margin: 0 0 20px 0; font-weight: 600; font-size: 1.25rem; color: var(--text-dim); }
        .val-container { margin: 20px 0; display: flex; justify-content: space-between; align-items: center; }
        .val-main { text-align: left; }
        .val { font-size: 4rem; font-weight: 700; line-height: 1; margin-bottom: 4px; transition: color 0.3s; }
        .unit { font-size: 1rem; color: var(--text-dim); text-transform: uppercase; letter-spacing: 0.1em; }
        #voltage_val { font-size: 1.25rem; color: var(--blue); margin-top: 8px; font-weight: 500; }
        
        .side-stats { display: flex; flex-direction: column; gap: 12px; text-align: right; border-left: 1px solid var(--border); padding-left: 15px; }
        .side-stat { display: flex; flex-direction: column; }
        .side-label { font-size: 0.65rem; color: var(--text-dim); text-transform: uppercase; font-weight: 600; }
        .side-value { font-size: 0.9rem; font-weight: 700; }

        #chart-container { width: 100%; height: 160px; margin: 20px 0; background: rgba(0,0,0,0.2); border-radius: 8px; overflow: hidden; position: relative; }
        canvas { width: 100%; height: 100%; }

        #status-bar { display: flex; gap: 8px; margin-bottom: 24px; width: 100%; max-width: 400px; }
        .indicator { flex: 1; padding: 6px; border-radius: 20px; background: var(--border); color: var(--text-dim); font-size: 0.7rem; font-weight: 700; text-align: center; transition: all 0.4s; border: 1px solid transparent; }
        .indicator.ok { background: rgba(34, 197, 94, 0.1); color: var(--green); border-color: rgba(34, 197, 94, 0.2); }
        .indicator.fail { background: rgba(239, 68, 68, 0.1); color: var(--red); border-color: rgba(239, 68, 68, 0.2); }

        .control-group { background: rgba(255,255,255,0.03); padding: 16px; border-radius: 12px; margin-bottom: 16px; text-align: left; }
        label { display: block; font-size: 0.8rem; color: var(--text-dim); margin-bottom: 8px; }
        select { width: 100%; padding: 10px; border-radius: 8px; background: var(--bg); color: var(--text); border: 1px solid var(--border); font-size: 1rem; }
        
        button { width: 100%; padding: 14px; font-size: 1rem; font-weight: 600; border: none; border-radius: 12px; cursor: pointer; transition: all 0.2s; margin-top: 8px; }
        .btn-start { background: var(--green); color: white; }
        .btn-start:hover { filter: brightness(1.1); }
        .btn-stop { background: var(--red); color: white; }
        .btn-stop:hover { filter: brightness(1.1); }

        .log-list { margin-top: 32px; width: 100%; max-width: 400px; text-align: left; }
        .log-header { font-size: 0.9rem; font-weight: 600; color: var(--text-dim); margin-bottom: 12px; display: flex; justify-content: space-between; }
        .log-item { background: var(--card-bg); padding: 12px 16px; border-radius: 12px; margin-bottom: 8px; border: 1px solid var(--border); display: flex; justify-content: space-between; align-items: center; animation: slideIn 0.3s ease-out; }
        .log-info { display: flex; flex-direction: column; }
        .log-name { font-size: 0.9rem; font-weight: 500; }
        .log-size { font-size: 0.75rem; color: var(--text-dim); }
        .log-actions { display: flex; gap: 16px; align-items: center; }
        .log-actions svg { width: 20px; height: 20px; cursor: pointer; fill: var(--text-dim); transition: fill 0.2s; }
        .log-actions .btn-download:hover svg { fill: var(--blue); }
        .log-actions .btn-view:hover svg { fill: var(--green); }
        .log-actions .btn-del:hover svg { fill: var(--red); }

        /* Log Viewer Modal */
        #viewer-modal { display: none; position: fixed; inset: 0; background: var(--bg); z-index: 2000; flex-direction: column; padding: 20px; overflow-y: auto; }
        .viewer-header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 20px; width: 100%; max-width: 800px; margin-inline: auto; }
        .viewer-title { font-weight: 600; color: var(--text-dim); }
        .btn-close { padding: 8px 16px; background: var(--border); border-radius: 8px; cursor: pointer; font-size: 0.9rem; color: var(--text); }
        #viewer-chart-container { width: 100%; max-width: 800px; height: 300px; background: rgba(0,0,0,0.2); border-radius: 12px; margin: 0 auto 20px auto; position: relative; }
        .viewer-stats { display: grid; grid-template-columns: repeat(auto-fit, minmax(150px, 1fr)); gap: 12px; width: 100%; max-width: 800px; margin-inline: auto; }

        /* Toast Notifications */
        #toast-container { position: fixed; top: 20px; right: 20px; left: 20px; display: flex; flex-direction: column; align-items: center; pointer-events: none; z-index: 1000; }
        .toast { background: var(--card-bg); color: var(--text); padding: 12px 24px; border-radius: 12px; box-shadow: 0 10px 15px -3px rgba(0,0,0,0.4); border: 1px solid var(--border); margin-bottom: 10px; animation: toastIn 0.3s ease-out, toastOut 0.3s ease-in 2.7s forwards; font-size: 0.9rem; pointer-events: auto; }
        
        @keyframes slideIn { from { opacity: 0; transform: translateY(10px); } to { opacity: 1; transform: translateY(0); } }
        @keyframes toastIn { from { opacity: 0; transform: translateY(-20px); } to { opacity: 1; transform: translateY(0); } }
        @keyframes toastOut { to { opacity: 0; transform: translateY(-20px); } }
    </style>
</head>
<body>
    <div id="toast-container"></div>
    
    <div id="viewer-modal">
        <div class="viewer-header">
            <div class="viewer-title" id="viewer-filename">Log View</div>
            <div class="btn-close" onclick="closeViewer()">Close</div>
        </div>
        <div id="viewer-chart-container">
            <canvas id="viewerChart"></canvas>
        </div>
        <div class="viewer-stats" id="viewer-stats"></div>
    </div>

    <div id="status-bar">
        <div id="stat-sensor" class="indicator">SENSOR</div>
        <div id="stat-sd" class="indicator">SD CARD</div>
        <div id="stat-log" class="indicator">LOGGING</div>
    </div>

    <div class="card">
        <h2>Live Monitoring</h2>
        <div class="val-container">
            <div class="val-main">
                <div id="current_val" class="val">0</div>
                <div class="unit">mA Current</div>
                <div id="voltage_val">0.00 V</div>
            </div>
            <div class="side-stats">
                <div class="side-stat">
                    <span class="side-label">Peak Charge</span>
                    <span id="peak-charge" class="side-value" style="color:var(--green)">0 mA</span>
                </div>
                <div class="side-stat">
                    <span class="side-label">Peak Draw</span>
                    <span id="peak-draw" class="side-value" style="color:var(--red)">0 mA</span>
                </div>
                <div class="side-stat">
                    <span class="side-label">Energy</span>
                    <span id="session-ah" class="side-value" style="color:var(--blue)">0 mAh</span>
                </div>
            </div>
        </div>

        <div id="chart-container">
            <canvas id="liveChart"></canvas>
        </div>

        <div class="control-group">
            <label>Logging Interval</label>
            <select id="interval" onchange="updateConfig()">
                <option value="1000">Every 1 Second</option>
                <option value="5000">Every 5 Seconds</option>
                <option value="10000" selected>Every 10 Seconds</option>
                <option value="30000">Every 30 Seconds</option>
                <option value="60000">Every 1 Minute</option>
            </select>
        </div>

        <button id="logBtn" class="btn-start" onclick="toggleLogging()">START LOGGING</button>
    </div>

    <div class="log-list" id="log-list">
        <!-- Files populated here -->
    </div>

    <script>
        let dataHistory = [];
        const maxPoints = 60;
        let peakCharge = 0;
        let peakDraw = 0;
        let total_mAh = 0;
        let lastUpdateTime = Date.now();

        function showToast(msg) {
            const container = document.getElementById('toast-container');
            const toast = document.createElement('div');
            toast.className = 'toast';
            toast.innerText = msg;
            container.appendChild(toast);
            setTimeout(() => toast.remove(), 3000);
        }

        function syncTime() {
            const now = Math.floor(Date.now() / 1000);
            const tz = -new Date().getTimezoneOffset() * 60;
            fetch(`/sync?t=${now}&tz=${tz}`).then(() => {
                showToast("Time Synchronized");
                refreshLogs();
            });
        }

        function drawChart() {
            const canvas = document.getElementById('liveChart');
            const ctx = canvas.getContext('2d');
            const w = canvas.width = canvas.offsetWidth * window.devicePixelRatio;
            const h = canvas.height = canvas.offsetHeight * window.devicePixelRatio;
            ctx.scale(window.devicePixelRatio, window.devicePixelRatio);
            const sw = canvas.offsetWidth;
            const sh = canvas.offsetHeight;
            const chartLeft = 55;

            ctx.clearRect(0, 0, sw, sh);
            
            let maxAbs = Math.max(...dataHistory.map(Math.abs), 100) * 1.2;
            
            // Grid & Labels
            const niceSteps = [10, 20, 50, 100, 200, 500, 1000, 2000, 5000];
            let step = niceSteps.find(s => (maxAbs / s) <= 8) || 1000;
            
            ctx.font = '10px sans-serif';
            ctx.textAlign = 'right';
            ctx.fillStyle = 'rgba(255,255,255,0.3)';
            ctx.strokeStyle = 'rgba(255,255,255,0.05)';
            ctx.lineWidth = 1;

            for (let v = -Math.floor(maxAbs/step)*step; v <= maxAbs; v += step) {
                let y = (sh / 2) - (v / maxAbs) * (sh / 2);
                if (y < 10 || y > sh - 10) continue;
                ctx.beginPath(); ctx.moveTo(chartLeft, y); ctx.lineTo(sw, y); ctx.stroke();
                ctx.fillText((v > 0 ? '+' : '') + v, chartLeft - 5, y + 3);
            }

            // Zero Line
            ctx.strokeStyle = 'rgba(255,255,255,0.2)';
            ctx.setLineDash([5, 5]);
            ctx.beginPath();
            ctx.moveTo(chartLeft, sh/2);
            ctx.lineTo(sw, sh/2);
            ctx.stroke();
            ctx.setLineDash([]);

            if (dataHistory.length < 2) return;

            // Draw Gradient Area
            const grad = ctx.createLinearGradient(0, 0, 0, sh);
            grad.addColorStop(0, 'rgba(34, 197, 94, 0.2)');
            grad.addColorStop(0.5, 'rgba(34, 197, 94, 0)');
            grad.addColorStop(0.5, 'rgba(239, 68, 68, 0)');
            grad.addColorStop(1, 'rgba(239, 68, 68, 0.2)');

            ctx.fillStyle = grad;
            ctx.beginPath();
            ctx.moveTo(chartLeft, sh/2);
            dataHistory.forEach((v, i) => {
                const x = chartLeft + (i / (maxPoints - 1)) * (sw - chartLeft);
                const y = (sh / 2) - (v / maxAbs) * (sh / 2);
                ctx.lineTo(x, y);
            });
            ctx.lineTo(sw, sh/2);
            ctx.closePath();
            ctx.fill();

            // Draw Line
            ctx.beginPath();
            ctx.lineWidth = 2;
            dataHistory.forEach((v, i) => {
                const x = chartLeft + (i / (maxPoints - 1)) * (sw - chartLeft);
                const y = (sh / 2) - (v / maxAbs) * (sh / 2);
                if (i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y);
            });
            ctx.strokeStyle = '#3b82f6';
            ctx.stroke();
        }

        function updateData() {
            fetch('/status').then(r => r.json()).then(data => {
                if (data.current !== undefined) {
                    const current = Math.round(data.current);
                    document.getElementById('current_val').innerText = current;
                    document.getElementById('current_val').style.color = current >= 0 ? 'var(--green)' : 'var(--red)';
                    
                    // Stats
                    if (current > peakCharge) peakCharge = current;
                    if (current < peakDraw) peakDraw = current;
                    
                    const now = Date.now();
                    const dt = (now - lastUpdateTime) / 1000 / 3600; // hours
                    total_mAh += Math.abs(current) * dt;
                    lastUpdateTime = now;

                    document.getElementById('peak-charge').innerText = peakCharge + " mA";
                    document.getElementById('peak-draw').innerText = peakDraw + " mA";
                    document.getElementById('session-ah').innerText = Math.round(total_mAh) + " mAh";

                    dataHistory.push(current);
                    if (dataHistory.length > maxPoints) dataHistory.shift();
                    drawChart();
                }
                
                document.getElementById('stat-sensor').className = data.ina ? "indicator ok" : "indicator fail";
                document.getElementById('stat-sd').className = data.sd ? "indicator ok" : "indicator fail";
                document.getElementById('stat-log').className = data.logging ? "indicator ok" : "indicator";

                if (data.voltage !== null) {
                    document.getElementById('voltage_val').innerText = data.voltage.toFixed(2) + " V";
                    document.getElementById('voltage_val').style.display = "block";
                }

                const btn = document.getElementById('logBtn');
                if (data.logging) {
                    btn.innerText = "STOP LOGGING (" + data.records + ")";
                    btn.className = "btn-stop";
                } else {
                    btn.innerText = "START LOGGING";
                    btn.className = "btn-start";
                }
            }).catch(() => {});
        }

        function refreshLogs() {
            fetch('/list').then(r => r.json()).then(files => {
                files.sort((a, b) => b.name.localeCompare(a.name));
                const list = document.getElementById('log-list');
                list.innerHTML = '<div class="log-header"><span>Stored Logs</span><span>' + files.length + ' files</span></div>';
                files.forEach(f => {
                    const item = document.createElement('div');
                    item.className = 'log-item';
                    item.innerHTML = `
                        <div class="log-info">
                            <span class="log-name">${f.name}</span>
                            <span class="log-size">${(f.size/1024).toFixed(1)} KB</span>
                        </div>
                        <div class="log-actions">
                            <div class="btn-view" onclick="viewLog('${f.name}')" title="View Chart">
                                <svg viewBox="0 0 24 24"><path d="M12 4.5C7 4.5 2.73 7.61 1 12c1.73 4.39 6 7.5 11 7.5s9.27-3.11 11-7.5c-1.73-4.39-6-7.5-11-7.5zM12 17c-2.76 0-5-2.24-5-5s2.24-5 5-5 5 2.24 5 5-2.24 5-5 5zm0-8c-1.66 0-3 1.34-3 3s1.34 3 3 3 3-1.34 3-3-1.34-3-3-3z"/></svg>
                            </div>
                            <a href="/download?file=${f.name}" class="btn-download" title="Download CSV">
                                <svg viewBox="0 0 24 24"><path d="M19 9h-4V3H9v6H5l7 7 7-7zM5 18v2h14v-2H5z"/></svg>
                            </a>
                            <div class="btn-del" onclick="deleteFile('${f.name}')" title="Delete File">
                                <svg viewBox="0 0 24 24"><path d="M6 19c0 1.1.9 2 2 2h8c1.1 0 2-.9 2-2V7H6v12zM19 4h-3.5l-1-1h-5l-1 1H5v2h14V4z"/></svg>
                            </div>
                        </div>
                    `;
                    list.appendChild(item);
                });
            });
        }

        function viewLog(name) {
            document.getElementById('viewer-filename').innerText = name;
            document.getElementById('viewer-modal').style.display = 'flex';
            document.getElementById('viewer-stats').innerHTML = "Loading data...";
            
            fetch(`/download?file=${name}`).then(r => r.text()).then(csv => {
                const lines = csv.split('\n').filter(l => l.trim() && !l.startsWith('Timestamp'));
                const values = lines.map(l => parseFloat(l.split(',').pop()));
                
                if (values.length === 0) {
                    document.getElementById('viewer-stats').innerHTML = "No data in file";
                    return;
                }

                const max = Math.max(...values);
                const min = Math.min(...values);
                const avg = values.reduce((a, b) => a + b, 0) / values.length;

                document.getElementById('viewer-stats').innerHTML = `
                    <div class="stat-card"><div class="stat-label">Max Current</div><div class="stat-value" style="color:var(--green)">${max.toFixed(0)} mA</div></div>
                    <div class="stat-card"><div class="stat-label">Min Current</div><div class="stat-value" style="color:var(--red)">${min.toFixed(0)} mA</div></div>
                    <div class="stat-card"><div class="stat-label">Average</div><div class="stat-value" style="color:var(--blue)">${avg.toFixed(0)} mA</div></div>
                    <div class="stat-card"><div class="stat-label">Samples</div><div class="stat-value">${values.length}</div></div>
                `;

                const canvas = document.getElementById('viewerChart');
                const ctx = canvas.getContext('2d');
                const sw = canvas.width = canvas.offsetWidth * window.devicePixelRatio;
                const sh = canvas.height = canvas.offsetHeight * window.devicePixelRatio;
                ctx.scale(window.devicePixelRatio, window.devicePixelRatio);
                const w = canvas.offsetWidth;
                const h = canvas.offsetHeight;
                const chartLeft = 55;

                ctx.clearRect(0, 0, w, h);
                let maxAbs = Math.max(Math.abs(max), Math.abs(min), 100) * 1.2;

                // Grid & Labels
                const niceSteps = [10, 20, 50, 100, 200, 500, 1000, 2000, 5000];
                let step = niceSteps.find(s => (maxAbs / s) <= 8) || 1000;
                
                ctx.font = '10px sans-serif';
                ctx.textAlign = 'right';
                ctx.fillStyle = 'rgba(255,255,255,0.3)';
                ctx.strokeStyle = 'rgba(255,255,255,0.05)';
                ctx.lineWidth = 1;

                for (let v = -Math.floor(maxAbs/step)*step; v <= maxAbs; v += step) {
                    let y = (h / 2) - (v / maxAbs) * (h / 2);
                    if (y < 10 || y > h - 10) continue;
                    ctx.beginPath(); ctx.moveTo(chartLeft, y); ctx.lineTo(w, y); ctx.stroke();
                    ctx.fillText((v > 0 ? '+' : '') + v, chartLeft - 5, y + 3);
                }

                // Zero Line
                ctx.strokeStyle = 'rgba(255,255,255,0.2)';
                ctx.setLineDash([5, 5]);
                ctx.beginPath(); ctx.moveTo(chartLeft, h/2); ctx.lineTo(w, h/2); ctx.stroke();
                ctx.setLineDash([]);

                // Plot
                ctx.beginPath();
                ctx.lineWidth = 1.5;
                ctx.strokeStyle = '#3b82f6';
                values.forEach((v, i) => {
                    const x = chartLeft + (i / (values.length - 1)) * (w - chartLeft);
                    const y = (h / 2) - (v / maxAbs) * (h / 2);
                    if (i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y);
                });
                ctx.stroke();
            });
        }

        function closeViewer() {
            document.getElementById('viewer-modal').style.display = 'none';
        }

        function deleteFile(name) {
            if (confirm(`Delete ${name}?`)) {
                fetch(`/delete?file=${name}`).then(() => {
                    showToast("File Deleted");
                    refreshLogs();
                });
            }
        }

        function toggleLogging() {
            fetch('/toggle').then(() => {
                updateData();
                setTimeout(refreshLogs, 500);
            });
        }

        function updateConfig() {
            const intv = document.getElementById('interval').value;
            fetch(`/config?interval=${intv}`).then(() => showToast("Interval Updated"));
        }

        window.onload = () => {
            syncTime();
            setInterval(updateData, 1000);
        };
    </script>
</body>
</html>
)=====";

/* --- 4. TIME & LOGGING HELPERS --- */

time_t get_now() {
    if (!timeIsSet) return 0;
    return base_epoch + ((millis() - base_millis) / 1000) + tz_offset_sec;
}

String getTimestamp() {
    time_t now = get_now();
    struct tm timeinfo;
    gmtime_r(&now, &timeinfo);
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
    return String(buf);
}

String getFilenameTimestamp() {
    time_t now = get_now();
    struct tm timeinfo;
    gmtime_r(&now, &timeinfo);
    char buf[32];
    strftime(buf, sizeof(buf), "%Y%m%d_%H%M%S", &timeinfo);
    return String(buf);
}

void logToSD(float current, float voltage) {
    if (!sdAvailable || !isLogging || !timeIsSet) return;

    if (currentLogFileName == "" || recordsInFile >= MAX_RECORDS) {
        currentLogFileName = "/log_" + getFilenameTimestamp() + ".csv";
        recordsInFile = 0;
        File file = SD.open(currentLogFileName, FILE_WRITE);
        if (file) {
            if (ENABLE_VOLTAGE) {
                file.println("Timestamp,Voltage_V,Current_mA");
            } else {
                file.println("Timestamp,Current_mA");
            }
            file.close();
        }
    }

    File file = SD.open(currentLogFileName, FILE_APPEND);
    if (file) {
        if (ENABLE_VOLTAGE) {
            file.printf("%s,%.2f,%.2f\n", getTimestamp().c_str(), voltage, current);
        } else {
            file.printf("%s,%.2f\n", getTimestamp().c_str(), current);
        }
        file.close();
        recordsInFile++;
    }
}

/* --- 5. WEB SERVER HANDLERS --- */

void handleRoot() {
    server.send_P(200, "text/html", INDEX_HTML);
}

void handleStatus() {
    String json = "{";
    json += "\"current\":" + String(ina219.getCurrent_mA());
    if (ENABLE_VOLTAGE) {
        json += ",\"voltage\":" + String(ina219.getBusVoltage_V() + (ina219.getShuntVoltage_mV() / 1000.0));
    } else {
        json += ",\"voltage\":null";
    }
    json += ",\"logging\":" + String(isLogging ? "true" : "false");
    json += ",\"sd\":" + String(sdAvailable ? "true" : "false");
    json += ",\"ina\":" + String(ina219Available ? "true" : "false");
    json += ",\"records\":" + String(recordsInFile);
    json += "}";
    server.send(200, "application/json", json);
}

void handleSync() {
    if (server.hasArg("t")) {
        base_epoch = server.arg("t").toInt();
        base_millis = millis();
        tz_offset_sec = server.hasArg("tz") ? server.arg("tz").toInt() : 0;
        timeIsSet = true;
        server.send(200, "text/plain", "OK");
    } else {
        server.send(400, "text/plain", "Missing Time");
    }
}

void handleList() {
    if (!sdAvailable) {
        server.send(500, "text/plain", "SD Not Found");
        return;
    }
    String json = "[";
    File root = SD.open("/");
    File file = root.openNextFile();
    bool first = true;
    while (file) {
        if (!file.isDirectory() && String(file.name()).endsWith(".csv")) {
            if (!first) json += ",";
            json += "{\"name\":\"" + String(file.name()) + "\",\"size\":" + String(file.size()) + "}";
            first = false;
        }
        file = root.openNextFile();
    }
    json += "]";
    server.send(200, "application/json", json);
}

void handleDownload() {
    if (!server.hasArg("file")) {
        server.send(400, "text/plain", "Missing Filename");
        return;
    }
    String path = "/" + server.arg("file");
    if (SD.exists(path)) {
        File file = SD.open(path, FILE_READ);
        server.streamFile(file, "text/csv");
        file.close();
    } else {
        server.send(404, "text/plain", "File Not Found");
    }
}

void handleDelete() {
    if (!server.hasArg("file")) {
        server.send(400, "text/plain", "Missing Filename");
        return;
    }
    String path = "/" + server.arg("file");
    if (SD.remove(path)) {
        server.send(200, "text/plain", "Deleted");
    } else {
        server.send(500, "text/plain", "Delete Failed");
    }
}

void handleToggle() {
    isLogging = !isLogging;
    if (!isLogging) currentLogFileName = "";
    server.send(200, "text/plain", "OK");
}

void handleConfig() {
    if (server.hasArg("interval")) {
        log_interval_ms = server.arg("interval").toInt();
        server.send(200, "text/plain", "OK");
    }
}

/* --- 6. SETUP & LOOP --- */

void setup() {
    if (DEBUG_SERIAL) {
        Serial.begin(115200);
        delay(1000);
    }

    // Hardware Init
    Wire.begin(I2C_SDA, I2C_SCL);
    if (!ina219.begin()) {
        if (DEBUG_SERIAL) Serial.println("INA219 Fail");
        ina219Available = false;
    } else {
        ina219.setCalibration_32V_2A();
        ina219Available = true;
    }

    SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
    if (!SD.begin(SD_CS)) {
        if (DEBUG_SERIAL) Serial.println("SD Fail");
        sdAvailable = false;
    } else {
        sdAvailable = true;
    }

    // WiFi Access Point
    WiFi.softAP(ssid, password);
    if (DEBUG_SERIAL) {
        Serial.println("AP Started: " + String(ssid));
        Serial.print("IP Address: ");
        Serial.println(WiFi.softAPIP());
    }

    // Web Server Routes
    server.on("/", handleRoot);
    server.on("/status", handleStatus);
    server.on("/sync", handleSync);
    server.on("/list", handleList);
    server.on("/download", handleDownload);
    server.on("/delete", handleDelete);
    server.on("/toggle", handleToggle);
    server.on("/config", handleConfig);
    server.begin();
}

void loop() {
    server.handleClient();

    static uint32_t last_log = 0;
    if (isLogging && (millis() - last_log >= log_interval_ms)) {
        last_log = millis();
        float current = ina219.getCurrent_mA();
        float voltage = ina219.getBusVoltage_V() + (ina219.getShuntVoltage_mV() / 1000.0);
        logToSD(current, voltage);
    }
    
    delay(10);
}
