// Version 54 by hand: win the field, see the siege, storm the town, read the cards.
const { chromium } = require('playwright');
const dir = __dirname;
(async () => {
  const browser = await chromium.launch();
  const page = await browser.newPage({ viewport: { width: 1440, height: 900 } });
  const errs = [];
  page.on('pageerror', (e) => errs.push(e.message));
  page.on('console', (m) => { if (m.type() === 'error') errs.push('console: ' + m.text()); });
  await page.addInitScript(() => { try { localStorage.setItem('tc-watch', '0'); } catch {} });
  await page.goto('file:///home/user/theater-command/dist/play.html');
  await page.waitForTimeout(800);
  await page.evaluate(() => window.theater.newGame('GER', 7, 1, 'grand', 'colonel'));
  await page.waitForTimeout(800);
  const closeAll = async () => { for (let i = 0; i < 12; i++) { const open = await page.evaluate(() => document.querySelector('#veil')?.classList.contains('open')); if (!open) break; await page.keyboard.press('Escape'); await page.waitForTimeout(120); } };
  const card = () => page.evaluate(() => document.querySelector('#veil')?.classList.contains('open') ? document.querySelector('#card')?.innerText?.replace(/\s+/g, ' ').slice(0, 1500) : '(no card)');
  const side = (n = 1400) => page.evaluate((n) => (document.querySelector('#side, aside, .side')?.innerText ?? '').replace(/\s+/g, ' ').slice(0, n), n);
  const acts = (sel = '') => page.evaluate((sel) => [...new Set([...document.querySelectorAll((sel ? sel + ' ' : '') + '[data-act]')].filter((e) => e.offsetParent !== null).map((e) => `${e.getAttribute('data-act')}|${(e.textContent || '').trim().replace(/\s+/g, ' ').slice(0, 50)}`))].filter((x) => /storm|siege|garrison|sally|takefield|raidtown|attack|town/.test(x)).join('  ;  '), sel);
  const press = (sel) => page.evaluate((s) => { const e = document.querySelector(s); if (!e) return 'missing ' + s; e.dispatchEvent(new MouseEvent('click', { bubbles: true })); return 'pressed ' + s + ' "' + (e.textContent || '').trim().replace(/\s+/g, ' ').slice(0, 40) + '"'; }, sel);
  await closeAll();
  // Find a Polish province beside German troops whose town has a guard of its own.
  const pairs = await page.evaluate(() => { const g = window.theater.game(); const out = []; for (const r of g.regions) { if (r.ctrl !== 'GER' || !r.units.some((u) => u.nat === 'GER' && !u.dead && u.kind !== 'fort')) continue; for (const j of r.nb) if (g.regions[j].ctrl === 'POL') out.push({ from: r.id, fromName: r.name, to: j, toName: g.regions[j].name, field: g.regions[j].units.length }); } return out; });
  let pick = null;
  for (const p of pairs) {
    await page.evaluate((id) => window.theater.onRegion(id), p.to); await page.waitForTimeout(120);
    const t = await side(700);
    const m = /TOWN GUARD ([\d.]+ div|Part of a division|None)/i.exec(t);
    if (m && !/None/i.test(m[1])) { pick = { ...p, guard: m[1], head: t.slice(60, 330) }; break; }
    await page.keyboard.press('Escape');
  }
  console.log('pick', JSON.stringify(pick));
  await page.keyboard.press('Escape'); await page.waitForTimeout(100);
  await page.evaluate((id) => window.theater.onRegion(id), pick.from); await page.waitForTimeout(200);
  await page.evaluate((id) => window.theater.onRegion(id), pick.to); await page.waitForTimeout(300);
  // Send every corps offered.
  await page.waitForTimeout(200);
  console.log('ATTACK CARD:', (await card()).slice(700, 1500));
  await page.screenshot({ path: dir + '/shots/49-attack-card-town.png' });
  console.log(await press('#card [data-act="attack"]')); await page.waitForTimeout(500);
  console.log('RESULT:', (await card()).slice(0, 700));
  await page.screenshot({ path: dir + '/shots/50-field-won.png' });
  await closeAll();
  await page.evaluate((id) => window.theater.onRegion(id), pick.to); await page.waitForTimeout(300);
  console.log('SIDE (besieged province):', await side(1300));
  console.log('ACTS:', await acts());
  await page.screenshot({ path: dir + '/shots/51-siege-panel.png' });
  console.log(await press('[data-act="storm"]')); await page.waitForTimeout(400);
  console.log('STORM CARD:', await card());
  console.log('ACTS in card:', await acts('#card'));
  await page.screenshot({ path: dir + '/shots/52-storm-card.png' });
  console.log(await press('#card [data-act="dosiege"]')); await page.waitForTimeout(500);
  console.log('STORM RESULT:', (await card()).slice(0, 900));
  await page.screenshot({ path: dir + '/shots/53-storm-result.png' });
  await closeAll();
  const st = await page.evaluate((id) => { const g = window.theater.game(); const r = g.regions[id]; return { ctrl: r.ctrl, field: r.units.map((u) => u.nat + ' ' + u.name + ' ap' + u.ap), garr: (r.garr || []).map((u) => u.nat + ' ' + u.name) }; }, pick.to);
  console.log('STATE', JSON.stringify(st));
  await page.evaluate((id) => window.theater.onRegion(id), pick.to); await page.waitForTimeout(300);
  console.log('SIDE after the assault:', await side(700));
  await page.screenshot({ path: dir + '/shots/53b-after-assault.png' });
  // My own capital: the garrison controls.
  const cap = await page.evaluate(() => window.theater.game().nations.GER.capital);
  await page.evaluate((id) => window.theater.onRegion(id), cap); await page.waitForTimeout(300);
  console.log('BERLIN SIDE:', (await side(1500)).slice(60, 420));
  console.log('ACTS:', await acts());
  await page.screenshot({ path: dir + '/shots/54-berlin-town.png' });
  const eng = await page.evaluate(() => { const b = [...document.querySelectorAll('[data-act="garrison"]')].find((e) => /Engineer/.test(e.textContent)); if (!b) return 'no engineer button'; b.dispatchEvent(new MouseEvent('click', { bubbles: true })); return 'pressed ' + b.textContent.trim(); });
  await page.waitForTimeout(300);
  const after = await page.evaluate(() => { const g = window.theater.game(); const r = g.regions[g.nations.GER.capital]; return { garr: (r.garr || []).map((u) => u.name), msg: (document.querySelector('.msg, #msg, .toast, .hint')?.innerText ?? '').slice(0, 200), card: document.querySelector('#veil')?.classList.contains('open') ? document.querySelector('#card').innerText.replace(/\s+/g, ' ').slice(0, 300) : '' }; });
  console.log('ENGINEER INTO BERLIN:', eng, JSON.stringify(after));
  await page.screenshot({ path: dir + '/shots/55-engineer-into-town.png' });
  console.log('errors', errs);
  await browser.close();
})();
