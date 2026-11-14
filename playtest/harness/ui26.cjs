// Version 64: the new cards as a player meets them, on a desk and on a phone, for saves made by probe42.
//   soften: the attack card's row "Soften the line first": its words, the probe (what the card reads before and after,
//           what the news says), the bombers on the line (the raid planner it opens, the raid flown, the card after);
//   fold:   the army tray's note, the fold card with each way round, the fold ordered, the cadre in the arsenal;
//   left:   the province panel's note on fieldworks standing empty.
// usage: [TC_PAGE=...] ui26.cjs soften|fold|left SAVE.json SAVE.txt SHOT-PREFIX
const { chromium } = require('playwright');
const fs = require('fs');
const [what, file, infoFile, shot] = process.argv.slice(2);
const info = JSON.parse(fs.readFileSync(infoFile, 'utf8'));
const flat = (s, n = 1800) => String(s).replace(/\n+/g, ' / ').replace(/\s+/g, ' ').slice(0, n);
(async () => {
  const browser = await chromium.launch();
  for (const [name, vp] of [['desk', { width: 1440, height: 900 }], ['phone', { width: 390, height: 844 }]]) {
    const page = await browser.newPage({ viewport: vp, deviceScaleFactor: 2, isMobile: name === 'phone', hasTouch: name === 'phone' });
    const errors = [];
    page.on('pageerror', (e) => errors.push(e.message));
    page.on('console', (m) => { if (m.type() === 'error') errors.push('console: ' + m.text()); });
    await page.addInitScript((s) => { try { localStorage.setItem('tc-watch', '0'); localStorage.setItem('theater-command-save-v1', s); } catch {} }, fs.readFileSync(file, 'utf8'));
    await page.goto(process.env.TC_PAGE || 'file:///home/user/tc64/dist/play.html');
    await page.waitForTimeout(1000);
    const cont = page.locator('[data-act="continue"]').first();
    if (await cont.count()) await cont.click(); else await page.evaluate(() => window.theater.continueGame());
    await page.waitForTimeout(1500);
    const say = (...a) => console.log(`[${name}]`, ...a);
    const card = page.locator('#card').first();
    const height = async () => { const d = await card.evaluate((e) => ({ scroll: e.scrollHeight, client: e.clientHeight })); return `${(d.scroll / Math.max(1, d.client)).toFixed(2)} screens ${JSON.stringify(d)}`; };
    const news = async (n = 3) => page.evaluate((k) => { const g = window.theater.game(); return g.headlines.slice(-k).map((h) => h.text); }, n);
    const msg = async () => { const e = page.locator('#msg, .toast, .msg').first(); return (await e.count()) ? flat(await e.innerText(), 300) : ''; };
    if (what === 'soften') {
      const openCard = async () => { await page.evaluate((id) => window.theater.onRegion(id), info.from); await page.waitForTimeout(400); await page.evaluate((id) => window.theater.onRegion(id), info.target); await page.waitForTimeout(700); return (await page.locator('#card .balance').count()) > 0; };
      const verdict = async () => flat(await page.locator('#card .balance .bhead').first().innerText(), 300);
      const works = async () => page.evaluate((id) => { const g = window.theater.game(); return g.regions[id].units.filter((u) => !u.dead).map((u) => `${u.name} ${u.dug ? (u.dugFor ?? 1).toFixed(1) : 'none'}`).join('; '); }, info.target);
      if (!(await openCard())) { say('NO ATTACK CARD:', flat(await card.innerText(), 500)); await page.close(); continue; }
      say('AS IT OPENS verdict:', await verdict(), '| card', await height());
      say('defenders\' works:', await works());
      const row = page.locator('#card .note.soften').first();
      say('SOFTEN ROW:', (await row.count()) ? flat(await row.innerText(), 1200) : 'NONE');
      for (const b of await page.locator('#card .note.soften button').all()) say('   button:', flat(await b.innerText(), 80), '| disabled:', await b.isDisabled(), '| title:', flat((await b.getAttribute('title')) ?? '', 400));
      if (await row.count()) { await row.scrollIntoViewIfNeeded(); await page.waitForTimeout(200); await page.screenshot({ path: `shots/${shot}-${name}-1-row.png` }); }
      const y = await row.count() ? await row.evaluate((e) => { const c = document.querySelector('#card'); return Math.round(((e.getBoundingClientRect().top - c.getBoundingClientRect().top + c.scrollTop) / c.clientHeight) * 100) / 100; }) : -1;
      say('the row sits', y, 'screens down the card');
      const pb = page.locator('#card [data-act="probe"]').first();
      if (await pb.count() && !(await pb.isDisabled())) {
        const before = await page.evaluate(() => { const g = window.theater.game(); return g.corps.filter((c) => c.nat === g.human && !c.dead).reduce((s, c) => s + c.divs * c.str, 0); });
        await pb.click(); await page.waitForTimeout(800);
        say('AFTER THE PROBE: message:', await msg(), '| news:', flat((await news(2)).join(' || '), 700));
        say('   card open still:', (await page.locator('#card .balance').count()) > 0);
        if (!(await page.locator('#card .balance').count())) await openCard();
        say('   verdict now:', await verdict());
        say('   works now:', await works());
        const r2 = page.locator('#card .note.soften').first();
        say('   row now:', (await r2.count()) ? flat(await r2.innerText(), 900) : 'NONE');
        if (await r2.count()) { await r2.scrollIntoViewIfNeeded(); await page.waitForTimeout(200); await page.screenshot({ path: `shots/${shot}-${name}-2-probed.png` }); }
        const after = await page.evaluate(() => { const g = window.theater.game(); return g.corps.filter((c) => c.nat === g.human && !c.dead).reduce((s, c) => s + c.divs * c.str, 0); });
        say('   strength lost in divisions:', (before - after).toFixed(3));
      } else say('no probe button to press');
      const bb = page.locator('#card [data-act="bombline"]').first();
      if (await bb.count() && !(await bb.isDisabled())) {
        await bb.click(); await page.waitForTimeout(900);
        say('AFTER "BOMB THE LINE": card:', flat(await card.innerText(), 1500), '| height', await height());
        await page.screenshot({ path: `shots/${shot}-${name}-3-planner.png` });
        const fly = page.locator('#card [data-act="raidgo"], #card [data-act="flyraid"], #card button.go').first();
        say('   fly button:', (await fly.count()) ? flat(await fly.innerText(), 80) + ' | disabled: ' + (await fly.isDisabled()) : 'NONE; buttons: ' + flat((await page.locator('#card button').allInnerTexts()).join(' | '), 500));
        if (await fly.count() && !(await fly.isDisabled())) {
          await fly.click(); await page.waitForTimeout(1200);
          say('   RAID REPORT:', flat(await card.innerText(), 1200));
          await page.screenshot({ path: `shots/${shot}-${name}-4-raid.png` });
          const x = page.locator('#card [data-act="close"]').first(); if (await x.count()) { await x.click(); await page.waitForTimeout(400); }
          say('   works after the raid:', await works());
          if (await openCard()) { say('   verdict after probe and raid:', await verdict()); const r3 = page.locator('#card .note.soften').first(); say('   row:', (await r3.count()) ? flat(await r3.innerText(), 700) : 'NONE'); await page.screenshot({ path: `shots/${shot}-${name}-5-after.png` }); }
          else say('   no attack card after the raid:', flat(await card.innerText(), 300));
        }
      } else say('no bomb button to press');
    }
    if (what === 'fold') {
      await page.evaluate((id) => window.theater.onRegion(id), info.reg); await page.waitForTimeout(700);
      const bar = page.locator('.foldbar').first();
      say('TRAY NOTE:', (await bar.count()) ? flat(await bar.innerText(), 400) + ' | visible: ' + (await bar.isVisible()) : 'NONE; tray: ' + flat(await page.locator('#tray').first().innerText().catch(() => ''), 400));
      const fb = page.locator('[data-act="foldcard"]').first();
      if (await fb.count()) {
        say('   button title:', flat((await fb.getAttribute('title')) ?? '', 500));
        await page.screenshot({ path: `shots/${shot}-${name}-1-tray.png` });
        await fb.click(); await page.waitForTimeout(700);
        say('FOLD CARD:', flat(await card.innerText(), 2200), '| height', await height());
        await page.screenshot({ path: `shots/${shot}-${name}-2-card.png` });
        const before = await page.evaluate((id) => { const g = window.theater.game(); return g.regions[id].units.filter((u) => u.nat === g.human && !u.dead).map((u) => `${u.name} ${u.kind} ${u.divs}x${Math.round(u.str * 100)}% ap ${u.ap}${u.dug ? ' dug ' + (u.dugFor ?? 1).toFixed(1) : ''}`).join('; '); }, info.reg);
        say('before:', before);
        const go = page.locator('#card [data-act="fold"]').first();
        if (await go.count() && !(await go.isDisabled())) {
          say('   order button:', flat(await go.innerText(), 100), '| title:', flat((await go.getAttribute('title')) ?? '', 300));
          await go.click(); await page.waitForTimeout(800);
          say('AFTER THE FOLD: message:', await msg(), '| news:', flat((await news(2)).join(' || '), 600));
          const after = await page.evaluate((id) => { const g = window.theater.game(); return g.regions[id].units.filter((u) => u.nat === g.human && !u.dead).map((u) => `${u.name} ${u.kind} ${u.divs}x${Math.round(u.str * 100)}% ap ${u.ap}${u.dug ? ' dug ' + (u.dugFor ?? 1).toFixed(1) : ''}`).join('; ') + ' | cadres: ' + (g.nations[g.human].cadres ?? []).map((c) => c.name).join(', '); }, info.reg);
          say('after:', after);
          await page.screenshot({ path: `shots/${shot}-${name}-3-folded.png` });
          // the cadre in the arsenal: the Build tab at the capital, the army's cards
          const tabb = page.locator('#tabs [data-act="tab"][data-tab="build"]').first();
          if (await tabb.count()) { await tabb.click().catch(() => {}); await page.waitForTimeout(400); }
          await page.evaluate(() => { const t = window.theater, g = t.game(); const cap = g.nations[g.human].capital; const r = typeof cap === 'number' ? g.regions[cap] : g.regions.find((x) => x.name === 'Berlin'); t.ui.tab = 'build'; t.ui.sel = r.id; t.refresh(); });
          await page.waitForTimeout(400);
          const armb = page.locator('#pane [data-act="arm"][data-arm="army"]').first();
          if (await armb.count()) { await armb.click().catch(() => {}); await page.waitForTimeout(300); }
          const cards = await page.locator('#pane [data-act="arsenal"]').evaluateAll((bs) => bs.map((x) => ({ key: x.getAttribute('data-key') || x.getAttribute('data-unit') || '', text: (x.innerText || '').replace(/\s+/g, ' ').trim(), title: x.getAttribute('title') || '', off: x.disabled === true || x.classList.contains('off') })));
          const cad = cards.filter((c) => /cadre|XV Corps|XIX Corps/i.test(c.key + ' ' + c.text + ' ' + c.title));
          say(`ARSENAL: ${cards.length} army cards; cadre cards ${cad.length}:`, cad.map((c) => `[${c.key}] ${flat(c.text, 260)} | title: ${flat(c.title, 420)} | off: ${c.off}`).join(' ;; ') || 'none; first card: ' + flat(JSON.stringify(cards[0] ?? {}), 300));
          const pane = page.locator('#pane').first();
          const where = await page.locator('#pane [data-act="arsenal"]').evaluateAll((bs) => { const i = bs.findIndex((x) => /cadre/i.test((x.getAttribute('data-key') || '') + (x.innerText || ''))); if (i < 0) return 'none'; const r = bs[i].getBoundingClientRect(); return `card ${i + 1} of ${bs.length}, top ${Math.round(r.top)} of a ${window.innerHeight} screen`; });
          say('   where the cadre card sits:', where);
          const cb = page.locator('#pane [data-act="arsenal"]').filter({ hasText: /cadre/i }).first();
          if (await cb.count()) { await cb.scrollIntoViewIfNeeded().catch(() => {}); await page.waitForTimeout(200); }
          await page.screenshot({ path: `shots/${shot}-${name}-4-arsenal.png` });
          if (await cb.count() && !(await cb.isDisabled().catch(() => false))) {
            const before2 = await page.evaluate(() => { const g = window.theater.game(); const n = g.nations[g.human]; return { treasury: Math.round(n.treasury ?? n.gold ?? n.money ?? 0), builds: (n.builds ?? g.builds ?? []).length, cadres: (n.cadres ?? []).length }; });
            await cb.click(); await page.waitForTimeout(700);
            const cardTxt = (await card.isVisible().catch(() => false)) ? flat(await card.innerText(), 700) : '';
            const after2 = await page.evaluate(() => { const g = window.theater.game(); const n = g.nations[g.human]; return { treasury: Math.round(n.treasury ?? n.gold ?? n.money ?? 0), builds: (n.builds ?? g.builds ?? []).length, cadres: (n.cadres ?? []).length }; });
            say('   raise the cadre: before', JSON.stringify(before2), 'after', JSON.stringify(after2), '| message:', await msg(), '| card:', cardTxt, '| news:', flat((await news(1)).join(' '), 300));
            await page.screenshot({ path: `shots/${shot}-${name}-5-raised.png` });
          }
        } else say('no order button on the fold card');
      }
    }
    if (what === 'left') {
      await page.evaluate((id) => window.theater.onRegion(id), info.reg); await page.waitForTimeout(700);
      const note = page.locator('.note.leftworks').first();
      say('PROVINCE NOTE:', (await note.count()) ? flat(await note.innerText(), 600) + ' | visible: ' + (await note.isVisible()) : 'NONE; panel: ' + flat(await page.locator('#pane').first().innerText().catch(() => ''), 600));
      say('banks drawn on the map:', await page.locator('svg .leftworks').count(), 'for', (info.all ?? []).length, 'provinces with empty works:', (info.all ?? []).join(', '));
      const stand = await page.evaluate((id) => { const g = window.theater.game(); return g.regions[id].units.filter((u) => !u.dead).map((u) => `${u.name} ${u.kind}${u.dug ? ' dug ' + (u.dugFor ?? 1).toFixed(1) : ' no works'}${u.town !== undefined ? ' (in the town)' : ''}`).join('; '); }, info.reg);
      say('who stands there:', stand || 'nobody');
      await page.screenshot({ path: `shots/${shot}-${name}-1-left.png` });
    }
    say('page errors:', errors.length, errors.slice(0, 3).join(' | '));
    await page.close();
  }
  await browser.close();
})();
