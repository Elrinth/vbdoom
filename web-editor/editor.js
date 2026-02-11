(function () {
  'use strict';

  const LEVEL_FORMAT_VERSION = 1;

  const TILE_NAMES = [
    'Empty', 'STARTAN', 'STONE', 'TECH', 'DOOR', 'SWITCH',
    'Secret(Brick)', 'Secret(Stone)', 'Secret(Tech)',
    'Key Red', 'Key Yellow', 'Key Blue'
  ];
  const DOOR_TILES = [4, 6, 7, 8, 9, 10, 11];
  /** In raycaster preview, player can walk through doors and switches for easier level design. */
  const PREVIEW_PASSTHROUGH_TILES = [4, 5, 6, 7, 8, 9, 10, 11]; // DOOR, SWITCH, secret doors, key doors
  const ENEMY_NAMES = ['Zombie', 'Sergeant', 'Imp', 'Demon'];
  const PICKUP_NAMES = ['Ammo clip', 'Health small', 'Health large', 'Shotgun', 'Helmet', 'Armor', 'Shells', 'Rocket launcher'];

  /** Game limits (must match enemy.h, pickup.h, door.h). Editor enforces these and shows caps in UI. */
  const GAME_LIMITS = {
    MAX_ENEMIES: 21,
    MAX_PICKUPS: 16,
    MAX_DOORS: 24,
    MAX_SWITCHES: 4
  };

  let state = {
    mapW: 32,
    mapH: 32,
    map: null,
    spawn1: null,
    spawn2: null,
    enemies: [],
    pickups: [],
    switchLinks: [],
    brush: 1,
    placing: null,
    enemyType: 0,
    pickupType: 0,
    hoverCell: null,   // { x, y, type, index? } or null
    hoverEntity: null, // { type: 'spawn1'|'spawn2'|'enemy'|'pickup', index } or null
    linkMode: false,
    pendingSwitchIndex: null,
    previewPos: null,   // { x, y } in tile space (float) or null to use spawn1
    previewAngle: null, // 0-1023 or null to use spawn1.angle
    /** Door orientation: key "tx,ty" -> "ns" | "ew". N-S = wall runs north-south (approach from E/W). E-W = wall runs east-west (approach from N/S). */
    doorOrientations: {}
  };

  function cellIndex(x, y) { return y * state.mapW + x; }
  function getCell(x, y) { return state.map[cellIndex(x, y)]; }
  function setCell(x, y, v) {
    const isBorder = (x === 0 || x === state.mapW - 1 || y === 0 || y === state.mapH - 1);
    if (isBorder && v === 0) v = 1;
    state.map[cellIndex(x, y)] = v;
  }

  function fillBorderWalls() {
    const w = state.mapW, h = state.mapH;
    for (let x = 0; x < w; x++) {
      state.map[cellIndex(x, 0)] = 1;
      state.map[cellIndex(x, h - 1)] = 1;
    }
    for (let y = 0; y < h; y++) {
      state.map[cellIndex(0, y)] = 1;
      state.map[cellIndex(w - 1, y)] = 1;
    }
  }

  function resizeMap(w, h) {
    const newMap = new Uint8Array(w * h);
    const copyW = Math.min(w, state.mapW);
    const copyH = Math.min(h, state.mapH);
    for (let y = 0; y < copyH; y++)
      for (let x = 0; x < copyW; x++)
        newMap[y * w + x] = getCell(x, y);
    state.mapW = w;
    state.mapH = h;
    state.map = newMap;
    state.spawn1 = state.spawn1 && state.spawn1.x < w && state.spawn1.y < h ? state.spawn1 : null;
    state.spawn2 = state.spawn2 && state.spawn2.x < w && state.spawn2.y < h ? state.spawn2 : null;
    state.enemies = state.enemies.filter(e => e.tileX < w && e.tileY < h);
    state.pickups = state.pickups.filter(p => p.tileX < w && p.tileY < h);
    state.switchLinks = [];
    fillBorderWalls();
    refreshDoorSwitchLists();
  }

  function getDoors() {
    const list = [];
    for (let y = 0; y < state.mapH; y++)
      for (let x = 0; x < state.mapW; x++)
        if (DOOR_TILES.includes(getCell(x, y)))
          list.push({ tileX: x, tileY: y, tile: getCell(x, y) });
    return list;
  }

  function getSwitches() {
    const list = [];
    for (let y = 0; y < state.mapH; y++)
      for (let x = 0; x < state.mapW; x++)
        if (getCell(x, y) === 5) list.push({ tileX: x, tileY: y });
    return list;
  }

  function getCellInfo(x, y) {
    if (x < 0 || x >= state.mapW || y < 0 || y >= state.mapH) return null;
    if (state.spawn1 && state.spawn1.x === x && state.spawn1.y === y)
      return { type: 'spawn1', index: 0, label: 'Spawn 1' };
    if (state.spawn2 && state.spawn2.x === x && state.spawn2.y === y)
      return { type: 'spawn2', index: 0, label: 'Spawn 2' };
    const ei = state.enemies.findIndex(e => e.tileX === x && e.tileY === y);
    if (ei >= 0) return { type: 'enemy', index: ei, label: ENEMY_NAMES[state.enemies[ei].type] + ' (' + x + ',' + y + ')' };
    const pi = state.pickups.findIndex(p => p.tileX === x && p.tileY === y);
    if (pi >= 0) return { type: 'pickup', index: pi, label: PICKUP_NAMES[state.pickups[pi].type] + ' (' + x + ',' + y + ')' };
    const doors = getDoors();
    const di = doors.findIndex(d => d.tileX === x && d.tileY === y);
    if (di >= 0) return { type: 'door', index: di, label: 'Door ' + di };
    const switches = getSwitches();
    const si = switches.findIndex(s => s.tileX === x && s.tileY === y);
    if (si >= 0) {
      const link = state.switchLinks[si];
      const linkStr = link === -1 ? 'EXIT' : 'Door ' + link;
      return { type: 'switch', index: si, label: 'Switch ' + si + ' → ' + linkStr };
    }
    const t = getCell(x, y);
    return { type: 'empty', index: 0, label: t === 0 ? 'Empty' : TILE_NAMES[t] };
  }

  function refreshDoorSwitchLists() {
    const doors = getDoors();
    const switches = getSwitches();
    while (state.switchLinks.length < switches.length) state.switchLinks.push(0);
    state.switchLinks.length = switches.length;

    const doorList = document.getElementById('doorList');
    const doorCap = GAME_LIMITS.MAX_DOORS;
    const switchCap = GAME_LIMITS.MAX_SWITCHES;
    const doorOver = doors.length > doorCap;
    const switchOver = switches.length > switchCap;
    const doorHeader = document.getElementById('doorListCap');
    if (doorHeader) {
      doorHeader.textContent = 'Doors: ' + doors.length + ' / ' + doorCap;
      doorHeader.classList.toggle('over-limit', doorOver);
    }
    const switchHeader = document.getElementById('switchListCap');
    if (switchHeader) {
      switchHeader.textContent = 'Switches: ' + switches.length + ' / ' + switchCap;
      switchHeader.classList.toggle('over-limit', switchOver);
    }
    doorList.innerHTML = '';
    if (doors.length) {
      doors.forEach((d, i) => {
        const key = d.tileX + ',' + d.tileY;
        if (state.doorOrientations[key] == null) state.doorOrientations[key] = 'ns';
        const li = document.createElement('li');
        const orientSel = document.createElement('select');
        orientSel.className = 'door-orient';
        orientSel.title = 'Wall direction: N-S = corridor runs E-W (approach from sides). E-W = corridor runs N-S (approach from top/bottom). If door won\'t open in-game, try the other.';
        orientSel.innerHTML = '<option value="ns">N-S</option><option value="ew">E-W</option>';
        orientSel.value = state.doorOrientations[key];
        orientSel.addEventListener('change', () => { state.doorOrientations[key] = orientSel.value; });
        li.appendChild(document.createTextNode('Door ' + i + ' (' + d.tileX + ',' + d.tileY + ') '));
        li.appendChild(orientSel);
        doorList.appendChild(li);
      });
    } else {
      doorList.appendChild(document.createElement('li')).textContent = 'None';
    }

    const switchList = document.getElementById('switchList');
    switchList.innerHTML = '';
    if (switches.length) {
      switches.forEach((s, i) => {
        const li = document.createElement('li');
        const sel = document.createElement('select');
        sel.className = 'switch-link';
        sel.innerHTML = '<option value="-1">EXIT</option>' + doors.map((_, d) => `<option value="${d}">Door ${d}</option>`).join('');
        sel.value = String(state.switchLinks[i] === -1 ? -1 : state.switchLinks[i]);
        sel.addEventListener('change', () => { state.switchLinks[i] = parseInt(sel.value, 10); });
        li.appendChild(document.createTextNode(`Switch ${i} (${s.tileX},${s.tileY}) → `));
        li.appendChild(sel);
        switchList.appendChild(li);
      });
    } else {
      switchList.appendChild(document.createElement('li')).textContent = 'None';
    }
  }

  function initMap() {
    const size = document.getElementById('mapSize').value.split(',').map(Number);
    const w = size[0], h = size[1];
    if (state.map === null) {
      state.map = new Uint8Array(w * h);
      state.mapW = w;
      state.mapH = h;
    } else
      resizeMap(w, h);
    fillBorderWalls();
    document.getElementById('status').textContent = `Map ${state.mapW}×${state.mapH}. Click or drag to paint.`;
    refreshDoorSwitchLists();
    refreshEntityList();
    state.previewPos = state.spawn1
      ? { x: state.spawn1.x + 0.5, y: state.spawn1.y + 0.5 }
      : { x: state.mapW / 2, y: state.mapH / 2 };
    state.previewAngle = state.spawn1 ? (state.spawn1.angle != null ? state.spawn1.angle : 512) : 0;
    drawGrid();
    if (typeof resizePreview === 'function') resizePreview();
  }

  const canvas = document.getElementById('grid');
  const ctx = canvas.getContext('2d');
  const CELL_PX = 12;
  const MAX_CANVAS = 600;

  function drawGrid() {
    const w = state.mapW;
    const h = state.mapH;
    let cellPx = CELL_PX;
    let cw = w * cellPx, ch = h * cellPx;
    if (cw > MAX_CANVAS || ch > MAX_CANVAS) {
      cellPx = Math.floor(MAX_CANVAS / Math.max(w, h));
      cw = w * cellPx;
      ch = h * cellPx;
    }
    canvas.width = cw;
    canvas.height = ch;
    canvas.dataset.cellPx = cellPx;

    for (let y = 0; y < h; y++)
      for (let x = 0; x < w; x++) {
        let tile = getCell(x, y);
        ctx.fillStyle = tile <= 11 ? ['#1a0a0a','#8b4513','#555','#4a4a6a','#6a3a2a','#5a4a3a','#7a3525','#454545','#3a3a5a','#8b2a2a','#8b7a2a','#2a4a8b'][tile] : '#333';
        ctx.fillRect(x * cellPx, y * cellPx, cellPx, cellPx);
        if (DOOR_TILES.includes(tile)) {
          ctx.strokeStyle = '#aaa';
          ctx.lineWidth = 1;
          ctx.strokeRect(x * cellPx, y * cellPx, cellPx, cellPx);
        }
      }
    ctx.strokeStyle = '#333';
    ctx.lineWidth = 1;
    for (let x = 0; x <= w; x++) ctx.strokeRect(x * cellPx, 0, 0, ch);
    for (let y = 0; y <= h; y++) ctx.strokeRect(0, y * cellPx, cw, 0);

    const cx = (x) => (x + 0.5) * cellPx;
    const cy = (y) => (y + 0.5) * cellPx;
    const r = cellPx * 0.4;
    const facingLen = cellPx * 0.48;
    const drawFacing = (centerX, centerY, angle, strokeColor) => {
      const angleRad = (angle / 1024) * 2 * Math.PI - Math.PI / 2;
      ctx.strokeStyle = strokeColor || '#fff';
      ctx.lineWidth = 2;
      ctx.beginPath();
      ctx.moveTo(centerX, centerY);
      ctx.lineTo(centerX + Math.cos(angleRad) * facingLen, centerY + Math.sin(angleRad) * facingLen);
      ctx.stroke();
    };
    if (state.spawn1) {
      const x = state.spawn1.x, y = state.spawn1.y;
      const angle = state.spawn1.angle != null ? state.spawn1.angle : 512;
      ctx.fillStyle = '#0a3';
      ctx.fillRect(x * cellPx, y * cellPx, cellPx, cellPx);
      ctx.strokeStyle = '#0a5';
      ctx.lineWidth = 2;
      ctx.beginPath();
      ctx.moveTo(cx(x) - r, cy(y)); ctx.lineTo(cx(x) + r, cy(y));
      ctx.moveTo(cx(x), cy(y) - r); ctx.lineTo(cx(x), cy(y) + r);
      ctx.stroke();
      drawFacing(cx(x), cy(y), angle, '#fff');
    }
    if (state.spawn2) {
      const x = state.spawn2.x, y = state.spawn2.y;
      const angle = state.spawn2.angle != null ? state.spawn2.angle : 512;
      ctx.fillStyle = '#03a';
      ctx.fillRect(x * cellPx, y * cellPx, cellPx, cellPx);
      ctx.strokeStyle = '#05a';
      ctx.lineWidth = 2;
      ctx.beginPath();
      ctx.moveTo(cx(x) - r, cy(y)); ctx.lineTo(cx(x) + r, cy(y));
      ctx.moveTo(cx(x), cy(y) - r); ctx.lineTo(cx(x), cy(y) + r);
      ctx.stroke();
      drawFacing(cx(x), cy(y), angle, '#fff');
    }
    const ENEMY_GRID_COLORS = ['#e05050', '#8b8b00', '#e08020', '#a020a0'];
    const PICKUP_GRID_COLORS = ['#ffcc00', '#50ff50', '#50ffff', '#ff9050', '#c0c0c0', '#8080ff', '#ffa060', '#ff4040'];
    state.enemies.forEach(e => {
      ctx.fillStyle = ENEMY_GRID_COLORS[e.type] || '#e00';
      ctx.beginPath();
      ctx.arc(cx(e.tileX), cy(e.tileY), r, 0, 2 * Math.PI);
      ctx.fill();
      ctx.strokeStyle = '#c00';
      ctx.lineWidth = 1.5;
      ctx.stroke();
      drawFacing(cx(e.tileX), cy(e.tileY), e.angle != null ? e.angle : 0, '#fff');
    });
    state.pickups.forEach(p => {
      const x = p.tileX, y = p.tileY;
      const left = x * cellPx + cellPx * 0.2, right = (x + 1) * cellPx - cellPx * 0.2;
      const top = y * cellPx + cellPx * 0.15, bottom = (y + 1) * cellPx - cellPx * 0.15;
      ctx.fillStyle = PICKUP_GRID_COLORS[p.type] || '#5a5';
      ctx.beginPath();
      ctx.moveTo(cx(x), top);
      ctx.lineTo(right, bottom);
      ctx.lineTo(left, bottom);
      ctx.closePath();
      ctx.fill();
    });

    if (state.hoverEntity) {
      let hx, hy;
      if (state.hoverEntity.type === 'spawn1' && state.spawn1) { hx = state.spawn1.x; hy = state.spawn1.y; }
      else if (state.hoverEntity.type === 'spawn2' && state.spawn2) { hx = state.spawn2.x; hy = state.spawn2.y; }
      else if (state.hoverEntity.type === 'enemy' && state.enemies[state.hoverEntity.index]) {
        const e = state.enemies[state.hoverEntity.index]; hx = e.tileX; hy = e.tileY;
      } else if (state.hoverEntity.type === 'pickup' && state.pickups[state.hoverEntity.index]) {
        const p = state.pickups[state.hoverEntity.index]; hx = p.tileX; hy = p.tileY;
      } else { hx = null; hy = null; }
      if (hx != null && hy != null) {
        ctx.strokeStyle = '#fff';
        ctx.lineWidth = 2;
        ctx.strokeRect(hx * cellPx, hy * cellPx, cellPx, cellPx);
      }
    }
    if (state.linkMode && state.pendingSwitchIndex !== null) {
      const switches = getSwitches();
      const sw = switches[state.pendingSwitchIndex];
      if (sw) {
        ctx.strokeStyle = '#ff0';
        ctx.lineWidth = 2;
        ctx.strokeRect(sw.tileX * cellPx, sw.tileY * cellPx, cellPx, cellPx);
      }
    }

    const overlay = document.getElementById('gridOverlay');
    if (overlay) {
      overlay.width = cw;
      overlay.height = ch;
    }
  }

  function drawPreviewOnMap() {
    const overlay = document.getElementById('gridOverlay');
    if (!overlay || overlay.width === 0 || overlay.height === 0 || !state.map) return;
    const cellPx = Number(canvas.dataset.cellPx) || CELL_PX;
    const preview = getPreviewPlayer();
    const ox = overlay.getContext('2d');
    ox.clearRect(0, 0, overlay.width, overlay.height);
    const px = preview.x * cellPx, py = preview.y * cellPx;
    const previewR = Math.max(3, cellPx * 0.2);
    const len = cellPx * 0.5;
    ox.fillStyle = '#ff0';
    ox.beginPath();
    ox.arc(px, py, previewR, 0, 2 * Math.PI);
    ox.fill();
    ox.strokeStyle = '#aa0';
    ox.lineWidth = 1;
    ox.stroke();
    ox.strokeStyle = '#ff0';
    ox.lineWidth = 2;
    const angleRad = (preview.angle / 1024) * 2 * Math.PI - Math.PI / 2;
    ox.beginPath();
    ox.moveTo(px, py);
    ox.lineTo(px + Math.cos(angleRad) * len, py + Math.sin(angleRad) * len);
    ox.stroke();
  }

  function getPreviewPlayer() {
    const x = state.previewPos != null ? state.previewPos.x : state.mapW / 2;
    const y = state.previewPos != null ? state.previewPos.y : state.mapH / 2;
    const angle = state.previewAngle != null ? state.previewAngle : 0;
    return { x, y, angle };
  }

  const WALL_COLORS = ['#1a0a0a','#8b4513','#555','#4a4a6a','#6a3a2a','#5a4a3a','#7a3525','#454545','#3a3a5a','#8b2a2a','#8b7a2a','#2a4a8b'];
  const WALL_TEXTURE_URLS = [
    null,
    '../doom_graphics_unvirtualboyed/wall_textures/WALL01_1.png',
    '../doom_graphics_unvirtualboyed/wall_textures/WALL02_1.png',
    '../doom_graphics_unvirtualboyed/wall_textures/WALL03_1.png',
    '../door_texture.png',
    '../gaint-switch1-doom-palette.png',
    '../doom_graphics_unvirtualboyed/wall_textures/WALL01_1.png',
    '../doom_graphics_unvirtualboyed/wall_textures/WALL02_1.png',
    '../doom_graphics_unvirtualboyed/wall_textures/WALL03_1.png',
    '../door_texture.png',
    '../door_texture.png',
    '../door_texture.png'
  ];
  const wallTextures = [];
  (function loadWallTextures() {
    for (let t = 1; t <= 11; t++) {
      const img = new Image();
      img.src = WALL_TEXTURE_URLS[t] || '';
      wallTextures[t] = img;
    }
  })();

  const ENEMY_SPRITE_URLS = [
    '../doom_graphics_unvirtualboyed/monster_imp/TROOA1.png',
    '../doom_graphics_unvirtualboyed/monster_imp/TROOA2A8.png',
    '../doom_graphics_unvirtualboyed/monster_imp/TROOA3A7.png',
    '../doom_graphics_unvirtualboyed/monster_imp/TROOA4A6.png',
    '../doom_graphics_unvirtualboyed/monster_imp/TROOA5.png'
  ];
  const enemySprites = [];
  (function loadEnemySprites() {
    ENEMY_SPRITE_URLS.forEach((url, i) => {
      const img = new Image();
      img.src = url;
      enemySprites[i] = img;
    });
  })();
  const spritePlayer = new Image();
  spritePlayer.src = '../doom_graphics_unvirtualboyed/player/PLAYA1.png';
  const spritePickup = new Image();
  spritePickup.src = '../doom_graphics_unvirtualboyed/stage_pickups_n_shit/AMMOA0.png';
  const spritePistol = new Image();
  spritePistol.src = '../doom_graphics_unvirtualboyed/pistol/PISGA0.png';

  function getEnemySpriteIndex(ent, px, py) {
    // Sprite sheet uses angle from ENEMY TO PLAYER; atan2 gives player→enemy, so add π
    const dx = ent.x - px, dy = ent.y - py;
    let angle = Math.atan2(dx, -dy) + Math.PI;
    if (angle < 0) angle += 2 * Math.PI;
    let segment = Math.floor((angle / (2 * Math.PI)) * 8) % 8;
    // Offset by enemy's facing so the dropdown direction updates the raycaster
    const enemy = state.enemies[ent.i];
    const faceAngle = enemy.angle != null ? enemy.angle : 0;
    segment = (segment + Math.floor(faceAngle / 128)) % 8;
    return segment;
  }

  function drawRaycastPreview(ctx, width, height) {
    const player = getPreviewPlayer();
    const px = player.x, py = player.y;
    const angleRad = (player.angle / 1024) * 2 * Math.PI - Math.PI / 2;
    const fov = Math.PI / 3;
    const halfFov = fov / 2;
    const numRays = width;
    const W = state.mapW, H = state.mapH;

    ctx.fillStyle = '#1a1a2a';
    ctx.fillRect(0, 0, width, height / 2);
    ctx.fillStyle = '#2a1a1a';
    ctx.fillRect(0, height / 2, width, height / 2);

    const step = 0.02;
    for (let col = 0; col < numRays; col++) {
      const t = (col / numRays) - 0.5;
      const rayAngle = angleRad + t * fov;
      const dx = Math.cos(rayAngle), dy = Math.sin(rayAngle);
      let x = px, y = py;
      let dist = 0;
      let hit = 0;
      let hitX = 0, hitY = 0;
      for (let i = 0; i < 400; i++) {
        const gx = Math.floor(x), gy = Math.floor(y);
        if (gx < 0 || gx >= W || gy < 0 || gy >= H) break;
        const cell = getCell(gx, gy);
        if (cell > 0 && !PREVIEW_PASSTHROUGH_TILES.includes(cell)) { hit = cell; hitX = x; hitY = y; break; }
        x += dx * step;
        y += dy * step;
        dist += step;
      }
      if (hit > 0 && hit <= 11) {
        const gx = Math.floor(hitX), gy = Math.floor(hitY);
        const fracX = hitX - gx, fracY = hitY - gy;
        const minDist = Math.min(fracX, 1 - fracX, fracY, 1 - fracY);
        const texU = (minDist === fracX || minDist === 1 - fracX) ? fracY : fracX;
        const wallHeight = Math.min(height * 1.2, height / (dist + 0.01));
        const top = (height - wallHeight) / 2;
        const shade = Math.min(1, 1 / (dist * 0.3));
        const img = wallTextures[hit];
        if (img && img.complete && img.naturalWidth) {
          const tw = img.naturalWidth, th = img.naturalHeight;
          const srcX = Math.floor(texU * tw) % tw;
          ctx.globalAlpha = shade;
          ctx.drawImage(img, srcX, 0, 1, th, col, top, 1, wallHeight);
          ctx.globalAlpha = 1;
        } else {
          ctx.fillStyle = WALL_COLORS[hit];
          ctx.globalAlpha = shade;
          ctx.fillRect(col, top, 1, wallHeight);
          ctx.globalAlpha = 1;
        }
      }
    }

    const viewAngle = (player.angle / 1024) * 2 * Math.PI;
    const halfFovRad = fov / 2;
    const proj = (width / 2) / Math.tan(halfFovRad);
    const SPRITE_WORLD_SIZE = 0.8;

    const ENEMY_COLORS = ['#e05050', '#8b8b00', '#e08020', '#a020a0'];
    const PICKUP_COLORS = ['#ffcc00', '#50ff50', '#50ffff', '#ff9050', '#c0c0c0', '#8080ff', '#ffa060', '#ff4040'];
    const entities = [];
    state.enemies.forEach((e, i) => entities.push({ x: e.tileX + 0.5, y: e.tileY + 0.5, type: 'enemy', i, enemyType: e.type }));
    state.pickups.forEach((p, i) => entities.push({ x: p.tileX + 0.5, y: p.tileY + 0.5, type: 'pickup', i, pickupType: p.type }));
    if (state.spawn1 && (Math.floor(px) !== state.spawn1.x || Math.floor(py) !== state.spawn1.y))
      entities.push({ x: state.spawn1.x + 0.5, y: state.spawn1.y + 0.5, type: 'spawn', id: 1 });
    if (state.spawn2)
      entities.push({ x: state.spawn2.x + 0.5, y: state.spawn2.y + 0.5, type: 'spawn', id: 2 });
    entities.forEach(ent => {
      const dx = ent.x - px, dy = ent.y - py;
      const dist = Math.sqrt(dx * dx + dy * dy);
      if (dist < 0.05) return;
      const angleTo = Math.atan2(dx, -dy);
      let angleDiff = angleTo - viewAngle;
      while (angleDiff > Math.PI) angleDiff -= 2 * Math.PI;
      while (angleDiff < -Math.PI) angleDiff += 2 * Math.PI;
      if (angleDiff < -halfFovRad || angleDiff > halfFovRad) return;
      ent.depth = dist * Math.cos(angleDiff);
      ent.angleDiff = angleDiff;
      ent.dist = dist;
    });
    const visible = entities.filter(e => e.depth != null);

    // Raycast from player to each entity; skip if blocked by wall (reuses step from above)
    const raycastTo = (ex, ey) => {
      const rdx = ex - px, rdy = ey - py;
      const rdist = Math.sqrt(rdx * rdx + rdy * rdy);
      if (rdist < 0.05) return true;
      const rnx = rdx / rdist * step, rny = rdy / rdist * step;
      let rx = px, ry = py;
      const steps = Math.ceil(rdist / step);
      for (let i = 0; i < steps; i++) {
        rx += rnx; ry += rny;
        const gx = Math.floor(rx), gy = Math.floor(ry);
        if (gx < 0 || gx >= W || gy < 0 || gy >= H) return false;
        const c = getCell(gx, gy);
        if (c > 0 && !PREVIEW_PASSTHROUGH_TILES.includes(c)) return false; // hit solid wall before entity
      }
      return true;
    };
    const reallyVisible = visible.filter(ent => raycastTo(ent.x, ent.y));
    reallyVisible.sort((a, b) => b.depth - a.depth);

    const ENEMY_SPRITE_MAP = [0, 1, 2, 3, 4, 3, 2, 1];
    const ENEMY_SPRITE_FLIP = [false, false, false, false, false, true, true, true];
    reallyVisible.forEach(ent => {
      const dist = ent.dist;
      const angleDiff = ent.angleDiff;
      const depth = ent.depth;
      const screenX = (width / 2) * (1 + Math.tan(angleDiff) / Math.tan(halfFovRad));
      const h = Math.max(8, Math.min(height * 0.9, (SPRITE_WORLD_SIZE * proj) / depth));
      const w = h;
      const left = screenX - w / 2;
      const top = (height - h) / 2;
      if (left + w < 0 || left > width) return;
      ctx.globalAlpha = 1; // full opacity for sprites (was distance shading)

      if (ent.type === 'enemy') {
        const segment = getEnemySpriteIndex(ent, px, py);
        const imgIdx = ENEMY_SPRITE_MAP[segment];
        const img = enemySprites[imgIdx];
        if (img && img.complete && img.naturalWidth) {
          const w = (img.naturalWidth / img.naturalHeight) * h;
          const x = screenX - w / 2;
          if (ENEMY_SPRITE_FLIP[segment]) {
            ctx.save();
            ctx.translate(screenX, 0);
            ctx.scale(-1, 1);
            ctx.translate(-screenX, 0);
            ctx.drawImage(img, x, top, w, h);
            ctx.restore();
          } else {
            ctx.drawImage(img, x, top, w, h);
          }
        } else {
          const c = ENEMY_COLORS[ent.enemyType] || '#e00';
          ctx.fillStyle = c;
          ctx.fillRect(screenX - 4, top, 8, h);
        }
      } else if (ent.type === 'pickup') {
        const img = spritePickup;
        if (img && img.complete && img.naturalWidth) {
          const w = (img.naturalWidth / img.naturalHeight) * h;
          ctx.drawImage(img, screenX - w / 2, top, w, h);
        } else {
          const c = PICKUP_COLORS[ent.pickupType] || '#5a5';
          ctx.fillStyle = c;
          ctx.fillRect(screenX - 4, top, 8, h);
        }
      } else {
        const img = spritePlayer;
        if (img && img.complete && img.naturalWidth) {
          const w = (img.naturalWidth / img.naturalHeight) * h;
          ctx.drawImage(img, screenX - w / 2, top, w, h);
        } else {
          ctx.fillStyle = '#0a5';
          ctx.fillRect(screenX - 4, top, 8, h);
          ctx.fillStyle = '#fff';
          ctx.font = '10px sans-serif';
          ctx.fillText(String(ent.id || ''), screenX - 3, top + 10);
        }
      }
      ctx.globalAlpha = 1;
    });
    ctx.globalAlpha = 1;

    if (spritePistol.complete && spritePistol.naturalWidth) {
      const pw = spritePistol.naturalWidth, ph = spritePistol.naturalHeight;
      const scale = Math.min(width / pw, height * 0.4 / ph);
      const w = pw * scale, h = ph * scale;
      ctx.drawImage(spritePistol, (width - w) / 2, height - h, w, h);
    }
  }

  function updateTooltip(clientX, clientY, info) {
    const el = document.getElementById('gridTooltip');
    if (!info) {
      el.classList.add('hidden');
      el.textContent = '';
      return;
    }
    el.textContent = info.label;
    el.classList.remove('hidden');
    el.style.left = (clientX + 10) + 'px';
    el.style.top = (clientY + 10) + 'px';
  }

  const ANGLE_OPTIONS = [
    [0, 'N'], [128, 'NE'], [256, 'E'], [384, 'SE'], [512, 'S'], [640, 'SW'], [768, 'W'], [896, 'NW']
  ];
  function refreshEntityList() {
    const ul = document.getElementById('entityList');
    ul.innerHTML = '';
    const add = (entityType, index, label, getAngle, setAngle) => {
      const li = document.createElement('li');
      li.setAttribute('data-entity', entityType);
      li.setAttribute('data-index', String(index));
      li.appendChild(document.createTextNode(label));
      if (getAngle != null && setAngle != null) {
        const sel = document.createElement('select');
        sel.className = 'entity-angle';
        sel.title = 'Direction';
        ANGLE_OPTIONS.forEach(([val, dir]) => {
          const opt = document.createElement('option');
          opt.value = String(val);
          opt.textContent = dir;
          sel.appendChild(opt);
        });
        const current = getAngle();
        const closest = ANGLE_OPTIONS.reduce((a, b) => Math.abs(a[0] - current) <= Math.abs(b[0] - current) ? a : b);
        sel.value = String(closest[0]);
        sel.addEventListener('change', () => { setAngle(parseInt(sel.value, 10)); drawGrid(); });
        li.appendChild(document.createTextNode(' '));
        li.appendChild(sel);
      }
      li.addEventListener('mouseenter', () => {
        state.hoverEntity = { type: entityType, index };
        drawGrid();
      });
      li.addEventListener('mouseleave', () => {
        state.hoverEntity = null;
        drawGrid();
      });
      ul.appendChild(li);
    };
    if (state.spawn1) add('spawn1', 0, 'Spawn 1 (' + state.spawn1.x + ',' + state.spawn1.y + ')', () => state.spawn1.angle != null ? state.spawn1.angle : 512, (v) => { state.spawn1.angle = v; });
    const enemyCap = GAME_LIMITS.MAX_ENEMIES;
    const pickupCap = GAME_LIMITS.MAX_PICKUPS;
    const entityCapEl = document.getElementById('entityListCap');
    if (entityCapEl) {
      entityCapEl.textContent = 'Enemies: ' + state.enemies.length + ' / ' + enemyCap + '  ·  Pickups: ' + state.pickups.length + ' / ' + pickupCap;
      entityCapEl.classList.toggle('over-limit', state.enemies.length > enemyCap || state.pickups.length > pickupCap);
    }
    if (state.spawn2) add('spawn2', 0, 'Spawn 2 (' + state.spawn2.x + ',' + state.spawn2.y + ')', () => state.spawn2.angle != null ? state.spawn2.angle : 512, (v) => { state.spawn2.angle = v; });
    state.enemies.forEach((e, i) => add('enemy', i, ENEMY_NAMES[e.type] + ' (' + e.tileX + ',' + e.tileY + ')', () => state.enemies[i].angle != null ? state.enemies[i].angle : 0, (v) => { state.enemies[i].angle = v; }));
    state.pickups.forEach((p, i) => add('pickup', i, PICKUP_NAMES[p.type] + ' (' + p.tileX + ',' + p.tileY + ')', null, null));
    if (!ul.children.length) {
      const li = document.createElement('li');
      li.textContent = 'None';
      ul.appendChild(li);
    }
    updateEntityListHighlight();
  }

  /** Show a short-lived message (toast) so user sees limits / errors without blocking. */
  let toastTimeout = null;
  function showToast(message, durationMs) {
    if (durationMs == null) durationMs = 2800;
    const el = document.getElementById('toast');
    if (!el) return;
    el.textContent = message;
    el.classList.remove('hidden');
    if (toastTimeout) clearTimeout(toastTimeout);
    toastTimeout = setTimeout(() => {
      el.classList.add('hidden');
      toastTimeout = null;
    }, durationMs);
  }

  function updateEntityListHighlight() {
    document.querySelectorAll('#entityList [data-entity]').forEach(li => {
      const type = li.getAttribute('data-entity');
      const index = parseInt(li.getAttribute('data-index'), 10);
      const match = state.hoverCell && state.hoverCell.type === type && state.hoverCell.index === index;
      li.classList.toggle('highlight', !!match);
    });
  }

  function xyFromEvent(e) {
    const rect = canvas.getBoundingClientRect();
    const cellPx = Number(canvas.dataset.cellPx) || CELL_PX;
    const x = Math.floor((e.clientX - rect.left) / cellPx);
    const y = Math.floor((e.clientY - rect.top) / cellPx);
    return { x, y };
  }

  function onGridClick(e) {
    const { x, y } = xyFromEvent(e);
    if (x < 0 || x >= state.mapW || y < 0 || y >= state.mapH) return;

    if (state.linkMode) {
      const switches = getSwitches();
      const doors = getDoors();
      const switchIdx = switches.findIndex(s => s.tileX === x && s.tileY === y);
      const doorIdx = doors.findIndex(d => d.tileX === x && d.tileY === y);
      if (state.pendingSwitchIndex === null) {
        if (switchIdx >= 0) {
          state.pendingSwitchIndex = switchIdx;
          document.getElementById('pendingSwitchNum').textContent = String(switchIdx);
          document.getElementById('linkModeExit').classList.remove('hidden');
          document.getElementById('status').textContent = 'Switch ' + switchIdx + ' selected. Click a door to link, or set EXIT below.';
          drawGrid();
        }
        return;
      }
      if (doorIdx >= 0) {
        state.switchLinks[state.pendingSwitchIndex] = doorIdx;
        state.pendingSwitchIndex = null;
        document.getElementById('linkModeExit').classList.add('hidden');
        refreshDoorSwitchLists();
        document.getElementById('status').textContent = 'Map ' + state.mapW + '×' + state.mapH + '. Click or drag to paint.';
        drawGrid();
        return;
      }
      if (switchIdx >= 0) {
        state.pendingSwitchIndex = switchIdx;
        document.getElementById('pendingSwitchNum').textContent = String(switchIdx);
        document.getElementById('linkModeExit').classList.remove('hidden');
        document.getElementById('status').textContent = 'Switch ' + switchIdx + ' selected. Click a door to link, or set EXIT below.';
        drawGrid();
        return;
      }
      state.pendingSwitchIndex = null;
      document.getElementById('linkModeExit').classList.add('hidden');
      document.getElementById('status').textContent = 'Map ' + state.mapW + '×' + state.mapH + '. Click or drag to paint.';
      drawGrid();
      return;
    }

    if (state.placing === 'spawn1') {
      state.spawn1 = { x, y, angle: 512 };
      state.placing = null;
      document.getElementById('brushSpawn1').classList.remove('selected');
    } else if (state.placing === 'spawn2') {
      state.spawn2 = { x, y, angle: 512 };
      state.placing = null;
      document.getElementById('brushSpawn2').classList.remove('selected');
    } else if (state.placing === 'enemy') {
      if (state.enemies.length >= GAME_LIMITS.MAX_ENEMIES) {
        showToast('Max enemies reached (' + GAME_LIMITS.MAX_ENEMIES + '). Remove one or increase MAX_ENEMIES in enemy.h.');
        return;
      }
      state.enemies.push({ type: state.enemyType, tileX: x, tileY: y, angle: 0 });
      state.placing = null;
      document.getElementById('brushEnemy').value = '';
    } else if (state.placing === 'pickup') {
      if (state.pickups.length >= GAME_LIMITS.MAX_PICKUPS) {
        showToast('Max pickups reached (' + GAME_LIMITS.MAX_PICKUPS + '). Remove one or increase MAX_PICKUPS in pickup.h.');
        return;
      }
      state.pickups.push({ type: state.pickupType, tileX: x, tileY: y });
      state.placing = null;
      document.getElementById('brushPickup').value = '';
    } else {
      setCell(x, y, state.brush);
      refreshDoorSwitchLists();
      const doors = getDoors();
      const switches = getSwitches();
      if (doors.length > GAME_LIMITS.MAX_DOORS)
        showToast('Too many door tiles (' + doors.length + '). Game max is ' + GAME_LIMITS.MAX_DOORS + ' (MAX_DOORS in door.h).');
      if (switches.length > GAME_LIMITS.MAX_SWITCHES)
        showToast('Too many switch tiles (' + switches.length + '). Game max is ' + GAME_LIMITS.MAX_SWITCHES + ' (MAX_SWITCHES in door.h).');
    }
    refreshEntityList();
    drawGrid();
  }

  function buildJson() {
    return {
      version: LEVEL_FORMAT_VERSION,
      mapW: state.mapW,
      mapH: state.mapH,
      map: Array.from(state.map),
      spawn1: state.spawn1,
      spawn2: state.spawn2,
      enemies: state.enemies.map(e => ({ type: e.type, tileX: e.tileX, tileY: e.tileY, angle: e.angle != null ? e.angle : 0 })),
      pickups: state.pickups.map(p => ({ type: p.type, tileX: p.tileX, tileY: p.tileY })),
      switchLinks: state.switchLinks.slice(),
      doorOrientations: state.doorOrientations ? { ...state.doorOrientations } : {}
    };
  }

  function loadFromJson(obj) {
    const v = obj.version;
    if (v != null && v > LEVEL_FORMAT_VERSION) {
      alert('Level was saved with a newer editor. Please update the editor.');
      return false;
    }
    const mapW = obj.mapW, mapH = obj.mapH, mapArr = obj.map;
    if (typeof mapW !== 'number' || typeof mapH !== 'number' || !Array.isArray(mapArr) || mapArr.length !== mapW * mapH) {
      alert('Invalid level JSON: bad map dimensions or map array.');
      return false;
    }
    state.mapW = mapW;
    state.mapH = mapH;
    state.map = new Uint8Array(mapArr);
    state.spawn1 = obj.spawn1 && typeof obj.spawn1.x === 'number' && typeof obj.spawn1.y === 'number'
      ? { x: obj.spawn1.x, y: obj.spawn1.y, angle: obj.spawn1.angle != null ? obj.spawn1.angle : 512 }
      : null;
    state.spawn2 = obj.spawn2 && typeof obj.spawn2.x === 'number' && typeof obj.spawn2.y === 'number'
      ? { x: obj.spawn2.x, y: obj.spawn2.y, angle: obj.spawn2.angle != null ? obj.spawn2.angle : 512 }
      : null;
    const rawEnemies = (obj.enemies || []).map(e => ({
      type: Number(e.type) || 0,
      tileX: Number(e.tileX) || 0,
      tileY: Number(e.tileY) || 0,
      angle: e.angle != null ? Number(e.angle) : 0
    }));
    const rawPickups = (obj.pickups || []).map(p => ({
      type: Number(p.type) || 0,
      tileX: Number(p.tileX) || 0,
      tileY: Number(p.tileY) || 0
    }));
    state.enemies = rawEnemies.slice(0, GAME_LIMITS.MAX_ENEMIES);
    state.pickups = rawPickups.slice(0, GAME_LIMITS.MAX_PICKUPS);
    if (rawEnemies.length > GAME_LIMITS.MAX_ENEMIES)
      showToast('Loaded level had ' + rawEnemies.length + ' enemies; trimmed to ' + GAME_LIMITS.MAX_ENEMIES + ' (game limit).');
    if (rawPickups.length > GAME_LIMITS.MAX_PICKUPS)
      showToast('Loaded level had ' + rawPickups.length + ' pickups; trimmed to ' + GAME_LIMITS.MAX_PICKUPS + ' (game limit).');
    state.switchLinks = Array.isArray(obj.switchLinks) ? obj.switchLinks.slice() : [];
    state.doorOrientations = obj.doorOrientations && typeof obj.doorOrientations === 'object' ? { ...obj.doorOrientations } : {};
    state.previewPos = state.spawn1
      ? { x: state.spawn1.x + 0.5, y: state.spawn1.y + 0.5 }
      : { x: state.mapW / 2, y: state.mapH / 2 };
    state.previewAngle = state.spawn1 ? (state.spawn1.angle != null ? state.spawn1.angle : 512) : 0;
    refreshDoorSwitchLists();
    refreshEntityList();
    drawGrid();
    document.getElementById('status').textContent = 'Map ' + state.mapW + '×' + state.mapH + '. Click or drag to paint.';
    return true;
  }

  /** Content for the .h file only (map + spawn defines). Do NOT put init functions here - they go in enemy.c/pickup.c. */
  function buildExportHeaderFileContent() {
    const id = (document.getElementById('levelId').value || 'e1m4').toLowerCase().replace(/\s/g, '');
    const prefix = id.toUpperCase();
    const W = state.mapW, H = state.mapH;
    const cells = W * H;
    let out = '';
    out += `/* VBDOOM_LEVEL_FORMAT ${LEVEL_FORMAT_VERSION} */\n`;
    out += `/* ${id} map: ${W}x${H} */\n`;
    out += `const u8 ${id}_map[${cells}] = {\n`;
    for (let y = 0; y < H; y++) {
      out += '/* Row ' + y + ' */ ';
      for (let x = 0; x < W; x++) out += getCell(x, y) + (x < W - 1 ? ',' : '');
      out += y < H - 1 ? ',\n' : '\n';
    }
    out += '};\n\n';
    const s1 = state.spawn1 || { x: 0, y: 0, angle: 512 };
    const s2 = state.spawn2 || { x: 0, y: 0, angle: 512 };
    out += `/* ${id} spawn and level data */\n`;
    out += `#define ${prefix}_SPAWN_X  (${s1.x} * 256 + 128)\n`;
    out += `#define ${prefix}_SPAWN_Y  (${s1.y} * 256 + 128)\n`;
    out += `#define ${prefix}_SPAWN2_X (${s2.x} * 256 + 128)\n`;
    out += `#define ${prefix}_SPAWN2_Y (${s2.y} * 256 + 128)\n`;
    return out;
  }

  function buildExport(headerOnly) {
    const id = (document.getElementById('levelId').value || 'e1m4').toLowerCase().replace(/\s/g, '');
    const prefix = id.toUpperCase();
    const W = state.mapW, H = state.mapH;
    const cells = W * H;
    const doors = getDoors();
    const switches = getSwitches();
    const s1 = state.spawn1 || { x: 0, y: 0, angle: 512 };
    const s2 = state.spawn2 || { x: 0, y: 0, angle: 512 };

    let out = '';
    out += `/* VBDOOM_LEVEL_FORMAT ${LEVEL_FORMAT_VERSION} */\n`;
    if (!headerOnly) {
      out += `/* ${id} map: ${W}x${H} */\n`;
      out += `const u8 ${id}_map[${cells}] = {\n`;
      for (let y = 0; y < H; y++) {
        out += '/* Row ' + y + ' */ ';
        for (let x = 0; x < W; x++) out += getCell(x, y) + (x < W - 1 ? ',' : '');
        out += y < H - 1 ? ',\n' : '\n';
      }
      out += '};\n\n';
    }

    out += `/* ${id} spawn and level data */\n`;
    out += `#define ${prefix}_SPAWN_X  (${s1.x} * 256 + 128)\n`;
    out += `#define ${prefix}_SPAWN_Y  (${s1.y} * 256 + 128)\n`;
    out += `#define ${prefix}_SPAWN2_X (${s2.x} * 256 + 128)\n`;
    out += `#define ${prefix}_SPAWN2_Y (${s2.y} * 256 + 128)\n\n`;

    if (!headerOnly) {
      out += `/* Paste initEnemies${prefix}() into enemy.c and initPickups${prefix}() into pickup.c */\n`;
      out += `void initEnemies${prefix}(void) {\n`;
      out += '\tint i; for (i = 0; i < MAX_ENEMIES; i++) { g_enemies[i].active = false; g_enemies[i].enemyType = ETYPE_ZOMBIEMAN; }\n';
      state.enemies.forEach((e, i) => {
        const health = e.type === 0 ? 'ZOMBIE_HEALTH' : e.type === 1 ? 'SGT_HEALTH' : e.type === 2 ? 'IMP_HEALTH' : 'DEMON_HEALTH';
        const angle = e.angle != null ? e.angle : 0;
        out += `\tg_enemies[${i}].x = ${e.tileX} * 256 + 128;\n`;
        out += `\tg_enemies[${i}].y = ${e.tileY} * 256 + 128;\n`;
        out += `\tg_enemies[${i}].angle = ${angle};\n`;
        out += `\tg_enemies[${i}].active = true;\n`;
        out += `\tg_enemies[${i}].enemyType = ETYPE_${['ZOMBIEMAN','SERGEANT','IMP','DEMON'][e.type]};\n`;
        out += `\tg_enemies[${i}].health = ${health};\n`;
      });
      out += '}\n\n';

      out += `void initPickups${prefix}(void) {\n`;
      out += '\tint i; for (i = 0; i < MAX_PICKUPS; i++) g_pickups[i].active = false;\n';
      state.pickups.forEach((p, i) => {
        out += `\tg_pickups[${i}].x = ${p.tileX} * 256 + 128;\n`;
        out += `\tg_pickups[${i}].y = ${p.tileY} * 256 + 128;\n`;
        out += `\tg_pickups[${i}].type = PICKUP_${['AMMO_CLIP','HEALTH_SMALL','HEALTH_LARGE','WEAPON_SHOTGUN','HELMET','ARMOR','SHELLS','WEAPON_ROCKET'][p.type]};\n`;
        out += `\tg_pickups[${i}].active = true;\n`;
      });
      out += '}\n\n';
    }

    out += `/* In loadLevel(), paste the block below into: else if (levelNum == 4) { ... }\n`;
    out += `   (Assumes MAP_X/MAP_Y = 64 and your header is included from RayCasterData.h) */\n\n`;
    out += `/* ========== PASTE THIS INTO gameLoop.c inside else if (levelNum == 4) { ========== */\n`;
    out += `\t{ u16 row; for (row = 0; row < ${H}; row++) copymem((u8*)g_map + row * MAP_X, (const u8*)${id}_map + row * ${W}, ${W}); }\n`;
    out += `\t{ u16 i; for (i = ${H} * MAP_X; i < MAP_CELLS; i++) ((u8*)g_map)[i] = 0; }\n`;
    out += `\t{ u16 row; for (row = 0; row < ${H}; row++) { u16 c; for (c = ${W}; c < MAP_X; c++) ((u8*)g_map)[row * MAP_X + c] = 0; } }\n`;
    out += `\tfPlayerX = ${prefix}_SPAWN_X;\n`;
    out += `\tfPlayerY = ${prefix}_SPAWN_Y;\n`;
    out += `\tfPlayerAng = ${s1.angle};\n`;
    out += `\tinitEnemies${prefix}();\n`;
    out += `\tinitPickups${prefix}();\n`;
    out += `\tinitDoors();\n`;
    doors.forEach((d, i) => {
      const key = d.tileX + ',' + d.tileY;
      const orient = (state.doorOrientations && state.doorOrientations[key]) || 'ns';
      out += `\tregisterDoor(${d.tileX}, ${d.tileY});  /* door ${i}: wall ${orient.toUpperCase()} */\n`;
    });
    switches.forEach((s, i) => {
      const link = state.switchLinks[i];
      const type = link === -1 ? 'SW_EXIT' : 'SW_DOOR';
      const linkArg = link === -1 ? '0' : String(link);
      out += `\tregisterSwitch(${s.tileX}, ${s.tileY}, ${type}, ${linkArg});\n`;
    });
    out += `/* ========== END PASTE ========== */\n`;

    return out;
  }

  function renderBrushGrid() {
    const grid = document.getElementById('brushGrid');
    grid.innerHTML = '';
    for (let t = 0; t <= 11; t++) {
      const cell = document.createElement('div');
          cell.className = 'brush-cell' + (state.brush === t ? ' selected' : '');
          cell.textContent = t;
          cell.title = TILE_NAMES[t];
          cell.style.background = ['#1a0a0a','#8b4513','#555','#4a4a6a','#6a3a2a','#5a4a3a','#7a3525','#454545','#3a3a5a','#8b2a2a','#8b7a2a','#2a4a8b'][t];
          cell.addEventListener('click', () => {
            state.brush = t;
            state.placing = null;
            document.querySelectorAll('.brush-cell').forEach(c => c.classList.remove('selected'));
            document.getElementById('brushSpawn1').classList.remove('selected');
            document.getElementById('brushSpawn2').classList.remove('selected');
            document.getElementById('brushEnemy').value = '';
            document.getElementById('brushPickup').value = '';
            cell.classList.add('selected');
          });
          grid.appendChild(cell);
    }
  }

  document.getElementById('mapSize').addEventListener('change', initMap);
  document.getElementById('levelId').addEventListener('change', () => refreshDoorSwitchLists());

  document.getElementById('brushSpawn1').addEventListener('click', () => {
    state.placing = 'spawn1';
    document.getElementById('brushSpawn2').classList.remove('selected');
    document.getElementById('brushEnemy').value = '';
    document.getElementById('brushPickup').value = '';
    document.getElementById('brushSpawn1').classList.add('selected');
  });
  document.getElementById('brushSpawn2').addEventListener('click', () => {
    state.placing = 'spawn2';
    document.getElementById('brushSpawn1').classList.remove('selected');
    document.getElementById('brushEnemy').value = '';
    document.getElementById('brushPickup').value = '';
    document.getElementById('brushSpawn2').classList.add('selected');
  });
  document.getElementById('brushEnemy').addEventListener('change', function () {
    const v = this.value;
    if (v === '') { state.placing = null; return; }
    if (state.enemies.length >= GAME_LIMITS.MAX_ENEMIES) {
      showToast('Max enemies (' + GAME_LIMITS.MAX_ENEMIES + ') reached. Remove one before adding more.');
      this.value = '';
      return;
    }
    state.placing = 'enemy';
    state.enemyType = parseInt(v, 10);
    document.getElementById('brushSpawn1').classList.remove('selected');
    document.getElementById('brushSpawn2').classList.remove('selected');
    document.getElementById('brushPickup').value = '';
  });
  document.getElementById('brushPickup').addEventListener('change', function () {
    const v = this.value;
    if (v === '') { state.placing = null; return; }
    if (state.pickups.length >= GAME_LIMITS.MAX_PICKUPS) {
      showToast('Max pickups (' + GAME_LIMITS.MAX_PICKUPS + ') reached. Remove one before adding more.');
      this.value = '';
      return;
    }
    state.placing = 'pickup';
    state.pickupType = parseInt(v, 10);
    document.getElementById('brushSpawn1').classList.remove('selected');
    document.getElementById('brushSpawn2').classList.remove('selected');
    document.getElementById('brushEnemy').value = '';
  });

  canvas.addEventListener('click', onGridClick);
  canvas.addEventListener('mousemove', function (e) {
    const { x, y } = xyFromEvent(e);
    const info = (x >= 0 && x < state.mapW && y >= 0 && y < state.mapH) ? getCellInfo(x, y) : null;
    state.hoverCell = info ? { x, y, type: info.type, index: info.index } : null;
    updateTooltip(e.clientX, e.clientY, info);
    updateEntityListHighlight();
    drawGrid();
  });
  canvas.addEventListener('mouseleave', function () {
    state.hoverCell = null;
    updateTooltip(null, null, null);
    updateEntityListHighlight();
    drawGrid();
  });
  canvas.addEventListener('contextmenu', (e) => e.preventDefault());
  canvas.addEventListener('mousedown', function (e) {
    if (e.button === 2) {
      const { x, y } = xyFromEvent(e);
      if (x < 0 || x >= state.mapW || y < 0 || y >= state.mapH) return;
      if (state.spawn1 && state.spawn1.x === x && state.spawn1.y === y) {
        state.spawn1 = null;
        refreshEntityList();
        drawGrid();
        return;
      }
      if (state.spawn2 && state.spawn2.x === x && state.spawn2.y === y) {
        state.spawn2 = null;
        refreshEntityList();
        drawGrid();
        return;
      }
      const ei = state.enemies.findIndex(en => en.tileX === x && en.tileY === y);
      if (ei >= 0) {
        state.enemies.splice(ei, 1);
        refreshEntityList();
        drawGrid();
        return;
      }
      const pi = state.pickups.findIndex(p => p.tileX === x && p.tileY === y);
      if (pi >= 0) {
        state.pickups.splice(pi, 1);
        refreshEntityList();
        drawGrid();
        return;
      }
      return;
    }
    if (e.button !== 0) return;
    let last = xyFromEvent(e);
    const paint = function (moveE) {
      const { x, y } = xyFromEvent(moveE);
      if (x >= 0 && x < state.mapW && y >= 0 && y < state.mapH && (x !== last.x || y !== last.y) && !state.placing) {
        setCell(x, y, state.brush);
        refreshDoorSwitchLists();
        drawGrid();
        last = { x, y };
      }
    };
    const up = () => { canvas.removeEventListener('mousemove', paint); document.removeEventListener('mouseup', up); };
    canvas.addEventListener('mousemove', paint);
    document.addEventListener('mouseup', up);
  });

  document.getElementById('btnSaveJson').addEventListener('click', () => {
    const json = JSON.stringify(buildJson(), null, 2);
    const blob = new Blob([json], { type: 'application/json' });
    const a = document.createElement('a');
    a.href = URL.createObjectURL(blob);
    a.download = 'level.json';
    a.click();
    URL.revokeObjectURL(a.href);
  });

  document.getElementById('btnLoadJson').addEventListener('click', () => {
    document.getElementById('loadJsonText').value = '';
    document.getElementById('loadJsonModal').classList.remove('hidden');
  });
  document.getElementById('btnLoadJsonChooseFile').addEventListener('click', () => {
    document.getElementById('loadJsonInput').click();
  });
  document.getElementById('loadJsonInput').addEventListener('change', function () {
    const f = this.files && this.files[0];
    if (!f) return;
    const r = new FileReader();
    r.onload = () => { document.getElementById('loadJsonText').value = r.result; };
    r.readAsText(f);
    this.value = '';
  });
  document.getElementById('btnLoadJsonApply').addEventListener('click', () => {
    try {
      const text = document.getElementById('loadJsonText').value.trim();
      if (!text) { alert('Paste JSON or choose a file.'); return; }
      const obj = JSON.parse(text);
      if (loadFromJson(obj)) document.getElementById('loadJsonModal').classList.add('hidden');
    } catch (err) {
      alert('Invalid JSON: ' + err.message);
    }
  });
  document.getElementById('btnCloseLoadJson').addEventListener('click', () => {
    document.getElementById('loadJsonModal').classList.add('hidden');
  });

  /* ---- Load .h (C header) map import ---- */

  /**
   * Parse a C header file containing a VB Doom map array and spawn defines.
   * Returns { levelId, mapW, mapH, map: number[], spawn1, spawn2 } or null on failure.
   */
  function parseHFile(text) {
    /* 1. Find the map array: const u8 <name>_map[<cells>] = { ... }; */
    const arrMatch = text.match(/const\s+u8\s+(\w+)_map\s*\[\s*(\d+)\s*\]\s*=\s*\{/);
    if (!arrMatch) return null;
    const levelId = arrMatch[1];          /* e.g. "e1m4" */
    const declaredCells = parseInt(arrMatch[2], 10);

    /* Extract body between the opening { and closing }; */
    const openIdx = text.indexOf('{', arrMatch.index);
    const closeIdx = text.indexOf('};', openIdx);
    if (openIdx < 0 || closeIdx < 0) return null;
    const body = text.substring(openIdx + 1, closeIdx);

    /* Parse all integers from the body (skip comments and whitespace) */
    const values = [];
    const numRe = /\b(\d+)\b/g;
    let m;
    /* Strip C comments first so "/* Row 12 */" row numbers aren't counted as tile values */
    const stripped = body.replace(/\/\*[\s\S]*?\*\//g, '');
    while ((m = numRe.exec(stripped)) !== null) {
      values.push(parseInt(m[1], 10));
    }

    if (values.length === 0) return null;
    if (values.length !== declaredCells) {
      /* Accept anyway if close, but warn */
      console.warn('Header declared ' + declaredCells + ' cells but found ' + values.length + ' values.');
    }

    /* 2. Determine dimensions */
    let mapW = 0, mapH = 0;
    /* Try the comment: "e1m4 map: 32x32" */
    const dimComment = text.match(/map:\s*(\d+)\s*x\s*(\d+)/i);
    if (dimComment) {
      mapW = parseInt(dimComment[1], 10);
      mapH = parseInt(dimComment[2], 10);
    }
    /* Fall back: count "Row N" comments to get height, derive width */
    if (!mapW || !mapH) {
      const rowComments = body.match(/\/\*\s*Row\s+\d+/g);
      if (rowComments && rowComments.length > 0) {
        mapH = rowComments.length;
        mapW = Math.round(values.length / mapH);
      }
    }
    /* Last resort: assume square */
    if (!mapW || !mapH || mapW * mapH !== values.length) {
      const sq = Math.sqrt(values.length);
      if (sq === Math.floor(sq)) {
        mapW = sq;
        mapH = sq;
      } else {
        /* Try common non-square sizes */
        const tryWidths = [64, 48, 32];
        for (let i = 0; i < tryWidths.length; i++) {
          if (values.length % tryWidths[i] === 0) {
            mapW = tryWidths[i];
            mapH = values.length / tryWidths[i];
            break;
          }
        }
      }
    }
    if (!mapW || !mapH || mapW * mapH !== values.length) return null;

    /* 3. Parse spawn defines: #define <PREFIX>_SPAWN_X (tile * 256 + 128) */
    const prefix = levelId.toUpperCase();
    function parseSpawnCoord(suffix) {
      const re = new RegExp('#define\\s+' + prefix + '_' + suffix + '\\s+\\(\\s*(\\d+)\\s*\\*\\s*256\\s*\\+\\s*128\\s*\\)');
      const sm = text.match(re);
      return sm ? parseInt(sm[1], 10) : null;
    }
    const s1x = parseSpawnCoord('SPAWN_X');
    const s1y = parseSpawnCoord('SPAWN_Y');
    const s2x = parseSpawnCoord('SPAWN2_X');
    const s2y = parseSpawnCoord('SPAWN2_Y');

    const spawn1 = (s1x !== null && s1y !== null) ? { x: s1x, y: s1y, angle: 512 } : null;
    const spawn2 = (s2x !== null && s2y !== null) ? { x: s2x, y: s2y, angle: 512 } : null;

    return { levelId: levelId, mapW: mapW, mapH: mapH, map: values, spawn1: spawn1, spawn2: spawn2 };
  }

  document.getElementById('btnLoadH').addEventListener('click', () => {
    document.getElementById('loadHInput').click();
  });
  document.getElementById('loadHInput').addEventListener('change', function () {
    const f = this.files && this.files[0];
    if (!f) return;
    const r = new FileReader();
    r.onload = function () {
      const parsed = parseHFile(r.result);
      if (!parsed) {
        alert('Could not find a valid map array in the .h file.\nExpected: const u8 <name>_map[N] = { ... };');
        return;
      }
      /* Build a JSON-like object and load via loadFromJson */
      const obj = {
        version: LEVEL_FORMAT_VERSION,
        mapW: parsed.mapW,
        mapH: parsed.mapH,
        map: parsed.map,
        spawn1: parsed.spawn1,
        spawn2: parsed.spawn2,
        enemies: [],
        pickups: [],
        switchLinks: [],
        doorOrientations: {}
      };
      if (loadFromJson(obj)) {
        /* Set the level ID field to match the loaded map */
        document.getElementById('levelId').value = parsed.levelId;
        /* Update map size dropdown if it matches a known size */
        const sizeKey = parsed.mapW + ',' + parsed.mapH;
        const sizeSelect = document.getElementById('mapSize');
        for (let i = 0; i < sizeSelect.options.length; i++) {
          if (sizeSelect.options[i].value === sizeKey) {
            sizeSelect.value = sizeKey;
            break;
          }
        }
        showToast('Loaded ' + parsed.levelId + ' (' + parsed.mapW + 'x' + parsed.mapH + ') from header file. Enemies/pickups not included in .h files — add them manually or load a JSON.');
      }
    };
    r.readAsText(f);
    this.value = '';
  });

  document.getElementById('btnHelp').addEventListener('click', () => {
    document.getElementById('helpModal').classList.remove('hidden');
  });
  document.getElementById('btnCloseHelp').addEventListener('click', () => {
    document.getElementById('helpModal').classList.add('hidden');
  });

  function getExportFileName() {
    const raw = (document.getElementById('levelId').value || 'e1m4').trim();
    const safe = raw.toLowerCase().replace(/\s+/g, '').replace(/[^a-z0-9_]/g, '') || 'map';
    return safe + '.h';
  }

  function openExportModal() {
    const fn = getExportFileName();
    document.getElementById('exportFileName').textContent = fn;
    const checklistEl = document.getElementById('exportFileNameChecklist');
    if (checklistEl) checklistEl.textContent = fn;
  }
  document.getElementById('btnExport').addEventListener('click', () => {
    document.getElementById('exportText').value = buildExport(false);
    openExportModal();
    document.getElementById('exportModal').classList.remove('hidden');
  });
  document.getElementById('btnExportHeader').addEventListener('click', () => {
    document.getElementById('exportText').value = buildExport(true);
    openExportModal();
    document.getElementById('exportModal').classList.remove('hidden');
  });
  document.getElementById('btnCloseExport').addEventListener('click', () => {
    document.getElementById('exportModal').classList.add('hidden');
  });

  document.getElementById('btnSaveAsHeader').addEventListener('click', () => {
    const fileName = getExportFileName();
    /* Save only map + spawn defines so the .h can be #included from RayCasterData.h without pulling in enemy/pickup types */
    const text = buildExportHeaderFileContent();
    const blob = new Blob([text], { type: 'text/plain;charset=utf-8' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = fileName;
    a.click();
    URL.revokeObjectURL(url);
  });

  document.getElementById('btnLinkMode').addEventListener('click', function () {
    state.linkMode = !state.linkMode;
    this.classList.toggle('selected', state.linkMode);
    if (!state.linkMode) {
      state.pendingSwitchIndex = null;
      document.getElementById('linkModeExit').classList.add('hidden');
      document.getElementById('status').textContent = 'Map ' + state.mapW + '×' + state.mapH + '. Click or drag to paint.';
    }
    drawGrid();
  });

  document.getElementById('btnSetExit').addEventListener('click', () => {
    if (state.pendingSwitchIndex === null) return;
    state.switchLinks[state.pendingSwitchIndex] = -1;
    state.pendingSwitchIndex = null;
    refreshDoorSwitchLists();
    document.getElementById('linkModeExit').classList.add('hidden');
    document.getElementById('status').textContent = 'Map ' + state.mapW + '×' + state.mapH + '. Click or drag to paint.';
    drawGrid();
  });

  const previewCanvas = document.getElementById('previewCanvas');
  const previewCtx = previewCanvas.getContext('2d');
  const PREVIEW_HEIGHT = 200;
  function resizePreview() {
    const cw = canvas.width || state.mapW * CELL_PX;
    const w = Math.min(cw, MAX_CANVAS);
    previewCanvas.width = w;
    previewCanvas.height = PREVIEW_HEIGHT;
    if (state.map) drawRaycastPreview(previewCtx, previewCanvas.width, previewCanvas.height);
  }
  const keysPressed = {};
  const PREVIEW_MOVE_SPEED = 0.08;
  const PREVIEW_TURN_SPEED = 4;
  function isPreviewKey(key) {
    return key === 'ArrowUp' || key === 'ArrowDown' || key === 'ArrowLeft' || key === 'ArrowRight' ||
           key === 'w' || key === 'W' || key === 's' || key === 'S' || key === 'a' || key === 'A' || key === 'd' || key === 'D';
  }
  function previewCellBlocked(x, y) {
    const gx = Math.floor(x), gy = Math.floor(y);
    if (gx < 0 || gx >= state.mapW || gy < 0 || gy >= state.mapH) return true;
    const tile = getCell(gx, gy);
    if (tile === 0) return false;
    if (PREVIEW_PASSTHROUGH_TILES.includes(tile)) return false; // walk through doors/switches in preview
    return true;
  }
  function applyPreviewInput() {
    if (document.activeElement !== previewCanvas) return;
    const fwd = keysPressed['ArrowUp'] || keysPressed['w'] || keysPressed['W'];
    const back = keysPressed['ArrowDown'] || keysPressed['s'] || keysPressed['S'];
    const left = keysPressed['ArrowLeft'] || keysPressed['a'] || keysPressed['A'];
    const right = keysPressed['ArrowRight'] || keysPressed['d'] || keysPressed['D'];
    if (!fwd && !back && !left && !right) return;
    const player = getPreviewPlayer();
    let nx = player.x, ny = player.y;
    let nangle = player.angle;
    const angleRad = (nangle / 1024) * 2 * Math.PI - Math.PI / 2;
    if (fwd) { nx += Math.cos(angleRad) * PREVIEW_MOVE_SPEED; ny += Math.sin(angleRad) * PREVIEW_MOVE_SPEED; }
    if (back) { nx -= Math.cos(angleRad) * PREVIEW_MOVE_SPEED; ny -= Math.sin(angleRad) * PREVIEW_MOVE_SPEED; }
    if (left) nangle = (nangle - PREVIEW_TURN_SPEED + 1024) % 1024;
    if (right) nangle = (nangle + PREVIEW_TURN_SPEED) % 1024;
    let finalX = nx, finalY = ny;
    if (previewCellBlocked(nx, ny)) {
      const slideX = !previewCellBlocked(nx, player.y);
      const slideY = !previewCellBlocked(player.x, ny);
      if (slideX && slideY) {
        finalX = nx;
        finalY = player.y;
      } else if (slideX) {
        finalX = nx;
        finalY = player.y;
      } else if (slideY) {
        finalX = player.x;
        finalY = ny;
      } else {
        finalX = player.x;
        finalY = player.y;
      }
    }
    state.previewPos = { x: finalX, y: finalY };
    state.previewAngle = nangle;
  }

  function tickPreview() {
    applyPreviewInput();
    if (previewCanvas.width && previewCanvas.height && state.map)
      drawRaycastPreview(previewCtx, previewCanvas.width, previewCanvas.height);
    drawPreviewOnMap();
    requestAnimationFrame(tickPreview);
  }
  tickPreview();

  window.addEventListener('keydown', function (e) {
    if (!isPreviewKey(e.key)) return;
    if (document.activeElement === previewCanvas) e.preventDefault();
    keysPressed[e.key] = true;
  });
  window.addEventListener('keyup', function (e) {
    if (isPreviewKey(e.key)) keysPressed[e.key] = false;
  });
  previewCanvas.addEventListener('blur', function () {
    Object.keys(keysPressed).forEach(k => { keysPressed[k] = false; });
  });

  document.getElementById('btnResetPreview').addEventListener('click', () => {
    state.previewPos = state.spawn1
      ? { x: state.spawn1.x + 0.5, y: state.spawn1.y + 0.5 }
      : { x: state.mapW / 2, y: state.mapH / 2 };
    state.previewAngle = state.spawn1 ? (state.spawn1.angle != null ? state.spawn1.angle : 512) : 0;
  });

  window.addEventListener('resize', resizePreview);
  initMap();
  renderBrushGrid();
  resizePreview();
})();
