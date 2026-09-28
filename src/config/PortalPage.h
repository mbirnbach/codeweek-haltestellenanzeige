#pragma once

// The single page of the setup portal. Plain HTML/CSS/JS, no framework.
// All dynamic text is inserted with textContent (never innerHTML), so SSIDs
// and stop names can't inject markup. The API token is never part of this
// page or of any JSON the ESP sends to the browser.

static const char PORTAL_PAGE[] = R"HTML(<!doctype html>
<html lang="de"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Haltestellenanzeige einrichten</title>
<style>
body{font-family:system-ui,-apple-system,sans-serif;max-width:480px;margin:0 auto;padding:12px 16px;background:#111;color:#eee}
h1{font-size:1.3em;color:#fc0;margin:8px 0}h2{font-size:1.1em;margin:0 0 6px}
section{background:#222;border-radius:12px;padding:12px;margin:12px 0}
section.off{opacity:.35;pointer-events:none}
input,select,button{width:100%;box-sizing:border-box;font-size:1.05em;padding:11px;margin:5px 0;border-radius:8px;border:1px solid #555;background:#000;color:#eee}
button{background:#fc0;color:#000;font-weight:bold;border:0}
button:disabled{opacity:.5}
ul{list-style:none;padding:0;margin:0}
li button{background:#333;color:#eee;text-align:left;font-weight:normal}
li button.sel{background:#fc0;color:#000;font-weight:bold}
.msg{min-height:1.3em;margin:6px 0}.ok{color:#6d6}.err{color:#f77}
small{color:#999}
</style></head><body>
<h1>Haltestellenanzeige einrichten</h1>

<section id="s1"><h2>1. WLAN</h2>
<button id="scan" type="button">WLANs suchen</button>
<select id="ssids"><option value="">WLAN auswählen …</option></select>
<input id="ssid" placeholder="WLAN-Name" maxlength="32" autocomplete="off" autocapitalize="off">
<input id="pw" type="password" placeholder="WLAN-Passwort" maxlength="63" autocomplete="off">
<button id="connect" type="button">Verbinden</button>
<p class="msg" id="m1"></p></section>

<section id="s2" class="off"><h2>2. Haltestelle</h2>
<input id="q" placeholder="z.B. Königsplatz" maxlength="64">
<button id="search" type="button">Suchen</button>
<p class="msg" id="m2"></p><ul id="stops"></ul></section>

<section id="s3" class="off"><h2>3. Fertig</h2>
<p id="chosen"></p>
<button id="save" type="button">Speichern und starten</button>
<p class="msg" id="m3"></p></section>

<small id="ap"></small>

<script>
const $ = id => document.getElementById(id);
const sleep = ms => new Promise(r => setTimeout(r, ms));
let stop = null, apName = '';

function msg(id, text, cls) { const e = $(id); e.textContent = text; e.className = 'msg ' + (cls || ''); }
function enable(id, on) { $(id).className = on ? '' : 'off'; }

async function call(path, body) {
  const opt = body ? { method: 'POST', headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
                       body: new URLSearchParams(body) } : {};
  const r = await fetch(path, opt);
  const j = await r.json().catch(() => ({}));
  if (!r.ok) throw new Error(j.error || ('Fehler ' + r.status));
  return j;
}

async function scan() {
  $('scan').disabled = true; msg('m1', 'Suche WLANs …');
  try {
    let j = {};
    for (let i = 0; i < 20; i++) { j = await call('/config/wifi/scan'); if (!j.scanning) break; await sleep(1000); }
    const sel = $('ssids'); sel.length = 1;
    (j.networks || []).forEach(n => {
      const o = document.createElement('option'); o.value = n.ssid;
      o.textContent = n.ssid + (n.secure ? '' : ' (offen)') + ' · ' + n.quality + '%';
      sel.appendChild(o);
    });
    msg('m1', (j.networks || []).length + ' WLANs gefunden.');
  } catch (e) { msg('m1', e.message, 'err'); }
  $('scan').disabled = false;
}

async function waitForWifi() {
  let misses = 0;
  for (let i = 0; i < 40; i++) {
    await sleep(1000);
    let j;
    try { j = await call('/config/wifi/status'); misses = 0; }
    catch (e) {
      // The ESP may switch its radio channel to the router's: the phone
      // loses the setup network for a moment.
      if (++misses > 3) msg('m1', 'Verbindung zum Gerät kurz weg – falls nötig, Handy wieder mit „' + apName + '“ verbinden …');
      continue;
    }
    if (j.status === 'connected') { msg('m1', 'Verbunden mit „' + j.ssid + '“!', 'ok'); enable('s2', true); return; }
    if (j.status === 'failed') { msg('m1', j.message || 'Verbindung fehlgeschlagen.', 'err'); return; }
  }
  msg('m1', 'Keine Antwort. Bitte nochmal „Verbinden“ drücken.', 'err');
}

async function connect() {
  const ssid = $('ssid').value.trim();
  if (!ssid) { msg('m1', 'Bitte ein WLAN auswählen oder den Namen eingeben.', 'err'); return; }
  $('connect').disabled = true; enable('s2', false); enable('s3', false);
  msg('m1', 'Verbinde mit „' + ssid + '“ …');
  try {
    await call('/config/wifi', { ssid: ssid, password: $('pw').value });
    await waitForWifi();
  } catch (e) { msg('m1', e.message, 'err'); }
  $('connect').disabled = false;
}

async function search() {
  const q = $('q').value.trim();
  if (q.length < 2) { msg('m2', 'Mindestens 2 Buchstaben eingeben.', 'err'); return; }
  $('search').disabled = true; msg('m2', 'Suche …'); $('stops').textContent = '';
  try {
    const list = await call('/config/stops?q=' + encodeURIComponent(q));
    msg('m2', list.length ? 'Bitte antippen:' : 'Nichts gefunden. Anders schreiben?');
    list.forEach(s => {
      const li = document.createElement('li'), b = document.createElement('button');
      b.type = 'button'; b.textContent = s.name;
      b.onclick = () => { stop = s; document.querySelectorAll('li button').forEach(x => x.className = ''); b.className = 'sel'; $('chosen').textContent = 'Haltestelle: ' + s.name; enable('s3', true); $('save').scrollIntoView(); };
      li.appendChild(b); $('stops').appendChild(li);
    });
  } catch (e) { msg('m2', e.message, 'err'); }
  $('search').disabled = false;
}

async function save() {
  if (!stop) return;
  $('save').disabled = true;
  try {
    await call('/config/save', { stop_id: stop.id, stop_name: stop.name });
    msg('m3', 'Gespeichert! Die Anzeige startet jetzt neu. Du kannst dieses Fenster schließen.', 'ok');
  } catch (e) { msg('m3', e.message, 'err'); $('save').disabled = false; }
}

$('scan').onclick = scan;
$('connect').onclick = connect;
$('search').onclick = search;
$('save').onclick = save;
$('q').onkeydown = e => { if (e.key === 'Enter') search(); };
$('ssids').onchange = () => { if ($('ssids').value) { $('ssid').value = $('ssids').value; $('pw').focus(); } };

(async () => {
  try {
    const s = await call('/config/state');
    apName = s.ap;
    $('ap').textContent = 'Gerät: ' + s.ap + ' · Firmware ' + s.version;
    if (s.ssid) { $('ssid').value = s.ssid; if (s.hasPassword) $('pw').placeholder = 'Passwort (leer = gespeichertes behalten)'; }
    if (s.stopName) $('chosen').textContent = 'Bisher: ' + s.stopName;
    if (s.wifi === 'connecting') { msg('m1', 'Verbinde mit gespeichertem WLAN „' + s.ssid + '“ …'); await waitForWifi(); }
    else if (s.wifi === 'connected') { msg('m1', 'Verbunden mit „' + s.ssid + '“.', 'ok'); enable('s2', true); }
  } catch (e) {}
  if (!$('ssids').value) scan();
})();
</script>
</body></html>
)HTML";
