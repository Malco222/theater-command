// A saved game opened on the page and the months ended from there, to read the month cards a player is shown near the
// end of a campaign: is he warned in a red band before his country falls? usage: node ui58.cjs SAVE.json PREFIX [MONTHS=2]
const fs = require('fs');
const { chromium } = require('playwright');
const PAGE = process.env.TC_PAGE || 'file:///home/user/tc823/dist/play.html';
const [save, prefix, monthsS] = process.argv.slice(2);
(async () => {
  const browser = await chromium.launch(); const errors = [];
  const page = await (await browser.newContext({ viewport: { width: 1440, height: 900 }, deviceScaleFactor: 2 })).newPage();
  page.on('pageerror', (e) => errors.push(e.message.slice(0, 300)));
  await page.goto(PAGE);
  await page.evaluate((text) => { localStorage.clear(); localStorage.setItem('tc-watch', '0'); localStorage.setItem('tc-ask-end', '0'); localStorage.setItem('theater-command-save-v1', text); }, fs.readFileSync(save, 'utf8'));
  await page.reload(); await page.waitForSelector('#card'); await page.evaluate(() => window.theater.continueGame()); await page.waitForTimeout(2500);
  const state = () => page.evaluate(() => { const g = window.theater.game(); const me = g.human; const n = me ? g.nations[me] : null; return { turn: g.turn, me, alive: n ? n.alive : null, corps: me ? g.corps.filter((c) => c.nat === me && !c.dead && c.kind !== 'fort').length : 0, prov: me ? g.regions.filter((r) => r.ctrl === me).length : 0, home: me ? g.regions.filter((r) => r.owner === me && r.ctrl === me).length : 0, cap: me && n ? g.regions[n.capital]?.ctrl : null }; });
  const veil = () => page.evaluate(() => { const v = document.querySelector('#veil'); return !!v && v.classList.contains('open'); });
  const card = () => page.evaluate(() => ({ head: (document.querySelector('#card .mc-head, #card h1, #card h2')?.innerText || '').replace(/\s+/g, ' ').trim(), bands: [...document.querySelectorAll('#card .mc-banner, #card .mc-photo.stake, #card .band, #card [class*="warn"]')].map((e) => `${e.className}: ${(e.innerText || '').replace(/\s+/g, ' ').trim().slice(0, 260)}`), text: (document.querySelector('#card').innerText || '').replace(/\s+/g, ' ').trim() }));
  console.log('loaded: ' + JSON.stringify(await state()));
  if (await veil()) { await page.evaluate(() => window.theater.openModal(null)); await page.waitForTimeout(300); }
  for (let m = 0; m < Number(monthsS || 2); m++) {
    const before = await state();
    const btn = await page.evaluate(() => { const b = document.querySelector('[data-act="end"]'); if (!b) return null; const t = (b.innerText || '').trim(); b.click(); return t; });
    if (!btn) { console.log('no END MONTH button: ' + JSON.stringify(before)); break; }
    await page.waitForTimeout(600);
    // The question before the month ends, if it is asked.
    await page.evaluate(() => { const b = [...document.querySelectorAll('#card button')].find((x) => /end the month/i.test(x.innerText || '')); if (b) b.click(); });
    let st = before; const t0 = Date.now();
    while (Date.now() - t0 < 90000) { await page.waitForTimeout(700); st = await state(); if (st.turn !== before.turn || st.me !== before.me || st.alive === false) { await page.waitForTimeout(1500); break; } }
    st = await state();
    console.log(`after "${btn}": ` + JSON.stringify(st));
    if (await veil()) {
      const c = await card();
      console.log(`  the card: head "${c.head}"; red bands ${JSON.stringify(c.bands)}`);
      console.log(`  the card's words: ${c.text.slice(0, 1300)}`);
      await page.screenshot({ path: `shots/${prefix}-t${st.turn}-card.png` });
      // Close month cards with their own button; a fall card stays.
      const closed = await page.evaluate(() => { const b = [...document.querySelectorAll('#card button')].find((x) => /give orders|continue|close/i.test(x.innerText || '')); if (b) { const t = (b.innerText || '').trim(); b.click(); return t; } return null; });
      await page.waitForTimeout(600);
      if (await veil()) { const c2 = await card(); if (c2.text !== c.text) { console.log(`  then, after "${closed}": ${c2.text.slice(0, 900)}`); await page.screenshot({ path: `shots/${prefix}-t${st.turn}-card2.png` }); } await page.evaluate(() => window.theater.openModal(null)); await page.waitForTimeout(300); }
    } else console.log('  no card shown');
    if (st.alive === false || st.me === null) { console.log('the player is out: ' + JSON.stringify(st)); break; }
  }
  console.log('page errors: ' + JSON.stringify(errors));
  await browser.close();
})();
