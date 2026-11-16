// Version 82.3 on the page: a cut railway on enemy ground known only from an old report wears a hollow lamp.
// A fixture, not play: the player's memory of cuts (g.intel.cuts, which no rule reads) is given one entry for an enemy
// province he cannot see, and the map is drawn again. usage: [TC_PAGE=...] node ui56.cjs [NAT=GER] [SEED=7] [PREFIX]
const { chromium } = require('playwright');
const PAGE = process.env.TC_PAGE || 'file:///home/user/tc823/dist/play.html';
const [nat = 'GER', seedS = '7', prefix = '105-v823-lamp'] = process.argv.slice(2);
(async () => {
  const browser = await chromium.launch(); const errors = [];
  const page = await (await browser.newContext({ viewport: { width: 1440, height: 900 }, deviceScaleFactor: 2 })).newPage();
  page.on('pageerror', (e) => errors.push(e.message.slice(0, 300)));
  await page.goto(PAGE); await page.evaluate(() => { localStorage.clear(); localStorage.setItem('tc-watch', '0'); }); await page.reload(); await page.waitForSelector('#card');
  await page.evaluate(([nat, seed]) => window.theater.newGame(nat, seed, 1, 'grand', 'colonel', true, false, 'theatre', 3), [nat, Number(seedS)]);
  await page.waitForTimeout(2500); await page.evaluate(() => window.theater.openModal(null)); await page.waitForTimeout(300);
  const count = () => page.evaluate(() => ({ lamps: document.querySelectorAll('#map .raillamp').length, hollow: document.querySelectorAll('#map .raillamp.old').length, bars: document.querySelectorAll('#map .railstop').length, oldBars: document.querySelectorAll('#map .railstop.old').length, oldLine: document.querySelectorAll('#map .railcut.old').length }));
  console.log('before: ' + JSON.stringify(await count()));
  // Enemy provinces two or more steps from anything the player holds: one of them out of his sight will take the entry.
  const set = await page.evaluate((nat) => { const g = window.theater.game(); const mine = (r) => r.ctrl === nat; const out = []; for (const r of g.regions) { if (!g.nations[nat].atWar.includes(r.ctrl) || r.overseas) continue; if (r.nb.some((n) => mine(g.regions[n]) || g.regions[n].nb.some((m) => mine(g.regions[m])))) continue; out.push(r); } g.intel.cuts = g.intel.cuts || {}; const named = []; for (const r of out.slice(0, 40)) { g.intel.cuts[r.id] = { t: g.turn, how: 'seen', why: 'bombed' }; named.push(`${r.name}(${r.ctrl})`); } return named; }, nat);
  console.log(`entries given for ${set.length} enemy provinces in the rear: ${set.slice(0, 8).join(', ')}…`);
  await page.evaluate(() => window.theater.refresh()); await page.waitForTimeout(700);
  const after = await count(); console.log('after: ' + JSON.stringify(after));
  const lamp = await page.evaluate(() => { const out = []; for (const e of document.querySelectorAll('#map .raillamp.old')) { const b = e.getBoundingClientRect(); const x = b.x + b.width / 2, y = b.y + b.height / 2; const t = document.elementFromPoint(x, y); out.push({ x: Math.round(x), y: Math.round(y), free: !!(t && t.closest('#map')), fill: getComputedStyle(e.querySelector('circle')).fill, dash: getComputedStyle(e.querySelector('circle')).strokeDasharray }); } return out; });
  console.log('hollow lamps: ' + JSON.stringify(lamp.slice(0, 5)));
  const p = lamp.find((l) => l.free && l.x > 20 && l.y > 130 && l.x < 1000 && l.y < 580);
  if (p) { await page.mouse.move(p.x + 9, p.y + 9); await page.waitForTimeout(500); console.log(`the pointer beside the lamp at ${p.x},${p.y}: ` + JSON.stringify(await page.evaluate(() => [...document.querySelectorAll('.tip')].filter((e) => getComputedStyle(e).display !== 'none' && e.getBoundingClientRect().width > 0).map((e) => (e.innerText || '').replace(/\s+/g, ' ').trim()).filter(Boolean))));
    await page.mouse.click(p.x + 9, p.y + 9); await page.waitForTimeout(600); console.log('the panel there: ' + JSON.stringify(await page.evaluate(() => [...document.querySelectorAll('#pane .rrow, #pane p, #pane div')].map((e) => (e.innerText || '').replace(/\s+/g, ' ').trim()).filter((t) => /railway|line was|report/i.test(t) && t.length < 260).slice(0, 4))));
    await page.screenshot({ path: `shots/${prefix}-1-hollow.png`, clip: { x: Math.max(0, p.x - 260), y: Math.max(110, p.y - 200), width: 520, height: 400 } }); }
  console.log('the legend: ' + JSON.stringify(await page.evaluate(() => { const e = document.querySelector('#legrailcut'); return e ? `${e.hidden ? '(hidden) ' : ''}${(e.innerText || '').replace(/\s+/g, ' ').trim()}` : null; })));
  console.log('page errors: ' + JSON.stringify(errors));
  await browser.close();
})();
