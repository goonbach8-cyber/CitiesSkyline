const canvas=document.getElementById('game'),ctx=canvas.getContext('2d');
const moneyEl=document.getElementById('money'),populationEl=document.getElementById('population'),jobsEl=document.getElementById('jobs'),dayEl=document.getElementById('day');
const pauseBtn=document.getElementById('pauseBtn'),speedBtn=document.getElementById('speedBtn'),saveBtn=document.getElementById('saveBtn'),loadBtn=document.getElementById('loadBtn'),resetBtn=document.getElementById('resetBtn');
const demandR=document.getElementById('demandR'),demandC=document.getElementById('demandC'),demandI=document.getElementById('demandI'),toast=document.getElementById('toast');

const CELL=30,COLS=90,ROWS=70;
const cost={road:120,residential:40,commercial:55,industrial:55,bulldoze:50};
let grid=[],selectedTool='road',money=100000,population=0,jobs=0,day=1,paused=false,speed=1,lastTime=performance.now(),tickAcc=0;
let camX=0,camY=0,zoom=1,dragging=false,dragStart=null,lastMouse=null,panning=false,spaceDown=false;
let demands={r:70,c:40,i:45};

function freshGrid(){grid=Array.from({length:ROWS},()=>Array.from({length:COLS},()=>({type:'empty',zone:null,built:false,level:0,residents:0,jobs:0})));}
freshGrid();

function resize(){const r=canvas.parentElement.getBoundingClientRect();canvas.width=Math.max(300,Math.floor(r.width));canvas.height=Math.max(300,Math.floor(r.height));}
window.addEventListener('resize',resize);resize();

function notify(msg){toast.textContent=msg;toast.classList.add('show');clearTimeout(notify.t);notify.t=setTimeout(()=>toast.classList.remove('show'),1400);}
function clamp(v,a,b){return Math.max(a,Math.min(b,v));}
function updateHUD(){
  moneyEl.textContent=Math.floor(money).toLocaleString('de-CH');
  populationEl.textContent=population.toLocaleString('de-CH');
  jobsEl.textContent=jobs.toLocaleString('de-CH');
  dayEl.textContent=day; pauseBtn.textContent=paused?'Weiter':'Pause'; speedBtn.textContent=`${speed}x`;
  demandR.style.width=`${demands.r}%`; demandC.style.width=`${demands.c}%`; demandI.style.width=`${demands.i}%`;
}
function screenToWorld(sx,sy){return {x:(sx-canvas.width/2)/zoom+camX,y:(sy-canvas.height/2)/zoom+camY};}
function worldToScreen(wx,wy){return {x:(wx-camX)*zoom+canvas.width/2,y:(wy-camY)*zoom+canvas.height/2};}
function mouseCell(e){const r=canvas.getBoundingClientRect(),sx=(e.clientX-r.left)*(canvas.width/r.width),sy=(e.clientY-r.top)*(canvas.height/r.height),w=screenToWorld(sx,sy);const x=Math.floor(w.x/CELL),y=Math.floor(w.y/CELL);return x>=0&&x<COLS&&y>=0&&y<ROWS?{x,y}:null;}
function adjacentRoad(x,y){return [[1,0],[-1,0],[0,1],[0,-1]].some(([dx,dy])=>{const nx=x+dx,ny=y+dy;return nx>=0&&ny>=0&&nx<COLS&&ny<ROWS&&grid[ny][nx].type==='road'});}
function roadNeighbors(x,y){return [[1,0],[-1,0],[0,1],[0,-1]].map(([dx,dy])=>{const nx=x+dx,ny=y+dy;return nx>=0&&ny>=0&&nx<COLS&&ny<ROWS&&grid[ny][nx].type==='road';});}

function paintCells(a,b){
  if(!a||!b)return;
  const minX=Math.min(a.x,b.x),maxX=Math.max(a.x,b.x),minY=Math.min(a.y,b.y),maxY=Math.max(a.y,b.y);
  if(selectedTool==='road'){
    const dx=Math.abs(b.x-a.x),dy=Math.abs(b.y-a.y);
    if(dx>=dy){const y=a.y;for(let x=minX;x<=maxX;x++)applyTool(x,y);}
    else{const x=a.x;for(let y=minY;y<=maxY;y++)applyTool(x,y);}
  }else{
    for(let y=minY;y<=maxY;y++)for(let x=minX;x<=maxX;x++)applyTool(x,y);
  }
}

function applyTool(x,y){
  const c=grid[y][x];
  if(selectedTool==='bulldoze'){
    if(c.type==='empty'&&!c.zone)return;
    if(money<cost.bulldoze)return;
    money-=cost.bulldoze;c.type='empty';c.zone=null;c.built=false;c.level=0;c.residents=0;c.jobs=0;return;
  }
  if(selectedTool==='road'){
    if(c.type==='road')return;if(money<cost.road)return;
    money-=cost.road;c.type='road';c.zone=null;c.built=false;c.residents=0;c.jobs=0;return;
  }
  if(c.type!=='empty'||c.zone===selectedTool)return;
  if(money<cost[selectedTool])return;
  money-=cost[selectedTool];c.zone=selectedTool;
}

function recalc(){population=0;jobs=0;for(const row of grid)for(const c of row){population+=c.residents||0;jobs+=c.jobs||0;}}

function simulate(){
  day++;
  const unemployment=Math.max(0,population-jobs),freeJobs=Math.max(0,jobs-population);
  demands.r=clamp(50+Math.min(35,freeJobs*.5)-Math.min(35,unemployment*.35)+Math.random()*10-5,5,95);
  demands.c=clamp(30+Math.min(45,population/8)+Math.random()*12-6,5,90);
  demands.i=clamp(35+Math.min(35,population/10)-Math.min(20,jobs/40)+Math.random()*10-5,5,90);

  for(let y=0;y<ROWS;y++)for(let x=0;x<COLS;x++){
    const c=grid[y][x];
    if(c.zone&&!c.built&&adjacentRoad(x,y)){
      const d=c.zone==='residential'?demands.r:c.zone==='commercial'?demands.c:demands.i;
      if(Math.random()<d/900){c.built=true;c.type=c.zone;c.level=1;if(c.type==='residential')c.residents=4+Math.floor(Math.random()*5);else c.jobs=c.type==='commercial'?5+Math.floor(Math.random()*5):8+Math.floor(Math.random()*8);}
    }else if(c.built){
      if(c.type==='residential'){
        if(c.residents<18&&jobs>population&&Math.random()<.22)c.residents++;
        if(c.residents>2&&jobs+15<population&&Math.random()<.10)c.residents--;
        money+=c.residents*.85;
      }else if(c.type==='commercial')money+=c.jobs*1.25;
      else if(c.type==='industrial')money+=c.jobs*1.05;
    }
  }
  recalc();updateHUD();
}

function drawRoad(px,py,x,y){
  ctx.fillStyle='#414741';ctx.fillRect(px,py,CELL,CELL);
  const [r,l,d,u]=roadNeighbors(x,y);ctx.strokeStyle='#727970';ctx.lineWidth=Math.max(1,zoom);
  ctx.beginPath();const cx=px+CELL/2,cy=py+CELL/2;
  if(r){ctx.moveTo(cx,cy);ctx.lineTo(px+CELL,cy)} if(l){ctx.moveTo(cx,cy);ctx.lineTo(px,cy)} if(d){ctx.moveTo(cx,cy);ctx.lineTo(cx,py+CELL)} if(u){ctx.moveTo(cx,cy);ctx.lineTo(cx,py)}
  if(!r&&!l&&!d&&!u){ctx.moveTo(px+4,cy);ctx.lineTo(px+CELL-4,cy)}ctx.stroke();
}
function drawBuilding(c,px,py){
  const palette=c.type==='residential'?['#9db59f','#c9d9ca']:c.type==='commercial'?['#7799b6','#b8cee0']:['#a88f67','#d2c09b'];
  ctx.fillStyle=palette[0];ctx.fillRect(px+4,py+4,CELL-8,CELL-8);
  ctx.fillStyle=palette[1];ctx.fillRect(px+7,py+7,CELL-14,CELL-14);
  ctx.fillStyle='rgba(20,25,22,.55)';ctx.fillRect(px+10,py+11,4,6);ctx.fillRect(px+17,py+8,4,9);
}
function draw(){
  ctx.clearRect(0,0,canvas.width,canvas.height);ctx.fillStyle='#132016';ctx.fillRect(0,0,canvas.width,canvas.height);
  ctx.save();ctx.translate(canvas.width/2,canvas.height/2);ctx.scale(zoom,zoom);ctx.translate(-camX,-camY);
  const minX=clamp(Math.floor((camX-canvas.width/(2*zoom))/CELL)-2,0,COLS-1),maxX=clamp(Math.ceil((camX+canvas.width/(2*zoom))/CELL)+2,0,COLS-1);
  const minY=clamp(Math.floor((camY-canvas.height/(2*zoom))/CELL)-2,0,ROWS-1),maxY=clamp(Math.ceil((camY+canvas.height/(2*zoom))/CELL)+2,0,ROWS-1);
  for(let y=minY;y<=maxY;y++)for(let x=minX;x<=maxX;x++){
    const c=grid[y][x],px=x*CELL,py=y*CELL;
    ctx.fillStyle=((x+y)%2===0)?'#27402d':'#29452f';ctx.fillRect(px,py,CELL,CELL);
    if(c.zone&&!c.built){ctx.fillStyle=c.zone==='residential'?'rgba(92,175,102,.38)':c.zone==='commercial'?'rgba(72,122,201,.38)':'rgba(197,161,66,.38)';ctx.fillRect(px+1,py+1,CELL-2,CELL-2);}
    if(c.type==='road')drawRoad(px,py,x,y);
    else if(c.built)drawBuilding(c,px,py);
    ctx.strokeStyle='rgba(255,255,255,.035)';ctx.lineWidth=.6;ctx.strokeRect(px,py,CELL,CELL);
  }
  ctx.strokeStyle='rgba(255,255,255,.16)';ctx.strokeRect(0,0,COLS*CELL,ROWS*CELL);
  ctx.restore();
}

canvas.addEventListener('mousedown',e=>{
  lastMouse={x:e.clientX,y:e.clientY};
  if(e.button===1||spaceDown){panning=true;return;}
  if(e.button!==0)return;
  dragging=true;dragStart=mouseCell(e);
});
window.addEventListener('mousemove',e=>{
  if(panning&&lastMouse){camX-=(e.clientX-lastMouse.x)/zoom;camY-=(e.clientY-lastMouse.y)/zoom;lastMouse={x:e.clientX,y:e.clientY};}
});
window.addEventListener('mouseup',e=>{
  if(e.button===1||panning){panning=false;lastMouse=null;return;}
  if(e.button!==0||!dragging)return;dragging=false;const end=mouseCell(e);paintCells(dragStart,end);dragStart=null;recalc();updateHUD();
});
canvas.addEventListener('wheel',e=>{
  e.preventDefault();const old=zoom;zoom=clamp(zoom*(e.deltaY<0?1.12:.89),.45,2.4);
  const r=canvas.getBoundingClientRect(),sx=(e.clientX-r.left)*(canvas.width/r.width),sy=(e.clientY-r.top)*(canvas.height/r.height);
  const before={x:(sx-canvas.width/2)/old+camX,y:(sy-canvas.height/2)/old+camY};
  camX=before.x-(sx-canvas.width/2)/zoom;camY=before.y-(sy-canvas.height/2)/zoom;
},{passive:false});
canvas.addEventListener('contextmenu',e=>e.preventDefault());

document.querySelectorAll('[data-tool]').forEach(btn=>btn.addEventListener('click',()=>{selectedTool=btn.dataset.tool;document.querySelectorAll('[data-tool]').forEach(b=>b.classList.toggle('active',b===btn));}));
pauseBtn.onclick=()=>{paused=!paused;updateHUD()};
speedBtn.onclick=()=>{speed=speed===1?2:speed===2?4:1;updateHUD()};
saveBtn.onclick=()=>{localStorage.setItem('cities-v02',JSON.stringify({grid,money,population,jobs,day,camX,camY,zoom}));notify('Stadt gespeichert')};
loadBtn.onclick=()=>{const raw=localStorage.getItem('cities-v02');if(!raw)return notify('Kein Spielstand');const s=JSON.parse(raw);Object.assign(window,s);grid=s.grid;money=s.money;population=s.population;jobs=s.jobs;day=s.day;camX=s.camX;camY=s.camY;zoom=s.zoom;updateHUD();notify('Spielstand geladen')};
resetBtn.onclick=()=>{if(!confirm('Neue Stadt starten?'))return;freshGrid();money=100000;population=0;jobs=0;day=1;paused=false;speed=1;camX=COLS*CELL/2;camY=ROWS*CELL/2;zoom=.75;updateHUD();notify('Neue Stadt gestartet')};

window.addEventListener('keydown',e=>{if(e.code==='Space')spaceDown=true;});
window.addEventListener('keyup',e=>{if(e.code==='Space')spaceDown=false;});
const keys={};window.addEventListener('keydown',e=>keys[e.key.toLowerCase()]=true);window.addEventListener('keyup',e=>keys[e.key.toLowerCase()]=false);

camX=COLS*CELL/2;camY=ROWS*CELL/2;zoom=.75;updateHUD();

function loop(now){
  const dt=Math.min(60,now-lastTime);lastTime=now;
  const move=520*dt/1000/zoom;if(keys.w||keys.arrowup)camY-=move;if(keys.s||keys.arrowdown)camY+=move;if(keys.a||keys.arrowleft)camX-=move;if(keys.d||keys.arrowright)camX+=move;
  if(!paused){tickAcc+=dt*speed;while(tickAcc>=1200){simulate();tickAcc-=1200;}}
  draw();requestAnimationFrame(loop);
}
requestAnimationFrame(loop);
