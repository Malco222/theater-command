// Which countries the start card offers for a grand campaign from 1939, and whether the United States is one.
const { chromium } = require('playwright');
(async () => {
  const browser = await chromium.launch();
  const page = await browser.newPage({ viewport: { width: 1440, height: 900 } });
  await page.addInitScript(() => { try { localStorage.setItem('tc-watch', '0'); } catch {} });
  await page.goto(process.env.TC_PAGE || 'file:///home/user/tc57/dist/play.html');
  await page.waitForTimeout(1200);
  const nats = await page.locator('[data-act="gcnat"]').evaluateAll((bs) => bs.map((b) => `${b.dataset.nat || b.dataset.id || ''}:${b.textContent.trim().slice(0, 30)}${b.disabled ? '[off]' : ''}`));
  console.log('countries offered:', nats.length, nats.join(' | ').slice(0, 1500));
  const usa = page.locator('[data-act="gcnat"]').filter({ hasText: /United States/i }).first();
  if (await usa.count()) { await usa.click(); await page.waitForTimeout(400); console.log('after picking it:', (await page.locator('.card, #modal').first().innerText()).replace(/\n+/g, ' / ').slice(0, 900)); }
  await browser.close();
})();
