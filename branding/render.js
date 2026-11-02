// Renders the SVG sources here into the app's PNGs: node render.js (from this folder)
const {chromium} = require('playwright');
const fs = require('fs');
const res = '../src/app/platform/android/res/';
const jobs = [
  ['logo.svg', 1640, 500, '../src/frontend/assets/logo.png'],
  ['banner.svg', 320, 180, res + 'mipmap-xhdpi/banner.png'],
];
const legacy = {ldpi: 36, mdpi: 48, hdpi: 72, xhdpi: 96, xxhdpi: 144, xxxhdpi: 192};
const adaptive = {mdpi: 108, hdpi: 162, xhdpi: 216, xxhdpi: 324, xxxhdpi: 432};
for (const [d, s] of Object.entries(legacy)) jobs.push(['icon.svg', s, s, res + `mipmap-${d}/icon.png`]);
for (const [d, s] of Object.entries(adaptive)) {
  jobs.push(['icon_fg.svg', s, s, res + `mipmap-${d}/icon_fg.png`]);
  jobs.push(['icon_bg.svg', s, s, res + `mipmap-${d}/icon_bg.png`]);
}
(async () => {
  const b = await chromium.launch();
  for (const [src, w, h, out] of jobs) {
    const p = await b.newPage({viewport: {width: w, height: h}});
    const svg = fs.readFileSync(src, 'utf8');
    await p.setContent(`<html><head><style>html,body{margin:0;background:transparent}svg{display:block;width:${w}px;height:${h}px}</style></head><body>${svg}</body></html>`);
    await p.screenshot({path: out, omitBackground: true});
    await p.close();
    console.log(out, w + 'x' + h);
  }
  await b.close();
})();
