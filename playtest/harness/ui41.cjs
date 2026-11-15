// Version 74 on the page: the wider chart and the new seats (the United States, Canada, Mexico) as a player meets them.
//   start NAT W H PREFIX [MONTHS]   the new-campaign card (who can be picked, what the card says of NAT), the campaign begun
//                                   from the card, where the map opens, the whole chart zoomed out, what Declare war offers,
//                                   then End month MONTHS times (default 3) with the cards that open and how long a month takes
// usage: [TC_PAGE=file:///home/user/tc74/dist/play.html] node ui41.cjs start USA 1440 900 99-v74-usa 4   (shots go to shots/PREFIX-*.png)
const { chromium } = require('playwright');
const [mode, ...rest] = process.argv.slice(2);
const PAGE = process.env.TC_PAGE || 'file:///home/user/tc74/dist/play.html';
const flat = (s, n = 600) => String(s ?? '').replace(/\s+/g, ' ').trim().slice(0, n);
(async () => {
  const browser = await chromium.launch();
  const errors = [];
  if (mode === 'start') {
    const [nat, W, H, prefix, monthsS = '3'] = rest; const w = +W, h = +H; const phone = Math.min(w, h) < 500;
    const ctx = await browser.newContext({ viewport: { width: w, height: h }, deviceScaleFactor: 2, isMobile: phone, hasTouch: phone });
    const page = await ctx.newPage();
    page.on('pageerror', (e) => errors.push(`${e.message} @ ${(e.stack || '').split('\n').slice(1, 3).join(' | ').slice(0, 200)}`));
    page.on('console', (m) => { if (m.type() === 'error') errors.push(`console: ${m.text().slice(0, 300)}`); });
    await page.addInitScript(() => { try { if (!sessionStorage.getItem('tc-once')) { sessionStorage.setItem('tc-once', '1'); localStorage.clear(); localStorage.setItem('tc-watch', '0'); } } catch {} });
    await page.goto(PAGE); await page.waitForTimeout(1200);
    const say = (k, v) => console.log(`${k}: ${typeof v === 'string' ? v : JSON.stringify(v)}`);
    const veil = () => page.evaluate(() => { const v = document.querySelector('#veil'); return !!v && v.classList.contains('open'); });
    const cardText = (n = 400) => page.evaluate((n) => (document.querySelector('#card')?.innerText || '').replace(/\s+/g, ' ').trim().slice(0, n), n);
    if (!(await page.locator('.gcnat').count())) { const b = page.locator('[data-act="grand"], [data-act="newgrand"], [data-act="new"]').first(); if (await b.count()) { await b.click(); await page.waitForTimeout(500); } }
    say('new-campaign card, the great powers', await page.evaluate(() => [...document.querySelectorAll('#card .gcnats:not(.small) .gcnat')].map((b) => `${b.dataset.nat || 'watch'}:${b.textContent.replace(/\s+/g, ' ').trim()}`)));
    say('new-campaign card, the smaller nations', await page.evaluate(() => [...document.querySelectorAll('#card .gcnats.small .gcnat')].map((b) => `${b.dataset.nat}:${b.textContent.replace(/\s+/g, ' ').trim()}`)));
    say('card fits', await page.evaluate(() => { const c = document.querySelector('#card'); const r = c.getBoundingClientRect(); return { card: [Math.round(r.width), Math.round(r.height)], scrolls: `${c.clientHeight} of ${c.scrollHeight}`, sideways: c.scrollWidth > c.clientWidth + 1 || document.documentElement.scrollWidth > innerWidth + 1, tilesCut: [...c.querySelectorAll('.gcnat')].filter((b) => { const q = b.getBoundingClientRect(); return q.right > r.right + 1 || q.left < r.left - 1 || b.scrollWidth > b.clientWidth + 1; }).map((b) => b.dataset.nat) }; }));
    await page.screenshot({ path: `shots/${prefix}-1-card.png` });
    for (const n of [...new Set([nat, 'USA', 'CAN', 'MEX'])].reverse()) {
      const t = page.locator(`.gcnat[data-nat="${n}"]`).first();
      if (!(await t.count())) { say(`no tile for ${n}`, true); continue; }
      await t.click(); await page.waitForTimeout(250);
      say(`the card with ${n} picked`, await page.evaluate(() => { const c = document.querySelector('#card'); const txt = (e) => (e ? e.textContent.replace(/\s+/g, ' ').trim() : ''); return { years: [...c.querySelectorAll('.gcyear')].map((b) => `${b.dataset.year}${b.disabled ? ' (off)' : ''}${b.classList.contains('on') ? ' [on]' : ''}`), blurb: txt(c.querySelector('.gcblurb, .gcabout, .gctext, .gcyeartext') ?? null).slice(0, 500), start: txt(c.querySelector('[data-act="start"]')), all: txt(c).slice(0, 0) }; }));
    }
    if (process.env.TC_YEAR) { const y = page.locator(`.gcyear[data-year="${process.env.TC_YEAR}"]`).first(); if (await y.count()) { await y.click(); await page.waitForTimeout(250); say('year picked', process.env.TC_YEAR); say('what the card says of the year and the seat', await page.evaluate(() => [...document.querySelectorAll('#card p')].map((p) => p.textContent.replace(/\s+/g, ' ').trim()).filter((t) => t.length > 40).slice(-3).join(' / ').slice(0, 900))); } else say('no such year on the card', process.env.TC_YEAR); }
    say('the card text with the seat picked', await cardText(1800));
    await page.screenshot({ path: `shots/${prefix}-2-card-${nat}.png` });
    const t0 = Date.now();
    await page.click('#card [data-act="start"]'); await page.waitForTimeout(2500);
    say('begun', await page.evaluate(() => { const g = window.theater.game(); return { human: g.human, turn: g.turn, provinces: g.regions.length, seas: (g.seas || []).length, nations: Object.keys(g.nations).length, wide: !!g.flags.world || g.regions.length > 400, flags: Object.keys(g.flags).filter((k) => /world|wide|open/.test(k)) }; }));
    say('seconds from the button to the map', ((Date.now() - t0) / 1000).toFixed(1));
    say('the player at the start', await page.evaluate(() => { const g = window.theater.game(); const n = g.nations[g.human]; const corps = g.corps.filter((c) => c.nat === g.human); const where = {}; for (const c of corps) { const r = g.regions[c.reg]; where[r.name] = (where[r.name] || 0) + 1; } return { turn: g.turn, atWar: n.atWar, corps: corps.length, divisions: corps.reduce((a, c) => a + (c.divs?.length ?? c.divisions?.length ?? 0), 0), where, fleets: g.fleets.filter((f) => f.nat === g.human).length }; }));
    const cards = [];
    for (let k = 0; k < 12 && await veil(); k++) { cards.push(await cardText(500)); await page.screenshot({ path: `shots/${prefix}-3-opening-card-${k + 1}.png` }); await page.evaluate(() => window.theater.openModal(null)); await page.waitForTimeout(300); }
    say('cards at the start', cards);
    await page.waitForTimeout(500);
    await page.screenshot({ path: `shots/${prefix}-4-map-at-start.png` });
    say('where the map opens', await page.evaluate(() => { const g = window.theater.game(); const svg = document.querySelector('#map svg, svg#map, #map'); const vb = svg?.getAttribute?.('viewBox') ?? null; const cap = g.regions[g.nations[g.human].capital]; const lab = [...document.querySelectorAll('#map text, svg text')].filter((t) => { const r = t.getBoundingClientRect(); return r.width > 0 && r.left >= 0 && r.right <= innerWidth && r.top >= 0 && r.bottom <= innerHeight; }).map((t) => t.textContent.trim()).filter((s) => s.length > 3 && !/^\d/.test(s)); return { viewBox: vb, capital: cap?.name, capitalNamedInView: lab.includes(cap?.name) || lab.some((s) => s.toUpperCase() === String(cap?.name).toUpperCase()), someLabelsInView: [...new Set(lab)].slice(0, 24) }; }));
    say('tabs on the bar', await page.evaluate(() => [...document.querySelectorAll('#tabs [data-act="tab"]')].filter((b) => b.offsetParent !== null).map((b) => b.textContent.replace(/\s+/g, ' ').trim())));
    say('top bar', await page.evaluate(() => [...document.querySelectorAll('header button, #top button, .topbar button')].filter((b) => b.offsetParent !== null).map((b) => b.textContent.replace(/\s+/g, ' ').trim()).slice(0, 14)));
    // Zoom out to the whole chart.
    const box = await page.evaluate(() => { const m = document.querySelector('#map'); const r = m.getBoundingClientRect(); return { x: r.left + r.width / 2, y: r.top + r.height / 2, w: r.width, h: r.height }; });
    await page.mouse.move(box.x, box.y);
    for (let k = 0; k < 14; k++) { await page.mouse.wheel(0, 400); await page.waitForTimeout(60); }
    await page.waitForTimeout(600);
    await page.screenshot({ path: `shots/${prefix}-5-map-zoomed-out.png` });
    say('zoomed out', await page.evaluate(() => { const svg = document.querySelector('#map svg, svg#map'); const lab = [...document.querySelectorAll('#map text, svg text')].filter((t) => { const r = t.getBoundingClientRect(); return r.width > 0 && r.left >= 0 && r.right <= innerWidth && r.top >= 0 && r.bottom <= innerHeight; }).map((t) => t.textContent.trim()).filter((s) => s.length > 3 && !/^\d/.test(s)); return { viewBox: svg?.getAttribute?.('viewBox') ?? null, labels: [...new Set(lab)].slice(0, 40) }; }));
    // What Declare war offers.
    const dw = page.locator('[data-act="declare"], [data-act="war"], button:has-text("Declare war")').first();
    if (await dw.count()) { await dw.click(); await page.waitForTimeout(500); say('Declare war offers', await cardText(1500)); await page.screenshot({ path: `shots/${prefix}-6-declare-war.png` }); await page.evaluate(() => window.theater.openModal(null)); await page.waitForTimeout(200); } else say('no Declare war button', true);
    // The Powers tab: how the player's country is described.
    await page.evaluate(() => document.querySelector('[data-act="tab"][data-tab="powers"]')?.click()); await page.waitForTimeout(500);
    say('powers tab', await page.evaluate(() => (document.querySelector('#pane')?.innerText || '').replace(/\s+/g, ' ').trim().slice(0, 1500)));
    await page.screenshot({ path: `shots/${prefix}-7-powers.png` });
    await page.evaluate(() => document.querySelector('[data-act="tab"][data-tab="build"]')?.click()); await page.waitForTimeout(500);
    say('build tab', await page.evaluate(() => (document.querySelector('#pane')?.innerText || '').replace(/\s+/g, ' ').trim().slice(0, 1200)));
    await page.evaluate(() => document.querySelector('[data-act="tab"][data-tab="staff"]')?.click()); await page.waitForTimeout(500);
    say('staff tab', await page.evaluate(() => (document.querySelector('#pane')?.innerText || '').replace(/\s+/g, ' ').trim().slice(0, 1500)));
    await page.screenshot({ path: `shots/${prefix}-8-staff.png` });
    for (let m = 0; m < +monthsS; m++) {
      const turn = await page.evaluate(() => window.theater.game().turn);
      const t1 = Date.now();
      await page.evaluate(() => document.querySelector('[data-act="end"]')?.click());
      // A confirmation may stand between the button and the month.
      await page.waitForTimeout(400);
      if (await veil()) { const c = await cardText(300); const go = page.locator('#card [data-act="endgo"], #card [data-act="endsure"], #card [data-act="end"], #card footer .btn.primary').first(); if (await go.count()) { say(`  a card before the month ends (${turn})`, c); await go.click(); } }
      try { await page.waitForFunction((t0) => window.theater.game().turn > t0, turn, { timeout: 240000 }); } catch { say('  the date did not move', turn); }
      const secs = ((Date.now() - t1) / 1000).toFixed(1);
      await page.waitForTimeout(1200);
      const got = [];
      for (let k = 0; k < 12 && await veil(); k++) { got.push(await cardText(700)); if (k === 0) await page.screenshot({ path: `shots/${prefix}-9-month-${m + 1}.png` }); await page.evaluate(() => window.theater.openModal(null)); await page.waitForTimeout(250); }
      say(`month ${m + 1} (turn ${turn} ended in ${secs} s), cards`, got);
    }
    say('page errors', errors.length ? errors.slice(0, 6) : 'none');
  }
  await browser.close();
})();
