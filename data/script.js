// ===== UI: Darstellung (Chips + Validierung) =====
(function(){
  const ALL_IDS = ["live","system","network","sensor","memory","time","settings"];

  function normList(str){
    return (str || "")
      .split(",")
      .map(s => s.trim())
      .filter(s => s.length > 0);
  }

  function uniq(list){
    const seen = new Set();
    const out = [];
    for(const x of list){
      if(!seen.has(x)){ seen.add(x); out.push(x); }
    }
    return out;
  }

  function setInput(id, list){
    const el = document.getElementById(id);
    if(el) el.value = list.join(",");
  }

  function renderChips(chipsId, inputId){
    const wrap = document.getElementById(chipsId);
    const input = document.getElementById(inputId);
    if(!wrap || !input) return;

    wrap.innerHTML = "";
    const cur = uniq(normList(input.value));

    for(const id of ALL_IDS){
      const on = cur.includes(id);
      const el = document.createElement("div");
      el.className = "chip" + (on ? " on" : "");
      el.textContent = id;

      el.addEventListener("click", () => {
        const now = uniq(normList(input.value));
        const idx = now.indexOf(id);
        if(idx >= 0) now.splice(idx, 1);
        else now.push(id);
        setInput(inputId, now);
        renderChips(chipsId, inputId);
        validate();
      });

      wrap.appendChild(el);
    }
  }

  function validate(){
    const rootEl = document.getElementById("ui_root_order");
    const infoEl = document.getElementById("ui_info_order");
    if(!rootEl || !infoEl) return;

    const root = uniq(normList(rootEl.value));
    const info = uniq(normList(infoEl.value));

    const rootHint = document.getElementById("ui_root_hint");
    const infoHint = document.getElementById("ui_info_hint");

    const badRoot = root.filter(x => !ALL_IDS.includes(x));
    const badInfo = info.filter(x => !ALL_IDS.includes(x));

    if(rootHint){
      if(badRoot.length){
        rootHint.className = "small hint-bad";
        rootHint.textContent = "Unbekannte ID(s): " + badRoot.join(", ");
      } else {
        rootHint.className = "small hint-ok";
        rootHint.textContent = "OK (" + root.length + " Block/Blöcke)";
      }
    }

    if(infoHint){
      if(badInfo.length){
        infoHint.className = "small hint-bad";
        infoHint.textContent = "Unbekannte ID(s): " + badInfo.join(", ");
      } else {
        infoHint.className = "small hint-ok";
        infoHint.textContent = "OK (" + info.length + " Block/Blöcke)";
      }
    }
  }

  function applyPreset(){
    const p = document.getElementById("ui_preset")?.value || "";
    const root = document.getElementById("ui_root_order");
    const info = document.getElementById("ui_info_order");
    if(!root || !info) return;

    if(p === "standard"){
      root.value = "live,network,time";
      info.value = "system,network,sensor,memory,time,settings";
    } else if(p === "minimal"){
      root.value = "live";
      info.value = "sensor,network,time";
    } else if(p === "debug"){
      root.value = "live,system,memory,network,time,settings";
      info.value = "system,memory,network,time,sensor,settings";
    }

    renderChips("chips_root", "ui_root_order");
    renderChips("chips_info", "ui_info_order");
    validate();
  }

  function setupDarstellungUI(){
    // Nur aktivieren, wenn wir auf der Settings-Seite die Controls haben
    if(!document.getElementById("ui_root_order") || !document.getElementById("ui_info_order")) return;

    renderChips("chips_root", "ui_root_order");
    renderChips("chips_info", "ui_info_order");
    validate();

    // Preset-Button
    const btn = document.getElementById("ui_preset_apply");
    if(btn) btn.addEventListener("click", applyPreset);

    // Live-Update beim Tippen
    document.addEventListener("input", (e)=>{
      if(e.target && (e.target.id === "ui_root_order" || e.target.id === "ui_info_order")){
        renderChips("chips_root", "ui_root_order");
        renderChips("chips_info", "ui_info_order");
        validate();
      }
    });
  }

  window.addEventListener("load", setupDarstellungUI);
})();
