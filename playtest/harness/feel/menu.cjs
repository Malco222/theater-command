const { chromium } = require('playwright');
const PAGE = process.env.TC_PAGE || 'file:///home/user/tc842/dist/play.html';
(async () => {
  const browser = await chromium.launch(); const page = await (await browser.newContext({ viewport: { width: 1440, height: 900 } })).newPage();
  await page.goto(PAGE); await page.evaluate(() => localStorage.clear()); await page.reload(); await page.waitForSelector('#card');
  const out = await page.evaluate(() => { const card = document.querySelector('#card'); const rows = []; const walk = (el, d) => { for (const c of el.children) { if (c.tagName === 'BUTTON') rows.push(`${'  '.repeat(d)}[${c.classList.contains('on') || c.classList.contains('sel') ? 'ON ' : ''}${(c.innerText || '').replace(/\s+/g, ' ').trim().slice(0, 70)}]`); else if (c.querySelector('button')) walk(c, d + 1); else { const t = (c.innerText || '').replace(/\s+/g, ' ').trim(); if (t) rows.push(`${'  '.repeat(d)}${t.slice(0, 110)}${t.length > 110 ? `… (${t.split(' ').length} words)` : ''}`); } } }; walk(card, 0); return rows; });
  console.log(out.join('\n'));
  await page.screenshot({ path: process.argv[2] || 'feel/menu.png', fullPage: false });
  await browser.close();
})();
