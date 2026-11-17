// flashtest: is the province flash what slows the map? Loads a saved game, drags the map near a place, then marks
// two provinces as "changed hands this month" (ui.flash) and drags again.
const { chromium } = require('playwright'); const fs = require('fs');
const PAGE = process.env.TC_PAGE || 'file:///home/user/tc842/dist/play.html';
const [save, place = 'Warsaw', mode = 'all'] = process.argv.slice(2);
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
  await centre(); await page.waitForTimeout(500); console.log(`as loaded:        ${await move()}`);
  const info = await page.evaluate(() => { const by = {}; for (const a of document.getAnimations()) { const el = a.effect && a.effect.target; let p = el; while (p && p.parentNode && p.parentNode.id !== 'map') p = p.parentNode; const k = `${a.animationName || 'transition'} on ${el ? el.tagName + '.' + (el.getAttribute('class') || '') : '?'} in #${p && p.id}`; by[k] = (by[k] || 0) + 1; } const layers = [...document.querySelector('#map').children].map((c) => `${c.id || c.tagName}:${c.querySelectorAll('*').length}`).join(' '); const t = window.theater; let t0 = performance.now(); for (let i = 0; i < 10; i++) t.applyView(); const av = (performance.now() - t0) / 10; t0 = performance.now(); for (let i = 0; i < 3; i++) t.refresh(); const rf = (performance.now() - t0) / 3; return { by, layers, applyView_ms: +av.toFixed(1), refresh_ms: +rf.toFixed(1), fx: document.querySelector('#fx').innerHTML.replace(/ d="[^"]*"/g, '').replace(/\s+/g, ' ').slice(0, 500) }; });
  console.log(JSON.stringify(info).slice(0, 2200));
  const tryCss = async (label, css) => { const h = await page.addStyleTag({ content: css }); await centre(); await page.waitForTimeout(400); const r = await move(); console.log(`${label}: ${r.split(';')[0]}`); await h.evaluate((n) => n.remove()); };
  await tryCss('all paused        ', '#map * { animation-play-state: paused !important; }');
  await tryCss('freeme rings off  ', '#fx .freeme { animation: none !important; }');
  await tryCss('siege rings off   ', '#sieges .sgi { animation: none !important; }');
  await tryCss('armour flicker off', '#sieges .pzfire { animation: none !important; }');
  await tryCss('rings off, flicker on', '#fx .freeme, #sieges .sgi { animation: none !important; }');
  await tryCss('fx and sieges hidden', '#fx, #sieges { display: none !important; }');
  await centre(); await page.waitForTimeout(400); console.log(`as loaded again   : ${(await move()).split(';')[0]}`);
  const css = await page.evaluate(() => { const out = []; for (const sh of document.styleSheets) { let rules; try { rules = sh.cssRules; } catch (e) { continue; } for (const r of rules) { const t = r.cssText; if (/freeturn|pzflick|\.freeme|\.sgi|\.pzfire/.test(t)) out.push(t.replace(/\s+/g, ' ').slice(0, 260)); } } return out.slice(0, 8); });
  console.log(css.join('\n'));
  await browser.close();
})();
