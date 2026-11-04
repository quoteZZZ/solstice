/* Solstice · 落点预览的标记层
   拖一张卡在世界上走时，把「这张卡会动到哪些格」画出来：
   已有格用一层薄覆盖（第二层描边），虚空里将要长出来的地方用另一层（虚格）。
   判据来自 sim（SOL.sim.castRange），这里只负责画 —— 规则不写在渲染层。 */
(function (R, S) {
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

  /* 游标：cell = {x, y, h}；传 null 收起 */
  M.cursor = function (cell) {
    M.init();
    if (!cell) { ring.count = 0; ring.visible = false; return; }
    dummy.position.set(cell.x, (cell.h || 0.3) + 0.016, cell.y);
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
      dummy.position.set(c.x, (c.h || 0.3) + 0.012, c.y);
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

})(SOL.render, SOL.sim);
