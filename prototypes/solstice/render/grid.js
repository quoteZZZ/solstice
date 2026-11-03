/* 至日 · 格与立牌
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

  /* 立牌贴图：只按「形状 + 元素色」缓存，同种同色共用一张 */
  function standeeTex(card) {
    const shape = (card.species && card.species.shape) || 'blob';
    const key = shape + ':' + card.primary;
    if (texCache.has(key)) return texCache.get(key);

    const SZ = 200, c = document.createElement('canvas');
    c.width = SZ; c.height = SZ;
    const g = c.getContext('2d');
    const col = shade(D.elColor(card.primary), -0.35);
    const dark = shade(col, -0.42), light = shade(col, 0.3);
    const cx = SZ / 2, base = SZ * 0.95;

    g.clearRect(0, 0, SZ, SZ);
    g.fillStyle = 'rgba(0,0,0,.4)';
    g.beginPath(); g.ellipse(cx, base - 5, SZ * 0.28, SZ * 0.05, 0, 0, 7); g.fill();

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
      g.strokeStyle = col; g.lineWidth = 9; g.lineCap = 'round';
      for (const [dx, dh] of [[-32, -64], [0, -86], [30, -58], [12, -72], [-14, -76]]) {
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

  function buildCell(cell) {
    if (cell.mesh) R.cellGroup.remove(cell.mesh);
    const kind = cell.kind, T = S.TERRAIN[kind] || S.TERRAIN.land;
    const h = Math.max(0.06, cell.h);

    const geo = new THREE.BoxGeometry(0.95, h, 0.95);
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
    const top = new THREE.Mesh(new THREE.PlaneGeometry(0.95, 0.95), topMat);
    top.rotation.x = -Math.PI / 2;
    top.position.y = h / 2 + 0.003;
    top.receiveShadow = true;
    m.add(top);

    const edges = new THREE.LineSegments(
      new THREE.EdgesGeometry(geo),
      new THREE.LineBasicMaterial({ color: 0x8fc0d8, transparent: true, opacity: 0.14 })
    );
    m.add(edges);

    cell.mesh = m;
    R.cellGroup.add(m);
  }

  function buildStandees(cell) {
    const key = S.K(cell.x, cell.y);
    const old = R.standees.get(key);
    if (old) for (const o of old) R.standeeGroup.remove(o);

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
      const tex = standeeTex(e.card);
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

  function sigOf(cell) {
    return cell.kind + '|' + cell.h.toFixed(2) + '|' +
      cell.ents.map(e => e.id + (e.ground ? 'g' : '')).join(',');
  }

  R.sync = function () {
    if (!R.grid) {
      R.grid = { cells: new Map(), standees: new Map(), sig: new Map() };
      R.standees = R.grid.standees;
    }
    let changed = false;

    for (const [key, cell] of W.cells) {
      cell.kind = S.classify(cell);
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
      if (cell.mesh) R.cellGroup.remove(cell.mesh);
      const st = R.grid.standees.get(key);
      if (st) for (const o of st) R.standeeGroup.remove(o);
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
