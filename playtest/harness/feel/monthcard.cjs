// monthcard: loads a save, ends the month by Enter, and prints every card that opens on the way and the to-do tray after.
const { chromium } = require('playwright'); const fs = require('fs');
const PAGE = process.env.TC_PAGE; const [save, out = ''] = process.argv.slice(2);
(async () => {
  const browser = await chromium.launch(); const page = await (await browser.newContext({ viewport: { width: 1600, height: 1000 } })).newPage();
  await page.goto(PAGE); await page.evaluate((t) => { localStorage.clear(); localStorage.setItem('theater-command-save-v1', t); localStorage.setItem('tc-watch', '0'); }, fs.readFileSync(save, 'utf8')); await page.reload(); await page.waitForSelector('#card');
  await page.evaluate(() => { try { window.theater.continueGame(); } catch (e) {} }); await page.waitForTimeout(2500); await page.evaluate(() => window.theater.openModal(null)); await page.waitForTimeout(300);
  const card = () => page.evaluate(() => { const v = document.querySelector('#veil'), c = document.querySelector('#card'); return v && v.classList.contains('open') && c ? c.innerText.replace(/\s+/g, ' ').trim() : ''; });
  const turn0 = await page.evaluate(() => window.theater.game().turn); const seen = []; let last = '';
  for (let i = 0; i < 40; i++) {
    const c = await card(); if (c && c !== last) { seen.push(c); last = c; }
    const t = await page.evaluate(() => window.theater.game().turn);
    if (t > turn0 && !c) break;
    await page.keyboard.press('Enter'); await page.waitForTimeout(700);
  }
  await page.waitForTimeout(800);
  const tray = await page.evaluate(() => [...document.querySelectorAll('#mapwrap *')].filter((e) => /todo|tray|chip|nudge/i.test(e.className && e.className.baseVal === undefined ? e.className : '') && e.children.length === 0 && e.innerText && e.innerText.trim()).map((e) => `[${e.className}] ${e.innerText.replace(/\s+/g, ' ').trim()}`).slice(0, 14));
  console.log(seen.map((c, i) => `CARD ${i + 1} (${c.split(' ').length} words): ${c}`).join('\n')); console.log('TRAY: ' + tray.join(' || '));
  if (out) await page.screenshot({ path: out });
  await browser.close();
})();
