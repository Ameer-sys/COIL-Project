// config text
const CONFIG_DESCS = {
    1: 'Test against the simulator. Browser talks to your server, then your server talks to the simulator.',
    2: 'Use this for direct robot control. Browser talks to your server, then your server talks directly to a robot.',
    3: 'Use this for routing. Browser talks to PC2, PC2 forwards to PC3, then PC3 talks to the robot.'
};

let currentConfig = 1;
let currentTarget = 'simulator';
let currentProtocol = 'UDP';
let pollInterval = null;
let isSending = false;
let formDirty = false;

// startup
window.addEventListener('DOMContentLoaded', () => {
    bindFormDirtyEvents();
    bindProtocolEvents();
    fetchStatus();
    pollInterval = setInterval(fetchStatus, 3000);
});

// mark form dirty
function bindFormDirtyEvents() {
    ['ipInput', 'portInput', 'nextHopIP', 'nextHopPort'].forEach(id => {
        const el = document.getElementById(id);
        if (!el) return;
        el.addEventListener('input', () => {
            formDirty = true;
        });
    });
}

// protocol event
function bindProtocolEvents() {
    const protocol = document.getElementById('protocolInput');
    if (!protocol) return;
    protocol.addEventListener('change', () => {
        formDirty = true;
    });
}

// auto port for protocol
function onProtocolChange() {
    const protocol = document.getElementById('protocolInput').value;
    currentProtocol = protocol;

    const portInput = document.getElementById('portInput');
    const currentPort = portInput.value.trim();

    if (protocol === 'UDP' && (currentPort === '' || currentPort === '29000')) {
        portInput.value = '29500';
    }

    if (protocol === 'TCP' && (currentPort === '' || currentPort === '29500')) {
        portInput.value = '29000';
    }

    formDirty = true;
}

// valid targets
function getAllowedTargets(configNum) {
    if (configNum === 1) return ['simulator'];
    return ['robot1', 'robot2'];
}

// update target buttons
function updateTargetButtons() {
    const allowed = getAllowedTargets(currentConfig);

    document.querySelectorAll('.preset-btn').forEach(btn => {
        const show = allowed.includes(btn.dataset.target);
        btn.classList.toggle('hidden', !show);
        btn.classList.toggle('active', btn.dataset.target === currentTarget && show);
    });

    if (!allowed.includes(currentTarget)) {
        currentTarget = allowed[0];
    }
}

// fetch status
function fetchStatus() {
    fetch('/status')
        .then(r => r.json())
        .then(data => {
            document.getElementById('statusDot').className = 'dot online';
            document.getElementById('statusText').textContent = 'Server Online';

            document.getElementById('stTarget').textContent = formatTarget(data.target);
            document.getElementById('stIP').textContent = data.ip;
            document.getElementById('stPort').textContent = data.port;
            document.getElementById('stProtocol').textContent = data.protocol || 'UDP';
            document.getElementById('stConfig').textContent = '#' + data.configuration;
            document.getElementById('stRoute').textContent = data.routing ? 'Routed' : 'Direct';
            document.getElementById('stLast').textContent = shortResult(data.lastResult || '—');

            const networkChip = document.getElementById('networkChip');
            if (data.lastAck && data.lastCRC) {
                networkChip.textContent = 'Last command verified';
                networkChip.className = 'status-chip ok';
            } else if (data.lastCommand && data.lastCommand !== 'None') {
                networkChip.textContent = 'Awaiting verified response';
                networkChip.className = 'status-chip warn';
            } else {
                networkChip.textContent = 'Target not verified yet';
                networkChip.className = 'status-chip';
            }

            if (data.configuration !== currentConfig) {
                currentConfig = data.configuration;
                updateConfigTabs();
            }

            if (data.target !== currentTarget) {
                currentTarget = data.target;
            }

            currentProtocol = data.protocol || 'UDP';

            updateTargetButtons();

            if (!formDirty) {
                document.getElementById('ipInput').value = data.ip;
                document.getElementById('portInput').value = data.port;
                document.getElementById('protocolInput').value = currentProtocol;
                if (data.nextHopIP) document.getElementById('nextHopIP').value = data.nextHopIP;
                if (data.nextHopPort) document.getElementById('nextHopPort').value = data.nextHopPort;
            }

            if (data.configuration === 3) {
                document.getElementById('routingPanel').classList.remove('hidden');
            } else {
                document.getElementById('routingPanel').classList.add('hidden');
            }

            renderLog(data.log);
            renderStatusResponse(data);
        })
        .catch(() => {
            document.getElementById('statusDot').className = 'dot offline';
            document.getElementById('statusText').textContent = 'Server Offline';
            document.getElementById('networkChip').textContent = 'Cannot reach server';
            document.getElementById('networkChip').className = 'status-chip fail';
        });
}

// set config
function setConfig(num) {
    currentConfig = num;

    const allowed = getAllowedTargets(num);
    if (!allowed.includes(currentTarget)) {
        currentTarget = allowed[0];
    }

    updateConfigTabs();
    updateTargetButtons();

    fetch('/routing_table/', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
            configuration: num,
            enabled: num === 3,
            activeTarget: currentTarget,
            protocol: document.getElementById('protocolInput').value,
            nextHopIP: document.getElementById('nextHopIP').value.trim() || '',
            nextHopPort: parseInt(document.getElementById('nextHopPort').value) || 3540
        })
    })
    .then(() => {
        formDirty = false;
        showToast(`Configuration #${num} selected`, 'ok');
        fetchStatus();
    })
    .catch(() => showToast('Failed to change configuration', 'error'));
}

// update config text
function updateConfigTabs() {
    document.querySelectorAll('.config-tab').forEach(btn => {
        btn.classList.toggle('active', parseInt(btn.dataset.config) === currentConfig);
    });

    document.getElementById('configDesc').textContent = CONFIG_DESCS[currentConfig] || '';

    if (currentConfig === 3) {
        document.getElementById('routingPanel').classList.remove('hidden');
    } else {
        document.getElementById('routingPanel').classList.add('hidden');
    }
}

// set target
function setTarget(target) {
    currentTarget = target;
    updateTargetButtons();

    fetch('/routing_table/', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
            activeTarget: target,
            configuration: currentConfig,
            enabled: currentConfig === 3,
            protocol: document.getElementById('protocolInput').value
        })
    })
    .then(() => {
        formDirty = false;
        showToast(`${formatTarget(target)} selected`, 'ok');
        fetchStatus();
    })
    .catch(() => showToast('Failed to change target', 'error'));
}

// connect
function connect() {
    const ip = document.getElementById('ipInput').value.trim();
    const port = parseInt(document.getElementById('portInput').value);
    const protocol = document.getElementById('protocolInput').value;

    if (!ip || !port) {
        showToast('Enter a valid IP and port', 'error');
        return;
    }

    fetch('/connect', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ ip, port, protocol })
    })
    .then(r => r.json())
    .then(data => {
        formDirty = false;
        showToast(`Target set to ${data.ip}:${data.port} via ${data.protocol}`, 'ok');
        fetchStatus();
    })
    .catch(() => showToast('Connection setup failed', 'error'));
}

// save routing
function saveRouting() {
    const ip = document.getElementById('nextHopIP').value.trim();
    const port = parseInt(document.getElementById('nextHopPort').value);

    if (!ip || !port) {
        showToast('Enter a valid next hop IP and port', 'error');
        return;
    }

    fetch('/routing_table/', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
            configuration: 3,
            enabled: true,
            nextHopIP: ip,
            nextHopPort: port,
            activeTarget: currentTarget,
            protocol: document.getElementById('protocolInput').value
        })
    })
    .then(() => {
        formDirty = false;
        showToast(`Routing saved: ${ip}:${port}`, 'ok');
        fetchStatus();
    })
    .catch(() => showToast('Failed to save routing', 'error'));
}

// send drive
function sendDrive(direction) {
    const duration = parseInt(document.getElementById('duration').value) || 3;
    const power = parseInt(document.getElementById('power').value) || 80;

    const body = {
        cmd: 'drive',
        direction: direction,
        duration: duration,
        power: (direction === 'left' || direction === 'right') ? 100 : power
    };

    const btn = document.querySelector(`.dpad-btn[data-dir="${direction}"]`);
    if (btn) {
        btn.classList.add('sending');
        setTimeout(() => btn.classList.remove('sending'), 500);
    }

    sendTelecommand(body);
}

// send sleep
function sendSleep() {
    sendTelecommand({ cmd: 'sleep' });
}

// disable buttons
function setButtonsDisabled(disabled) {
    document.querySelectorAll('.dpad-btn, .btn-warning, .btn-info').forEach(btn => {
        btn.disabled = disabled;
        btn.classList.toggle('disabled', disabled);
    });
}

// send telecommand
function sendTelecommand(body) {
    if (isSending) return;

    isSending = true;
    setButtonsDisabled(true);

    fetch('/telecommand/', {
        method: 'PUT',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(body)
    })
    .then(r => r.json())
    .then(data => {
        renderResponse(body, data);
        fetchStatus();
    })
    .catch(() => {
        renderResponseError(body.cmd || 'command');
        showToast('Failed to send command', 'error');
    })
    .finally(() => {
        isSending = false;
        setButtonsDisabled(false);
    });
}

// request telemetry
function requestTelemetry() {
    if (isSending) return;

    isSending = true;
    setButtonsDisabled(true);

    fetch('/telementry_request/')
        .then(r => r.json())
        .then(data => {
            renderTelemetry(data);
            fetchStatus();
            showToast('Telemetry updated', 'ok');
        })
        .catch(() => showToast('Telemetry request failed', 'error'))
        .finally(() => {
            isSending = false;
            setButtonsDisabled(false);
        });
}

// render response
function renderResponse(cmd, data) {
    const panel = document.getElementById('responsePanel');

    const ack = !!data.ack;
    const crc = !!data.crc;
    const label = cmd.cmd === 'sleep'
        ? 'Sleep / Reset'
        : cmd.cmd === 'drive'
            ? ((cmd.direction === 'left' || cmd.direction === 'right')
                ? `Turn ${cap(cmd.direction)}`
                : `Drive ${cap(cmd.direction)}`)
            : cap(cmd.cmd);

    const time = new Date().toLocaleTimeString();
    const statusText =
    data.bodyText && data.bodyText.trim() !== ''
        ? data.bodyText
        : (data.message || 'Command response received');

setTelemetryValue('tvStatus', statusText);
setTelemetryValue('tvTarget', formatTarget(currentTarget));
setTelemetryValue('tvProtocol', currentProtocol || '--');
setTelemetryValue('tvPkt', data.pktCount ? `#${data.pktCount}` : '--');
setTelemetryValue('tvAck', data.ack ? 'Yes' : 'No');
setTelemetryValue('tvCrc', data.crc ? 'Valid ✓' : 'Invalid ✗');
setTelemetryValue('tvCmd', label);

document.getElementById('telemetryMessage').textContent = statusText;

    panel.innerHTML = `
        <div class="response-item">
            <div class="response-row">
                <span class="response-label">Command</span>
                <span class="response-val">${label}</span>
            </div>
            <div class="response-row">
                <span class="response-label">Time</span>
                <span class="response-val">${time}</span>
            </div>
            <div class="response-row">
                <span class="response-label">Target</span>
                <span class="response-val">${formatTarget(currentTarget)}</span>
            </div>
            <div class="response-row">
                <span class="response-label">Route</span>
                <span class="response-val">${currentConfig === 3 ? 'Routed' : 'Direct'}</span>
            </div>
            <div class="response-row">
                <span class="response-label">Acknowledged</span>
                <span class="${ack ? 'response-val-ok' : 'response-val-fail'}">${ack ? 'Yes ✓' : 'No ✗'}</span>
            </div>
            <div class="response-row">
                <span class="response-label">CRC</span>
                <span class="${crc ? 'response-val-ok' : 'response-val-fail'}">${crc ? 'Valid ✓' : 'Invalid ✗'}</span>
            </div>
            <div class="response-row">
                <span class="response-label">Packet</span>
                <span class="response-val">#${data.pktCount || 0}</span>
            </div>
            <div class="response-note ${ack && crc ? 'ok-note' : 'fail-note'}">
                ${data.message || (ack && crc ? 'Command executed successfully' : 'Command failed or timed out')}
            </div>
        </div>`;

    setTelemetryValue('tvTarget', formatTarget(currentTarget));
    setTelemetryValue('tvProtocol', currentProtocol || '--');
    setTelemetryValue('tvAck', data.ack ? 'Yes' : 'No');
    setTelemetryValue('tvCmd', label);
    document.getElementById('telemetryMessage').textContent = data.message || 'Command response received';
}

// render status response
function renderStatusResponse(data) {
    if (!data.lastCommand || data.lastCommand === 'None') return;

    const panel = document.getElementById('responsePanel');
    panel.innerHTML = `
        <div class="response-item">
            <div class="response-row">
                <span class="response-label">Last Command</span>
                <span class="response-val">${data.lastCommand}</span>
            </div>
            <div class="response-row">
                <span class="response-label">Time</span>
                <span class="response-val">${data.lastCommandTime}</span>
            </div>
            <div class="response-row">
                <span class="response-label">Target</span>
                <span class="response-val">${data.lastCommandTarget}</span>
            </div>
            <div class="response-row">
                <span class="response-label">Route</span>
                <span class="response-val">${data.lastCommandRoute}</span>
            </div>
            <div class="response-row">
                <span class="response-label">Acknowledged</span>
                <span class="${data.lastAck ? 'response-val-ok' : 'response-val-fail'}">${data.lastAck ? 'Yes ✓' : 'No ✗'}</span>
            </div>
            <div class="response-row">
                <span class="response-label">CRC</span>
                <span class="${data.lastCRC ? 'response-val-ok' : 'response-val-fail'}">${data.lastCRC ? 'Valid ✓' : 'Invalid ✗'}</span>
            </div>
            <div class="response-row">
                <span class="response-label">Packet</span>
                <span class="response-val">#${data.lastPktCount || 0}</span>
            </div>
            <div class="response-note ${data.lastAck && data.lastCRC ? 'ok-note' : 'fail-note'}">
                ${data.lastResult || 'No response available'}
            </div>
        </div>`;
}

// render response error
function ponseError(cmdName) {
    const panel = document.getElementById('responsePanel');
    panel.innerHTML = `
        <div class="response-item">
            <div class="response-row">
                <span class="response-label">Command</span>
                <span class="response-val">${cap(cmdName)}</span>
            </div>
            <div class="response-note fail-note">
                No response received within timeout period
            </div>
        </div>`;
}

// render telemetry
function renderTelemetry(data) {
    const statusText =
        data.bodyText && data.bodyText.trim() !== ''
            ? data.bodyText
            : (data.message && data.message.trim() !== '' ? data.message : 'Response received');

    setTelemetryValue('tvStatus', statusText);
    setTelemetryValue('tvTarget', formatTarget(currentTarget));
    setTelemetryValue('tvProtocol', currentProtocol || '--');
    setTelemetryValue('tvPkt', data.pktCount ? `#${data.pktCount}` : '--');
    setTelemetryValue('tvAck', 'Yes');
    setTelemetryValue('tvCrc', data.crc ? 'Valid ✓' : 'Invalid ✗');

    if (data.lastCmd && data.lastCmd !== '--') {
        setTelemetryValue('tvCmd', data.lastCmd);
    } else {
        setTelemetryValue('tvCmd', 'Telemetry Request');
    }

    const msg = document.getElementById('telemetryMessage');
    msg.textContent = statusText;
}

// set telemetry value
function setTelemetryValue(id, value) {
    const el = document.getElementById(id);
    if (el) el.textContent = value;
}

// render log
function renderLog(entries) {
    const panel = document.getElementById('logPanel');
    if (!entries || entries.length === 0) {
        panel.innerHTML = '<div class="log-empty">No packets logged yet</div>';
        return;
    }

    panel.innerHTML = [...entries].reverse().map(e => {
        const cls = !e.success ? 'fail'
            : e.dir === 'OUT' ? 'out'
            : e.dir === 'IN' ? 'in'
            : 'sys';

        return `
            <div class="log-entry ${cls}">
                <span class="log-time">${e.time}</span>
                <span class="log-dir">${e.dir}</span>
                <span class="log-type">${e.type}</span>
                <span class="log-detail">${e.details}</span>
            </div>`;
    }).join('');
}

// clear log
function clearLog() {
    fetch('/log/clear', { method: 'POST' })
        .then(r => r.json())
        .then(() => {
            showToast('Server log cleared', 'ok');
            fetchStatus();
        })
        .catch(() => showToast('Failed to clear log', 'error'));
}

// download log
function downloadLog() {
    window.open('/log/download', '_blank');
}

// toast helper
function showToast(msg, type) {
    const existing = document.getElementById('toast');
    if (existing) existing.remove();

    const t = document.createElement('div');
    t.id = 'toast';
    t.className = 'toast ' + (type === 'ok' ? 'toast-ok' : 'toast-error');
    t.textContent = msg;
    document.body.appendChild(t);

    setTimeout(() => {
        if (t) t.remove();
    }, 3000);
}

// capitalize helper
function cap(s) {
    if (!s) return '';
    return s.charAt(0).toUpperCase() + s.slice(1);
}

// target label
function formatTarget(t) {
    return t === 'robot1' ? 'Robot 1'
        : t === 'robot2' ? 'Robot 2'
        : 'Simulator';
}

// shorten text
function shortResult(s) {
    if (!s) return '—';
    return s.length > 24 ? s.substring(0, 24) + '...' : s;
}