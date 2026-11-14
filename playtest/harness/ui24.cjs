// Version 62, the small screen faults of Version 60 checked again: the arsenal's tooltips (Build tab), and the office picker on
// the card that starts a new campaign. usage: [TC_PAGE=...] ui24.cjs SAVE.json SHOT-PREFIX
const { chromium } = require('playwright');
const fs = require('fs');
const [file, shot] = process.argv.slice(2);
const flat = (s, n = 900) => String(s).replace(/\n+/g, ' / ').replace(/\s+/g, ' ').slice(0, n);
(async () => {
  const browser = await chromium.launch();
  const page = await browser.newPage({ viewport: { width: 1440, height: 900 }, deviceScaleFactor: 2 });
  const errors = [];
  page.on('pageerror', (e) => errors.push(e.message));
  await page.addInitScript((s) => { try { localStorage.setItem('tc-watch', '0'); localStorage.setItem('theater-command-save-v1', s); } catch {} }, fs.readFileSync(file, 'utf8'));
  await page.goto(process.env.TC_PAGE || 'file:///home/user/tc62/dist/play.html');
  await page.waitForTimeout(1000);
  // the start card, before continuing
  const start = await page.evaluate(() => { const c = document.querySelector('#card'); return c ? c.innerText : ''; });
  console.log('START CARD:', flat(start, 700));
  const office = await page.locator('[data-act="setoffice"], #setoffice, [data-office]').evaluateAll((es) => es.map((e) => (e.innerText || e.getAttribute('data-secure') || '').trim()));
  console.log('START OFFICE CONTROLS:', JSON.stringify(office));
  const cont = page.locator('[data-act="continue"]').first();
  if (await cont.count()) await cont.click(); else await page.evaluate(() => window.theater.continueGame());
  await page.waitForTimeout(1500);
  await page.locator('#tabs [data-act="tab"][data-tab="build"]').click();
  await page.waitForTimeout(500);
  await page.evaluate(() => { const t = window.theater, g = t.game(); const r = g.regions.find((x) => x.name === 'Berlin'); t.ui.tab = 'build'; t.ui.sel = r.id; t.refresh(); });
  await page.waitForTimeout(400);
  console.log('PANE:', flat(await page.locator('#pane').innerText(), 500));
  console.log('ARM BUTTONS:', JSON.stringify(await page.locator('#pane [data-act]').evaluateAll((es) => [...new Set(es.map((e) => e.getAttribute('data-act') + (e.getAttribute('data-arm') ? ':' + e.getAttribute('data-arm') : '')))].slice(0, 30))));
  for (const arm of ['army', 'air', 'navy', 'works', 'towns']) {
    const b = page.locator(`#pane [data-act="arm"][data-arm="${arm}"]`).first();
    if (await b.count()) { await b.click(); await page.waitForTimeout(300); }
    const tips = await page.locator('#pane [data-act="arsenal"]').evaluateAll((bs) => bs.map((x) => x.getAttribute('title') || ''));
    console.log(`ARSENAL ${arm}: ${tips.length} cards, ${tips.filter((t) => !t.trim()).length} with an empty tooltip; first: ${flat(tips[0] ?? '', 300)}`);
  }
  await page.screenshot({ path: `shots/${shot}-arsenal.png` });
  console.log('page errors:', errors.length ? errors.join(' | ') : 'none');
  // a new campaign: is the office picker on the set-up card?
  const p2 = await browser.newPage({ viewport: { width: 1440, height: 900 }, deviceScaleFactor: 2 });
  await p2.goto(process.env.TC_PAGE || 'file:///home/user/tc62/dist/play.html');
  await p2.waitForTimeout(1200);
  const txt = await p2.evaluate(() => document.body.innerText);
  const m = txt.match(/[^\n]*office[^\n]*/gi) || [];
  console.log('NEW GAME, lines with "office":', JSON.stringify(m.slice(0, 8)));
  console.log('NEW GAME, setoffice controls:', await p2.locator('[data-act="setoffice"]').count());
  await p2.screenshot({ path: `shots/${shot}-newgame.png` });
  await browser.close();
})();
