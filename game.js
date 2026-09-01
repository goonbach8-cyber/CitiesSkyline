const canvas = document.getElementById('game');
const ctx = canvas.getContext('2d');
const moneyEl = document.getElementById('money');
const populationEl = document.getElementById('population');
const dayEl = document.getElementById('day');
const pauseBtn = document.getElementById('pauseBtn');
const speedBtn = document.getElementById('speedBtn');
const resetBtn = document.getElementById('resetBtn');

const CELL = 32;
const COLS = 42;
const ROWS = 28;
const costs = { road: 120, residential: 450, commercial: 650, industrial: 700, bulldoze: 50 };
const refund = { road: 30, residential: 110, commercial: 160, industrial: 175 };

let selectedTool = 'road';
let money = 100000;
let population = 0;
let day = 1;
let paused = false;
let speed = 1;
let tickAccumulator = 0;
let lastTime = performance.now();

const grid = Array.from({ length: ROWS }, () =>
  Array.from({ length: COLS }, () => ({ type: 'empty', level: 0, residents: 0 }))
);

function resizeCanvas() {
  const rect = canvas.parentElement.getBoundingClientRect();
  canvas.width = Math.max(900, Math.floor(rect.width));
  canvas.height = Math.max(600, Math.floor(rect.height));
}
window.addEventListener('resize', resizeCanvas);
resizeCanvas();

function updateHUD() {
  moneyEl.textContent = Math.floor(money).toLocaleString('de-CH');
  populationEl.textContent = population.toLocaleString('de-CH');
  dayEl.textContent = day;
  pauseBtn.textContent = paused ? 'Weiter' : 'Pause';
  speedBtn.textContent = `${speed}x`;
}

function colorFor(type) {
  if (type === 'road') return '#444';
  if (type === 'residential') return '#7b9f76';
  if (type === 'commercial') return '#6483a8';
  if (type === 'industrial') return '#a18a5d';
  return '#1f2b22';
}

function hasAdjacentRoad(x, y) {
  const n = [[1,0],[-1,0],[0,1],[0,-1]];
  return n.some(([dx,dy]) => {
    const nx = x + dx, ny = y + dy;
    return nx >= 0 && nx < COLS && ny >= 0 && ny < ROWS && grid[ny][nx].type === 'road';
  });
}

function draw() {
  ctx.clearRect(0, 0, canvas.width, canvas.height);

  const totalW = COLS * CELL;
  const totalH = ROWS * CELL;
  const ox = Math.max(18, (canvas.width - totalW) / 2);
  const oy = Math.max(18, (canvas.height - totalH) / 2);

  ctx.fillStyle = '#18231b';
  ctx.fillRect(ox, oy, totalW, totalH);

  for (let y = 0; y < ROWS; y++) {
    for (let x = 0; x < COLS; x++) {
      const cell = grid[y][x];
      const px = ox + x * CELL;
      const py = oy + y * CELL;

      ctx.fillStyle = colorFor(cell.type);
      ctx.fillRect(px + 1, py + 1, CELL - 2, CELL - 2);

      if (cell.type === 'road') {
        ctx.fillStyle = '#666';
        ctx.fillRect(px + CELL / 2 - 2, py + 3, 4, CELL - 6);
        ctx.fillRect(px + 3, py + CELL / 2 - 2, CELL - 6, 4);
      }

      if (['residential','commercial','industrial'].includes(cell.type)) {
        const margin = 6;
        ctx.fillStyle = cell.type === 'residential' ? '#c7d7bf' : cell.type === 'commercial' ? '#b8cce5' : '#d6c59e';
        ctx.fillRect(px + margin, py + margin, CELL - margin * 2, CELL - margin * 2);
        ctx.fillStyle = '#222';
        ctx.fillRect(px + 11, py + 13, 5, 8);
        ctx.fillRect(px + 19, py + 10, 4, 11);
      }

      ctx.strokeStyle = 'rgba(255,255,255,.055)';
      ctx.strokeRect(px, py, CELL, CELL);
    }
  }
}

function getCellFromMouse(e) {
  const rect = canvas.getBoundingClientRect();
  const sx = canvas.width / rect.width;
  const sy = canvas.height / rect.height;
  const mx = (e.clientX - rect.left) * sx;
  const my = (e.clientY - rect.top) * sy;
  const totalW = COLS * CELL;
  const totalH = ROWS * CELL;
  const ox = Math.max(18, (canvas.width - totalW) / 2);
  const oy = Math.max(18, (canvas.height - totalH) / 2);
  const x = Math.floor((mx - ox) / CELL);
  const y = Math.floor((my - oy) / CELL);
  if (x < 0 || x >= COLS || y < 0 || y >= ROWS) return null;
  return { x, y };
}

function placeAt(x, y) {
  const cell = grid[y][x];

  if (selectedTool === 'bulldoze') {
    if (cell.type === 'empty') return;
    if (money < costs.bulldoze) return;
    money -= costs.bulldoze;
    money += refund[cell.type] || 0;
    cell.type = 'empty';
    cell.residents = 0;
    recalcPopulation();
    updateHUD();
    return;
  }

  if (cell.type !== 'empty') return;
  const cost = costs[selectedTool];
  if (money < cost) return;

  if (['residential','commercial','industrial'].includes(selectedTool) && !hasAdjacentRoad(x, y)) return;

  money -= cost;
  cell.type = selectedTool;
  cell.level = 1;
  if (selectedTool === 'residential') cell.residents = 2;
  recalcPopulation();
  updateHUD();
}

canvas.addEventListener('mousedown', (e) => {
  if (e.button !== 0) return;
  const c = getCellFromMouse(e);
  if (c) placeAt(c.x, c.y);
});

canvas.addEventListener('contextmenu', e => e.preventDefault());

document.querySelectorAll('[data-tool]').forEach(btn => {
  btn.addEventListener('click', () => {
    selectedTool = btn.dataset.tool;
    document.querySelectorAll('[data-tool]').forEach(b => b.classList.toggle('active', b === btn));
  });
});

pauseBtn.addEventListener('click', () => { paused = !paused; updateHUD(); });
speedBtn.addEventListener('click', () => {
  speed = speed === 1 ? 2 : speed === 2 ? 4 : 1;
  updateHUD();
});

resetBtn.addEventListener('click', () => {
  for (const row of grid) for (const cell of row) {
    cell.type = 'empty'; cell.level = 0; cell.residents = 0;
  }
  money = 100000; population = 0; day = 1; paused = false; speed = 1;
  updateHUD();
});

function recalcPopulation() {
  population = grid.flat().reduce((sum, c) => sum + (c.residents || 0), 0);
}

function simulationTick() {
  day++;
  let income = 0;
  for (let y = 0; y < ROWS; y++) {
    for (let x = 0; x < COLS; x++) {
      const c = grid[y][x];
      if (c.type === 'residential' && hasAdjacentRoad(x,y)) {
        const cap = 10 + c.level * 5;
        if (c.residents < cap && Math.random() < 0.55) c.residents += 1;
        income += c.residents * 3;
      } else if (c.type === 'commercial' && hasAdjacentRoad(x,y)) {
        income += 18 + Math.min(population, 500) * 0.03;
      } else if (c.type === 'industrial' && hasAdjacentRoad(x,y)) {
        income += 25;
      }
    }
  }
  money += income;
  recalcPopulation();
  updateHUD();
}

function loop(now) {
  const dt = Math.min(100, now - lastTime);
  lastTime = now;
  if (!paused) {
    tickAccumulator += dt * speed;
    while (tickAccumulator >= 1700) {
      simulationTick();
      tickAccumulator -= 1700;
    }
  }
  draw();
  requestAnimationFrame(loop);
}

updateHUD();
requestAnimationFrame(loop);
