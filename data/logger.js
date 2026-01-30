(function(){
  const canvas = document.getElementById('c');
  const ctx = canvas.getContext('2d');
  const err = document.getElementById('err');

  function selMetrics(){
    return Array.from(document.querySelectorAll('.m:checked')).map(x => x.value);
  }
  function qs(obj){
    return Object.keys(obj).map(k => k + '=' + encodeURIComponent(obj[k])).join('&');
  }
  function showErr(msg){ err.hidden=false; err.textContent=msg; }
  function hideErr(){ err.hidden=true; err.textContent=''; }
  function clear(){ ctx.clearRect(0,0,canvas.width,canvas.height); }

  const colors = { temp:'#1f77b4', hum:'#2ca02c', press:'#ff7f0e', co2:'#d62728' };

  const mode = payload.mode || 'line';

function drawChart(payload){
  clear();

  const W = canvas.width, H = canvas.height;
  const padL=48, padR=16, padT=14, padB=34;
  const plotW = W - padL - padR;
  const plotH = H - padT - padB;

  const labels = payload.labels || [];
  const series = payload.series || {};
  const mode   = payload.mode || 'line';   // ✅ HIER richtig

  // min/max
  let ymin=Infinity, ymax=-Infinity, any=false;
  Object.values(series).forEach(arr=>{
    arr.forEach(v=>{
      if(v==null || isNaN(v)) return;
      ymin=Math.min(ymin,v);
      ymax=Math.max(ymax,v);
      any=true;
    });
  });

  if(!any || labels.length<2){
    ctx.fillText('Keine Daten',20,30);
    return;
  }

  if(ymin===ymax){ ymin-=1; ymax+=1; }

  const xAt = i => padL + (i/(labels.length-1))*plotW;
  const yAt = v => padT + (1-(v-ymin)/(ymax-ymin))*plotH;

  /* ===== Achsen ===== */
  ctx.strokeStyle='#cfd3d7';
  ctx.beginPath();
  ctx.moveTo(padL,padT);
  ctx.lineTo(padL,padT+plotH);
  ctx.lineTo(padL+plotW,padT+plotH);
  ctx.stroke();

  /* ===== Zeichenfunktionen ===== */

  function drawLines(){
    Object.keys(series).forEach(key=>{
      const arr = series[key];
      ctx.strokeStyle = colors[key] || '#000';
      ctx.lineWidth=2;
      ctx.beginPath();
      let started=false;
      arr.forEach((v,i)=>{
        if(v==null || isNaN(v)) return;
        const x=xAt(i), y=yAt(v);
        if(!started){ctx.moveTo(x,y);started=true;}
        else ctx.lineTo(x,y);
      });
      ctx.stroke();
    });
  }

  function drawBars(){
    const keys = Object.keys(series);
    const n = labels.length;
    const groupW = plotW/n;
    const gap = Math.max(2,groupW*0.15);
    const barW = (groupW-gap)/keys.length;

    for(let i=0;i<n;i++){
      const x0 = padL + i*groupW + gap/2;
      keys.forEach((key,ki)=>{
        const v = series[key][i];
        if(v==null || isNaN(v)) return;
        ctx.fillStyle = colors[key]||'#000';
        ctx.fillRect(
          x0+ki*barW,
          yAt(v),
          barW-1,
          (padT+plotH)-yAt(v)
        );
      });
    }
  }

  /* ===== HIER ist das gesuchte if ===== */
  if (mode === 'bar') drawBars();
  else drawLines();
}


async function load(){
  hideErr();
  const range  = document.getElementById('range').value;
  const chart  = document.getElementById('chart').value;
  const metrics = selMetrics();

  if (metrics.length === 0){
    showErr('Bitte mindestens einen Wert auswählen.');
    clear();
    return;
  }

  try{
    const r = await fetch('/api/history?' + qs({
      range,
      chart,
      metrics: metrics.join(',')
    }));
    if (!r.ok) throw new Error('HTTP ' + r.status);
    const data = await r.json();
    drawChart(data);
  }catch(e){
    showErr('Konnte Daten nicht laden: ' + e.message);
    clear();
  }
}


document.getElementById('range').addEventListener('change', load);
document.getElementById('chart').addEventListener('change', load); // <- neu
document.querySelectorAll('.m').forEach(x => x.addEventListener('change', load));

  load();
})();
