/* Solstice · 3D 场景壳：渲染器、光、星野、虚空、蓝图、主循环
   render 只读 sim 的状态，不往 sim 里写世界（相机与悬停属于表现，放 render/cam.js）。 */
window.SOL = window.SOL || {};
SOL.render = SOL.render || {};
(function (R, S) {
  const W = S.W;

  R.canvas = null;
  R.renderer = null;
  R.scene = null;
  R.camera = null;
  R.cellGroup = null;
  R.standeeGroup = null;
  R.traceGroup = null;
  R.blueprint = null;
  R.sunOrb = null;
  R.sunGlow = null;
  R.moon = null;

  /* 天穹：一块反面球壳，按高度烘渐变顶点色，在地平线附近给一条亮带。
     之前 scene.background 是一个纯色 #0a1822，画面上半部分（俯仰 30° 时顶边已经
     在水平线以上）是一整片死蓝——亮度统计里「0.5 以上 0 个像素」有一半来自这里：
     画面里本来有天空，但那片天是纯黑的。球壳而不是背景贴图：
     背景贴图不随相机转，转镜头时天不动，一眼假。
     材质 fog:false —— 否则雾会把这个「最远的东西」先吃掉，天空反而没了。 */
  /* 亮带必须窄、也必须收着：第一版把地平线放在 #3f7288、一直铺到仰角 10 %，
     结果低视角下屏幕顶上是一条霓虹青，像游泳池的内壁。
     现在峰值降到 #2c5a6e，且 5 % 仰角就掉一半、18 % 就并入夜色——
     它是一道「光从海平面升起来」的迹象，不是一条装饰带。 */
  const SKY_STOPS = [
    /* 地平线以下必须等于雾色（#16303d）：虚空水面在半径 130 处被雾吃干，
       那边界要落到天上时如果颜色对不上，屏幕上会出现一条横贯的硬线
       （看起来像水槽边缘）。这一条不是配色问题，是接缝问题。 */
    [-1.00, 0x16303d],
    [-0.04, 0x16303d],
    [0.00, 0x1c3d4b],     // 贴着雾色：天际线两侧的值差太小才不会有硬边
    [0.09, 0x2c5a6e],     // 亮带抬到水平线之上一点，像被雾托起来的一道天光
    [0.34, 0x15323f],
    [1.00, 0x070f17]
  ];
  R.sky = null;

  function buildSky() {
    const R0 = 128;
    /* 高度分段必须是 64 而不是 20：渐变是按顶点插值的，20 段时每段 9°，
       而地平线亮带整个只有 4° 高 —— 它会被压进一圈顶点里，屏幕上是一条硬边。
       分段加密是这一层唯一能平滑的办法（顶点色没法在片元里插值）。 */
    const geo = new THREE.SphereGeometry(R0, 32, 64);
    const pos = geo.attributes.position, n = pos.count;
    const col = new Float32Array(n * 3);
    const stops = SKY_STOPS.map(([y, hex]) => [y, new THREE.Color(hex)]);
    const SUNC = new THREE.Color(0xffd9a0);
    const sd = new THREE.Vector3(-7.5, 10, 5.5).normalize();
    const v = new THREE.Vector3(), c = new THREE.Color();
    for (let i = 0; i < n; i++) {
      v.set(pos.getX(i), pos.getY(i), pos.getZ(i));
      const t = v.y / R0;
      let a = stops[0], b = stops[stops.length - 1];
      for (let k = 0; k < stops.length - 1; k++) {
        if (t >= stops[k][0] && t <= stops[k + 1][0]) { a = stops[k]; b = stops[k + 1]; break; }
      }
      const f = b[0] === a[0] ? 0 : (t - a[0]) / (b[0] - a[0]);
      c.copy(a[1]).lerp(b[1], Math.max(0, Math.min(1, f)));
      /* 太阳方向加一圈暖光：没有它，天空只有明度没有光源，仍然读不出方向 */
      v.normalize();
      const g = Math.pow(Math.max(0, v.dot(sd)), 14);
      c.r += SUNC.r * g * 0.9; c.g += SUNC.g * g * 0.9; c.b += SUNC.b * g * 0.9;
      col[i * 3] = c.r; col[i * 3 + 1] = c.g; col[i * 3 + 2] = c.b;
    }
    geo.setAttribute('color', new THREE.BufferAttribute(col, 3));
    const mesh = new THREE.Mesh(geo, new THREE.MeshBasicMaterial({
      vertexColors: true, side: THREE.BackSide, fog: false, depthWrite: false
    }));
    mesh.renderOrder = -1;
    mesh.frustumCulled = false;
    return mesh;
  }

  R.init = function () {
    const cv = R.canvas = document.getElementById('cv');
    const renderer = R.renderer = new THREE.WebGLRenderer({ canvas: cv, antialias: true });
    renderer.setPixelRatio(Math.min(devicePixelRatio || 1, 2));
    renderer.shadowMap.enabled = true;
    /* PCF 而非 PCFSoft：只给立牌投一次影，软分支的采样补偿换不来可见差别，白烧带宽 */
    renderer.shadowMap.type = THREE.PCFShadowMap;
    renderer.toneMapping = THREE.ACESFilmicToneMapping;
    /* 1.15 → 1.32：曝光是唯一能把「0.2–0.4 挤成一片」的直方图往右推的全局旋钮，
       配合地形底色提亮（sim/core.js 的地形表），画面才会出现 0.5 以上的亮部。 */
    renderer.toneMappingExposure = 1.78;

    const scene = R.scene = new THREE.Scene();
    scene.background = new THREE.Color('#0a1822');
    scene.fog = new THREE.FogExp2('#16303d', 0.05);

    R.camera = new THREE.PerspectiveCamera(36, 1, 0.1, 220);

    /* 三盏灯的分工要分开，否则整幅画面是同一个亮度（实测所有元素亮度接近、读不出主次）：
       环境光只托底（原来 1.85 是主光，把一切都泡成同一个蓝），
       半球光给上下色温差，方向光才是唯一的明暗来源。 */
    scene.add(new THREE.AmbientLight(0x2c4356, 0.80));
    scene.add(new THREE.HemisphereLight(0x93b6c9, 0x1a242e, 1.25));

    const moon = R.moon = new THREE.DirectionalLight(0xfff0d6, 3.2);
    moon.position.set(-9, 15, 7);
    moon.castShadow = true;
    moon.shadow.mapSize.set(1024, 1024);
    const sc = moon.shadow.camera;
    sc.left = -18; sc.right = 18; sc.top = 18; sc.bottom = -18; sc.far = 48;
    moon.shadow.bias = -0.0012;
    scene.add(moon);

    R.sky = buildSky();
    scene.add(R.sky);

    const orb = R.sunOrb = new THREE.Mesh(
      new THREE.SphereGeometry(0.85, 26, 18),
      new THREE.MeshBasicMaterial({ color: 0xffe8b0, transparent: true, opacity: 0 })
    );
    orb.position.set(-7.5, 10, 5.5);
    scene.add(orb);

    const glow = R.sunGlow = new THREE.PointLight(0xffd890, 0, 44);
    glow.position.copy(orb.position);
    scene.add(glow);

    /* 星野 */
    const g = new THREE.BufferGeometry(), p = [];
    for (let i = 0; i < 700; i++) {
      const r = 26 + Math.random() * 26, a = Math.random() * Math.PI * 2, y = 3 + Math.random() * 20;
      p.push(Math.cos(a) * r, y, Math.sin(a) * r);
    }
    g.setAttribute('position', new THREE.Float32BufferAttribute(p, 3));
    scene.add(new THREE.Points(g, new THREE.PointsMaterial({
      color: 0x9fc4d8, size: 0.15, transparent: true, opacity: 0.5
    })));

    /* 虚空水面 */
    const plane = new THREE.Mesh(
      new THREE.PlaneGeometry(260, 260, 1, 1),
      new THREE.MeshStandardMaterial({ color: 0x1e4a60, roughness: 0.22, metalness: 0.4, transparent: true, opacity: 0.9 })
    );
    plane.rotation.x = -Math.PI / 2;
    plane.position.y = -0.62;
    plane.receiveShadow = true;
    scene.add(plane);

    R.blueprint = new THREE.Group();
    scene.add(R.blueprint);
    R.cellGroup = new THREE.Group(); scene.add(R.cellGroup);
    R.standeeGroup = new THREE.Group(); scene.add(R.standeeGroup);
    R.traceGroup = new THREE.Group(); scene.add(R.traceGroup);

    window.addEventListener('resize', R.resize);
    R.resize();
    return R;
  };

  R.resize = function () {
    const cv = R.canvas;
    if (!cv) return;
    const r = cv.parentElement.getBoundingClientRect();
    R.renderer.setSize(r.width, r.height, false);
    R.camera.aspect = r.width / Math.max(1, r.height);
    R.camera.updateProjectionMatrix();
  };

  /* 释放一棵对象树的 geometry / material。
     贴图不在这里动——立牌贴图是 texCache 里共享的，释放了会让别的格子变白。
     共享几何（地形方块、水面顶、界缘线段）与共享材质也要跳过：
     删一格就把全场的方块几何 dispose 掉，剩下的格子会在下一帧集体变成黑块。
     没有这一层时，60 格的界跑 72 个月会把 geometry 从 2 涨到 1008（REVIEW-DEMO X-3）。 */
  R.dispose = function (obj) {
    if (!obj) return;
    obj.traverse(o => {
      if (o.geometry && !o.geometry.userData.shared) o.geometry.dispose();
      const m = o.material;
      if (!m) return;
      (Array.isArray(m) ? m : [m]).forEach(mat => {
        if (mat && !mat.userData.shared) mat.dispose();
      });
    });
  };

  /* 形制蓝图：虚空上的一圈虚线，告诉你这一界想长成什么形状（不参与结算） */
  R.setBlueprint = function (form) {
    const g = R.blueprint;
    while (g.children.length) { const c = g.children[0]; g.remove(c); R.dispose(c); }
    const isles = (form && form.isles) || [];
    for (const [cx, cy, rx, ry] of isles) {
      const curve = new THREE.EllipseCurve(cx, cy, rx, ry, 0, Math.PI * 2);
      const pts = curve.getPoints(72).map(p => new THREE.Vector3(p.x, -0.6, p.y));
      const line = new THREE.Line(
        new THREE.BufferGeometry().setFromPoints(pts),
        new THREE.LineDashedMaterial({ color: 0x7fb2c8, transparent: true, opacity: 0.22, dashSize: 0.35, gapSize: 0.3 })
      );
      line.computeLineDistances();
      g.add(line);
    }
  };

  /* 世界被清空时，渲染层要跟着清 */
  R.clear = function () {
    for (const grp of [R.cellGroup, R.standeeGroup, R.traceGroup]) {
      if (!grp) continue;
      while (grp.children.length) { const c = grp.children[0]; grp.remove(c); R.dispose(c); }
    }
    R.grid = null;
    R.traceCache = [];
  };

  /* 设置面板真正接线的三项 */
  R.pixelRatio = 1.5;
  R.shadows = true;
  R.fogLevel = '标准';
  const FOG_DENSITY = { 无: 0.012, 薄: 0.032, 标准: 0.05, 厚: 0.085 };

  R.setPixelRatio = function (v) {
    R.pixelRatio = v;
    R.renderer.setPixelRatio(Math.min(v, 2));
    R.resize();
  };
  R.setShadows = function (on) {
    R.shadows = !!on;
    R.renderer.shadowMap.enabled = R.shadows;
    R.moon.castShadow = R.shadows;
    R.renderer.shadowMap.needsUpdate = true;
  };
  R.setFog = function (level) {
    R.fogLevel = level;
    R.scene.fog.density = FOG_DENSITY[level] == null ? 0.05 : FOG_DENSITY[level];
  };

  /* 主循环不在这里——时钟归装配层（ui/boot.js），render 只是被叫来画一帧。
     旧版 R.loop 自己推进世界，等于「渲染器拥有时钟」：存档、回放、加速档
     全被锁在表现层手里。现在 render 只读状态：要画就画，不画也不影响世界推进。 */
  R.frame = function (now, ticks) {
    /* 太阳球与光照跟着 W.sun 走 */
    if (R.sunOrb) {
      R.sunOrb.material.opacity = Math.min(1, W.sun * 0.42);
      R.sunGlow.intensity = Math.min(2.8, W.sun * 0.9);
      R.moon.intensity = Math.min(1.8, 1.5 + W.sun * 0.1);
      R.sunOrb.scale.setScalar(1 + Math.sin(now * 0.0012) * 0.045);
    }

    R.sync();
    R.tickPops(now);
    R.tickWater(now);
    SOL.render.trace.sync();
    SOL.render.trace.fade();
    SOL.render.marks.frame(now);
    SOL.render.cam.frame(now);

    R.renderer.render(R.scene, R.camera);
  };

})(SOL.render, SOL.sim);
