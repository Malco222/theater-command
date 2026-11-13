// Version 59: men, the cabinet switch and the directive's seal as a player meets them. Loads a save made by probe34 and reads:
// the Men gauge on the top bar and its tip, the alerts, the ledger's count of men, what the arsenal says of a new corps,
// the directive line and the seal on the map, the level card with the cabinet switch; then switches the cabinet off and on again.
// usage: [TC_PAGE=file:///path/dist/play.html] ui19.cjs SAVE.json SHOT-PREFIX
const { chromium } = require('playwright');
const fs = require('fs');
const [file, shot] = process.argv.slice(2);
const flat = (s, n = 1600) => String(s).replace(/\n+/g, ' / ').replace(/\s+/g, ' ').slice(0, n);
(async () => {
  const browser = await chromium.launch();
  const page = await browser.newPage({ viewport: { width: 1440, height: 900 }, deviceScaleFactor: 2 });
  const errors = [];
  page.on('pageerror', (e) => errors.push(e.message));
  page.on('console', (m) => { if (m.type() === 'error') errors.push('console: ' + m.text()); });
  const save = fs.readFileSync(file, 'utf8');
  await page.addInitScript((s) => { try { localStorage.setItem('tc-watch', '0'); localStorage.setItem('theater-command-save-v1', s); } catch {} }, save);
  await page.goto(process.env.TC_PAGE || 'file:///home/user/tc59/dist/play.html');
  await page.waitForTimeout(1000);
  const cont = page.locator('[data-act="continue"]').first();
  if (await cont.count()) await cont.click(); else console.log('continue (no card):', await page.evaluate(() => window.theater.continueGame()));
  await page.waitForTimeout(1500);
  const st = async () => page.evaluate(() => { const g = window.theater.game(); const n = g.nations[g.human]; return { turn: g.turn, human: g.human, nogov: g.flags.nogov ?? 0, pc: Math.round(n.pc * 10) / 10, trust: n.gov ? Math.round(n.gov.trust * 10) / 10 : null, dir: n.gov && n.gov.dir ? `${n.gov.dir.kind} ${n.gov.dir.region ?? ''}` : null, missed: n.gov ? n.gov.missed : null }; });
  const alerts = async () => [...new Set(await page.locator('button.alert').evaluateAll((bs) => bs.map((b) => `[${b.dataset.kind}] ${b.textContent.trim()}`)))];
  const close = async () => { await page.keyboard.press('Escape'); await page.waitForTimeout(250); };
  console.log('loaded:', JSON.stringify(await st()));
  // 1. the top bar
  const gauge = page.locator('.purse.store.men').first();
  console.log('MEN GAUGE:', await gauge.count() ? JSON.stringify({ text: flat(await gauge.innerText()), cls: await gauge.getAttribute('class'), tip: await gauge.getAttribute('title') }) : 'NONE');
  const cab = page.locator('[data-act="cabinet"]:not(.dirline)').first();
  console.log('CABINET BUTTON:', await cab.count() ? JSON.stringify({ text: flat(await cab.innerText()), cls: await cab.getAttribute('class'), tip: await cab.getAttribute('title') }) : 'NONE');
  console.log('ALERTS:\n  ' + (await alerts()).join('\n  '));
  const dl = page.locator('.dirline').first();
  console.log('DIRECTIVE LINE:', await dl.count() ? flat(await dl.innerText()) : 'none');
  const seals = await page.evaluate(() => [...document.querySelectorAll('.dseal')].map((e) => ({ tag: e.tagName, parent: e.parentElement?.id || e.parentElement?.className?.baseVal || e.parentElement?.className || '', text: (e.textContent || '').trim().slice(0, 40), box: (() => { const b = e.getBoundingClientRect(); return [Math.round(b.x), Math.round(b.y), Math.round(b.width), Math.round(b.height)]; })(), title: e.querySelector?.('title')?.textContent || e.getAttribute('title') || '' })));
  console.log('SEALS:', JSON.stringify(seals).slice(0, 900));
  await page.screenshot({ path: `shots/${shot}-1-map.png` });
  // 2. the ledger's count of men
  if (await gauge.count()) {
    await gauge.click(); await page.waitForTimeout(500);
    const men = await page.evaluate(() => { const h = document.querySelector('#lmen'); if (!h) return null; let t = h.textContent.trim(), e = h.nextElementSibling, n = 0; while (e && e.tagName !== 'H3' && n++ < 8) { t += '\n' + e.innerText; e = e.nextElementSibling; } h.scrollIntoView({ block: 'center' }); return t; });
    console.log('LEDGER, MEN:', men ? flat(men, 1800) : 'NO #lmen BLOCK');
    await page.waitForTimeout(200); await page.screenshot({ path: `shots/${shot}-2-ledger-men.png` });
    await close();
  }
  // 3. the arsenal: what a new corps says about men
  const rc = await page.evaluate(() => { const g = window.theater.game(); const cap = g.nations[g.human].capital; window.theater.onRegion(cap); return cap; });
  await page.waitForTimeout(500);
  const arsBtn = page.locator('[data-act="tab"][data-tab="arsenal"], [data-act="arsenalopen"], [data-act="recruit"], [data-act="build"]').first();
  if (await arsBtn.count()) { await arsBtn.click(); await page.waitForTimeout(400); }
  const cards = await page.locator('.rcard').evaluateAll((bs) => bs.filter((b) => /corps:/.test(b.dataset.key || '')).slice(0, 5).map((b) => `${b.dataset.key}${b.disabled ? ' [off]' : ''}: ${b.innerText.replace(/\n+/g, ' / ').slice(0, 260)} || tip: ${(b.getAttribute('title') || '').slice(0, 300)}`));
  console.log('ARSENAL corps cards (' + cards.length + '):\n  ' + (cards.join('\n  ') || 'none on screen; buttons with data-act: ' + (await page.evaluate(() => [...new Set([...document.querySelectorAll('[data-act]')].filter((b) => b.offsetParent !== null).map((b) => b.dataset.act + (b.dataset.tab ? ':' + b.dataset.tab : '')))].join(' ')))));
  if (cards.length) { await page.locator('.rcard').first().scrollIntoViewIfNeeded(); await page.screenshot({ path: `shots/${shot}-3-arsenal.png` }); }
  await close();
  // 4. the level card and the cabinet switch
  const rank = page.locator('[data-act="level"]').first();
  console.log('LEVEL BADGE:', await rank.count() ? JSON.stringify({ text: flat(await rank.innerText()), tip: await rank.getAttribute('title') }) : 'NONE');
  if (await rank.count()) {
    await rank.click(); await page.waitForTimeout(500);
    const part = async () => page.evaluate(() => { const h = document.querySelector('#setcab'); if (!h) return null; let t = h.textContent.trim(), e = h.nextElementSibling, n = 0; while (e && n++ < 3) { t += '\n' + e.innerText; e = e.nextElementSibling; } h.scrollIntoView({ block: 'center' }); return t; });
    console.log('LEVEL CARD lead:', flat(await page.locator('#card .lead').first().innerText().catch(() => ''), 500));
    console.log('CABINET SWITCH:', flat(await part() || 'NO #setcab', 900));
    await page.waitForTimeout(200); await page.screenshot({ path: `shots/${shot}-4-switch.png` });
    const before = await st();
    await page.locator('[data-act="setcabinet"][data-on="0"]').first().click(); await page.waitForTimeout(500);
    console.log('SWITCHED OFF: before', JSON.stringify(before), 'after', JSON.stringify(await st()));
    console.log('  switch now:', flat(await part() || '', 900));
    await page.screenshot({ path: `shots/${shot}-5-switched-off.png` });
    await close();
    console.log('  cabinet button now:', await page.locator('[data-act="cabinet"]').count(), '| directive line:', await page.locator('.dirline').count(), '| seals:', await page.locator('.dseal').count());
    console.log('  alerts now:\n    ' + (await alerts()).join('\n    '));
    await page.screenshot({ path: `shots/${shot}-6-map-no-cabinet.png` });
    await rank.click().catch(() => {}); await page.waitForTimeout(400);
    const on = page.locator('[data-act="setcabinet"][data-on="1"]').first();
    if (await on.count()) { await on.click(); await page.waitForTimeout(500); console.log('SWITCHED ON AGAIN:', JSON.stringify(await st())); await close(); }
    console.log('  cabinet button now:', await page.locator('[data-act="cabinet"]:not(.dirline)').count(), '| alerts:\n    ' + (await alerts()).join('\n    '));
  }
  console.log('page errors:', errors.length, errors.slice(0, 5).join(' | '));
  await browser.close();
})();
