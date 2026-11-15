// The month as the player meets it: loads a save, presses the End month BUTTON (never window.theater.nextMonth(), which
// is the watcher's step and opens no Dispatches card), and reads every card that opens by itself before closing it: the
// Dispatches card with its event cards at the head and its plain lines below, then any card that waits behind it.
// usage: [TC_PAGE=...] node ui37.cjs SAVE.json PREFIX [MONTHS=1] [W H]
const { chromium } = require('playwright');
const fs = require('fs');
const [save, prefix, monthsS = '1', W = '1440', H = '900'] = process.argv.slice(2);
(async () => {
  const browser = await chromium.launch();
  const phone = Math.min(+W, +H) < 500;
  const ctx = await browser.newContext({ viewport: { width: +W, height: +H }, deviceScaleFactor: 2, isMobile: phone, hasTouch: phone });
  const page = await ctx.newPage();
  const errors = [];
  page.on('pageerror', (e) => errors.push(e.message));
  await page.addInitScript((s) => { try { if (!sessionStorage.getItem('tc-once')) { sessionStorage.setItem('tc-once', '1'); localStorage.clear(); localStorage.setItem('tc-watch', '0'); localStorage.setItem('theater-command-save-v1', s); } } catch {} }, fs.readFileSync(save, 'utf8'));
  await page.goto(process.env.TC_PAGE || 'file:///home/user/tc72/dist/play.html'); await page.waitForTimeout(1000);
  const c = page.locator('[data-act="continue"]').first(); if (await c.count()) await c.click(); else await page.evaluate(() => window.theater.continueGame());
  await page.waitForTimeout(1500);
  const open = () => page.evaluate(() => { const v = document.querySelector('#veil'); return !!v && v.classList.contains('open'); });
  const readCard = () => page.evaluate(() => {
    const card = document.querySelector('#card'); if (!card) return null;
    const kick = card.querySelector('.kick')?.textContent.trim() ?? '';
    const head = card.querySelector('h2')?.textContent.trim() ?? '';
    const ev = [...card.querySelectorAll('.evcard')].map((x) => ({ cls: x.className, label: x.querySelector('.ek')?.textContent.trim() ?? '', text: x.querySelector('p')?.textContent.trim().slice(0, 200) ?? '', button: x.querySelector('button')?.textContent.trim() ?? '' }));
    const reel = [...card.querySelectorAll('.reel p')].map((x) => x.textContent.replace(/\s+/g, ' ').trim());
    const hit = reel.map((x, i) => (/presses its claim|marches for|declares war on|warned that it may march|book is closed/i.test(x) ? `line ${i + 1} of ${reel.length}: ${x.slice(0, 130)}` : '')).filter(Boolean);
    const buttons = [...card.querySelectorAll('button[data-act]')].map((b) => `${b.dataset.act} "${b.textContent.trim().slice(0, 30)}"`).slice(0, 10);
    return { kick, head, eventCards: ev, plainLines: reel.length, dialLinesAmongPlain: hit, buttons, text: ev.length || reel.length ? '' : card.innerText.replace(/\s+/g, ' ').slice(0, 400) };
  });
  // Cards left over from the load are closed without counting.
  for (let k = 0; k < 12 && await open(); k++) { await page.evaluate(() => window.theater.openModal(null)); await page.waitForTimeout(200); }
  for (let m = 1; m <= Number(monthsS); m++) {
    const t = await page.evaluate(() => window.theater.game().turn);
    await page.locator('[data-act="end"]').first().click();
    await page.waitForFunction((t0) => window.theater.game().turn > t0, t, { timeout: 240000 });
    await page.waitForTimeout(1500);
    const now = await page.evaluate(() => { const g = window.theater.game(); return { turn: g.turn, wars: g.nations[g.human].atWar.filter((e) => g.nations[e].alive).join(' ') }; });
    console.log(`month ${m}: turn ${t} to ${now.turn}; at war with: ${now.wars}`);
    let n = 0;
    while (await open() && n < 10) {
      n++;
      const card = await readCard();
      console.log(`  card ${n} that opened by itself: ${JSON.stringify(card)}`);
      await page.screenshot({ path: `shots/${prefix}-m${m}-card${n}.png` });
      // Close it the way a player does, so that whatever waits behind it opens.
      const x = page.locator('#card [data-act="close"]').first();
      if (await x.count()) await x.click(); else await page.evaluate(() => window.theater.openModal(null));
      await page.waitForTimeout(700);
    }
    if (!n) console.log('  no card opened by itself');
    const chips = await page.evaluate(() => [...document.querySelectorAll('.nudge, .todo, .hint, .chipnote, [data-act="nudge"]')].map((x) => x.textContent.replace(/\s+/g, ' ').trim()).slice(0, 12));
    console.log('  notices beside the map:', JSON.stringify(chips));
    await page.screenshot({ path: `shots/${prefix}-m${m}-map.png` });
  }
  console.log('page errors:', errors.length ? JSON.stringify(errors.slice(0, 4)) : 'none');
  await browser.close();
})();
