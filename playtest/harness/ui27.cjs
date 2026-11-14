// Version 65: page checks.
//   phone:  with an army picked, how much of the map can still be tapped at several screen sizes: the map's box, the
//           corps tray's and the hint's, the share of the map's box where a tap lands on the map, and whether each
//           province the army could march to or attack can be tapped at all. Before the pick and after it. It then
//           taps one of them with a finger and says what happened.
//   swap:   a corps sent at a full province next door: the "Change places" card, its words, and the order given.
//   powers: the Powers tab: what it says of each computer staff.
//   cadre:  the cadre's card in the arsenal with the money, without it ("N short"), and raised (REGION-ID: where two worn corps stand).
// usage: [TC_PAGE=...] ui27.cjs phone|swap|powers|cadre SAVE.json SHOT-PREFIX [REGION-ID]
const { chromium } = require('playwright');
const fs = require('fs');
const [what, file, shot, regArg] = process.argv.slice(2);
const flat = (s, n = 1800) => String(s).replace(/\n+/g, ' / ').replace(/\s+/g, ' ').slice(0, n);
const SIZES = what === 'phone'
  ? [['phone-390x844', { width: 390, height: 844 }, true], ['phone-360x640', { width: 360, height: 640 }, true], ['phone-430x932', { width: 430, height: 932 }, true], ['phone-landscape-844x390', { width: 844, height: 390 }, true], ['tablet-820x1180', { width: 820, height: 1180 }, true], ['desk-1440x900', { width: 1440, height: 900 }, false]]
  : [['desk', { width: 1440, height: 900 }, false], ['phone', { width: 390, height: 844 }, true]];
(async () => {
  const browser = await chromium.launch();
  for (const [name, vp, touch] of SIZES) {
    const page = await browser.newPage({ viewport: vp, deviceScaleFactor: 2, isMobile: touch, hasTouch: touch });
    const errors = [];
    page.on('pageerror', (e) => errors.push(e.message));
    page.on('console', (m) => { if (m.type() === 'error') errors.push('console: ' + m.text()); });
    await page.addInitScript((s) => { try { localStorage.setItem('tc-watch', '0'); localStorage.setItem('theater-command-save-v1', s); } catch {} }, fs.readFileSync(file, 'utf8'));
    await page.goto(process.env.TC_PAGE || 'file:///home/user/tc65/dist/play.html');
    await page.waitForTimeout(1000);
    const cont = page.locator('[data-act="continue"]').first();
    if (await cont.count()) await cont.click(); else await page.evaluate(() => window.theater.continueGame());
    await page.waitForTimeout(1500);
    const say = (...a) => console.log(`[${name}]`, ...a);
    // What lies over the map: for a grid of points in the map's box, is the thing under the finger part of the map?
    const cover = () => page.evaluate(() => {
      const box = (sel) => { const e = document.querySelector(sel); if (!e) return null; const r = e.getBoundingClientRect(); const cs = getComputedStyle(e); return cs.display === 'none' || r.width === 0 ? null : { x: Math.round(r.left), y: Math.round(r.top), w: Math.round(r.width), h: Math.round(r.height) }; };
      const m = document.querySelector('#mapwrap').getBoundingClientRect();
      const svg = document.querySelector('#map');
      let on = 0, n = 0; const by = {};
      for (let i = 0; i < 24; i++) for (let j = 0; j < 24; j++) {
        const x = m.left + ((i + 0.5) / 24) * m.width, y = m.top + ((j + 0.5) / 24) * m.height;
        if (y >= innerHeight || x >= innerWidth) continue;
        n++;
        const e = document.elementFromPoint(x, y);
        if (e && svg.contains(e)) on++;
        else { const k = e ? (e.closest('#tray') ? 'tray' : e.closest('#hint') ? 'hint' : e.closest('.zoom') ? 'zoom buttons' : e.closest('#passing') ? 'passing' : e.closest('.legend') ? 'legend' : e.closest('#alerts') ? 'alerts' : e.closest('.side') ? 'side panel' : e.closest('.bar') ? 'top bar' : (e.id || e.className || e.tagName)) : 'nothing'; by[k] = (by[k] ?? 0) + 1; }
      }
      return { screen: `${innerWidth}x${innerHeight}`, map: box('#mapwrap'), tray: box('#tray'), hint: box('#hint'), bar: box('.bar'), side: box('.side'), free: n ? Math.round((100 * on) / n) : 0, coveredBy: Object.fromEntries(Object.entries(by).map(([k, v]) => [k, Math.round((100 * v) / n)])), hintText: (document.querySelector('#hint')?.textContent ?? '').slice(0, 300) };
    });
    // Each province the picked army can reach: can a finger find any point of it?
    const reachable = () => page.evaluate(() => {
      const svg = document.querySelector('#map'), m = document.querySelector('#mapwrap').getBoundingClientRect();
      const out = [];
      const ids = {}; for (const q of document.querySelectorAll('#regs path.reg')) ids[q.getAttribute('d')] = Number(q.getAttribute('data-id'));
      for (const p of document.querySelectorAll('#marks path.mk.move, #marks path.mk.hit, #marks path.mk.war')) {
        const cls = p.getAttribute('class') || '';
        const r = p.getBoundingClientRect();
        let seen = 0, tap = 0, at = null;
        for (let i = 0; i < 7; i++) for (let j = 0; j < 7; j++) {
          const x = r.left + ((i + 0.5) / 7) * r.width, y = r.top + ((j + 0.5) / 7) * r.height;
          if (x < m.left || x > m.right || y < m.top || y > m.bottom) continue;
          const pt = svg.createSVGPoint(); pt.x = x; pt.y = y;
          const inside = p.isPointInFill(pt.matrixTransform(p.getScreenCTM().inverse()));
          if (!inside) continue;
          seen++;
          const e = document.elementFromPoint(x, y);
          if (e && svg.contains(e)) { tap++; if (!at) at = { x: Math.round(x), y: Math.round(y) }; }
        }
        out.push({ id: ids[p.getAttribute('d')] ?? -1, cls: cls.replace(/\bmk\b/, '').trim(), inView: seen, canTap: tap, at });
      }
      return out;
    });
    if (what === 'phone') {
      const g0 = await cover();
      say('BEFORE A PICK:', JSON.stringify(g0));
      await page.screenshot({ path: `shots/${shot}-${name}-1-before.png` });
      // Pick the army the way a finger does: tap its counter on the map.
      const reg = regArg !== undefined ? Number(regArg) : await page.evaluate(() => { const g = window.theater.game(); let best = -1, n = 0; for (const r of g.regions) { const k = r.units.filter((u) => u.nat === g.human && !u.dead && u.kind !== 'fort' && u.ap > 0).length; const front = r.nb.some((j) => g.regions[j].units.some((u) => !u.dead && g.nations[g.human].atWar.includes(u.nat))); if (front && k > n) { n = k; best = r.id; } } return best; });
      const ctr = page.locator(`#units .army[data-reg="${reg}"]`).first();
      const where = await ctr.count() ? await ctr.boundingBox() : null;
      say('army at region', reg, await page.evaluate((id) => window.theater.game().regions[id].name, reg), 'counter box', JSON.stringify(where));
      let picked = false;
      if (where && where.y + where.height / 2 < vp.height) {
        if (touch) await page.touchscreen.tap(where.x + where.width / 2, where.y + where.height / 2); else await page.mouse.click(where.x + where.width / 2, where.y + where.height / 2);
        await page.waitForTimeout(800);
        picked = await page.evaluate(() => document.querySelector('#tray')?.classList.contains('open'));
      }
      if (!picked) { await page.evaluate((id) => window.theater.onRegion(id), reg); await page.waitForTimeout(800); say('(picked through the page call, not by a tap)'); }
      const g1 = await cover();
      say('WITH THE ARMY PICKED:', JSON.stringify(g1));
      say('tray says:', flat(await page.locator('#tray').innerText().catch(() => ''), 500));
      await page.screenshot({ path: `shots/${shot}-${name}-2-picked.png` });
      const rs = await reachable();
      const names = await page.evaluate((ids) => ids.map((i) => window.theater.game().regions[i]?.name ?? '?'), rs.map((r) => r.id));
      say(`provinces lit for this army: ${rs.length}; a finger can find ${rs.filter((r) => r.canTap > 0).length}; hidden under a panel ${rs.filter((r) => r.inView > 0 && r.canTap === 0).length}; off the screen ${rs.filter((r) => r.inView === 0).length}`);
      for (let i = 0; i < rs.length; i++) say(`   ${names[i]} (${rs[i].cls}): points in view ${rs[i].inView}, can be tapped ${rs[i].canTap}`);
      // Tap one that can be tapped, and one that cannot (at its middle).
      const ok = rs.find((r) => r.canTap > 0 && /attack|hit/.test(r.cls)) ?? rs.find((r) => r.canTap > 0);
      if (ok) {
        if (touch) await page.touchscreen.tap(ok.at.x, ok.at.y); else await page.mouse.click(ok.at.x, ok.at.y);
        await page.waitForTimeout(900);
        const veil = await page.evaluate(() => { const v = document.querySelector('#veil'); return v && getComputedStyle(v).display !== 'none' ? (document.querySelector('#card h2')?.textContent ?? 'a card') : null; });
        say('TAPPED', names[rs.indexOf(ok)], 'at', JSON.stringify(ok.at), '-> card:', veil, '| tray open:', await page.evaluate(() => document.querySelector('#tray')?.classList.contains('open')), '| hint:', flat(await page.locator('#hint').innerText().catch(() => ''), 200));
        await page.screenshot({ path: `shots/${shot}-${name}-3-tapped.png` });
      } else say('NO lit province can be tapped at all');
    }
    if (what === 'powers') {
      const tab = page.locator('#tabs [data-tab="powers"], #tabs [data-act="tab"][data-id="powers"]').first();
      if (await tab.count()) await tab.click(); else { const t2 = page.locator('#tabs button', { hasText: 'Powers' }).first(); if (await t2.count()) await t2.click(); }
      await page.waitForTimeout(700);
      const staffs = await page.locator('#pane .staff').allInnerTexts();
      say('staff lines on the Powers tab:', staffs.length);
      for (const s of staffs.slice(0, 14)) say('   ', flat(s, 400));
      const head = page.locator('#pane .staffs').first();
      say('the block about staffs:', (await head.count()) ? flat(await head.innerText(), 1500) : 'NONE');
      await page.screenshot({ path: `shots/${shot}-${name}-powers.png` });
      const chr = page.locator('#pane button.tag.chr').first();
      if (await chr.count()) { await chr.click(); await page.waitForTimeout(600); say('after a tap on a character tag:', flat(await page.locator('#card').innerText().catch(() => ''), 1500)); await page.screenshot({ path: `shots/${shot}-${name}-character.png` }); }
    }
    if (what === 'swap') {
      // A corps of the player's beside a full province of his own.
      const pair = await page.evaluate(() => {
        const g = window.theater.game(), me = g.human;
        for (const r of g.regions) {
          if (r.ctrl !== me) continue;
          const full = r.units.filter((u) => !u.dead).length >= 5;
          if (!full) continue;
          for (const j of r.nb) { const q = g.regions[j]; if (q.ctrl !== me) continue; const c = q.units.find((u) => u.nat === me && !u.dead && u.kind !== 'fort' && u.kind !== 'eng' && u.town === undefined && u.ap >= 2); if (c) return { corps: c.id, name: c.name, from: q.id, fromName: q.name, to: r.id, toName: r.name, there: r.units.filter((u) => !u.dead).map((u) => `${u.name} ${Math.round(u.str * 100)}% ${u.dug ? 'works ' + (u.dugFor ?? 1) : 'no works'}`) }; }
        }
        return null;
      });
      say('PAIR:', JSON.stringify(pair));
      if (pair) {
        await page.evaluate((id) => window.theater.onRegion(id), pair.from); await page.waitForTimeout(600);
        // Keep one corps picked: tap its card in the tray if the tray has several.
        const cards = page.locator(`#tray [data-corps="${pair.corps}"], #tray [data-id="${pair.corps}"]`);
        say('tray cards for the corps:', await cards.count(), '| tray:', flat(await page.locator('#tray').innerText().catch(() => ''), 300));
        const picked = await page.evaluate((id) => { const t = window.theater; t.ui.picked.clear(); t.ui.picked.add(id); t.refresh(); return [...t.ui.picked].join(','); }, pair.corps);
        say('picked one corps through:', picked);
        await page.screenshot({ path: `shots/${shot}-${name}-1-picked.png` });
        // The order as a hand gives it: a right-click on a desk, a press and hold on a phone, at a point of the province a finger can find.
        // Bring the province under a spot of the map that nothing covers (on a phone that is a corner beside the hint).
        const spot = await page.evaluate((id) => {
          const t = window.theater, r = t.game().regions[id], svg = document.querySelector('#map'), m = document.querySelector('#mapwrap').getBoundingClientRect();
          let best = null;
          for (let i = 0; i < 12; i++) for (let j = 0; j < 12; j++) {
            const x = m.left + ((i + 0.5) / 12) * m.width, y = m.top + ((j + 0.5) / 12) * m.height;
            const ok = (dx, dy) => { const e = document.elementFromPoint(x + dx, y + dy); return !!e && svg.contains(e); };
            if (!(ok(0, 0) && ok(-14, 0) && ok(14, 0) && ok(0, -14) && ok(0, 14))) continue;
            const d = Math.abs(i - 5.5) + Math.abs(j - 3);
            if (!best || d < best.d) best = { x, y, d };
          }
          if (!best) return null;
          const h = t.ui.view.w * (svg.clientHeight / Math.max(1, svg.clientWidth));
          t.ui.view.x = r.x - ((best.x - m.left) / m.width) * t.ui.view.w; t.ui.view.y = r.y - ((best.y - m.top) / m.height) * h; t.applyView();
          return { x: Math.round(best.x), y: Math.round(best.y) };
        }, pair.to);
        say('a free spot on the map to bring it under:', JSON.stringify(spot));
        await page.waitForTimeout(400);
        const pt = await page.evaluate((id) => {
          const svg = document.querySelector('#map'), p = document.querySelector(`#regs path.reg[data-id="${id}"]`), r = p.getBoundingClientRect();
          let seen = 0, hit = null;
          for (let i = 0; i < 9; i++) for (let j = 0; j < 9; j++) {
            const x = r.left + ((i + 0.5) / 9) * r.width, y = r.top + ((j + 0.5) / 9) * r.height;
            const q = svg.createSVGPoint(); q.x = x; q.y = y;
            if (!p.isPointInFill(q.matrixTransform(p.getScreenCTM().inverse()))) continue;
            seen++;
            const e = document.elementFromPoint(x, y);
            if (!hit && e && svg.contains(e) && (e === p || !e.closest('[data-reg]'))) hit = { x: Math.round(x), y: Math.round(y) };
          }
          return { seen, hit };
        }, pair.to);
        say('a point of', pair.toName, 'a finger can find:', JSON.stringify(pt));
        if (pt.hit) {
          if (!touch) await page.mouse.click(pt.hit.x, pt.hit.y, { button: 'right' });
          else {
            const cdp = await page.context().newCDPSession(page);
            await cdp.send('Input.dispatchTouchEvent', { type: 'touchStart', touchPoints: [{ x: pt.hit.x, y: pt.hit.y }] });
            await page.waitForTimeout(900);
            await cdp.send('Input.dispatchTouchEvent', { type: 'touchEnd', touchPoints: [] });
          }
          await page.waitForTimeout(800);
        } else { await page.evaluate((id) => window.theater.onRegion(id), pair.to); await page.waitForTimeout(800); say('(no point to press: gave the click through the page call)'); }
        say('message after the order:', flat(await page.evaluate(() => window.theater.ui.msg ?? ''), 300));
        const card = page.locator('#card').first();
        const open = await page.evaluate(() => { const v = document.querySelector('#veil'); return v && getComputedStyle(v).display !== 'none'; });
        say('card open:', open, '|', open ? flat(await card.innerText(), 1800) : '');
        await page.screenshot({ path: `shots/${shot}-${name}-2-card.png` });
        const go = page.locator('#card [data-act="swap"]').first();
        if (await go.count()) {
          say('   button:', flat(await go.innerText(), 80), '| title:', flat((await go.getAttribute('title')) ?? '', 400));
          await go.click(); await page.waitForTimeout(900);
          const after = await page.evaluate((p) => { const g = window.theater.game(); const c = g.corps.find((x) => x.id === p.corps); return { corpsNowAt: g.regions[c.reg].name, works: c.dug ? c.dugFor ?? 1 : 0, ap: c.ap, from: g.regions[p.from].units.filter((u) => !u.dead).map((u) => `${u.name} ${u.dug ? 'works ' + (u.dugFor ?? 1) : 'no works'} ap ${u.ap}`), news: g.headlines.slice(-2).map((h) => h.text) }; }, pair);
          say('   AFTER THE ORDER:', JSON.stringify(after));
          await page.screenshot({ path: `shots/${shot}-${name}-3-done.png` });
        } else say('   no swap button on the card');
      }
    }
    if (what === 'cadre') {
      // Fold the first pair on offer, then read the cadre's card in the arsenal with the treasury full and with it short.
      const done = await page.evaluate(() => { const g = window.theater.game(), n = g.nations[g.human]; return { pp: Math.round(n.pp), cadres: (n.cadres ?? []).map((c) => c.name), queue: (n.queue ?? []).length }; });
      say('before:', JSON.stringify(done));
      if (!done.cadres.length) {
        const reg = Number(regArg);
        await page.evaluate((id) => window.theater.onRegion(id), reg); await page.waitForTimeout(600);
        const fb = page.locator('[data-act="foldcard"]').first();
        if (await fb.count()) { await fb.evaluate((e) => e.click()); await page.waitForTimeout(600); const go = page.locator('#card [data-act="fold"]').first(); if (await go.count()) { await go.click(); await page.waitForTimeout(700); } }
        const x = page.locator('#card [data-act="close"]').first(); if (await x.count() && await x.isVisible().catch(() => false)) { await x.click(); await page.waitForTimeout(300); }
      }
      const read = async (label) => {
        await page.evaluate(() => { const t = window.theater; t.ui.tab = 'build'; t.refresh(); }); await page.waitForTimeout(500);
        const c = page.locator('#pane [data-act="arsenal"][data-key^="cadre:"], #pane .rcard', { hasText: /a cadre/ }).first();
        if (!(await c.count())) { say(label, 'NO CADRE CARD; pane:', flat(await page.locator('#pane').innerText(), 500)); return null; }
        await c.scrollIntoViewIfNeeded().catch(() => {});
        say(label, flat(await c.innerText(), 900), '| disabled:', await c.isDisabled().catch(() => 'n/a'), '| class:', await c.getAttribute('class'), '| title:', flat((await c.getAttribute('title')) ?? '', 300));
        await page.screenshot({ path: `shots/${shot}-${name}-${label.replace(/\W+/g, '-')}.png` });
        return c;
      };
      await read('CARD WITH THE MONEY:');
      await page.evaluate(() => { const g = window.theater.game(); g.nations[g.human].pp = 1.36; window.theater.refresh(); });
      const c2 = await read('CARD WITH 136 IN THE TREASURY:');
      await page.evaluate(() => { const g = window.theater.game(); g.nations[g.human].pp = 90; window.theater.refresh(); });
      const c3 = await read('CARD WITH 9,000:');
      if (c3) {
        await c3.click(); await page.waitForTimeout(700);
        const after = await page.evaluate(() => { const g = window.theater.game(), n = g.nations[g.human]; return { pp: Math.round(n.pp), cadres: (n.cadres ?? []).map((c) => c.name), queue: (n.queue ?? []).map((b) => `${b.kind ?? b.key ?? ''} ${b.cadre !== undefined ? 'cadre ' + b.cadre : ''} due ${b.due ?? b.turn ?? ''}`), msg: window.theater.ui.msg }; });
        say('AFTER A CLICK ON IT:', JSON.stringify(after), '| card open:', flat(await page.locator('#card').innerText().catch(() => ''), 400));
        await page.screenshot({ path: `shots/${shot}-${name}-clicked.png` });
      }
    }
    if (errors.length) say('PAGE ERRORS:', errors.slice(0, 5).join(' || '));
    await page.close();
  }
  await browser.close();
})();
