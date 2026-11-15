// Version 75 on the page: an American player's first hour, played as a player would.
//   From September 1939 (or YEAR): the line under the years; Declare war on Germany and what the card says; the Build
//   tab's word on war work; New York's corps picked up, "Ship by sea", the list of ports by country; every corps in a
//   home port shipped to Britain from the list, month after month (a refused click is written down); the navy
//   desk's crossings, the first named as the plan, the staff's word on it; the goals panel; Canada on the Powers tab.
// usage: [TC_PAGE=file:///home/user/tc75/dist/play.html] node ui44.cjs YEAR MONTHS PREFIX [declare|wait]
const { chromium } = require('playwright');
const [year = '1939', monthsS = '8', prefix = '99-v75-hour', how = 'declare'] = process.argv.slice(2);
const PAGE = process.env.TC_PAGE || 'file:///home/user/tc75/dist/play.html';
(async () => {
  const browser = await chromium.launch(); const errors = [];
  const ctx = await browser.newContext({ viewport: { width: 1440, height: 900 }, deviceScaleFactor: 2 });
  const page = await ctx.newPage();
  page.on('pageerror', (e) => errors.push(`${e.message} @ ${(e.stack || '').split('\n').slice(1, 3).join(' | ').slice(0, 200)}`));
  page.on('console', (m) => { if (m.type() === 'error') errors.push(`console: ${m.text().slice(0, 300)}`); });
  const say = (k, v) => console.log(`${k}: ${typeof v === 'string' ? v : JSON.stringify(v)}`);
  const veil = () => page.evaluate(() => { const v = document.querySelector('#veil'); return !!v && v.classList.contains('open'); });
  const shut = async () => { for (let i = 0; i < 12 && await veil(); i++) { await page.evaluate(() => window.theater.openModal(null)); await page.waitForTimeout(200); } };
  const shot = (name) => page.screenshot({ path: `shots/${prefix}-${name}.png` });
  await page.goto(PAGE);
  await page.evaluate(() => { localStorage.clear(); localStorage.setItem('tc-watch', '0'); });
  await page.reload(); await page.waitForSelector('#card');
  await page.click('.gcnat[data-nat="USA"]'); await page.waitForTimeout(200);
  say('the years as first offered', await page.evaluate(() => [...document.querySelectorAll('.gcyear')].map((x) => `${x.textContent.trim().slice(0, 4)}${x.classList.contains('on') ? ' [on]' : ''}`)));
  await page.evaluate((year) => { const y = [...document.querySelectorAll('.gcyear')].find((x) => x.textContent.trim().startsWith(year)); if (y && !y.disabled) y.click(); }, year);
  await page.waitForTimeout(200);
  say('start card', await page.evaluate(() => ({ under: document.querySelector('.gcunder')?.textContent ?? null, goals: [...document.querySelectorAll('.sgoal')].map((x) => x.textContent.replace(/\s+/g, ' ').trim()), brief: document.querySelector('.gcbrief .you')?.textContent ?? null })));
  await page.evaluate(() => document.querySelector('.gcunder')?.scrollIntoView({ block: 'center' })); await page.waitForTimeout(250);
  await shot('1-start-card');
  await page.click('.gcgo');
  await page.waitForFunction(() => { const g = window.theater.game(); return g && g.human === 'USA' && !document.querySelector('#veil.open .busy'); }, null, { timeout: 240000 });
  await page.waitForTimeout(1500); await shut();
  const state = () => page.evaluate(() => {
    const g = window.theater.game(), n = g.nations.USA;
    const mine = g.corps.filter((c) => c.nat === 'USA' && !c.dead && c.kind !== 'fort');
    const where = {}; for (const c of mine) { const r = g.regions[c.reg]; const k = `${r.name}${r.owner !== 'USA' ? ` (${r.owner})` : ''}`; where[k] = (where[k] || 0) + 1; }
    return { turn: g.turn, atWar: n.atWar, align: n.align, treasury: Math.round(n.treasury ?? n.money ?? 0), sealift: n.sealift, warWork: g.flags['ww:USA'] ?? null, corps: mine.length, where };
  });
  say('begun', await state());
  const buildNote = () => page.evaluate(() => { const t = window.theater; t.ui.tab = 'build'; t.refresh(); const notes = [...document.querySelectorAll('#pane .note, #pane .sub, #pane p')].map((x) => x.textContent.replace(/\s+/g, ' ').trim()); const money = [...document.querySelectorAll('#pane .stat, #pane .money, #pane .kv')].map((x) => x.textContent.replace(/\s+/g, ' ').trim()).slice(0, 4); return { note: notes.find((x) => /war work/.test(x)) ?? null, money, head: (document.querySelector('#pane')?.innerText || '').replace(/\s+/g, ' ').slice(0, 520) }; });
  say('build tab at the start', await buildNote());
  if (how === 'declare' && (await page.evaluate(() => !window.theater.game().nations.USA.atWar.length))) {
    say('declare list', await page.evaluate(() => { const t = window.theater; t.ui.tab = 'region'; t.refresh(); const btn = [...document.querySelectorAll('button')].find((x) => x.dataset.act === 'declare'); if (btn) btn.click(); const card = document.querySelector('#veil.open #card'); return { button: !!btn, list: card ? [...card.querySelectorAll('[data-act="askdeclare"]')].map((x) => x.textContent.replace(/\s+/g, ' ').trim().slice(0, 90)) : null }; }));
    await page.waitForTimeout(300); await shot('2-declare-list');
    say('the card for Germany', await page.evaluate(() => { const el = document.querySelector('#veil.open #card [data-act="askdeclare"][data-nat="GER"]'); if (el) el.click(); const card = document.querySelector('#veil.open #card'); return card ? { text: card.innerText.replace(/\s+/g, ' ').trim().slice(0, 1800), buttons: [...card.querySelectorAll('button')].map((x) => `${x.textContent.trim()} [${x.dataset.act ?? ''}]`) } : null; }));
    await page.waitForTimeout(300); await shot('3-declare-germany');
    say('declared', await page.evaluate(() => { const card = document.querySelector('#veil.open #card'); const yes = card ? [...card.querySelectorAll('button')].find((x) => /declare war/i.test(x.textContent) && x.dataset.act !== 'askdeclare') : null; if (yes) yes.click(); const g = window.theater.game(); return { clicked: yes ? yes.textContent.trim() : null, atWar: g.nations.USA.atWar, align: g.nations.USA.align, news: g.headlines.slice(-5).map((h) => h.text) }; }));
    await page.waitForTimeout(600); await shut();
    say('build tab after the declaration', await buildNote());
    await shot('4-build-at-war');
  }
  // Ship every corps standing in a home port to Britain from the list; write down what the page says.
  const shipAll = async (label, picture) => page.evaluate(async ({ picture }) => {
    const t = window.theater; const g = t.game(); const out = [];
    const ports = g.regions.filter((r) => r.port && r.owner === 'USA' && r.units.some((c) => c.nat === 'USA' && !c.dead && c.kind !== 'fort' && c.ap > 0));
    for (const r of ports) {
      t.ui.tab = 'region'; t.refresh(); t.onRegion(r.id);
      const all = [...document.querySelectorAll('button')].find((x) => x.dataset.act === 'pickall'); if (all) all.click();
      const picked = [...t.ui.picked].length;
      const ship = [...document.querySelectorAll('button')].find((x) => x.dataset.act === 'ship');
      if (!ship) { out.push({ from: r.name, picked, ship: 'no Ship by sea button', buttons: [...document.querySelectorAll('#pane button, #tray button')].map((x) => x.textContent.trim().slice(0, 24)).slice(0, 12) }); continue; }
      ship.click();
      const list = document.querySelector('.shiplist');
      const rows = list ? [...list.querySelectorAll('.shiprow')].map((x) => `${x.querySelector('.shipnat')?.textContent.trim()}: ${[...x.querySelectorAll('button')].map((b) => b.textContent.trim()).join(', ')}`) : null;
      const lead = list ? list.innerText.replace(/\s+/g, ' ').slice(0, 260) : null;
      const row = list ? [...list.querySelectorAll('.shiprow')].find((x) => /United Kingdom/.test(x.querySelector('.shipnat')?.textContent ?? '')) : null;
      const bts = row ? [...row.querySelectorAll('button')] : []; const bt = bts.find((b) => /^Kent$/i.test(b.textContent.trim())) ?? bts.find((b) => /^Hampshire$/i.test(b.textContent.trim())) ?? bts.find((b) => /^West Country$/i.test(b.textContent.trim())) ?? bts[0]; const id = bt ? Number(bt.dataset.id) : -1;
      if (bt) bt.click();
      out.push({ from: r.name, picked, hint: t.ui.msg, lead, rows, to: id >= 0 ? g.regions[id].name : null, after: t.ui.msg, americansThere: id >= 0 ? g.regions[id].units.filter((c) => c.nat === 'USA').length : 0, stillPicked: [...t.ui.picked].length });
      if (t.ui.ship) { t.ui.ship = false; t.ui.picked?.clear?.(); t.refresh(); }
    }
    return out;
  }, { picture });
  if (await page.evaluate(() => window.theater.game().nations.USA.atWar.length > 0)) {
    // The list as a player sees it, pictured once.
    await page.evaluate(() => { const t = window.theater; const g = t.game(); const ny = g.byName['New York']; t.ui.tab = 'region'; t.refresh(); t.onRegion(ny.id); const all = [...document.querySelectorAll('button')].find((x) => x.dataset.act === 'pickall'); if (all) all.click(); const ship = [...document.querySelectorAll('button')].find((x) => x.dataset.act === 'ship'); if (ship) ship.click(); document.querySelector('.shiplist')?.scrollIntoView({ block: 'center' }); });
    await page.waitForTimeout(400); await shot('5-ship-list');
    say('the map in ship mode', await page.evaluate(() => { const svg = document.querySelector('#map svg, svg#map, svg'); return { viewBox: svg?.getAttribute('viewBox'), hint: window.theater.ui.msg }; }));
    await page.evaluate(() => { const t = window.theater; t.ui.ship = false; t.ui.picked?.clear?.(); t.refresh(); });
    say('shipped, month 0', await shipAll());
  }
  for (let m = 1; m <= +monthsS; m++) {
    const turn = await page.evaluate(() => window.theater.game().turn);
    const t1 = Date.now();
    await page.evaluate(() => document.querySelector('[data-act="end"]')?.click());
    await page.waitForTimeout(400);
    if (await veil()) { const go = page.locator('#card [data-act="endgo"], #card [data-act="endsure"], #card [data-act="end"], #card footer .btn.primary').first(); if (await go.count()) { say(`  a card before month ${m} ends`, await page.evaluate(() => (document.querySelector('#card')?.innerText || '').replace(/\s+/g, ' ').slice(0, 300))); await go.click(); } }
    try { await page.waitForFunction((t0) => window.theater.game().turn > t0, turn, { timeout: 240000 }); } catch { say('  the date did not move', turn); }
    await page.waitForTimeout(900);
    const cards = []; for (let k = 0; k < 12 && await veil(); k++) { cards.push(await page.evaluate(() => (document.querySelector('#card')?.innerText || '').replace(/\s+/g, ' ').trim().slice(0, 160))); await page.evaluate(() => window.theater.openModal(null)); await page.waitForTimeout(200); }
    say(`month ${m} (${((Date.now() - t1) / 1000).toFixed(1)} s)`, { ...(await state()), cards: cards.length, firstCard: cards[0] ?? null });
    if (await page.evaluate(() => window.theater.game().nations.USA.atWar.length > 0)) { const s = await shipAll(); if (s.length) say(`  shipped, month ${m}`, s.map((x) => ({ from: x.from, picked: x.picked, to: x.to, after: x.after, there: x.americansThere, ship: x.ship }))); }
    if (m === 2 || m === +monthsS) say(`  build tab, month ${m}`, (await buildNote()).note);
  }
  // The navy desk: the crossings, the first named as the plan, the staff's word.
  say('the crossings on the navy desk', await page.evaluate(() => { const t = window.theater; t.ui.tab = 'navy'; t.refresh(); const pane = document.querySelector('#pane'); const txt = (e) => (e ? e.textContent.replace(/\s+/g, ' ').trim() : ''); return [...pane.querySelectorAll('.cross')].map((x) => ({ plan: x.classList.contains('plan'), head: txt(x.querySelector('.crhead')), sub: txt(x.querySelector('.sub')).slice(0, 300), buttons: [...x.querySelectorAll('button')].map((b) => `${b.textContent.trim()} [${b.dataset.act}]`) })).slice(0, 8); }));
  say('the navy desk, the words round the crossing', await page.evaluate(() => { const all = (document.querySelector('#pane')?.innerText || '').replace(/\s+/g, ' '); const k = all.search(/Crossing the sea|crossing/i); return k >= 0 ? all.slice(Math.max(0, k - 80), k + 900) : all.slice(0, 500); }));
  say('named as the plan', await page.evaluate(() => { const b = document.querySelector('#pane [data-act="crossplan"]'); if (b) b.click(); const t = window.theater; t.refresh(); const w = document.querySelector('.crossword'); return { button: !!b, word: w ? w.textContent.replace(/\s+/g, ' ').trim() : null, state: w ? w.className : null, plan: (document.querySelector('#pane .cross.plan')?.textContent ?? '').replace(/\s+/g, ' ').slice(0, 400) }; }));
  await page.evaluate(() => document.querySelector('.crossword, #pane .cross.plan')?.scrollIntoView({ block: 'center' })); await page.waitForTimeout(300); await shot('6-navy-crossing');
  say('the staff tab on the crossing', await page.evaluate(() => { const t = window.theater; t.ui.tab = 'staff'; t.refresh(); const w = document.querySelector('.crossword'); const lead = document.querySelector('#pane .crosslead'); return { word: w ? w.textContent.replace(/\s+/g, ' ').trim() : null, state: w ? w.className : null, lead: lead ? lead.textContent.replace(/\s+/g, ' ').trim().slice(0, 500) : null }; }));
  await page.evaluate(() => document.querySelector('.crossword, #pane .crosslead')?.scrollIntoView({ block: 'center' })); await page.waitForTimeout(300); await shot('7-staff-crossing');
  say('goals', await page.evaluate(() => { const t = window.theater; t.ui.tab = 'region'; t.refresh(); const g = [...document.querySelectorAll('.goal')].map((x) => x.textContent.replace(/\s+/g, ' ').trim().slice(0, 320)); return g.length ? g : 'no goals on the page now'; }));
  say('Canada on the Powers tab', await page.evaluate(() => { const t = window.theater; t.ui.tab = 'powers'; t.refresh(); const all = (document.querySelector('#pane')?.innerText || '').replace(/\s+/g, ' '); const k = all.indexOf('Canada about'); return k >= 0 ? all.slice(k, k + 140) : null; }));
  say('page errors', errors.length ? errors.slice(0, 6) : 'none');
  await browser.close();
})().catch((e) => { console.error('FAILED', e); process.exit(1); });
