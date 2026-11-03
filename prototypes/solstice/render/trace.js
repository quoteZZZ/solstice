/* 至日 · 痕迹层
   每次施卡在地面留一个能看见的东西（否则玩家会觉得卡没用）。
   痕迹是纯表现 + 一条数据（sim.W.traces），点选在右栏列表里做，不在 3D 里做射线。 */
(function (R, S, D) {
  const W = S.W;
  const T = R.trace = {};

  let sig = '';

  T.sync = function (force) {
    const cur = W.traces.map(t => t.id + ':' + t.count + ':' + t.tier).join('|') + '@' + (force ? W.month : '');
    if (cur === sig) return;
    sig = cur;

    const g = R.traceGroup;
    while (g.children.length) g.remove(g.children[0]);

    for (const t of W.traces) {
      const cell = S.at(t.x, t.y);
      if (!cell) continue;
      const col = new THREE.Color(D.elColor(t.el));
      const base = cell.h + 0.014;
      const r = 0.16 + Math.min(4, t.count || 1) * 0.045 + t.tier * 0.012;

      const ring = new THREE.Mesh(
        new THREE.RingGeometry(r * 0.72, r, 26),
        new THREE.MeshBasicMaterial({ color: col, transparent: true, opacity: t.tier >= 7 ? 0.55 : 0.36, side: THREE.DoubleSide })
      );
      ring.rotation.x = -Math.PI / 2;
      ring.position.set(t.x, base, t.y);
      g.add(ring);

      const dot = new THREE.Mesh(
        new THREE.OctahedronGeometry(r * 0.34),
        new THREE.MeshBasicMaterial({ color: col, transparent: true, opacity: 0.8 })
      );
      dot.position.set(t.x, base + 0.1, t.y);
      dot.userData.trace = t;
      g.add(dot);
    }
  };

  T.reset = function () { sig = ''; };

})(SOL.render, SOL.sim, SOL.data);
