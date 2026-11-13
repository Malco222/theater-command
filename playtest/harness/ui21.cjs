// Version 59/60: the start card's cabinet picker, a campaign begun with no cabinet, and the promise button on a province.
// usage: [TC_PAGE=...] ui21.cjs SHOT-PREFIX [SAVE-WITH-A-PROMISE-ON-OFFER.json]
const { chromium } = require('playwright');
const fs = require('fs');
const [shot, save] = process.argv.slice(2);
const flat = (s, n = 1400) => String(s).replace(/\n+/g, ' / ').replace(/\s+/g, ' ').slice(0, n);
(async () => {
  const browser = await chromium.launch();
  const errors = [];
  const open = async (init) => { const page = await browser.newPage({ viewport: { width: 1440, height: 900 }, deviceScaleFactor: 2 }); page.on('pageerror', (e) => errors.push(e.message)); await page.addInitScript(init.fn, init.arg); await page.goto(process.env.TC_PAGE || 'file:///home/user/tc60/dist/play.html'); await page.waitForTimeout(1200); return page; };
  // 1. the start card
  let page = await open({ fn: () => { try { localStorage.setItem('tc-watch', '0'); } catch {} }, arg: null });
  await page.locator('[data-act="gcnat"][data-nat="GER"]').first().click(); await page.waitForTimeout(400);
  const picker = page.locator('[data-act="gccabinet"]');
  console.log('START CARD cabinet picker buttons:', await picker.evaluateAll((bs) => bs.map((b) => `${b.textContent.trim()}${b.classList.contains('on') ? ' [ON]' : ''}`)));
  const note = async () => flat(await page.locator('.gclevelnote').last().innerText());
  console.log('note (with):', await note());
  await picker.first().scrollIntoViewIfNeeded(); await page.screenshot({ path: `${shot}-1-start-card.png` });
  await page.locator('[data-act="gccabinet"][data-on="0"]').click(); await page.waitForTimeout(300);
  console.log('note (without):', await note());
  await page.locator('[data-act="gccabinet"]').first().scrollIntoViewIfNeeded(); await page.screenshot({ path: `${shot}-2-start-card-no-cabinet.png` });
  await page.locator('[data-act="start"]').first().click(); await page.waitForTimeout(2500);
  const state = await page.evaluate(() => { const g = window.theater.game(); return { turn: g.turn, human: g.human, nogov: g.flags.nogov ?? 0, gov: !!g.nations[g.human].gov }; });
  console.log('started:', JSON.stringify(state));
  const bar = await page.locator('button, .btn').evaluateAll((bs) => bs.map((b) => b.textContent.trim()).filter((t) => /cabinet/i.test(t)).slice(0, 6));
  console.log('buttons that name the cabinet with it off:', JSON.stringify(bar));
  await page.screenshot({ path: `${shot}-3-started-no-cabinet.png` });
  await page.close();
  // 2. the promise button on a province
  if (save) {
    page = await open({ fn: (s) => { try { localStorage.setItem('tc-watch', '0'); localStorage.setItem('theater-command-save-v1', s); } catch {} }, arg: fs.readFileSync(save, 'utf8') });
    const cont = page.locator('[data-act="continue"]').first();
    if (await cont.count()) await cont.click(); else await page.evaluate(() => window.theater.continueGame());
    await page.waitForTimeout(1500);
    const targets = await page.evaluate(() => { const g = window.theater.game(); const d = g.nations[g.human].gov; return { turn: g.turn, vow: d && d.vow ? d.vow.title : null }; });
    console.log('save:', JSON.stringify(targets));
    // walk the enemy provinces beside the player's ground until one offers the button
    const ids = await page.evaluate(() => { const g = window.theater.game(); return g.regions.filter((r) => r.ctrl !== g.human && r.nb.some((j) => g.regions[j].ctrl === g.human)).map((r) => r.id); });
    let found = -1;
    for (const id of ids) { await page.evaluate((i) => window.theater.onRegion(i), id); await page.waitForTimeout(120); if (await page.locator('.vowbtn').count()) { found = id; break; } }
    console.log('provinces beside mine:', ids.length, 'first with a promise button:', found);
    if (found >= 0) {
      const b = page.locator('.vowbtn').first();
      console.log('button:', flat(await b.innerText()), '| tip:', await b.getAttribute('title'));
      await b.scrollIntoViewIfNeeded(); await page.screenshot({ path: `${shot}-4-map-promise-button.png` });
      await b.click(); await page.waitForTimeout(500);
      console.log('after the click, the card shows:', flat(await page.locator('#card').first().innerText(), 1200));
      const rows = await page.locator('.vowrow').count(), opts = await page.locator('.vowopt').evaluateAll((bs) => bs.map((b) => b.textContent.trim().replace(/\s+/g, ' ')).slice(0, 8));
      console.log('rows', rows, 'options', JSON.stringify(opts));
      await page.screenshot({ path: `${shot}-5-map-promise-card.png` });
    }
    await page.close();
  }
  console.log('page errors:', errors.length, errors.slice(0, 3).join(' | '));
  await browser.close();
})();
