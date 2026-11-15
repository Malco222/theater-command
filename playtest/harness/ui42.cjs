// Version 74 on the page: a small country's "Your path" card from a new seat (Mexico), and what follows the choice.
//   path NAT W H PREFIX CHOICE [MONTHS]   begin a 1939 campaign as NAT from the card, open "Your path", read it, take
//                                         CHOICE (axis | allies | alone), read what the page says after, what Declare
//                                         war offers then, end MONTHS months (default 3), and say where the country stands
// usage: node ui42.cjs path MEX 1440 900 99-v74-mex-path allies 4     (shots go to shots/PREFIX-*.png)
const { chromium } = require('playwright');
const [mode, ...rest] = process.argv.slice(2);
const PAGE = process.env.TC_PAGE || 'file:///home/user/tc74/dist/play.html';
(async () => {
  const browser = await chromium.launch();
  const errors = [];
  if (mode === 'path') {
    const [nat, W, H, prefix, choice = 'allies', monthsS = '3'] = rest; const w = +W, h = +H; const phone = Math.min(w, h) < 500;
    const ctx = await browser.newContext({ viewport: { width: w, height: h }, deviceScaleFactor: 2, isMobile: phone, hasTouch: phone });
    const page = await ctx.newPage();
    page.on('pageerror', (e) => errors.push(`${e.message} @ ${(e.stack || '').split('\n').slice(1, 3).join(' | ').slice(0, 200)}`));
    page.on('console', (m) => { if (m.type() === 'error') errors.push(`console: ${m.text().slice(0, 300)}`); });
    await page.addInitScript(() => { try { if (!sessionStorage.getItem('tc-once')) { sessionStorage.setItem('tc-once', '1'); localStorage.clear(); localStorage.setItem('tc-watch', '0'); } } catch {} });
    await page.goto(PAGE); await page.waitForTimeout(1200);
    const say = (k, v) => console.log(`${k}: ${typeof v === 'string' ? v : JSON.stringify(v)}`);
    const veil = () => page.evaluate(() => { const v = document.querySelector('#veil'); return !!v && v.classList.contains('open'); });
    const cardText = (n = 400) => page.evaluate((n) => (document.querySelector('#card')?.innerText || '').replace(/\s+/g, ' ').trim().slice(0, n), n);
    const stand = () => page.evaluate(() => { const g = window.theater.game(); const n = g.nations[g.human]; const corps = g.corps.filter((c) => c.nat === g.human); const where = {}; for (const c of corps) { const r = g.regions[c.reg]; where[r.name] = (where[r.name] || 0) + 1; } return { turn: g.turn, side: n.bloc ?? n.side ?? n.align ?? null, lean: n.lean ?? n.leaning ?? null, atWar: n.atWar, corps: corps.length, where }; });
    if (!(await page.locator('.gcnat').count())) { const b = page.locator('[data-act="grand"], [data-act="newgrand"], [data-act="new"]').first(); if (await b.count()) { await b.click(); await page.waitForTimeout(500); } }
    await page.locator(`.gcnat[data-nat="${nat}"]`).first().click(); await page.waitForTimeout(250);
    say('the card says of the seat', await page.evaluate(() => [...document.querySelectorAll('#card p.you, #card .you')].map((p) => p.textContent.replace(/\s+/g, ' ').trim()).join(' / ').slice(0, 700)));
    await page.click('#card [data-act="start"]'); await page.waitForTimeout(2500);
    for (let k = 0; k < 12 && await veil(); k++) { say(`card at the start ${k + 1}`, await cardText(500)); await page.evaluate(() => window.theater.openModal(null)); await page.waitForTimeout(300); }
    say('at the start', await stand());
    const pb = page.locator('[data-act="path"]:not([data-ask])').first();
    say('"Your path" button on the page', await page.evaluate(() => [...document.querySelectorAll('[data-act="path"]')].filter((b) => b.offsetParent !== null).map((b) => `${b.textContent.replace(/\s+/g, ' ').trim()} (${b.title})`)));
    if (!(await pb.count())) { say('no "Your path" button', true); }
    else {
      await pb.click(); await page.waitForTimeout(500);
      say('the path card', await cardText(2400));
      say('the paths', await page.evaluate(() => [...document.querySelectorAll('#card .pathrow')].map((r) => ({ path: r.className.replace('pathrow', '').trim(), head: r.querySelector('.pathhead b')?.textContent, button: r.querySelector('button[data-ask]')?.textContent?.trim() ?? null, refused: r.querySelector('.pathno')?.textContent ?? null }))));
      await page.screenshot({ path: `shots/${prefix}-1-path-card.png` });
      const ask = page.locator(`#card [data-act="path"][data-ask="${choice}"]`).first();
      if (await ask.count()) {
        await ask.click(); await page.waitForTimeout(400);
        say('asked to confirm', await page.evaluate(() => (document.querySelector('#card .pathask')?.innerText || '').replace(/\s+/g, ' ').trim()));
        await page.screenshot({ path: `shots/${prefix}-2-path-confirm.png` });
        await page.locator(`#card [data-act="takepath"][data-path="${choice}"]`).first().click(); await page.waitForTimeout(700);
        say('the path card after the choice', await cardText(900));
        await page.screenshot({ path: `shots/${prefix}-3-path-taken.png` });
      } else say(`the path "${choice}" cannot be taken`, true);
      await page.evaluate(() => window.theater.openModal(null)); await page.waitForTimeout(300);
    }
    say('after the choice', await stand());
    const dw = page.locator('[data-act="declare"], [data-act="war"], button:has-text("Declare war")').first();
    if (await dw.count()) { await dw.click(); await page.waitForTimeout(500); say('Declare war offers', await cardText(1500)); say('Declare war, the countries', await page.evaluate(() => [...document.querySelectorAll('#card [data-nat], #card [data-on], #card .dwrow b, #card .natrow b')].map((b) => b.textContent.replace(/\s+/g, ' ').trim().slice(0, 40)).slice(0, 40))); await page.screenshot({ path: `shots/${prefix}-4-declare-war.png` }); await page.evaluate(() => window.theater.openModal(null)); await page.waitForTimeout(200); } else say('no Declare war button', true);
    await page.evaluate(() => document.querySelector('[data-act="tab"][data-tab="staff"]')?.click()); await page.waitForTimeout(500);
    say('staff tab, on courting', await page.evaluate(() => { const t = (document.querySelector('#pane')?.innerText || '').replace(/\s+/g, ' '); const i = t.search(/court/i); return i < 0 ? 'no word of courting' : t.slice(Math.max(0, i - 200), i + 300); }));
    for (let m = 0; m < +monthsS; m++) {
      const turn = await page.evaluate(() => window.theater.game().turn);
      const t1 = Date.now();
      await page.evaluate(() => document.querySelector('[data-act="end"]')?.click());
      await page.waitForTimeout(400);
      if (await veil()) { const c = await cardText(300); const go = page.locator('#card [data-act="endgo"], #card [data-act="endsure"], #card [data-act="end"], #card footer .btn.primary').first(); if (await go.count()) { say(`  a card before the month ends (${turn})`, c); await go.click(); } }
      try { await page.waitForFunction((t0) => window.theater.game().turn > t0, turn, { timeout: 240000 }); } catch { say('  the date did not move', turn); }
      const secs = ((Date.now() - t1) / 1000).toFixed(1);
      await page.waitForTimeout(1000);
      const got = [];
      for (let k = 0; k < 12 && await veil(); k++) { got.push(await cardText(260)); await page.evaluate(() => window.theater.openModal(null)); await page.waitForTimeout(250); }
      say(`month ${m + 1} (turn ${turn} ended in ${secs} s)`, { cards: got.length, first: got[0] ?? '', stand: await stand() });
    }
    await page.evaluate(() => document.querySelector('[data-act="tab"][data-tab="powers"]')?.click()); await page.waitForTimeout(500);
    say('powers tab, the player\'s country', await page.evaluate(() => { const g = window.theater.game(); const name = g.nations[g.human].name; const t = (document.querySelector('#pane')?.innerText || '').replace(/\s+/g, ' '); const i = t.indexOf(name + ' about'); return i < 0 ? `no row for ${name}` : t.slice(i, i + 260); }));
    await page.screenshot({ path: `shots/${prefix}-5-powers.png` });
    say('page errors', errors.length ? errors.slice(0, 6) : 'none');
  }
  await browser.close();
})();
