(function(){
  const canvas = document.getElementById('c');
  const ctx = canvas.getContext('2d');
  const err = document.getElementById('err');

  const meta = {
    temp:  { color:'#1f77b4', label:'Temperatur', unit:'°C'  },
    hum:   { color:'#2ca02c', label:'Luftfeuchte', unit:'%'  },
    press: { color:'#ff7f0e', label:'Luftdruck', unit:'hPa'  },
    co2:   { color:'#d62728', label:'CO₂', unit:'ppm'        }
  };

  function selMetrics(){
    return Array.from(document.querySelectorAll('.m:checked')).map(x => x.value);
  }
  function qs(obj){
    return Object.keys(obj).map(k => k + '=' + encodeURIComponent(obj[k])).join('&');
  }
  function showErr(msg){ err.hidden=false; err.textContent=msg; }
  function hideErr(){ err.hidden=true; err.textContent=''; }
  function clear(){ ctx.clearRect(0,0,canvas.width,canvas.height); }

  function drawChart(payload){
    clear();

    const W = canvas.width, H = canvas.height;
    const padL=52, padR=16, padT=14, padB=36;
    const plotW = W - padL - padR;
    const plotH = H - padT - padB;

    const labels = payload.labels || [];
    const series = payload.series || {};
    const mode   = payload.mode || 'line';

    const keys = Object.keys(series);

    // min/max über ALLE ausgewählten Serien (Achtung: unterschiedliche Einheiten!)
    let ymin=Infinity, ymax=-Infinity, any=false;
    keys.forEach(k=>{
      (series[k]||[]).forEach(v=>{
        if (v == null || Number.isNaN(v)) return;
        ymin = Math.min(ymin, v);
        ymax = Math.max(ymax, v);
        any = true;
      });
    });

    if (!any || labels.length < 2){
      ctx.fillStyle = '#333';
      ctx.font = '14px system-ui, sans-serif';
      ctx.fillText('Keine Daten', 20, 30);
      return;
    }

    if (ymin === ymax){ ymin -= 1; ymax += 1; }

    const xAt = (i)=> padL + (i/(labels.length-1))*plotW;
    const yAt = (v)=> padT + (1 - (v - ymin)/(ymax - ymin))*plotH;

    // Achsen
    ctx.strokeStyle = '#cfd3d7';
    ctx.beginPath();
    ctx.moveTo(padL, padT);
    ctx.lineTo(padL, padT+plotH);
    ctx.lineTo(padL+plotW, padT+plotH);
    ctx.stroke();

    // y Ticks
    ctx.fillStyle = '#333';
    ctx.font = '12px system-ui, sans-serif';
    for(let t=0;t<=2;t++){
      const v = ymin + (t/2)*(ymax-ymin);
      const y = yAt(v);
      ctx.fillText(v.toFixed(1), 6, y+4);
      ctx.strokeStyle = '#eee';
      ctx.beginPath();
      ctx.moveTo(padL, y);
      ctx.lineTo(padL+plotW, y);
      ctx.stroke();
    }

    // x label start/end
    const l0 = labels[0] || '';
    const l1 = labels[labels.length-1] || '';
    ctx.fillStyle = '#333';
    ctx.fillText(l0, padL, H-10);
    const tw = ctx.measureText(l1).width;
    ctx.fillText(l1, W-padR-tw, H-10);

    function drawLines(){
      keys.forEach(key=>{
        const arr = series[key] || [];
        ctx.strokeStyle = (meta[key]?.color) || '#000';
        ctx.lineWidth = 2;
        ctx.beginPath();
        let started=false;
        for(let i=0;i<arr.length;i++){
          const v = arr[i];
          if (v == null || Number.isNaN(v)) continue;
          const x = xAt(i);
          const y = yAt(v);
          if (!started){ ctx.moveTo(x,y); started=true; }
          else ctx.lineTo(x,y);
        }
        ctx.stroke();
      });
    }

    function drawBars(){
      const n = labels.length;
      const k = keys.length;
      if (n <= 0 || k <= 0) return;

      const groupW = plotW / n;

      // sauberes Innen-Padding -> verhindert Überlappung
      const inner = Math.max(3, groupW * 0.18);           // links/rechts im Slot
      const avail = Math.max(2, groupW - inner*2);
      const barW  = avail / k;
      const barGap = Math.max(1, barW * 0.10);
      const barDrawW = Math.max(1, barW - barGap);

      for(let i=0;i<n;i++){
        const slotX = padL + i*groupW + inner;

        keys.forEach((key, ki)=>{
          const arr = series[key] || [];
          const v = arr[i];
          if (v == null || Number.isNaN(v)) return;

          const x = slotX + ki*barW + barGap/2;
          const y = yAt(v);
          const h = (padT + plotH) - y;

          ctx.fillStyle = (meta[key]?.color) || '#000';
          ctx.fillRect(x, y, barDrawW, h);
        });
      }
    }

    // Legende mit Einheit
    let lx = padL, ly = padT + 12;
    keys.forEach(key=>{
      const c = (meta[key]?.color) || '#000';
      const name = (meta[key]?.label) || key;
      const unit = (meta[key]?.unit) ? ` (${meta[key].unit})` : '';

      ctx.fillStyle = c;
      ctx.fillRect(lx, ly-8, 10, 3);
      ctx.fillStyle = '#333';
      ctx.fillText(name + unit, lx+14, ly-2);
      lx += ctx.measureText(name + unit).width + 30;
    });

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
      const r = await fetch('/api/history?' + qs({ range, chart, metrics: metrics.join(',') }));
      if (!r.ok) throw new Error('HTTP ' + r.status);
      const data = await r.json();
      drawChart(data);
    }catch(e){
      showErr('Konnte Daten nicht laden: ' + e.message);
      clear();
    }
  }

  document.getElementById('range').addEventListener('change', load);
  document.getElementById('chart').addEventListener('change', load);
  document.querySelectorAll('.m').forEach(x => x.addEventListener('change', load));
  load();
})();
