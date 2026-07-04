// RIPoker_Solver viewer - loads solver_export.json (auto-fetch or manual file
// picker) and lets you click through the solved tree the way a GTO trainer
// does: pick an action, drill into the resulting node, repeat.

// The bet/raise sizing grid is configurable on the backend (see game.cpp's
// bet_sizings/raise_sizings), so the set and order of actions is read from
// data.actionOrder/data.actionLabels (populated in init()) rather than
// hardcoded here. Only Check/Call/Fold/Allin get a fixed color - every other
// action is treated as a bet/raise and colored by interpolating across
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

// Real chip size behind each bet/raise at whichever node is currently
// rendered - the same for every hand in that node, so it's collected once in
// buildNodeCard() and reused by the legend/action buttons/picker built from
// it, rather than threaded through every function's parameters.
let currentActionSizes = {};

function collectActionSizes(node) {
  const sizes = {};
  for (const h of node.hands) {
    for (const s of h.strategy) {
      if (s.size !== undefined && s.size !== null) sizes[s.action] = s.size;
    }
  }
  return sizes;
}

let data = null;
let historyIndex = null;
let pathStack = [];

function stageLabel(stage) {
  return ['Preflop', 'Flop', 'Turn'][stage] || '?';
}

function buildHistoryIndex(nodes) {
  const idx = new Map();
  for (const n of nodes) {
    const key = n.stage + '|' + n.history;
    if (!idx.has(key)) idx.set(key, []);
    idx.get(key).push(n);
  }
  return idx;
}

function findChildren(current, actionName) {
  const letter = data.actionLetters[actionName];
  if (letter === undefined) return [];
  const targetHistory = current.history + letter;
  const sameStage = historyIndex.get(current.stage + '|' + targetHistory) || [];
  const nextStage = historyIndex.get((current.stage + 1) + '|' + (targetHistory + ',')) || [];
  return sameStage.concat(nextStage);
}

function candidateLabel(current, candidate) {
  if (candidate.stage > current.stage) {
    const slot = current.stage; // 0: preflop->flop reveals board[0]; 1: flop->turn reveals board[1]
    return 'Board: ' + (candidate.board[slot] ?? '?');
  }
  return `Pot ${candidate.pot}, to call ${candidate.bet}`;
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

function buildLegend() {
  const el = document.createElement('div');
  el.className = 'legend';
  for (const action of ACTION_ORDER) {
    const span = document.createElement('span');
    span.innerHTML = `<span class="swatch" style="background:${actionColor(action)}"></span>${actionLabel(action, currentActionSizes[action])}`;
    el.appendChild(span);
  }
  return el;
}

function buildHandTable(node) {
  const hands = [...node.hands].sort((a, b) => {
    const ra = parseInt(a.rank, 10), rb = parseInt(b.rank, 10);
    if (ra !== rb) return ra - rb;
    return (a.flushCategory ?? 0) - (b.flushCategory ?? 0);
  });

  const table = document.createElement('table');
  table.className = 'hands';
  table.innerHTML = '<thead><tr><th>Hand</th><th>Strategy</th><th>Visits</th></tr></thead>';
  const tbody = document.createElement('tbody');
  for (const h of hands) {
    const tr = document.createElement('tr');
    const flushHtml = h.flushLabel ? `<span class="flush">${h.flushLabel}</span>` : '';
    tr.innerHTML = `
      <td class="rank">${h.rank}${flushHtml}</td>
      <td>${renderBar(h.strategy)}</td>
      <td class="visits">${h.visits}</td>
    `;
    tbody.appendChild(tr);
  }
  table.appendChild(tbody);
  return table;
}

function buildActionButtons(node) {
  const wrap = document.createElement('div');
  wrap.className = 'action-group';

  const title = document.createElement('div');
  title.className = 'title';
  title.textContent = 'Continue with:';
  wrap.appendChild(title);

  const actionSet = new Set();
  for (const h of node.hands) for (const s of h.strategy) actionSet.add(s.action);

  const buttons = document.createElement('div');
  buttons.className = 'action-buttons';
  let any = false;
  for (const action of ACTION_ORDER) {
    if (!actionSet.has(action)) continue;
    const children = findChildren(node, action);
    const btn = document.createElement('button');
    btn.textContent = actionLabel(action, currentActionSizes[action]);
    btn.disabled = children.length === 0;
    if (children.length > 0) {
      any = true;
      btn.addEventListener('click', () => handleActionClick(node, action, children));
    }
    buttons.appendChild(btn);
  }
  wrap.appendChild(buttons);

  if (!any) {
    const empty = document.createElement('div');
    empty.className = 'empty';
    empty.textContent = 'No further data past this point (not enough training visits were exported for it).';
    wrap.appendChild(empty);
  }

  return wrap;
}

function handleActionClick(node, action, children) {
  const label = actionLabel(action, currentActionSizes[action]);
  if (children.length === 1) {
    navigateTo(label, children[0]);
    return;
  }
  const content = document.getElementById('content');
  const picker = document.createElement('div');
  picker.className = 'node-card';
  const title = document.createElement('div');
  title.className = 'meta';
  title.textContent = `Multiple outcomes observed after "${label}" - pick one:`;
  picker.appendChild(title);

  const pickerRow = document.createElement('div');
  pickerRow.className = 'picker';
  const sorted = [...children].sort((a, b) => b.visits - a.visits);
  for (const c of sorted) {
    const candLabel = candidateLabel(node, c);
    const btn = document.createElement('button');
    btn.textContent = `${candLabel} (${c.visits} visits)`;
    btn.addEventListener('click', () => navigateTo(`${label} → ${candLabel}`, c));
    pickerRow.appendChild(btn);
  }
  picker.appendChild(pickerRow);
  content.innerHTML = '';
  content.appendChild(picker);
}

function navigateTo(label, node) {
  pathStack.push({ label, node });
  render();
}

function buildNodeCard(node) {
  currentActionSizes = collectActionSizes(node);

  const card = document.createElement('div');
  card.className = 'node-card';

  const meta = document.createElement('div');
  meta.className = 'meta';
  const boardStr = node.board.map((b) => b ?? '?').join('  ');
  meta.innerHTML = `<b>${stageLabel(node.stage)}</b> &middot; Board: ${boardStr} &middot; Pot: ${node.pot} &middot; To call: ${node.bet} &middot; visits: ${node.visits}`;
  card.appendChild(meta);

  const historyMeta = document.createElement('div');
  historyMeta.className = 'meta';
  historyMeta.textContent = 'History code: ' + (node.history || '(none)');
  card.appendChild(historyMeta);

  card.appendChild(buildLegend());

  if (node.hands.length === 0) {
    const empty = document.createElement('div');
    empty.className = 'empty';
    empty.textContent = 'No hand data recorded for this node.';
    card.appendChild(empty);
  } else {
    card.appendChild(buildHandTable(node));
  }

  card.appendChild(buildActionButtons(node));
  return card;
}

function renderBreadcrumb() {
  const el = document.getElementById('breadcrumb');
  el.innerHTML = '';
  pathStack.forEach((step, i) => {
    if (i > 0) {
      const sep = document.createElement('span');
      sep.className = 'sep';
      sep.textContent = '›';
      el.appendChild(sep);
    }
    const btn = document.createElement('button');
    btn.textContent = step.label;
    btn.disabled = i === pathStack.length - 1;
    btn.addEventListener('click', () => {
      pathStack = pathStack.slice(0, i + 1);
      render();
    });
    el.appendChild(btn);
  });
}

function render() {
  renderBreadcrumb();
  const node = pathStack[pathStack.length - 1].node;
  const content = document.getElementById('content');
  content.innerHTML = '';
  content.appendChild(buildNodeCard(node));
}

function init(parsed) {
  data = parsed;
  ACTION_ORDER = data.actionOrder.slice();
  // Allin is the largest possible action but the backend lists it right
  // after Fold (it's grouped with the other "misc" actions there) - move it
  // to the end so the bar/legend read left-to-right as: passive actions,
  // then every bet/raise size in ascending order, then all-in.
  const allinIdx = ACTION_ORDER.indexOf('Allin');
  if (allinIdx !== -1) ACTION_ORDER.push(ACTION_ORDER.splice(allinIdx, 1)[0]);
  ACTION_LABELS = Object.assign({}, data.actionLabels);
  historyIndex = buildHistoryIndex(data.nodes);
  const root = (historyIndex.get('0|') || [])[0];
  if (!root) {
    document.getElementById('content').innerHTML =
      '<div class="empty">No preflop root node found in this export (try training more hands, or lowering the export visit threshold).</div>';
    return;
  }
  pathStack = [{ label: 'Preflop', node: root }];
  document.getElementById('loader').style.display = 'none';
  document.getElementById('breadcrumb').style.display = 'flex';
  render();
}

fetch('solver_export.json')
  .then((r) => {
    if (!r.ok) throw new Error('not found');
    return r.json();
  })
  .then(init)
  .catch(() => {
    document.getElementById('loader-status').textContent =
      'Could not auto-load solver_export.json (serve this folder over http and place the file alongside index.html, or pick it manually below).';
  });

document.getElementById('fileInput').addEventListener('change', (e) => {
  const file = e.target.files[0];
  if (!file) return;
  const reader = new FileReader();
  reader.onload = () => init(JSON.parse(reader.result));
  reader.readAsText(file);
});
