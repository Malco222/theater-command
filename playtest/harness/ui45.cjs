// Version 76 on the page: a Latin player's first hour (Brazil by default), played as a player would.
//   The start card (the brief, the goals, the note on small countries); the map as it opens; the path card and each
//   path's own page (Axis, Allies, the League and its willing partners); the Powers tab; the Build tab; then HOW:
//     look    nothing more, MONTHS months are played
//     allies  the Allies' path is taken on the card, then Declare war and the card for Germany, MONTHS months played
//     axis    the Axis path is taken, then Declare war and the list
//     league  the League is founded with the first willing partner
//   After the months: the news that named the country, the trade and build words, the page errors.
// usage: [TC_PAGE=file:///home/user/tc76/dist/play.html] node ui45.cjs NAT MONTHS PREFIX [look|allies|axis|league]
const { chromium } = require('playwright');
const [nat = 'BRA', monthsS = '4', prefix = '99-v76-latin', how = 'look'] = process.argv.slice(2);
const PAGE = process.env.TC_PAGE || 'file:///home/user/tc76/dist/play.html';
(async () => {
  const browser = await chromium.launch(); const errors = [];
  const ctx = await browser.newContext({ viewport: { width: 1440, height: 900 }, deviceScaleFactor: 2 });
  const page = await ctx.newPage();
  page.on('pageerror', (e) => errors.push(`${e.message} @ ${(e.stack || '').split('\n').slice(1, 3).join(' | ').slice(0, 200)}`));
  page.on('console', (m) => { if (m.type() === 'error') errors.push(`console: ${m.text().slice(0, 300)}`); });
  const say = (k, v) => console.log(`${k}: ${typeof v === 'string' ? v : JSON.stringify(v)}`);
  const veil = () => page.evaluate(() => { const v = document.querySelector('#veil'); return !!v && v.classList.contains('open'); });
  const shut = async () => { const seen = []; for (let i = 0; i < 12 && await veil(); i++) { seen.push(await page.evaluate(() => (document.querySelector('#card')?.innerText || '').replace(/\s+/g, ' ').trim().slice(0, 240))); await page.evaluate(() => window.theater.openModal(null)); await page.waitForTimeout(200); } return seen; };
  const shot = (name) => page.screenshot({ path: `shots/${prefix}-${name}.png` });
  const card = () => page.evaluate(() => { const c = document.querySelector('#veil.open #card'); return c ? { text: c.innerText.replace(/\s+/g, ' ').trim().slice(0, 2600), buttons: [...c.querySelectorAll('button')].map((x) => `${x.textContent.replace(/\s+/g, ' ').trim().slice(0, 50)} [${x.dataset.act ?? ''}${x.dataset.ask ? ':' + x.dataset.ask : ''}${x.dataset.path ? ':' + x.dataset.path : ''}${x.dataset.nat ? ':' + x.dataset.nat : ''}]${x.disabled ? ' (off)' : ''}`) } : null; });
  await page.goto(PAGE);
  await page.evaluate(() => { localStorage.clear(); localStorage.setItem('tc-watch', '0'); });
  await page.reload(); await page.waitForSelector('#card');
  say('the small countries on the picker', await page.evaluate(() => ({ great: [...document.querySelectorAll('.gcnats:not(.small) .gcnat')].map((x) => x.textContent.trim()), small: [...document.querySelectorAll('.gcnats.small .gcnat')].map((x) => x.textContent.trim()), note: document.querySelector('.gcsmallnote')?.textContent ?? null })));
  await page.click(`.gcnat[data-nat="${nat}"]`); await page.waitForTimeout(250);
  say('start card', await page.evaluate(() => ({ years: [...document.querySelectorAll('.gcyear')].map((x) => `${x.textContent.trim().slice(0, 4)}${x.disabled ? ' (off)' : ''}${x.classList.contains('on') ? ' [on]' : ''}`), under: document.querySelector('.gcunder')?.textContent ?? null, goals: [...document.querySelectorAll('.sgoal')].map((x) => x.textContent.replace(/\s+/g, ' ').trim()), text: (document.querySelector('#card')?.innerText || '').replace(/\s+/g, ' ').trim().slice(0, 1500) })));
  await shot('1-start-card');
  await page.click('.gcgo');
  await page.waitForFunction((nat) => { const g = window.theater.game(); return g && g.human === nat && !document.querySelector('#veil.open .busy'); }, nat, { timeout: 300000 });
  await page.waitForTimeout(1500);
  say('cards as the campaign opens', await shut());
  await page.waitForTimeout(400); await shot('2-map-as-it-opens');
  const state = () => page.evaluate((nat) => {
    const g = window.theater.game(), n = g.nations[nat];
    const mine = g.corps.filter((c) => c.nat === nat && !c.dead && c.kind !== 'fort');
    return { turn: g.turn, atWar: n.atWar, align: n.align, league: !!n.league, pp: Math.round(n.pp), pc: n.pc, corps: mine.length, away: mine.filter((c) => g.regions[c.reg].owner !== nat).length };
  }, nat);
  say('begun', await state());
  say('the top bar', await page.evaluate(() => (document.querySelector('#top, header, .topbar')?.innerText || '').replace(/\s+/g, ' ').trim().slice(0, 500)));
  // The path card.
  say('the path button', await page.evaluate(() => { const b = [...document.querySelectorAll('button')].find((x) => x.dataset.act === 'path'); if (b) b.click(); return b ? { text: b.textContent.trim(), title: b.title } : null; }));
  await page.waitForTimeout(300); say('the path card', await card()); await shot('3-path-card');
  for (const id of ['allies', 'axis', 'found']) {
    await page.evaluate((id) => { const b = document.querySelector(`#veil.open #card [data-act="path"][data-ask="${id}"]`); if (b) b.click(); }, id);
    await page.waitForTimeout(250); say(`the path card, asked for ${id}`, await card());
    if (id === 'found') await shot('4-path-league');
  }
  if (how === 'allies' || how === 'axis' || how === 'league') {
    const want = how === 'league' ? 'found' : how;
    await page.evaluate((id) => { const b = document.querySelector(`#veil.open #card [data-act="path"][data-ask="${id}"]`); if (b) b.click(); }, want);
    await page.waitForTimeout(250);
    say(`taking the path ${want}`, await page.evaluate((id) => { const b = [...document.querySelectorAll(`#veil.open #card [data-act="takepath"][data-path="${id}"]`)].find((x) => !x.disabled); if (b) b.click(); const g = window.theater.game(); return { clicked: b ? `${b.textContent.trim()} ${b.dataset.nat ?? ''}` : null, news: g.headlines.slice(-3).map((h) => h.text) }; }, want));
    await page.waitForTimeout(400); say('the path card after', await card()); await shot('5-path-taken'); await shut();
    say('after the path', await state());
  } else await shut();
  // Declare war: the list and one card.
  say('declare list', await page.evaluate(() => { const t = window.theater; t.ui.tab = 'region'; t.refresh(); const btn = [...document.querySelectorAll('button')].find((x) => x.dataset.act === 'declare'); if (btn) btn.click(); const c = document.querySelector('#veil.open #card'); return { button: !!btn, list: c ? [...c.querySelectorAll('[data-act="askdeclare"]')].map((x) => x.textContent.replace(/\s+/g, ' ').trim().slice(0, 70)) : null }; }));
  await page.waitForTimeout(300); await shot('6-declare-list');
  const foe = how === 'axis' ? 'USA' : 'GER';
  say(`the card for ${foe}`, await page.evaluate((foe) => { const el = document.querySelector(`#veil.open #card [data-act="askdeclare"][data-nat="${foe}"]`); if (el) el.click(); const c = document.querySelector('#veil.open #card'); return el && c ? { text: c.innerText.replace(/\s+/g, ' ').trim().slice(0, 1800), buttons: [...c.querySelectorAll('button')].map((x) => `${x.textContent.trim()} [${x.dataset.act ?? ''}]`) } : { offered: false }; }, foe));
  await page.waitForTimeout(300); await shot('7-declare-card');
  if (how === 'allies' || how === 'axis') {
    say('declared', await page.evaluate((nat) => { const c = document.querySelector('#veil.open #card'); const yes = c ? [...c.querySelectorAll('button')].find((x) => /declare war/i.test(x.textContent) && x.dataset.act !== 'askdeclare') : null; if (yes) yes.click(); const g = window.theater.game(); return { clicked: yes ? yes.textContent.trim() : null, atWar: g.nations[nat].atWar, align: g.nations[nat].align, news: g.headlines.slice(-4).map((h) => h.text) }; }, nat));
    await page.waitForTimeout(600);
  }
  await shut();
  // The desks.
  const pane = (tab) => page.evaluate((tab) => { const t = window.theater; t.ui.tab = tab; t.refresh(); return (document.querySelector('#pane')?.innerText || '').replace(/\s+/g, ' ').trim().slice(0, 1500); }, tab);
  say('build tab', await pane('build')); await shot('8-build');
  say('powers tab', await pane('powers')); await shot('9-powers');
  say('navy tab', await pane('navy'));
  say('staff tab', await pane('staff'));
  await page.evaluate(() => { const t = window.theater; t.ui.tab = 'region'; t.refresh(); });
  const h0 = await page.evaluate(() => window.theater.game().headlines.length);
  for (let m = 1; m <= +monthsS; m++) {
    const turn = await page.evaluate(() => window.theater.game().turn);
    const t1 = Date.now();
    await page.evaluate(() => document.querySelector('[data-act="end"]')?.click());
    await page.waitForTimeout(400);
    if (await veil()) { const go = page.locator('#card [data-act="endgo"], #card [data-act="endsure"], #card [data-act="end"], #card footer .btn.primary').first(); if (await go.count()) { say(`  a card before month ${m} ends`, await page.evaluate(() => (document.querySelector('#card')?.innerText || '').replace(/\s+/g, ' ').slice(0, 400))); await go.click(); } }
    try { await page.waitForFunction((t0) => window.theater.game().turn > t0, turn, { timeout: 300000 }); } catch { say('  the date did not move', turn); }
    await page.waitForTimeout(900);
    const cards = await shut();
    say(`month ${m} (${((Date.now() - t1) / 1000).toFixed(1)} s)`, { ...(await state()), cards: cards.length, first: cards[0] ?? null });
  }
  say('news naming the country in those months', await page.evaluate(({ nat, h0 }) => { const g = window.theater.game(); const name = g.nations[nat].name; return g.headlines.slice(h0).filter((h) => h.nat === nat || h.text.includes(name)).map((h) => h.text.slice(0, 200)); }, { nat, h0 }));
  await shot('10-after-the-months');
  say('page errors', errors.length ? errors.slice(0, 8) : 'none');
  await browser.close();
})();
