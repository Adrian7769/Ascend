// ═══════════════════════════════════════════════════
// ASCEND Flight Telemetry Dashboard v3.0
// Updated for TX array v4.2 (50-byte frame)
// ═══════════════════════════════════════════════════

let telemetryData = [];
let logsData = [];
let map = null;
let flightPath = null;
let markers = [];
let charts = {};
let lastCount = 0;
let lastLogCount = 0;
let currentLogFilter = 'ALL';
let autoScrollLogs = true;
let mapInitialized = false;
let isFirstLoad = true;
const startTime = Date.now();

const REFRESH_INTERVAL = 3000;
const TELEMETRY_URL = 'data/telemetry.json';
const LOGS_URL = 'data/logs.json';

// ── Byte regions for the 50-byte hex display ──
// [0..1] HDR  [2..4] SER  [5] DATALINK  [6..21] GNSS
// [22..37] I2C  [38] RSVD  [39..46] ANALOG  [47..49] MODEM
const BYTE_REGIONS = [
    { start: 0,  end: 1,  cls: 'hdr',    label: 'HEADER' },
    { start: 2,  end: 4,  cls: 'ser',    label: 'SERIAL' },
    { start: 5,  end: 5,  cls: 'status', label: 'DATALINK' },
    { start: 6,  end: 21, cls: 'gnss',   label: 'GNSS' },
    { start: 22, end: 37, cls: 'i2c',    label: 'I2C' },
    { start: 38, end: 38, cls: 'modem',  label: 'MODEM-MSB' },
    { start: 39, end: 46, cls: 'analog', label: 'ANALOG' },
    { start: 47, end: 47, cls: 'modem',  label: 'MODEM-LSB' },
    { start: 48, end: 48, cls: 'modem',  label: 'PREV-MODEM-MSB' },
    { start: 49, end: 49, cls: 'modem',  label: 'PREV-MODEM-LSB' },
];

function getByteRegion(i) {
    for (const r of BYTE_REGIONS) {
        if (i >= r.start && i <= r.end) return r;
    }
    return { cls: 'rsv', label: '?' };
}

function hexToBytes(hexStr) {
    const clean = (hexStr || '').replace(/\s/g, '');
    const bytes = [];
    for (let i = 0; i + 1 < clean.length; i += 2) {
        bytes.push(parseInt(clean.substr(i, 2), 16));
    }
    return bytes;
}


// ── Conversion helpers ────────────────────────────────────────────────────
// Arizona is MST (UTC-7) year-round — no daylight saving time observed
function utcToArizona(hours, minutes, seconds) {
    let totalMin = hours * 60 + minutes - 7 * 60;
    // wrap negative into previous day (still valid for display)
    totalMin = ((totalMin % 1440) + 1440) % 1440;
    const h = Math.floor(totalMin / 60);
    const m = totalMin % 60;
    const ampm = h >= 12 ? 'PM' : 'AM';
    const h12 = h % 12 === 0 ? 12 : h % 12;
    return String(h12).padStart(2, '0') + ':' +
           String(m).padStart(2, '0') + ':' +
           String(seconds).padStart(2, '0') + ' ' + ampm + ' MST';
}

function cToF(c) {
    if (c == null) return null;
    return c * 9 / 5 + 32;
}

// ══════════════════════════════════════════
// INIT
// ══════════════════════════════════════════
function init() {
    initCharts();
    initLogControls();
    loadData();
    setInterval(loadData, REFRESH_INTERVAL);
    setInterval(updateUptime, 1000);
}

function updateUptime() {
    const s = Math.floor((Date.now() - startTime) / 1000);
    const h = String(Math.floor(s / 3600)).padStart(2, '0');
    const m = String(Math.floor((s % 3600) / 60)).padStart(2, '0');
    const sec = String(s % 60).padStart(2, '0');
    const el = document.getElementById('uptime');
    if (el) el.textContent = `${h}:${m}:${sec}`;
}

// ══════════════════════════════════════════
// DATA FETCH
// ══════════════════════════════════════════
async function loadData() { await loadTelemetry(); await loadLogs(); }

async function loadTelemetry() {
    try {
        const res = await fetch(TELEMETRY_URL + '?t=' + Date.now());
        if (!res.ok) throw new Error(`HTTP ${res.status}`);
        const data = await res.json();
        if (data.telemetry && data.telemetry.length > 0) {
            telemetryData = data.telemetry;
            document.getElementById('loading').style.display = 'none';
            document.getElementById('dashboard').style.display = 'block';
            if (isFirstLoad && !map) {
                initMap();
                setTimeout(() => { if (map) map.invalidateSize(); updateDashboard(data); }, 100);
            } else {
                updateDashboard(data);
            }
        }
    } catch (e) { console.error('[ERR] Load:', e); }
}

async function loadLogs() {
    try {
        const res = await fetch(LOGS_URL + '?t=' + Date.now());
        if (!res.ok) return;
        const data = await res.json();
        if (data.logs && data.logs.length > 0 && data.logs.length !== lastLogCount) {
            logsData = data.logs;
            updateLogDisplay();
            lastLogCount = data.logs.length;
        }
    } catch (e) { /* logs optional */ }
}

// ══════════════════════════════════════════
// DASHBOARD UPDATE
// ══════════════════════════════════════════
function updateDashboard(data) {
    updateStatusBar(data);
    updateDatalink();
    updatePosition();
    updateModem();
    updateRadio();
    updateSensors();
    updateHexDump();
    updateMap();
    updateCharts();
    if (isFirstLoad) isFirstLoad = false;
}

function updateStatusBar(data) {
    const t = new Date(data.lastUpdated * 1000);
    setText('lastUpdate', t.toLocaleTimeString());
    setText('totalRecords', data.totalRecords);
}

function updateDatalink() {
    if (telemetryData.length === 0) return;
    const p = telemetryData[telemetryData.length - 1].payload;
    if (!p) return;

    const good = p.btLinkGood === true;
    const strip = document.getElementById('datalinkStrip');
    const status = document.getElementById('datalinkStatus');
    if (strip) {
        strip.classList.toggle('good', good);
        strip.classList.toggle('bad', !good);
    }
    if (status) status.textContent = good ? 'GOOD' : 'BAD';

    // Radio sweep stats in the right section of the strip
    if (p.radio) {
        setText('radioSweep',  p.radio.sweepCount ?? '--');
        setText('radioPeakSig', p.radio.peakSignal  ?? '--');
    }
}

function updatePosition() {
    if (telemetryData.length === 0) return;
    const p = telemetryData[telemetryData.length - 1].payload;
    if (!p) return;

    // GNSS validity badge
    const badge = document.getElementById('gnssBadge');
    if (badge) {
        if (p.gnssValid) {
            badge.textContent = 'VALID';
            badge.className = 'gnss-badge on';
        } else {
            badge.textContent = 'NO FIX';
            badge.className = 'gnss-badge off';
        }
    }

    // Lat/Lon — always show regardless of GNSS fix quality
    setText('gpsLat', p.latitude  != null ? p.latitude.toFixed(6)  + '°' : '--');
    setText('gpsLon', p.longitude != null ? p.longitude.toFixed(6) + '°' : '--');

    // Altitude — only show when units are meters (sanity-valid data)
    const altIsValid = p.altitudeUnits === 'meters';
    if (altIsValid) {
        setText('gpsAlt',  p.altitude.toFixed(0));
        setText('altUnit', 'm');
    } else {
        setText('gpsAlt',  'NO FIX');
        setText('altUnit', ' ');  // non-breaking space keeps card height aligned
    }

    if (p.utcHours !== undefined) {
        setText('gpsUtc', utcToArizona(p.utcHours, p.utcMinutes, p.utcSeconds));
    }
}

function updateModem() {
    if (telemetryData.length === 0) return;
    const entry = telemetryData[telemetryData.length - 1];
    const p = entry.payload;
    if (!p) return;

    setText('modemImei', entry.imei || '--');

    if (p.modem) {
        setText('modemStatus', p.modem.currentCode + ' — ' + (p.modem.currentDesc || ''));
        setText('modemPrev', p.modem.previousCode + ' — ' + (p.modem.previousDesc || ''));
        // TX count removed — dashboard counts packets by MOMSN delta
    }

    const hdr = p.header || '--';
    const valid = p.headerValid ? '✓' : '✗';
    const ser = p.serialNumber || '--';
    setText('headerInfo', `"${hdr}" ${valid} / SN ${ser}`);
}

function updateRadio() {
    if (telemetryData.length === 0) return;
    const p = telemetryData[telemetryData.length - 1].payload;
    if (!p || !p.radio) return;

    setText('radioFreq',   p.radio.frequencyMHz.toFixed(1) + ' MHz');
    setText('radioSignal', p.radio.signalStrength + ' / 15');
    setText('radioMode',   p.radio.stereo ? 'STEREO' : 'MONO');
}

function updateSensors() {
    if (telemetryData.length === 0) return;
    const p = telemetryData[telemetryData.length - 1].payload;
    if (!p) return;

    // AHT20
    if (p.aht20) {
        const ahtF = cToF(p.aht20.tempC);
        setText('ahtTemp', ahtF != null ? ahtF.toFixed(1) : '--');
        setText('ahtHum', p.aht20.humidityRH != null ? p.aht20.humidityRH.toFixed(1) : '--');
        setText('ahtStatus', p.aht20.statusString || (p.aht20.isValid ? 'OK' : 'ERR'));
        setBar('ahtTempBar', ahtF, -58, 176);
        setBar('ahtHumBar', p.aht20.humidityRH, 0, 100);
        setBar('ahtStatusBar', p.aht20.isValid ? 100 : 0, 0, 100);
    }

    // Analog sensors
    const s = p.sensors;
    if (s) {
        const intF = cToF(s.internalTemp?.measurement);
        setText('intTemp', intF != null ? intF.toFixed(1) : '--');
        const extF = cToF(s.externalTemp?.measurement);
        setText('extTemp', extF != null ? extF.toFixed(1) : '--');
        setText('masterBatt', s.masterBattery?.measurement?.toFixed(2) || '--');
        setText('slaveBatt', s.slaveBattery?.measurement?.toFixed(2) || '--');

        setBar('intTempBar', intF, -76, 140);
        setBar('extTempBar', extF, -76, 140);
        setBar('masterBattBar', s.masterBattery?.measurement, 0, 5);
        setBar('slaveBattBar',  s.slaveBattery?.measurement,  0, 9);
    }

    // Altitude gauge — only when meters (units flag = sanity check)
    const altValid = p.altitudeUnits === 'meters';
    setText('altGauge',     altValid ? p.altitude.toFixed(0) : '--');
    setText('altGaugeUnit', altValid ? 'm' : ' ');  // nbsp holds line height when no fix
    setBar('altBar', altValid ? p.altitude : null, 0, 120000);
}

function updateHexDump() {
    if (telemetryData.length === 0) return;
    const entry = telemetryData[telemetryData.length - 1];
    const hexStr = entry.hexData || '';

    const meta = document.getElementById('hexMeta');
    if (meta) meta.textContent = hexStr
        ? `(MOMSN ${entry.momsn}, ${hexStr.replace(/\s/g, '').length / 2} bytes)`
        : '';

    const container = document.getElementById('hexBytes');
    if (!container) return;
    const bytes = hexToBytes(hexStr);

    if (bytes.length === 0) {
        container.textContent = '-- no data --';
        return;
    }

    container.innerHTML = bytes.map((b, i) => {
        const r = getByteRegion(i);
        const hex = b.toString(16).toUpperCase().padStart(2, '0');
        return `<span class="hb ${r.cls}" title="[${i}] 0x${hex} (${r.label})">${hex}</span>`;
    }).join('');
}

// ══════════════════════════════════════════
// MAP
// ══════════════════════════════════════════
function initMap() {
    map = L.map('map', { zoomControl: true }).setView([33.4484, -112.074], 8);
    L.tileLayer('https://{s}.basemaps.cartocdn.com/dark_all/{z}/{x}/{y}{r}.png', {
        attribution: '© OSM © CartoDB', maxZoom: 19
    }).addTo(map);
}

function updateMap() {
    if (!map || telemetryData.length === 0) return;
    if (isFirstLoad) {
        markers.forEach(m => map.removeLayer(m));
        markers = [];
        if (flightPath) map.removeLayer(flightPath);
    }

    const coords = [];
    telemetryData.forEach((e, i) => {
        if (e.payload?.gnssValid && e.payload?.latitude && e.payload?.longitude) {
            const lat = e.payload.latitude, lon = e.payload.longitude;
            const alt = e.payload.altitude || 0;
            coords.push([lat, lon]);

            const show = i === 0 || i === telemetryData.length - 1 || i % 5 === 0;
            const exists = markers.some(m => m.options.idx === i);
            if (show && !exists) {
                const isLast  = i === telemetryData.length - 1;
                const isFirst = i === 0;
                const c = isLast ? '#f87171' : (isFirst ? '#34d399' : '#38bdf8');
                const icon = L.divIcon({
                    className: '',
                    html: `<div style="background:${c};width:8px;height:8px;border-radius:50%;border:1px solid #0c0f14;box-shadow:0 0 6px ${c};"></div>`,
                    iconSize: [8, 8]
                });
                const mk = L.marker([lat, lon], { icon, idx: i }).addTo(map);
                mk.bindPopup(
                    `<b style="color:var(--accent)">MOMSN ${e.momsn}</b><br>` +
                    `ALT: ${alt} ${e.payload.altitudeUnits || ''}<br>` +
                    `LAT: ${lat.toFixed(6)}<br>LON: ${lon.toFixed(6)}<br>` +
                    `BT: ${e.payload.btLinkGood ? '<span style="color:var(--green)">GOOD</span>' : '<span style="color:var(--red)">BAD</span>'}<br>` +
                    `<span style="color:var(--text-dim)">${e.transmitTime || ''}</span>`
                );
                markers.push(mk);
            }
        }
    });

    if (coords.length > 0) {
        if (flightPath) map.removeLayer(flightPath);
        flightPath = L.polyline(coords, { color: '#38bdf8', weight: 2, opacity: .7, dashArray: '4 6' }).addTo(map);
        if (isFirstLoad || !mapInitialized) {
            map.fitBounds(flightPath.getBounds(), { padding: [20, 20] });
            mapInitialized = true;
        }
    }
}

// ══════════════════════════════════════════
// CHARTS
// ══════════════════════════════════════════
function initCharts() {
    const grid = '#1a1e27';
    const tick = '#6b7280';
    const baseOpts = {
        responsive: true, maintainAspectRatio: false,
        animation: { duration: 300 },
        plugins: { legend: { display: true, position: 'top', labels: { color: '#6b7280', font: { family: "'JetBrains Mono',monospace", size: 10 }, boxWidth: 10, padding: 8 } } },
        scales: {
            x: { display: true, grid: { color: grid }, ticks: { color: tick, font: { family: "'JetBrains Mono',monospace", size: 9 } } },
            y: { display: true, grid: { color: grid }, ticks: { color: tick, font: { family: "'JetBrains Mono',monospace", size: 9 } }, beginAtZero: false }
        }
    };

    charts.altitude = new Chart(document.getElementById('altChart').getContext('2d'), {
        type: 'line',
        data: { labels: [], datasets: [
            { label: 'ALT', data: [], borderColor: '#38bdf8', backgroundColor: 'rgba(56,189,248,.05)', tension: .3, fill: true, pointRadius: 2, borderWidth: 1.5 }
        ]},
        options: baseOpts
    });

    charts.temperature = new Chart(document.getElementById('tempChart').getContext('2d'), {
        type: 'line',
        data: { labels: [], datasets: [
            { label: 'AHT20', data: [], borderColor: '#22d3ee', tension: .3, pointRadius: 2, borderWidth: 1.5 },
            { label: 'INT',   data: [], borderColor: '#f87171', tension: .3, pointRadius: 2, borderWidth: 1.5 },
            { label: 'EXT',   data: [], borderColor: '#fbbf24', tension: .3, pointRadius: 2, borderWidth: 1.5 }
        ]},
        options: baseOpts
    });

    const dualOpts = JSON.parse(JSON.stringify(baseOpts));
    dualOpts.scales.y1 = { type: 'linear', display: true, position: 'right', grid: { drawOnChartArea: false, color: grid }, ticks: { color: tick, font: { family: "'JetBrains Mono',monospace", size: 9 } } };

    charts.battery = new Chart(document.getElementById('battChart').getContext('2d'), {
        type: 'line',
        data: { labels: [], datasets: [
            { label: 'M.BATT', data: [], borderColor: '#34d399', tension: .3, yAxisID: 'y', pointRadius: 2, borderWidth: 1.5 },
            { label: 'S.BATT', data: [], borderColor: '#a78bfa', tension: .3, yAxisID: 'y', pointRadius: 2, borderWidth: 1.5 },
            { label: 'HUM%',   data: [], borderColor: '#38bdf8', tension: .3, yAxisID: 'y1', pointRadius: 2, borderWidth: 1.5 }
        ]},
        options: dualOpts
    });
}

function updateCharts() {
    if (telemetryData.length === 0) return;
    if (telemetryData.length === lastCount && !isFirstLoad) return;

    const labels = [], altD = [], ahtT = [], intT = [], extT = [], mBatt = [], sBatt = [], humD = [];

    telemetryData.forEach(e => {
        if (!e.payload) return;
        const p = e.payload;
        labels.push(`#${e.momsn}`);
        altD.push(p.altitudeUnits === 'meters' ? (p.altitude ?? null) : null);
        ahtT.push(cToF(p.aht20?.tempC) ?? null);
        intT.push(cToF(p.sensors?.internalTemp?.measurement) ?? null);
        extT.push(cToF(p.sensors?.externalTemp?.measurement) ?? null);
        mBatt.push(p.sensors?.masterBattery?.measurement ?? null);
        sBatt.push(p.sensors?.slaveBattery?.measurement ?? null);
        humD.push(p.aht20?.humidityRH ?? null);
    });

    const mode = isFirstLoad ? 'active' : 'none';

    charts.altitude.data.labels = labels;
    charts.altitude.data.datasets[0].data = altD;
    charts.altitude.update(mode);

    charts.temperature.data.labels = labels;
    charts.temperature.data.datasets[0].data = ahtT;
    charts.temperature.data.datasets[1].data = intT;
    charts.temperature.data.datasets[2].data = extT;
    charts.temperature.update(mode);

    charts.battery.data.labels = labels;
    charts.battery.data.datasets[0].data = mBatt;
    charts.battery.data.datasets[1].data = sBatt;
    charts.battery.data.datasets[2].data = humD;
    charts.battery.update(mode);

    lastCount = telemetryData.length;
}

// ══════════════════════════════════════════
// HELPERS
// ══════════════════════════════════════════
function setText(id, val) {
    const el = document.getElementById(id);
    if (el) el.textContent = val;
}

function setFlag(id, active, colorClass) {
    const el = document.getElementById(id);
    if (!el) return;
    el.classList.toggle('on', !!active);
    el.classList.toggle(colorClass, !!active);
}

function setBar(id, value, min, max) {
    const el = document.getElementById(id);
    if (!el || value == null) return;
    el.style.width = Math.max(0, Math.min(100, ((value - min) / (max - min)) * 100)) + '%';
}

// ══════════════════════════════════════════
// LOG PANEL
// ══════════════════════════════════════════
function initLogControls() {
    document.querySelectorAll('.log-filter').forEach(f => {
        f.addEventListener('click', () => {
            document.querySelectorAll('.log-filter').forEach(b => b.classList.remove('active'));
            f.classList.add('active');
            currentLogFilter = f.dataset.level;
            updateLogDisplay();
        });
    });
    const lc = document.getElementById('logContent');
    lc.addEventListener('scroll', () => {
        autoScrollLogs = lc.scrollHeight - lc.scrollTop <= lc.clientHeight + 50;
        const ind = document.getElementById('autoScrollIndicator');
        if (ind) {
            ind.classList.toggle('paused', !autoScrollLogs);
            ind.textContent = autoScrollLogs ? 'AUTO-SCROLL ●' : 'SCROLL PAUSED ○';
        }
    });
}

function updateLogDisplay() {
    const lc = document.getElementById('logContent');
    let filtered = logsData;
    if (currentLogFilter !== 'ALL') filtered = logsData.filter(l => l.level === currentLogFilter);

    setText('logTotal', logsData.length);
    setText('logFiltered', filtered.length);

    const levelMap = { DEBUG: 'DBG', INFO: 'INF', WARNING: 'WRN', ERROR: 'ERR' };
    lc.innerHTML = filtered.map(log => {
        const time = log.timestamp ? log.timestamp.split(' ')[1] || log.timestamp : '--';
        const tag = levelMap[log.level] || log.level;
        return `<div class="log-entry ${log.level}"><span class="log-ts">${esc(time)}</span><span class="log-lvl ${log.level}">[${tag}]</span><span class="log-msg">${esc(log.message)}</span></div>`;
    }).join('') || '<div class="log-entry INFO"><span class="log-msg">no logs</span></div>';

    if (autoScrollLogs) lc.scrollTop = lc.scrollHeight;
}

function esc(t) {
    if (!t) return '';
    const m = {'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#039;'};
    return t.replace(/[&<>"']/g, c => m[c]);
}

// ══════════════════════════════════════════
window.addEventListener('DOMContentLoaded', init);