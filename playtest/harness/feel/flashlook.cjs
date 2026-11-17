// flashlook: does the flash still show and fade? Reads the flash shapes' strength at set times after refresh() and takes one picture.
const { chromium } = require('playwright'); const fs = require('fs');
const PAGE = process.env.TC_PAGE; const [save, place = 'Helsinki', shot = ''] = process.argv.slice(2);
(async () => {
  const browser = await chromium.launch(); const page = await (await browser.newContext({ viewport: { width: 1600, height: 1000 } })).newPage();
  await page.goto(PAGE); await page.evaluate((t) => { localStorage.clear(); localStorage.setItem('theater-command-save-v1', t); }, fs.readFileSync(save, 'utf8')); await page.reload(); await page.waitForSelector('#card');
  await page.evaluate(() => { try { window.theater.continueGame(); } catch (e) {} }); await page.waitForTimeout(2500); await page.evaluate(() => window.theater.openModal(null)); await page.waitForTimeout(300);
  await page.evaluate((place) => { const t = window.theater, g = t.game(), el = document.querySelector('#map'); const r = g.regions.find((x) => x.name === place); const tall = el.clientHeight / el.clientWidth; t.ui.view.w = 240; t.ui.view.x = r.x - 120; t.ui.view.y = r.y - 120 * tall; t.applyView(); const near = g.regions.map((q) => ({ id: q.id, d: Math.hypot(q.x - r.x, q.y - r.y) })).sort((a, b) => a.d - b.d).slice(0, 2).map((q) => q.id); t.ui.flash = near; t.refresh(); window.__t0 = performance.now(); }, place);
  const out = [];
  for (const at of [60, 400, 800, 1300, 1800, 2150, 2400]) {
    const now = await page.evaluate(() => performance.now() - window.__t0); if (at > now) await page.waitForTimeout(at - now);
    if (at === 60 && shot) await page.screenshot({ path: shot });
    out.push(await page.evaluate(() => { const f = [...document.querySelectorAll('.flash')]; return `${Math.round(performance.now() - window.__t0)} ms: ${f.length} shapes${f.length ? ', strength ' + getComputedStyle(f[0]).opacity.slice(0, 5) + ', animations ' + f[0].getAnimations().length : ''}`; }));
  }
  console.log(out.join('\n')); await browser.close();
})();
