// Version 82.2 on the page: the two reaches of an army in hand (march green, rail blue), the pointer's "by rail" line,
// and what the army's panel says of the railway. usage: [TC_PAGE=...] node ui53.cjs [NAT=GER] [SEED=7] [PREFIX]
const { chromium } = require('playwright');
const PAGE = process.env.TC_PAGE || 'file:///home/user/tc822/dist/play.html';
const [nat = 'GER', seedS = '7', prefix = '104-rail'] = process.argv.slice(2);
(async () => {
  const browser = await chromium.launch(); const errors = [];
  const page = await (await browser.newContext({ viewport: { width: 1440, height: 900 }, deviceScaleFactor: 2 })).newPage();
  page.on('pageerror', (e) => errors.push(e.message.slice(0, 300)));
  await page.goto(PAGE); await page.evaluate(() => { localStorage.clear(); localStorage.setItem('tc-watch', '0'); }); await page.reload(); await page.waitForSelector('#card');
  await page.evaluate(([nat, seed]) => window.theater.newGame(nat, seed, 1, 'grand', 'colonel', true, false, 'theatre', 3), [nat, Number(seedS)]);
  await page.waitForTimeout(2500); await page.evaluate(() => window.theater.openModal(null)); await page.waitForTimeout(300);
  // An army in the rear with a railway: pick the rear region with most corps.
  const pick = await page.evaluate((nat) => { const g = window.theater.game(); const foe = (r) => r.nb.some((n) => { const o = g.regions[n]; return o.ctrl !== nat && g.nations[nat].atWar.includes(o.ctrl); }); const rear = g.regions.filter((r) => r.ctrl === nat && !r.overseas && !foe(r) && r.units.some((u) => u.nat === nat && u.kind !== 'fort')); rear.sort((a, b) => b.units.length - a.units.length); return rear[0] ? { id: rear[0].id, name: rear[0].name } : null; }, nat);
  console.log('the army in hand: ' + JSON.stringify(pick));
  await page.evaluate((id) => window.theater.onRegion(id), pick.id); await page.waitForTimeout(600);
  const marks = await page.evaluate(() => { const by = {}; for (const e of document.querySelectorAll('#map .mk')) { const k = e.getAttribute('class'); by[k] = (by[k] || 0) + 1; } return by; });
  console.log('marks on the map by class: ' + JSON.stringify(marks));
  console.log('the hint above the map: ' + JSON.stringify(await page.evaluate(() => [...document.querySelectorAll('.hint, #hint, .banner, #say')].map((e) => (e.innerText || '').replace(/\s+/g, ' ').trim()).filter(Boolean).slice(0, 3))));
  console.log('the railway on the panel: ' + JSON.stringify(await page.evaluate(() => [...document.querySelectorAll('#pane *')].filter((e) => !e.children.length || e.classList.contains('rrow')).map((e) => (e.innerText || '').replace(/\s+/g, ' ').trim()).filter((t) => /rail|train/i.test(t)).slice(0, 8))));
  // Zoom out to see both tints, then hover a region of the rail reach.
  for (let i = 0; i < 2; i++) { await page.evaluate(() => document.querySelector('[data-act="zout"]')?.click()); await page.waitForTimeout(300); }
  await page.screenshot({ path: `shots/${prefix}-1-reaches.png` });
  const far = await page.evaluate(() => { const svg = document.querySelector('#map'); const els = [...document.querySelectorAll('#map .mk.railfar')]; const out = []; for (const e of els.slice(0, 40)) { const b = e.getBoundingClientRect(); const x = b.x + b.width / 2, y = b.y + b.height / 2; if (x > 60 && y > 180 && x < 1380 && y < 840) out.push({ x, y, id: e.dataset.id || e.getAttribute('data-id') || '' }); } return out; });
  console.log(`rail-reach marks on screen: ${far.length}`);
  for (const p of far.slice(0, 3)) { await page.mouse.move(p.x, p.y); await page.waitForTimeout(500); console.log(`  the pointer at ${Math.round(p.x)},${Math.round(p.y)}: ` + JSON.stringify(await page.evaluate(() => [...document.querySelectorAll('.tip')].filter((e) => e.offsetParent !== null || getComputedStyle(e).display !== 'none').map((e) => (e.innerText || '').replace(/\s+/g, ' ').trim().slice(0, 330))))); }
  if (far.length) { await page.screenshot({ path: `shots/${prefix}-2-pointer.png` }); await page.mouse.click(far[0].x, far[0].y, { button: 'right' }); await page.waitForTimeout(700); console.log('after a right-click there: ' + JSON.stringify(await page.evaluate(() => [...document.querySelectorAll('#say, .say, .toast, #msg')].map((e) => (e.innerText || '').replace(/\s+/g, ' ').trim()).filter(Boolean).slice(0, 3)))); console.log('  news: ' + JSON.stringify(await page.evaluate(() => { const g = window.theater.game(); return (g.log || []).slice(-2); }))); await page.screenshot({ path: `shots/${prefix}-3-train.png` }); }
  console.log('page errors: ' + JSON.stringify(errors));
  await browser.close();
})();
