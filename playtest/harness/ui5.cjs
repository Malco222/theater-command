// What does the player see when the country falls at the opening of its own month?
const { chromium } = require('playwright');
const fs = require('fs');
const dir = __dirname;
(async () => {
  const browser = await chromium.launch();
  const page = await browser.newPage({ viewport: { width: 1440, height: 900 } });
  const errs = [];
  page.on('pageerror', (e) => errs.push(e.message));
  const save = fs.readFileSync(dir + '/out/ita-fall.json', 'utf8');
  await page.addInitScript((s) => { try { localStorage.setItem('tc-watch', '0'); localStorage.setItem('theater-command-save-v1', s); } catch {} }, save);
  await page.goto('file:///home/user/theater-command/dist/play.html');
  await page.waitForTimeout(800);
  const ok = await page.evaluate(() => window.theater.continueGame());
  await page.waitForTimeout(400);
  const closeAll = async () => { for (let i = 0; i < 12; i++) { const open = await page.evaluate(() => document.querySelector('#veil')?.classList.contains('open')); if (!open) break; await page.keyboard.press('Escape'); await page.waitForTimeout(120); } };
  await closeAll();
  const st = () => page.evaluate(() => { const g = window.theater.game(); return { turn: g.turn, human: g.human, alive: g.human ? g.nations[g.human].alive : null, bar: document.querySelector('header, #top, .top')?.innerText?.replace(/\s+/g, ' ').slice(0, 200), veil: document.querySelector('#veil')?.classList.contains('open'), card: document.querySelector('#card')?.innerText?.replace(/\s+/g, ' ').slice(0, 400) }; });
  console.log('continue', ok, JSON.stringify(await st()));
  await page.screenshot({ path: dir + '/shots/20-ita-before.png' });
  // End the month.
  await page.evaluate(() => document.querySelector('[data-act="end"]').dispatchEvent(new MouseEvent('click', { bubbles: true })));
  await page.waitForTimeout(2500);
  console.log('after end 1', JSON.stringify(await st()));
  await page.screenshot({ path: dir + '/shots/21-ita-after-end.png' });
  await closeAll();
  console.log('closed', JSON.stringify(await st()));
  await page.screenshot({ path: dir + '/shots/22-ita-my-month.png' });
  const endBtn = await page.evaluate(() => { const b = document.querySelector('[data-act="next"]'); return b ? { text: b.innerText, vis: b.offsetParent !== null } : null; });
  console.log('end button', JSON.stringify(endBtn));
  if (endBtn) { await page.evaluate(() => document.querySelector('[data-act="next"]').dispatchEvent(new MouseEvent('click', { bubbles: true }))); await page.waitForTimeout(2500); console.log('after end 2', JSON.stringify(await st())); await page.screenshot({ path: dir + '/shots/23-ita-after-end2.png' }); }
  console.log('errors', errs);
  await browser.close();
})();
