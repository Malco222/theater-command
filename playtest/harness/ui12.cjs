// Version 56: click a wreck button and the scorched earth controls, and see what the player is told.
const { chromium } = require('playwright');
const out = process.argv[2] ?? 'shots';
(async () => {
  const browser = await chromium.launch();
  const page = await browser.newPage({ viewport: { width: 1440, height: 900 }, deviceScaleFactor: 1 });
  const errors = []; let dialogs = 0;
  page.on('pageerror', (e) => errors.push(e.message));
  page.on('dialog', async (d) => { dialogs++; console.log('DIALOG:', d.message().slice(0, 200)); await d.dismiss(); });
  await page.addInitScript(() => { try { localStorage.setItem('tc-watch', '0'); } catch {} });
  await page.goto(process.env.TC_PAGE || 'file:///home/user/theater-command/dist/play.html');
  await page.waitForTimeout(800);
  await page.locator('[data-act="start"]').click();
  await page.waitForTimeout(2500);
  const G = () => page.evaluate(() => { const g = window.theater.game(); const n = g.nations.GER; return { turn: g.turn, fac: g.regions.find((r) => r.name === 'Hamburg').fac, flags: Object.keys(g.flags).filter((k) => /^(pz|prize|out|burn|scorch|wreck)/.test(k)).map((k) => `${k}=${g.flags[k]}`) }; });
  const id = await page.evaluate(() => window.theater.game().regions.find((r) => r.name === 'Hamburg').id);
  await page.evaluate((i) => window.theater.onRegion(i), id);
  await page.waitForTimeout(400);
  console.log('before:', JSON.stringify(await G()));
  const msg = async () => page.evaluate(() => [...document.querySelectorAll('#msg, .msg, .toast, .hint, [class*=msg]')].map((e) => e.textContent.trim()).filter(Boolean).join(' || ').slice(0, 400));
  const btn = page.locator('.places button', { hasText: 'Plough it up' }).first();
  await btn.click();
  await page.waitForTimeout(500);
  console.log('after one click on Plough it up: dialogs', dialogs, '| message:', await msg());
  console.log('state:', JSON.stringify(await G()));
  const block = page.locator('.places').first();
  console.log((await block.innerText()).replace(/\n+/g, ' / ').slice(0, 700));
  await page.screenshot({ path: `${out}/72-after-wreck-v56.png` });
  const undo = await page.locator('button, [data-act]').filter({ hasText: /undo|take back|cancel the order/i }).count();
  console.log('undo controls on the page:', undo);
  // the policy buttons
  await page.locator('.places button', { hasText: 'Everywhere' }).first().click();
  await page.waitForTimeout(300);
  console.log('after Everywhere:', await msg());
  console.log((await page.locator('.places .scorch').first().innerText()).replace(/\n+/g, ' / ').slice(0, 600));
  await page.screenshot({ path: `${out}/73-policy-everywhere-v56.png` });
  // a small window: is the block reachable?
  await page.setViewportSize({ width: 1280, height: 720 });
  await page.waitForTimeout(300);
  const bb = await block.boundingBox();
  const scroller = await page.evaluate(() => { const b = document.querySelector('.places'); let p = b.parentElement; while (p && getComputedStyle(p).overflowY !== 'auto' && getComputedStyle(p).overflowY !== 'scroll') p = p.parentElement; return p ? { cls: p.className, scrollH: p.scrollHeight, clientH: p.clientHeight } : null; });
  console.log('at 1280x720 the block box is', JSON.stringify(bb), 'scroll container', JSON.stringify(scroller));
  await page.screenshot({ path: `${out}/74-panel-1280-v56.png` });
  console.log('page errors:', errors.length, errors.slice(0, 2).join(' | '));
  await browser.close();
})();
