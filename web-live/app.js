// RIPoker_Solver live viewer - talks to the embedded HTTP server to configure
// stacks, train, and query the solver's current strategy for any position.

// Actions/order come from /api/actions (loadActionConfig). Check/Call/Fold
// get a fixed color; bets/raises/all-in are colored by real chip size below.
let ACTION_ORDER = [];
let ACTION_LABELS = {};
const FIXED_COLORS = {
  Check: 'var(--action-check)',
  Call: 'var(--action-call)',
  Fold: 'var(--action-fold)',
};

function hexToRgb(hex) {
  const n = parseInt(hex.replace('#', ''), 16);
  return [(n >> 16) & 255, (n >> 8) & 255, n & 255];
}

function rgbToHex(rgb) {
  return '#' + rgb.map((v) => Math.round(v).toString(16).padStart(2, '0')).join('');
}

function lerpHex(startHex, endHex, t) {
  const a = hexToRgb(startHex);
  const b = hexToRgb(endHex);
  return rgbToHex(a.map((v, i) => v + (b[i] - v) * t));
}

// Colors a bet/raise/all-in by chip size, linear (y=mx+b) across the red ramp:
// smallest present bet = red, largest (all-in) = darkest. `sizes`: action->chips.
function actionColor(action, sizes) {
  if (FIXED_COLORS[action]) return FIXED_COLORS[action];
  sizes = sizes || {};
  const size = sizes[action];
  if (size === undefined) return '#888';
  const vals = Object.values(sizes);
  const mn = Math.min(...vals), mx = Math.max(...vals);
  const t = mx > mn ? (size - mn) / (mx - mn) : 0;
  const style = getComputedStyle(document.documentElement);
  const start = style.getPropertyValue('--bet-ramp-start').trim();
  const end = style.getPropertyValue('--bet-ramp-end').trim();
  return lerpHex(start, end, t);
}

// "Bet 40"/"Raise to 100" from the backend-provided chip size; falls back to
// the abstract label ("Bet 50%") when no size was resolved (bucketed mode).
function actionLabel(action, size) {
  if (size === undefined || size === null) return ACTION_LABELS[action];
  if (action[0] === 'B') return `Bet ${size}`;
  if (action[0] === 'R') return `Raise to ${size}`;
  return ACTION_LABELS[action];
}

// Per-position context (aggressive-action sizes + the legal action set),
// collected once in renderPosition() and shared by the bars and legend.
let currentActionSizes = {};
let currentPresent = new Set();

function collectActionSizes(pos) {
  const sizes = {};
  for (const h of pos.hands) {
    if (!h) continue;
    for (const s of h.strategy) {
      if (s.size !== undefined && s.size !== null) sizes[s.action] = s.size;
    }
  }
  return sizes;
}

function collectPresentActions(pos) {
  const present = new Set();
  for (const h of pos.hands) if (h) for (const s of h.strategy) present.add(s.action);
  return present;
}

async function loadActionConfig() {
  const res = await fetch('/api/actions');
  const cfg = await res.json();
  ACTION_ORDER = cfg.actionOrder.slice();
  // Move Allin to the end so bars/legend read passive -> bet sizes -> all-in.
  const allinIdx = ACTION_ORDER.indexOf('Allin');
  if (allinIdx !== -1) ACTION_ORDER.push(ACTION_ORDER.splice(allinIdx, 1)[0]);
  ACTION_LABELS = Object.assign({}, cfg.actionLabels);
}

const STAGE_NAMES = ['Preflop', 'Flop', 'Turn'];
const RANK_LABELS = ['2', '3', '4', '5', '6', '7'];

function setStatus(id, text, kind) {
  const el = document.getElementById(id);
  el.textContent = text;
  el.className = 'status' + (kind ? ' ' + kind : '');
}

function renderBar(strategy) {
  const byAction = new Map(strategy.map((s) => [s.action, s.prob]));
  const sizeByAction = new Map(strategy.map((s) => [s.action, s.size]));
  let html = '<div class="bar">';
  for (const action of ACTION_ORDER) {
    const prob = byAction.get(action);
    if (!prob || prob <= 0.001) continue;
    const pct = (prob * 100).toFixed(1);
    const label = actionLabel(action, sizeByAction.get(action));
    html += `<div class="seg" style="width:${pct}%;background:${actionColor(action, currentActionSizes)}" data-tip="${label}: ${pct}%"></div>`;
  }
  html += '</div>';
  return html;
}

function buildLegend() {
  let html = '<div class="legend">';
  for (const action of ACTION_ORDER) {
    if (!currentPresent.has(action)) continue;
    html += `<span><span class="swatch" style="background:${actionColor(action, currentActionSizes)}"></span>${actionLabel(action, currentActionSizes[action])}</span>`;
  }
  html += '</div>';
  return html;
}

function renderPosition(pos) {
  currentActionSizes = collectActionSizes(pos);
  currentPresent = collectPresentActions(pos);

  const board = pos.board.map((b) => b ?? '?').join('  ');
  const moneyLabel = `Pot: ${pos.pot} &middot; To call: ${pos.bet}`;
  let html = '';
  html += `<div class="position-meta"><b>${STAGE_NAMES[pos.stage] || '?'}</b> &middot; Board: ${board} &middot; ${moneyLabel} &middot; History: ${pos.history || '(none)'} &middot; total visits: ${pos.visits}</div>`;
  html += buildLegend();

  // pos.hands is always 6 entries (index = hand value), each an object or null
  pos.hands.forEach((hand, i) => {
    html += '<div class="hand-row">';
    if (!hand) {
      html += `<div class="rank">${RANK_LABELS[i] ?? '?'}</div><div class="null-strategy">null</div><div class="visits"></div>`;
    } else {
      html += `<div class="rank">${hand.rank}</div>${renderBar(hand.strategy)}<div class="visits">${hand.visits} visits</div>`;
    }
    html += '</div>';
  });

  document.getElementById('results').innerHTML = html;
}

async function fetchPosition(history, board0, board1) {
  const params = new URLSearchParams({ history: history || '' });
  if (board0) params.set('board0', board0);
  if (board1) params.set('board1', board1);
  const res = await fetch('/api/position?' + params.toString());
  return res.json();
}

async function fetchRandomPosition() {
  const res = await fetch('/api/random-position');
  return res.json();
}

async function postConfigure(stack0, stack1) {
  const res = await fetch('/api/configure', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ stack0, stack1 }),
  });
  return res.json();
}

async function postTrain(iterations) {
  const res = await fetch('/api/train', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ iterations }),
  });
  return res.json();
}

async function postCancelTrain() {
  const res = await fetch('/api/train/cancel', { method: 'POST' });
  return res.json();
}

function currentPositionInputs() {
  return {
    history: document.getElementById('history').value.trim(),
    board0: document.getElementById('board0').value.trim(),
    board1: document.getElementById('board1').value.trim(),
  };
}

async function refreshCurrentPosition() {
  const { history, board0, board1 } = currentPositionInputs();
  const pos = await fetchPosition(history, board0, board1);
  renderPosition(pos);
}

// Tracks the in-flight training run so Save/Cancel can stop it and wait -
// the server has no locking, so a concurrent Reset() would be a real race.
let trainingCancelRequested = false;
let currentTrainingPromise = null;

async function runTraining(iterations) {
  const btn = document.getElementById('trainBtn');
  const cancelBtn = document.getElementById('cancelTrainBtn');
  const track = document.getElementById('trainProgressTrack');
  const fill = document.getElementById('trainProgressFill');

  // Chunked so the progress bar and strategy bars refresh every CHUNK_SIZE
  // hands. Cancellation is server-side per-hand, so it's near-instant anyway.
  const CHUNK_SIZE = 25;
  const chunkSize = Math.min(CHUNK_SIZE, Math.max(1, iterations));

  btn.disabled = true;
  cancelBtn.disabled = false;
  track.style.display = 'block';
  fill.style.width = '0%';
  let trained = 0;
  let lastResult = null;
  try {
    while (trained < iterations) {
      if (trainingCancelRequested) break;
      const batch = Math.min(chunkSize, iterations - trained);
      lastResult = await postTrain(batch);
      trained += lastResult.trained; // may be less than `batch` if cancelled mid-chunk
      const pct = Math.min(100, Math.round((trained / iterations) * 100));
      fill.style.width = pct + '%';
      await refreshCurrentPosition();
      await refreshExploitChart();
      if (lastResult.cancelled) break;
      setStatus('trainStatus', `Training… ${trained}/${iterations} hands (${pct}%)`);
    }
    if (trainingCancelRequested || (lastResult && lastResult.cancelled)) {
      setStatus('trainStatus', `Stopped after ${trained}/${iterations} hands.`, 'error');
    } else {
      setStatus('trainStatus', `Done. Total hands trained: ${lastResult.iteration}, infosets: ${lastResult.infosets}.`, 'ok');
    }
  } catch (e) {
    setStatus('trainStatus', 'Training failed: ' + e, 'error');
  } finally {
    btn.disabled = false;
    cancelBtn.disabled = true;
    trainingCancelRequested = false;
    currentTrainingPromise = null;
  }
}

// Cancel/Save helper: stops the current run (server-side) and waits for the
// in-flight request to finish before the caller proceeds.
async function cancelTraining(statusElId) {
  if (!currentTrainingPromise) return;
  trainingCancelRequested = true;
  if (statusElId) setStatus(statusElId, 'Stopping training…');
  await postCancelTrain();
  await currentTrainingPromise;
}

document.getElementById('cancelTrainBtn').addEventListener('click', () => {
  cancelTraining('trainStatus');
});

document.getElementById('saveStacksBtn').addEventListener('click', async () => {
  await cancelTraining('stacksStatus');

  // Both players get the same stack (single effective-stack knob).
  const stack = parseInt(document.getElementById('stack').value, 10);
  setStatus('stacksStatus', 'Saving…');
  try {
    await postConfigure(stack, stack);
    document.getElementById('history').value = '';
    document.getElementById('board0').value = '';
    document.getElementById('board1').value = '';
    setStatus('stacksStatus', `Saved. All trained data wiped (effective stack: ${stack}).`, 'ok');
    await refreshCurrentPosition();
    await refreshExploitChart();
  } catch (e) {
    setStatus('stacksStatus', 'Failed to save: ' + e, 'error');
  }
});

document.getElementById('trainBtn').addEventListener('click', () => {
  if (currentTrainingPromise) return; // already running
  const iterations = parseInt(document.getElementById('iterations').value, 10);
  currentTrainingPromise = runTraining(iterations);
});

document.getElementById('lookupBtn').addEventListener('click', async () => {
  setStatus('positionStatus', 'Looking up…');
  try {
    await refreshCurrentPosition();
    setStatus('positionStatus', '');
  } catch (e) {
    setStatus('positionStatus', 'Lookup failed: ' + e, 'error');
  }
});

document.getElementById('randomBtn').addEventListener('click', async () => {
  setStatus('positionStatus', 'Picking a random position…');
  try {
    const pos = await fetchRandomPosition();
    document.getElementById('history').value = pos.history;
    document.getElementById('board0').value = pos.board[0] || '';
    document.getElementById('board1').value = pos.board[1] || '';
    renderPosition(pos);
    setStatus('positionStatus', '');
  } catch (e) {
    setStatus('positionStatus', 'Random pick failed: ' + e, 'error');
  }
});

// --- Exploitability chart: single-series SVG line, one point per 1,000 hands ---
let exploitPoints = [];

async function refreshExploitChart() {
  try {
    const res = await fetch('/api/exploitability');
    exploitPoints = (await res.json()).points || [];
  } catch (e) { /* keep the last good series */ }
  renderExploitChart();
}

function fmtIter(n) {
  return n >= 1000 ? n / 1000 + 'K' : String(n);
}

// 1/2/5 stepping for clean axis ticks.
function niceStep(range, targetTicks) {
  const raw = range / Math.max(1, targetTicks);
  const mag = Math.pow(10, Math.floor(Math.log10(raw)));
  for (const m of [1, 2, 5, 10]) if (raw <= m * mag) return m * mag;
  return 10 * mag;
}

function renderExploitChart() {
  const host = document.getElementById('exploitChart');
  if (!host) return;
  if (exploitPoints.length === 0) {
    host.innerHTML = '<div class="exploit-empty">No samples yet — a point is added every 1,000 trained hands.</div>';
    return;
  }

  const W = host.clientWidth || 600;
  const H = host.clientHeight || 120;
  const M = { l: 46, r: 64, t: 8, b: 18 };
  const iw = W - M.l - M.r;
  const ih = H - M.t - M.b;

  const maxIter = exploitPoints[exploitPoints.length - 1].iter;
  const maxVal = Math.max(...exploitPoints.map((p) => p.value), 0);
  const yStep = niceStep(maxVal || 1, 3);
  const yMax = Math.max(yStep, Math.ceil((maxVal || 1) / yStep) * yStep);
  const x = (it) => M.l + (it / maxIter) * iw;
  const y = (v) => M.t + ih - (v / yMax) * ih;

  const css = getComputedStyle(document.documentElement);
  const accent = css.getPropertyValue('--accent').trim();
  const grid = css.getPropertyValue('--gridline').trim();
  const muted = css.getPropertyValue('--text-muted').trim();
  const secondary = css.getPropertyValue('--text-secondary').trim();
  const surface = css.getPropertyValue('--surface-1').trim();

  let s = `<svg width="${W}" height="${H}" font-family="inherit">`;

  // Recessive hairline grid + left tick labels (clean 1/2/5 values).
  for (let v = 0; v <= yMax + 1e-9; v += yStep) {
    s += `<line x1="${M.l}" y1="${y(v)}" x2="${M.l + iw}" y2="${y(v)}" stroke="${grid}" stroke-width="1"/>`;
    s += `<text x="${M.l - 6}" y="${y(v) + 3.5}" text-anchor="end" font-size="10" fill="${muted}" style="font-variant-numeric:tabular-nums">${+v.toFixed(4)}</text>`;
  }
  const xStep = niceStep(maxIter, 5);
  for (let it = xStep; it <= maxIter + 1e-9; it += xStep) {
    s += `<text x="${x(it)}" y="${M.t + ih + 13}" text-anchor="middle" font-size="10" fill="${muted}" style="font-variant-numeric:tabular-nums">${fmtIter(it)}</text>`;
  }

  // Area wash + 2px line, both in the single series hue.
  const pts = exploitPoints.map((p) => `${x(p.iter).toFixed(1)},${y(p.value).toFixed(1)}`);
  if (exploitPoints.length > 1) {
    s += `<path d="M${pts.join('L')}L${x(maxIter).toFixed(1)},${y(0)}L${x(exploitPoints[0].iter).toFixed(1)},${y(0)}Z" fill="${accent}" opacity="0.1"/>`;
    s += `<path d="M${pts.join('L')}" fill="none" stroke="${accent}" stroke-width="2" stroke-linejoin="round" stroke-linecap="round"/>`;
  }

  // End dot with a surface ring, plus a direct label for the latest value.
  const last = exploitPoints[exploitPoints.length - 1];
  s += `<circle cx="${x(last.iter)}" cy="${y(last.value)}" r="4" fill="${accent}" stroke="${surface}" stroke-width="2"/>`;
  s += `<text x="${x(last.iter) + 8}" y="${y(last.value) + 3.5}" font-size="11" fill="${secondary}" style="font-variant-numeric:tabular-nums">${last.value.toFixed(3)}</text>`;

  // Crosshair (snaps to the nearest sample) + transparent hover layer.
  s += `<line id="exCross" x1="0" y1="${M.t}" x2="0" y2="${M.t + ih}" stroke="${muted}" stroke-width="1" visibility="hidden"/>`;
  s += `<circle id="exHoverDot" r="4" fill="${accent}" stroke="${surface}" stroke-width="2" visibility="hidden"/>`;
  s += `<rect x="${M.l}" y="${M.t}" width="${iw}" height="${ih}" fill="transparent"/>`;
  s += '</svg><div class="exploit-tip" id="exTip"></div>';
  host.innerHTML = s;

  const svg = host.querySelector('svg');
  const cross = host.querySelector('#exCross');
  const hoverDot = host.querySelector('#exHoverDot');
  const tip = host.querySelector('#exTip');

  svg.addEventListener('pointermove', (ev) => {
    const px = ev.clientX - host.getBoundingClientRect().left;
    let best = exploitPoints[0];
    for (const p of exploitPoints) if (Math.abs(x(p.iter) - px) < Math.abs(x(best.iter) - px)) best = p;
    cross.setAttribute('x1', x(best.iter));
    cross.setAttribute('x2', x(best.iter));
    cross.setAttribute('visibility', 'visible');
    hoverDot.setAttribute('cx', x(best.iter));
    hoverDot.setAttribute('cy', y(best.value));
    hoverDot.setAttribute('visibility', 'visible');
    tip.innerHTML = '';
    const b = document.createElement('b');
    b.textContent = best.value.toFixed(4);
    tip.appendChild(b);
    tip.appendChild(document.createTextNode(` chips/hand · hand ${best.iter.toLocaleString()}`));
    tip.style.display = 'block';
    const flip = x(best.iter) > W - 150;
    tip.style.left = flip ? '' : x(best.iter) + 10 + 'px';
    tip.style.right = flip ? W - x(best.iter) + 10 + 'px' : '';
    tip.style.top = Math.max(0, y(best.value) - 30) + 'px';
  });
  svg.addEventListener('pointerleave', () => {
    cross.setAttribute('visibility', 'hidden');
    hoverDot.setAttribute('visibility', 'hidden');
    tip.style.display = 'none';
  });
}

window.addEventListener('resize', renderExploitChart);

// Initial load: fetch the action config once, then show the default
// preflop root position.
(async function initLive() {
  await loadActionConfig();
  await refreshCurrentPosition();
  await refreshExploitChart();
})();
