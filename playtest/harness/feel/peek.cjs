// names3: the pieces a seat starts with, entered through the menu at a chosen depth. usage: node names3.cjs "United States" "Supreme command"
const { chromium } = require('playwright');
const PAGE = process.env.TC_PAGE || 'file:///home/user/tc842/dist/play.html';
const [nat = 'United States', depth = '', year = ''] = process.argv.slice(2);
(async () => {
  const browser = await chromium.launch(); const page = await (await browser.newContext({ viewport: { width: 1440, height: 900 } })).newPage();
  await page.goto(PAGE); await page.evaluate(() => localStorage.clear()); await page.reload(); await page.waitForSelector('#card');
  const press = (words, root) => page.evaluate(({ words, root }) => { const r = document.querySelector(root); const b = [...r.querySelectorAll('button')].find((x) => (x.innerText || '').replace(/\s+/g, ' ').trim().toLowerCase().startsWith(words.toLowerCase())); if (b) b.click(); return !!b; }, { words, root });
  await press(nat, '#card'); await page.waitForTimeout(300); if (year) { await press(year, '#card'); await page.waitForTimeout(300); } if (depth) { await press(depth, '#card'); await page.waitForTimeout(300); } await press('BEGIN THE CAMPAIGN', '#card'); await page.waitForTimeout(2500);
  const expr = process.argv[5] || '1';
  const out = await page.evaluate((e) => { const g = window.theater.game(), me = g.human, R = g.regions, ui = window.theater.ui; return eval(e); }, expr);
  console.log(JSON.stringify(out).slice(0, +(process.env.MAXC || 1500)));
  await browser.close();
})();
