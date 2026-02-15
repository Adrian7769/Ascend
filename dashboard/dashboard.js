// ═══════════════════════════════════════════════════════════
// GCC ASCEND TELEMETRY TERMINAL v2.0
// ═══════════════════════════════════════════════════════════

let telemetryData = [];
let logsData = [];
let map = null;
let flightPath = null;
let markers = [];
let charts = {};
let currentLogFilter = 'ALL';
let autoScrollLogs = true;
let lastTelemetryCount = 0;
let lastLogCount = 0;
let mapInitialized = false;
let isFirstLoad = true;
const startTime = Date.now();


let historyIndex = -1; // -1 = no selection, 0 = oldest
let activeHexTab = 'latest';


const REFRESH_INTERVAL = 3000;
const TELEMETRY_URL = 'data/telemetry.json';
const LOGS_URL = 'data/logs.json';

const BYTE_REGIONS = [
    { start: 0,  end: 1,  cls: 'hdr', label: 'HEADER' },
    { start: 2,  end: 4,  cls: 'ser', label: 'SERIAL' },
    { start: 5,  end: 7,  cls: 'utc', label: 'UTC' },
    { start: 8,  end: 15, cls: 'gps', label: 'GPS' },
    { start: 16, end: 20, cls: 'alt', label: 'ALT' },
    { start: 21, end: 36, cls: 'adc', label: 'ADC' },
    { start: 37, end: 47, cls: 'rsv', label: 'RSVD' },
    { start: 48, end: 49, cls: 'mdm', label: 'MODEM' },
];

function getByteRegion(index) {
    for (const r of BYTE_REGIONS) {
        if (index >= r.start && index <= r.end) return r;
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

function renderHexRows(targetId, bytes) {
    const el = document.getElementById(targetId);
    if (!el) return;
    if (bytes.length === 0) { el.textContent = '-- no data --'; return; }

    let html = '';
    for (let row = 0; row < bytes.length; row += 16) {
        const offset = row.toString(16).toUpperCase().padStart(6, '0');
        let byteStr = '';
        let ascii = '';
        for (let col = 0; col < 16; col++) {
            const idx = row + col;
            if (idx < bytes.length) {
                const b = bytes[idx];
                const region = getByteRegion(idx);
                byteStr += `<span class="hex-byte-${region.cls}">${b.toString(16).toUpperCase().padStart(2, '0')}</span> `;
                ascii += (b >= 32 && b <= 126) ? String.fromCharCode(b) : '.';
            } else {
                byteStr += '   ';
                ascii += ' ';
            }
        }
        html += `<div class="hex-row"><span class="offset">${offset}</span> <span class="bytes">${byteStr}</span><span class="ascii-col">${escapeHtml(ascii)}</span></div>`;
    }
    el.innerHTML = html;
}

function renderByteMap(targetId, bytes) {
    const el = document.getElementById(targetId);
    if (!el) return;
    if (bytes.length === 0) { el.innerHTML = ''; return; }
    el.innerHTML = bytes.map((b, i) => {
        const region = getByteRegion(i);
        const hex = b.toString(16).toUpperCase().padStart(2, '0');
        return `<div class="byte-cell ${region.cls}" title="[${i}] 0x${hex} (${region.label})">${hex}</div>`;
    }).join('');
}

function formatHexString(hexStr) {
    const clean = (hexStr || '').replace(/\s/g, '');
    return clean.match(/.{1,2}/g)?.join(' ').toUpperCase() || '';
}

// ══════════════════════════════════════════════════════════
// INIT
// ══════════════════════════════════════════════════════════
function init() {
    console.log('[INIT] Telemetry terminal starting...');
    initCharts();
    initLogControls();
    initHexTabs();
    initHistoryNav();
    loadData();
    setInterval(loadData, REFRESH_INTERVAL);
    setInterval(updateUptime, 1000);
}

function updateUptime() {
    const elapsed = Math.floor((Date.now() - startTime) / 1000);
    const h = String(Math.floor(elapsed / 3600)).padStart(2, '0');
    const m = String(Math.floor((elapsed % 3600) / 60)).padStart(2, '0');
    const s = String(elapsed % 60).padStart(2, '0');
    const el = document.getElementById('uptime');
    if (el) el.textContent = `${h}:${m}:${s}`;
}

// ── Hex Tab Switching ──────────────────────────────────────
function initHexTabs() {
    document.querySelectorAll('.hex-tab').forEach(tab => {
        tab.addEventListener('click', () => {
            document.querySelectorAll('.hex-tab').forEach(t => t.classList.remove('active'));
            tab.classList.add('active');
            activeHexTab = tab.dataset.tab;

            document.getElementById('tabLatest').style.display = activeHexTab === 'latest' ? '' : 'none';
            document.getElementById('tabHistory').style.display = activeHexTab === 'history' ? '' : 'none';

            if (activeHexTab === 'history') {
                if (telemetryData.length > 0 && historyIndex < 0) {
                    historyIndex = telemetryData.length - 1;
                }
                renderHistoryView();
            }
        });
    });
}

function initHistoryNav() {
    document.getElementById('histFirst').addEventListener('click', () => { historyIndex = 0; renderHistoryView(); });
    document.getElementById('histPrev').addEventListener('click', () => { if (historyIndex > 0) { historyIndex--; renderHistoryView(); } });
    document.getElementById('histNext').addEventListener('click', () => { if (historyIndex < telemetryData.length - 1) { historyIndex++; renderHistoryView(); } });
    document.getElementById('histLast').addEventListener('click', () => { historyIndex = telemetryData.length - 1; renderHistoryView(); });

    document.addEventListener('keydown', (e) => {
        if (activeHexTab !== 'history' || telemetryData.length === 0) return;
        if (e.key === 'ArrowLeft' || e.key === 'ArrowUp') {
            e.preventDefault();
            if (historyIndex > 0) { historyIndex--; renderHistoryView(); }
        } else if (e.key === 'ArrowRight' || e.key === 'ArrowDown') {
            e.preventDefault();
            if (historyIndex < telemetryData.length - 1) { historyIndex++; renderHistoryView(); }
        } else if (e.key === 'Home') {
            e.preventDefault();
            historyIndex = 0; renderHistoryView();
        } else if (e.key === 'End') {
            e.preventDefault();
            historyIndex = telemetryData.length - 1; renderHistoryView();
        }
    });
}

function renderHistoryView() {
    if (telemetryData.length === 0 || historyIndex < 0) return;

    const record = telemetryData[historyIndex];
    const total = telemetryData.length;

    document.getElementById('histMomsn').textContent = record.momsn ?? '--';
    document.getElementById('histIndex').textContent = `[ ${historyIndex + 1} / ${total} ]`;
    document.getElementById('histFirst').disabled = (historyIndex === 0);
    document.getElementById('histPrev').disabled = (historyIndex === 0);
    document.getElementById('histNext').disabled = (historyIndex >= total - 1);
    document.getElementById('histLast').disabled = (historyIndex >= total - 1);

    const metaEl = document.getElementById('histMeta');
    const modemCode = record.payload?.modemStatus ?? '--';
    const modemDesc = record.payload?.modemStatusDescription ?? '';
    metaEl.innerHTML = `
        <span class="hist-meta-item"><span class="hist-meta-key">TX:</span><span class="hist-meta-val">${escapeHtml(record.transmitTime || '--')}</span></span>
        <span class="hist-meta-item"><span class="hist-meta-key">IMEI:</span><span class="hist-meta-val">${escapeHtml(record.imei || '--')}</span></span>
        <span class="hist-meta-item"><span class="hist-meta-key">MODEM:</span><span class="hist-meta-val">${modemCode} ${escapeHtml(modemDesc)}</span></span>
        <span class="hist-meta-item"><span class="hist-meta-key">SESSION:</span><span class="hist-meta-val">${record.sessionStatus ?? '--'}</span></span>
    `;

    const hexStr = record.hexData || '';
    document.getElementById('histRawHex').textContent = hexStr ? formatHexString(hexStr) : '-- no hex data --';

    const bytes = hexToBytes(hexStr);
    renderHexRows('histHexRows', bytes);
    renderByteMap('histByteMap', bytes);

    renderDiff(historyIndex, bytes);
}

function renderDiff(currentIdx, currentBytes) {
    const diffEl = document.getElementById('histDiff');
    if (currentIdx <= 0 || telemetryData.length < 2) {
        diffEl.innerHTML = '<span class="diff-same">-- first record, no previous frame to compare --</span>';
        return;
    }

    const prevRecord = telemetryData[currentIdx - 1];
    const prevBytes = hexToBytes(prevRecord.hexData || '');

    if (prevBytes.length === 0) {
        diffEl.innerHTML = '<span class="diff-same">-- previous frame has no hex data --</span>';
        return;
    }

    const maxLen = Math.max(currentBytes.length, prevBytes.length);
    let changedCount = 0;
    let diffHtml = '';

    for (let i = 0; i < maxLen; i++) {
        const cur = i < currentBytes.length ? currentBytes[i] : undefined;
        const prev = i < prevBytes.length ? prevBytes[i] : undefined;
        const curHex = cur !== undefined ? cur.toString(16).toUpperCase().padStart(2, '0') : '--';
        const prevHex = prev !== undefined ? prev.toString(16).toUpperCase().padStart(2, '0') : '--';
        const region = getByteRegion(i);

        if (cur !== prev) {
            changedCount++;
            diffHtml += `<span class="diff-changed" title="[${i}] ${region.label}: ${prevHex}→${curHex}">${curHex}</span> `;
        } else {
            diffHtml += `<span class="diff-same">${curHex}</span> `;
        }
    }

    diffHtml += `<div class="diff-summary">${changedCount} of ${maxLen} bytes changed vs MOMSN ${prevRecord.momsn ?? '?'}</div>`;
    diffEl.innerHTML = diffHtml;
}

function initMap() {
    map = L.map('map', { zoomControl: true, attributionControl: true }).setView([33.4484, -112.0740], 8);
    L.tileLayer('https://{s}.basemaps.cartocdn.com/dark_all/{z}/{x}/{y}{r}.png', {
        attribution: '&copy; OSM &copy; CartoDB', maxZoom: 19
    }).addTo(map);
}

function initCharts() {
    const gridColor = '#0a3a0a';
    const tickColor = '#147014';
    const baseOpts = {
        responsive: true, maintainAspectRatio: false,
        animation: { duration: 300 },
        plugins: { legend: { display: true, position: 'top', labels: { color: '#1a8c1a', font: { family: "'IBM Plex Mono', monospace", size: 10 }, boxWidth: 10, padding: 8 } } },
        scales: {
            x: { display: true, grid: { color: gridColor, drawBorder: false }, ticks: { color: tickColor, font: { family: "'IBM Plex Mono', monospace", size: 9 } }, title: { display: true, text: 'MOMSN', color: '#1a8c1a', font: { family: "'IBM Plex Mono', monospace", size: 9 } } },
            y: { display: true, grid: { color: gridColor, drawBorder: false }, ticks: { color: tickColor, font: { family: "'IBM Plex Mono', monospace", size: 9 } }, beginAtZero: false }
        }
    };

    charts.altitude = new Chart(document.getElementById('altitudeChart').getContext('2d'), {
        type: 'line', data: { labels: [], datasets: [{ label: 'ALT (ft)', data: [], borderColor: '#00ffcc', backgroundColor: 'rgba(0,255,204,0.05)', tension: 0.3, fill: true, pointRadius: 2, pointHoverRadius: 4, borderWidth: 1.5 }] }, options: baseOpts
    });

    charts.temperature = new Chart(document.getElementById('temperatureChart').getContext('2d'), {
        type: 'line', data: { labels: [], datasets: [
            { label: 'INT (°C)', data: [], borderColor: '#ff5555', backgroundColor: 'rgba(255,85,85,0.05)', tension: 0.3, pointRadius: 2, borderWidth: 1.5 },
            { label: 'EXT (°C)', data: [], borderColor: '#ffaa00', backgroundColor: 'rgba(255,170,0,0.05)', tension: 0.3, pointRadius: 2, borderWidth: 1.5 }
        ] }, options: baseOpts
    });

    const dualOpts = JSON.parse(JSON.stringify(baseOpts));
    dualOpts.scales.y1 = { type: 'linear', display: true, position: 'right', grid: { drawOnChartArea: false, color: gridColor }, ticks: { color: tickColor, font: { family: "'IBM Plex Mono', monospace", size: 9 } } };

    charts.battery = new Chart(document.getElementById('batteryChart').getContext('2d'), {
        type: 'line', data: { labels: [], datasets: [
            { label: 'BATT (V)', data: [], borderColor: '#33ff33', backgroundColor: 'rgba(51,255,51,0.05)', tension: 0.3, yAxisID: 'y', pointRadius: 2, borderWidth: 1.5 },
            { label: 'PSI', data: [], borderColor: '#cc66ff', backgroundColor: 'rgba(204,102,255,0.05)', tension: 0.3, yAxisID: 'y1', pointRadius: 2, borderWidth: 1.5 }
        ] }, options: dualOpts
    });
}

function initLogControls() {
    document.querySelectorAll('.log-filter').forEach(filter => {
        filter.addEventListener('click', () => {
            document.querySelectorAll('.log-filter').forEach(f => f.classList.remove('active'));
            filter.classList.add('active');
            currentLogFilter = filter.dataset.level;
            updateLogDisplay();
        });
    });
    const logContent = document.getElementById('logContent');
    logContent.addEventListener('scroll', () => {
        autoScrollLogs = logContent.scrollHeight - logContent.scrollTop <= logContent.clientHeight + 50;
        const indicator = document.getElementById('autoScrollIndicator');
        if (indicator) {
            indicator.classList.toggle('paused', !autoScrollLogs);
            indicator.textContent = autoScrollLogs ? 'AUTO-SCROLL ●' : 'SCROLL PAUSED ○';
        }
    });
}

async function loadData() { await loadTelemetry(); await loadLogs(); }

async function loadTelemetry() {
    try {
        const response = await fetch(TELEMETRY_URL + '?t=' + Date.now());
        if (!response.ok) throw new Error(`HTTP ${response.status}`);
        const data = await response.json();
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
    } catch (error) { console.error('[ERR] Telemetry load:', error); }
}

async function loadLogs() {
    try {
        const response = await fetch(LOGS_URL + '?t=' + Date.now());
        if (!response.ok) return;
        const data = await response.json();
        if (data.logs && data.logs.length > 0 && data.logs.length !== lastLogCount) {
            logsData = data.logs;
            updateLogDisplay();
            lastLogCount = data.logs.length;
        }
    } catch (e) { /* logs optional */ }
}

function updateLogDisplay() {
    const logContent = document.getElementById('logContent');
    let filtered = logsData;
    if (currentLogFilter !== 'ALL') filtered = logsData.filter(l => l.level === currentLogFilter);

    document.getElementById('logTotal').textContent = logsData.length;
    document.getElementById('logFiltered').textContent = filtered.length;

    const levelMap = { DEBUG: 'DBG', INFO: 'INF', WARNING: 'WRN', ERROR: 'ERR' };
    logContent.innerHTML = filtered.map(log => {
        const time = log.timestamp ? log.timestamp.split(' ')[1] || log.timestamp : '--';
        const tag = levelMap[log.level] || log.level;
        return `<div class="log-entry ${log.level}"><span class="log-timestamp">${escapeHtml(time)}</span><span class="log-level ${log.level}">[${tag}]</span><span class="log-message">${escapeHtml(log.message)}</span></div>`;
    }).join('') || '<div class="log-entry INFO"><span class="log-message">no logs</span></div>';

    if (autoScrollLogs) logContent.scrollTop = logContent.scrollHeight;
}

function escapeHtml(text) {
    if (!text) return '';
    const m = { '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#039;' };
    return text.replace(/[&<>"']/g, c => m[c]);
}

function updateDashboard(data) {
    updateStatusBar(data);
    updateCurrentStats();
    updateModemPanel();
    updateLatestHex();
    if (activeHexTab === 'history' && historyIndex >= 0) {
        renderHistoryView();
    }
    updateMap();
    updateCharts();
    if (isFirstLoad) isFirstLoad = false;
}

function updateStatusBar(data) {
    const t = new Date(data.lastUpdated * 1000);
    document.getElementById('lastUpdate').textContent = t.toLocaleTimeString();
    document.getElementById('totalRecords').textContent = data.totalRecords;
}

function updateCurrentStats() {
    if (telemetryData.length === 0) return;
    const latest = telemetryData[telemetryData.length - 1];
    if (!latest.payload) return;
    const p = latest.payload;

    document.getElementById('currentAltitude').textContent = p.altitude ? p.altitude.toFixed(0) : '----';
    document.getElementById('currentInternalTemp').textContent = p.sensors?.internalTemp?.measurement?.toFixed(1) || '--.-';
    document.getElementById('currentExternalTemp').textContent = p.sensors?.externalTemp?.measurement?.toFixed(1) || '--.-';
    document.getElementById('currentPressure').textContent = p.sensors?.pressure?.measurement?.toFixed(2) || '--.--';
    document.getElementById('currentBattery').textContent = p.sensors?.battery?.measurement?.toFixed(2) || '--.--';
    document.getElementById('currentAccelX').textContent = p.sensors?.accelX?.measurement?.toFixed(2) || '--.--';
    document.getElementById('currentAccelY').textContent = p.sensors?.accelY?.measurement?.toFixed(2) || '--.--';
    document.getElementById('currentMomsn').textContent = latest.momsn || '--';

    if (p.utcHours !== undefined) {
        document.getElementById('currentUTC').textContent =
            String(p.utcHours).padStart(2, '0') + ':' +
            String(p.utcMinutes).padStart(2, '0') + ':' +
            String(p.utcSeconds).padStart(2, '0');
    }

    setBar('altBar', p.altitude, 0, 120000);
    setBar('intTempBar', p.sensors?.internalTemp?.measurement, -60, 60);
    setBar('extTempBar', p.sensors?.externalTemp?.measurement, -60, 60);
    setBar('psiBar', p.sensors?.pressure?.measurement, 0, 15);
    setBar('battBar', p.sensors?.battery?.measurement, 0, 5);
    setBar('accXBar', p.sensors?.accelX?.measurement, -3, 3);
    setBar('accYBar', p.sensors?.accelY?.measurement, -3, 3);
}

function setBar(id, value, min, max) {
    const el = document.getElementById(id);
    if (!el || value === undefined || value === null) return;
    el.style.width = Math.max(0, Math.min(100, ((value - min) / (max - min)) * 100)) + '%';
}

function updateModemPanel() {
    if (telemetryData.length === 0) return;
    const latest = telemetryData[telemetryData.length - 1];

    document.getElementById('latestImei').textContent = latest.imei || '--';
    document.getElementById('latestTime').textContent = latest.transmitTime || '--';
    document.getElementById('latestSessionStatus').textContent = latest.sessionStatus !== undefined ? latest.sessionStatus : '--';

    const codeEl = document.getElementById('modemCode');
    const descEl = document.getElementById('modemDesc');
    if (latest.payload?.modemStatus !== undefined) {
        const code = latest.payload.modemStatus;
        codeEl.textContent = code;
        descEl.textContent = latest.payload.modemStatusDescription || 'UNKNOWN';
        codeEl.classList.remove('error', 'warn', 'ok');
        if (code >= 400) codeEl.classList.add('ok');
        else if (code >= 300) codeEl.classList.add('warn');
        else codeEl.classList.add('error');
    }

    if (latest.payload) {
        document.getElementById('latestGps').textContent = `${latest.payload.latitude?.toFixed(6) || '--'}, ${latest.payload.longitude?.toFixed(6) || '--'}`;
        document.getElementById('latestHeader').textContent = latest.payload.header ? `"${latest.payload.header}" ${latest.payload.headerValid ? '✓' : '✗'}` : '--';
    }
    document.getElementById('latestIridiumGps').textContent = `${latest.iridiumLatitude?.toFixed(4) || '--'}, ${latest.iridiumLongitude?.toFixed(4) || '--'}`;
    document.getElementById('latestCep').textContent = latest.iridiumCep?.toFixed(1) || '--';
}

function updateLatestHex() {
    if (telemetryData.length === 0) return;
    const latest = telemetryData[telemetryData.length - 1];
    const hexStr = latest.hexData || '';

    const metaEl = document.getElementById('latestHexMeta');
    if (metaEl) metaEl.textContent = hexStr ? `(MOMSN ${latest.momsn}, ${hexStr.replace(/\s/g, '').length / 2} bytes)` : '';

    document.getElementById('rawHex').textContent = hexStr ? formatHexString(hexStr) : '-- no hex data in JSON --';

    const bytes = hexToBytes(hexStr);
    renderHexRows('hexRows', bytes);
    renderByteMap('byteMap', bytes);
}

function updateMap() {
    if (!map || telemetryData.length === 0) return;
    if (isFirstLoad) {
        markers.forEach(m => map.removeLayer(m)); markers = [];
        if (flightPath) map.removeLayer(flightPath);
    }
    const coords = [];
    telemetryData.forEach((entry, i) => {
        if (entry.payload?.latitude && entry.payload?.longitude) {
            const lat = entry.payload.latitude, lon = entry.payload.longitude, alt = entry.payload.altitude || 0;
            coords.push([lat, lon]);
            const shouldAdd = i === 0 || i === telemetryData.length - 1 || i % 5 === 0;
            const exists = markers.some(m => m.options.index === i);
            if (shouldAdd && !exists) {
                const isLatest = i === telemetryData.length - 1, isFirst = i === 0;
                const c = isLatest ? '#ff3333' : (isFirst ? '#33ff33' : '#00ffcc');
                const icon = L.divIcon({ className: 'custom-marker', html: `<div style="background:${c};width:8px;height:8px;border-radius:50%;border:1px solid #010a01;box-shadow:0 0 6px ${c};"></div>`, iconSize: [8, 8] });
                const marker = L.marker([lat, lon], { icon, index: i }).addTo(map);
                marker.bindPopup(`<div style="font-family:'IBM Plex Mono',monospace;font-size:10px;background:#020e02;color:#22cc22;padding:8px;border:1px solid #0a3a0a;"><b style="color:#00ffcc;">MOMSN ${entry.momsn}</b><br>ALT: ${alt.toFixed(0)} ft<br>LAT: ${lat.toFixed(6)}<br>LON: ${lon.toFixed(6)}<br><span style="color:#147014;">${entry.transmitTime || ''}</span></div>`);
                markers.push(marker);
            }
        }
    });
    if (coords.length > 0) {
        if (flightPath) map.removeLayer(flightPath);
        flightPath = L.polyline(coords, { color: '#00ffcc', weight: 2, opacity: 0.7, dashArray: '4 6' }).addTo(map);
        if (isFirstLoad || !mapInitialized) { map.fitBounds(flightPath.getBounds(), { padding: [20, 20] }); mapInitialized = true; }
    }
}

function updateCharts() {
    if (telemetryData.length === 0) return;
    if (telemetryData.length === lastTelemetryCount && !isFirstLoad) return;

    const labels = [], altD = [], intT = [], extT = [], battD = [], psiD = [];
    telemetryData.forEach(e => {
        if (e.payload) {
            labels.push(`#${e.momsn}`);
            altD.push(e.payload.altitude || null);
            intT.push(e.payload.sensors?.internalTemp?.measurement || null);
            extT.push(e.payload.sensors?.externalTemp?.measurement || null);
            battD.push(e.payload.sensors?.battery?.measurement || null);
            psiD.push(e.payload.sensors?.pressure?.measurement || null);
        }
    });

    const mode = isFirstLoad ? 'active' : 'none';
    charts.altitude.data.labels = labels; charts.altitude.data.datasets[0].data = altD; charts.altitude.update(mode);
    charts.temperature.data.labels = labels; charts.temperature.data.datasets[0].data = intT; charts.temperature.data.datasets[1].data = extT; charts.temperature.update(mode);
    charts.battery.data.labels = labels; charts.battery.data.datasets[0].data = battD; charts.battery.data.datasets[1].data = psiD; charts.battery.update(mode);
    lastTelemetryCount = telemetryData.length;
}

(function injectHexStyles() {
    const s = document.createElement('style');
    s.textContent = `
        .hex-byte-hdr{color:#66ff33} .hex-byte-ser{color:#33ccff} .hex-byte-utc{color:#ffaa33}
        .hex-byte-gps{color:#5588ff} .hex-byte-alt{color:#cc66ff} .hex-byte-adc{color:#33ffaa}
        .hex-byte-rsv{color:#555555} .hex-byte-mdm{color:#ff5555}
    `;
    document.head.appendChild(s);
})();

window.addEventListener('DOMContentLoaded', init);