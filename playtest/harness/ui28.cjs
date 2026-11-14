// Version 66: page checks of how deep you play, on a desk and two phones.
//   start:  the new-campaign card: the depth picker (which is offered first, what each says), how long the card is
//           in screens and how far down the Begin button sits, then a campaign begun at the depth offered.
//   staff:  a campaign at each depth, three months in: which tabs show and which are tucked away, the Staff tab
//           (how long, anything wider than the screen, the smallest buttons, what it says), "Look in" on a tucked
//           panel and its bar, "Take it back", the allowance picker, the settings card behind the badge (how long,
//           its sections), a change of depth from the card, and the staff's lines on the News tab.
// usage: [TC_PAGE=...] ui28.cjs start|staff SHOT-PREFIX [SEED] [NAT]
const { chromium } = require('playwright');
const [what, shot, seedArg, natArg] = process.argv.slice(2);
const seed = Number(seedArg || 7), nat = natArg || 'GER';
const flat = (s, n = 1500) => String(s).replace(/\n+/g, ' / ').replace(/\s+/g, ' ').slice(0, n);
const SIZES = [['desk', { width: 1440, height: 900 }, false], ['phone', { width: 390, height: 844 }, true], ['small-phone', { width: 360, height: 640 }, true]];
// How a scrolling box fits its screen: its length in screens, anything in it wider than the screen, and its smallest buttons.
const measure = (page, sel) => page.evaluate((sel) => {
  const box = document.querySelector(sel);
  if (!box) return null;
  const r = box.getBoundingClientRect();
  const wide = [];
  for (const e of box.querySelectorAll('*')) { const q = e.getBoundingClientRect(); if (q.width > 0 && (q.right > innerWidth + 1 || q.left < -1)) { wide.push(`${e.tagName.toLowerCase()}.${(e.className || '').toString().split(' ')[0]} ${Math.round(q.left)}..${Math.round(q.right)}`); if (wide.length > 4) break; } }
  const btns = [...box.querySelectorAll('button, .btn, a')].filter((b) => b.offsetParent !== null).map((b) => { const q = b.getBoundingClientRect(); return { t: b.textContent.trim().slice(0, 28), w: Math.round(q.width), h: Math.round(q.height) }; });
  const small = btns.filter((b) => b.h < 32 || b.w < 32);
  return { screen: `${innerWidth}x${innerHeight}`, top: Math.round(r.top), shown: Math.round(Math.min(r.bottom, innerHeight) - r.top), long: box.scrollHeight, screens: Math.round((box.scrollHeight / Math.max(1, Math.min(r.bottom, innerHeight) - r.top)) * 10) / 10, ofWholeScreen: Math.round((box.scrollHeight / innerHeight) * 10) / 10, widerThanScreen: wide, sideways: box.scrollWidth > box.clientWidth + 1, buttons: btns.length, under32: small.length, smallest: small.sort((a, b) => a.h - b.h).slice(0, 5).map((b) => `${b.t} ${b.w}x${b.h}`) };
}, sel);
(async () => {
  const browser = await chromium.launch();
  for (const [name, vp, touch] of SIZES) {
    const say = (...a) => console.log(`[${name}]`, ...a);
    const errors = [];
    const fresh = async () => {
      const page = await browser.newPage({ viewport: vp, deviceScaleFactor: 2, isMobile: touch, hasTouch: touch });
      page.on('pageerror', (e) => errors.push(e.message));
      page.on('console', (m) => { if (m.type() === 'error') errors.push('console: ' + m.text()); });
      await page.addInitScript(() => { try { localStorage.clear(); localStorage.setItem('tc-watch', '0'); } catch {} });
      await page.goto(process.env.TC_PAGE || 'file:///home/user/tc66/dist/play.html');
      await page.waitForTimeout(1000);
      return page;
    };
    if (what === 'start') {
      const page = await fresh();
      await page.locator(`[data-act="gcnat"][data-nat="${nat}"]`).first().click(); await page.waitForTimeout(500);
      const heads = await page.locator('#card h3, #card h2').evaluateAll((hs) => hs.map((h) => h.textContent.trim()));
      say('START CARD sections in order:', JSON.stringify(heads));
      const depths = await page.locator('[data-act="gcdepth"]').evaluateAll((bs) => bs.map((b) => `${b.textContent.trim().replace(/\s+/g, ' ')}${b.classList.contains('on') ? ' [ON]' : ''} ${Math.round(b.getBoundingClientRect().width)}x${Math.round(b.getBoundingClientRect().height)}`));
      say('depth picker:', JSON.stringify(depths));
      say('the card:', JSON.stringify(await measure(page, '#card')));
      const where = await page.evaluate(() => { const c = document.querySelector('#card'); const y = (sel) => { const e = c.querySelector(sel); return e ? Math.round(e.getBoundingClientRect().top - c.getBoundingClientRect().top + c.scrollTop) : null; }; return { depthPickerAt: y('[data-act="gcdepth"]'), levelPickerAt: y('[data-act="gclevel"]'), beginAt: y('[data-act="start"]'), cardShown: Math.round(c.clientHeight), beginStuck: (() => { const b = c.querySelector('[data-act="start"]'); if (!b) return null; const q = b.getBoundingClientRect(); return q.top < innerHeight && q.bottom > 0; })() }; });
      say('where things sit in the card (points from its top):', JSON.stringify(where));
      await page.screenshot({ path: `${shot}-${name}-1-start-card.png` });
      for (const d of ['field', 'theatre', 'supreme']) {
        await page.locator(`[data-act="gcdepth"][data-depth="${d}"]`).click(); await page.waitForTimeout(250);
        const note = await page.locator('[data-act="gcdepth"]').first().evaluate((b) => { const n = b.parentElement.nextElementSibling; return n ? n.innerText : ''; });
        say(`picked ${d}:`, flat(note, 700));
        if (d === 'field') { await page.locator('[data-act="gcdepth"]').first().scrollIntoViewIfNeeded(); await page.screenshot({ path: `${shot}-${name}-2-start-depth-field.png` }); }
      }
      await page.locator('[data-act="gcdepth"][data-depth="theatre"]').click(); await page.waitForTimeout(200);
      await page.locator('[data-act="gcdepth"]').first().scrollIntoViewIfNeeded(); await page.screenshot({ path: `${shot}-${name}-3-start-depth-theatre.png` });
      await page.locator('[data-act="start"]').first().click(); await page.waitForTimeout(2500);
      const st = await page.evaluate(() => { const g = window.theater.game(); return { turn: g.turn, human: g.human, desks: Object.keys(g.flags).filter((k) => k.startsWith('dk:')), nofog: g.flags.nofog ?? 0, nogov: g.flags.nogov ?? 0, eve: g.flags.eve ?? 0 }; });
      say('started:', JSON.stringify(st));
      say('tabs:', JSON.stringify(await page.locator('#tabs [data-act="tab"]').evaluateAll((bs) => bs.filter((b) => b.offsetParent !== null).map((b) => b.textContent.trim().replace(/\s+/g, ' ')))));
      say('badge beside the date:', flat(await page.locator('.rank').first().innerText().catch(() => 'none'), 120));
      await page.screenshot({ path: `${shot}-${name}-4-started.png` });
      say('page errors:', errors.length ? JSON.stringify(errors.slice(0, 4)) : 'none');
      await page.close();
      continue;
    }
    for (const depth of ['field', 'theatre', 'supreme']) {
      const page = await fresh();
      const tag = `${name}-${depth}`;
      await page.evaluate(([nat, seed, depth]) => window.theater.newGame(nat, seed, 1, 'grand', 'colonel', true, false, depth), [nat, seed, depth]);
      await page.waitForTimeout(1500);
      // Three months in, so that the staff has lines to show.
      const shut = async () => { for (let k = 0; k < 8; k++) { const open = await page.evaluate(() => { const v = document.querySelector('#veil'); if (!v || !v.classList.contains('open')) return false; window.theater.openModal(null); return true; }); if (!open) break; await page.waitForTimeout(150); } };
      await shut();
      for (let i = 0; i < 4; i++) { await page.evaluate(() => window.theater.nextMonth()); await page.waitForTimeout(2500); await shut(); }
      await page.waitForTimeout(400);
      const st = await page.evaluate(() => { const g = window.theater.game(); return { turn: g.turn, human: g.human, staffed: Object.keys(g.flags).filter((k) => /^dk:[a-z]+$/.test(k) && k !== 'dk:cap'), cap: g.flags['dk:cap'] ?? 50, fog: g.flags.nofog !== 1, treasury: Math.round(g.nations[g.human].pp * 100) }; });
      say(`${depth.toUpperCase()}:`, JSON.stringify(st));
      const tabs = () => page.locator('#tabs [data-act="tab"]').evaluateAll((bs) => bs.filter((b) => b.offsetParent !== null).map((b) => b.textContent.trim().replace(/\s+/g, ' ')));
      say(`${depth} tabs:`, JSON.stringify(await tabs()));
      say(`${depth} tab bar:`, JSON.stringify(await page.evaluate(() => { const t = document.querySelector('#tabs'); const r = t.getBoundingClientRect(); return { wide: t.scrollWidth, shown: Math.round(r.width), sideways: t.scrollWidth > t.clientWidth + 1, high: Math.round(r.height) }; })));
      const staffTab = page.locator('#tabs [data-act="tab"][data-tab="staff"]');
      if (!(await staffTab.count())) { say(`${depth}: NO STAFF TAB`); await page.close(); continue; }
      await staffTab.first().scrollIntoViewIfNeeded(); await staffTab.first().click(); await page.waitForTimeout(500);
      say(`${depth} Staff tab:`, JSON.stringify(await measure(page, '#pane')));
      say(`${depth} Staff tab says:`, flat(await page.locator('#pane').innerText(), 2600));
      await page.screenshot({ path: `${shot}-${tag}-1-staff-tab.png` });
      await page.locator('#pane').evaluate((p) => { p.scrollTop = p.scrollHeight; }); await page.waitForTimeout(200);
      await page.screenshot({ path: `${shot}-${tag}-2-staff-tab-foot.png` });
      // The allowance picker.
      const cap = page.locator('#pane [data-act="setcap"][data-cap="75"]');
      if (await cap.count()) { await cap.click(); await page.waitForTimeout(300); say(`${depth} allowance set to 75:`, flat(await page.locator('#pane .gclevelnote').last().innerText(), 500), '| flag', await page.evaluate(() => window.theater.game().flags['dk:cap'])); await page.locator('#pane [data-act="setcap"][data-cap="50"]').click(); await page.waitForTimeout(200); }
      // Look in on a tucked panel, and take it back.
      const look = page.locator('#pane .desk.staffed [data-act="tab"]').first();
      if (await look.count()) {
        const which = await look.getAttribute('data-tab');
        await look.scrollIntoViewIfNeeded(); await look.click(); await page.waitForTimeout(500);
        say(`${depth} looked in on ${which}; tabs now:`, JSON.stringify(await tabs()));
        say(`${depth} the bar on that panel:`, flat(await page.locator('#pane .deskbar').first().innerText().catch(() => 'NO BAR'), 500), JSON.stringify(await page.locator('#pane .deskbar').first().evaluate((b) => { const q = b.getBoundingClientRect(); return { h: Math.round(q.height), ofScreen: Math.round((100 * q.height) / innerHeight) }; }).catch(() => null)));
        await page.screenshot({ path: `${shot}-${tag}-3-look-in-${which}.png` });
        const back = page.locator('#pane .deskbar [data-act="desk"][data-on="0"]').first();
        if (await back.count()) { await back.click(); await page.waitForTimeout(500); say(`${depth} took ${which} back; staffed now:`, JSON.stringify(await page.evaluate(() => Object.keys(window.theater.game().flags).filter((k) => /^dk:[a-z]+$/.test(k) && k !== 'dk:cap'))), '| tabs', JSON.stringify(await tabs()), '| badge', flat(await page.locator('.rank').first().innerText().catch(() => ''), 80)); const foot = page.locator('#pane .deskfoot').first(); say(`${depth} foot of the panel:`, flat(await foot.innerText().catch(() => 'NO FOOT'), 200)); if (await foot.count()) { await foot.locator('button').click(); await page.waitForTimeout(400); say(`${depth} handed it over again; staffed:`, JSON.stringify(await page.evaluate(() => Object.keys(window.theater.game().flags).filter((k) => /^dk:[a-z]+$/.test(k) && k !== 'dk:cap')))); } }
      }
      // The News tab: the staff's lines and their links.
      const news = page.locator('#tabs [data-act="tab"][data-tab="news"]');
      if (await news.count()) { await news.first().click(); await page.waitForTimeout(400); const ls = await page.locator('#pane [data-act="desk"][data-on="0"]').evaluateAll((bs) => bs.map((b) => (b.parentElement ? b.parentElement.textContent.trim().replace(/\s+/g, ' ').slice(0, 200) : ''))); say(`${depth} staff lines on the News tab with a way to take the desk back: ${ls.length}`, JSON.stringify(ls.slice(0, 4))); await page.screenshot({ path: `${shot}-${tag}-4-news.png` }); }
      // The settings card behind the badge.
      const badge = page.locator('.rank').first();
      if (await badge.count()) {
        await badge.click(); await page.waitForTimeout(500);
        say(`${depth} settings card:`, JSON.stringify(await measure(page, '#card')));
        say(`${depth} settings card sections:`, JSON.stringify(await page.locator('#card h2, #card h3').evaluateAll((hs) => hs.map((h) => h.textContent.trim()))));
        await page.screenshot({ path: `${shot}-${tag}-5-settings.png` });
        const dp = page.locator('#card [data-act="setdepth"]').first();
        if (await dp.count()) { await dp.scrollIntoViewIfNeeded(); await page.screenshot({ path: `${shot}-${tag}-6-settings-depth.png` }); }
        // Change the depth from the card: to the next one down the list, and say what the card asks.
        const to = depth === 'field' ? 'supreme' : 'field';
        const pick = page.locator(`#card [data-act="setdepth"][data-depth="${to}"]`).first();
        if (await pick.count()) {
          await pick.click(); await page.waitForTimeout(500);
          const ask = await page.locator('#card .note.warn').first().innerText().catch(() => '');
          say(`${depth} to ${to}:`, ask ? `the card asks: ${flat(ask, 500)}` : 'no question asked');
          if (ask) { await page.screenshot({ path: `${shot}-${tag}-7-settings-ask.png` }); await page.locator('#card [data-act="setdepth"][data-sure="1"]').first().click(); await page.waitForTimeout(500); }
          say(`${depth} to ${to}, now:`, JSON.stringify(await page.evaluate(() => { const g = window.theater.game(); const v = window.theater.game && g ? { staffed: Object.keys(g.flags).filter((k) => /^dk:[a-z]+$/.test(k) && k !== 'dk:cap'), nofog: g.flags.nofog ?? 0, nogov: g.flags.nogov ?? 0 } : null; return v; })), '| badge', flat(await page.locator('.rank').first().innerText().catch(() => ''), 80));
        }
      }
      say(`${depth} page errors:`, errors.length ? JSON.stringify(errors.slice(0, 4)) : 'none');
      await page.close();
    }
  }
  await browser.close();
})();
