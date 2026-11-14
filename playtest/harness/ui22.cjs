// Version 61: the attack card on a dug-in line: the Troops line, the single Dug in line, "Send everyone who can reach"
// with its odds, the card's length on a desk and on a phone, and what the battle report says about works worn.
// usage: ui22.cjs SAVE.json SAVE.txt SHOT-PREFIX
const { chromium } = require('playwright');
const fs = require('fs');
const [file, infoFile, shot] = process.argv.slice(2);
const info = JSON.parse(fs.readFileSync(infoFile, 'utf8'));
const flat = (s, n = 1800) => s.replace(/\n+/g, ' / ').replace(/\s+/g, ' ').slice(0, n);
(async () => {
  const browser = await chromium.launch();
  for (const [name, vp, dsf] of [['desk', { width: 1440, height: 900 }, 2], ['phone', { width: 390, height: 844 }, 2]]) {
    const page = await browser.newPage({ viewport: vp, deviceScaleFactor: dsf, isMobile: name === 'phone', hasTouch: name === 'phone' });
    const errors = [];
    page.on('pageerror', (e) => errors.push(e.message));
    page.on('console', (m) => { if (m.type() === 'error') errors.push('console: ' + m.text()); });
    const save = fs.readFileSync(file, 'utf8');
    await page.addInitScript((s) => { try { localStorage.setItem('tc-watch', '0'); localStorage.setItem('theater-command-save-v1', s); } catch {} }, save);
    await page.goto(process.env.TC_PAGE || 'file:///home/user/tc61/dist/play.html');
    await page.waitForTimeout(1000);
    const cont = page.locator('[data-act="continue"]').first();
    if (await cont.count()) { await cont.click(); } else console.log('continue (no card):', await page.evaluate(() => window.theater.continueGame()));
    await page.waitForTimeout(1500);
    console.log(`\n===== ${name} ${vp.width}x${vp.height} =====`);
    await page.evaluate((id) => window.theater.onRegion(id), info.from); await page.waitForTimeout(400);
    await page.evaluate((id) => window.theater.onRegion(id), info.target); await page.waitForTimeout(600);
    const card = page.locator('#card').first();
    const bal = page.locator('#card .balance').first();
    if (!(await bal.count())) { console.log('NO ATTACK CARD:', flat(await card.innerText(), 600)); await page.close(); continue; }
    console.log('AS IT OPENS verdict:', flat(await bal.innerText(), 400));
    console.log('corps offered:', await page.locator('#card [data-act="atkpick"]').count(), 'picked:', await page.locator('#card [data-act="atkpick"].pick').count());
    const mass = page.locator('#card .massnote').first();
    console.log('MASS NOTE:', await mass.count() ? flat(await mass.innerText(), 500) : 'NONE');
    const lines = async () => page.locator('#card .ticket tr, #card .tline, #card .ticket li, #card .ticket .trow').evaluateAll((rs) => rs.map((r) => r.innerText.replace(/\s+/g, ' ').trim()));
    let ls = await lines();
    console.log('ticket lines (' + ls.length + '):'); for (const l of ls) console.log('   ', l.slice(0, 360));
    const dims0 = await card.evaluate((e) => ({ scroll: e.scrollHeight, client: e.clientHeight, w: e.clientWidth }));
    console.log('card height as it opens:', JSON.stringify(dims0), 'screens', (dims0.scroll / Math.max(1, dims0.client)).toFixed(2), '| page wider than the screen:', await page.evaluate(() => document.documentElement.scrollWidth > window.innerWidth + 2));
    await page.screenshot({ path: `shots/${shot}-${name}-1-card.png` });
    const btn = page.locator('#card [data-act="atkall"]').first();
    if (await btn.count() && !process.env.JAB) {
      const box = await btn.boundingBox();
      console.log('send-everyone button:', flat(await btn.innerText(), 80), '| size', JSON.stringify(box), '| title:', await btn.getAttribute('title'));
      await btn.scrollIntoViewIfNeeded(); await page.screenshot({ path: `shots/${shot}-${name}-2-massnote.png` });
      await btn.click(); await page.waitForTimeout(500);
      console.log('AFTER SEND EVERYONE verdict:', flat(await page.locator('#card .balance').first().innerText(), 400));
      console.log('picked now:', await page.locator('#card [data-act="atkpick"].pick').count(), '| mass note still there:', await page.locator('#card .massnote').count());
      ls = await lines();
      for (const l of ls) if (/^(Troops|Dug in|Artillery|Forces|Frontage|Roads)/.test(l)) console.log('   ', l.slice(0, 420));
      await page.screenshot({ path: `shots/${shot}-${name}-3-everyone.png` });
    } else console.log(process.env.JAB ? 'JAB: ordering the attack with the corps picked as the card opened' : 'NO send-everyone button');
    const need = page.locator('.balance .need').first();
    console.log('white mark:', await need.count() ? JSON.stringify({ title: await need.getAttribute('title'), left: await need.evaluate((e) => e.style.left) }) : 'NONE');
    console.log('notes on the card:', flat((await page.locator('#card .note, #card .fhelp, #card .warn').allInnerTexts()).join(' ; '), 1200));
    for (const key of ['Troops', 'Dug in']) { const el = page.locator('#card .ticket').getByText(key, { exact: true }).first(); if (await el.count()) { await el.scrollIntoViewIfNeeded(); await page.waitForTimeout(150); await page.screenshot({ path: `shots/${shot}-${name}-4-${key.replace(' ', '')}.png` }); } else console.log('no ticket label', key); }
    const go = page.locator('#card [data-act="attack"]').first();
    if (await go.count()) {
      console.log('attack button:', flat(await go.innerText(), 200), await go.isDisabled() ? '[off]' : '');
      await go.click(); await page.waitForTimeout(900);
      const rep = await page.evaluate(() => { const g = window.theater.game(); const r = g.reports[g.reports.length - 1]; return r ? { where: r.regionName, odds: r.odds, card: r.aPower / r.dPower, taken: r.taken, siege: !!r.siege, prepared: r.prepared, worn: r.worn, works: r.works, attLost: r.attLost, defLost: r.defLost, attackers: (r.attackers || []).length } : null; });
      console.log('RESULT:', JSON.stringify(rep));
      console.log('WHAT THE PLAYER IS SHOWN:', flat(await page.evaluate(() => { const m = document.querySelector('#modal:not([hidden]), .modal.show, .battle, .report'); return (m || document.querySelector('#card')).innerText; }), 2600));
      await page.screenshot({ path: `shots/${shot}-${name}-5-result.png` });
      // the card again after the battle: do the works read lower?
      { const x = page.locator('#veil [data-act="close"]').first(); if (await x.count() && await x.isVisible()) await x.click(); else await page.keyboard.press('Escape'); await page.waitForTimeout(300); }
      await page.evaluate((id) => window.theater.onRegion(id), info.target); await page.waitForTimeout(600);
      const b2 = page.locator('#card .balance').first();
      if (await b2.count() && process.env.JAB) {
        console.log('SECOND LOOK as it opens:', flat(await b2.innerText(), 300), '| corps offered', await page.locator('#card [data-act="atkpick"]').count(), 'picked', await page.locator('#card [data-act="atkpick"].pick').count());
        const m2 = page.locator('#card .massnote').first(); console.log('SECOND LOOK mass note:', await m2.count() ? flat(await m2.innerText(), 400) : 'NONE');
        const b3 = page.locator('#card [data-act="atkall"]').first(); if (await b3.count()) { await b3.click(); await page.waitForTimeout(400); }
        console.log('SECOND LOOK, everyone:', flat(await page.locator('#card .balance').first().innerText(), 300));
        for (const l of await lines()) if (/^(DUG IN|Dug in|FORCES|Forces)/i.test(l)) console.log('   ', l.slice(0, 460));
        await page.screenshot({ path: `shots/${shot}-${name}-6-second-look.png` });
        const go2 = page.locator('#card [data-act="attack"]').first();
        if (await go2.count()) { await go2.click(); await page.waitForTimeout(900);
          console.log('SECOND RESULT:', JSON.stringify(await page.evaluate(() => { const g = window.theater.game(); const r = g.reports[g.reports.length - 1]; return r ? { where: r.regionName, odds: r.odds, card: r.aPower / r.dPower, taken: r.taken, prepared: r.prepared, worn: r.worn, works: r.works, attLost: r.attLost, defLost: r.defLost, attackers: (r.attackers || []).length } : null; })));
          console.log('SECOND REPORT:', flat(await page.evaluate(() => { const m = document.querySelector('#modal:not([hidden]), .modal.show, .battle, .report'); return (m || document.querySelector('#card')).innerText; }), 500)); }
      } else if (await b2.count()) { console.log('SECOND LOOK verdict:', flat(await b2.innerText(), 400)); for (const l of await lines()) if (/^(Dug in|Troops)/.test(l)) console.log('   ', l.slice(0, 420)); await page.screenshot({ path: `shots/${shot}-${name}-6-second-look.png` }); }
      else console.log('SECOND LOOK: no attack card;', flat(await card.innerText(), 500));
    }
    console.log('page errors:', errors.length, errors.slice(0, 3).join(' | '));
    await page.close();
  }
  await browser.close();
})();
