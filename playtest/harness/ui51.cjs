// What a player is shown in the month his country falls (Version 82.1 on). Poland by default, a fixed seed.
// usage: [TC_PAGE=...] node ui51.cjs SEED [MONTHS=3] [PREFIX] [NAT=POL]
// Each month: END MONTH, the question answered, then every change of (turn, player, the card open) is printed.
const { chromium } = require('playwright');
const PAGE = process.env.TC_PAGE || 'file:///home/user/tc821/dist/play.html';
const [seedS = '7', monthsS = '3', prefix = '101-fall', nat = 'POL'] = process.argv.slice(2);
(async () => {
  const browser = await chromium.launch(); const errors = [];
  const page = await (await browser.newContext({ viewport: { width: 1440, height: 900 }, deviceScaleFactor: 2 })).newPage();
  page.on('pageerror', (e) => errors.push(e.message.slice(0, 300)));
  await page.goto(PAGE); await page.evaluate(() => { localStorage.clear(); localStorage.setItem('tc-watch', '0'); }); await page.reload(); await page.waitForSelector('#card');
  await page.evaluate(([nat, seed]) => window.theater.newGame(nat, seed, 1, 'grand', 'colonel', true, false, 'theatre', 3), [nat, Number(seedS)]);
  await page.waitForTimeout(2500);
  const st = () => page.evaluate((nat) => { const g = window.theater.game(); const v = document.querySelector('#veil'); const open = !!v && v.classList.contains('open'); const own = g.regions.filter((r) => r.owner === nat && !r.overseas); return `turn ${g.turn} player ${g.human} ${nat} ${g.nations[nat].alive ? 'alive' : 'OUT'} holds ${own.filter((r) => r.ctrl === nat).length}/${own.length} | card ${open ? 'OPEN: ' + (document.querySelector('#card')?.innerText || '').replace(/\s+/g, ' ').slice(0, 420) : 'shut'} | header: ${(document.querySelector('header')?.innerText || document.querySelector('#top')?.innerText || '').replace(/\s+/g, ' ').slice(0, 150)}`; }, nat);
  const poll = async (label, ms) => { let last = ''; const t0 = Date.now(); while (Date.now() - t0 < ms) { const s = await st(); if (s !== last) { console.log(`${label} +${Math.round((Date.now() - t0) / 100) / 10}s: ${s}`); last = s; } await page.waitForTimeout(300); } };
  const answer = () => page.evaluate(() => { const v = document.querySelector('#veil'); if (!v || !v.classList.contains('open')) return null; const t = document.querySelector('#card').innerText; if (!/BEFORE THE MONTH ENDS/i.test(t)) return null; const bs = [...document.querySelectorAll('#card button')]; const b = bs.find((x) => /end the month/i.test(x.innerText || '')); b?.click(); return 'question answered'; });
  if (await page.evaluate(() => /DEPLOYMENT/i.test(document.querySelector('[data-act="end"]')?.innerText || ''))) {
    await page.evaluate(() => document.querySelector('[data-act="eveplan"]')?.click()); await page.waitForTimeout(400);
    await page.evaluate(() => document.querySelector('[data-act="end"]')?.click()); await page.waitForTimeout(4000);
  }
  for (let m = 1; m <= Number(monthsS); m++) {
    await page.evaluate(() => window.theater.openModal(null)); await page.waitForTimeout(300);
    const top = await page.evaluate(() => (document.querySelector('[data-act="end"]')?.innerText || '(no END button)').replace(/\s+/g, ' '));
    if (!/END MONTH/i.test(top)) { console.log(`month ${m}: the top button reads "${top}": stopping`); break; }
    await page.evaluate(() => document.querySelector('[data-act="end"]')?.click()); await page.waitForTimeout(800);
    await answer();
    await poll(`month ${m}`, 9000);
    console.log(`month ${m}: buttons on the open card: ${JSON.stringify(await page.evaluate(() => [...document.querySelectorAll('#veil.open #card button')].map((b) => (b.innerText || '').replace(/\s+/g, ' ').slice(0, 40)).filter(Boolean)))}`);
    await page.screenshot({ path: `shots/${prefix}-s${seedS}-m${m}.png` });
    if (!(await page.evaluate((nat) => window.theater.game().nations[nat].alive, nat))) { console.log('news: ' + JSON.stringify(await page.evaluate((nat) => { const g = window.theater.game(); return g.headlines.filter((h) => h.turn >= g.turn - 1 && new RegExp(g.nations[nat].name).test(h.text)).slice(-5).map((h) => `t${h.turn} [${h.kind}] ${h.text.slice(0, 220)}`); }, nat))); break; }
  }
  console.log('page errors: ' + JSON.stringify(errors));
  await browser.close();
})();
