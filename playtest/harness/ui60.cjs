// Version 83.1: a turning point's first card by hand (ruling 123). usage: [TC_PAGE=...] node ui60.cjs PREFIX NAT FROM [MONTHS=1]
// Opens the page with nothing saved, goes to "Play a turning point of the war", starts NAT from turn FROM, reads the
// first card (its title, its bands, its lead), then ends MONTHS months and reads each card. TC_DUMP=file writes the
// page's own save of the new campaign to a file (for ui57.cjs). MONTHS=0 ends no month.
const { chromium } = require('playwright');
const PAGE = process.env.TC_PAGE || 'file:///home/user/tc831/dist/play.html';
const [prefix, nat, from, monthsS] = process.argv.slice(2);
(async () => {
  const browser = await chromium.launch(); const errors = [];
  const page = await (await browser.newContext({ viewport: { width: 1440, height: 900 }, deviceScaleFactor: 2 })).newPage();
  page.on('pageerror', (e) => errors.push(e.message.slice(0, 300)));
  await page.goto(PAGE); await page.evaluate(() => { localStorage.clear(); localStorage.setItem('tc-watch', '0'); }); await page.reload(); await page.waitForSelector('#card'); await page.waitForTimeout(800);
  await page.evaluate(() => document.querySelector('[data-act="menu"][data-menu="scenarios"]')?.click()); await page.waitForTimeout(600);
  const offers = await page.evaluate(() => [...document.querySelectorAll('[data-act="start"]')].map((b) => `${b.dataset.nat || 'watch'}@${b.dataset.from}: ${(b.innerText || '').replace(/\s+/g, ' ').trim().slice(0, 50)}`));
  console.log('turning points offered: ' + offers.length + '; for ' + nat + ': ' + JSON.stringify(offers.filter((o) => o.startsWith(nat + '@'))));
  if (process.env.TC_OFFERS) console.log(offers.join('\n'));
  const ok = await page.evaluate(([nat, from]) => { const b = document.querySelector(`[data-act="start"][data-nat="${nat}"][data-from="${from}"]`); if (b) b.click(); return !!b; }, [nat, from]);
  if (!ok) { console.log('no such start'); await browser.close(); return; }
  await page.waitForTimeout(3500);
  const card = () => page.evaluate(() => { const c = document.querySelector('#card'); const t = (e) => (e ? (e.innerText || '').replace(/\s+/g, ' ').trim() : ''); const g = window.theater.game(); const red = [...c.querySelectorAll('*')].filter((e) => /band/.test(e.className || '') && e.children.length < 12).slice(0, 4).map((e) => `${e.className}: ${t(e).slice(0, 400)}`); return { me: g?.human, turn: g?.turn, open: document.querySelector('#veil')?.classList.contains('open'), title: t(c.querySelector('h2')), bands: red, text: t(c).slice(0, 1200) }; });
  if (process.env.TC_DUMP) { const text = await page.evaluate(() => localStorage.getItem('theater-command-save-v1')); if (text) { require('fs').writeFileSync(process.env.TC_DUMP, text); console.log(`the page's own save written to ${process.env.TC_DUMP}: ${text.length} bytes`); } else console.log('the page has written no save yet'); }
  console.log('the first card: ' + JSON.stringify(await card())); await page.screenshot({ path: `shots/${prefix}-first-card.png` });
  for (let m = 1; m <= Number(monthsS ?? 1); m++) {
    const t0 = await page.evaluate(() => window.theater.game().turn);
    await page.evaluate(() => window.theater.openModal(null)); await page.waitForTimeout(400);
    await page.evaluate(() => document.querySelector('[data-act="end"]')?.click()); await page.waitForTimeout(600);
    for (let k = 0; k < 6; k++) { const sure = await page.evaluate(() => { if (!document.querySelector('#veil')?.classList.contains('open')) return null; const b = [...document.querySelectorAll('#card footer button')].find((x) => /end the month|end month|yes|go on|anyway/i.test(x.innerText)); if (b) { b.click(); return b.innerText; } return null; }); if (!sure) break; await page.waitForTimeout(500); }
    for (let k = 0; k < 120; k++) { const t = await page.evaluate(() => window.theater.game()?.turn); if (t > t0) break; await page.waitForTimeout(1000); }
    await page.waitForTimeout(2500);
    console.log(`card ${m} month(s) on: ` + JSON.stringify(await card())); await page.screenshot({ path: `shots/${prefix}-card-${m}.png` });
  }
  console.log('page errors: ' + JSON.stringify(errors));
  await browser.close();
})();
