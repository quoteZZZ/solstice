/* 至日 · 相机与拾取
   连续 yaw / pitch / dist，另有四档俯仰与五档缩放（F1–F4 / Z）。
   世界坐标 → 屏幕坐标只在这里算，UI 不要自己碰投影。 */
(function (R, S) {
  const C = R.cam = {};
  const W = S.W;

  C.dist = 9;
  C.yaw = Math.PI * 0.25;
  C.pitch = 0.52;
  C.target = null;
  C.hover = null;

  const PITCHES = [0.26, 0.52, 0.95, 1.45];
  const ZOOMS = [24, 14, 8.6, 5.2, 3.4];

  C.init = function () {
    const THREE = window.THREE;
    C.target = C.target || new THREE.Vector3(0, 0.22, 0);

    C.hoverBox = new THREE.LineSegments(
      new THREE.EdgesGeometry(new THREE.BoxGeometry(1.0, 0.02, 1.0)),
      new THREE.LineBasicMaterial({ color: 0x8fd2e8, transparent: true, opacity: 0 })
    );
    C.hoverBox.visible = false;
    R.scene.add(C.hoverBox);

    C.ray = new THREE.Raycaster();
    C.ptr = new THREE.Vector2();
    C.plane = new THREE.Plane(new THREE.Vector3(0, 1, 0), 0);
    C.hit = new THREE.Vector3();

    C.place();
  };

  C.place = function () {
    const cam = R.camera, t = C.target;
    const y = Math.sin(C.pitch) * C.dist, r = Math.cos(C.pitch) * C.dist;
    cam.position.set(t.x + Math.cos(C.yaw) * r, t.y + y, t.z + Math.sin(C.yaw) * r);
    cam.lookAt(t);
  };

  C.recenter = function () {
    if (!W.cells.size) return;
    let sx = 0, sy = 0;
    for (const c of W.cells.values()) { sx += c.x; sy += c.y; }
    C.target.set(sx / W.cells.size, 0.22, sy / W.cells.size);
    C.place();
  };

  C.setPitch = function (i) {
    C.pitch = PITCHES[Math.max(0, Math.min(3, i))];
    C.pitchIdx = Math.max(0, Math.min(3, i));
    C.place();
  };

  C.setZoom = function (i) {
    C.zoomIdx = Math.max(0, Math.min(4, i));
    C.dist = ZOOMS[C.zoomIdx];
    C.place();
  };

  C.nudgeZoom = function (d) {
    C.setZoom((C.zoomIdx == null ? 2 : C.zoomIdx) + d);
  };

  /* 世界 → 屏幕（DOM 像素），涟漪 / 飘字用 */
  C.screenOf = function (x, y, h) {
    const v = new THREE.Vector3(x, h == null ? 0.3 : h, y).project(R.camera);
    const r = R.canvas.getBoundingClientRect();
    return { x: (v.x * 0.5 + 0.5) * r.width + r.left, y: (-v.y * 0.5 + 0.5) * r.height + r.top };
  };

  function setPtr(ev) {
    const r = R.canvas.getBoundingClientRect();
    C.ptr.x = ((ev.clientX - r.left) / r.width) * 2 - 1;
    C.ptr.y = -((ev.clientY - r.top) / r.height) * 2 + 1;
    C.ray.setFromCamera(C.ptr, R.camera);
  }

  /* 点到哪一格（含子 mesh） */
  C.pickAt = function (clientX, clientY) {
    setPtr({ clientX: clientX, clientY: clientY });
    const hits = C.ray.intersectObjects(R.cellGroup.children, true);
    for (const h of hits) {
      let o = h.object;
      while (o && !o.userData.cell) o = o.parent;
      if (o && o.userData.cell) return o.userData.cell;
    }
    return null;
  };

  /* 虚空平面上的落点（土地类卡用） */
  C.planeAt = function (clientX, clientY) {
    setPtr({ clientX: clientX, clientY: clientY });
    const p = new THREE.Vector3();
    if (!C.ray.ray.intersectPlane(C.plane, p)) return null;
    return { x: Math.round(p.x), y: Math.round(p.z) };
  };

  C.setHover = function (cell) {
    C.hover = cell;
    if (!cell) { C.hoverBox.visible = false; return; }
    C.hoverBox.position.set(cell.x, cell.h + 0.012, cell.y);
    C.hoverBox.material.opacity = 0.85;
    C.hoverBox.material.color.set(0x8fd2e8);
    C.hoverBox.visible = true;
  };

  C.frame = function (now) {
    R.faceCam(now);
    if (C.hoverBox && C.hoverBox.visible) C.hoverBox.material.opacity *= 0.985;
  };

  /* 指针：拖拽转相机；不拖拽时悬停高亮；点击交给 UI */
  C.attach = function (hooks) {
    const cv = R.canvas;
    let drag = false, lx = 0, ly = 0, moved = 0;

    cv.addEventListener('pointerdown', e => {
      drag = true; lx = e.clientX; ly = e.clientY; moved = 0;
      cv.setPointerCapture && cv.setPointerCapture(e.pointerId);
    });
    cv.addEventListener('pointerup', e => {
      drag = false;
      if (moved <= 10 && hooks && hooks.onClick) {
        const cell = C.pickAt(e.clientX, e.clientY);
        const pt = cell ? { x: cell.x, y: cell.y } : C.planeAt(e.clientX, e.clientY);
        hooks.onClick(cell, pt, e);
      }
    });
    cv.addEventListener('pointermove', e => {
      if (drag) {
        const dx = e.clientX - lx, dy = e.clientY - ly;
        moved += Math.abs(dx) + Math.abs(dy);
        C.yaw += dx * 0.006;
        C.pitch = Math.max(0.16, Math.min(1.52, C.pitch + dy * 0.004));
        lx = e.clientX; ly = e.clientY;
        C.place();
      }
      if (!drag && hooks && hooks.onHover) {
        const cell = C.pickAt(e.clientX, e.clientY);
        C.setHover(cell);
        hooks.onHover(cell, e);
      }
    });
    cv.addEventListener('pointerleave', () => { C.setHover(null); if (hooks && hooks.onHover) hooks.onHover(null); });
    cv.addEventListener('wheel', e => {
      e.preventDefault();
      C.dist = Math.max(3, Math.min(34, C.dist + e.deltaY * 0.012));
      C.zoomIdx = null;
      C.place();
    }, { passive: false });
  };

  C.pitchDeg = () => Math.round(C.pitch * 180 / Math.PI);

})(SOL.render, SOL.sim);
