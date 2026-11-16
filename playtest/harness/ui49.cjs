// Version 82 on the page: NEXT ARMY, a march kept for next month, the gates, and the month on one card.
// A player (Germany by default) opens 1939, reads the brass button and END MONTH, presses Tab three times, stands one
// army, orders one rear army to a far friendly place by right-click (a kept march), reads the gate marks, ends the
// month, and reads the month card for MONTHS months. Screenshots go to shots/PREFIX-*.png; page errors are listed.
// usage: [TC_PAGE=file:///home/user/tc82/dist/play.html] node ui49.cjs NAT MONTHS PREFIX
const { chromium } = require('playwright');
const [nat = 'GER', monthsS = '3', prefix = '99-v82'] = process.argv.slice(2);
const PAGE = process.env.TC_PAGE || 'file:///home/user/tc82/dist/play.html';
(async () => {
  const browser = await chromium.launch(); const errors = [];
  const page = await (await browser.newContext({ viewport: { width: 1440, height: 900 }, deviceScaleFactor: 2 })).newPage();
  page.on('pageerror', (e) => errors.push(e.message.slice(0, 200)));
  page.on('console', (m) => { if (m.type() === 'error') errors.push(`console: ${m.text().slice(0, 200)}`); });
  const say = (k, v) => console.log(`${k}: ${typeof v === 'string' ? v : JSON.stringify(v)}`);
  const shot = (name) => page.screenshot({ path: `shots/${prefix}-${name}.png` });
  const veil = () => page.evaluate(() => { const v = document.querySelector('#veil'); return !!v && v.classList.contains('open'); });
  const shut = async () => { for (let i = 0; i < 12 && await veil(); i++) { await page.evaluate(() => window.theater.openModal(null)); await page.waitForTimeout(200); } };
  const txt = (sel, n = 300) => page.evaluate(([sel, n]) => [...document.querySelectorAll(sel)].map((e) => (e.innerText || e.textContent || '').replace(/\s+/g, ' ').trim().slice(0, n)), [sel, n]);
  const round = async () => ({ next: (await txt('.nextarmy, .nextrear, .allset'))[0] || '(none)', end: await page.evaluate(() => { const b = [...document.querySelectorAll('button')].find((b) => /END MONTH/i.test(b.innerText || '')); return b ? `${b.className}${b.disabled ? ' disabled' : ''}` : '(no END MONTH button)'; }), sel: await page.evaluate(() => { const t = window.theater, g = t.game(); return t.ui.sel >= 0 ? g.regions[t.ui.sel].name : '(none)'; }), msg: await page.evaluate(() => window.theater.ui.msg || '') });
  await page.goto(PAGE); await page.evaluate(() => { localStorage.clear(); localStorage.setItem('tc-watch', '0'); }); await page.reload(); await page.waitForSelector('#card');
  await page.click(`.gcnat[data-nat="${nat}"]`); await page.waitForTimeout(250); await page.click('.gcgo');
  await page.waitForFunction((nat) => { const g = window.theater.game(); return g && g.human === nat && !document.querySelector('#veil.open .busy'); }, nat, { timeout: 300000 });
  await page.waitForTimeout(1500);
  if (await veil()) say('the card that opens the campaign', (await txt('#card', 500))[0]);
  await shut();
  say('as the campaign opens', await round()); await shot('1-open');
  for (let i = 1; i <= 3; i++) { await page.keyboard.press('Tab'); await page.waitForTimeout(400); say(`after Tab ${i}`, await round()); }
  await shot('2-next-army');
  const stood = await page.evaluate(() => { const b = document.querySelector('[data-act="standmonth"]'); if (!b) return null; const t = (b.closest('.tnow')?.innerText || b.innerText).replace(/\s+/g, ' ').trim(); b.click(); return t; });
  await page.waitForTimeout(300); say('STAND THIS MONTH, the line on the army card', stood); say('after standing it', await round());
  // A march that is kept: an army in the rear, a friendly place too far for this month.
  const plan = await page.evaluate((nat) => {
    const g = window.theater.game();
    const mine = g.regions.filter((r) => r.ctrl === nat && !r.overseas);
    const foe = (r) => r.nb.some((n) => { const o = g.regions[n]; return o.ctrl !== nat && g.nations[nat].atWar.includes(o.ctrl); });
    const rear = mine.filter((r) => !foe(r) && r.units.some((u) => u.nat === nat && u.kind !== 'fort'));
    // The farthest friendly place that can be walked to over friendly ground (hops), from an army in the rear.
    let best = null;
    for (const a of rear) {
      const dist = new Map([[a.id, 0]]); const q = [a.id];
      while (q.length) { const id = q.shift(); for (const n of g.regions[id].nb) { const o = g.regions[n]; if (o.ctrl === nat && !o.overseas && !dist.has(n)) { dist.set(n, dist.get(id) + 1); q.push(n); } } }
      for (const [id, d] of dist) if (d >= 4 && (!best || d > best.d)) best = { from: a.id, to: id, d, fromName: a.name, toName: g.regions[id].name };
    }
    return best;
  }, nat);
  say('the march asked for', plan);
  if (plan) {
    await page.evaluate((id) => window.theater.onRegion(id), plan.from); await page.waitForTimeout(400);
    const at = await page.evaluate((id) => { const g = window.theater.game(), r = g.regions[id]; const el = document.querySelector(`#regs .reg[data-id="${id}"]`); const svg = document.querySelector('#map'); const p = svg.createSVGPoint(); p.x = r.x; p.y = r.y; const s = p.matrixTransform(el.getScreenCTM()); return { x: s.x, y: s.y, w: innerWidth, h: innerHeight }; }, plan.to);
    say('where the right-click lands', at);
    if (at.x > 0 && at.y > 0 && at.x < at.w && at.y < at.h) { await page.mouse.click(at.x, at.y, { button: 'right' }); }
    else { await page.evaluate((id) => { const g = window.theater.game(), r = g.regions[id]; const v = window.theater.ui.view; v.x = r.x - v.w / 2; v.y = r.y - v.w / 3; window.theater.applyView?.(); window.theater.refresh(); }, plan.to); await page.waitForTimeout(400); const at2 = await page.evaluate((id) => { const g = window.theater.game(), r = g.regions[id]; const el = document.querySelector(`#regs .reg[data-id="${id}"]`); const svg = document.querySelector('#map'); const p = svg.createSVGPoint(); p.x = r.x; p.y = r.y; const s = p.matrixTransform(el.getScreenCTM()); return { x: s.x, y: s.y }; }, plan.to); say('after moving the map', at2); await page.mouse.click(at2.x, at2.y, { button: 'right' }); }
    await page.waitForTimeout(500);
    if (await veil()) { say('a card after the right-click', (await txt('#card', 400))[0]); await shut(); }
    say('after the right-click', await round());
    say('the army card', (await txt('#pane', 700))[0]);
    await shot('3-kept-march');
  }
  say('gate marks', await page.evaluate(() => [...document.querySelectorAll('#gates .gatemark')].map((m) => `${m.getAttribute('class')} | ${(m.querySelector('title')?.textContent || m.getAttribute('aria-label') || '').replace(/\s+/g, ' ').slice(0, 200)}`)));
  for (let m = 1; m <= Number(monthsS); m++) {
    const before = await page.evaluate(() => window.theater.game().turn);
    const clicked = await page.evaluate(() => { const b = [...document.querySelectorAll('button')].find((b) => /END MONTH/i.test(b.innerText || '')); if (!b) return null; const t = `${b.className} | ${(b.innerText || '').replace(/\s+/g, ' ').trim()} | ${b.title || ''}`.slice(0, 260); b.click(); return t; });
    say(`month ${m}: END MONTH pressed`, clicked); await page.waitForTimeout(700);
    { const ask = (await txt('#card', 500))[0] || ''; if ((await veil()) && !/Dispatches/.test(ask)) { say(`month ${m}: the question before the month ends`, ask); await shot(`4-month${m}-question`); say(`month ${m}: its buttons`, await txt('#card button', 60)); await page.evaluate(() => { const bs = [...document.querySelectorAll('#card button')]; const b = bs.find((b) => /end the month|end month/i.test(b.innerText)) || bs[bs.length - 1]; b?.click(); }); } }
    await page.waitForFunction((t) => { const g = window.theater.game(); return g.turn > t && !document.querySelector('#veil.open .busy'); }, before, { timeout: 300000 }).catch(() => errors.push(`month ${m}: the month did not turn`));
    await page.waitForTimeout(2500);
    say(`month ${m}: turn`, await page.evaluate(() => window.theater.game().turn));
    say(`month ${m}: the month card, head`, await txt('.mc-head', 200));
    say(`month ${m}: red bands`, await txt('.mc-banner', 260));
    say(`month ${m}: tiles`, await txt('.mc-tile', 60));
    say(`month ${m}: lead story`, await txt('.mc-leadcol', 420));
    say(`month ${m}: rows`, await txt('.mc-row', 150));
    say(`month ${m}: the three boxes`, await txt('.mc-box', 420));
    await shot(`5-month${m}-card`);
    if (plan && m === 1) say('the kept march a month on', await page.evaluate((p) => { const g = window.theater.game(); return { from: g.regions[p.from].units.filter((u) => u.nat === g.human && u.kind !== 'fort').length, to: g.regions[p.to].units.filter((u) => u.nat === g.human && u.kind !== 'fort').length, news: g.headlines.filter((h) => /march/i.test(h.text)).slice(-3).map((h) => h.text.slice(0, 160)) }; }, plan));
    await shut(); await page.waitForTimeout(300);
    say(`month ${m}: as the next month opens`, await round());
    if (m === 1) await shot('6-second-month-map');
  }
  say('page errors', errors);
  await browser.close();
})();
