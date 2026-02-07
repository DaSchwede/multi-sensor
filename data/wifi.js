let _afterSave = false;
let _pollTimer = null;

function $(id){ return document.getElementById(id); }

async function wifiPollStatus(){
  try{
    const r = await fetch('/api/wifi/status', { cache:'no-store' });
    if(!r.ok) return;
    const s = await r.json();

    const stConn = $('st_conn');
    const stSsid = $('st_ssid');
    const stIp   = $('st_ip');
    if(stConn) stConn.textContent = s.connected ? 'Ja' : 'Nein';
    if(stSsid) stSsid.textContent = s.ssid || '—';
    if(stIp)   stIp.textContent   = s.ip || '—';

    const goWrap = $('go');
    const goIp   = $('go_ip');
    const goMdns = $('go_mdns');

    if(goWrap) goWrap.style.display = (s.connected ? 'flex' : 'none');
    if(goIp && s.ip) goIp.href = 'http://' + s.ip + '/';
    if(goMdns) goMdns.href = 'http://' + (s.host || 'multi-sensor') + '.local/';

    // Redirect nur, wenn wir gerade gespeichert haben
    if(_afterSave && s.connected && s.ip && s.ip !== '0.0.0.0'){
      _afterSave = false;
      if(_pollTimer){ clearInterval(_pollTimer); _pollTimer = null; }
      setTimeout(()=>{ window.location.href = 'http://' + s.ip + '/'; }, 700);
    }
  } catch(e){
    // still: kein spam im UI
  }
}

async function wifiConnectTo(ssid){
  const msg = $('msg');
  let pass = prompt('Passwort für ' + ssid + ' (leer lassen für offen)');
  if(pass === null) return;

  const body = new URLSearchParams();
  body.set('ssid', ssid);
  body.set('pass', pass);

  if(msg) msg.textContent = 'Speichere & verbinde...';
  const r = await fetch('/api/wifi/save', {
    method:'POST',
    headers:{'Content-Type':'application/x-www-form-urlencoded'},
    body
  });

  const t = await r.text();
  if(msg) msg.textContent = t;

  _afterSave = true;
  if(!_pollTimer) _pollTimer = setInterval(wifiPollStatus, 1200);
}

async function wifiScan(){
  const msg  = $('msg');
  const list = $('list');
  if(msg) msg.textContent = 'Scanne...';
  if(list) list.innerHTML = '';

  try{
    const r = await fetch('/api/wifi/scan', { cache:'no-store' });
    const ct = (r.headers.get('content-type') || '').toLowerCase();

    if(!r.ok){
      const t = await r.text();
      if(msg) msg.textContent = `Scan fehlgeschlagen (${r.status}): ${t.substring(0,120)}`;
      return;
    }
    if(!ct.includes('application/json')){
      const t = await r.text();
      if(msg) msg.textContent = `Scan: unerwartete Antwort (${ct}): ${t.substring(0,120)}`;
      return;
    }

    const j = await r.json();
    if(msg) msg.textContent = 'Gefunden: ' + j.length;

    if(!j.length){
      if(list) list.innerHTML = '<div class="card"><p class="small">Keine Netzwerke gefunden.</p></div>';
      return;
    }

    let h = '<div class="card"><h2>Netzwerke</h2><table class="tbl">';
    h += '<tr><th>SSID</th><th>RSSI</th><th>Aktion</th></tr>';

    j.forEach(n=>{
      const ssid = n.ssid || '';
      const ssidEsc = ssid.replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;').replace(/"/g,'&quot;');
      const ssidJs  = ssid.replace(/'/g,"\\'");
      h += `<tr>
        <td><b>${ssidEsc}</b></td>
        <td>${n.rssi} dBm</td>
        <td><button class="btn-primary" type="button" onclick="wifiConnectTo('${ssidJs}')">Verbinden</button></td>
      </tr>`;
    });

    h += '</table></div>';
    if(list) list.innerHTML = h;

  } catch(e){
    if(msg) msg.textContent = 'JS Fehler beim Scan: ' + (e && e.message ? e.message : e);
  }
}

// Expose für onclick
window.wifiScan = wifiScan;
window.wifiConnectTo = wifiConnectTo;

window.addEventListener('load', ()=>{
  wifiPollStatus();
  setInterval(wifiPollStatus, 3000);
});
