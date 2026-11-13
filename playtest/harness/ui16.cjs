// Version 57: the government screen as a player meets it. Loads a save made by probe25 and reads the top bar, the
// alerts and the Cabinet card, then clicks through it.   usage: ui16.cjs SAVE.json SHOT-PREFIX [steps: aside,appoint,decree,show]
const { chromium } = require('playwright');
const fs = require('fs');
const [file, shot, stepsS] = process.argv.slice(2);
const steps = (stepsS ?? 'aside,appoint,decree,show').split(',');
const flat = (s, n = 1800) => s.replace(/\n+/g, ' / ').replace(/\s+/g, ' ').slice(0, n);
(async () => {
  const browser = await chromium.launch();
  const page = await browser.newPage({ viewport: { width: 1440, height: 900 }, deviceScaleFactor: 2 });
  const errors = [];
  page.on('pageerror', (e) => errors.push(e.message));
  page.on('console', (m) => { if (m.type() === 'error') errors.push('console: ' + m.text()); });
  const save = fs.readFileSync(file, 'utf8');
  await page.addInitScript((s) => { try { localStorage.setItem('tc-watch', '0'); localStorage.setItem('theater-command-save-v1', s); } catch {} }, save);
  await page.goto(process.env.TC_PAGE || 'file:///home/user/tc57/dist/play.html');
  await page.waitForTimeout(1000);
  const cont = page.locator('[data-act="continue"]').first();
  if (await cont.count()) { await cont.click(); } else console.log('continue (no card):', await page.evaluate(() => window.theater.continueGame()));
  await page.waitForTimeout(1500);
  const st = async () => page.evaluate(() => { const g = window.theater.game(); const n = g.nations[g.human]; return { turn: g.turn, human: g.human, pc: n.pc, trust: n.gov ? Math.round(n.gov.trust * 10) / 10 : null, seats: n.gov ? n.gov.seats : null, steps: n.gov ? n.gov.steps : null, dir: n.gov && n.gov.dir ? n.gov.dir.kind : null, heads: g.headlines.length }; });
  console.log('loaded:', JSON.stringify(await st()));
  // 1. the top bar and the alerts
  const cab = page.locator('[data-act="cabinet"]').first();
  console.log('cabinet button:', await cab.count() ? JSON.stringify({ text: (await cab.innerText()).trim(), cls: await cab.getAttribute('class'), title: await cab.getAttribute('title') }) : 'NONE');
  const bar = await page.evaluate(() => [...document.querySelectorAll('header .btn, #top .btn, .topbar .btn')].filter((b) => b.offsetParent !== null).map((b) => b.textContent.trim()).join(' | '));
  console.log('top bar buttons:', bar.slice(0, 500));
  const alerts = await page.locator('button.alert').evaluateAll((bs) => bs.map((b) => `[${b.dataset.kind}] ${b.textContent.trim()}`));
  console.log('alerts (' + alerts.length + '):\n  ' + alerts.join('\n  '));
  await page.screenshot({ path: `shots/${shot}-1-map.png` });
  // 2. the card
  await cab.click(); await page.waitForTimeout(500);
  const card = page.locator('.body.gov').first();
  if (!(await card.count())) { console.log('NO CABINET CARD'); await browser.close(); return; }
  const dims = await card.evaluate((e) => ({ scroll: e.scrollHeight, client: e.clientHeight, w: e.clientWidth }));
  console.log('card:', JSON.stringify(dims), 'screens of scrolling', (dims.scroll / dims.client).toFixed(2));
  console.log('TEXT:', flat(await card.innerText(), 5200));
  await page.screenshot({ path: `shots/${shot}-2-cabinet.png` });
  // scroll through the card and shoot each screenful
  const pages = Math.ceil(dims.scroll / dims.client);
  for (let i = 1; i < pages && i < 5; i++) { await card.evaluate((e, y) => { e.scrollTop = y; }, i * (dims.client - 40)); await page.waitForTimeout(200); await page.screenshot({ path: `shots/${shot}-2-cabinet-p${i + 1}.png` }); }
  await card.evaluate((e) => { e.scrollTop = 0; });
  const btnList = async () => (await page.locator('.body.gov button').evaluateAll((bs) => bs.map((b) => `${b.dataset.act}${b.dataset.seat ? ':' + b.dataset.seat + '/' + b.dataset.min : ''}${b.dataset.track ? ':' + b.dataset.track + '/' + b.dataset.step : ''}${b.disabled ? ' [off]' : ''}`))).join(' | ');
  console.log('buttons:', await btnList());
  if (steps.includes('aside')) {
    const a = page.locator('.body.gov [data-act="govaside"]').first();
    if (await a.count() && !(await a.isDisabled())) {
      await a.click(); await page.waitForTimeout(300);
      console.log('after one click on Set it aside:', JSON.stringify(await st()), '/ note:', flat(await page.locator('.gvdir').first().innerText(), 500));
      await page.locator('.gvdir').first().scrollIntoViewIfNeeded(); await page.screenshot({ path: `shots/${shot}-3-aside-ask.png` });
      const k = page.locator('[data-act="govkeep"]').first(); if (await k.count()) { await k.click(); await page.waitForTimeout(200); console.log('kept:', JSON.stringify(await st())); }
    } else console.log('no directive to set aside (or not allowed)');
  }
  if (steps.includes('appoint')) {
    const b = page.locator('.body.gov [data-act="govappoint"]:not([disabled])').first();
    if (await b.count()) {
      const what = await b.evaluate((e) => `${e.dataset.seat}/${e.dataset.min}: ${e.innerText.replace(/\n+/g, ' / ')}`);
      const before = await st();
      await b.click(); await page.waitForTimeout(400);
      const after = await st();
      console.log('APPOINT one click on', what, '\n  before', JSON.stringify(before), '\n  after ', JSON.stringify(after), '\n  asked first?', after.pc === before.pc ? 'yes or refused' : 'NO, done at once');
      console.log('  buttons now:', await btnList());
      const seat = page.locator('.gvseat').first(); console.log('  first seat now:', flat(await seat.innerText(), 600));
      await page.screenshot({ path: `shots/${shot}-4-appointed.png` });
      const news = await page.evaluate(() => window.theater.game().headlines.slice(-2).map((h) => h.text));
      console.log('  last headlines:', JSON.stringify(news));
    } else console.log('no appointment can be made now');
  }
  if (steps.includes('decree')) {
    const b = page.locator('.body.gov [data-act="govdecree"]:not([disabled])').first();
    if (await b.count()) {
      const what = await b.evaluate((e) => `${e.dataset.track}/${e.dataset.step}: ${e.innerText}`);
      const before = await st();
      await b.scrollIntoViewIfNeeded(); await b.click(); await page.waitForTimeout(400);
      const after = await st();
      console.log('DECREE one click on', what, '\n  before', JSON.stringify(before), '\n  after ', JSON.stringify(after));
      await page.screenshot({ path: `shots/${shot}-5-decreed.png` });
    } else { console.log('no decree can be made now; why:', flat((await page.locator('.gvtrack .whyno').allInnerTexts()).join(' ; '), 500)); }
  }
  if (steps.includes('show')) {
    const b = page.locator('.body.gov [data-act="govshow"]').first();
    if (await b.count()) {
      const id = await b.getAttribute('data-id');
      await b.click(); await page.waitForTimeout(900);
      const open = await page.evaluate(() => !!document.querySelector('.body.gov'));
      const panel = await page.evaluate(() => { const p = document.querySelector('#panel, .panel, aside'); return p ? p.innerText.slice(0, 200).replace(/\n+/g, ' / ') : ''; });
      console.log('SHOW on the map: region', id, 'card still open?', open, '/ panel:', panel);
      await page.screenshot({ path: `shots/${shot}-6-shown.png` });
    } else console.log('no Show it on the map button');
  }
  console.log('page errors:', errors.length, errors.slice(0, 3).join(' | ').slice(0, 400));
  await browser.close();
})();
