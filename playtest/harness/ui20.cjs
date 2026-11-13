// Version 60: the cabinet that talks back, as a player meets it. Loads a save made by probe34 and reads, in the Cabinet card:
// the hawks and doves block, the promise block (the list of towns, the terms, giving one's word), the head the cabinet asks for
// (dismiss or stand by, each asked twice) and the general stood by.   usage: [TC_PAGE=...] ui20.cjs SAVE.json SHOT-PREFIX [promise,head:stand|head:sack,back]
const { chromium } = require('playwright');
const fs = require('fs');
const [file, shot, stepsS] = process.argv.slice(2);
const steps = (stepsS ?? 'promise,head:stand').split(',');
const flat = (s, n = 1600) => String(s).replace(/\n+/g, ' / ').replace(/\s+/g, ' ').slice(0, n);
(async () => {
  const browser = await chromium.launch();
  const page = await browser.newPage({ viewport: { width: 1440, height: 900 }, deviceScaleFactor: 2 });
  const errors = [];
  page.on('pageerror', (e) => errors.push(e.message));
  page.on('console', (m) => { if (m.type() === 'error') errors.push('console: ' + m.text()); });
  await page.addInitScript((s) => { try { localStorage.setItem('tc-watch', '0'); localStorage.setItem('theater-command-save-v1', s); } catch {} }, fs.readFileSync(file, 'utf8'));
  await page.goto(process.env.TC_PAGE || 'file:///home/user/tc60/dist/play.html');
  await page.waitForTimeout(1000);
  const cont = page.locator('[data-act="continue"]').first();
  if (await cont.count()) await cont.click(); else await page.evaluate(() => window.theater.continueGame());
  await page.waitForTimeout(1500);
  const st = async () => page.evaluate(() => { const g = window.theater.game(); const n = g.nations[g.human], v = n.gov || {}; return { turn: g.turn, human: g.human, pc: Math.round(n.pc * 10) / 10, trust: Math.round((v.trust ?? 0) * 10) / 10, vow: v.vow ? `${v.vow.title} (${v.vow.band})` : null, head: v.head ? v.head.general : null, stood: v.stood ? v.stood.general : null, kept: v.kept ?? 0, broken: v.broken ?? 0, wing: v.wing ?? null }; });
  const block = async (sel) => { const e = page.locator(sel).first(); return (await e.count()) ? flat(await e.innerText(), 2200) : 'NONE'; };
  console.log('loaded:', JSON.stringify(await st()));
  const alerts = [...new Set(await page.locator('button.alert').evaluateAll((bs) => bs.map((b) => `[${b.dataset.kind}] ${b.textContent.trim()}`)))];
  console.log('ALERTS:\n  ' + alerts.join('\n  '));
  const cab = page.locator('[data-act="cabinet"]:not(.dirline)').first();
  console.log('CABINET BUTTON:', await cab.count() ? JSON.stringify({ cls: await cab.getAttribute('class'), tip: await cab.getAttribute('title') }) : 'NONE');
  await page.screenshot({ path: `shots/${shot}-1-map.png` });
  await cab.click(); await page.waitForTimeout(500);
  const card = page.locator('.body.gov').first();
  const dims = await card.evaluate((e) => ({ scroll: e.scrollHeight, client: e.clientHeight }));
  console.log('card height:', JSON.stringify(dims), 'window 900: screens', (dims.scroll / 900).toFixed(2));
  console.log('WINGS:', await block('.gvwings'));
  console.log('PROMISE BLOCK:', await block('#gvvow'));
  console.log('HEAD BLOCK:', await block('#gvhead'));
  console.log('STOOD BY:', await block('.gvstood'));
  console.log('SEATS (first 700):', (await block('.gvseats')).slice(0, 700));
  await page.screenshot({ path: `shots/${shot}-2-cabinet.png` });
  for (const id of ['#gvvow', '#gvhead']) { const e = page.locator(id).first(); if (await e.count()) { await e.scrollIntoViewIfNeeded(); await page.waitForTimeout(150); await page.screenshot({ path: `shots/${shot}-3${id.replace('#', '-')}.png` }); } }
  if (steps.includes('promise')) {
    const go = page.locator('.body.gov [data-act="govpromise"]:not([data-id])').first();
    if (await go.count() && !(await go.isDisabled())) {
      await go.click(); await page.waitForTimeout(400);
      const rows = await page.locator('.vowrow').evaluateAll((rs) => rs.map((r) => r.innerText.replace(/\n+/g, ' / ') + ' || ' + [...r.querySelectorAll('.vowopt')].map((b) => `${b.className.replace('vowopt ', '')}:${b.dataset.months}mo "${(b.getAttribute('title') || '').slice(0, 160)}"`).join(' ; ')));
      console.log('PROMISE LIST (' + rows.length + ' towns):\n  ' + rows.join('\n  ').slice(0, 4200));
      await page.locator('#gvvow').first().scrollIntoViewIfNeeded(); await page.screenshot({ path: `shots/${shot}-4-promise-list.png` });
      const opt = page.locator('.vowopt').first();
      if (await opt.count()) {
        const what = await opt.evaluate((b) => `${b.className} ${b.dataset.id}/${b.dataset.months}`);
        await opt.click(); await page.waitForTimeout(400);
        console.log('PICKED', what, '| after one click:', JSON.stringify(await st()));
        console.log('TERMS:', await block('#gvvow'));
        await page.locator('#gvvow').first().scrollIntoViewIfNeeded(); await page.screenshot({ path: `shots/${shot}-5-promise-terms.png` });
        const yes = page.locator('[data-act="govpromise"][data-sure="1"]').first();
        if (await yes.count()) { await yes.click(); await page.waitForTimeout(400); console.log('WORD GIVEN:', JSON.stringify(await st())); console.log('PROMISE BLOCK NOW:', await block('#gvvow')); await page.screenshot({ path: `shots/${shot}-6-promise-made.png` }); }
      }
    } else console.log('no promise can be made here:', await block('#gvvow'));
  }
  if (steps.includes('back')) {
    const b = page.locator('[data-act="govback"]:not([data-sure])').first();
    if (await b.count() && !(await b.isDisabled())) { await b.click(); await page.waitForTimeout(300); console.log('TAKE BACK asks:', await block('#gvvow')); const y = page.locator('[data-act="govback"][data-sure="1"]').first(); if (await y.count()) { await y.click(); await page.waitForTimeout(300); console.log('TAKEN BACK:', JSON.stringify(await st()), '|', await block('#gvvow')); } }
    else console.log('no promise to take back');
  }
  const hs = steps.find((s) => s.startsWith('head:'));
  if (hs) {
    const how = hs.split(':')[1];
    const b = page.locator(`[data-act="govhead"][data-how="${how}"]:not([data-sure])`).first();
    if (await b.count() && !(await b.isDisabled())) {
      const before = await st();
      await b.click(); await page.waitForTimeout(300);
      console.log(`HEAD, one click on ${how}:`, JSON.stringify(await st()), '| asks:', await block('#gvhead'));
      await page.locator('#gvhead').first().scrollIntoViewIfNeeded(); await page.screenshot({ path: `shots/${shot}-7-head-ask.png` });
      const y = page.locator(`[data-act="govhead"][data-how="${how}"][data-sure="1"]`).first();
      if (await y.count()) { await y.click(); await page.waitForTimeout(400); console.log('HEAD ANSWERED: before', JSON.stringify(before), 'after', JSON.stringify(await st())); console.log('HEAD BLOCK NOW:', await block('#gvhead'), '| STOOD BY:', await block('.gvstood')); await page.screenshot({ path: `shots/${shot}-8-head-done.png` }); }
    } else console.log('the cabinet asks for no head here');
  }
  console.log('page errors:', errors.length, errors.slice(0, 5).join(' | '));
  await browser.close();
})();
