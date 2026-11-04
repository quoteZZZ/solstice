/* Solstice · 世界画面审计探针（只读源码；写截图与 metrics.json）
   跑法：node tests/review-run/world-probe.js
   输出：tests/review-run/world-shots/ + metrics.json */
const path = require('path'), fs = require('fs');
const { pathToFileURL } = require('url');
const { chromium } = require('playwright-core');
const { loadPNG, region, scan, px, luma, hex } = require('./pngstat');

const PAGE = pathToFileURL(path.join(__dirname, '..', '..', 'index.html')).href;
const OUT = path.join(__dirname, 'world-shots');
fs.mkdirSync(OUT, { recursive: true });
const M = { shots: [], failures: [] };
const shot = async (page, name, clip) => {
  const f = path.join(OUT, name + '.png');
  await page.waitForTimeout(260);
  await page.screenshot({ path: f, clip });
  M.shots.push(name);
  console.log('shot', name);
};
/* 相机位姿是直接改的，three 的 matrixWorldInverse 只在 render 时更新；
   探针必须自己同步，否则所有投影读的都是上一帧的镜头。 */
const SYNC = `const sync = () => { const c = R.camera; c.updateMatrixWorld(true);
  c.matrixWorldInverse.copy(c.matrixWorld).invert(); c.updateProjectionMatrix(); };`;
const PROJ = `const r = R.canvas.getBoundingClientRect();
  const proj = (x,y,z) => { const v = new THREE.Vector3(x,y,z).project(R.camera);
    return { x: (v.x*.5+.5)*r.width + r.left, y: (-v.y*.5+.5)*r.height + r.top, z: v.z }; };`;

(async () => {
  const browser = await chromium.launch({ channel: 'msedge', headless: true });
  const page = await browser.newPage({ viewport: { width: 1680, height: 1000 }, deviceScaleFactor: 1 });
  page.on('pageerror', e => M.failures.push('[pageerror] ' + e.message));
  page.on('console', m => { if (m.type() === 'error') M.failures.push('[console] ' + m.text()); });
  await page.goto(PAGE);
  await page.waitForTimeout(1500);

  /* ───────── 0. 样本地：13×13，含草/树/灌木/芦苇/水带/岩坡 ───────── */
  await page.evaluate(() => {
    const { S, D, UI } = window.__dbg;
    UI.enterFromCreator();
    const g = id => D.byId.get(id);
    const water = g('terrain.mineral.0.matter.water');
    const rock = g('terrain.mineral.0.matter.rock');
    const grass = g('flora.wood.1.matter.grass');
    const tree = g('flora.wood.2.matter.tree');
    const shrub = g('flora.wood.1.matter.shrub');
    const reed = g('flora.water.1.matter.reed');
    const rabbit = g('fauna.earth.1.matter.rabbit');
    for (let x = -6; x <= 6; x++) for (let y = -6; y <= 6; y++) S.addCell(x, y, { h: 0.30 });
    for (let x = -6; x <= 6; x++) S.cmd.cast(water, x, -2);              // 13 格水带
    for (let x = 4; x <= 6; x++) for (let y = 3; y <= 6; y++) S.cmd.cast(rock, x, y);
    for (let x = -6; x <= 2; x++) for (let y = -6; y <= 6; y++) {
      const c = S.at(x, y); if (!c || c.kind === 'water' || c.kind === 'rock') continue;
      S.spawn(grass, c, {});
    }
    for (const [x, y] of [[-1, 1], [0, 1], [1, 1], [-2, 2], [3, -4], [3, -3], [-4, -5], [-5, -5]]) S.spawn(tree, S.at(x, y), {});
    for (const [x, y] of [[2, 3], [2, 4], [-3, -1], [-4, 0]]) S.spawn(shrub, S.at(x, y), {});
    for (const [x, y] of [[-3, -2], [-2, -2], [1, -2], [2, -2]]) S.spawn(reed, S.at(x, y), {});
    for (const [x, y] of [[0, -3], [1, -4], [-2, -4]]) S.spawn(rabbit, S.at(x, y), {});
    for (const c of S.W.cells.values()) S.classifyAround(c);
    S.W.sun = 2; S.W.essence = 999;
    UI.updateHud(); UI.renderRight(); UI.renderHand();
  });
  await page.waitForTimeout(800);

  /* ───────── 1. 场景清单 ───────── */
  M.inventory = await page.evaluate(() => {
    const { S, R } = window.__dbg;
    const kinds = {}; let ents = 0;
    for (const c of S.W.cells.values()) { kinds[c.kind] = (kinds[c.kind] || 0) + 1; ents += c.ents.length; }
    let casters = 0, meshes = 0, shadowRecv = 0;
    R.scene.traverse(o => { if (o.isMesh) { meshes++; if (o.castShadow) casters++; if (o.receiveShadow) shadowRecv++; } });
    const texUuid = new Set();
    let front = 0, back = 0;
    for (const list of R.standees.values()) for (const m of list) {
      if (m.material.color.r > 0.3) { front++; texUuid.add(m.material.map.uuid); } else back++;
    }
    const info = R.renderer.info;
    return {
      cells: S.W.cells.size, kinds, ents,
      standeeMeshes: front + back, front, back, uniqueStandeeTextures: texUuid.size,
      meshes, casters, shadowRecv,
      heightMax: +Math.max(...[...S.W.cells.values()].map(c => R.hOf(c))).toFixed(3),
      drawCalls: info.render.calls, triangles: info.render.triangles,
      geometries: info.memory.geometries, textures: info.memory.textures
    };
  });

  /* ───────── 2. 立牌个体：世界尺寸 / 越界 / 屏幕矩形 / 朝向 / 明度 ───────── */
  await page.evaluate(() => {
    const { R } = window.__dbg;
    R.cam.yaw = Math.PI / 4; R.cam.pitch = 0.52; R.cam.dist = 8.6;
    R.cam.target.set(0, 0.22, 0); R.cam.place(); R.cam.snapGoal();
  });
  await page.waitForTimeout(300);
  M.standees = await page.evaluate(`(() => {
    const { S, R } = window.__dbg; const THREE = window.THREE;
    ${SYNC} ${PROJ}
    sync();
    const rows = [];
    for (const [key, list] of R.standees) {
      const [cx, cy] = key.split(',').map(Number);
      const cell = S.at(cx, cy);
      for (const m of list) {
        if (!(m.material.color.r > 0.3)) continue;
        const geo = m.geometry.parameters;
        const box = new THREE.Box3().setFromObject(m);
        const sz = box.getSize(new THREE.Vector3());
        const hw = geo.width / 2;
        const pts = [[-hw,0],[hw,0],[-hw,geo.height],[hw,geo.height]].map(([x,y]) =>
          proj(...new THREE.Vector3(x,y,0).applyMatrix4(m.matrixWorld).toArray()));
        const sx0 = Math.min(...pts.map(p=>p.x)), sx1 = Math.max(...pts.map(p=>p.x));
        const sy0 = Math.min(...pts.map(p=>p.y)), sy1 = Math.max(...pts.map(p=>p.y));
        const own = Math.max(0.001, (Math.min(box.max.x, cx+0.5)-Math.max(box.min.x, cx-0.5)) *
          (Math.min(box.max.z, cy+0.5)-Math.max(box.min.z, cy-0.5)));
        rows.push({
          cell: [cx, cy], kind: cell && cell.kind,
          w: +geo.width.toFixed(3), h: +geo.height.toFixed(3),
          bboxW: +sz.x.toFixed(3), bboxD: +sz.z.toFixed(3),
          outX: +Math.max(0, box.max.x-(cx+0.5)) + Math.max(0, (cx-0.5)-box.min.x),
          outRatio: +(1 - own / Math.max(0.001, sz.x*sz.z)).toFixed(3),
          scr: [Math.round(sx0), Math.round(sy0), Math.round(sx1), Math.round(sy1)],
          scrW: +(sx1-sx0).toFixed(1), scrH: +(sy1-sy0).toFixed(1),
          tex: m.material.map.uuid, tint: +m.material.color.r.toFixed(3),
          rotY: +m.rotation.y.toFixed(3), rotX: +m.rotation.x.toFixed(3),
          pos: [+m.position.x.toFixed(3), +m.position.z.toFixed(3)],
          baseY: +m.position.y.toFixed(3), groundY: +(cell ? R.hOf(cell) : 0).toFixed(3)
        });
      }
    }
    return { rows };
  })()`);
  {
    const rows = M.standees.rows;
    const by = rows.filter(r => r.h > 1.0), sm = rows.filter(r => r.h <= 0.9);
    const agg = a => a.length ? {
      n: a.length,
      wMin: Math.min(...a.map(r => r.w)), wMax: Math.max(...a.map(r => r.w)),
      hMin: Math.min(...a.map(r => r.h)), hMax: Math.max(...a.map(r => r.h)),
      outMax: Math.max(...a.map(r => r.outRatio)),
      outMean: +(a.reduce((s, r) => s + r.outRatio, 0) / a.length).toFixed(3),
      scrHMin: Math.min(...a.map(r => r.scrH)), scrHMax: Math.max(...a.map(r => r.scrH)),
      texN: new Set(a.map(r => r.tex)).size
    } : null;
    const rotY = [...new Set(rows.map(r => r.rotY))];
    const tints = rows.map(r => r.tint);
    M.standeeSummary = {
      tree: agg(by), small: agg(sm),
      rotYUnique: rotY.length, rotY: rotY.slice(0, 3),
      tintMin: Math.min(...tints), tintMax: Math.max(...tints),
      gapMax: +Math.max(...rows.map(r => r.baseY - r.groundY)).toFixed(3),
      gapMin: +Math.min(...rows.map(r => r.baseY - r.groundY)).toFixed(3),
      floating: rows.filter(r => r.baseY > r.groundY + 0.001).length,
      sunk: rows.filter(r => r.baseY < r.groundY - 0.03).length
    };
    /* 相邻格立牌屏幕矩形重叠（同一屏里互相压边） */
    const inter = (a, b) => Math.max(0, Math.min(a[2], b[2]) - Math.max(a[0], b[0])) *
      Math.max(0, Math.min(a[3], b[3]) - Math.max(a[1], b[1]));
    let pairs = 0, overlapPairs = 0, ratios = [];
    for (let i = 0; i < rows.length; i++) for (let j = i + 1; j < rows.length; j++) {
      const a = rows[i], b = rows[j];
      if (Math.abs(a.cell[0] - b.cell[0]) + Math.abs(a.cell[1] - b.cell[1]) !== 1) continue;
      pairs++;
      const ia = inter(a.scr, b.scr);
      const ra = ia / Math.min((a.scr[2]-a.scr[0])*(a.scr[3]-a.scr[1]), (b.scr[2]-b.scr[0])*(b.scr[3]-b.scr[1]));
      if (ia > 0) { overlapPairs++; ratios.push(ra); }
    }
    M.standeeSummary.adjacentPairs = pairs;
    M.standeeSummary.overlapPairs = overlapPairs;
    M.standeeSummary.overlapRatioMean = +(ratios.reduce((s, v) => s + v, 0) / Math.max(1, ratios.length)).toFixed(3);
    M.standeeSummary.overlapRatioMax = +Math.max(0, ...ratios).toFixed(3);
  }
  M.texReuse = await page.evaluate(() => {
    const { R } = window.__dbg;
    const m = new Map();
    for (const list of R.standees.values()) for (const o of list) {
      if (!(o.material.color.r > 0.3)) continue;
      m.set(o.material.map.uuid, (m.get(o.material.map.uuid) || 0) + 1);
    }
    const arr = [...m.entries()].sort((a, b) => b[1] - a[1]);
    return { textures: arr.length, top: arr.slice(0, 5).map(e => e[1]), total: arr.reduce((s, e) => s + e[1], 0) };
  });

  /* ───────── 3. 相机 8 方位 × 4 俯仰 + 缩放两端 ───────── */
  M.cam = await page.evaluate(`(() => {
    const { S, R } = window.__dbg; const THREE = window.THREE;
    ${SYNC} ${PROJ}
    const PITCHES = [0.26, 0.52, 0.95, 1.45];
    const cell = S.at(0, 0), top = R.hOf(cell);
    const quad = () => {
      const pts = [[-0.5,-0.5],[0.5,-0.5],[0.5,0.5],[-0.5,0.5]].map(([dx,dz]) => proj(cell.x+dx, top, cell.y+dz));
      let area = 0;
      for (let i = 0; i < 4; i++) { const a = pts[i], b = pts[(i+1)%4]; area += a.x*b.y - b.x*a.y; }
      const w = Math.hypot(pts[1].x-pts[0].x, pts[1].y-pts[0].y);
      const h = Math.hypot(pts[3].x-pts[0].x, pts[3].y-pts[0].y);
      return { area: +Math.abs(area/2).toFixed(0), w: +w.toFixed(1), h: +h.toFixed(1),
        ratio: +(h/w).toFixed(3), skew: +(Math.atan2(pts[1].y-pts[0].y, pts[1].x-pts[0].x)*180/Math.PI).toFixed(1) };
    };
    const standeeH = () => {
      let mx = 0, n = 0;
      for (const [key, list] of R.standees) {
        const [cx, cy] = key.split(',').map(Number);
        if (Math.abs(cx) > 1 || Math.abs(cy) > 1) continue;
        for (const m of list) {
          if (!(m.material.color.r > 0.3)) continue;
          const g = m.geometry.parameters, hw = g.width/2;
          const pts = [[-hw,0],[hw,0],[-hw,g.height],[hw,g.height]].map(([x,y]) =>
            proj(...new THREE.Vector3(x,y,0).applyMatrix4(m.matrixWorld).toArray()));
          const hh = Math.max(...pts.map(p=>p.y)) - Math.min(...pts.map(p=>p.y));
          if (hh > mx) mx = hh; n++;
        }
      }
      return { h: +mx.toFixed(1), n };
    };
    const out = [];
    for (let yi = 0; yi < 8; yi++) for (let pi = 0; pi < 4; pi++) {
      R.cam.yaw = yi*Math.PI/4; R.cam.pitch = PITCHES[pi]; R.cam.dist = 8.6;
      R.cam.target.set(0, 0.22, 0); R.cam.place(); R.cam.snapGoal(); sync();
      const q = quad(), s = standeeH();
      out.push({ yawDeg: yi*45, pitchIdx: pi+1, pitchDeg: Math.round(PITCHES[pi]*180/Math.PI),
        camY: +R.camera.position.y.toFixed(2), cell: q, standeePx: s.h, standeeCount: s.n,
        standeeVsCell: +(s.h / Math.max(1, q.h)).toFixed(2) });
    }
    const zooms = [];
    for (const d of [24, 14, 8.6, 5.2, 3.4, 3]) {
      R.cam.yaw = Math.PI/4; R.cam.pitch = 0.52; R.cam.dist = d;
      R.cam.target.set(0, 0.22, 0); R.cam.place(); R.cam.snapGoal(); sync();
      const q = quad(), s = standeeH();
      zooms.push({ dist: d, camY: +R.camera.position.y.toFixed(2), cell: q, standeePx: s.h,
        standeeVsCell: +(s.h / Math.max(1, q.h)).toFixed(2) });
    }
    R.cam.dist = 8.6; R.cam.pitch = 0.52; R.cam.place(); R.cam.snapGoal(); sync();
    return { grid: out, zooms };
  })()`);

  /* 相机九宫格截图 */
  await page.evaluate(() => { const { R } = window.__dbg; R.cam.target.set(0, 0.22, 0); R.cam.snapGoal(); });
  for (let i = 0; i < 8; i++) {
    await page.evaluate(i => {
      const { R } = window.__dbg;
      R.cam.yaw = i * Math.PI / 4; R.cam.pitch = 0.52; R.cam.dist = 8.6;
      R.cam.place(); R.cam.snapGoal();
    }, i);
    await shot(page, 'w-yaw-' + i);
  }
  for (let p = 0; p < 4; p++) {
    await page.evaluate(p => {
      const { R } = window.__dbg;
      R.cam.yaw = Math.PI / 4; R.cam.pitch = [0.26, 0.52, 0.95, 1.45][p]; R.cam.dist = 8.6;
      R.cam.place(); R.cam.snapGoal();
    }, p);
    await shot(page, 'w-pitch-F' + (p + 1));
  }
  for (const [n, d] of [['max', 24], ['min', 3.4]]) {
    await page.evaluate(d => {
      const { R } = window.__dbg;
      R.cam.yaw = Math.PI / 4; R.cam.pitch = 0.52; R.cam.dist = d;
      R.cam.place(); R.cam.snapGoal();
    }, d);
    await shot(page, 'w-zoom-' + n);
  }

  /* ───────── 4. 水面：台阶、接缝、材质、边界线 ───────── */
  M.water = await page.evaluate(`(() => {
    const { S, R } = window.__dbg; const THREE = window.THREE;
    ${SYNC} ${PROJ}
    const waters = [...S.W.cells.values()].filter(c => c.kind === 'water');
    const waterTop = R.hOf(waters[0]);
    const landTop = R.hOf(S.at(-6, -1));
    const wave = (lx, ly, t) => Math.sin(t*1.7 + lx*4.2 + ly*3.1)*0.024 + Math.sin(t*1.1 + ly*7.3 - lx*2.2)*0.011;
    let maxSeam = 0, seamAt = null;
    for (let ti = 0; ti < 24; ti++) { const t = ti*0.35;
      for (let ly = -0.499; ly <= 0.499; ly += 0.1) {
        const d = Math.abs(wave(0.499, ly, t) - wave(-0.499, ly, t));
        if (d > maxSeam) { maxSeam = d; seamAt = { t: +t.toFixed(2), ly: +ly.toFixed(2) }; }
      }
    }
    R.cam.yaw = Math.PI/4; R.cam.pitch = 0.52; R.cam.dist = 8.6;
    R.cam.target.set(0, 0.22, 0); R.cam.place(); R.cam.snapGoal(); sync();
    const a = proj(0, waterTop, -2), b = proj(0, waterTop + 0.07, -2);
    const pxPerWorldY = Math.abs(a.y - b.y) / 0.07;
    let topMat = null, waterMeshes = 0;
    R.scene.traverse(o => {
      if (o.isMesh && o.material && o.material.type === 'MeshStandardMaterial' && o.material.transparent) {
        waterMeshes++;
        if (!topMat) topMat = { rough: o.material.roughness, metal: o.material.metalness, op: o.material.opacity, env: !!o.material.envMap };
      }
    });
    /* 界缘线段：数「有界缘的面」占全部格面的比例 */
    let edgeSegs = 0;
    for (const c of S.W.cells.values()) if (c.mesh) c.mesh.traverse(o => {
      if (o.isLineSegments && o.geometry.attributes.position) edgeSegs += o.geometry.attributes.position.count / 2;
    });
    return { waterCells: waters.length, waterTop: +waterTop.toFixed(3), landTop: +landTop.toFixed(3),
      step: +(landTop - waterTop).toFixed(3), seamWorld: +maxSeam.toFixed(4), seamAt,
      pxPerWorldY: +pxPerWorldY.toFixed(1), seamPx: +(maxSeam*pxPerWorldY).toFixed(1),
      waterMat: topMat, waterMeshes, edgeSegs,
      cells: S.W.cells.size, facesTotal: S.W.cells.size*4, edgeFaceRatio: +(edgeSegs/(S.W.cells.size*4)).toFixed(3) };
  })()`);

  /* 水面近景 / 低角 / 交界 */
  await page.evaluate(() => {
    const { R } = window.__dbg;
    R.cam.target.set(-2, 0.2, -2); R.cam.snapGoal();
    R.cam.dist = 6.5; R.cam.pitch = 0.62; R.cam.yaw = 0.9; R.cam.place(); R.cam.snapGoal();
  });
  await shot(page, 'w-water-near');
  await page.screenshot({ path: path.join(OUT, 'w-water-crop.png'), clip: { x: 500, y: 300, width: 640, height: 420 } });
  await page.evaluate(() => {
    const { R } = window.__dbg;
    R.cam.target.set(0, 0.2, -1.2); R.cam.snapGoal();
    R.cam.dist = 4.2; R.cam.pitch = 0.30; R.cam.yaw = 1.2; R.cam.place(); R.cam.snapGoal();
  });
  await shot(page, 'w-water-edge-low');

  /* ───────── 5. 痕迹 / 预览 / 扩散环 ───────── */
  M.traces = await page.evaluate(`(() => {
    const { S, D, R } = window.__dbg;
    S.W.traces = [];
    const cast = (id, x, y) => S.cmd.cast(D.byId.get(id), x, y);
    /* 同一格三种元素 → 三个同心环 */
    cast('flora.wood.1.matter.grass', 0, 3);
    cast('flora.wood.1.matter.grass', 0, 3);
    cast('flora.wood.1.matter.grass', 0, 3);
    cast('terrain.mineral.0.matter.water', 0, 3);
    cast('terrain.mineral.0.matter.rock', 0, 3);
    for (const [x, y] of [[1, 3], [2, 3], [1, 2], [2, 2], [-1, 3]]) cast('flora.wood.1.matter.grass', x, y);
    R.trace.sync();
    const rings = R.traceGroup.children.filter(c => c.geometry.type === 'RingGeometry');
    return {
      count: S.W.traces.length, rings: rings.length,
      opacityOne: rings[0] && rings[0].material.opacity,
      alphaStack3: +(1 - Math.pow(1 - 0.16, 3)).toFixed(3),
      radii: rings.map(r => +r.geometry.parameters.outerRadius.toFixed(3)),
      bandWorld: rings[0] ? +(rings[0].geometry.parameters.outerRadius - rings[0].geometry.parameters.innerRadius).toFixed(3) : null,
      colors: [...new Set(rings.map(r => r.material.color.getHexString()))],
      sameCellTiers: S.W.traces.filter(t => t.x === 0 && t.y === 3).map(t => ({ el: t.el, tier: t.tier, count: t.count })),
      lifeTiers: S.W.traces.map(t => t.life),
      maxTraces: 160, tier1Life: S.traceLife(D.byId.get('flora.wood.1.matter.grass'))
    };
  })()`);

  /* 三环同格 + 预览 + 扩散环 同屏 */
  await page.evaluate(() => {
    const { S, D, R, UI } = window.__dbg;
    const card = D.byId.get('terrain.mineral.0.matter.water');
    S.W.hand.unshift(card); S.W.sel = 0;
    R.marks.set(S.castRange(card, S.at(1, 3)));
    R.marks.cursor(S.at(1, 3));
    R.marks.pulse(0, 3, '#7fd8f0', 0.12);
    UI.renderHand();
    R.cam.target.set(0.5, 0.22, 3); R.cam.snapGoal();
    R.cam.dist = 5.2; R.cam.pitch = 0.95; R.cam.yaw = Math.PI / 4; R.cam.place(); R.cam.snapGoal();
  });
  await page.waitForTimeout(120);
  await shot(page, 'w-trace-preview-pulse', { x: 420, y: 100, width: 820, height: 640 });
  await page.waitForTimeout(900);
  await shot(page, 'w-trace-preview-only', { x: 420, y: 100, width: 820, height: 640 });

  /* ───────── 6. 草地近景 / 立牌接地 ───────── */
  await page.evaluate(() => {
    const { R } = window.__dbg;
    R.cam.target.set(-3, 0.3, 2); R.cam.snapGoal();
    R.cam.dist = 5.2; R.cam.pitch = 0.52; R.cam.yaw = Math.PI / 4; R.cam.place(); R.cam.snapGoal();
  });
  await shot(page, 'w-grass-near');
  await page.screenshot({ path: path.join(OUT, 'w-grass-crop.png'), clip: { x: 420, y: 200, width: 760, height: 560 } });
  await page.evaluate(() => {
    const { R } = window.__dbg;
    R.cam.target.set(0, 0.3, 1); R.cam.snapGoal();
    R.cam.dist = 3.4; R.cam.pitch = 0.30; R.cam.yaw = 0.4; R.cam.place(); R.cam.snapGoal();
  });
  await shot(page, 'w-standee-contact');
  await page.screenshot({ path: path.join(OUT, 'w-standee-contact-crop.png'), clip: { x: 560, y: 300, width: 560, height: 420 } });

  /* ───────── 7. 摆件模式 ───────── */
  await page.evaluate(() => {
    const { R } = window.__dbg;
    R.cam.yaw = Math.PI / 4; R.cam.pitch = 0.52; R.cam.dist = 8.6;
    R.cam.target.set(0, 0.22, 0); R.cam.place(); R.cam.snapGoal();
  });
  const frameState = () => page.evaluate(`(() => {
    const { S, R, UI } = window.__dbg; const THREE = window.THREE;
    ${SYNC} ${PROJ}
    sync();
    let x0 = 1e9, x1 = -1e9, y0 = 1e9, y1 = -1e9, sx = 0, sy = 0, n = 0, behind = 0;
    for (const c of S.W.cells.values()) {
      const h = R.hOf(c);
      for (const [dx, dz] of [[-0.5,-0.5],[0.5,-0.5],[0.5,0.5],[-0.5,0.5]]) {
        const p = proj(c.x+dx, h, c.y+dz);
        if (p.z > 1 || !isFinite(p.x) || !isFinite(p.y)) { behind++; continue; }
        x0 = Math.min(x0,p.x); x1 = Math.max(x1,p.x); y0 = Math.min(y0,p.y); y1 = Math.max(y1,p.y);
        sx += p.x; sy += p.y; n++;
      }
    }
    const els = {};
    for (const id of ['top','leftrail','rightpanel','handwrap','hand','hint']) {
      const el = document.getElementById(id); if (!el) continue;
      const b = el.getBoundingClientRect(); const cs = getComputedStyle(el);
      els[id] = { display: cs.display, op: +(+(cs.opacity || 1)).toFixed(2),
        box: [Math.round(b.x), Math.round(b.y), Math.round(b.width), Math.round(b.height)],
        onScreen: b.width > 0 && b.height > 0 && b.bottom > 0 && b.top < innerHeight };
    }
    const visibleTop = els.top && els.top.onScreen;
    const wrap = document.getElementById('handwrap').getBoundingClientRect();
    return { canvas: [Math.round(r.left), Math.round(r.top), Math.round(r.width), Math.round(r.height)],
      worldBBox: [Math.round(x0), Math.round(y0), Math.round(x1), Math.round(y1)], behind,
      worldCenterOffset: { dx: Math.round(sx/n - (r.left + r.width/2)), dy: Math.round(sy/n - (r.top + r.height/2)) },
      coverage: +(((Math.min(x1, r.right)-Math.max(x0, r.left)) * (Math.min(y1, r.bottom)-Math.max(y0, r.top))) / (r.width*r.height)).toFixed(3),
      uiMode: UI.state.uiMode,
      camera: { yaw: +R.cam.yaw.toFixed(3), pitch: +R.cam.pitch.toFixed(3), dist: R.cam.dist,
        target: [+R.cam.target.x.toFixed(2), +R.cam.target.z.toFixed(2)] },
      handPeekPx: Math.max(0, Math.round(innerHeight - wrap.top)),
      topBarPx: visibleTop ? Math.round(els.top.box[3]) : 0, els };
  })()`);
  M.ambient = { before: await frameState() };
  await shot(page, 'w-ambient-before');
  await page.evaluate(() => window.__dbg.UI.toggleAmbient());
  await page.waitForTimeout(700);
  M.ambient.after = await frameState();
  await shot(page, 'w-ambient-after');
  await page.evaluate(() => window.__dbg.UI.toggleAmbient());
  await page.waitForTimeout(300);

  /* ───────── 8. 长局：几何 / 结构 / 单帧 CPU 时间 ───────── */
  const stats = () => page.evaluate(() => {
    const { S, R } = window.__dbg;
    let standees = 0, cellMeshes = 0, meshes = 0, lines = 0;
    for (const l of R.standees.values()) standees += l.length;
    for (const c of S.W.cells.values()) if (c.mesh) cellMeshes++;
    R.scene.traverse(o => { if (o.isMesh) meshes++; if (o.isLineSegments) lines++; });
    const i = R.renderer.info;
    return { month: S.W.month, cells: S.W.cells.size, traces: S.W.traces.length,
      standees, cellMeshes, meshes, lines, geometries: i.memory.geometries,
      textures: i.memory.textures, drawCalls: i.render.calls, triangles: i.render.triangles };
  });
  const cpu = () => page.evaluate(() => {
    const { R } = window.__dbg;
    const t0 = performance.now();
    for (let i = 0; i < 20; i++) R.frame(t0 + i * 16, 0);
    return +((performance.now() - t0) / 20).toFixed(2);
  });
  M.longRun = { before: await stats(), cpuBefore: await cpu() };
  await shot(page, 'w-long-before');
  await page.evaluate(() => {
    const { S, UI } = window.__dbg;
    for (let i = 0; i < 48; i++) S.cmd.tick(1);
    UI.updateHud(); UI.renderRight(); UI.renderHand();
  });
  await page.waitForTimeout(900);
  M.longRun.after = await stats();
  M.longRun.cpuAfter = await cpu();
  await shot(page, 'w-long-after');

  /* ───────── 9. 相机穿模：垒高一格岩，低俯角近距 ───────── */
  M.penetration = await page.evaluate(`(() => {
    const { S, D, R } = window.__dbg;
    const hill = D.byId.get('terrain.earth.5.matter.hill');
    for (let i = 0; i < 24; i++) S.cmd.cast(hill, 0, 0);
    const c = S.at(0, 0);
    const top = R.hOf(c);
    const cases = [];
    for (const [pitch, dist] of [[0.16,3],[0.16,3.4],[0.26,3.4],[0.26,5.2],[0.52,3.4],[0.52,8.6],[0.95,8.6],[1.45,8.6]]) {
      R.cam.yaw = Math.PI/4; R.cam.pitch = pitch; R.cam.dist = dist;
      R.cam.target.set(0, 0.22, 0); R.cam.place(); R.cam.snapGoal();
      const cy = R.camera.position.y;
      cases.push({ pitchDeg: Math.round(pitch*180/Math.PI), dist, camY: +cy.toFixed(3),
        belowTerrain: cy < top, clearance: +(cy-top).toFixed(3) });
    }
    return { cellH: c.h, top: +top.toFixed(3), groundTopNoHill: +R.hOf(Object.assign({}, c, { h: 0.30 })).toFixed(3), cases };
  })()`);
  await page.evaluate(() => {
    const { R } = window.__dbg;
    R.cam.yaw = Math.PI / 4; R.cam.pitch = 0.26; R.cam.dist = 3.4;
    R.cam.target.set(0, 0.22, 0); R.cam.place(); R.cam.snapGoal();
  });
  await shot(page, 'w-cam-inside-hill');

  /* ───────── 10. 像素统计：明度分布 / 硬边 ───────── */
  const img = loadPNG(path.join(OUT, 'w-yaw-2.png'));
  M.pixels = {
    size: [img.w, img.h],
    whole: region(img, 0, 0, img.w, img.h),
    skyTop: region(img, 600, 5, 700, 110),
    landMid: region(img, 620, 380, 420, 220),
    histBands: (() => {
      const b = [0, 0, 0, 0, 0];
      for (let y = 0; y < img.h; y += 3) for (let x = 0; x < img.w; x += 3) {
        const L = luma(px(img, x, y));
        b[L < 30 ? 0 : L < 60 ? 1 : L < 100 ? 2 : L < 150 ? 3 : 4]++;
      }
      const n = b.reduce((s, v) => s + v, 0);
      return b.map(v => +(v / n).toFixed(3));
    })()
  };
  const wimg = loadPNG(path.join(OUT, 'w-water-edge-low.png'));
  const sc = scan(wimg, 'v', 840, 400, 760);
  M.waterPixel = { maxJump: sc.maxJump, at: sc.at, around: sc.jumps.slice(Math.max(0, sc.at - 6), sc.at + 6) };

  /* 痕迹环的像素可见度：同机位「有环 / 无环」差分 */
  const trail = loadPNG(path.join(OUT, 'w-trace-preview-only.png'));
  M.pixels.traceCrop = region(trail, 0, 0, trail.w, trail.h);

  fs.writeFileSync(path.join(OUT, 'metrics.json'), JSON.stringify(M, null, 1));
  console.log('--- failures', M.failures.length);
  if (M.failures.length) console.log(M.failures.slice(0, 5));
  await browser.close();
})();
