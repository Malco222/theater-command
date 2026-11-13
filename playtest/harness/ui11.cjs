// Version 56: the "Places of their own" block of the province panel, the wreck buttons and the scorched earth order.
// usage: [TC_PAGE=...] node ui11.cjs SHOTS_DIR [REGION,REGION]
const { chromium } = require('playwright');
const out = process.argv[2] ?? 'shots';
const names = (process.argv[3] ?? 'Ruhr,Hamburg,Upper Silesia,Berlin').split(',');
(async () => {
  const browser = await chromium.launch();
  const page = await browser.newPage({ viewport: { width: 1440, height: 900 }, deviceScaleFactor: 1 });
  const errors = [];
  page.on('pageerror', (e) => errors.push(e.message));
  await page.addInitScript(() => { try { localStorage.setItem('tc-watch', '0'); } catch {} });
  await page.goto(process.env.TC_PAGE || 'file:///home/user/theater-command/dist/play.html');
  await page.waitForTimeout(800);
  await page.locator('[data-act="start"]').click();
  await page.waitForTimeout(2500);
  // close any opening cards
  for (let i = 0; i < 8; i++) { const b = page.locator('.card [data-act]').filter({ hasText: /close|continue|begin|ok|to the map|dismiss/i }).first(); if (await b.count() && await b.isVisible().catch(() => false)) { console.log('closing card:', (await b.innerText()).slice(0, 40)); await b.click().catch(() => {}); await page.waitForTimeout(400); } else break; }
  const all = await page.evaluate(() => window.theater.game().regions.filter((r) => r.ctrl === 'GER').map((r) => r.name));
  console.log('German provinces:', all.length);
  let k = 0;
  for (const name of names) {
    const id = await page.evaluate((n) => window.theater.game().regions.find((r) => r.name === n)?.id ?? -1, name);
    if (id < 0) { console.log(name, 'not found'); continue; }
    await page.evaluate((i) => window.theater.onRegion(i), id);
    await page.waitForTimeout(500);
    const block = page.locator('.places').first();
    const has = await block.count();
    console.log(`--- ${name} (region ${id}): places block ${has ? 'shown' : 'absent'}`);
    if (has) {
      console.log((await block.innerText()).replace(/\n+/g, '\n').slice(0, 1800));
      const btns = await block.locator('button').evaluateAll((bs) => bs.map((b) => `${b.textContent.trim()}${b.disabled ? ' [disabled]' : ''} :: ${b.getAttribute('title') ?? ''}`));
      console.log('buttons:\n  ' + btns.join('\n  '));
      await block.scrollIntoViewIfNeeded();
      await page.screenshot({ path: `${out}/7${k}-places-${name.replace(/\W+/g, '-')}-v56.png` });
      const bb = await block.boundingBox();
      console.log('block box', JSON.stringify(bb));
      k++;
    }
  }
  console.log('page errors:', errors.length, errors.slice(0, 3).join(' | '));
  await browser.close();
})();
