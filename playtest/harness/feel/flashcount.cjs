// flashcount: what the province-capture flash costs the map. Loads a saved game, centres on a place, marks the two
// provinces nearest it as "changed hands" (ui.flash), calls refresh(), and counts the pictures the browser makes in
// the 2.4 seconds after, standing still and then in a drag. The same with no flash beside it. Three rounds each.
// A count of pictures over a timed move, never a middle figure (the design director's rule of 2026-11-17).
//   TC_PAGE=file:///.../dist/play.html [TC_SIZE=1600x1000] node flashcount.cjs SAVE PLACE|@mark
const { chromium } = require('playwright'); const fs = require('fs');
const PAGE = process.env.TC_PAGE; const [VW, VH] = (process.env.TC_SIZE || '1600x1000').split('x').map(Number);
const [save, place = 'Helsinki'] = process.argv.slice(2); const SPAN = 2400;
(async () => {
  const browser = await chromium.launch(); const page = await (await browser.newContext({ viewport: { width: VW, height: VH } })).newPage();
  await page.goto(PAGE); await page.evaluate((t) => { localStorage.clear(); localStorage.setItem('theater-command-save-v1', t); }, fs.readFileSync(save, 'utf8')); await page.reload(); await page.waitForSelector('#card');
  await page.evaluate(() => { try { window.theater.continueGame(); } catch (e) {} }); await page.waitForTimeout(2500); await page.evaluate(() => window.theater.openModal(null)); await page.waitForTimeout(300);
  // where to look: a named province, or the first mark that turns for ever
  const spot = await page.evaluate((place) => {
    const t = window.theater, g = t.game(); let at = null;
    if (place === '@mark') { for (const a of document.getAnimations()) { const el = a.effect && a.effect.target; if (!el || !el.closest || !el.closest('svg') || a.effect.getTiming().iterations !== Infinity) continue; const b = el.getBoundingClientRect(); if (!b.width && !b.height) continue; const mr = document.querySelector('#map').getBoundingClientRect(), v = t.ui.view; const k = v.w / mr.width; at = { x: v.x + (b.x + b.width / 2 - mr.x) * k, y: v.y + (b.y + b.height / 2 - mr.y) * k }; break; } }
    else { const r = g.regions.find((x) => x.name === place); if (r) at = { x: r.x, y: r.y }; }
    if (!at) return null;
    const near = g.regions.map((r) => ({ id: r.id, name: r.name, d: Math.hypot(r.x - at.x, r.y - at.y) })).sort((a, b) => a.d - b.d).slice(0, 2);
    return { ...at, ids: near.map((n) => n.id), names: near.map((n) => n.name) };
  }, place);
  if (!spot) { console.log(`no such place: ${place}`); await browser.close(); return; }
  console.log(`window ${VW} by ${VH}; looking at ${place}; the flash is laid on ${spot.names.join(' and ')}`);
  const centre = (w) => page.evaluate(([x, y, w]) => { const t = window.theater, el = document.querySelector('#map'); const tall = el.clientHeight / el.clientWidth; t.ui.view.w = w; t.ui.view.x = x - w / 2; t.ui.view.y = y - (w / 2) * tall; t.applyView(); }, [spot.x, spot.y, w]);
  const marksInView = () => page.evaluate(() => { const r = document.querySelector('#map').getBoundingClientRect(); let n = 0; for (const a of document.getAnimations()) { const el = a.effect && a.effect.target; if (!el || !el.closest || !el.closest('svg') || a.effect.getTiming().iterations !== Infinity) continue; const b = el.getBoundingClientRect(); if (b.right > r.left && b.left < r.right && b.bottom > r.top && b.top < r.bottom && (b.width || b.height)) n++; } return n; });
  const still = (flash) => page.evaluate(async ({ flash, ids, SPAN }) => {
    const t = window.theater; t.ui.flash = flash ? ids.slice() : []; t.refresh();
    const n0 = document.querySelectorAll('.flash').length; const t0 = performance.now(); const gaps = []; let last = t0;
    await new Promise((res) => { const tick = () => { const p = performance.now(); gaps.push(p - last); last = p; if (p - t0 >= SPAN) res(); else requestAnimationFrame(tick); }; requestAnimationFrame(tick); });
    return { n: gaps.length, ms: Math.round(last - t0), slow: gaps.filter((x) => x > 34).length, worst: Math.round(Math.max(...gaps)), n0, n1: document.querySelectorAll('.flash').length };
  }, { flash, ids: spot.ids, SPAN });
  const drag = async (flash) => {
    const box = await page.evaluate(() => { const r = document.querySelector('#map').getBoundingClientRect(); return { x: r.x + r.width / 2, y: r.y + r.height / 2 }; });
    await page.mouse.move(box.x, box.y);
    const x0 = await page.evaluate(({ flash, ids }) => { const t = window.theater; t.ui.flash = flash ? ids.slice() : []; t.refresh(); const f = (window.__f = { on: true, gaps: [], t0: performance.now(), last: performance.now(), n0: document.querySelectorAll('.flash').length }); const tick = () => { if (!f.on) return; const p = performance.now(); f.gaps.push(p - f.last); f.last = p; requestAnimationFrame(tick); }; requestAnimationFrame(tick); return t.ui.view.x; }, { flash, ids: spot.ids });
    await page.mouse.down(); const s = Date.now(); let i = 0, far = 0;
    while (Date.now() - s < SPAN) { const a = (i++ / 30) * Math.PI * 2; await page.mouse.move(box.x + Math.cos(a) * 60 - 60, box.y + Math.sin(a) * 40); await page.waitForTimeout(16); if (i % 6 === 3) far = Math.max(far, Math.abs((await page.evaluate(() => window.theater.ui.view.x)) - x0)); }
    const r = await page.evaluate(() => { const f = window.__f; f.on = false; return { n: f.gaps.length, ms: Math.round(f.last - f.t0), slow: f.gaps.filter((x) => x > 34).length, worst: Math.round(Math.max(...f.gaps)), n0: f.n0, n1: document.querySelectorAll('.flash').length }; });
    await page.mouse.up(); r.moved = far > 0; r.far = far; return r;
  };
  const line = (label, w, c, v) => console.log(`${label.padEnd(24)} width ${String(w).padStart(3)}  marks in view ${String(c).padStart(2)}  pictures ${v.map((r) => r.n).join(' / ')} in ${v.map((r) => (r.ms / 1000).toFixed(2)).join(' / ')} s = ${v.map((r) => Math.round((r.n / r.ms) * 1000)).join(' / ')} a second;  over 34 ms: ${v.map((r) => r.slow).join(' / ')};  worst ${v.map((r) => r.worst).join(' / ')} ms;  flash shapes at start ${v[0].n0}, at end ${v[0].n1}${v.some((r) => r.moved === false) ? '  (THE MAP DID NOT MOVE in a round)' : ''}`);
  for (const w of [240, 760]) {
    for (const [label, fn, flash] of [['still, no flash', still, false], ['still, the flash', still, true], ['dragged, no flash', drag, false], ['dragged, the flash', drag, true]]) {
      const v = []; let c = 0;
      for (let i = 0; i < 3; i++) { await centre(w); await page.waitForTimeout(500); c = await marksInView(); v.push(await fn(flash)); await page.waitForTimeout(200); }
      line(label, w, c, v);
    }
  }
  await browser.close();
})();
