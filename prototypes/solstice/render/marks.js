/* Solstice · 落点预览的标记层
   拖一张卡在世界上走时，把「这张卡会动到哪些格」画出来：
   已有格用一层薄覆盖（第二层描边），虚空里将要长出来的地方用另一层（虚格）。
   判据来自 sim（SOL.sim.castRange），这里只负责画 —— 规则不写在渲染层。 */
(function (R, S, D) {
  const M = R.marks = {};
  let mesh = null, ghost = null, ring = null;
  const dummy = new THREE.Object3D();

  function make(geo, color, opacity) {
    const mat = new THREE.MeshBasicMaterial({
      color: color, transparent: true, opacity: opacity,
      depthWrite: false, side: THREE.DoubleSide
    });
    const m = new THREE.InstancedMesh(geo, mat, 512);
    m.count = 0;
    m.frustumCulled = false;
    m.renderOrder = 3;
    R.scene.add(m);
    return m;
  }

  M.init = function () {
    if (mesh) return;
    const g1 = new THREE.PlaneGeometry(0.98, 0.98);
    g1.rotateX(-Math.PI / 2);
    mesh = make(g1, 0xe0a33e, 0.17);

    const g2 = new THREE.PlaneGeometry(0.98, 0.98);
    g2.rotateX(-Math.PI / 2);
    ghost = make(g2, 0x8fd2e8, 0.12);
    ghost.renderOrder = 2;

    /* 键盘游标：鼠标有指针本身作指示，键盘没有，只靠 cam 那根 1px 线框
       在蓝图上几乎看不见。这里给游标脚下垫一层白，光标去哪一眼就知道。 */
    const g3 = new THREE.PlaneGeometry(1.02, 1.02);
    g3.rotateX(-Math.PI / 2);
    ring = make(g3, 0xf4f9ff, 0.2);
    ring.renderOrder = 4;
  };

  /* 覆盖层要贴在「看得见的那个面」上。sim 给的 h 是逻辑高度（草地/林地都是 0.30），
     地形一抬高，标记就会陷进方块里——所以有格的走 R.hOf，虚空格照旧用建好后的基准高。 */
  const hv = function (c) {
    const cell = S.at(c.x, c.y);
    if (cell) return R.hOf(cell);
    return c.fresh ? 0.30 : (c.h || 0.30);
  };

  /* 游标：cell = {x, y, h}；传 null 收起 */
  M.cursor = function (cell) {
    M.init();
    if (!cell) { ring.count = 0; ring.visible = false; return; }
    dummy.position.set(cell.x, hv(cell) + 0.016, cell.y);
    dummy.updateMatrix();
    ring.setMatrixAt(0, dummy.matrix);
    ring.count = 1;
    ring.instanceMatrix.needsUpdate = true;
    ring.visible = true;
  };

  /* list: [{x, y, h, fresh}] */
  M.set = function (list) {
    M.init();
    let n = 0, g = 0;
    for (const c of list || []) {
      const t = c.fresh ? ghost : mesh;
      const i = c.fresh ? g++ : n++;
      if (i >= 512) continue;
      dummy.position.set(c.x, hv(c) + 0.012, c.y);
      dummy.updateMatrix();
      t.setMatrixAt(i, dummy.matrix);
    }
    mesh.count = n;
    ghost.count = g;
    mesh.instanceMatrix.needsUpdate = true;
    ghost.instanceMatrix.needsUpdate = true;
    mesh.visible = n > 0;
    ghost.visible = g > 0;
  };

  M.count = function () {
    return { n: mesh ? mesh.count : -1, g: ghost ? ghost.count : -1 };
  };

  M.clear = function () {
    if (!mesh) return;
    mesh.count = 0;
    ghost.count = 0;
    mesh.visible = false;
    ghost.visible = false;
  };

  /* ───────── 落点扩散环 ─────────
     施一张卡之后，画面上唯一的变化是「那一格多了个东西」。落在草丛里、
     或者打在已经满的格子上时，连这个都看不出来——玩家会怀疑卡没生效。
     这里给每次施卡一个 0.6 秒的扩散环：纯表现，8 个池子循环用，不建新对象。 */
  const PULSE_N = 8, pulses = [];
  let pulseAt = 0, pulseSeq = 0;

  M.initPulses = function () {
    if (pulses.length) return;
    const geo = new THREE.RingGeometry(0.40, 0.50, 40);
    geo.rotateX(-Math.PI / 2);
    for (let i = 0; i < PULSE_N; i++) {
      const m = new THREE.Mesh(geo, new THREE.MeshBasicMaterial({
        color: 0xffffff, transparent: true, opacity: 0, depthWrite: false, side: THREE.DoubleSide
      }));
      m.visible = false;
      m.renderOrder = 5;
      m.userData.age = 9999;
      R.scene.add(m);
      pulses.push(m);
    }
  };

  M.pulse = function (x, y, color, h) {
    M.initPulses();
    const m = pulses[(pulseSeq++) % PULSE_N];
    m.position.set(x, (h == null ? 0.30 : h) + 0.02, y);
    m.material.color.set(color == null ? 0xffffff : color);
    m.userData.age = 0;
    m.visible = true;
  };

  M.frame = function (now) {
    if (!pulses.length) return;
    const dt = pulseAt ? Math.min(60, now - pulseAt) : 16;
    pulseAt = now;
    for (const m of pulses) {
      if (!m.visible) continue;
      m.userData.age += dt;
      /* 施卡成功之后画面上唯一的变化就是这个环。原来 0.5 的峰值、620ms，
         在满地都是痕迹环的地面上根本分不出来（UI 评审 H06：失败有 toast、成功什么都没有）。 */
      const k = Math.min(1, m.userData.age / 780);
      m.scale.setScalar(0.38 + k * 1.85);
      m.material.opacity = 0.92 * (1 - k) * (1 - k);
      if (k >= 1) m.visible = false;
    }
  };

  /* 施卡成功 → 落点闪一圈。颜色跟卡的元素走，所以「这张卡是什么」一眼能看出来。 */
  S.on('cast', function (ev) {
    if (!ev || ev.op === 'law' || ev.op === 'sun') return;   // 全局卡没有落点
    const cell = S.at(ev.x, ev.y);
    const h = cell ? R.hOf(cell) : 0.30;
    M.pulse(ev.x, ev.y, D.elColor(ev.card.primary), h);
  });

})(SOL.render, SOL.sim, SOL.data);
