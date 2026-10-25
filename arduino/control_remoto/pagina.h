#pragma once

// Página de control remoto. Todo va en línea: la red del carro no tiene internet,
// así que no se puede cargar nada de afuera (fuentes, librerías, íconos).
const char PAGINA[] = R"rawliteral(<!doctype html>
<html lang="es">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1, user-scalable=no">
<title>Control remoto</title>
<style>
  :root {
    --fondo: #f4f5f7; --panel: #ffffff; --texto: #1c1e21; --suave: #6b7280;
    --borde: #e3e5e8; --acento: #2563eb; --ok: #16a34a; --mal: #dc2626;
    --negro: #111827; --base: #e8ebf0; --perilla: #2563eb;
  }
  @media (prefers-color-scheme: dark) {
    :root {
      --fondo: #0f1115; --panel: #181b21; --texto: #e7e9ec; --suave: #9aa1ab;
      --borde: #2a2f37; --acento: #60a5fa; --ok: #22c55e; --mal: #f87171;
      --negro: #f3f4f6; --base: #232832; --perilla: #3b82f6;
    }
  }
  * { box-sizing: border-box; }
  html, body { margin: 0; background: var(--fondo); color: var(--texto);
    font: 15px/1.4 -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
    -webkit-user-select: none; user-select: none; -webkit-tap-highlight-color: transparent; }
  main { max-width: 460px; margin: 0 auto; padding: 16px; display: flex; flex-direction: column; gap: 14px; }
  header { display: flex; align-items: center; justify-content: space-between; }
  h1 { font-size: 18px; margin: 0; font-weight: 650; }
  .estado { display: flex; align-items: center; gap: 8px; font-size: 13px; color: var(--suave); }
  .punto { width: 10px; height: 10px; border-radius: 50%; background: var(--mal); }
  .punto.ok { background: var(--ok); }
  .panel { background: var(--panel); border: 1px solid var(--borde); border-radius: 14px; padding: 14px; }
  .sensores { display: grid; grid-template-columns: repeat(3, 1fr); gap: 12px; }
  .sensor { display: flex; flex-direction: column; align-items: center; gap: 6px; font-size: 13px; }
  .barra { width: 100%; height: 64px; background: var(--base); border-radius: 8px; position: relative; overflow: hidden; }
  .relleno { position: absolute; left: 0; right: 0; bottom: 0; background: var(--acento); transition: height .15s; }
  .sensor.negro .relleno { background: var(--negro); }
  .valor { font-variant-numeric: tabular-nums; font-weight: 600; }
  .etiqueta { color: var(--suave); }
  .zona-joy { display: flex; justify-content: center; padding: 6px 0; }
  .joy { width: min(72vw, 270px); aspect-ratio: 1; border-radius: 50%; background: var(--base);
    border: 1px solid var(--borde); position: relative; touch-action: none; cursor: grab; }
  .joy::before, .joy::after { content: ""; position: absolute; background: var(--borde); }
  .joy::before { left: 50%; top: 12%; bottom: 12%; width: 1px; }
  .joy::after { top: 50%; left: 12%; right: 12%; height: 1px; }
  .perilla { width: 36%; aspect-ratio: 1; border-radius: 50%; background: var(--perilla);
    position: absolute; left: 32%; top: 32%; box-shadow: 0 4px 14px rgba(0,0,0,.25); z-index: 1; }
  .control { display: grid; grid-template-columns: 112px 1fr 48px; align-items: center; gap: 10px; font-size: 14px; }
  .control + .control { margin-top: 10px; }
  .control output { text-align: right; font-variant-numeric: tabular-nums; color: var(--suave); }
  input[type=range] { width: 100%; accent-color: var(--acento); }
  .stop { width: 100%; padding: 18px; font-size: 20px; font-weight: 700; letter-spacing: .04em;
    border: 0; border-radius: 14px; background: var(--mal); color: #fff; cursor: pointer; }
  .stop:active { filter: brightness(.9); }
  .ruedas { display: flex; justify-content: space-between; font-size: 13px; color: var(--suave); }
  .ayuda { font-size: 12px; color: var(--suave); text-align: center; }
</style>
</head>
<body>
<main>
  <header>
    <h1>Agente laberinto</h1>
    <div class="estado"><span id="txtEstado">sin conexión</span><span class="punto" id="punto"></span></div>
  </header>

  <section class="panel sensores" aria-label="Sensores">
    <div class="sensor" id="sIzq"><div class="barra"><div class="relleno"></div></div><span class="valor">–</span><span class="etiqueta">Izq</span></div>
    <div class="sensor" id="sCen"><div class="barra"><div class="relleno"></div></div><span class="valor">–</span><span class="etiqueta">Centro</span></div>
    <div class="sensor" id="sDer"><div class="barra"><div class="relleno"></div></div><span class="valor">–</span><span class="etiqueta">Der</span></div>
  </section>

  <div class="zona-joy"><div class="joy" id="joy"><div class="perilla" id="perilla"></div></div></div>

  <div class="ruedas"><span>Izq: <b id="vIzq">0</b></span><span>Der: <b id="vDer">0</b></span></div>

  <section class="panel">
    <div class="control"><label for="vel">Velocidad</label><input id="vel" type="range" min="20" max="100" value="60"><output id="oVel">60%</output></div>
    <div class="control"><label for="trim">Compensación</label><input id="trim" type="range" min="-25" max="25" value="0"><output id="oTrim">0</output></div>
  </section>

  <button class="stop" id="stop">STOP</button>
  <p class="ayuda">Arrastra el joystick o usa las flechas / WASD. Espacio = STOP.<br>
  Si el carro se va hacia un lado, mueve la compensación hacia el otro.</p>
</main>

<script>
const UMBRAL_NEGRO = 300;   // escala 0-1000, igual que el firmware
const $ = id => document.getElementById(id);
const joy = $('joy'), perilla = $('perilla'), vel = $('vel'), trim = $('trim');
const st = { x: 0, y: 0, arrastrando: false, teclas: new Set() };

// ---- joystick ----
function moverPerilla(x, y) {
  const r = joy.clientWidth * 0.32;
  perilla.style.transform = `translate(${x * r}px, ${-y * r}px)`;
}
function desdePuntero(e) {
  const b = joy.getBoundingClientRect(), R = b.width / 2;
  let x = (e.clientX - b.left - R) / (R * 0.68), y = -(e.clientY - b.top - R) / (R * 0.68);
  const m = Math.hypot(x, y);
  if (m > 1) { x /= m; y /= m; }
  st.x = x; st.y = y; moverPerilla(x, y);
}
function soltar() {
  st.arrastrando = false; st.x = 0; st.y = 0; moverPerilla(0, 0);
}
joy.addEventListener('pointerdown', e => { joy.setPointerCapture(e.pointerId); st.arrastrando = true; desdePuntero(e); });
joy.addEventListener('pointermove', e => { if (st.arrastrando) desdePuntero(e); });
['pointerup', 'pointercancel', 'lostpointercapture'].forEach(ev => joy.addEventListener(ev, soltar));

// ---- teclado ----
const MAPA = { ArrowUp: 'u', KeyW: 'u', ArrowDown: 'd', KeyS: 'd', ArrowLeft: 'l', KeyA: 'l', ArrowRight: 'r', KeyD: 'r' };
function desdeTeclas() {
  if (st.arrastrando) return;
  const t = st.teclas;
  const y = (t.has('u') ? 1 : 0) - (t.has('d') ? 1 : 0);
  const x = (t.has('r') ? 1 : 0) - (t.has('l') ? 1 : 0);
  // girando con avance: curva suave; sin avance: giro en el sitio a media potencia
  st.x = y ? x * 0.5 : x * 0.6; st.y = y;
  moverPerilla(st.x, st.y);
}
addEventListener('keydown', e => {
  if (e.code === 'Space') { e.preventDefault(); detener(); return; }
  const k = MAPA[e.code]; if (!k) return;
  e.preventDefault(); st.teclas.add(k); desdeTeclas();
});
addEventListener('keyup', e => { const k = MAPA[e.code]; if (k) { st.teclas.delete(k); desdeTeclas(); } });

// ---- mezcla diferencial ----
function mezcla() {
  let izq = st.y + st.x, der = st.y - st.x;
  const m = Math.max(1, Math.abs(izq), Math.abs(der));
  izq /= m; der /= m;
  const v = vel.value / 100, c = trim.value / 100;
  // compensación > 0 frena la rueda derecha (corrige un carro que se va a la izquierda);
  // < 0 frena la izquierda (corrige uno que se va a la derecha)
  der *= v * (c > 0 ? 1 - c : 1);
  izq *= v * (c < 0 ? 1 + c : 1);
  return { d: Math.round(der * 100), i: Math.round(izq * 100) };
}

// ---- envío ----
let enVuelo = false, ultimo = '', ultimaRespuesta = 0;
async function enviar() {
  const { d, i } = mezcla();
  $('vIzq').textContent = i; $('vDer').textContent = d;
  const q = `d=${d}&i=${i}`;
  const quieto = d === 0 && i === 0;
  if (enVuelo || (quieto && ultimo === q)) return;   // en movimiento se repite siempre (hombre muerto)
  enVuelo = true;
  try {
    const r = await fetch('/m?' + q, { cache: 'no-store' });
    if (r.ok) { ultimaRespuesta = Date.now(); ultimo = q; }
  } catch (_) { ultimo = ''; }
  finally { enVuelo = false; }
}
setInterval(enviar, 100);

function detener() {
  st.teclas.clear(); soltar(); ultimo = '';
  fetch('/stop', { cache: 'no-store' }).catch(() => {});
}
$('stop').addEventListener('click', detener);
addEventListener('blur', detener);
document.addEventListener('visibilitychange', () => { if (document.hidden) detener(); });

vel.addEventListener('input', () => $('oVel').textContent = vel.value + '%');
trim.addEventListener('input', () => $('oTrim').textContent = (trim.value > 0 ? '→ ' : trim.value < 0 ? '← ' : '') + Math.abs(trim.value));

// ---- sensores y estado ----
function pintarSensor(id, v) {
  const el = $(id);
  el.querySelector('.relleno').style.height = (v / 10) + '%';
  el.querySelector('.valor').textContent = v;
  el.classList.toggle('negro', v > UMBRAL_NEGRO);
}
let leyendo = false;
async function estado() {
  if (leyendo) return;
  leyendo = true;
  try {
    const r = await fetch('/s', { cache: 'no-store' });
    const s = await r.json();
    ultimaRespuesta = Date.now();
    pintarSensor('sIzq', s.izq); pintarSensor('sCen', s.cen); pintarSensor('sDer', s.der);
    if (s.reset === 'BROWNOUT') $('txtEstado').textContent = 'conectado · último reinicio: batería baja';
    else $('txtEstado').textContent = 'conectado · ' + s.uptime + ' s';
  } catch (_) {}
  finally { leyendo = false; }
}
setInterval(estado, 300);
setInterval(() => {
  const ok = Date.now() - ultimaRespuesta < 1500;
  $('punto').classList.toggle('ok', ok);
  if (!ok) $('txtEstado').textContent = 'sin conexión';
}, 500);
</script>
</body>
</html>
)rawliteral";
