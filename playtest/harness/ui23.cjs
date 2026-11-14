// Version 62: the fair cabinet's screens, as a player meets them, on a desk and on a phone. Loads a save made by probe34 and reads:
// the Cabinet card page by page (Business, Ministers, Decrees, Minutes) with its height in screens, the directive's odds line and
// the button that takes the player to a small job, the head block with the general's record; the men block of the ledger with
// the two call-up orders (the early class is asked twice); the level card with "Your office is secure" and the bill for
// switching the cabinet off; the arsenal's tooltips; and every "trench" still on the page.
// usage: [TC_PAGE=...] ui23.cjs SAVE.json SHOT-PREFIX [early,hold,secure,off]
const { chromium } = require('playwright');
const fs = require('fs');
const [file, shot, stepsS] = process.argv.slice(2);
const steps = (stepsS ?? '').split(',').filter(Boolean);
const flat = (s, n = 1600) => String(s).replace(/\n+/g, ' / ').replace(/\s+/g, ' ').slice(0, n);
(async () => {
  const browser = await chromium.launch();
  for (const [name, viewport, scale] of [['desk', { width: 1440, height: 900 }, 2], ['phone', { width: 390, height: 844 }, 3]]) {
    const page = await browser.newPage({ viewport, deviceScaleFactor: scale, isMobile: name === 'phone', hasTouch: name === 'phone' });
    const errors = [];
    const trench = new Set();
    page.on('pageerror', (e) => errors.push(e.message));
    page.on('console', (m) => { if (m.type() === 'error') errors.push('console: ' + m.text()); });
    await page.addInitScript((s) => { try { localStorage.setItem('tc-watch', '0'); localStorage.setItem('theater-command-save-v1', s); } catch {} }, fs.readFileSync(file, 'utf8'));
    await page.goto(process.env.TC_PAGE || 'file:///home/user/tc62/dist/play.html');
    await page.waitForTimeout(1000);
    const cont = page.locator('[data-act="continue"]').first();
    if (await cont.count()) await cont.click(); else await page.evaluate(() => window.theater.continueGame());
    await page.waitForTimeout(1500);
    const H = viewport.height;
    const say = (...a) => console.log(`[${name}]`, ...a);
    const st = async () => page.evaluate(() => { const g = window.theater.game(); const n = g.nations[g.human], v = n.gov || {}; return { turn: g.turn, human: g.human, pc: Math.round(n.pc * 10) / 10, trust: Math.round((v.trust ?? 0) * 10) / 10, atWar: n.atWar.length, flags: Object.keys(g.flags).filter((k) => /nofall|mph|mpe|cabinet/i.test(k)).map((k) => `${k}=${g.flags[k]}`).join(' ') }; });
    const block = async (sel, n = 1800) => { const e = page.locator(sel).first(); return (await e.count()) ? flat(await e.innerText(), n) : 'NONE'; };
    const sweep = async (where) => { const t = await page.evaluate(() => document.body.innerText + ' ' + [...document.querySelectorAll('[title]')].map((e) => e.getAttribute('title')).join(' ')); for (const m of t.matchAll(/[^.]{0,60}\btrench\w*[^.]{0,60}/gi)) trench.add(`${where}: ${m[0].replace(/\s+/g, ' ')}`); };
    const close = async () => { const c = page.locator('#card [data-act="close"]').first(); if ((await c.count()) && (await c.isVisible())) { await c.click(); await page.waitForTimeout(250); } else { await page.keyboard.press('Escape'); await page.waitForTimeout(250); } };
    const shown = async () => { const c = page.locator('#card').first(); if ((await c.count()) && (await c.isVisible())) return 'CARD: ' + flat(await c.innerText(), 300); const p = page.locator('#pane').first(); return (await p.count()) ? 'SIDE PANEL: ' + flat(await p.innerText(), 700) : 'nothing'; };
    say('loaded:', JSON.stringify(await st()));
    await sweep('map');
    const alerts = [...new Set(await page.locator('button.alert').evaluateAll((bs) => bs.map((b) => `[${b.dataset.kind}] ${b.textContent.trim()}`)))];
    say('ALERTS: ' + alerts.join(' || ').slice(0, 900));
    // ---- the Cabinet card, page by page
    const cab = page.locator('[data-act="cabinet"]:not(.dirline)').first();
    if (await cab.count()) {
      await cab.click(); await page.waitForTimeout(500);
      const tabs = await page.locator('.gvtabs button').evaluateAll((bs) => bs.map((b) => `${b.dataset.tab}:"${b.innerText.replace(/\s+/g, ' ').trim()}"${b.classList.contains('on') ? ' (open)' : ''}`));
      say('CABINET PAGES:', tabs.join(', ') || 'NONE');
      for (const tab of ['now', 'table', 'decrees', 'minutes']) {
        const b = page.locator(`.gvtabs [data-tab="${tab}"]`).first();
        if (!(await b.count())) continue;
        await b.click(); await page.waitForTimeout(300);
        const card = page.locator('.body.gov').first();
        const dims = await card.evaluate((e) => ({ scroll: e.scrollHeight, client: e.clientHeight }));
        say(`PAGE ${tab}: ${dims.scroll}px, ${(dims.scroll / H).toFixed(2)} screens of ${H}`);
        if (tab === 'now') {
          say('  TRUST:', await block('.gvmeter', 300), '|', await block('.body.gov .note', 500));
          say('  DIRECTIVE:', await block('.gvdir', 1400));
          say('  ODDS LINE:', await block('.gvodds', 500));
          say('  GO BUTTON:', await page.locator('[data-act="govgo"]').evaluateAll((bs) => bs.map((b) => `${b.dataset.to}:"${b.innerText.trim()}"`).join(', ')) || 'NONE');
          say('  HEAD:', await block('#gvhead', 1800));
          say('  RECORD:', await block('.gvrecord', 900));
          say('  PROMISE:', await block('#gvvow', 300));
        } else say('  TEXT:', (await block('.body.gov', 5000)).slice(tab === 'minutes' ? 0 : 0, 600));
        await sweep(`cabinet ${tab}`);
        await page.screenshot({ path: `shots/${shot}-${name}-cab-${tab}.png` });
      }
      // a small job's button takes the player where the job is done
      await page.locator('.gvtabs [data-tab="now"]').first().click().catch(() => {}); await page.waitForTimeout(200);
      const go = page.locator('[data-act="govgo"]').first();
      if (await go.count()) { await go.click(); await page.waitForTimeout(500); say('AFTER THE GO BUTTON:', await shown()); await page.screenshot({ path: `shots/${shot}-${name}-go.png` }); }
      await close();
    } else say('no cabinet button');
    // ---- the men block of the ledger and the two orders
    const men = page.locator('.purse.store.men').first();
    if (await men.count()) {
      say('MEN GAUGE:', flat(await men.innerText(), 200), '| tip:', flat(await men.getAttribute('title'), 400));
      await men.click(); await page.waitForTimeout(500);
      say('MEN BLOCK:', await block('.lfacts.men', 600));
      say('MEN NOW:', await block('.mennow', 300));
      say('CALL-UP ORDERS:', await block('.menorders', 1400));
      const e = page.locator('.menorders').first(); if (await e.count()) { await e.scrollIntoViewIfNeeded(); await page.waitForTimeout(150); }
      await page.screenshot({ path: `shots/${shot}-${name}-men.png` });
      await sweep('ledger');
      if (steps.includes('early')) {
        const b = page.locator('[data-act="menearly"]:not([data-sure])').first();
        if (await b.count() && !(await b.isDisabled())) {
          const before = await page.evaluate(() => { const g = window.theater.game(); return Object.entries(g.flags).filter(([k]) => /^(mp|men)/.test(k) && k.endsWith(g.human)).map(([k, v]) => `${k}=${Math.round(v * 10) / 10}`).join(' '); });
          await b.click(); await page.waitForTimeout(400);
          say('EARLY CLASS ASKED:', await block('.menorder.asking', 900));
          await page.locator('.menorder.asking').first().scrollIntoViewIfNeeded().catch(() => {}); await page.screenshot({ path: `shots/${shot}-${name}-men-ask.png` });
          const yes = page.locator('.menorder.asking [data-act="menearly"], .menorder.asking .btn.warn, .menorder.asking .btn.go').first();
          if (await yes.count()) { await yes.click(); await page.waitForTimeout(500); const after = await page.evaluate(() => { const g = window.theater.game(); return Object.entries(g.flags).filter(([k]) => /^(mp|men)/.test(k) && k.endsWith(g.human)).map(([k, v]) => `${k}=${Math.round(v * 10) / 10}`).join(' '); }); say('EARLY CLASS CALLED: flags before', before, '| after', after); say('MEN BLOCK NOW:', await block('.lfacts.men', 600), '|', await block('.mennow', 300)); say('ORDERS NOW:', await block('.menorders', 900)); await page.screenshot({ path: `shots/${shot}-${name}-men-called.png` }); }
        } else say('the early class cannot be called here:', await block('.menorders', 700));
      }
      if (steps.includes('hold')) {
        const b = page.locator('[data-act="menhold"]').first();
        if (await b.count()) { say('HOLD BUTTON:', flat(await b.innerText(), 80), await b.isDisabled() ? '(disabled)' : ''); if (!(await b.isDisabled())) { await b.click(); await page.waitForTimeout(400); say('AFTER HOLD:', await block('.menorders', 900)); await page.screenshot({ path: `shots/${shot}-${name}-men-held.png` }); } }
        else say('no standing call-up to hold here');
      }
      await close();
    } else say('no men gauge');
    // ---- the level card: the office and the cabinet's switch
    const lvl = page.locator('[data-act="level"]').first();
    if (await lvl.count()) {
      await lvl.click(); await page.waitForTimeout(500);
      say('LEVEL CARD LEAD:', await block('#card .lead', 400));
      say('OFFICE:', await block('#setoffice ~ .gclevels', 500), '|', await block('#setoffice ~ .gclevelnote, #setoffice ~ .gclevels ~ .gclevelnote', 600));
      const dims = await page.locator('#card .body').first().evaluate((e) => ({ scroll: e.scrollHeight, client: e.clientHeight }));
      say(`LEVEL CARD: ${dims.scroll}px, ${(dims.scroll / H).toFixed(2)} screens`);
      await page.locator('#setoffice').first().scrollIntoViewIfNeeded().catch(() => {}); await page.screenshot({ path: `shots/${shot}-${name}-office.png` });
      await sweep('level card');
      if (steps.includes('secure')) {
        const b = page.locator('[data-act="setoffice"][data-secure="1"]').first();
        if (await b.count()) { await b.click(); await page.waitForTimeout(400); say('SECURE CLICKED:', JSON.stringify(await st())); say('OFFICE NOW:', await block('#setoffice ~ .gclevelnote, #setoffice ~ .gclevels ~ .gclevelnote', 600)); await page.locator('#setoffice').first().scrollIntoViewIfNeeded().catch(() => {}); await page.screenshot({ path: `shots/${shot}-${name}-office-secure.png` }); }
        else say('no office buttons');
      }
      if (steps.includes('off')) {
        const b = page.locator('[data-act="setcabinet"][data-on="0"]:not([data-sure])').first();
        if (await b.count()) { await b.click(); await page.waitForTimeout(400); say('SWITCH OFF ASKED:', await block('#card .note.warn', 1200)); say('STATE:', JSON.stringify(await st())); await page.locator('.gvbill').first().scrollIntoViewIfNeeded().catch(() => {}); await page.screenshot({ path: `shots/${shot}-${name}-off-ask.png` });
          const yes = page.locator('[data-act="setcabinet"][data-sure="1"]').first();
          if (await yes.count()) { await yes.click(); await page.waitForTimeout(400); say('SWITCHED OFF:', JSON.stringify(await st()), '|', await block('#card .lead', 300)); const on = page.locator('[data-act="setcabinet"][data-on="1"]').first(); if (await on.count()) { await on.click(); await page.waitForTimeout(400); say('ON AGAIN:', JSON.stringify(await st())); } }
          else say('no bill was shown: the switch took hold at once?', JSON.stringify(await st()));
        }
      }
      await close();
    }
    // after the secure click and the off-and-on: the cabinet card again
    if (steps.includes('secure') || steps.includes('off')) {
      const c2 = page.locator('[data-act="cabinet"]:not(.dirline)').first();
      if (await c2.count()) { await c2.click(); await page.waitForTimeout(400); say('CABINET AFTER:', await block('.gvmeter', 300), '|', await block('.gvscale', 200), '| DIRECTIVE:', await block('.gvdir', 400), '| HEAD:', await block('#gvhead', 200), '| PROMISE:', await block('#gvvow', 200)); await page.screenshot({ path: `shots/${shot}-${name}-cab-after.png` }); await close(); }
    }
    // ---- the arsenal's tooltips
    const ars = page.locator('#tabs [data-act="tab"][data-tab="build"]').first();
    if (await ars.count()) { await ars.click().catch(() => {}); await page.waitForTimeout(500); const tips = await page.locator('.rcard').evaluateAll((bs) => bs.slice(0, 40).map((b) => b.getAttribute('title') || '')); say(`ARSENAL: ${tips.length} cards, ${tips.filter((t) => !t).length} with an empty tooltip; first: ${flat(tips[0] ?? '', 260)}`); await sweep('arsenal'); await close(); }
    else say('no arsenal button found');
    say('TRENCH still on the page:', trench.size ? [...trench].slice(0, 12).join(' || ') : 'nowhere');
    say('page errors:', errors.length ? errors.slice(0, 5).join(' | ') : 'none');
    await page.close();
  }
  await browser.close();
})();
