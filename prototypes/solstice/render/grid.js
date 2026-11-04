/* Solstice · 格与立牌
   一格 = 一个方块（顶面亮一档）；活物 = 一张朝相机的立牌（Canvas 现画，同种合一张）。
   数据变了才重建：整数签名不匹配才动 mesh。 */
(function (R, S, D) {
  const W = S.W;
  R.grid = null;
  R.standees = new Map();
  R._pops = [];

  const texCache = new Map();

  /* 立牌的两个材质常量：
     - STANDEE_AT 必须低于接地阴影的 alpha（贴图里画的是 0.42），否则整圈阴影被裁掉——
       之前是 0.5，贴图画了阴影而画面上一点也看不见，根因就是它。
     - TINT 是受光材质的补偿：立牌从 MeshBasicMaterial 换成 Lambert 后要跟地面吃同一盏光，
       而环境光只有 0x334b66×1.85，白面会掉到原来的一半左右。 */
  const STANDEE_AT = 0.34;
  const TINT = new THREE.Color(0.78, 0.78, 0.78);

  /* 地形几何全场共用：整个世界的方块只有这一个 BoxGeometry。
     高度做成 1，实际高度交给 mesh.scale.y —— 否则 1300 个格子就要 1300 个 BoxGeometry
     （实测 1674 个 geometry：比格数还多，因为顶面又各建了一块平面）。
     共享对象打 __shared，R.dispose 会跳过它：删一格不能把全场的几何一起废掉。 */
  const GEO = {};
  function shared(g) { g.userData.shared = true; return g; }

  /* 顶点色烘明暗：顶面全亮、侧面按朝向分四档、底面压到最暗。
     这是「没有实时阴影也能读出立体」的唯一办法——
     场地只有一盏方向光加环境光，格与格等高，漫反射打不出任何差别，
     上一版做的连续起伏因此一点也看不出来（起伏要靠明暗才读得出）。
     光向与 scene.js 的 moon 同向（−9, 15, 7 在 XZ 上的投影）。 */
  const LX = -0.79, LZ = 0.61;
  function bakeShade(geo) {
    const nrm = geo.attributes.normal, n = nrm.count;
    const col = new Float32Array(n * 3);
    for (let i = 0; i < n; i++) {
      const ny = nrm.getY(i);
      let k;
      if (ny > 0.5) k = 1.00;              // 顶面：受光
      else if (ny < -0.5) k = 0.34;        // 底面：几乎看不见，但要暗
      else k = 0.50 + 0.40 * Math.max(0, nrm.getX(i) * LX + nrm.getZ(i) * LZ);
      col[i * 3] = col[i * 3 + 1] = col[i * 3 + 2] = k;
    }
    geo.setAttribute('color', new THREE.BufferAttribute(col, 3));
  }

  function ensureGeo() {
    if (GEO.box) return;
    GEO.box = shared(new THREE.BoxGeometry(0.998, 1, 0.998));
    bakeShade(GEO.box);
    /* 水面顶要分段：1 段只有 4 个角，位移等于整块倾斜，看不出波纹。
       旋转不烘进几何——波纹 shader 用的是几何里的 position.y，
       烘掉 rotateX 之后 y 恒为 0，第二个正弦项就变成一个常数。 */
    GEO.waterTop = shared(new THREE.PlaneGeometry(0.998, 0.998, 4, 4));
  }

  /* 纸纹：一张 256² 的近白噪点 + 几道纤维，全场地表共用。
     上一版地表是纯顶点色，一格之内一个像素的起伏都没有，近景看起来是塑料积木。
     贴上纸纹之后同样的色块立刻变成「纸糊的地」，这是纸片剧场定调里最便宜的一步：
     一张贴图、零额外 draw call（材质本来就每格一份），代价只是 4 万个像素的显存。
     均值必须留在接近白（0.93 左右）：它是**乘数**，压到 0.5 的灰等于把整幅画面砍掉四倍亮度。 */
  let GRAIN = null;
  function grainTex() {
    if (GRAIN) return GRAIN;
    const SZ = 256, c = document.createElement('canvas');
    c.width = c.height = SZ;
    const g = c.getContext('2d');
    const img = g.createImageData(SZ, SZ);
    const d = img.data;
    let seed = 20261104;
    const rnd = () => { seed = (seed * 1664525 + 1013904223) | 0; return ((seed >>> 8) & 0xffff) / 65535; };
    for (let y = 0; y < SZ; y++) {
      for (let x = 0; x < SZ; x++) {
        const i = (y * SZ + x) * 4;
        /* 三级噪声：细颗粒 + 两道很淡的纤维（横向拉长，像手工纸的帘纹） */
        const fine = (rnd() - 0.5) * 0.105;
        const fiber = Math.sin(y * 0.42 + Math.sin(x * 0.021) * 2.2) * 0.016;
        const v = 0.945 + fine + fiber;
        const b = Math.max(0, Math.min(255, Math.round(v * 255)));
        d[i] = b; d[i + 1] = b; d[i + 2] = b; d[i + 3] = 255;
      }
    }
    g.putImageData(img, 0, 0);
    GRAIN = new THREE.CanvasTexture(c);
    GRAIN.colorSpace = THREE.SRGBColorSpace;
    GRAIN.wrapS = GRAIN.wrapT = THREE.RepeatWrapping;
    GRAIN.anisotropy = Math.min(4, (R.renderer && R.renderer.capabilities.getMaxAnisotropy()) || 1);
    return GRAIN;
  }

  /* 水面波纹：时间做成**共享 uniform**，注入进水材质的顶点着色器。
     零 CPU 成本——不在 R.frame 里遍历水格，只在 GPU 上按顶点算。 */
  const WATER_U = { value: 0 };
  R.tickWater = function (now) { WATER_U.value = now * 0.001; };

  /* 波幅从 0.013+0.007 提到 0.024+0.011：远景下原来的振幅只有 1–2 px，
     整片海等于一块蓝纸。水面上沿同时抬到「振幅 + 1 mm」，波谷才不会沉进方块。 */
  const WAVE_Y = 0.036;
  function waveInject(mat) {
    mat.customProgramCacheKey = () => 'solstice-water';
    mat.onBeforeCompile = shader => {
      shader.uniforms.uWave = WATER_U;
      shader.vertexShader = ['uniform float uWave;', shader.vertexShader.replace(
        '#include <begin_vertex>',
        ['#include <begin_vertex>',
         'float w1 = sin(uWave * 1.7 + position.x * 4.2 + position.y * 3.1);',
         'float w2 = sin(uWave * 1.1 + position.y * 7.3 - position.x * 2.2);',
         'transformed.z += w1 * 0.024 + w2 * 0.011;'].join('\n')
      )].join('\n');
    };
  }

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

  /* 卡面调色板 → 立体物件的反照率。
     这两件事的取值域不同：palette 是画在**近黑卡面**上的墨色，为了压住底色，
     亮度天然落在 0.10–0.30；照抄到 3D 里再吃一盏暗环境光，草地当场变成一片黑刺。
     这里把亮度抬进 0.42–0.60、彩度压到 0.55 以下——色相保留（物种之间还认得出），
     明度归一（不至于某一种草在场上跳出来）。 */
  function lift(hex, lMin, lMax) {
    const n = parseInt(hex.slice(1), 16);
    const r = ((n >> 16) & 255) / 255, g = ((n >> 8) & 255) / 255, b = (n & 255) / 255;
    const mx = Math.max(r, g, b), mn = Math.min(r, g, b);
    let h = 0, s = 0;
    const l0 = (mx + mn) / 2;
    if (mx !== mn) {
      const d = mx - mn;
      s = l0 > 0.5 ? d / (2 - mx - mn) : d / (mx + mn);
      if (mx === r) h = (g - b) / d + (g < b ? 6 : 0);
      else if (mx === g) h = (b - r) / d + 2;
      else h = (r - g) / d + 4;
      h /= 6;
    }
    const l = Math.min(lMax || 0.58, Math.max(lMin || 0.44, l0));
    /* 彩度压在 0.42 以下：立牌吃的是偏蓝的半球光，绿色一饱和就和天光混成薄荷色，
       整丛草看上去是塑料花。纸片剧场的立牌该是「印上去的颜色」，不是发光体。 */
    s = Math.min(0.42, s);
    const q = l < 0.5 ? l * (1 + s) : l + s - l * s;
    const p = 2 * l - q;
    const hue = t => {
      t = (t + 1) % 1;
      if (t < 1 / 6) return p + (q - p) * 6 * t;
      if (t < 1 / 2) return q;
      if (t < 2 / 3) return p + (q - p) * (2 / 3 - t) * 6;
      return p;
    };
    const to = v => Math.round(Math.min(255, Math.max(0, v * 255)));
    return '#' + ((1 << 24) + (to(hue(h + 1 / 3)) << 16) + (to(hue(h)) << 8) + to(hue(h - 1 / 3))).toString(16).slice(1);
  }

  /* 立牌贴图：按「形状 + 元素色 + 变体」缓存，同种同色同变体共用一张。
     变体只有 3 个（旋转 ±7°、缩放 0.9–1.08）——一格一图会让 500 格出现 500 张 canvas，
     而 3 个变体就足以打散「同一张精灵铺满全场」的壁纸感（REVIEW-DEMO V-2）。

     贴图 200 → 400：最近档（dist 3.4）一格约合 150 屏幕像素，200 px 的源在 2 倍屏上
     已经被拉伸，近景一眼糊。400 只多 3 倍内存（3 变体 × 形状数 × 色数，量级仍是十几张）。 */
  const TEX_SZ = 400;
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

    const SZ = TEX_SZ, c = document.createElement('canvas');
    c.width = SZ; c.height = SZ;
    const g = c.getContext('2d');
    /* 主体色 = 卡面色抬到「物件反照率」的明度档（见 lift 的注释）。
       原来这里是 shade(-0.35)：木 #5e8f47 被压成 (5,54,0)，再进 Lambert 的暗环境光，
       立牌在画面上就是一团黑剪影——草看起来像黑色的刺。 */
    const col = lift(col0);
    const dark = shade(col, -0.20), light = shade(col, 0.12);
    const edge = shade(col, -0.42);        // 纸片描边：把它从地表的颜色里切出来
    const cx = SZ / 2, base = SZ * 0.95;
    const U = SZ / 200;                    // 下面所有尺寸都按 200 写，乘 U 换到实尺寸

    g.clearRect(0, 0, SZ, SZ);
    /* 变体：绕根部做一点旋转与缩放，保证「落地」不飘 */
    const rot = (v - 1) * 0.12, scv = [0.92, 1, 1.08][v] || 1;
    g.translate(cx, base); g.rotate(rot); g.scale(scv, scv); g.translate(-cx, -base);

    /* 接地阴影用径向渐变而不是实心椭圆：实心椭圆在近景是一块硬边黑饼。
       峰值仍是 0.42（STANDEE_AT = 0.34 才留得住它）。 */
    const shR = SZ * (shape === 'tree' ? 0.30 : 0.26);
    const grd = g.createRadialGradient(cx, base - 5 * U, 0, cx, base - 5 * U, shR);
    grd.addColorStop(0, 'rgba(0,0,0,.42)');
    grd.addColorStop(0.62, 'rgba(0,0,0,.26)');
    grd.addColorStop(1, 'rgba(0,0,0,0)');
    g.fillStyle = grd;
    g.beginPath(); g.ellipse(cx, base - 5 * U, shR, SZ * 0.052, 0, 0, 7); g.fill();

    g.lineJoin = 'round'; g.lineCap = 'round';
    g.strokeStyle = edge; g.lineWidth = 3.4 * U;
    const fillStroke = () => { g.fill(); g.stroke(); };

    if (shape === 'tree') {
      g.fillStyle = dark; g.fillRect(cx - 7 * U, base - 76 * U, 14 * U, 76 * U);
      g.fillStyle = col;
      g.beginPath(); g.moveTo(cx, 12 * U); g.lineTo(cx + 62 * U, 96 * U); g.lineTo(cx - 62 * U, 96 * U); g.closePath(); fillStroke();
      g.fillStyle = light;
      g.beginPath(); g.moveTo(cx, 12 * U); g.lineTo(cx + 62 * U, 96 * U); g.lineTo(cx, 96 * U); g.closePath(); g.fill();
      g.fillStyle = col;
      g.beginPath(); g.moveTo(cx, 58 * U); g.lineTo(cx + 48 * U, 124 * U); g.lineTo(cx - 48 * U, 124 * U); g.closePath(); fillStroke();
    } else if (shape === 'bush') {
      g.fillStyle = col;
      g.beginPath(); g.arc(cx, 116 * U, 40 * U, 0, 7); fillStroke();
      g.beginPath(); g.arc(cx - 36 * U, 128 * U, 29 * U, 0, 7); fillStroke();
      g.beginPath(); g.arc(cx + 36 * U, 128 * U, 29 * U, 0, 7); fillStroke();
      g.fillStyle = light;
      g.beginPath(); g.arc(cx - 10 * U, 104 * U, 21 * U, 0, 7); g.fill();
    } else if (shape === 'grass') {
      /* 三套草型：叶数、疏密、长短都不同。只做旋转/缩放是看不出差别的——
         对称的草转 7° 还是同一张草（这是第一版 V-2 没修掉的原因）。 */
      const BLADES = [
        [[-32, -64], [0, -86], [30, -58], [12, -72], [-14, -76]],
        [[-22, -78], [2, -96], [26, -70]],
        [[-40, -56], [-18, -74], [2, -64], [20, -84], [38, -56], [10, -92], [-8, -46]]
      ][v] || null;
      const bl = BLADES || [[-32, -64], [0, -86], [30, -58]];
      /* 草用两遍描边：先粗一遍暗色当边，再细一遍本色。
         但不能用 edge（−0.52）——草叶只有 9 px 宽，两侧各吃掉 5 px 就只剩一条暗芯，
         上一版整丛草在画面上是几根黑刺。暗色用 −0.26，且只比本色宽 2.5 px。 */
      for (const pass of [0, 1]) {
        g.strokeStyle = pass ? col : dark;
        g.lineWidth = (pass ? 9 : 11.5) * U;
        for (const [dx, dh] of bl) {
          g.beginPath();
          g.moveTo(cx + dx * U, base);
          g.quadraticCurveTo(cx + dx * 1.5 * U, base + dh * 0.55 * U, cx + dx * 2.1 * U, base + dh * U);
          g.stroke();
        }
      }
      g.strokeStyle = light; g.lineWidth = 6 * U;
      g.beginPath(); g.moveTo(cx + 3 * U, base);
      g.quadraticCurveTo(cx + 10 * U, base - 48 * U, cx + 18 * U, base - 78 * U); g.stroke();
    } else if (shape === 'animal') {
      g.fillStyle = col;
      g.beginPath(); g.ellipse(cx, base - 32 * U, 42 * U, 29 * U, 0, 0, 7); fillStroke();
      g.beginPath(); g.arc(cx, base - 76 * U, 30 * U, 0, 7); fillStroke();
      g.beginPath(); g.arc(cx - 24 * U, base - 98 * U, 12 * U, 0, 7); g.fill();
      g.beginPath(); g.arc(cx + 24 * U, base - 98 * U, 12 * U, 0, 7); g.fill();
      g.fillStyle = light;
      g.beginPath(); g.arc(cx - 10 * U, base - 82 * U, 5 * U, 0, 7); g.fill();
      g.beginPath(); g.arc(cx + 10 * U, base - 82 * U, 5 * U, 0, 7); g.fill();
    } else {
      g.fillStyle = col;
      g.beginPath(); g.ellipse(cx, base - 30 * U, 46 * U, 26 * U, 0, 0, 7); fillStroke();
    }

    const tex = new THREE.CanvasTexture(c);
    tex.colorSpace = THREE.SRGBColorSpace;
    /* 近景被拉大、斜视被压扁时，双线性会糊成一片；各向异性把斜面上的细节拉回来。
       上限取 4 而不是显卡最大值：这是 3 万张像素级别的贴图，再高只是白烧带宽。 */
    tex.anisotropy = Math.min(4, (R.renderer && R.renderer.capabilities.getMaxAnisotropy()) || 1);
    tex.needsUpdate = true;
    texCache.set(key, tex);
    return tex;
  }

  /* 表现高度 = 地形基准 + 「地盘上升」的增量 + 每格极微起伏。
     原来直接读 cell.h，而 cell.h 一律 0.30（只有抬地的卡会动），
     草地/林地/岩地全一样高——玩家看不出世界有高差。
     基准表只有一份（sim 的 S.TERRAIN），render 不再自己抄。

     起伏用**连续函数**，不用逐格随机——这两者的差别是整片地像不像一块地：
       · 逐格随机（hash）：相邻两格可以差 0.09，侧壁整条露出来，草地中间出现一级级台阶，
         整片地看起来是一堆浮在虚空上的饼干板；
       · 连续正弦：相邻两格最多差 0.03（一格 1.0 宽 = 3 %），看不出台阶，
         但整片地会缓慢起伏，低角度上能读出地形层次。
     波长刻意取 2 格左右（系数 0.5）——太快就变回逐格噪声，太慢就看不出起伏。 */
  R.jitOf = cell => (cell.kind === 'water' ? 0
    : Math.sin(cell.x * 0.52 + 1.7) * 0.016 + Math.cos(cell.y * 0.61 - 0.9) * 0.016);
  R.hOf = function (cell) {
    const t = S.TERRAIN[cell.kind] || S.TERRAIN.land;
    return Math.max(0.05, t.h + (cell.h - 0.30) + R.jitOf(cell));
  };

  /* 顶面亮度：顶点色已经把顶面放在 1.0，这里给的是「顶面相对底色的增益」。
     1.30 会把顶面推到高光档，但整片地也跟着发白——真正的亮部交给纸纹与天空，
     这里只留 1.10，让「顶面比侧面亮」这件事由顶点色的差距表达。 */
  const TOP_GAIN = 1.10;

  function buildCell(cell, isNew) {
    ensureGeo();
    if (cell.mesh) { R.cellGroup.remove(cell.mesh); R.dispose(cell.mesh); cell.mesh = null; }
    const kind = cell.kind, T = S.TERRAIN[kind] || S.TERRAIN.land;
    const h = R.hOf(cell);
    /* 有水源的格：地面提亮一档并偏水色，否则「水泉」在画面里不存在（它只是格里有件实体） */
    const wet = cell.ents.some(e => e.tags.indexOf('watersource') >= 0);

    /* 地表色 = 地形色 × 三级变化：
       · 大尺度光照（波长约 28 格）：月光横扫过整座岛的明暗斜坡。没有它，一块 7×7 的
         裸地是同一个色值，看起来是一张纸；
       · 低频斑（波长约 14 格）：地表干湿不均；
       · 每格微差 ±3 %：打散规则感。
       三级全部只改颜色、不产生几何，所以不会像高程那样在格与格之间立起台阶。 */
    const sheen = 1 + 0.17 * Math.sin(cell.x * 0.28 + cell.y * 0.23 + 0.9);
    const lowFreq = Math.sin(cell.x * 0.44 + 2.1) * Math.cos(cell.y * 0.51 - 1.3);
    const spot = sheen * (0.94 + lowFreq * 0.075) * (0.97 + hash3(cell.x, cell.y, 23) * 0.06);
    const BASE = new THREE.Color(T.col).multiplyScalar(spot * TOP_GAIN);
    if (wet) BASE.lerp(new THREE.Color(0x6fd0ea), 0.34).multiplyScalar(1.10);
    if (cell.flags.edge) BASE.lerp(new THREE.Color(0x0a1822), 0.55);

    /* 顶面合并进方块：原来「方块 + 子平面」是两个 mesh、两次 draw call，
       全世界 1300 格就是 2600 次提交（实测 917 次/帧）。顶面的亮度改由顶点色表达，
       一个格子只剩一个 mesh。Lambert 代替 Standard 的另一半原因在这里：
       这套光只有漫反射，Standard 的 PBR 分支是纯开销。 */
    const mat = new THREE.MeshLambertMaterial({
      color: BASE, vertexColors: true, map: kind === 'water' ? null : grainTex(),
      transparent: kind === 'water', opacity: kind === 'water' ? 0.86 : 1
    });
    const m = new THREE.Mesh(GEO.box, mat);
    /* 新长出来的格从 0.55 高弹起，180 ms 落定。
       施一张土地卡之后画面上唯一的变化是「多了块地」——如果它直接出现，
       眼睛要靠对比上一帧才知道发生了事；弹一下就是在说「这里刚生成」。
       只有新格弹：材质/实体的变化也会重建 mesh，那些不该再弹一次。 */
    m.scale.y = isNew ? h * 0.55 : h;
    m.position.set(cell.x, h / 2, cell.y);
    if (isNew) {
      R._pops.push({ m: m, h: h, t: 0 });
      if (R._pops.length > 600) R._pops.shift();
    }
    /* 方块不投影：1300 个方块进 shadow map 是 1300 次额外 draw call，
       而它们的顶面全在同一高度，阴影只落在自己的侧壁上——明暗已经由顶点色给了。
       立牌仍然投影（那道影子是它「站在地上」的唯一依据）。 */
    m.castShadow = false; m.receiveShadow = true;
    m.userData.cell = cell;

    if (kind === 'water') {
      const topMat = new THREE.MeshStandardMaterial({
        color: new THREE.Color(T.col).multiplyScalar(spot).lerp(new THREE.Color(0x7fd8f0), 0.18),
        roughness: 0.16, metalness: 0.34,
        transparent: true, opacity: 0.82
      });
      waveInject(topMat);
      const top = new THREE.Mesh(GEO.waterTop, topMat);
      top.rotation.x = -Math.PI / 2;
      /* 水面顶要比方块上沿高出「波纹振幅 + 1 毫米」：位移是双向的，
         只留 3 mm 的话波谷会沉进方块里，水面上出现一片片被切掉的暗块。
         坐标要除以 h：方块几何高度是 1、真实高度靠 scale.y，子节点的 local y
         会被父级 scale 再乘一次——写成 h/2 的话水面被压到方块里面去（h=0.08 时只剩 6 mm）。 */
      top.position.y = 0.5 + WAVE_Y / h;
      top.receiveShadow = true;
      m.add(top);
    }

    /* 界缘：只在「没有邻居」或「邻居是另一种地」的那一面画线。
       全场格线是棋盘语言；只画界缘，线就变成了海岸线（REVIEW-DEMO V-6，也是红线「界缘必须有交代」）。
       线材质全场共用一份：每格 new 一个材质等于每格一次 uniform 上传。
       亮度从 0.26 收到 0.16、颜色从冷白改成偏青：0.26 时高差边上有明显的一圈白镶边，
       草地被描成「贴上去的一块板」；要的是「这里断了一级」，不是「这里画了根线」。 */
    if (!R.edgeMat) {
      R.edgeMat = new THREE.LineBasicMaterial({ color: 0x8fb8c8, transparent: true, opacity: 0.16 });
      /* 打上共享标记，否则删掉一格时 R.dispose 会把它一起 dispose 掉，
         之后所有还在场上的界缘线一起消失（材质 dispose 不会自动重传）。 */
      R.edgeMat.userData.shared = true;
    }
    const sides = [[0, -1], [0, 1], [-1, 0], [1, 0]];
    /* 同样要归一化：0.5 是方块顶面，0.004/h 是那一毫米的抬升在局部坐标里的量 */
    const y = 0.5 + 0.004 / h, E = 0.499;
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
      const line = new THREE.LineSegments(lg, R.edgeMat);
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
        new THREE.MeshBasicMaterial({ map: tex, color: 0x0a0f14, alphaTest: STANDEE_AT, side: THREE.DoubleSide, transparent: true })
      );
      back.position.set(px, base, pz);
      back.rotation.order = 'YXZ';       // 先 billboard（Y）再后仰（X），见 faceCam
      back.userData.cell = cell;
      back.userData.ph = ph;
      R.standeeGroup.add(back);
      list.push(back);

      const gFront = new THREE.PlaneGeometry(w, h);
      gFront.translate(0, h / 2, 0);
      /* 立牌是**印好的纸片**，不参与光照。
         两个原因叠在一起：billboard 永远朝着相机，而主光是一个固定方向，
         转到背面时整张牌都在背光里；更要命的是环境光与天光都偏蓝，
         受光的绿叶会被染成薄荷色——试过 Lambert + emissive 修补，
         只要还留着漫反射那一项，蓝色就一定会渗进来。
         纸片剧场里立牌的颜色该由画它的笔决定，不由灯决定，所以直接不给光照。
         代价是背光面不会变暗：用 0.86 的 tint 把它整体压一档，离地感交给接地阴影。 */
      /* 每张牌再乘一个 ±11 % 的明度抖动（按格散列）：三个变体只解决「形状一样」，
         解决不了「同一个亮度铺满全场」——一片 90 % 都是草的界上，整片草地会读成壁纸。
         抖动挂在材质上而不是纹理上，所以不新增一张贴图。 */
      const jit = 0.89 + hash3(cell.y, cell.x, 53) * 0.22;
      const m = new THREE.Mesh(
        gFront,
        new THREE.MeshBasicMaterial({
          map: tex, color: TINT.clone().multiplyScalar(jit),
          alphaTest: STANDEE_AT, side: THREE.DoubleSide
        })
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

  /* ───────── 变更检测 ─────────
     旧版每帧给 1307 个格子各拼一个字符串（含 4 次 S.K 的 `x + ',' + y` 分配、
     toFixed、join），R.sync 实测 1.05 ms/帧 —— 纯字符串垃圾。
     现在拆成两段：
       · 自身签名 = 整数（kind / 高度 / 实体数 / 末位 uid），每帧都算，只有几次整数运算；
       · 邻格签名要 4 次 map 查找，只在「有格子自身签名变了」的那一帧重算。
     邻格 kind 会因生态判地（eco.js 直接改 c.kind）而变，所以不能只看结构脏标记，
     必须靠自身签名发现变化——发现之后整场重建一次邻格表，代价摊到罕见帧上。 */
  const KIND_N = { land: 1, water: 2, rock: 3, grass: 4, forest: 5 };
  const NB = [[0, -1], [0, 1], [-1, 0], [1, 0]];
  const nbCache = new Map();

  function ownSig(cell) {
    const ents = cell.ents;
    const last = ents.length ? ents[ents.length - 1].uid : 0;
    return ((KIND_N[cell.kind] || 0) * 31 + ((cell.h * 100) | 0)) * 8191 + ents.length * 4096 + (last % 4096);
  }

  function nbSigOf(cell) {
    let s = 0;
    for (let i = 0; i < 4; i++) {
      const n = W.cells.get(S.K(cell.x + NB[i][0], cell.y + NB[i][1]));
      s = s * 5 + (n ? (KIND_N[n.kind] || 1) : 0);
    }
    return s;
  }

  R.sync = function () {
    if (!R.grid) {
      R.grid = { cells: new Map(), standees: new Map(), sig: new Map() };
      R.standees = R.grid.standees;
    }
    let changed = false;

    /* 第一遍：只算自身签名。没有一格变化就整帧不动邻格表。 */
    let anyOwn = false;
    for (const [key, cell] of W.cells) {
      const o = ownSig(cell);
      if (o !== cell._own) { cell._own = o; anyOwn = true; }
    }
    if (anyOwn || nbCache.size === 0) {
      for (const [key, cell] of W.cells) nbCache.set(key, nbSigOf(cell));
    }

    /* 第二遍：重建需要动的格子 */
    for (const [key, cell] of W.cells) {
      const sig = cell._own * 7 + (nbCache.get(key) | 0);
      const prev = R.grid.sig.get(key);
      if (prev === sig && cell.mesh) continue;
      const isNew = !cell.mesh;
      buildCell(cell, isNew);
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
      nbCache.delete(key);
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
     俯视时它就变回一张「地贴」。（rotation.order = 'YXZ' 保证先 billboard 再后仰）

     摆动改成「主摆动 + 高频抖动」两层：单一正弦全场同频，800 个立牌会一起晃，
     看起来像整块布在抖。高频项的相位再乘 2.7，让相邻格的节奏错开。 */
  /* 新格的落定动画。用 R.frame 的 now 而不是各自记起点：同一帧生成的格子必须同步，
     否则一片 25 格一口气长出来时会呈波浪状依次弹，看起来像卡顿。 */
  R.tickPops = function (now) {
    const p = R._pops;
    if (!p.length) return;
    if (R._popT == null) R._popT = now;
    const dt = Math.min(80, now - R._popT);
    R._popT = now;
    for (let i = p.length - 1; i >= 0; i--) {
      const e = p[i];
      e.t += dt;
      const k = Math.min(1, e.t / 180);
      /* easeOutBack 的简化版：冲到 1.06 再回落，落地有「压一下」的手感 */
      const s = k < 1 ? 1 + (0.06 * Math.sin(k * Math.PI) - (1 - k) * (1 - k) * 0.45) : 1;
      e.m.scale.y = e.h * (0.55 + 0.45 * k) * s;
      if (k >= 1) { e.m.scale.y = e.h; p.splice(i, 1); }
    }
    if (!p.length) R._popT = null;
  };

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
      const ph = m.userData.ph;
      m.rotation.z = (Math.sin(t * 1.3 + ph) * 0.026 + Math.sin(t * 4.1 + ph * 2.7) * 0.006) * (1 - lean);
    }
  };

})(SOL.render, SOL.sim, SOL.data);
