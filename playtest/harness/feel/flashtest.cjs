// flashtest: is the province flash what slows the map? Loads a saved game, drags the map near a place, then marks
// two provinces as "changed hands this month" (ui.flash) and drags again.
const { chromium } = require('playwright'); const fs = require('fs');
const PAGE = process.env.TC_PAGE || 'file:///home/user/tc842/dist/play.html';
const [save, place = 'Warsaw', other = 'Krakow'] = process.argv.slice(2);
(async () => {
  const browser = await chromium.launch(); const page = await (await browser.newContext({ viewport: { width: 1440, height: 900 } })).newPage();
  await page.goto(PAGE); await page.evaluate((t) => { localStorage.clear(); localStorage.setItem('theater-command-save-v1', t); }, fs.readFileSync(save, 'utf8')); await page.reload(); await page.waitForSelector('#card');
  await page.evaluate(() => { try { window.theater.continueGame(); } catch (e) {} }); await page.waitForTimeout(2500); await page.evaluate(() => window.theater.openModal(null));
  const centre = () => page.evaluate((place) => { const t = window.theater, g = t.game(), el = document.querySelector('#map'); const r = g.regions.find((x) => x.name === place); const tall = el.clientHeight / el.clientWidth; t.ui.view.w = 240; t.ui.view.x = r.x - 120; t.ui.view.y = r.y - 120 * tall; t.applyView(); }, place);
  const move = async () => {
    const box = await page.evaluate(() => { const r = document.querySelector('#map').getBoundingClientRect(); return { x: r.x + r.width / 2, y: r.y + r.height / 2 }; });
    await page.evaluate(() => { const f = (window.__frames = { on: true, t: [], last: performance.now() }); const tick = (now) => { if (!f.on) return; f.t.push(now - f.last); f.last = now; requestAnimationFrame(tick); }; requestAnimationFrame(tick); });
    await page.mouse.move(box.x, box.y); await page.mouse.down();
    for (let i = 0; i < 40; i++) { const a = (i / 40) * Math.PI * 2; await page.mouse.move(box.x + Math.cos(a) * 160 - 160, box.y + Math.sin(a) * 110); await page.waitForTimeout(16); }
    await page.mouse.up(); await page.mouse.move(box.x + 40, box.y + 30);
    for (let i = 0; i < 8; i++) { await page.mouse.wheel(0, -120); await page.waitForTimeout(40); } for (let i = 0; i < 8; i++) { await page.mouse.wheel(0, 120); await page.waitForTimeout(40); }
    return page.evaluate(() => { const f = window.__frames; f.on = false; const t = f.t.slice(2).sort((a, b) => a - b); const q = (p) => t[Math.min(t.length - 1, Math.floor(t.length * p))]; return `${t.length} pictures, middle ${(1000 / q(0.5)).toFixed(0)} a second, slow ones ${(1000 / q(0.95)).toFixed(0)} a second, worst ${t[t.length - 1].toFixed(0)}ms; fx nodes ${document.querySelector('#fx').children.length}, animations ${document.getAnimations().length}`; });
  };
  await centre(); await page.waitForTimeout(500); console.log(`no flash:   ${await move()}`);
  await page.evaluate(({ place, other }) => { const t = window.theater, g = t.game(); t.ui.flash = g.regions.filter((r) => r.name === place || r.name === other).map((r) => r.id); t.refresh(); }, { place, other });
  await centre(); await page.waitForTimeout(500); console.log(`two flashes: ${await move()}`);
  await page.evaluate(() => { const t = window.theater; t.ui.flash = []; t.refresh(); });
  await centre(); await page.waitForTimeout(500); console.log(`cleared:    ${await move()}`);
  await browser.close();
})();
