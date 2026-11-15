// Version 68: what the player sees when the rules crash in the computer's month. Loads a save written after the
// player's orders (probe55), presses End month, and reports the page's errors, the date on the top bar before and
// after, and whether the game can go on.
// usage: [TC_PAGE=...] node ui32.cjs SAVE.json SHOT-PREFIX
const { chromium } = require('playwright');
const fs = require('fs');
const [file, shot] = process.argv.slice(2);
const flat = (s, n = 400) => String(s).replace(/\n+/g, ' / ').replace(/\s+/g, ' ').slice(0, n);
(async () => {
  const browser = await chromium.launch();
  const errors = [];
  const page = await browser.newPage({ viewport: { width: 1440, height: 900 }, deviceScaleFactor: 2 });
  page.on('pageerror', (e) => errors.push(`${e.message} @ ${(e.stack || '').split('\n').slice(1, 3).join(' | ').slice(0, 200)}`));
  page.on('console', (m) => { if (m.type() === 'error') errors.push(`console: ${m.text().slice(0, 300)}`); });
  await page.addInitScript((s) => { try { localStorage.setItem('tc-watch', '0'); localStorage.setItem('theater-command-save-v1', s); } catch {} }, fs.readFileSync(file, 'utf8'));
  await page.goto(process.env.TC_PAGE || 'file:///home/user/tc68/dist/play.html'); await page.waitForTimeout(1000);
  const cont = page.locator('[data-act="continue"]').first();
  if (await cont.count()) await cont.click(); else await page.evaluate(() => window.theater.continueGame());
  await page.waitForTimeout(1500);
  const closeCards = async () => { for (let k = 0; k < 8; k++) { const open = await page.evaluate(() => { const v = document.querySelector('#veil'); if (!v || !v.classList.contains('open')) return false; window.theater.openModal(null); return true; }); if (!open) break; await page.waitForTimeout(200); } };
  await closeCards();
  const bar = async () => flat(await page.locator('.bar').first().innerText().catch(() => ''), 160);
  const end = async () => page.evaluate(() => { const b = document.querySelector('[data-act="end"]'); return b ? { text: b.textContent.trim(), disabled: !!b.disabled, shown: b.offsetParent !== null } : null; });
  console.log('before: top bar:', await bar()); console.log('before: End month button:', JSON.stringify(await end()));
  await page.screenshot({ path: `shots/${shot}-1-before.png` });
  for (let press = 1; press <= 3; press++) {
    const n0 = errors.length;
    await page.evaluate(() => { const b = document.querySelector('[data-act="end"]'); if (b) b.dispatchEvent(new MouseEvent('click', { bubbles: true })); });
    await page.waitForTimeout(6000);
    const card = await page.evaluate(() => { const v = document.querySelector('#veil'); return v && v.classList.contains('open') ? (document.querySelector('#card')?.innerText || '').replace(/\s+/g, ' ').slice(0, 300) : null; });
    console.log(`press ${press}: new page errors ${errors.length - n0}; top bar: ${await bar()}; End month button: ${JSON.stringify(await end())}; card open: ${card ? JSON.stringify(card) : 'none'}`);
    await page.screenshot({ path: `shots/${shot}-${press + 1}-after-press-${press}.png` });
    await closeCards();
  }
  const st = await page.evaluate(() => { try { const s = JSON.parse(localStorage.getItem('theater-command-save-v1') || 'null'); return s ? { turn: s.turn ?? s.g?.turn, keys: Object.keys(s).slice(0, 8) } : null; } catch (e) { return String(e); } });
  console.log('the save in the browser after:', JSON.stringify(st));
  console.log('page errors:', errors.length ? JSON.stringify([...new Set(errors)].slice(0, 5)) : 'none');
  await browser.close();
})();
