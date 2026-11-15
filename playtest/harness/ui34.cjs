// Version 70: the News tab in the month a neighbour marches on the player. Loads a save, presses End month once, opens
// News, and reports where the lines about claims and declarations of war stand in the list and how they are marked.
// usage: [TC_PAGE=...] node ui34.cjs SAVE.json PREFIX [W H]
const { chromium } = require('playwright');
const fs = require('fs');
const [save, prefix, W = '1440', H = '900'] = process.argv.slice(2);
(async () => {
  const browser = await chromium.launch();
  const phone = Math.min(+W, +H) < 500;
  const ctx = await browser.newContext({ viewport: { width: +W, height: +H }, deviceScaleFactor: 2, isMobile: phone, hasTouch: phone });
  const page = await ctx.newPage();
  const errors = [];
  page.on('pageerror', (e) => errors.push(e.message));
  await page.addInitScript((s) => { try { if (!sessionStorage.getItem('tc-once')) { sessionStorage.setItem('tc-once', '1'); localStorage.clear(); localStorage.setItem('tc-watch', '0'); localStorage.setItem('theater-command-save-v1', s); } } catch {} }, fs.readFileSync(save, 'utf8'));
  await page.goto(process.env.TC_PAGE || 'file:///home/user/tc70/dist/play.html'); await page.waitForTimeout(1000);
  const c = page.locator('[data-act="continue"]').first(); if (await c.count()) await c.click(); else await page.evaluate(() => window.theater.continueGame());
  await page.waitForTimeout(1500);
  const closeAll = async () => { for (let k = 0; k < 12; k++) { const was = await page.evaluate(() => { const v = document.querySelector('#veil'); if (!v || !v.classList.contains('open')) return false; window.theater.openModal(null); return true; }); if (!was) break; await page.waitForTimeout(200); } };
  await closeAll();
  const t = await page.evaluate(() => window.theater.game().turn);
  await page.evaluate(() => window.theater.nextMonth());
  await page.waitForFunction((t0) => window.theater.game().turn > t0, t, { timeout: 240000 });
  await page.waitForTimeout(1500); await closeAll();
  await page.screenshot({ path: `shots/${prefix}-1-month-opens.png` });
  const chips = await page.evaluate(() => [...document.querySelectorAll('.nudge, .todo, .hint, .chipnote, [data-act="nudge"]')].map((x) => x.textContent.replace(/\s+/g, ' ').trim()).slice(0, 12));
  console.log('notices as the month opens:', JSON.stringify(chips));
  await page.evaluate(() => document.querySelector('[data-act="tab"][data-tab="reel"]')?.click()); await page.waitForTimeout(700);
  const rows = await page.evaluate(() => {
    const h = [...document.querySelectorAll('h2, h3')].find((x) => /Newsreel/i.test(x.textContent));
    const box = h?.parentElement; if (!box) return { found: false };
    const lines = box.innerText.split('\n').map((x) => x.trim()).filter((x) => x && !/^WEEK \d$/.test(x));
    const hit = lines.map((x, i) => (/presses its claim|marches for|declares war on|warned that it may march/i.test(x) ? i : -1)).filter((i) => i >= 0);
    const seen = [...box.querySelectorAll('*')].filter((x) => x.children.length === 0 && /declares war on|marches for|presses its claim/i.test(x.textContent)).map((x) => { const r = x.getBoundingClientRect(); return { top: Math.round(r.top), onScreen: r.top < window.innerHeight && r.bottom > 0, bold: Number(getComputedStyle(x).fontWeight) >= 600, text: x.textContent.trim().slice(0, 90) }; });
    return { found: true, lines: lines.length, at: hit.map((i) => `line ${i + 1} of ${lines.length}: ${lines[i].slice(0, 120)}`), seen };
  });
  console.log('Newsreel:', JSON.stringify(rows));
  await page.screenshot({ path: `shots/${prefix}-2-news.png` });
  console.log('page errors:', errors.length ? JSON.stringify(errors.slice(0, 4)) : 'none');
  await browser.close();
})();
