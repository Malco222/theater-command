// Version 67: does every row of the Build panel that cannot be ordered say why? Loads a save, opens the Build tab and
// walks its sections (army, air force, navy, works, towns) on a desk and a phone: for every greyed row, the words under
// it (.whyno) and its tooltip. Also tries the tray's fold button on the phone.
// usage: [TC_PAGE=...] ui31.cjs SAVE.json SHOT-PREFIX
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
    await page.goto(process.env.TC_PAGE || 'file:///home/user/tc67/dist/play.html'); await page.waitForTimeout(1000);
    const cont = page.locator('[data-act="continue"]').first();
    if (await cont.count()) await cont.click(); else await page.evaluate(() => window.theater.continueGame());
    await page.waitForTimeout(1500);
    for (let k = 0; k < 8; k++) { const open = await page.evaluate(() => { const v = document.querySelector('#veil'); if (!v || !v.classList.contains('open')) return false; window.theater.openModal(null); return true; }); if (!open) break; await page.waitForTimeout(200); }
    say('top bar:', flat(await page.locator('.bar').first().innerText().catch(() => ''), 400));
    const tab = page.locator('#tabs [data-act="tab"][data-tab="build"]').first();
    if (!(await tab.count()) || !(await tab.isVisible())) {
      // A staffed desk tucks its tab away: look in from the Staff tab.
      await page.locator('#tabs [data-act="tab"][data-tab="staff"]').first().click(); await page.waitForTimeout(400);
      await page.locator('#pane .desk', { hasText: 'Building and trade' }).first().locator('[data-act="tab"]').first().click();
    } else await tab.click();
    await page.waitForTimeout(600);
    const sections = await page.locator('#pane [data-act="arsenaltab"], #pane .segs button, #pane .seg button').evaluateAll((bs) => bs.filter((b) => b.offsetParent !== null).map((b) => b.textContent.trim()));
    say('sections:', JSON.stringify(sections));
    const read = () => page.evaluate(() => {
      const rows = [...document.querySelectorAll('#pane button.buy, #pane .rcard, #pane button.offer')].filter((e) => e.offsetParent !== null);
      const out = [];
      for (const e of rows) {
        const grey = e.disabled || e.classList.contains('off') || e.getAttribute('aria-disabled') === 'true';
        const why = e.querySelector('.whyno')?.textContent?.trim() ?? '';
        const label = (e.querySelector('b')?.textContent ?? e.textContent).trim().replace(/\s+/g, ' ').slice(0, 50);
        out.push({ label, grey, why: why.slice(0, 140), tip: (e.getAttribute('title') ?? '').slice(0, 90), kind: e.className.split(' ').slice(0, 3).join('.') });
      }
      return out;
    });
    const walk = async (label) => {
      const rows = await read();
      const grey = rows.filter((r) => r.grey);
      say(`${label}: rows ${rows.length}, greyed ${grey.length}, greyed with no words ${grey.filter((r) => !r.why).length}, greyed with neither words nor tooltip ${grey.filter((r) => !r.why && !r.tip).length}`);
      for (const r of grey) say(`    ${r.why ? 'says' : 'SILENT'} | ${r.label} | ${r.why || r.tip || '-'} | ${r.kind}`);
    };
    await walk('as opened');
    await page.screenshot({ path: `${shot}-${name}-1-build.png` });
    for (const s of sections) {
      const b = page.locator('#pane button', { hasText: s }).first();
      if (!(await b.count())) continue;
      await b.click().catch(() => {}); await page.waitForTimeout(400);
      await walk(`section ${s}`);
      await page.screenshot({ path: `${shot}-${name}-2-${s.toLowerCase().replace(/[^a-z]+/g, '-')}.png` });
    }
    if (touch) {
      // The tray's fold: pick the biggest army, read the folded tray, open it, read it again.
      const reg = await page.evaluate(() => { const g = window.theater.game(); let best = -1, n = 0; for (const r of g.regions) { const k = r.units.filter((u) => u.nat === g.human).length; if (k > n) { n = k; best = r.id; } } return best; });
      await page.evaluate((id) => window.theater.onRegion(id), reg); await page.waitForTimeout(700);
      const box = async () => page.evaluate(() => { const t = document.querySelector('#tray'); const f = t?.querySelector('.tfold'); const r = t?.getBoundingClientRect(); const fr = f?.getBoundingClientRect(); return { open: t?.classList.contains('open'), slim: t?.classList.contains('slim'), h: r ? Math.round(r.height) : 0, fold: fr ? `${Math.round(fr.width)} by ${Math.round(fr.height)}` : 'none', text: f?.textContent?.trim() ?? '' }; });
      say('tray folded:', JSON.stringify(await box()), '|', flat(await page.locator('#tray').innerText().catch(() => ''), 200));
      await page.screenshot({ path: `${shot}-${name}-3-tray-folded.png` });
      const f = page.locator('#tray .tfold').first();
      if (await f.count()) { await f.tap().catch(async () => { await f.click(); }); await page.waitForTimeout(500); }
      say('tray opened:', JSON.stringify(await box()), '|', flat(await page.locator('#tray').innerText().catch(() => ''), 300));
      await page.screenshot({ path: `${shot}-${name}-4-tray-open.png` });
    }
    say('page errors:', errors.length ? JSON.stringify(errors.slice(0, 4)) : 'none');
    await page.close();
  }
  await browser.close();
})();
