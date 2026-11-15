// Version 76: a picture of South America as a player first sees it, zoomed out. usage: node ui47.cjs NAT PREFIX [CLICKS]
const { chromium } = require('playwright');
const [nat = 'BRA', prefix = '99-v76-map', clicksS = '3'] = process.argv.slice(2);
const PAGE = process.env.TC_PAGE || 'file:///home/user/tc76/dist/play.html';
(async () => {
  const browser = await chromium.launch(); const errors = [];
  const page = await (await browser.newContext({ viewport: { width: 1440, height: 900 }, deviceScaleFactor: 2 })).newPage();
  page.on('pageerror', (e) => errors.push(e.message));
  await page.goto(PAGE); await page.evaluate(() => { localStorage.clear(); localStorage.setItem('tc-watch', '0'); }); await page.reload(); await page.waitForSelector('#card');
  await page.click(`.gcnat[data-nat="${nat}"]`); await page.waitForTimeout(200); await page.click('.gcgo');
  await page.waitForFunction((nat) => { const g = window.theater.game(); return g && g.human === nat && !document.querySelector('#veil.open .busy'); }, nat, { timeout: 300000 });
  await page.waitForTimeout(1200);
  for (let i = 0; i < 12; i++) { if (!(await page.evaluate(() => document.querySelector('#veil')?.classList.contains('open')))) break; await page.evaluate(() => window.theater.openModal(null)); await page.waitForTimeout(150); }
  console.log('zoom buttons:', await page.evaluate(() => [...document.querySelectorAll('button')].filter((b) => /zoom/i.test(b.dataset.act || '') || /^[+−-]$/.test(b.textContent.trim())).map((b) => `${b.textContent.trim()} [${b.dataset.act}]`)));
  for (let i = 0; i < +clicksS; i++) { await page.evaluate(() => { const b = [...document.querySelectorAll('button')].find((x) => /zoomout|zoom-out|out/i.test(x.dataset.act || '') || x.textContent.trim() === '−'); if (b) b.click(); }); await page.waitForTimeout(350); }
  await page.waitForTimeout(600);
  await page.screenshot({ path: `shots/${prefix}-${nat}.png` });
  console.log('errors:', errors.length ? errors.slice(0, 3) : 'none');
  await browser.close();
})();
