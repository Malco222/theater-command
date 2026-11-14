// Version 63: the attack card's new parts on a desk and a phone: the force it opens with and the way back to the pick,
// a wide band given as a best guess, the bill of losses before the order, the Battle Ticket folded on a phone, the
// lines Earlier attacks and Weight of attack, and the report's price in men.
// usage: ui25.cjs SAVE.json SAVE.txt SHOT-PREFIX [mine]   (mine: go back to the corps picked before attacking: a jab)
const { chromium } = require('playwright');
const fs = require('fs');
const [file, infoFile, shot, how] = process.argv.slice(2);
const info = JSON.parse(fs.readFileSync(infoFile, 'utf8'));
const flat = (s, n = 1800) => s.replace(/\n+/g, ' / ').replace(/\s+/g, ' ').slice(0, n);
(async () => {
  const browser = await chromium.launch();
  for (const [name, vp] of [['desk', { width: 1440, height: 900 }], ['phone', { width: 390, height: 844 }]]) {
    const page = await browser.newPage({ viewport: vp, deviceScaleFactor: 2, isMobile: name === 'phone', hasTouch: name === 'phone' });
    const errors = [];
    page.on('pageerror', (e) => errors.push(e.message));
    page.on('console', (m) => { if (m.type() === 'error') errors.push('console: ' + m.text()); });
    const save = fs.readFileSync(file, 'utf8');
    await page.addInitScript((s) => { try { localStorage.setItem('tc-watch', '0'); localStorage.setItem('theater-command-save-v1', s); } catch {} }, save);
    await page.goto(process.env.TC_PAGE || 'file:///home/user/tc63/dist/play.html');
    await page.waitForTimeout(1000);
    const cont = page.locator('[data-act="continue"]').first();
    if (await cont.count()) await cont.click(); else await page.evaluate(() => window.theater.continueGame());
    await page.waitForTimeout(1500);
    console.log(`\n===== ${name} ${vp.width}x${vp.height} =====`);
    await page.evaluate((id) => window.theater.onRegion(id), info.from); await page.waitForTimeout(400);
    await page.evaluate((id) => window.theater.onRegion(id), info.target); await page.waitForTimeout(600);
    const card = page.locator('#card').first();
    if (!(await page.locator('#card .balance').count())) { console.log('NO ATTACK CARD:', flat(await card.innerText(), 600)); await page.close(); continue; }
    const read = async (tag) => {
      console.log(`${tag} verdict:`, flat(await page.locator('#card .balance .bhead').first().innerText(), 400));
      const cost = page.locator('#card .costline');
      console.log(`${tag} bill:`, await cost.count() ? flat((await cost.allInnerTexts()).join(' ; '), 500) + ' | title: ' + await cost.first().getAttribute('title') : 'NONE');
      console.log(`${tag} corps offered:`, await page.locator('#card [data-act="atkpick"]').count(), 'picked:', await page.locator('#card [data-act="atkpick"].pick').count());
      const d = await card.evaluate((e) => ({ scroll: e.scrollHeight, client: e.clientHeight }));
      console.log(`${tag} card height: ${(d.scroll / Math.max(1, d.client)).toFixed(2)} screens`, JSON.stringify(d));
    };
    await read('AS IT OPENS');
    const open = page.locator('#card .opennote').first();
    console.log('opening note:', await open.count() ? flat(await open.innerText(), 500) : 'NONE');
    const fog = page.locator('#card .fogged').first();
    console.log('fog note:', await fog.count() ? flat(await fog.innerText(), 700) : 'NONE');
    const tick = async () => page.locator('#card .ticket tr').evaluateAll((rs) => rs.filter((r) => r.offsetParent !== null).map((r) => r.innerText.replace(/\s+/g, ' ').trim()));
    let ls = await tick();
    console.log(`ticket lines in view (${ls.length}):`); for (const l of ls) console.log('   ', l.slice(0, 330));
    await page.screenshot({ path: `shots/${shot}-${name}-1-opens.png` });
    const costEl = page.locator('#card .costline').first();
    if (await costEl.count()) { await costEl.scrollIntoViewIfNeeded(); await page.waitForTimeout(200); await page.screenshot({ path: `shots/${shot}-${name}-2-bill.png` }); }
    const all = page.locator('#card [data-act="ticketall"]').first();
    if (await all.count() && await all.isVisible()) {
      console.log('fold button:', flat(await all.innerText(), 80));
      await all.scrollIntoViewIfNeeded(); await page.waitForTimeout(150); await page.screenshot({ path: `shots/${shot}-${name}-3-folded.png` });
      await all.click(); await page.waitForTimeout(400);
      const d = await card.evaluate((e) => ({ scroll: e.scrollHeight, client: e.clientHeight }));
      console.log(`with every line shown: ${(d.scroll / Math.max(1, d.client)).toFixed(2)} screens; lines in view ${(await tick()).length}`);
    } else console.log('no fold button (the ticket shows every line)');
    const mine = page.locator('#card [data-act="atkmine"]').first();
    if (how === 'mine' && await mine.count()) {
      await mine.click(); await page.waitForTimeout(500);
      await read('ONLY THE CORPS PICKED');
      ls = await tick(); for (const l of ls) if (/^(Weight of attack|Earlier attacks|Forces)/.test(l)) console.log('   ', l.slice(0, 420));
      const c2 = page.locator('#card .costline').first(); if (await c2.count()) { await c2.scrollIntoViewIfNeeded(); await page.waitForTimeout(150); }
      await page.screenshot({ path: `shots/${shot}-${name}-4-pick.png` });
    }
    const go = page.locator('#card [data-act="attack"]').first();
    if (await go.count() && !(await go.isDisabled())) {
      await go.click(); await page.waitForTimeout(900);
      const rep = await page.evaluate(() => { const g = window.theater.game(); const r = g.reports[g.reports.length - 1]; return r ? { where: r.regionName, card: +(r.aPower / r.dPower).toFixed(2), day: +r.odds.toFixed(2), taken: !!r.taken, siege: !!r.siege, attLost: r.attLost, defLost: r.defLost, attackers: r.attackers.length, weight: r.weight } : null; });
      console.log('RESULT:', JSON.stringify(rep));
      const sub = page.locator('#card .sub').first();
      console.log('report says:', flat(await card.innerText(), 700));
      await page.screenshot({ path: `shots/${shot}-${name}-5-report.png` });
      // Close the report and look at the same line again: a second blow the same month should meet tired defenders.
      const x = page.locator('#card [data-act="close"], #veil [data-act="close"]').first();
      if (await x.count()) { await x.click(); await page.waitForTimeout(400); }
      await page.evaluate((id) => window.theater.onRegion(id), info.from); await page.waitForTimeout(300);
      await page.evaluate((id) => window.theater.onRegion(id), info.target); await page.waitForTimeout(600);
      if (await page.locator('#card .balance').count()) {
        await read('SECOND LOOK');
        const a2 = page.locator('#card [data-act="ticketall"]').first(); if (await a2.count() && await a2.isVisible()) { await a2.click(); await page.waitForTimeout(300); }
        ls = await tick(); for (const l of ls) if (/^(Weight of attack|Earlier attacks|Dug in)/.test(l)) console.log('   ', l.slice(0, 420));
        await page.screenshot({ path: `shots/${shot}-${name}-6-second.png` });
      } else console.log('SECOND LOOK: no attack card (', flat(await card.innerText(), 200), ')');
    }
    console.log('page errors:', errors.length, errors.slice(0, 3).join(' | '));
    await page.close();
  }
  await browser.close();
})();
