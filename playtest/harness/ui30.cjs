// Version 66: a Field command save from late in a long war, on a desk and a phone: what the top bar says of the
// treasury and the men, what the Staff tab says of the building desk, and what the Build panel offers once looked in on.
// usage: [TC_PAGE=...] ui30.cjs SAVE.json SHOT-PREFIX
const { chromium } = require('playwright');
const fs = require('fs');
const [file, shot] = process.argv.slice(2);
const flat = (s, n = 1500) => String(s).replace(/\n+/g, ' / ').replace(/\s+/g, ' ').slice(0, n);
const SIZES = [['desk', { width: 1440, height: 900 }, false], ['phone', { width: 390, height: 844 }, true]];
(async () => {
  const browser = await chromium.launch();
  for (const [name, vp, touch] of SIZES) {
    const say = (...a) => console.log(`[${name}]`, ...a);
    const errors = [];
    const page = await browser.newPage({ viewport: vp, deviceScaleFactor: 2, isMobile: touch, hasTouch: touch });
    page.on('pageerror', (e) => errors.push(e.message));
    await page.addInitScript((s) => { try { localStorage.setItem('tc-watch', '0'); localStorage.setItem('theater-command-save-v1', s); } catch {} }, fs.readFileSync(file, 'utf8'));
    await page.goto(process.env.TC_PAGE || 'file:///home/user/tc66/dist/play.html'); await page.waitForTimeout(1000);
    const cont = page.locator('[data-act="continue"]').first();
    if (await cont.count()) await cont.click(); else await page.evaluate(() => window.theater.continueGame());
    await page.waitForTimeout(1500);
    for (let k = 0; k < 8; k++) { const open = await page.evaluate(() => { const v = document.querySelector('#veil'); if (!v || !v.classList.contains('open')) return false; window.theater.openModal(null); return true; }); if (!open) break; await page.waitForTimeout(150); }
    say('top bar:', flat(await page.locator('.bar').first().innerText().catch(() => ''), 500));
    say('tabs:', JSON.stringify(await page.locator('#tabs [data-act="tab"]').evaluateAll((bs) => bs.filter((b) => b.offsetParent !== null).map((b) => b.textContent.trim().replace(/\s+/g, ' ')))));
    await page.screenshot({ path: `${shot}-${name}-1-map.png` });
    await page.locator('#tabs [data-act="tab"][data-tab="staff"]').first().click(); await page.waitForTimeout(500);
    const rows = await page.locator('#pane .desk').evaluateAll((ds) => ds.map((d) => d.innerText.replace(/\n+/g, ' / ').slice(0, 520)));
    for (const r of rows) say('Staff tab row:', r);
    say('allowance:', flat(await page.locator('#pane .gclevelnote').last().innerText().catch(() => ''), 400));
    const b = page.locator('#pane .desk', { hasText: 'Building and trade' }).first();
    await b.scrollIntoViewIfNeeded(); await page.screenshot({ path: `${shot}-${name}-2-staff-building.png` });
    const look = b.locator('[data-act="tab"]').first();
    await look.click(); await page.waitForTimeout(600);
    say('looked in; the bar:', flat(await page.locator('#pane .deskbar').first().innerText().catch(() => 'NO BAR'), 500));
    const cards = await page.evaluate(() => { const pane = document.querySelector('#pane'); const all = [...pane.querySelectorAll('.rcard, [data-act="order"], [data-act="buy"], button')].filter((e) => e.offsetParent !== null); const out = []; for (const e of all) { const t = e.innerText.trim().replace(/\s+/g, ' ').slice(0, 110); if (!t) continue; out.push(`${e.disabled || e.classList.contains('off') || e.getAttribute('aria-disabled') === 'true' ? 'GREYED ' : ''}${t}${e.title ? ' {' + e.title.slice(0, 120) + '}' : ''}`); } return { long: pane.scrollHeight, items: out.slice(0, 60) }; });
    say('Build panel:', JSON.stringify(cards).slice(0, 5000));
    say('Build panel text:', flat(await page.locator('#pane').innerText(), 2200));
    await page.screenshot({ path: `${shot}-${name}-3-build-panel.png` });
    await page.locator('#pane').evaluate((p) => { p.scrollTop = 500; }); await page.waitForTimeout(200);
    await page.screenshot({ path: `${shot}-${name}-4-build-panel-down.png` });
    say('page errors:', errors.length ? JSON.stringify(errors.slice(0, 4)) : 'none');
    await page.close();
  }
  await browser.close();
})();
