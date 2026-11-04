/* 裁剪放大已有截图：node crop.js in.png x y w h scale out.png */
const path = require('path');
const fs = require('fs');
const { chromium } = require('playwright-core');
(async () => {
  const [inp, x, y, w, h, scale, out] = process.argv.slice(2);
  const S = +scale || 2, W = Math.round(+w * S), H = Math.round(+h * S);
  const b = fs.readFileSync(inp);
  const iw = b.readUInt32BE(16), ih = b.readUInt32BE(20);
  const url = 'data:image/png;base64,' + b.toString('base64');
  const browser = await chromium.launch({ channel: 'msedge', headless: true });
  const page = await browser.newPage({ viewport: { width: W, height: H }, deviceScaleFactor: 1 });
  await page.setContent(`<style>html,body{margin:0;padding:0;overflow:hidden;background:#000}
    #v{width:${W}px;height:${H}px;background-image:url('${url}');
    background-repeat:no-repeat;background-size:${iw * S}px ${ih * S}px;
    background-position:${-x * S}px ${-y * S}px}</style><div id="v"></div>`);
  await page.waitForTimeout(300);
  await page.screenshot({ path: path.resolve(out) });
  await browser.close();
  console.log('crop ->', out, W + 'x' + H);
})();
