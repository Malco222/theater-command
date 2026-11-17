// mksave: loads a save on the built page, runs one expression on the game (g, R, me, T), and writes the game's own save text.
const { chromium } = require('playwright'); const fs = require('fs');
const PAGE = process.env.TC_PAGE; const [save, expr, out] = process.argv.slice(2);
(async () => {
  const browser = await chromium.launch(); const page = await (await browser.newContext({ viewport: { width: 1600, height: 1000 } })).newPage();
  await page.goto(PAGE); await page.evaluate((t) => { localStorage.clear(); localStorage.setItem('theater-command-save-v1', t); }, fs.readFileSync(save, 'utf8')); await page.reload(); await page.waitForSelector('#card');
  await page.evaluate(() => { try { window.theater.continueGame(); } catch (e) {} }); await page.waitForTimeout(2500); await page.evaluate(() => window.theater.openModal(null)); await page.waitForTimeout(300);
  const text = await page.evaluate((e) => { const T = window.theater, g = T.game(), me = g.human, R = g.regions; eval(e); T.refresh(); return T.save(); }, expr);
  fs.writeFileSync(out, text); console.log(`${out}: ${text.length} characters`); await browser.close();
})();
