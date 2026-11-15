// Version 71 on the page: the last act and the two tests for winning the world. Loads a save from the month before the
// war is decided, ends the month, reads the card that offers the victory or World Domination, orders World Domination,
// and reads the standings on the Powers tab (the two tests), then plays two more months.
// usage: [TC_PAGE=file:///home/user/tc71/dist/play.html] node ui36.cjs SAVE.json PREFIX [W H]
const { chromium } = require('playwright');
const fs = require('fs');
const [save, prefix, W = '1440', H = '900'] = process.argv.slice(2);
const flat = (s, n = 700) => String(s ?? '').replace(/\s+/g, ' ').trim().slice(0, n);
(async () => {
  const browser = await chromium.launch();
  const phone = Math.min(+W, +H) < 500;
  const page = await (await browser.newContext({ viewport: { width: +W, height: +H }, deviceScaleFactor: 2, isMobile: phone, hasTouch: phone })).newPage();
  const errors = [];
  page.on('pageerror', (e) => errors.push(`${e.message} @ ${(e.stack || '').split('\n').slice(1, 3).join(' | ').slice(0, 200)}`));
  await page.addInitScript((s) => { try { if (!sessionStorage.getItem('tc-once')) { sessionStorage.setItem('tc-once', '1'); localStorage.clear(); localStorage.setItem('tc-watch', '0'); localStorage.setItem('theater-command-save-v1', s); } } catch {} }, fs.readFileSync(save, 'utf8'));
  await page.goto(process.env.TC_PAGE || 'file:///home/user/tc71/dist/play.html'); await page.waitForTimeout(1000);
  const c = page.locator('[data-act="continue"]').first(); if (await c.count()) await c.click(); else await page.evaluate(() => window.theater.continueGame());
  await page.waitForTimeout(1500);
  const card = (n = 900) => page.evaluate((n) => { const v = document.querySelector('#veil'); return v && v.classList.contains('open') ? (document.querySelector('#card')?.innerText || '').replace(/\s+/g, ' ').slice(0, n) : null; }, n);
  const closeAll = async (keep) => { const seen = []; for (let k = 0; k < 12; k++) { const t = await card(300); if (!t) break; seen.push(t); if (keep && keep.test(t)) return seen; await page.evaluate(() => window.theater.openModal(null)); await page.waitForTimeout(250); } return seen; };
  await closeAll();
  const turn = () => page.evaluate(() => window.theater.game().turn);
  const month = async (keep) => { const t = await turn(); await page.evaluate(() => window.theater.nextMonth()); try { await page.waitForFunction((t0) => window.theater.game().turn > t0, t, { timeout: 240000 }); } catch { console.log('  the date did not move'); } await page.waitForTimeout(1500); return closeAll(keep); };
  let found = false;
  for (let i = 0; i < 4 && !found; i++) {
    const cards = await month(/World Domination/);
    const t = await card(1400);
    console.log(`turn ${await turn()}: cards ${cards.length}; last-act card: ${t && /World Domination/.test(t) ? 'OPEN' : 'no'}`);
    if (t && /World Domination/.test(t)) { found = true; console.log('  the card says:', flat(t, 1400)); await page.screenshot({ path: `shots/${prefix}-1-the-offer.png` }); }
  }
  if (!found) {
    console.log('no card came by itself; notices:', JSON.stringify(await page.evaluate(() => { const g = window.theater.game(); return { flags: Object.fromEntries(Object.entries(g.flags).filter(([k]) => k.startsWith('dom'))) }; })));
    await page.evaluate(() => document.querySelector('[data-act="lastact"]')?.click()); await page.waitForTimeout(500);
    console.log('  opened by hand:', flat(await card(1200), 1200));
  }
  const buttons = await page.evaluate(() => [...document.querySelectorAll('#card [data-act]')].map((b) => `${b.dataset.act}${b.dataset.ask ? ':' + b.dataset.ask : ''}${b.disabled ? '!' : ''} "${b.textContent.trim().slice(0, 40)}"`));
  console.log('buttons on the card:', JSON.stringify(buttons));
  await page.evaluate(() => { const b = document.querySelector('#card [data-act="lastact"][data-ask="1"]'); if (b) b.click(); }); await page.waitForTimeout(500);
  const ask = await card(900);
  if (ask) { console.log('asked to confirm:', flat(ask, 900)); await page.screenshot({ path: `shots/${prefix}-2-confirm.png` }); }
  await page.evaluate(() => document.querySelector('#card [data-act="domination"]')?.click()); await page.waitForTimeout(1200);
  console.log('ordered:', JSON.stringify(await page.evaluate(() => { const g = window.theater.game(); return { dom: g.flags.dom ?? null, tab: window.theater.ui.tab, alive: Object.values(g.nations).filter((n) => n.alive).length }; })));
  await closeAll();
  await page.evaluate(() => document.querySelector('[data-act="tab"][data-tab="powers"]')?.click()); await page.waitForTimeout(600);
  const tests = () => page.evaluate(() => ({ tests: [...document.querySelectorAll('.wtest')].map((x) => `${x.classList.contains('met') ? '[met] ' : '[not met] '}${x.textContent.replace(/\s+/g, ' ').trim()}`), sub: (document.querySelector('.wtests')?.closest('div')?.parentElement?.querySelector('.sub')?.textContent || '').replace(/\s+/g, ' ').trim().slice(0, 400), facts: [...document.querySelectorAll('.fact')].map((x) => x.textContent.replace(/\s+/g, ' ').trim()) }));
  console.log('the standings:', JSON.stringify(await tests()));
  await page.evaluate(() => document.querySelector('.wtests')?.scrollIntoView({ block: 'center' })); await page.waitForTimeout(300);
  await page.screenshot({ path: `shots/${prefix}-3-standings.png` });
  for (let i = 0; i < 2; i++) { const cards = await month(); console.log(`turn ${await turn()}: cards ${cards.length} ${JSON.stringify(cards.map((x) => x.slice(0, 90)))}`); }
  await page.evaluate(() => document.querySelector('[data-act="tab"][data-tab="powers"]')?.click()); await page.waitForTimeout(600);
  console.log('two months on:', JSON.stringify(await tests()));
  console.log('page errors:', errors.length ? JSON.stringify([...new Set(errors)].slice(0, 5)) : 'none');
  await browser.close();
})();
