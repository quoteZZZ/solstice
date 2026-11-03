/* Solstice · 格与立牌
   一格 = 一个方块（顶面亮一档）；活物 = 一张朝相机的立牌（Canvas 现画，同种合一张）。
   数据变了才重建：signature 不匹配才动 mesh。 */
(function (R, S, D) {
  const W = S.W;
  R.grid = null;
  R.standees = new Map();

  const texCache = new Map();

  function shade(hex, amt) {
    const n = parseInt(hex.slice(1), 16);
    let r = (n >> 16) & 255, g = (n >> 8) & 255, b = n & 255;
    r = Math.min(255, Math.max(0, Math.round(r + 255 * amt)));
    g = Math.min(255, Math.max(0, Math.round(g + 255 * amt)));
    b = Math.min(255, Math.max(0, Math.round(b + 255 * amt)));
    return '#' + ((1 << 24) + (r << 16) + (g << 8) + b).toString(16).slice(1);
  }
  function hash3(a, b, c) {
    let h = (a * 374761393 + b * 668265263 + c * 1274126177) | 0;
    h = (h ^ (h >>> 13)) * 1274126177 | 0;
    return ((h ^ (h >>> 16)) >>> 0) / 4294967296;
  }

  /* 立牌贴图：按「形状 + 元素色 + 变体」缓存，同种同色同变体共用一张。
     变体只有 3 个（旋转 ±7°、缩放 0.9–1.08）——一格一图会让 500 格出现 500 张 200×200 canvas，
     而 3 个变体就足以打散「同一张精灵铺满全场」的壁纸感（REVIEW-DEMO V-2）。 */
  function standeeTex(card, variant) {
    const shape = (card.species && card.species.shape) || 'blob';
    const v = variant | 0;
    const key = shape + ':' + card.primary + ':' + v;
    if (texCache.has(key)) return texCache.get(key);

    const SZ = 200, c = document.createElement('canvas');
    c.width = SZ; c.height = SZ;
    const g = c.getContext('2d');
    const col = shade(D.elColor(card.primary), -0.35);
    const dark = shade(col, -0.42), light = shade(col, 0.3);
    const cx = SZ / 2, base = SZ * 0.95;

    g.clearRect(0, 0, SZ, SZ);
    /* 变体：绕根部做一点旋转与缩放，保证「落地」不飘 */
    const rot = (v - 1) * 0.12, scv = [0.92, 1, 1.08][v] || 1;
    g.translate(cx, base); g.rotate(rot); g.scale(scv, scv); g.translate(-cx, -base);

    g.fillStyle = 'rgba(0,0,0,.42)';
    g.beginPath(); g.ellipse(cx, base - 5, SZ * (shape === 'tree' ? 0.3 : 0.26), SZ * 0.052, 0, 0, 7); g.fill();

    if (shape === 'tree') {
      g.fillStyle = dark; g.fillRect(cx - 7, base - 76, 14, 76);
      g.fillStyle = col;
      g.beginPath(); g.moveTo(cx, 12); g.lineTo(cx + 62, 96); g.lineTo(cx - 62, 96); g.closePath(); g.fill();
      g.fillStyle = light;
      g.beginPath(); g.moveTo(cx, 12); g.lineTo(cx + 62, 96); g.lineTo(cx, 96); g.closePath(); g.fill();
      g.fillStyle = col;
      g.beginPath(); g.moveTo(cx, 58); g.lineTo(cx + 48, 124); g.lineTo(cx - 48, 124); g.closePath(); g.fill();
    } else if (shape === 'bush') {
      g.fillStyle = col;
      g.beginPath(); g.arc(cx, 116, 40, 0, 7); g.fill();
      g.beginPath(); g.arc(cx - 36, 128, 29, 0, 7); g.fill();
      g.beginPath(); g.arc(cx + 36, 128, 29, 0, 7); g.fill();
      g.fillStyle = light;
      g.beginPath(); g.arc(cx - 10, 104, 21, 0, 7); g.fill();
    } else if (shape === 'grass') {
      /* 三套草型：叶数、疏密、长短都不同。只做旋转/缩放是看不出差别的——
         对称的草转 7° 还是同一张草（这是第一版 V-2 没修掉的原因）。 */
      const BLADES = [
        [[-32, -64], [0, -86], [30, -58], [12, -72], [-14, -76]],
        [[-22, -78], [2, -96], [26, -70]],
        [[-40, -56], [-18, -74], [2, -64], [20, -84], [38, -56], [10, -92], [-8, -46]]
      ][v] || null;
      const bl = BLADES || [[-32, -64], [0, -86], [30, -58]];
      g.strokeStyle = col; g.lineWidth = v === 2 ? 7 : 9; g.lineCap = 'round';
      for (const [dx, dh] of bl) {
        g.beginPath();
        g.moveTo(cx + dx, base);
        g.quadraticCurveTo(cx + dx * 1.5, base + dh * 0.55, cx + dx * 2.1, base + dh);
        g.stroke();
      }
      g.strokeStyle = light; g.lineWidth = 6;
      g.beginPath(); g.moveTo(cx + 3, base);
      g.quadraticCurveTo(cx + 10, base - 48, cx + 18, base - 78); g.stroke();
    } else if (shape === 'animal') {
      g.fillStyle = col;
      g.beginPath(); g.ellipse(cx, base - 32, 42, 29, 0, 0, 7); g.fill();
      g.beginPath(); g.arc(cx, base - 76, 30, 0, 7); g.fill();
      g.beginPath(); g.arc(cx - 24, base - 98, 12, 0, 7); g.fill();
      g.beginPath(); g.arc(cx + 24, base - 98, 12, 0, 7); g.fill();
      g.fillStyle = light;
      g.beginPath(); g.arc(cx - 10, base - 82, 5, 0, 7); g.fill();
      g.beginPath(); g.arc(cx + 10, base - 82, 5, 0, 7); g.fill();
    } else {
      g.fillStyle = col;
      g.beginPath(); g.ellipse(cx, base - 30, 46, 26, 0, 0, 7); g.fill();
    }

    const tex = new THREE.CanvasTexture(c);
    tex.colorSpace = THREE.SRGBColorSpace;
    tex.needsUpdate = true;
    texCache.set(key, tex);
    return tex;
  }

  const KIND_H = { land: 0.30, water: 0.11, rock: 0.44, forest: 0.30, grass: 0.30 };

  function buildCell(cell) {
    if (cell.mesh) { R.cellGroup.remove(cell.mesh); R.dispose(cell.mesh); cell.mesh = null; }
    const kind = cell.kind, T = S.TERRAIN[kind] || S.TERRAIN.land;
    /* 高程抖动：每格 ±0.018，只影响表现，不写回 sim（写回去会毁确定性）。
       水面保持平——它是水面，不是地面。 */
    const jit = kind === 'water' ? 0 : (hash3(cell.x, cell.y, 7) - 0.5) * 0.036;
    const h = Math.max(0.06, cell.h + jit);

    const geo = new THREE.BoxGeometry(0.995, h, 0.995);
    const mat = new THREE.MeshStandardMaterial({
      color: T.col, roughness: kind === 'water' ? 0.22 : 0.9,
      metalness: kind === 'water' ? 0.28 : 0.02,
      transparent: kind === 'water', opacity: kind === 'water' ? 0.8 : 1
    });
    const m = new THREE.Mesh(geo, mat);
    m.position.set(cell.x, h / 2, cell.y);
    m.castShadow = true; m.receiveShadow = true;
    m.userData.cell = cell;

    const topMat = new THREE.MeshStandardMaterial({
      color: new THREE.Color(T.col).multiplyScalar(kind === 'water' ? 1.6 : 1.5),
      roughness: kind === 'water' ? 0.18 : 0.9,
      metalness: kind === 'water' ? 0.3 : 0.02,
      transparent: kind === 'water', opacity: kind === 'water' ? 0.85 : 1
    });
    const top = new THREE.Mesh(new THREE.PlaneGeometry(0.995, 0.995), topMat);
    top.rotation.x = -Math.PI / 2;
    top.position.y = h / 2 + 0.003;
    top.receiveShadow = true;
    m.add(top);

    /* 界缘：只在「没有邻居」或「邻居是另一种地」的那一面画线。
       全场格线是棋盘语言；只画界缘，线就变成了海岸线（REVIEW-DEMO V-6，也是红线「界缘必须有交代」）。 */
    const sides = [[0, -1], [0, 1], [-1, 0], [1, 0]];
    const y = h / 2 + 0.004, E = 0.4975;
    const pts = [];
    for (const [dx, dz] of sides) {
      const n = W.cells.get(S.K(cell.x + dx, cell.y + dz));
      if (n && n.kind === kind) continue;
      if (dx === 0) pts.push(-E, y, dz * E, E, y, dz * E);
      else pts.push(dx * E, y, -E, dx * E, y, E);
    }
    if (pts.length) {
      const lg = new THREE.BufferGeometry();
      lg.setAttribute('position', new THREE.Float32BufferAttribute(pts, 3));
      const line = new THREE.LineSegments(lg, new THREE.LineBasicMaterial({
        color: 0x9fd0e2, transparent: true, opacity: 0.26
      }));
      m.add(line);
    }

    cell.mesh = m;
    R.cellGroup.add(m);
  }

  function buildStandees(cell) {
    const key = S.K(cell.x, cell.y);
    const old = R.standees.get(key);
    if (old) for (const o of old) { R.standeeGroup.remove(o); R.dispose(o); }

    const list = [];
    const seen = new Set();
    const ents = cell.ents.filter(e => {
      if (e.ground) return false;
      if (seen.has(e.id)) return false;      // 同种只立一个牌
      seen.add(e.id);
      return true;
    }).slice(0, 4);

    const h0 = Math.max(0.06, cell.h);
    ents.forEach((e, i) => {
      const v = Math.floor(hash3(cell.x, cell.y, e.name.length + i * 31) * 3);
      const tex = standeeTex(e.card, Math.min(2, v));
      const isTree = e.shape === 'tree';
      const j1 = hash3(cell.x, cell.y, e.name.length);
      const j2 = hash3(cell.y, cell.x, e.name.length * 7);
      const sc = 0.84 + j1 * 0.34;
      const w = (isTree ? 0.9 : 0.66) * sc;
      const h = (isTree ? 1.2 : 0.74) * sc;
      const off = (i - (ents.length - 1) / 2) * 0.19 + (j2 - 0.5) * 0.11;
      const px = cell.x + off, py = h0 + h / 2 - 0.02, pz = cell.y + 0.08 + (j1 - 0.5) * 0.13;

      const back = new THREE.Mesh(
        new THREE.PlaneGeometry(w * 1.075, h * 1.045),
        new THREE.MeshBasicMaterial({ map: tex, color: 0x04080c, alphaTest: 0.5, side: THREE.DoubleSide, transparent: true })
      );
      back.position.set(px, py, pz - 0.006);
      back.userData.cell = cell;
      R.standeeGroup.add(back);
      list.push(back);

      const m = new THREE.Mesh(
        new THREE.PlaneGeometry(w, h),
        new THREE.MeshBasicMaterial({ map: tex, alphaTest: 0.5, side: THREE.DoubleSide })
      );
      m.position.set(px, py, pz);
      m.userData.cell = cell;
      R.standeeGroup.add(m);
      list.push(m);
    });
    R.standees.set(key, list);
  }

  /* 签名要含邻格地形：界缘线是"邻居不同才画"，邻居一变就得重画 */
  function nbSig(cell) {
    let s = '';
    for (const [dx, dz] of [[0, -1], [0, 1], [-1, 0], [1, 0]]) {
      const n = W.cells.get(S.K(cell.x + dx, cell.y + dz));
      s += n ? n.kind[0] : '-';
    }
    return s;
  }

  function sigOf(cell) {
    return cell.kind + '|' + cell.h.toFixed(2) + '|' + nbSig(cell) + '|' +
      cell.ents.map(e => e.id + (e.ground ? 'g' : '')).join(',');
  }

  R.sync = function () {
    if (!R.grid) {
      R.grid = { cells: new Map(), standees: new Map(), sig: new Map() };
      R.standees = R.grid.standees;
    }
    let changed = false;

    for (const [key, cell] of W.cells) {
      const sig = sigOf(cell);
      const prev = R.grid.sig.get(key);
      if (prev === sig && cell.mesh) continue;
      buildCell(cell);
      buildStandees(cell);
      R.grid.sig.set(key, sig);
      R.grid.cells.set(key, cell);
      changed = true;
    }
    for (const [key, cell] of R.grid.cells) {
      if (W.cells.has(key)) continue;
      if (cell.mesh) { R.cellGroup.remove(cell.mesh); R.dispose(cell.mesh); cell.mesh = null; }
      const st = R.grid.standees.get(key);
      if (st) for (const o of st) { R.standeeGroup.remove(o); R.dispose(o); }
      R.grid.cells.delete(key);
      R.grid.standees.delete(key);
      R.grid.sig.delete(key);
      changed = true;
    }
    R.changed = changed;
    S.clearCellDirty();
    return changed;
  };

  /* 立牌朝相机 + 轻微摆动 */
  R.faceCam = function (now) {
    const cam = R.camera;
    if (!cam) return;
    const t = now * 0.001;
    let i = 0;
    for (const list of R.standees.values()) for (const m of list) {
      m.rotation.y = Math.atan2(cam.position.x - m.position.x, cam.position.z - m.position.z);
      m.rotation.z = Math.sin(t * 1.3 + i * 0.8) * 0.022;
      i++;
    }
  };

})(SOL.render, SOL.sim, SOL.data);
