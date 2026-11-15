// Version 74 on the page: an American player's first job in 1942, shipping a corps across the ocean.
//   ship NAT YEAR W H PREFIX   begin a campaign as NAT in YEAR, pick up the corps in the port where most of the army
//                              stands, press "Ship by sea", read what the page says and which ports it marks, zoom out
//                              to see them, ship to a British port, then read where the corps is and what it can do next.
// usage: node ui43.cjs ship USA 1942 1440 900 99-v74-ship
const { chromium } = require('playwright');
const [mode, nat = 'USA', year = '1942', W = '1440', H = '900', prefix = '99-v74-ship'] = process.argv.slice(2);
const PAGE = process.env.TC_PAGE || 'file:///home/user/tc74/dist/play.html';
(async () => {
  const browser = await chromium.launch(); const errors = [];
  const ctx = await browser.newContext({ viewport: { width: +W, height: +H }, deviceScaleFactor: 2 });
  const page = await ctx.newPage();
  page.on('pageerror', (e) => errors.push(`${e.message} @ ${(e.stack || '').split('\n').slice(1, 3).join(' | ').slice(0, 200)}`));
  await page.addInitScript(() => { try { if (!sessionStorage.getItem('tc-once')) { sessionStorage.setItem('tc-once', '1'); localStorage.clear(); localStorage.setItem('tc-watch', '0'); } } catch {} });
  await page.goto(PAGE); await page.waitForTimeout(1200);
  const say = (k, v) => console.log(`${k}: ${typeof v === 'string' ? v : JSON.stringify(v)}`);
  const veil = () => page.evaluate(() => { const v = document.querySelector('#veil'); return !!v && v.classList.contains('open'); });
  const msg = () => page.evaluate(() => [...document.querySelectorAll('.msg, #msg, .hintbar, #hint, .banner')].map((e) => e.textContent.replace(/\s+/g, ' ').trim()).filter(Boolean).join(' / ').slice(0, 400));
  if (!(await page.locator('.gcnat').count())) { const b = page.locator('[data-act="grand"], [data-act="newgrand"], [data-act="new"]').first(); if (await b.count()) { await b.click(); await page.waitForTimeout(500); } }
  await page.locator(`.gcnat[data-nat="${nat}"]`).first().click(); await page.waitForTimeout(200);
  const y = page.locator(`.gcyear[data-year="${year}"]`).first(); if (await y.count()) { await y.click(); await page.waitForTimeout(200); }
  await page.click('#card [data-act="start"]'); await page.waitForTimeout(3000);
  for (let k = 0; k < 12 && await veil(); k++) { await page.evaluate(() => window.theater.openModal(null)); await page.waitForTimeout(300); }
  const st = await page.evaluate(() => { const g = window.theater.game(); const me = g.human; const by = {}; for (const c of g.corps) if (c.nat === me && g.regions[c.reg].port) by[c.reg] = (by[c.reg] || 0) + 1; const quay = +Object.entries(by).sort((a, b) => b[1] - a[1])[0][0]; return { quay, name: g.regions[quay].name, corps: by[quay], turn: g.turn, lift: g.nations[me].sealift, atWar: g.nations[me].atWar }; });
  say('the quay', st);
  await page.evaluate((q) => window.theater.onRegion(q), st.quay); await page.waitForTimeout(600);
  say('the buttons under the force', await page.evaluate(() => [...document.querySelectorAll('.acts button')].filter((b) => b.offsetParent !== null).map((b) => `${b.textContent.trim()}${b.disabled ? ' (off)' : ''}${b.title ? ` [${b.title}]` : ''}`)));
  await page.screenshot({ path: `shots/${prefix}-1-quay.png` });
  const ship = page.locator('[data-act="ship"]').first();
  if (!(await ship.count())) { say('no "Ship by sea" button', true); }
  else {
    await ship.click(); await page.waitForTimeout(600);
    say('the page says in ship mode', await msg());
    const marks = await page.evaluate(() => { const g = window.theater.game(); const ui = window.theater.ui; return { ship: !!ui.ship, marked: document.querySelectorAll('#map .ship, #map .can-ship, #map .target, #map .reach, #map [class*="ship"]').length, classes: [...new Set([...document.querySelectorAll('#map [class]')].map((e) => e.getAttribute('class')).filter((c) => /ship|target|reach|can|dest/.test(c)))].slice(0, 12) }; });
    say('marks on the map in ship mode', marks);
    await page.screenshot({ path: `shots/${prefix}-2-ship-mode.png` });
    const box = await page.evaluate(() => { const m = document.querySelector('#map'); const r = m.getBoundingClientRect(); return { x: r.left + r.width / 2, y: r.top + r.height / 2 }; });
    await page.mouse.move(box.x, box.y);
    for (let k = 0; k < 14; k++) { await page.mouse.wheel(0, 400); await page.waitForTimeout(60); }
    await page.waitForTimeout(600);
    await page.screenshot({ path: `shots/${prefix}-3-ship-mode-whole-chart.png` });
    // Ship to a port in Britain: the first British home port the page will take.
    const went = await page.evaluate(() => { const g = window.theater.game(); const before = g.corps.filter((c) => c.nat === g.human).map((c) => c.reg).join(','); const tried = []; for (const r of g.regions) { if (!r.port || r.owner !== 'UK' || r.ctrl !== 'UK' || r.overseas) continue; tried.push(r.name); window.theater.onRegion(r.id); const after = g.corps.filter((c) => c.nat === g.human).map((c) => c.reg).join(','); if (after !== before) return { to: r.name, tried }; if (tried.length > 5) break; } return { to: null, tried }; });
    say('shipped', went);
    await page.waitForTimeout(700);
    say('the page says after', await msg());
    if (await veil()) { say('a card opened', await page.evaluate(() => (document.querySelector('#card')?.innerText || '').replace(/\s+/g, ' ').trim().slice(0, 600))); await page.screenshot({ path: `shots/${prefix}-4-card.png` }); await page.evaluate(() => window.theater.openModal(null)); }
    say('the army now', await page.evaluate(() => { const g = window.theater.game(); const by = {}; for (const c of g.corps) if (c.nat === g.human) { const r = g.regions[c.reg]; by[`${r.name} (${r.owner})`] = (by[`${r.name} (${r.owner})`] || 0) + 1; } return { by, lift: g.nations[g.human].sealift }; }));
    await page.screenshot({ path: `shots/${prefix}-5-after.png` });
  }
  say('page errors', errors.length ? errors.slice(0, 5) : 'none');
  await browser.close();
})();
