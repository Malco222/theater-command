// Loads a saved game and prints what an expression gives (g = the game, R = regions, me = the player's nation, ui, T = theater).
const { chromium } = require('playwright'); const fs = require('fs');
const PAGE = process.env.TC_PAGE; const [save, expr = '1'] = process.argv.slice(2);
(async () => {
  const browser = await chromium.launch(); const page = await (await browser.newContext({ viewport: { width: 1600, height: 1000 } })).newPage();
  await page.goto(PAGE); await page.evaluate((t) => { localStorage.clear(); localStorage.setItem('theater-command-save-v1', t); }, fs.readFileSync(save, 'utf8')); await page.reload(); await page.waitForSelector('#card');
  await page.evaluate(() => { try { window.theater.continueGame(); } catch (e) {} }); await page.waitForTimeout(2500); await page.evaluate(() => window.theater.openModal(null)); await page.waitForTimeout(300);
  const out = await page.evaluate(async (e) => { const T = window.theater, g = T.game(), me = g.human, R = g.regions, ui = T.ui; const tab = async (k) => { const t = [...document.querySelectorAll('#tabs [data-tab]')].find((t) => t.dataset.tab === k); if (t) t.click(); await new Promise((r) => setTimeout(r, 500)); return document.querySelector('#pane').innerText.replace(/\s+/g, ' '); }; return await eval(e); }, expr);
  console.log((typeof out === 'string' ? out : JSON.stringify(out)).slice(0, +(process.env.MAXC || 1500)));
  await browser.close();
})();
