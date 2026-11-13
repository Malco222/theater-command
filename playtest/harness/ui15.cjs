// Version 56: the besieger's view. Loads a save made by probe22 (the player holds the field outside a town with
// places of its own) and reads the province panel.   usage: ui15.cjs SAVE.json PROVINCE [SHOT-NAME]
const { chromium } = require('playwright');
const fs = require('fs');
const [file, name, shot] = process.argv.slice(2);
(async () => {
  const browser = await chromium.launch();
  const page = await browser.newPage({ viewport: { width: 1440, height: 900 }, deviceScaleFactor: 2 });
  const errors = [];
  page.on('pageerror', (e) => errors.push(e.message));
  page.on('console', (m) => { if (m.type() === 'error') errors.push('console: ' + m.text()); });
  const save = fs.readFileSync(file, 'utf8');
  await page.addInitScript((s) => { try { localStorage.setItem('tc-watch', '0'); localStorage.setItem('theater-command-save-v1', s); } catch {} }, save);
  await page.goto(process.env.TC_PAGE || 'file:///home/user/tc56/dist/play.html');
  await page.waitForTimeout(1000);
  const cont = page.locator('[data-act="continue"]').first();
  if (await cont.count()) { await cont.click(); console.log('continue: clicked the card'); } else console.log('continue (no card):', await page.evaluate(() => window.theater.continueGame()));
  await page.waitForTimeout(1500);
  console.log('loaded:', JSON.stringify(await page.evaluate(() => { const g = window.theater.game(); return { turn: g.turn, human: g.human }; })));
  const id = await page.evaluate((n) => window.theater.game().regions.find((r) => r.name === n).id, name);
  await page.evaluate((i) => window.theater.onRegion(i), id);
  await page.waitForTimeout(600);
  const block = page.locator('.places').first();
  if (await block.count()) {
    console.log((await block.innerText()).replace(/\n+/g, ' / ').slice(0, 2500));
    const btns = await block.locator('button').evaluateAll((bs) => bs.map((b) => `${b.textContent.trim()}${b.disabled ? ' [disabled]' : ''} :: ${b.getAttribute('title') ?? ''}`));
    console.log('buttons: ' + btns.join(' | ').slice(0, 1500));
    await block.scrollIntoViewIfNeeded();
    await page.waitForTimeout(300);
    await page.screenshot({ path: `shots/${shot ?? '75-besieger-places-v56'}.png` });
  } else console.log('no places block at', name);
  const news = await page.evaluate(() => window.theater.game().headlines.filter((h) => /though the town holds|Scorched earth/i.test(h.text)).map((h) => `t${h.turn}: ${h.text}`).slice(-6));
  console.log('prize news so far:\n  ' + news.join('\n  '));
  console.log('page errors:', errors.length, errors.slice(0, 3).join(' | ').slice(0, 400));
  await browser.close();
})();
