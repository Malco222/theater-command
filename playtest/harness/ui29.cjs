// Version 66: what a player who hands desks over has to read. For each depth, a passive German four months in:
// the unread counts on the tabs, the News tab's lines by kind (the staff's own lines against the rest), the Battles
// tab's reports by kind, where a Supreme command player finds the way to hand a desk over, and the badge on a phone.
// usage: [TC_PAGE=...] ui29.cjs SHOT-PREFIX [SEED] [NAT] [MONTHS]
const { chromium } = require('playwright');
const [shot, seedArg, natArg, monthsArg] = process.argv.slice(2);
const seed = Number(seedArg || 7), nat = natArg || 'GER', months = Number(monthsArg || 4);
const flat = (s, n = 1500) => String(s).replace(/\n+/g, ' / ').replace(/\s+/g, ' ').slice(0, n);
const SIZES = [['desk', { width: 1440, height: 900 }, false], ['phone', { width: 390, height: 844 }, true]];
(async () => {
  const browser = await chromium.launch();
  for (const [name, vp, touch] of SIZES) {
    for (const depth of ['field', 'theatre', 'supreme']) {
      const say = (...a) => console.log(`[${name} ${depth}]`, ...a);
      const errors = [];
      const page = await browser.newPage({ viewport: vp, deviceScaleFactor: 2, isMobile: touch, hasTouch: touch });
      page.on('pageerror', (e) => errors.push(e.message));
      await page.addInitScript(() => { try { localStorage.clear(); localStorage.setItem('tc-watch', '0'); } catch {} });
      await page.goto(process.env.TC_PAGE || 'file:///home/user/tc66/dist/play.html'); await page.waitForTimeout(900);
      await page.evaluate(([nat, seed, depth]) => window.theater.newGame(nat, seed, 1, 'grand', 'colonel', true, false, depth), [nat, seed, depth]);
      await page.waitForTimeout(1200);
      const shut = async () => { for (let k = 0; k < 8; k++) { const open = await page.evaluate(() => { const v = document.querySelector('#veil'); if (!v || !v.classList.contains('open')) return false; window.theater.openModal(null); return true; }); if (!open) break; await page.waitForTimeout(150); } };
      await shut();
      for (let i = 0; i < months; i++) { await page.evaluate(() => window.theater.nextMonth()); await page.waitForTimeout(2500); await shut(); }
      const tabs = await page.locator('#tabs [data-act="tab"]').evaluateAll((bs) => bs.filter((b) => b.offsetParent !== null).map((b) => b.textContent.trim().replace(/\s+/g, ' ')));
      say('tabs:', JSON.stringify(tabs), '| badge:', flat(await page.locator('.rank').first().innerText().catch(() => 'none'), 80), '| badge size', JSON.stringify(await page.locator('.rank').first().evaluate((b) => { const q = b.getBoundingClientRect(); return `${Math.round(q.width)}x${Math.round(q.height)}`; }).catch(() => '')));
      const news = await page.evaluate(() => { const g = window.theater.game(); const by = {}; let desk = 0; const deskBy = {}; for (const h of g.headlines) { by[h.kind] = (by[h.kind] || 0) + 1; if (h.desk) { desk++; deskBy[h.desk] = (deskBy[h.desk] || 0) + 1; } } return { all: g.headlines.length, byKind: by, staffLines: desk, byDesk: deskBy, battles: (g.reports || g.battles || []).length }; });
      say('headlines so far:', JSON.stringify(news));
      await page.locator('#tabs [data-act="tab"][data-tab="reel"]').first().click(); await page.waitForTimeout(400);
      const reel = await page.evaluate(() => { const ps = [...document.querySelectorAll('#pane .reel p')]; const pane = document.querySelector('#pane'); const r = pane.getBoundingClientRect(); return { lines: ps.length, staff: ps.filter((p) => p.classList.contains('desk')).length, long: pane.scrollHeight, shown: Math.round(Math.min(r.bottom, innerHeight) - r.top), first: ps.slice(0, 14).map((p) => (p.classList.contains('desk') ? 'STAFF ' : '') + p.textContent.trim().replace(/\s+/g, ' ').slice(0, 150)), links: [...document.querySelectorAll('#pane .reel p.desk [data-act]')].slice(0, 3).map((b) => `${b.dataset.act}:${b.textContent.trim()} ${Math.round(b.getBoundingClientRect().width)}x${Math.round(b.getBoundingClientRect().height)}`) }; });
      say('News tab:', JSON.stringify(reel));
      await page.screenshot({ path: `${shot}-${name}-${depth}-8-news.png` });
      await page.locator('#tabs [data-act="tab"][data-tab="battles"]').first().click(); await page.waitForTimeout(400);
      say('Battles tab:', flat(await page.locator('#pane').innerText(), 900));
      await page.screenshot({ path: `${shot}-${name}-${depth}-9-battles.png` });
      if (depth === 'supreme') {
        await page.locator('#tabs [data-act="tab"][data-tab="research"]').first().click(); await page.waitForTimeout(400);
        const foot = await page.evaluate(() => { const f = document.querySelector('#pane .deskfoot'); const pane = document.querySelector('#pane'); if (!f) return null; const q = f.getBoundingClientRect(); return { text: f.innerText.trim(), fromTop: Math.round(q.top - pane.getBoundingClientRect().top + pane.scrollTop), paneLong: pane.scrollHeight, paneShown: Math.round(Math.min(pane.getBoundingClientRect().bottom, innerHeight) - pane.getBoundingClientRect().top) }; });
        say('Research panel, the way to hand it over:', JSON.stringify(foot));
        await page.locator('.rank').first().click(); await page.waitForTimeout(500);
        const card = await page.evaluate(() => { const c = document.querySelector('#card'); const y = (sel) => { const e = c.querySelector(sel); return e ? Math.round(e.getBoundingClientRect().top - c.getBoundingClientRect().top + c.scrollTop) : null; }; return { long: c.scrollHeight, shown: c.clientHeight, whoRunsWhatAt: y('[data-act="desk"]'), heads: [...c.querySelectorAll('h2, h3')].map((h) => h.textContent.trim()) }; });
        say('settings card:', JSON.stringify(card));
        await page.screenshot({ path: `${shot}-${name}-${depth}-5-settings.png` });
      }
      say('page errors:', errors.length ? JSON.stringify(errors.slice(0, 4)) : 'none');
      await page.close();
    }
  }
  await browser.close();
})();
