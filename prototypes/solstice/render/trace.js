/* Solstice · 痕迹层
   每次施卡在地面留一个能看见的东西（否则玩家会觉得卡没用）。
   痕迹是纯表现 + 一条数据（sim.W.traces），点选在右栏列表里做，不在 3D 里做射线。

   REVIEW-DEMO V-3：原来 op 0.36/0.55 的实心环 + 每月全量重建，
   25 张卡之后地表被同心圆环铺满，地形本身看不见了。现在改成
   「贴地、低透、按寿命衰减」，并且只在痕迹真的变了才重建。 */
(function (R, S, D) {
  const W = S.W;
  const T = R.trace = {};

  let sig = '';

  T.sync = function () {
    /* 签名只跟痕迹数据走——不再带 @month。
       带上月份等于每月重建一次全部环，既漏 geometry 又没必要。 */
    const cur = W.traces.length + '#' + W.traces.map(t => t.id + ':' + t.count + ':' + t.tier).join('|');
    if (cur === sig) return;
    sig = cur;

    const g = R.traceGroup;
    while (g.children.length) { const c = g.children[0]; g.remove(c); R.dispose(c); }

    for (const t of W.traces) {
      const cell = S.at(t.x, t.y);
      if (!cell) continue;
      const col = new THREE.Color(D.elColor(t.el));
      /* 贴地高度取「看得见的那个面」——原来读 sim 的 cell.h（一律 0.30），
         地形一抬高，环就埋进方块里了。 */
      const base = (R.hOf ? R.hOf(cell) : cell.h) + 0.012;
      const r = 0.14 + Math.min(4, t.count || 1) * 0.018 + t.tier * 0.006;

      /* 环：贴地、细、低透。它是"被写过的印子"，不是靶心。 */
      const ring = new THREE.Mesh(
        new THREE.RingGeometry(r * 0.78, r, 24),
        new THREE.MeshBasicMaterial({
          color: col, transparent: true, side: THREE.DoubleSide,
          opacity: 0.16, depthWrite: false
        })
      );
      ring.rotation.x = -Math.PI / 2;
      ring.position.set(t.x, base, t.y);
      ring.userData.trace = t;
      g.add(ring);

      /* 中心那颗小结晶是"这里真发生过事"的唯一高对比标记，只给 3 环以上 */
      if (t.tier >= 3) {
        const dot = new THREE.Mesh(
          new THREE.OctahedronGeometry(r * 0.22),
          new THREE.MeshBasicMaterial({ color: col, transparent: true, opacity: 0.55 })
        );
        dot.position.set(t.x, base + 0.07, t.y);
        dot.userData.trace = t;
        g.add(dot);
      }
    }
  };

  /* 按剩余寿命淡出：越接近过期越淡。放逐帧做，免得为透明度重建整层。 */
  T.fade = function () {
    const g = R.traceGroup;
    if (!g) return;
    for (const m of g.children) {
      const t = m.userData.trace;
      if (!t || !t.life || t.life >= 999) continue;
      const age = W.month - t.born;
      const k = Math.max(0, 1 - age / Math.max(1, t.life));
      m.material.opacity = (m.geometry.type === 'RingGeometry' ? 0.16 : 0.55) * (0.35 + 0.65 * k);
    }
  };

  T.reset = function () { sig = ''; };

})(SOL.render, SOL.sim, SOL.data);
