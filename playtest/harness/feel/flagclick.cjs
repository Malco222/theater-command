// flagclick: does a click on each of my armies' flags pick that army up? (Version 84.2.4: a flag takes the click before
// an airfield pad or a sea lane under it.) Starts a new game as NATION, and for every flag of mine on the map centres
// the map on it at each WIDTH, clicks the middle of the flag's cloth with the real mouse, and reads what the page did:
// the province selected, whether an army is in hand, and which side page is up.
//   TC_PAGE=file:///.../dist/play.html node flagclick.cjs "United States" [YEAR] [WIDTHS=600,1400]
const { chromium } = require('playwright');
const PAGE = process.env.TC_PAGE; const [nat = 'United States', year = '', widths = '600,1400'] = process.argv.slice(2);
(async () => {
  const browser = await chromium.launch(); const page = await (await browser.newContext({ viewport: { width: 1600, height: 1000 } })).newPage();
  await page.goto(PAGE); await page.evaluate(() => localStorage.clear()); await page.reload(); await page.waitForSelector('#card');
  const press = (words) => page.evaluate((words) => { const r = document.querySelector('#card'); const b = [...r.querySelectorAll('button')].find((x) => (x.innerText || '').replace(/\s+/g, ' ').trim().toLowerCase().startsWith(words.toLowerCase())); if (b) b.click(); return !!b; }, words);
  await press(nat); await page.waitForTimeout(300); if (year) { await press(year); await page.waitForTimeout(300); } await press('BEGIN THE CAMPAIGN'); await page.waitForTimeout(2500);
  await page.evaluate(() => window.theater.openModal(null)); await page.waitForTimeout(300);
  const flags = await page.evaluate(() => { const g = window.theater.game(); return [...document.querySelectorAll('#units g.ctr.army')].filter((e) => e.dataset.nat === g.human).map((e) => { const r = g.regions.find((x) => x.id === +e.dataset.reg); return { reg: +e.dataset.reg, name: r.name, x: r.x, y: r.y }; }); });
  let bad = 0, n = 0;
  for (const w of widths.split(',').map(Number)) {
    for (const f of flags) {
      await page.keyboard.press('Escape'); await page.waitForTimeout(60);
      const at = await page.evaluate(([f, w]) => {
        const t = window.theater, el = document.querySelector('#map'); const tall = el.clientHeight / el.clientWidth;
        t.ui.view.w = w; t.ui.view.x = f.x - w / 2; t.ui.view.y = f.y - (w / 2) * tall; t.applyView(); t.refresh();
        const e = document.querySelector(`#units g.ctr.army[data-reg="${f.reg}"]`); if (!e) return null; const u = e.querySelector('use') || e; const b = u.getBoundingClientRect();
        return { x: b.x + b.width / 2, y: b.y + b.height / 2 };
      }, [f, w]);
      if (!at) { console.log(`width ${w}  ${f.name}: no flag found`); continue; }
      await page.waitForTimeout(120); await page.mouse.click(at.x, at.y); await page.waitForTimeout(200);
      const r = await page.evaluate(() => { const t = window.theater, g = t.game(), u = t.ui; const s = g.regions.find((x) => x.id === u.sel); return { sel: s ? s.name : String(u.sel), inHand: u.picked ? (u.picked.size !== undefined ? u.picked.size : u.picked.length !== undefined ? u.picked.length : Object.keys(u.picked).length) : 0, tab: u.tab, card: !!document.querySelector('#veil.open') }; });
      n++; const ok = r.sel === f.name && r.inHand > 0 && !r.card;
      if (!ok) { bad++; console.log(`width ${w}  ${f.name}: NOT PICKED UP. selected ${r.sel}, in hand ${r.inHand}, side page ${r.tab}${r.card ? ', a card opened' : ''}`); await page.evaluate(() => window.theater.openModal(null)); }
    }
  }
  console.log(`${nat}: ${flags.length} flags, ${n} clicks, ${n - bad} picked the army up, ${bad} did not`);
  await browser.close();
})();
