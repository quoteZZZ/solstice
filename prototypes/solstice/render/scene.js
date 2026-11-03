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

  R.init = function () {
    const cv = R.canvas = document.getElementById('cv');
    const renderer = R.renderer = new THREE.WebGLRenderer({ canvas: cv, antialias: true });
    renderer.setPixelRatio(Math.min(devicePixelRatio || 1, 2));
    renderer.shadowMap.enabled = true;
    renderer.shadowMap.type = THREE.PCFSoftShadowMap;
    renderer.toneMapping = THREE.ACESFilmicToneMapping;
    renderer.toneMappingExposure = 1.15;

    const scene = R.scene = new THREE.Scene();
    scene.background = new THREE.Color('#0a1822');
    scene.fog = new THREE.FogExp2('#0a1822', 0.05);

    R.camera = new THREE.PerspectiveCamera(36, 1, 0.1, 220);

    scene.add(new THREE.AmbientLight(0x334b66, 1.85));
    scene.add(new THREE.HemisphereLight(0x587f9c, 0x0d151a, 1.05));

    const moon = R.moon = new THREE.DirectionalLight(0xeaf4ff, 1.5);
    moon.position.set(-9, 15, 7);
    moon.castShadow = true;
    moon.shadow.mapSize.set(1024, 1024);
    const sc = moon.shadow.camera;
    sc.left = -18; sc.right = 18; sc.top = 18; sc.bottom = -18; sc.far = 48;
    moon.shadow.bias = -0.0012;
    scene.add(moon);

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
      new THREE.PlaneGeometry(140, 140, 1, 1),
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
     没有这一层时，60 格的界跑 72 个月会把 geometry 从 2 涨到 1008（REVIEW-DEMO X-3）。 */
  R.dispose = function (obj) {
    if (!obj) return;
    obj.traverse(o => {
      if (o.geometry) o.geometry.dispose();
      const m = o.material;
      if (!m) return;
      (Array.isArray(m) ? m : [m]).forEach(mat => mat.dispose());
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
    SOL.render.trace.sync();
    SOL.render.trace.fade();
    SOL.render.cam.frame(now);

    R.renderer.render(R.scene, R.camera);
  };

})(SOL.render, SOL.sim);
