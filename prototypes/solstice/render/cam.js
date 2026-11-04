/* Solstice · 相机与拾取
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

  /* 目标位姿。档位切换（F1–F4 / Z / 滚轮档 / 回中）只改这里，
     真正落到相机上的值每帧朝它靠——原来是 setPitch 里直接 place()，
     镜头一格一格跳，切换俯仰时整个画面「啪」地闪一下。
     拖拽与滚轮不属于「过渡」：那是手在直接操作，必须 1:1 跟手，
     所以它们会先把 goal 拉回当前值，打断进行中的缓动。 */
  C.goal = { yaw: 0, pitch: 0, dist: 0, x: 0, y: 0, z: 0 };
  let glideLast = 0;

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
    C.snapGoal();
  };

  C.place = function () {
    const cam = R.camera, t = C.target;
    const y = Math.sin(C.pitch) * C.dist, r = Math.cos(C.pitch) * C.dist;
    const px = t.x + Math.cos(C.yaw) * r, pz = t.z + Math.sin(C.yaw) * r;
    let py = t.y + y;
    /* 相机不许钻进地形：低俯角 + 近距离时，岩顶 0.986 会把镜头埋进去，
       画面变成一整片暗色（第二轮 W28 实测 camY 0.698 vs 岩顶 0.986）。
       抬到「机位下方与目标处地面较高者 + 0.22」——仍是原角度，只是不再穿进去。 */
    if (S && S.at && R.hOf) {
      const c1 = S.at(Math.round(px), Math.round(pz));
      const c2 = S.at(Math.round(t.x), Math.round(t.z));
      const g = Math.max(c1 ? R.hOf(c1) : 0.3, c2 ? R.hOf(c2) : 0.3);
      if (py < g + 0.22) py = g + 0.22;
    }
    cam.position.set(px, py, pz);
    cam.lookAt(t);
  };

  /* 把目标拉回当前值（手一动就打断缓动） */
  C.snapGoal = function () {
    const g = C.goal, t = C.target;
    g.yaw = C.yaw; g.pitch = C.pitch; g.dist = C.dist;
    if (t) { g.x = t.x; g.y = t.y; g.z = t.z; }
  };

  C.glideTo = function (x, y, z) {
    C.goal.x = x; C.goal.y = y; C.goal.z = z;
  };

  C.recenter = function () {
    if (!W.cells.size) return;
    let sx = 0, sy = 0;
    for (const c of W.cells.values()) { sx += c.x; sy += c.y; }
    C.glideTo(sx / W.cells.size, 0.22, sy / W.cells.size);
  };

  C.setPitch = function (i) {
    C.pitchIdx = Math.max(0, Math.min(3, i));
    C.goal.pitch = PITCHES[C.pitchIdx];
  };

  C.setZoom = function (i) {
    C.zoomIdx = Math.max(0, Math.min(4, i));
    C.goal.dist = ZOOMS[C.zoomIdx];
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

  /* 指针跟着鼠标时高亮会自己淡掉（它只是一次经过）；键盘游标不同——
     游标是「我现在指着这一格」，人要想两秒，淡掉就等于没有。 */
  C.setHover = function (cell, keep) {
    C.hover = cell;
    C.keepHover = !!keep;
    if (!cell) { C.hoverBox.visible = false; return; }
    /* 贴地高度必须走 R.hOf：cell.h 恒为 0.30，草地/林地/岩地比它高，
       用逻辑高度画框会让框陷进方块里（草地那一圈几乎看不见）。 */
    const top = (R.hOf && S.at(cell.x, cell.y)) ? R.hOf(S.at(cell.x, cell.y)) : cell.h;
    C.hoverBox.position.set(cell.x, top + 0.012, cell.y);
    C.hoverBox.material.opacity = 0.85;
    C.hoverBox.material.color.set(0x8fd2e8);
    C.hoverBox.visible = true;
  };

  C.frame = function (now) {
    /* 朝目标位姿缓动。指数趋近（1 − e^(−dt/τ)）而不是定速 lerp：
       定速在掉帧时会变慢、在帧率稳定时又显得僵硬，指数永远同一个「手感时间」。 */
    const g = C.goal, t = C.target;
    const dt = glideLast ? Math.min(64, now - glideLast) : 16;
    glideLast = now;
    const k = 1 - Math.exp(-dt / 78);
    let moving = false;
    if (Math.abs(g.yaw - C.yaw) > 1e-4) { C.yaw += (g.yaw - C.yaw) * k; moving = true; }
    if (Math.abs(g.pitch - C.pitch) > 1e-4) { C.pitch += (g.pitch - C.pitch) * k; moving = true; }
    if (Math.abs(g.dist - C.dist) > 1e-3) { C.dist += (g.dist - C.dist) * k; moving = true; }
    if (t) {
      if (Math.abs(g.x - t.x) > 1e-3) { t.x += (g.x - t.x) * k; moving = true; }
      if (Math.abs(g.y - t.y) > 1e-3) { t.y += (g.y - t.y) * k; moving = true; }
      if (Math.abs(g.z - t.z) > 1e-3) { t.z += (g.z - t.z) * k; moving = true; }
    }
    if (moving) {
      /* 收敛到阈内就吸附，否则永远差最后一点点、每帧都 place() 一次 */
      if (Math.abs(g.yaw - C.yaw) < 1e-3) C.yaw = g.yaw;
      if (Math.abs(g.pitch - C.pitch) < 1e-3) C.pitch = g.pitch;
      if (Math.abs(g.dist - C.dist) < 1e-2) C.dist = g.dist;
      if (t) {
        if (Math.abs(g.x - t.x) < 1e-2) t.x = g.x;
        if (Math.abs(g.y - t.y) < 1e-2) t.y = g.y;
        if (Math.abs(g.z - t.z) < 1e-2) t.z = g.z;
      }
      C.place();
    }
    R.faceCam(now);
    if (!C.hoverBox || !C.hoverBox.visible) return;
    C.hoverBox.material.opacity = C.keepHover ? 0.85 : C.hoverBox.material.opacity * 0.985;
  };

  /* 指针：拖拽转相机；不拖拽时悬停高亮；点击交给 UI */
  C.attach = function (hooks) {
    const cv = R.canvas;
    let drag = false, lx = 0, ly = 0, moved = 0, downOnCv = false;

    cv.addEventListener('pointerdown', e => {
      drag = true; downOnCv = true; lx = e.clientX; ly = e.clientY; moved = 0;
      cv.setPointerCapture && cv.setPointerCapture(e.pointerId);
    });
    cv.addEventListener('pointerup', e => {
      drag = false;
      /* 只处理「在画布上按下、在画布上抬起」的点击。
         从手牌拖进来的 pointerup 也落在画布上，moved 还是 0 → 会被当成一次点击，
         于是同一次拖放施了两张牌：endDrag 一张、这里又一张（第二轮 S03）。 */
      const wasDown = downOnCv; downOnCv = false;
      if (!wasDown) return;
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
        C.yaw += dx * 0.006 * C.sens;
        C.pitch = Math.max(0.16, Math.min(1.52, C.pitch + dy * 0.004 * C.sens));
        lx = e.clientX; ly = e.clientY;
        /* 手在拖，就打断缓动：不打断的话每帧缓动会把镜头往旧目标拽一点，拖起来发涩 */
        C.snapGoal();
        C.place();
      }
      if (!drag && hooks && hooks.onHover) {
        const cell = C.pickAt(e.clientX, e.clientY);
        C.setHover(cell);
        /* 悬停也要给「平面点」：土地这类卡落在虚空上时没有格子可拾取，
           预览要画的就是虚空里将要长出来的那块地（REVIEW-DEMO #11）。 */
        hooks.onHover(cell, e, cell ? null : C.planeAt(e.clientX, e.clientY));
      }
    });
    cv.addEventListener('pointerleave', () => { C.setHover(null); if (hooks && hooks.onHover) hooks.onHover(null); });
    cv.addEventListener('wheel', e => {
      e.preventDefault();
      C.dist = Math.max(3, Math.min(34, C.dist + e.deltaY * 0.012 * C.sens));
      C.zoomIdx = null;
      C.snapGoal();
      C.place();
    }, { passive: false });
  };

  C.pitchDeg = () => Math.round(C.pitch * 180 / Math.PI);

  /* 拖动灵敏度：设置面板里那一项原来是个空实现（set: () => {}），
     点了没反应还写在「只有真正接线的项」的列表里。现在真接到 yaw / pitch / wheel。 */
  C.SENS = { 慢: 0.6, 中: 1, 快: 1.6 };
  C.sens = 1;
  C.sensLabel = '中';
  C.setSens = function (label) {
    C.sensLabel = C.SENS[label] == null ? '中' : label;
    C.sens = C.SENS[C.sensLabel];
  };

})(SOL.render, SOL.sim);
