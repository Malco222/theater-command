// Version 84: open a save, end one month by the page's own button, press READ THE BATTLE REPORT on the month card and
// print the report (the ticket's rows and the who-paid line). usage: [TC_PAGE=...] node ui63.cjs SAVE.json PREFIX [WORDS-REGEX]
const fs = require('fs');
const { chromium } = require('playwright');
const PAGE = process.env.TC_PAGE || 'file:///home/user/tc84/dist/play.html';
const [save, prefix, rx] = process.argv.slice(2);
(async () => {
  const browser = await chromium.launch(); const errors = [];
  const page = await (await browser.newContext({ viewport: { width: 1440, height: 900 }, deviceScaleFactor: 2 })).newPage();
  page.on('pageerror', (e) => errors.push(e.message.slice(0, 300)));
  await page.goto(PAGE);
  await page.evaluate((text) => { localStorage.clear(); localStorage.setItem('tc-watch', '0'); localStorage.setItem('tc-ask-end', '0'); localStorage.setItem('theater-command-save-v1', text); }, fs.readFileSync(save, 'utf8'));
  await page.reload(); await page.waitForSelector('#card'); await page.evaluate(() => { try { window.theater.continueGame(); } catch (e) {} }); await page.waitForTimeout(2500);
  for (let i = 0; i < 4; i++) await page.evaluate(() => { const v = document.querySelector('#veil'); if (v && v.classList.contains('open')) window.theater.openModal(null); });
  const before = await page.evaluate(() => window.theater.game().turn);
  await page.evaluate(() => document.querySelector('[data-act="end"]').click()); await page.waitForTimeout(600);
  await page.evaluate(() => { const b = [...document.querySelectorAll('#card button')].find((x) => /end the month/i.test(x.innerText || '')); if (b) b.click(); });
  const t0 = Date.now(); while (Date.now() - t0 < 120000) { await page.waitForTimeout(700); if (await page.evaluate(() => window.theater.game().turn) !== before) { await page.waitForTimeout(1800); break; } }
  const pressed = await page.evaluate(() => { const b = [...document.querySelectorAll('#card button, #card a, #card [data-act]')].find((x) => /read the battle report/i.test(x.innerText || '')); if (!b) return null; b.click(); return (b.innerText || '').trim(); });
  await page.waitForTimeout(900);
  const rep = await page.evaluate(() => { const c = document.querySelector('#card'); const t = (e) => (e ? e.innerText.replace(/\s+/g, ' ').trim() : ''); return { signs: [...c.querySelectorAll('.tc-signline')].map(t), rows: [...c.querySelectorAll('tr, .tline, .trow, li')].map(t).filter((s) => s && s.length < 260), all: t(c) }; });
  console.log(`pressed: ${pressed}`);
  console.log(`who-paid and sign lines: ${JSON.stringify(rep.signs)}`);
  const want = new RegExp(rx || 'desert|anti-tank|dug|driven|open|guns|tanks|works|pays', 'i');
  console.log('rows that match: ' + rep.rows.filter((s) => want.test(s)).slice(0, 14).join(' // '));
  console.log('the report (first 700): ' + rep.all.slice(0, 700));
  await page.screenshot({ path: `shots/${prefix}-report.png` });
  await page.evaluate(() => { const c = document.querySelector('#card'); const s = [...c.querySelectorAll('*')].find((e) => /Open desert/.test(e.innerText || '') && e.children.length < 6); if (s) s.scrollIntoView({ block: 'center' }); }); await page.waitForTimeout(300);
  await page.screenshot({ path: `shots/${prefix}-report2.png` });
  console.log(`page errors: ${errors.length}`);
  await browser.close();
})();
