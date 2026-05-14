/**
 * @file html_pages.h
 * @brief Páginas HTML embebidas para el portal de configuración WiFi.
 *
 * Contiene:
 *  - getPortalHTML()     → Formulario de configuración (modo AP)
 *  - getConnectedHTML()  → Página de estado (modo STA conectado)
 */

#pragma once
#include <Arduino.h>

// ════════════════════════════════════════════════════════════════════════════
//  PORTAL DE CONFIGURACIÓN (Modo AP)
// ════════════════════════════════════════════════════════════════════════════
String getPortalHTML() {
  return R"rawhtml(
<!DOCTYPE html>
<html lang="es">
<head>
  <meta charset="UTF-8"/>
  <meta name="viewport" content="width=device-width, initial-scale=1.0"/>
  <title>ESP32 · Configuración WiFi</title>
  <style>
    :root {
      --bg:      #0f1117;
      --surface: #1a1d27;
      --border:  #2e3348;
      --accent:  #4f9eff;
      --accent2: #7c3aed;
      --text:    #e2e8f0;
      --muted:   #64748b;
      --success: #22c55e;
      --error:   #ef4444;
      --warning: #f59e0b;
    }
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body {
      font-family: 'Segoe UI', system-ui, sans-serif;
      background: var(--bg);
      color: var(--text);
      min-height: 100vh;
      display: flex;
      align-items: center;
      justify-content: center;
      padding: 1rem;
    }
    .card {
      background: var(--surface);
      border: 1px solid var(--border);
      border-radius: 16px;
      width: 100%;
      max-width: 440px;
      padding: 2rem;
      box-shadow: 0 25px 50px rgba(0,0,0,.5);
    }
    .logo {
      display: flex; align-items: center; gap: .75rem; margin-bottom: 1.5rem;
    }
    .logo-icon {
      width: 40px; height: 40px; border-radius: 10px;
      background: linear-gradient(135deg, var(--accent), var(--accent2));
      display: flex; align-items: center; justify-content: center;
      font-size: 1.2rem;
    }
    h1 { font-size: 1.4rem; font-weight: 700; }
    .subtitle { color: var(--muted); font-size: .85rem; margin-top: .2rem; }

    label {
      display: block; font-size: .8rem; color: var(--muted);
      text-transform: uppercase; letter-spacing: .06em;
      margin-bottom: .4rem; margin-top: 1.2rem;
    }
    input[type=text], input[type=password] {
      width: 100%; padding: .7rem 1rem;
      background: var(--bg); border: 1px solid var(--border);
      border-radius: 8px; color: var(--text); font-size: .95rem;
      transition: border-color .2s;
    }
    input:focus { outline: none; border-color: var(--accent); }

    .network-list {
      margin-top: .6rem; max-height: 180px; overflow-y: auto;
      border: 1px solid var(--border); border-radius: 8px;
      background: var(--bg);
    }
    .network-item {
      display: flex; align-items: center; justify-content: space-between;
      padding: .6rem 1rem; cursor: pointer; transition: background .15s;
      border-bottom: 1px solid var(--border);
    }
    .network-item:last-child { border-bottom: none; }
    .network-item:hover { background: rgba(79,158,255,.08); }
    .network-item.selected { background: rgba(79,158,255,.15); }
    .net-name { font-size: .9rem; font-weight: 500; }
    .net-meta { display: flex; align-items: center; gap: .5rem; }
    .rssi { font-size: .75rem; color: var(--muted); }
    .lock { font-size: .75rem; }

    .btn {
      display: block; width: 100%; padding: .85rem;
      border: none; border-radius: 8px; cursor: pointer;
      font-size: .95rem; font-weight: 600; margin-top: 1.5rem;
      transition: opacity .2s, transform .1s;
    }
    .btn:active { transform: scale(.98); }
    .btn-primary {
      background: linear-gradient(135deg, var(--accent), var(--accent2));
      color: #fff;
    }
    .btn-scan {
      background: transparent; border: 1px solid var(--border);
      color: var(--text); font-size: .85rem; margin-top: .6rem;
      padding: .6rem;
    }
    .btn:disabled { opacity: .4; cursor: not-allowed; }

    #status-msg {
      margin-top: 1rem; padding: .75rem 1rem;
      border-radius: 8px; font-size: .85rem; display: none;
    }
    .msg-success { background: rgba(34,197,94,.1); border: 1px solid rgba(34,197,94,.3); color: var(--success); }
    .msg-error   { background: rgba(239,68,68,.1);  border: 1px solid rgba(239,68,68,.3);  color: var(--error);   }
    .msg-info    { background: rgba(79,158,255,.1); border: 1px solid rgba(79,158,255,.3); color: var(--accent);  }

    .spinner {
      display: inline-block; width: 14px; height: 14px;
      border: 2px solid rgba(255,255,255,.3);
      border-top-color: #fff; border-radius: 50%;
      animation: spin .6s linear infinite; vertical-align: middle;
      margin-right: .5rem;
    }
    @keyframes spin { to { transform: rotate(360deg); } }

    .footer {
      margin-top: 1.5rem; padding-top: 1rem;
      border-top: 1px solid var(--border);
      font-size: .75rem; color: var(--muted); text-align: center;
    }
  </style>
</head>
<body>
<div class="card">
  <div class="logo">
    <div class="logo-icon">📡</div>
    <div>
      <h1>ESP32 Config</h1>
      <div class="subtitle">Portal de configuración WiFi</div>
    </div>
  </div>

  <label>Red WiFi disponibles</label>
  <div class="network-list" id="network-list">
    <div class="network-item" style="justify-content:center;color:var(--muted)">
      Presiona "Escanear" para buscar redes
    </div>
  </div>
  <button class="btn btn-scan" onclick="scanNetworks()">🔍 Escanear redes</button>

  <label for="ssid">SSID (Nombre de la red)</label>
  <input type="text" id="ssid" placeholder="Nombre de la red WiFi" autocomplete="off"/>

  <label for="password">Contraseña</label>
  <input type="password" id="password" placeholder="Contraseña WiFi"/>

  <button class="btn btn-primary" id="connect-btn" onclick="connectWifi()">
    Conectar
  </button>

  <div id="status-msg"></div>

  <div class="footer">
    ESP32 · Modo AP &nbsp;|&nbsp; IP: 192.168.4.1
  </div>
</div>

<script>
  function scanNetworks() {
    const list = document.getElementById('network-list');
    list.innerHTML = '<div class="network-item" style="justify-content:center;color:var(--muted)"><span class="spinner"></span> Escaneando...</div>';
    fetch('/scan')
      .then(r => r.json())
      .then(data => renderNetworks(data.networks))
      .catch(() => {
        list.innerHTML = '<div class="network-item" style="color:var(--error)">Error al escanear</div>';
      });
  }

  function renderNetworks(networks) {
    const list = document.getElementById('network-list');
    if (!networks || networks.length === 0) {
      list.innerHTML = '<div class="network-item" style="color:var(--muted)">Sin redes encontradas</div>';
      return;
    }
    list.innerHTML = networks.map(n => `
      <div class="network-item" onclick="selectNetwork('${n.ssid.replace(/'/g,"\\'")}')">
        <span class="net-name">${n.ssid}</span>
        <span class="net-meta">
          <span class="rssi">${n.rssi} dBm</span>
          <span class="lock">${n.security ? '🔒' : '🔓'}</span>
        </span>
      </div>`).join('');
  }

  function selectNetwork(ssid) {
    document.getElementById('ssid').value = ssid;
    document.querySelectorAll('.network-item').forEach(el => {
      el.classList.toggle('selected', el.querySelector('.net-name')?.textContent === ssid);
    });
    document.getElementById('password').focus();
  }

  function connectWifi() {
    const ssid = document.getElementById('ssid').value.trim();
    const pass = document.getElementById('password').value;
    if (!ssid) { showMsg('El SSID no puede estar vacío.', 'error'); return; }

    const btn = document.getElementById('connect-btn');
    btn.disabled = true;
    btn.innerHTML = '<span class="spinner"></span> Conectando...';

    const body = new URLSearchParams({ ssid, password: pass });
    fetch('/connect', { method: 'POST', body, headers: { 'Content-Type': 'application/x-www-form-urlencoded' } })
      .then(r => r.json())
      .then(data => {
        if (data.error) { showMsg(data.error, 'error'); btn.disabled = false; btn.textContent = 'Conectar'; }
        else { showMsg('✅ Credenciales guardadas. El dispositivo se reiniciará...', 'success'); }
      })
      .catch(() => { showMsg('Error de comunicación con el ESP32.', 'error'); btn.disabled = false; btn.textContent = 'Conectar'; });
  }

  function showMsg(msg, type) {
    const el = document.getElementById('status-msg');
    el.textContent = msg;
    el.className = 'msg-' + type;
    el.style.display = 'block';
  }
</script>
</body>
</html>
)rawhtml";
}

// ════════════════════════════════════════════════════════════════════════════
//  PÁGINA DE ESTADO (Modo STA – Conectado)
// ════════════════════════════════════════════════════════════════════════════
String getConnectedHTML(const String& ssid, const String& ip, const String& rssi) {
  String html = R"rawhtml(
<!DOCTYPE html>
<html lang="es">
<head>
  <meta charset="UTF-8"/>
  <meta name="viewport" content="width=device-width, initial-scale=1.0"/>
  <title>ESP32 · Conectado</title>
  <style>
    :root {
      --bg:#0f1117;--surface:#1a1d27;--border:#2e3348;
      --accent:#4f9eff;--accent2:#7c3aed;--text:#e2e8f0;
      --muted:#64748b;--success:#22c55e;--error:#ef4444;
    }
    *{box-sizing:border-box;margin:0;padding:0;}
    body{font-family:'Segoe UI',system-ui,sans-serif;background:var(--bg);color:var(--text);
         min-height:100vh;display:flex;align-items:center;justify-content:center;padding:1rem;}
    .card{background:var(--surface);border:1px solid var(--border);border-radius:16px;
          width:100%;max-width:400px;padding:2rem;box-shadow:0 25px 50px rgba(0,0,0,.5);}
    .status-badge{display:inline-flex;align-items:center;gap:.5rem;background:rgba(34,197,94,.1);
                  border:1px solid rgba(34,197,94,.3);color:var(--success);
                  padding:.4rem .9rem;border-radius:999px;font-size:.8rem;font-weight:600;margin-bottom:1.5rem;}
    .dot{width:8px;height:8px;border-radius:50%;background:var(--success);
         animation:pulse 2s ease-in-out infinite;}
    @keyframes pulse{0%,100%{opacity:1;}50%{opacity:.4;}}
    h1{font-size:1.4rem;font-weight:700;margin-bottom:.3rem;}
    .subtitle{color:var(--muted);font-size:.85rem;margin-bottom:1.5rem;}
    .info-grid{display:grid;gap:.75rem;margin-bottom:1.5rem;}
    .info-row{display:flex;justify-content:space-between;align-items:center;
              background:var(--bg);padding:.65rem 1rem;border-radius:8px;
              border:1px solid var(--border);}
    .info-label{font-size:.78rem;color:var(--muted);text-transform:uppercase;letter-spacing:.05em;}
    .info-value{font-size:.9rem;font-weight:600;}
    .btn-reset{display:block;width:100%;padding:.75rem;border:1px solid rgba(239,68,68,.4);
               border-radius:8px;background:transparent;color:var(--error);cursor:pointer;
               font-size:.85rem;font-weight:600;transition:background .2s;}
    .btn-reset:hover{background:rgba(239,68,68,.08);}
    #msg{margin-top:1rem;padding:.65rem 1rem;border-radius:8px;font-size:.82rem;display:none;}
    .footer{margin-top:1.5rem;padding-top:1rem;border-top:1px solid var(--border);
            font-size:.75rem;color:var(--muted);text-align:center;}
  </style>
</head>
<body>
<div class="card">
  <div class="status-badge"><span class="dot"></span> Conectado</div>
  <h1>ESP32 Online</h1>
  <p class="subtitle">El dispositivo está operativo en la red WiFi.</p>

  <div class="info-grid">
    <div class="info-row">
      <span class="info-label">Red (SSID)</span>
      <span class="info-value">)rawhtml";
  html += ssid;
  html += R"rawhtml(</span>
    </div>
    <div class="info-row">
      <span class="info-label">Dirección IP</span>
      <span class="info-value">)rawhtml";
  html += ip;
  html += R"rawhtml(</span>
    </div>
    <div class="info-row">
      <span class="info-label">Señal (RSSI)</span>
      <span class="info-value">)rawhtml";
  html += rssi;
  html += R"rawhtml( dBm</span>
    </div>
  </div>

  <button class="btn-reset" onclick="resetDevice()">🔄 Restablecer configuración WiFi</button>
  <div id="msg"></div>

  <div class="footer">ESP32 · Modo STA &nbsp;|&nbsp; )rawhtml";
  html += ip;
  html += R"rawhtml(</div>
</div>
<script>
  function resetDevice() {
    if (!confirm('¿Borrar credenciales WiFi y reiniciar en modo AP?')) return;
    fetch('/reset', { method: 'POST' })
      .then(r => r.json())
      .then(d => {
        const el = document.getElementById('msg');
        el.textContent = d.message;
        el.style.cssText = 'display:block;background:rgba(239,68,68,.1);border:1px solid rgba(239,68,68,.3);color:#ef4444;';
      }).catch(() => alert('Error al comunicarse con el dispositivo.'));
  }
</script>
</body>
</html>)rawhtml";
  return html;
}
