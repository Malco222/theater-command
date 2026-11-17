// Version 84: the desert from an Axis seat, by hand on the page. Opens a save, takes the army on the desert line in
// hand, reads what the page tells the player there (the corps tags such as "Digging", the words on the province and
// the army about the enemy across the line), then ends MONTHS months by the page's own button and reads each month card.
// usage: [TC_PAGE=...] node ui62.cjs SAVE.json PREFIX [MONTHS=2] [PROVINCE-NAME]
const fs = require('fs');
const { chromium } = require('playwright');
const PAGE = process.env.TC_PAGE || 'file:///home/user/tc84/dist/play.html';
const [save, prefix, monthsS, provS] = process.argv.slice(2);
const clean = (s) => (s || '').replace(/\s+/g, ' ').trim();
(async () => {
  const browser = await chromium.launch(); const errors = [];
  const page = await (await browser.newContext({ viewport: { width: 1440, height: 900 }, deviceScaleFactor: 2 })).newPage();
  page.on('pageerror', (e) => errors.push(e.message.slice(0, 300)));
  await page.goto(PAGE);
  await page.evaluate((text) => { localStorage.clear(); localStorage.setItem('tc-watch', '0'); localStorage.setItem('tc-ask-end', '0'); localStorage.setItem('theater-command-save-v1', text); }, fs.readFileSync(save, 'utf8'));
  await page.reload(); await page.waitForSelector('#card'); await page.evaluate(() => { try { window.theater.continueGame(); } catch (e) {} }); await page.waitForTimeout(2500);
  const veil = () => page.evaluate(() => { const v = document.querySelector('#veil'); return !!v && v.classList.contains('open'); });
  const closeAll = async () => { for (let i = 0; i < 4 && await veil(); i++) { await page.evaluate(() => window.theater.openModal(null)); await page.waitForTimeout(300); } };
  await closeAll();
  const look = async (tag) => {
    const spot = await page.evaluate((want) => {
      const g = window.theater.game(), me = g.human; const rows = [];
      for (const r of g.regions) { if (r.terrain !== 'desert' && !(want && r.name === want)) continue; const mine = r.units.filter((c) => c.nat === me && !c.dead); if (!mine.length) continue; const foes = r.nb.map((j) => g.regions[j]).filter((t) => t.ctrl !== me && t.units.some((c) => !c.dead && c.nat === t.ctrl)); rows.push({ id: r.id, name: r.name, n: mine.length, foes: foes.map((t) => `${t.name} (${t.ctrl})`), dug: mine.map((c) => `${c.name}: dug ${c.dug ?? c.entrench ?? c.digs ?? '?'}`) }); }
      rows.sort((a, b) => (want ? (b.name === want) - (a.name === want) : 0) || b.foes.length - a.foes.length || b.n - a.n);
      return { turn: g.turn, me, rows: rows.slice(0, 5) };
    }, provS || '');
    console.log(`\n${tag}: ${spot.me} turn ${spot.turn}; my corps on desert ground: ${spot.rows.map((r) => `${r.name} ${r.n}${r.foes.length ? ' facing ' + r.foes.join(', ') : ''}`).join('; ') || 'none'}`);
    if (!spot.rows.length) return;
    const r = spot.rows[0];
    await page.evaluate((id) => window.theater.onRegion(id), r.id); await page.waitForTimeout(600);
    const seen = await page.evaluate(() => { const t = (e) => (e ? e.innerText.replace(/\s+/g, ' ').trim() : ''); const all = [...document.querySelectorAll('#tray, #pane, #side, #info')]; const text = all.map(t).join(' || '); const tags = [...document.querySelectorAll('#tray *, #pane *')].filter((e) => e.children.length === 0 && /^(Digging|Dug in|Boxed|In a box|Open desert)/i.test((e.innerText || '').trim())).map((e) => `${e.className || e.tagName}: ${t(e)}${e.getAttribute('title') ? ' [hover: ' + e.getAttribute('title').slice(0, 160) + ']' : ''}`); const hovers = [...new Set([...document.querySelectorAll('#tray [title], #pane [title]')].map((e) => e.getAttribute('title')).filter((x) => /dig|desert|dug|box|open ground|armour/i.test(x || '')).map((x) => x.slice(0, 200)))].slice(0, 8); return { text: text.slice(0, 1500), tags: tags.slice(0, 10), hovers }; });
    console.log(`  in hand at ${r.name}: tags ${JSON.stringify(seen.tags)}`);
    console.log(`  hover words about digging and desert: ${JSON.stringify(seen.hovers)}`);
    console.log(`  what the page says there: ${seen.text}`);
    console.log(`  the rules' own count: ${r.dug.slice(0, 6).join('; ')}`);
    await page.screenshot({ path: `shots/${prefix}-${tag}-army.png` });
  };
  await look('start');
  for (let m = 0; m < Number(monthsS || 2); m++) {
    await closeAll();
    const before = await page.evaluate(() => window.theater.game().turn);
    const btn = await page.evaluate(() => { const b = document.querySelector('[data-act="end"]'); if (!b) return null; const t = (b.innerText || '').trim(); b.click(); return t; });
    if (!btn) { console.log('no END MONTH button'); break; }
    await page.waitForTimeout(600);
    await page.evaluate(() => { const b = [...document.querySelectorAll('#card button')].find((x) => /end the month/i.test(x.innerText || '')); if (b) b.click(); });
    const t0 = Date.now(); let now = before;
    while (Date.now() - t0 < 120000) { await page.waitForTimeout(700); now = await page.evaluate(() => window.theater.game().turn); if (now !== before) { await page.waitForTimeout(1800); break; } }
    console.log(`\nafter "${btn}": turn ${now}`);
    if (await veil()) {
      const c = await page.evaluate(() => (document.querySelector('#card').innerText || '').replace(/\s+/g, ' ').trim());
      console.log(`  the month card: ${c.slice(0, 1500)}`);
      await page.screenshot({ path: `shots/${prefix}-t${now}-card.png` });
    } else console.log('  no card shown');
    await closeAll();
    await look(`t${now}`);
  }
  console.log(`\npage errors: ${errors.length}${errors.length ? ' ' + JSON.stringify(errors.slice(0, 3)) : ''}`);
  await browser.close();
})();
