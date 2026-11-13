// Version 56: an older save (Poznan under siege, made on Version 55) loaded in the new page, and what the panel says
// about the places outside a town the player is besieging.
const { chromium } = require('playwright');
const fs = require('fs');
const out = process.argv[2] ?? 'shots';
(async () => {
  const browser = await chromium.launch();
  const page = await browser.newPage({ viewport: { width: 1440, height: 900 }, deviceScaleFactor: 1 });
  const errors = [];
  page.on('pageerror', (e) => errors.push(e.message));
  page.on('console', (m) => { if (m.type() === 'error') errors.push('console: ' + m.text()); });
  const save = fs.readFileSync(__dirname + '/out/siege.json', 'utf8');
  await page.addInitScript((s) => { try { localStorage.setItem('tc-watch', '0'); localStorage.setItem('theater-command-save-v1', s); } catch {} }, save);
  await page.goto(process.env.TC_PAGE || 'file:///home/user/theater-command/dist/play.html');
  await page.waitForTimeout(1000);
  console.log('continue:', await page.evaluate(() => window.theater.continueGame()));
  await page.waitForTimeout(1200);
  const st = await page.evaluate(() => { const g = window.theater.game(); return { turn: g.turn, human: g.human, regions: g.regions.length }; });
  console.log('loaded:', JSON.stringify(st));
  for (const name of ['Poznan']) {
    const id = await page.evaluate((n) => window.theater.game().regions.find((r) => r.name === n).id, name);
    await page.evaluate((i) => window.theater.onRegion(i), id);
    await page.waitForTimeout(500);
    const block = page.locator('.places').first();
    if (await block.count()) {
      console.log((await block.innerText()).replace(/\n+/g, ' / ').slice(0, 1500));
      const btns = await block.locator('button').evaluateAll((bs) => bs.map((b) => `${b.textContent.trim()}${b.disabled ? ' [disabled]' : ''} :: ${b.getAttribute('title') ?? ''}`));
      console.log('buttons: ' + btns.join(' | ').slice(0, 900));
      await block.scrollIntoViewIfNeeded();
      await page.screenshot({ path: `${out}/75-besieged-places-v56.png` });
    } else console.log('no places block at', name);
  }
  // end the month twice and look for news of prizes
  for (let i = 0; i < 2; i++) { await page.evaluate(() => window.theater.nextMonth()); await page.waitForTimeout(2500); for (let k = 0; k < 10; k++) { const b = page.locator('.card [data-act]').filter({ hasText: /close|continue|ok|to the map|dismiss|next/i }).first(); if (await b.count() && await b.isVisible().catch(() => false)) { await b.click().catch(() => {}); await page.waitForTimeout(300); } else break; } }
  const news = await page.evaluate(() => window.theater.game().headlines.filter((h) => /though the town holds|Scorched earth|wreck|blow up|plough/i.test(h.text)).map((h) => `t${h.turn}: ${h.text}`).slice(-8));
  console.log('prize news:\n  ' + news.join('\n  '));
  console.log('page errors:', errors.length, errors.slice(0, 3).join(' | ').slice(0, 400));
  await browser.close();
})();
