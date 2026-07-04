// RIPoker_Solver live viewer - talks to the embedded HTTP server (src/server.cpp)
// to configure stacks, trigger training, and query the solver's current
// in-memory strategy for any position.

// The bet/raise sizing grid is configurable on the backend (see game.cpp's
// bet_sizings/raise_sizings), so the set and order of actions is fetched
// from /api/actions (see loadActionConfig()) rather than hardcoded here.
// Only Check/Call/Fold/Allin get a fixed color - every other action is
// treated as a bet/raise and colored by interpolating across
// --bet-ramp-start/--bet-ramp-end according to its rank among however many
// bet/raise sizes are actually present.
let ACTION_ORDER = [];
let ACTION_LABELS = {};
const FIXED_COLORS = {
  Check: 'var(--action-check)',
  Call: 'var(--action-call)',
  Fold: 'var(--action-fold)',
  Allin: 'var(--action-allin)',
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

function actionColor(action) {
  if (FIXED_COLORS[action]) return FIXED_COLORS[action];
  const rampActions = ACTION_ORDER.filter((a) => !FIXED_COLORS[a]);
  const idx = rampActions.indexOf(action);
  if (idx === -1) return '#888';
  const t = rampActions.length > 1 ? idx / (rampActions.length - 1) : 0;
  const style = getComputedStyle(document.documentElement);
  const start = style.getPropertyValue('--bet-ramp-start').trim();
  const end = style.getPropertyValue('--bet-ramp-end').trim();
  return lerpHex(start, end, t);
}

// The backend reports the actual chip amount behind a bet/raise (replayed
// from the real pot/bet_states at that position - see
// Game::ReplayExactHistory) whenever it can, rather than the abstract sizing
// name - show that real number instead of "Bet 50%"/"Raise 2.2x" wherever
// it's available. Falls back to the abstract label if the backend couldn't
// resolve a size (e.g. history recorded in bucketed rather than exact mode).
function actionLabel(action, size) {
  if (size === undefined || size === null) return ACTION_LABELS[action];
  if (action[0] === 'B') return `Bet ${size}`;
  if (action[0] === 'R') return `Raise to ${size}`;
  return ACTION_LABELS[action];
}

// Real chip size behind each bet/raise at whichever position is currently
// shown - the same for every hand there, so it's collected once in
// renderPosition() and reused by the legend built from it.
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

async function loadActionConfig() {
  const res = await fetch('/api/actions');
  const cfg = await res.json();
  ACTION_ORDER = cfg.actionOrder.slice();
  // Allin is the largest possible action but the backend lists it right
  // after Fold (it's grouped with the other "misc" actions there) - move it
  // to the end so the bar/legend read left-to-right as: passive actions,
  // then every bet/raise size in ascending order, then all-in.
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
    html += `<div class="seg" style="width:${pct}%;background:${actionColor(action)}" data-tip="${label}: ${pct}%"></div>`;
  }
  html += '</div>';
  return html;
}

function buildLegend(actionSizes) {
  let html = '<div class="legend">';
  for (const action of ACTION_ORDER) {
    html += `<span><span class="swatch" style="background:${actionColor(action)}"></span>${actionLabel(action, actionSizes[action])}</span>`;
  }
  html += '</div>';
  return html;
}

function renderPosition(pos) {
  document.getElementById('useBetSizeBuckets').checked = pos.useBetSizeBuckets;

  const board = pos.board.map((b) => b ?? '?').join('  ');
  const moneyLabel = `Pot: ${pos.pot} &middot; To call: ${pos.bet}`;
  let html = '';
  html += `<div class="position-meta"><b>${STAGE_NAMES[pos.stage] || '?'}</b> &middot; Board: ${board} &middot; ${moneyLabel} &middot; History: ${pos.history || '(none)'} &middot; total visits: ${pos.visits}</div>`;
  html += buildLegend(collectActionSizes(pos));

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

async function postConfigure(stack0, stack1, useBetSizeBuckets) {
  const res = await fetch('/api/configure', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ stack0, stack1, useBetSizeBuckets }),
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

// Tracks the in-flight training run (if any) so Save/Cancel can stop it and
// wait for the current chunk's request to actually finish before doing
// anything else - the server has no locking around the shared solver state,
// so a /api/configure Reset() running concurrently with an in-flight
// /api/train batch would be a real race, not just a UX glitch.
let trainingCancelRequested = false;
let currentTrainingPromise = null;

async function runTraining(iterations) {
  const btn = document.getElementById('trainBtn');
  const cancelBtn = document.getElementById('cancelTrainBtn');
  const track = document.getElementById('trainProgressTrack');
  const fill = document.getElementById('trainProgressFill');

  // Chunked into a series of smaller sequential requests so the progress bar
  // and strategy bars refresh every CHUNK_SIZE hands, no matter how large the
  // total run is. Cancellation itself doesn't depend on this chunking though -
  // the server checks its own cancel flag every single TrainCFR() call (see
  // src/server.cpp's /api/train), so a cancel takes effect within about one
  // hand's training time even in the middle of a chunk, not just between them.
  const CHUNK_SIZE = 200;
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

// Shared by the Cancel button and Save: stops the current run (server-side,
// so it takes effect immediately even mid-chunk) and waits for the
// in-flight request to actually finish before the caller proceeds.
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

  // Both players get the same stack - the solver still supports asymmetric
  // stacks internally, but a single effective-stack figure is the only
  // knob that matters for typical heads-up analysis.
  const stack = parseInt(document.getElementById('stack').value, 10);
  const useBetSizeBuckets = document.getElementById('useBetSizeBuckets').checked;
  setStatus('stacksStatus', 'Saving…');
  try {
    await postConfigure(stack, stack, useBetSizeBuckets);
    document.getElementById('history').value = '';
    document.getElementById('board0').value = '';
    document.getElementById('board1').value = '';
    setStatus('stacksStatus', `Saved. All trained data wiped (effective stack: ${stack}, bet-size buckets: ${useBetSizeBuckets}).`, 'ok');
    await refreshCurrentPosition();
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

// Initial load: fetch the action config once, then show the default
// preflop root position.
(async function initLive() {
  await loadActionConfig();
  await refreshCurrentPosition();
})();
