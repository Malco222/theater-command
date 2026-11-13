const { chromium } = require('playwright');
const out = process.argv[2];
(async () => {
  const browser = await chromium.launch();
  const page = await browser.newPage({ viewport: { width: 1440, height: 900 }, deviceScaleFactor: 1 });
  const errors = [];
  page.on('console', (m) => { if (m.type() === 'error') errors.push(`${m.type()}: ${m.text()}`); });
  page.on('pageerror', (e) => errors.push(`pageerror: ${e.message}\n${e.stack}`));
  await page.goto('file:///home/user/theater-command/dist/play.html');
  await page.waitForTimeout(1200);
  // Is the Begin button in view without scrolling?
  const box = await page.locator('[data-act="start"]').boundingBox();
  console.log('begin button box', JSON.stringify(box), 'viewport 900 high');
  await page.locator('[data-act="start"]').scrollIntoViewIfNeeded();
  await page.screenshot({ path: out + '/02-start-scrolled.png' });
  const t0 = Date.now();
  await page.locator('[data-act="start"]').click();
  await page.waitForTimeout(2500);
  console.log('start took', Date.now() - t0);
  await page.screenshot({ path: out + '/03-ger-first.png' });
  const dump = async (label) => {
    const acts = await page.evaluate(() => [...document.querySelectorAll('[data-act]')].filter((e) => e.offsetParent !== null).map((e) => `${e.getAttribute('data-act')}|${e.getAttribute('data-id') ?? ''}|${(e.textContent || '').trim().replace(/\s+/g, ' ').slice(0, 60)}`));
    console.log(`--- ${label}: ${acts.length} controls ---\n` + [...new Set(acts)].slice(0, 80).join('\n'));
  };
  await dump('after start');
  const text = await page.evaluate(() => document.querySelector('#card, .modal, [role=dialog]')?.textContent?.replace(/\s+/g, ' ').slice(0, 1500) ?? '(no modal)');
  console.log('MODAL:', text);
  console.log('ERRORS', errors.join('\n'));
  await browser.close();
})();
