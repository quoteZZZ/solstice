/* Solstice · 格与立牌
   一格 = 一个方块（顶面亮一档）；活物 = 一张朝相机的立牌（Canvas 现画，同种合一张）。
   数据变了才重建：signature 不匹配才动 mesh。 */
(function (R, S, D) {
  const W = S.W;
  R.grid = null;
  R.standees = new Map();

  const texCache = new Map();

  /* 立牌的两个材质常量：
     - STANDEE_AT 必须低于接地阴影的 alpha（贴图里画的是 0.42），否则整圈阴影被裁掉——
       之前是 0.5，贴图画了阴影而画面上一点也看不见，根因就是它。
     - TINT 是受光材质的补偿：立牌从 MeshBasicMaterial 换成 Lambert 后要跟地面吃同一盏光，
       而环境光只有 0x334b66×1.85，白面会掉到原来的一半左右。 */
  const STANDEE_AT = 0.34;
  const TINT = new THREE.Color(1.35, 1.35, 1.35);

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
    /* 立牌的颜色跟卡面的调色板走，不再一律用元素色。
       原来同元素的所有物种是同一个绿：一片林子里 5 种树长得一模一样，
       而卡面上它们本来就各有各的色（art.palette 是卡面画的真源）。 */
    const pal = card.art && card.art.palette;
    const col0 = (pal && pal[1]) || D.elColor(card.primary);
    const key = shape + ':' + col0 + ':' + v;
    if (texCache.has(key)) return texCache.get(key);

    const SZ = 200, c = document.createElement('canvas');
    c.width = SZ; c.height = SZ;
    const g = c.getContext('2d');
    /* 主体色就是卡面色，只压一点点。
       原来这里是 shade(-0.35)：木 #5e8f47 被压成 (5,54,0)，再进 Lambert 的暗环境光，
       立牌在画面上就是一团黑剪影——草看起来像黑色的刺。暗部/亮部各取 ±0.2 就够了。 */
    const col = shade(col0, -0.06);
    const dark = shade(col0, -0.26), light = shade(col0, 0.22);
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

  /* 表现高度 = 地形基准 + 「地盘上升」的增量。
     原来直接读 cell.h，而 cell.h 一律 0.30（只有抬地的卡会动），
     草地/林地/岩地全一样高——玩家看不出世界有高差。
     基准表只有一份（sim 的 S.TERRAIN），render 不再自己抄。 */
  R.hOf = function (cell) {
    const t = S.TERRAIN[cell.kind] || S.TERRAIN.land;
    return Math.max(0.06, t.h + (cell.h - 0.30));
  };

  function buildCell(cell) {
    if (cell.mesh) { R.cellGroup.remove(cell.mesh); R.dispose(cell.mesh); cell.mesh = null; }
    const kind = cell.kind, T = S.TERRAIN[kind] || S.TERRAIN.land;
    /* 高程抖动：每格 ±0.018，只影响表现，不写回 sim（写回去会毁确定性）。
       水面保持平——它是水面，不是地面。 */
    const jit = kind === 'water' ? 0 : (hash3(cell.x, cell.y, 7) - 0.5) * 0.036;
    const base = R.hOf(cell);
    const h = Math.max(0.06, base + jit);
    /* 有水源的格：地面提亮一档并偏水色，否则「水泉」在画面里不存在（它只是格里有件实体） */
    const wet = cell.ents.some(e => e.tags.indexOf('watersource') >= 0);

    const geo = new THREE.BoxGeometry(0.995, h, 0.995);
    /* 雾墙：大陆卡（type 130 / 9×9）最外圈压向背景色，边缘才不会是一条硬切边。
       背景色与 scene.background 同一个值，改那边记得同步。 */
    const BASE = new THREE.Color(T.col);
    if (cell.flags.edge) BASE.lerp(new THREE.Color(0x0a1822), 0.55);
    const mat = new THREE.MeshStandardMaterial({
      color: BASE, roughness: kind === 'water' ? 0.22 : 0.9,
      metalness: kind === 'water' ? 0.28 : 0.02,
      transparent: kind === 'water', opacity: kind === 'water' ? 0.8 : 1
    });
    const m = new THREE.Mesh(geo, mat);
    m.position.set(cell.x, h / 2, cell.y);
    m.castShadow = true; m.receiveShadow = true;
    m.userData.cell = cell;

    /* 水源格：顶面提亮 + 向水色偏一点。不这么做的话「水泉」只是格里有件实体，
       画面里看不到任何水，玩家不知道这块地为什么会长东西。 */
    const topCol = new THREE.Color(T.col).multiplyScalar(kind === 'water' ? 1.6 : 1.5);
    if (wet) topCol.lerp(new THREE.Color(0x4fb8d8), 0.34).multiplyScalar(1.12);
    if (cell.flags.edge) topCol.lerp(new THREE.Color(0x0a1822), 0.55);
    const topMat = new THREE.MeshStandardMaterial({
      color: topCol,
      roughness: kind === 'water' || wet ? 0.18 : 0.9,
      metalness: kind === 'water' || wet ? 0.3 : 0.02,
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

    const h0 = R.hOf(cell);
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
      const px = cell.x + off, pz = cell.y + 0.08 + (j1 - 0.5) * 0.13;
      const base = h0 - 0.02;
      /* 摆动相位按格坐标散列：正反两面共用同一个相位，否则背板会和正面错开、露出黑边 */
      const ph = hash3(cell.x, cell.y, 11) * 6.2832;

      /* 几何体先把原点挪到「脚底」，网格再摆到地面高度上。
         这样俯视档让立牌后仰时，它是绕着自己的根倒下去的，而不是绕中心转——
         绕中心转的版本在 83° 下会整片平移、并且把背板翻到上面来（一片黑）。 */
      const gBack = new THREE.PlaneGeometry(w * 1.075, h * 1.045);
      gBack.translate(0, (h * 1.045) / 2, -0.006);
      const back = new THREE.Mesh(
        gBack,
        new THREE.MeshLambertMaterial({ map: tex, color: 0x04080c, alphaTest: STANDEE_AT, side: THREE.DoubleSide, transparent: true })
      );
      back.position.set(px, base, pz);
      back.rotation.order = 'YXZ';       // 先 billboard（Y）再后仰（X），见 faceCam
      back.userData.cell = cell;
      back.userData.ph = ph;
      R.standeeGroup.add(back);
      list.push(back);

      const gFront = new THREE.PlaneGeometry(w, h);
      gFront.translate(0, h / 2, 0);
      const m = new THREE.Mesh(
        gFront,
        new THREE.MeshLambertMaterial({ map: tex, color: TINT, alphaTest: STANDEE_AT, side: THREE.DoubleSide })
      );
      m.position.set(px, base, pz);
      m.rotation.order = 'YXZ';
      m.userData.cell = cell;
      m.userData.ph = ph;
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

  /* 立牌朝相机 + 轻微摆动。
     摆动相位存在 userData.ph（按格散列），正反两面才会同步；原来是按遍历序号，
     背板和正面各拿一个相位，摆动时黑边就从侧面漏出来。
     相机没动就不重算 atan2 —— 1200 个立牌、每帧 2400 次三角函数，白算。

     俯视档还要往后仰：立牌只绕 Y 转的话，83° 俯角下整片林子会塌成一条条竖线，
     图上什么都读不出来。仰角按俯仰档插值到 0–83°，最高档几乎平铺在地面上，
     俯视时它就变回一张「地贴」。（rotation.order = 'YXZ' 保证先 billboard 再后仰） */
  R.faceCam = function (now) {
    const cam = R.camera;
    if (!cam) return;
    const t = now * 0.001;
    const cx = cam.position.x, cz = cam.position.z;
    const moved = R._camX !== cx || R._camZ !== cz;
    R._camX = cx; R._camZ = cz;
    const lean = Math.min(1, Math.max(0, ((R.cam.pitch || 0.52) - 0.26) / 1.19));
    const tilt = lean * lean * 1.45;
    for (const list of R.standees.values()) for (const m of list) {
      if (moved) m.rotation.y = Math.atan2(cx - m.position.x, cz - m.position.z);
      /* 负号是必须的：+X 旋转会把 +Y 轴转向 +Z（也就是倒向相机），
         超过 90° 之后正面朝下，俯视看到的是背面（Lambert 反面法线朝下 = 一片黑）。 */
      m.rotation.x = -tilt;
      m.rotation.z = Math.sin(t * 1.3 + m.userData.ph) * 0.022 * (1 - lean);
    }
  };

})(SOL.render, SOL.sim, SOL.data);
