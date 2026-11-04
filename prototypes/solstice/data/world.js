/* Solstice · 世界掷骰数据
   形制 / 气候 / 特征 / 判据 / 命名词表。真源见 specs/SETTING.md，本文件只是可跑的一份投影。 */
window.SOL = window.SOL || {};
SOL.data = SOL.data || {};

SOL.data.world = {
  /* 形制：拓扑 + 顶视轮廓（剖面与 3D 都从这里取） */
  forms: [
    { name:'群岛界', code:'ARC', suffix:'屿', blocks:'5–12', topo:'破碎', note:'水道纵横，每块岛各长各的',
      isles:[[-9,0,5.4,2.6],[4,-2,4.6,2.2],[10,4,3.4,1.8],[-3,7,2.8,1.4]] },
    { name:'单岛界', code:'ISL', suffix:'岛', blocks:'1', topo:'团块', note:'基准形制：一块地、一圈海',
      isles:[[0,0,8.2,4.2]] },
    { name:'大陆界', code:'CON', suffix:'原', blocks:'1（≥85 %）', topo:'团块', note:'大到看不见边，只能走进去',
      isles:[[0,0,13,6.4]] },
    { name:'半岛界', code:'PEN', suffix:'岬', blocks:'1', topo:'折枝', note:'一半是陆地，一半是外面',
      isles:[[-2,0,9.4,4.6]] },
    { name:'环礁界', code:'ATL', suffix:'环礁', blocks:'1 环', topo:'环', note:'一圈礁围着中间的水',
      isles:[[0,0,8.6,4.0],[0,0,4.6,2.0]] },
    { name:'峡湾界', code:'FJO', suffix:'湾', blocks:'1', topo:'折枝', note:'长水道切进山里',
      isles:[[0,0,8.8,4.2]] },
    { name:'内陆湖国', code:'LAK', suffix:'泊', blocks:'1', topo:'团块', note:'没有海岸线，水源在内陆',
      isles:[[0,0,9.2,4.4]] },
    { name:'裂谷界', code:'RIF', suffix:'裂谷', blocks:'1', topo:'破碎', note:'一道深谷把世界劈开',
      isles:[[0,0,9.6,4.4]] },
    { name:'全海界', code:'OCE', suffix:'海', blocks:'0 陆', topo:'无', note:'只有水；陆是施卡造出来的',
      isles:[] },
    { name:'全陆界', code:'LAN', suffix:'陆', blocks:'无海岸', topo:'团块', note:'整块陆地，边缘收进雾里',
      isles:[[0,0,14,7]] }
  ],

  climates: ['热带雨林','热带季风','沙漠','地中海','温带海洋','温带大陆','寒温带针叶','极地苔原','高原'],

  /* 特征：一条特征必须能被世界读到，否则不进列表。
     `wired:true` = 有结算消费者（time.js 的 W.effects 或 cast.js 的 op）；
     `wired:false` = 只在剖面图里出现，UI 必须明写「仅剖面」，不能假装生效。
     没有 `init` 字段：开局是 0 格（SETTING §1.1），"初始地形"与虚无开局不能同时成立。 */
  traits: [
    { ico:'矿', name:'金属富集', cat:'资源', note:'新铺的土地有 35 % 概率含矿；含矿格敲钟多 2 时素',
      tags:['metal'], wired:true, val:'35% 含矿' },
    { ico:'热', name:'地热活跃', cat:'地貌', note:'全界气温 +0.5 °C，蒸发更快',
      tags:['heat'], wired:true, val:'+0.5 °C' },
    { ico:'潮', name:'强潮汐', cat:'气候', note:'潮差加倍，潮间带变宽',
      tags:['tide'], wired:false },
    { ico:'浮', name:'浮空岛', cat:'奇观', note:'一处奇观：悬空陆地投下移动阴影',
      tags:['wonder'], wired:false },
    { ico:'雷', name:'永久雷暴', cat:'气候', note:'一片不散的雷云，落雷在岩面留下焦痕',
      tags:['storm'], wired:false },
    { ico:'隔', name:'物种隔离', cat:'生态', note:'外来物种迁入更慢：繁殖速率 ×0.8',
      tags:['isolate'], wired:true, val:'×0.8 繁殖' },
    { ico:'月', name:'双月', cat:'天象', note:'潮汐加倍，夜光更亮',
      tags:['moon'], wired:false },
    { ico:'树', name:'古树群', cat:'奇观', note:'一株通天巨树，冠层自成天气',
      tags:['ancient'], wired:false },
    { ico:'崖', name:'深海崖岸', cat:'地貌', note:'海岸多断崖，浅滩变少',
      tags:['cliff'], wired:false },
    { ico:'瘠', name:'贫瘠土壤', cat:'生态', note:'土壤肥力 −30 %，演替更慢',
      tags:['barren'], wired:true, val:'×0.7 生长' }
  ],

  judge: ['边缘有交代','有层次','有对比','有落点','有尺度物','无死面','三级成立'],

  syl1: ['灰','白','长','碎','冷','雾','沉','青','赤','双','孤','暖'],
  syl2: ['羽','石','脊','沙','潮','星','苔','纹','岚','野'],
  suffixes: ['屿','岛','原','岬','环礁','湾','泊','裂谷','海','陆']
};

/* 元素：顺序即图鉴编号顺序（schema §2：金木水火土光暗 + 复合风） */
SOL.data.elements = {
  order: ['金','木','水','火','土','光','暗','风'],
  defs: {
    金:{ c:'#c8a94b', rgb:'200,169,75' },
    木:{ c:'#5e8f47', rgb:'94,143,71' },
    水:{ c:'#3f8fab', rgb:'63,143,171' },
    火:{ c:'#b4432e', rgb:'180,67,46' },
    土:{ c:'#8a6a45', rgb:'138,106,69' },
    光:{ c:'#e0d6b8', rgb:'224,214,184' },
    暗:{ c:'#6a5a8a', rgb:'96,82,132' },
    风:{ c:'#7fa3a8', rgb:'127,163,168' }
  },
  cls: { 金:'el-metal', 木:'el-wood', 水:'el-water', 火:'el-fire', 土:'el-earth', 光:'el-light', 暗:'el-dark', 风:'el-wind' },
  /* 元素纹样（100×100 viewBox），卡面画芯缺 art 时用 */
  sigil: {
    金:'<path d="M50 14 L78 50 L50 86 L22 50 Z" fill="none" stroke="currentColor" stroke-width="2.6"/><path d="M50 30 L63 50 L50 70 L37 50 Z" fill="currentColor" opacity=".4"/>',
    木:'<path d="M50 88 L50 38" stroke="currentColor" stroke-width="3.2"/><path d="M50 46 C32 42 26 28 28 17 C45 21 50 32 50 46 Z" fill="currentColor" opacity=".85"/><path d="M50 58 C68 54 74 40 72 29 C55 33 50 44 50 58 Z" fill="currentColor" opacity=".55"/>',
    水:'<path d="M14 56 C28 42 38 70 50 56 C62 42 72 70 86 56" fill="none" stroke="currentColor" stroke-width="3"/><path d="M14 70 C28 56 38 84 50 70 C62 56 72 84 86 70" fill="none" stroke="currentColor" stroke-width="2" opacity=".5"/>',
    火:'<path d="M50 12 C62 34 76 44 67 62 C61 75 50 82 42 75 C29 66 36 48 45 42 C45 57 51 55 51 47 C51 34 47 24 50 12 Z" fill="currentColor" opacity=".85"/>',
    土:'<path d="M20 62 L50 36 L80 62 L80 82 L20 82 Z" fill="none" stroke="currentColor" stroke-width="2.8"/><path d="M33 62 L50 47 L67 62" fill="none" stroke="currentColor" stroke-width="2" opacity=".6"/>',
    光:'<circle cx="50" cy="50" r="12" fill="currentColor"/><path d="M50 4 L50 20 M50 80 L50 96 M4 50 L20 50 M80 50 L96 50 M18 18 L29 29 M71 71 L82 82 M82 18 L71 29 M29 71 L18 82" stroke="currentColor" stroke-width="2.6" opacity=".8"/>',
    暗:'<path d="M62 16 A34 34 0 1 0 62 84 A26 26 0 1 1 62 16 Z" fill="currentColor" opacity=".85"/>',
    风:'<path d="M14 40 C34 28 50 50 68 40 C78 34 86 36 92 40" fill="none" stroke="currentColor" stroke-width="2.8" opacity=".85"/><path d="M18 56 C38 44 52 66 70 56 C80 50 88 52 94 56" fill="none" stroke="currentColor" stroke-width="2" opacity=".55"/><path d="M24 74 C42 64 54 84 72 74" fill="none" stroke="currentColor" stroke-width="1.6" opacity=".35"/>'
  }
};
