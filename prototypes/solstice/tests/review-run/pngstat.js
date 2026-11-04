/* 极简 PNG 解码 + 像素统计（只给评审脚本用，不进产品）
   支持 8bit 的 color type 0/2/4/6、非隔行——Playwright 截图就是这一类。 */
const fs = require('fs');
const zlib = require('zlib');

function loadPNG(file) {
  const buf = fs.readFileSync(file);
  if (buf.readUInt32BE(0) !== 0x89504e47) throw new Error('not png');
  let pos = 8, ihdr = null;
  const idat = [];
  while (pos < buf.length) {
    const len = buf.readUInt32BE(pos);
    const type = buf.toString('ascii', pos + 4, pos + 8);
    const data = buf.subarray(pos + 8, pos + 8 + len);
    if (type === 'IHDR') {
      ihdr = {
        w: data.readUInt32BE(0), h: data.readUInt32BE(4),
        depth: data[8], color: data[9], interlace: data[12]
      };
    } else if (type === 'IDAT') idat.push(data);
    else if (type === 'IEND') break;
    pos += 12 + len;
  }
  if (!ihdr) throw new Error('no IHDR');
  if (ihdr.depth !== 8 || ihdr.interlace !== 0) throw new Error('unsupported png ' + JSON.stringify(ihdr));
  const bpp = { 0: 1, 2: 3, 3: 1, 4: 2, 6: 4 }[ihdr.color];
  if (!bpp) throw new Error('color type ' + ihdr.color);
  const raw = zlib.inflateSync(Buffer.concat(idat));
  const { w, h } = ihdr;
  const stride = w * bpp;
  const out = Buffer.alloc(w * h * 4);
  let prev = Buffer.alloc(stride);
  let rp = 0;
  for (let y = 0; y < h; y++) {
    const ft = raw[rp++];
    const line = Buffer.from(raw.subarray(rp, rp + stride)); rp += stride;
    for (let i = 0; i < stride; i++) {
      const a = i >= bpp ? line[i - bpp] : 0;
      const b = prev[i];
      const c = i >= bpp ? prev[i - bpp] : 0;
      let v = line[i];
      if (ft === 1) v += a;
      else if (ft === 2) v += b;
      else if (ft === 3) v += (a + b) >> 1;
      else if (ft === 4) {
        const p = a + b - c;
        const pa = Math.abs(p - a), pb = Math.abs(p - b), pc = Math.abs(p - c);
        v += (pa <= pb && pa <= pc) ? a : (pb <= pc ? b : c);
      }
      line[i] = v & 255;
    }
    if (ihdr.color === 6) line.copy(out, y * w * 4);
    else if (ihdr.color === 2) {
      for (let x = 0; x < w; x++) {
        out[y * w * 4 + x * 4] = line[x * 3];
        out[y * w * 4 + x * 4 + 1] = line[x * 3 + 1];
        out[y * w * 4 + x * 4 + 2] = line[x * 3 + 2];
        out[y * w * 4 + x * 4 + 3] = 255;
      }
    } else if (ihdr.color === 0) {
      for (let x = 0; x < w; x++) {
        const g = line[x];
        out[y * w * 4 + x * 4] = g; out[y * w * 4 + x * 4 + 1] = g; out[y * w * 4 + x * 4 + 2] = g; out[y * w * 4 + x * 4 + 3] = 255;
      }
    }
    prev = line;
  }
  return { w, h, data: out };
}

const px = (img, x, y) => {
  const i = ((y | 0) * img.w + (x | 0)) * 4;
  return [img.data[i], img.data[i + 1], img.data[i + 2]];
};
const luma = ([r, g, b]) => 0.2126 * r + 0.7152 * g + 0.0722 * b;
const hex = ([r, g, b]) => '#' + [r, g, b].map(v => v.toString(16).padStart(2, '0')).join('');
const dist = (a, b) => Math.sqrt((a[0] - b[0]) ** 2 + (a[1] - b[1]) ** 2 + (a[2] - b[2]) ** 2);

/* 区域统计：均值亮度、亮度标准差、亮部/暗部占比 */
function region(img, x0, y0, w, h) {
  let n = 0, s = 0, s2 = 0, dark = 0, bright = 0;
  for (let y = Math.max(0, y0); y < Math.min(img.h, y0 + h); y++) {
    for (let x = Math.max(0, x0); x < Math.min(img.w, x0 + w); x++) {
      const L = luma(px(img, x, y));
      n++; s += L; s2 += L * L;
      if (L < 40) dark++;
      if (L > 140) bright++;
    }
  }
  const mean = s / Math.max(1, n);
  return { n, mean: +mean.toFixed(1), sd: +Math.sqrt(Math.max(0, s2 / n - mean * mean)).toFixed(1), dark: +(dark / n).toFixed(3), bright: +(bright / n).toFixed(3) };
}

/* 取一条扫描线（水平或竖直），返回亮度与相邻差分；用来找「硬边」 */
function scan(img, kind, fixed, from, to) {
  const arr = [];
  for (let t = from; t <= to; t++) {
    const x = kind === 'h' ? t : fixed;
    const y = kind === 'h' ? fixed : t;
    arr.push(luma(px(img, x, y)));
  }
  let maxJump = 0, at = 0, jumps = [];
  for (let i = 1; i < arr.length; i++) {
    const d = Math.abs(arr[i] - arr[i - 1]);
    jumps.push(+d.toFixed(1));
    if (d > maxJump) { maxJump = d; at = i; }
  }
  return { arr: arr.map(v => +v.toFixed(1)), maxJump: +maxJump.toFixed(1), at, jumps };
}

module.exports = { loadPNG, px, luma, hex, dist, region, scan };
