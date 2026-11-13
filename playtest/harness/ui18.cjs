// Version 58: prepared defences as a player meets them. Loads a save made by probe31 (a month with an attack on a
// dug-in line on offer) and reads: the army card of a dug-in army (tags, the stance button), the earth banks on the
// map, the attack card on the prepared line (verdict, the white even-chance mark, the "Prepared defence" line), what
// happens when the attack is ordered, then the wrecking buttons (asked twice; a greyed one says why) and the policy.
// usage: ui18.cjs SAVE.json SAVE.txt SHOT-PREFIX
const { chromium } = require('playwright');
const fs = require('fs');
const [file, infoFile, shot] = process.argv.slice(2);
const info = JSON.parse(fs.readFileSync(infoFile, 'utf8'));
const flat = (s, n = 1800) => s.replace(/\n+/g, ' / ').replace(/\s+/g, ' ').slice(0, n);
(async () => {
  const browser = await chromium.launch();
  const page = await browser.newPage({ viewport: { width: 1440, height: 900 }, deviceScaleFactor: 2 });
  const errors = [];
  page.on('pageerror', (e) => errors.push(e.message));
  page.on('console', (m) => { if (m.type() === 'error') errors.push('console: ' + m.text()); });
  const save = fs.readFileSync(file, 'utf8');
  await page.addInitScript((s) => { try { localStorage.setItem('tc-watch', '0'); localStorage.setItem('theater-command-save-v1', s); } catch {} }, save);
  await page.goto(process.env.TC_PAGE || 'file:///home/user/tc58/dist/play.html');
  await page.waitForTimeout(1000);
  const cont = page.locator('[data-act="continue"]').first();
  if (await cont.count()) { await cont.click(); } else console.log('continue (no card):', await page.evaluate(() => window.theater.continueGame()));
  await page.waitForTimeout(1500);
  const st = async () => page.evaluate(() => { const g = window.theater.game(); const mine = g.corps.filter((c) => c.nat === g.human && !c.dead && c.kind !== 'fort' && c.town === undefined); return { turn: g.turn, human: g.human, field: mine.length, dug: mine.filter((c) => c.dug).length, canMove: mine.filter((c) => c.ap > 0).length }; });
  console.log('loaded:', JSON.stringify(await st()), 'target', info.targetName, 'from', info.fromName);
  // 1. the map: earth banks
  const banks = await page.evaluate(() => ({ works: document.querySelectorAll('path.works').length, flags: document.querySelectorAll('.flag, g.army, [data-army]').length }));
  console.log('earth banks drawn on the map (path.works):', JSON.stringify(banks));
  await page.screenshot({ path: `shots/${shot}-1-map.png` });
  // 2. the dug-in army's card
  await page.evaluate((id) => window.theater.onRegion(id), info.from); await page.waitForTimeout(400);
  const card = page.locator('#card').first();
  const tags = await page.locator('#pane .tag, #tray .tag').evaluateAll((ts) => ts.map((t) => `${t.textContent.trim()}${t.title ? ' {' + t.title.slice(0, 160) + '}' : ''}`));
  const count = {}; for (const t of tags) { const k = t.split(' {')[0]; count[k] = (count[k] || 0) + 1; }
  console.log('army card tags:', JSON.stringify(count));
  console.log('one "Dug in" tip:', (tags.find((t) => t.startsWith('Dug in')) || 'NONE').slice(0, 400));
  console.log('one "Digging in" tip:', (tags.find((t) => t.startsWith('Digging in')) || 'NONE').slice(0, 400));
  const stance = await page.locator('#pane [data-act="ifattacked"], #tray [data-act="ifattacked"]').evaluateAll((bs) => bs.map((b) => `${b.textContent.trim()}${b.classList.contains('on') ? ' [on]' : ''} {${(b.title || '').slice(0, 220)}}`));
  console.log('stance buttons:', stance.join(' | ') || 'NONE');
  console.log('ARMY TRAY TEXT:', flat(await page.locator('#tray').first().innerText(), 1500));
  await page.screenshot({ path: `shots/${shot}-2-army.png` });
  // 3. the attack card on the prepared line
  await page.evaluate((id) => window.theater.onRegion(id), info.target); await page.waitForTimeout(500);
  const first = page.locator('#card .balance').first();
  if (await first.count()) console.log('ATTACK CARD as it opens (the corps picked in one province):', flat(await first.innerText(), 300), '| corps offered to join:', await page.locator('#card [data-act="atkpick"]').count(), 'of them in:', await page.locator('#card [data-act="atkpick"].pick').count());
  let clicks = 0;
  for (let i = 0; i < 40; i++) { const o = page.locator('#card [data-act="atkpick"].out').first(); if (!(await o.count())) break; await o.click(); clicks++; await page.waitForTimeout(40); }
  console.log('clicks to bring every corps in:', clicks, '| now in:', await page.locator('#card [data-act="atkpick"].pick').count(), '| any "add all" button:', await page.locator('#card [data-act="atkall"], #card [data-act="atkevery"]').count() ? 'yes' : 'no');
  const bal = page.locator('#card .balance').first();
  if (await bal.count()) {
    console.log('ATTACK CARD verdict:', flat(await bal.innerText(), 600));
    const need = page.locator('.balance .need').first();
    console.log('white mark:', await need.count() ? JSON.stringify({ title: await need.getAttribute('title'), left: await need.evaluate((e) => e.style.left), box: await need.boundingBox() }) : 'NONE');
    const lines = await page.locator('#card .ticket tr, #card .tline, #card .ticket li, #card .ticket .trow').evaluateAll((rs) => rs.map((r) => r.innerText.replace(/\s+/g, ' ').trim()));
    console.log('ticket lines (' + lines.length + '):'); for (const l of lines) console.log('   ', l.slice(0, 330));
    if (!lines.length) console.log('CARD TEXT:', flat(await card.innerText(), 3500));
    console.log('notes on the card:', flat((await page.locator('#card .note, #card .fhelp, #card .warn').allInnerTexts()).join(' ; '), 900));
    const dims = await card.evaluate((e) => ({ scroll: e.scrollHeight, client: e.clientHeight }));
    console.log('card height:', JSON.stringify(dims), 'screens of scrolling', (dims.scroll / Math.max(1, dims.client)).toFixed(2));
    await page.screenshot({ path: `shots/${shot}-3-attack-card.png` });
    const prep = page.locator('#card').getByText('Prepared defence').first();
    if (await prep.count()) { await prep.scrollIntoViewIfNeeded(); await page.waitForTimeout(200); await page.screenshot({ path: `shots/${shot}-3b-prepared-line.png` }); }
    // order it
    const before = await st();
    const go = page.locator('#card [data-act="attack"]').first();
    if (await go.count()) {
      console.log('attack button:', flat(await go.innerText(), 200), await go.isDisabled() ? '[off]' : '');
      await go.click(); await page.waitForTimeout(900);
      const rep = await page.evaluate(() => { const g = window.theater.game(); const r = g.reports[g.reports.length - 1]; return r ? { where: r.regionName, odds: r.odds, card: r.aPower / r.dPower, taken: r.taken, siege: !!r.siege, prepared: r.prepared, attLost: r.attLost, defLost: r.defLost, level: r.level } : null; });
      console.log('RESULT:', JSON.stringify(rep));
      const after = await st();
      console.log('corps with moves left before', before.canMove, 'after', after.canMove);
      const shown = page.locator('#modal, .modal, .report, #card').first();
      console.log('WHAT THE PLAYER IS SHOWN:', flat(await page.evaluate(() => { const m = document.querySelector('#modal:not([hidden]), .modal.show, .battle, .report'); return (m || document.querySelector('#card')).innerText; }), 2200));
      await page.screenshot({ path: `shots/${shot}-4-result.png` });
    } else console.log('NO ATTACK BUTTON; card:', flat(await card.innerText(), 800));
  } else console.log('NO ATTACK CARD; card says:', flat(await card.innerText(), 800));
  { const x = page.locator('#veil [data-act="close"]').first(); if (await x.count() && await x.isVisible()) await x.click(); else await page.keyboard.press('Escape'); await page.waitForTimeout(300); }
  // 4. wrecking: find a province of the player's with a place of its own
  const ids = await page.evaluate(() => { const g = window.theater.game(); return g.regions.filter((r) => r.ctrl === g.human).map((r) => r.id); });
  let found = 0, greyed = 0;
  for (const id of ids) {
    if (found >= 1 && greyed >= 1) break;
    await page.evaluate((i) => window.theater.onRegion(i), id); await page.waitForTimeout(60);
    const btns = page.locator('#pane .places [data-act="wreckask"]');
    const n = await btns.count(); if (!n) continue;
    for (let i = 0; i < n; i++) {
      const b = btns.nth(i); const off = await b.isDisabled();
      if (off && greyed < 1) {
        greyed++;
        const row = b.locator('xpath=ancestor::div[contains(@class,"place")][1]');
        console.log('GREYED WRECK BUTTON in', await page.evaluate((i2) => window.theater.game().regions[i2].name, id), ':', flat(await row.innerText(), 500), '/ why in words:', await row.locator('.whynot').count() ? flat(await row.locator('.whynot').first().innerText(), 300) : 'NONE (tooltip only: ' + (await b.getAttribute('title')) + ')');
        await row.scrollIntoViewIfNeeded(); await page.screenshot({ path: `shots/${shot}-5-wreck-greyed.png` });
      }
      if (!off && found < 1) {
        found++;
        const name = await page.evaluate((i2) => window.theater.game().regions[i2].name, id);
        const row = b.locator('xpath=ancestor::div[contains(@class,"place")][1]');
        console.log('WRECK BUTTON in', name, ':', flat(await row.innerText(), 400));
        const prizesBefore = await page.evaluate(() => JSON.stringify(window.theater.game().prizes ?? null).length);
        await b.scrollIntoViewIfNeeded(); await b.click(); await page.waitForTimeout(300);
        const ask = page.locator('#pane .wreckask').first();
        console.log('after ONE click: asked again?', await ask.count() ? 'YES: ' + flat(await ask.innerText(), 200) + ' / ' + flat(await page.locator('#pane .whynot.ask').first().innerText().catch(() => ''), 300) : 'NO');
        console.log('state changed by the first click?', prizesBefore === await page.evaluate(() => JSON.stringify(window.theater.game().prizes ?? null).length) ? 'no' : 'YES');
        await page.screenshot({ path: `shots/${shot}-6-wreck-ask.png` });
        const keep = page.locator('#pane [data-act="wreckno"]').first(); if (await keep.count()) { await keep.click(); await page.waitForTimeout(200); console.log('Keep it pressed; ask still showing?', await page.locator('#pane .wreckask').count() ? 'yes' : 'no'); }
        break;
      }
    }
  }
  console.log('wreck buttons found:', found, 'greyed found:', greyed);
  // 5. the policy, folded
  const pol = page.locator('#pane [data-act="polopen"]').first();
  if (await pol.count()) {
    console.log('policy button (folded):', flat(await pol.innerText(), 100), '/ policy buttons showing before opening:', await page.locator('#pane [data-act="scorch"]').count());
    await pol.scrollIntoViewIfNeeded(); await pol.click(); await page.waitForTimeout(300);
    console.log('after opening:', (await page.locator('#pane [data-act="scorch"]').evaluateAll((bs) => bs.map((b) => `${b.textContent.trim()}${b.classList.contains('on') ? ' [on]' : ''}`))).join(' | '));
    const sc = page.locator('#pane .scorch').first(); if (await sc.count()) { console.log('SCORCH ROW:', flat(await sc.innerText(), 700)); await sc.scrollIntoViewIfNeeded(); }
    await page.screenshot({ path: `shots/${shot}-7-policy.png` });
  } else console.log('no policy button on this card');
  // 6. the help list
  const help = await page.evaluate(() => { const hits = []; for (const e of document.querySelectorAll('li, p, small')) { const t = e.textContent || ''; if (/dig in|dug in|digs in/i.test(t) && t.length < 600 && !e.closest('#card') && !e.closest('#pane') && !e.closest('#tray')) hits.push(t.replace(/\s+/g, ' ').trim()); } return [...new Set(hits)].slice(0, 6); });
  console.log('help text about digging in outside the card:', help.length ? '\n   ' + help.join('\n   ') : 'none in the page until a help panel is opened');
  console.log('page errors:', errors.length, errors.slice(0, 5).join(' ; '));
  await browser.close();
})();
