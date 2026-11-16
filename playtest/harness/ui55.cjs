// Version 82.3 on the page: is the rail reach of an army in hand reachable with the pointer (or hidden under the army's
// panel), is the lit railway seen outside the panel, and what the pointing box says after a click on the map.
// usage: [TC_PAGE=...] node ui55.cjs [NAT=GER] [SEED=7] [PREFIX]
const { chromium } = require('playwright');
const PAGE = process.env.TC_PAGE || 'file:///home/user/tc823/dist/play.html';
const [nat = 'GER', seedS = '7', prefix = '105-v823-point'] = process.argv.slice(2);
const clean = (s) => (s || '').replace(/\s+/g, ' ').trim();
(async () => {
  const browser = await chromium.launch(); const errors = [];
  const page = await (await browser.newContext({ viewport: { width: 1440, height: 900 }, deviceScaleFactor: 2 })).newPage();
  page.on('pageerror', (e) => errors.push(e.message.slice(0, 300)));
  await page.goto(PAGE); await page.evaluate(() => { localStorage.clear(); localStorage.setItem('tc-watch', '0'); }); await page.reload(); await page.waitForSelector('#card');
  await page.evaluate(([nat, seed]) => window.theater.newGame(nat, seed, 1, 'grand', 'colonel', true, false, 'theatre', 3), [nat, Number(seedS)]);
  await page.waitForTimeout(2500); await page.evaluate(() => window.theater.openModal(null)); await page.waitForTimeout(300);
  const pick = await page.evaluate((nat) => { const g = window.theater.game(); const foe = (r) => r.nb.some((n) => { const o = g.regions[n]; return o.ctrl !== nat && g.nations[nat].atWar.includes(o.ctrl); }); const rear = g.regions.filter((r) => r.ctrl === nat && !r.overseas && !foe(r) && r.units.length); rear.sort((a, b) => b.units.length - a.units.length); const r = rear[0]; return r ? { id: r.id, name: r.name, corps: r.units.length } : null; }, nat);
  console.log('the army in hand: ' + JSON.stringify(pick));
  await page.evaluate((id) => window.theater.onRegion(id), pick.id); await page.waitForTimeout(700);
  const tipNow = async () => page.evaluate(() => [...document.querySelectorAll('.tip')].filter((e) => { const s = getComputedStyle(e); return s.display !== 'none' && s.visibility !== 'hidden' && Number(s.opacity) > 0.05 && e.getBoundingClientRect().width > 0; }).map((e) => (e.innerText || '').replace(/\s+/g, ' ').trim()).filter(Boolean));
  const look = async (label) => {
    const r = await page.evaluate(() => {
      const top = (x, y) => { const e = document.elementFromPoint(x, y); if (!e) return 'nothing'; const inMap = !!e.closest('#map'); const p = e.closest('[id]'); return (inMap ? 'map' : 'COVERED by #' + (p ? p.id : '?')) + ' <' + e.tagName.toLowerCase() + (e.getAttribute('class') ? '.' + String(e.getAttribute('class')).split(' ').join('.') : '') + '>'; };
      const far = [...document.querySelectorAll('#map .mk.railfar')].map((e) => { const b = e.getBoundingClientRect(); const x = b.x + b.width / 2, y = b.y + b.height / 2; return { x: Math.round(x), y: Math.round(y), on: x > 0 && y > 0 && x < innerWidth && y < innerHeight, top: top(x, y) }; });
      const lit = [...document.querySelectorAll('#map .raillit')].map((e) => { const b = e.getBoundingClientRect(); return { x: Math.round(b.x), y: Math.round(b.y), w: Math.round(b.width), h: Math.round(b.height), stroke: getComputedStyle(e).stroke, sw: getComputedStyle(e).strokeWidth, op: getComputedStyle(e).opacity }; });
      const say = document.querySelector('.railsay'); let panel = null; if (say) { let p = say; while (p.parentElement && p.parentElement !== document.body && p.getBoundingClientRect().height < 120) p = p.parentElement; const b = p.getBoundingClientRect(); panel = { id: p.id || p.className, x: Math.round(b.x), y: Math.round(b.y), w: Math.round(b.width), h: Math.round(b.height) }; }
      const map = document.querySelector('#map').getBoundingClientRect();
      return { far, lit, panel, map: { x: Math.round(map.x), y: Math.round(map.y), w: Math.round(map.width), h: Math.round(map.height) } };
    });
    console.log(`${label}: the map ${JSON.stringify(r.map)}; the army's panel ${JSON.stringify(r.panel)}`);
    console.log(`${label}: rail-reach marks ${r.far.length}: ` + JSON.stringify(r.far));
    console.log(`${label}: lit railway pieces ${r.lit.length}: ` + JSON.stringify(r.lit.slice(0, 6)));
    return r;
  };
  let r = await look('as taken in hand');
  await page.screenshot({ path: `shots/${prefix}-1-in-hand.png` });
  // Hover each rail-reach mark where the map is on top; then just beside it (a mark may itself swallow the pointer).
  for (const p of r.far) { for (const [dx, dy] of [[0, 0], [14, -14]]) { await page.mouse.move(p.x + dx, p.y + dy); await page.waitForTimeout(450); console.log(`  the pointer at ${p.x + dx},${p.y + dy} (${p.top}): ` + JSON.stringify(await tipNow())); } }
  // Zoom out twice and look again.
  for (let i = 0; i < 2; i++) { await page.evaluate(() => document.querySelector('[data-act="zout"]')?.click()); await page.waitForTimeout(350); }
  r = await look('zoomed out twice');
  for (const p of r.far) { await page.mouse.move(p.x, p.y); await page.waitForTimeout(450); console.log(`  the pointer at ${p.x},${p.y} (${p.top}): ` + JSON.stringify(await tipNow())); }
  await page.screenshot({ path: `shots/${prefix}-2-zoomed-out.png` });
  // The pointing box after a click on the map: click an empty friendly province far from the army, then read the box.
  const other = await page.evaluate(([nat, id]) => { const g = window.theater.game(); const els = [...document.querySelectorAll('#map [data-id], #map [data-reg], #map [data-region]')]; const out = []; for (const e of els) { const rid = Number(e.dataset.id ?? e.dataset.reg ?? e.dataset.region); const reg = g.regions[rid]; if (!reg || reg.ctrl !== nat || rid === id) continue; const b = e.getBoundingClientRect(); const x = b.x + b.width / 2, y = b.y + b.height / 2; const t = document.elementFromPoint(x, y); if (!t || !t.closest('#map')) continue; out.push({ id: rid, name: reg.name, corps: reg.units.length, x: Math.round(x), y: Math.round(y) }); } const seen = new Set(); return out.filter((o) => (seen.has(o.id) ? false : (seen.add(o.id), true))).slice(0, 12); }, [nat, pick.id]);
  console.log('own provinces in view to click: ' + JSON.stringify(other.map((o) => `${o.name}(${o.corps})`)));
  const before = clean(await page.evaluate(() => document.querySelector('.railsay')?.closest('section, .card, .pane, div')?.innerText || '')).slice(0, 160);
  console.log('the army card before the click: ' + before);
  for (const o of other.filter((o) => o.corps > 0).slice(0, 2).concat(other.filter((o) => o.corps === 0).slice(0, 1))) {
    await page.mouse.move(o.x, o.y); await page.waitForTimeout(350); const hov = await tipNow();
    await page.mouse.click(o.x, o.y); await page.waitForTimeout(700); const aft = await tipNow();
    const hand = await page.evaluate(() => { const u = window.theater.ui || {}; const g = window.theater.game(); const sel = u.sel ?? u.selected ?? u.region ?? null; return { sel, name: typeof sel === 'number' && g.regions[sel] ? g.regions[sel].name : null, say: [...document.querySelectorAll('.railsay')].map((e) => (e.innerText || '').replace(/\s+/g, ' ').trim()) }; });
    console.log(`  clicked ${o.name} (${o.corps} corps) at ${o.x},${o.y}: the box before the click ${JSON.stringify(hov)}; after ${JSON.stringify(aft)}; in hand now ${JSON.stringify(hand)}`);
    console.log(`    names ${pick.name} after the click: ${aft.some((t) => t.includes(pick.name)) ? 'YES' : 'no'}`);
  }
  await page.screenshot({ path: `shots/${prefix}-3-after-click.png` });
  console.log('page errors: ' + JSON.stringify(errors));
  await browser.close();
})();
